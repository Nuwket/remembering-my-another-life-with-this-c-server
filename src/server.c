/*
 * server.c - Bounded thread-pool TCP echo server.
 *
 * Concurrency model: acceptor thread (server_run caller) + N workers.
 * Justification: bounded threads + bounded queue give backpressure and
 * predictable memory; simpler and race-auditable vs epoll for this scale.
 * See docs/ADR-001-concurrency-model.md.
 *
 * Hot path: stack buffer (no malloc), one recv/send per iteration,
 * short mutex scope only for queue ops. Stats are lock-free atomics.
 */

#include "server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <poll.h>
#include <unistd.h>

#include "log.h"

#define MODULE "server"
#define MAX_THREADS 64
#define MAX_QUEUE_SIZE 4096
#define MAX_BACKLOG 1024

struct ConnQueue {
    int *fds;
    int capacity;
    int head;
    int tail;
    int count;
    bool closed;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
};

struct Server {
    ServerConfig config;
    int listen_fd;
    atomic_bool stop_requested;
    pthread_t *workers;
    int worker_count;
    struct ConnQueue queue;
    struct ServerStats stats;
};

static void stats_bump(atomic_ulong *counter) {
    atomic_fetch_add_explicit(counter, 1UL, memory_order_relaxed);
}

static void stats_add(atomic_ulong *counter, unsigned long value) {
    atomic_fetch_add_explicit(counter, value, memory_order_relaxed);
}

static int queue_init(struct ConnQueue *queue, int capacity) {
    if (queue == NULL || capacity <= 0) {
        errno = EINVAL;
        return -1;
    }
    queue->fds = calloc((size_t)capacity, sizeof(int));
    if (queue->fds == NULL) {
        return -1;
    }
    queue->capacity = capacity;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    queue->closed = false;
    if (pthread_mutex_init(&queue->mutex, NULL) != 0) {
        free(queue->fds);
        queue->fds = NULL;
        return -1;
    }
    if (pthread_cond_init(&queue->not_empty, NULL) != 0) {
        pthread_mutex_destroy(&queue->mutex);
        free(queue->fds);
        queue->fds = NULL;
        return -1;
    }
    if (pthread_cond_init(&queue->not_full, NULL) != 0) {
        pthread_cond_destroy(&queue->not_empty);
        pthread_mutex_destroy(&queue->mutex);
        free(queue->fds);
        queue->fds = NULL;
        return -1;
    }
    return 0;
}

static void queue_destroy(struct ConnQueue *queue) {
    if (queue == NULL) {
        return;
    }
    pthread_cond_destroy(&queue->not_full);
    pthread_cond_destroy(&queue->not_empty);
    pthread_mutex_destroy(&queue->mutex);
    free(queue->fds);
    queue->fds = NULL;
}

static void queue_close(struct ConnQueue *queue) {
    pthread_mutex_lock(&queue->mutex);
    queue->closed = true;
    pthread_cond_broadcast(&queue->not_empty);
    pthread_cond_broadcast(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);
}

/* Non-blocking push used by acceptor to apply backpressure: -1 if full. */
static int queue_try_push(struct ConnQueue *queue, int fd) {
    int result = 0;
    pthread_mutex_lock(&queue->mutex);
    if (queue->closed) {
        result = -1;
    } else if (queue->count == queue->capacity) {
        result = -1;
    } else {
        queue->fds[queue->tail] = fd;
        queue->tail = (queue->tail + 1) % queue->capacity;
        queue->count++;
        pthread_cond_signal(&queue->not_empty);
        result = 0;
    }
    pthread_mutex_unlock(&queue->mutex);
    return result;
}

static int queue_pop(struct ConnQueue *queue) {
    pthread_mutex_lock(&queue->mutex);
    while (queue->count == 0 && !queue->closed) {
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }
    if (queue->count == 0 && queue->closed) {
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }
    int fd = queue->fds[queue->head];
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);
    return fd;
}

bool server_config_validate(const ServerConfig *config) {
    if (config == NULL) {
        errno = EINVAL;
        return false;
    }
    if (config->port == 0) {
        errno = EINVAL;
        return false;
    }
    /* Upper bound (>65535) is enforced at parse boundary + uint16_t type limit. */
    if (config->thread_count <= 0 || config->thread_count > MAX_THREADS) {
        errno = EINVAL;
        return false;
    }
    if (config->backlog <= 0 || config->backlog > MAX_BACKLOG) {
        errno = EINVAL;
        return false;
    }
    if (config->queue_size <= 0 || config->queue_size > MAX_QUEUE_SIZE) {
        errno = EINVAL;
        return false;
    }
    return true;
}

int server_send_all(int fd, const void *buf, size_t len) {
    if (buf == NULL && len != 0) {
        errno = EINVAL;
        return -1;
    }
    if (len == 0) {
        return 0;
    }
    const uint8_t *cursor = (const uint8_t *)buf;
    size_t remaining = len;
    while (remaining > 0) {
        ssize_t sent = send(fd, cursor, remaining, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        /* send() returning 0 on blocking socket is unexpected; treat as EPIPE. */
        if (sent == 0) {
            errno = EPIPE;
            return -1;
        }
        cursor += (size_t)sent;
        remaining -= (size_t)sent;
    }
    return 0;
}

static void format_peer(const struct sockaddr_in *addr, char *out, size_t out_len) {
    char ip[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip)) == NULL) {
        snprintf(out, out_len, "unknown");
        return;
    }
    snprintf(out, out_len, "%s:%u", ip, (unsigned)ntohs(addr->sin_port));
}

static int set_timeouts(int fd) {
    struct timeval recv_timeout = {
        .tv_sec = SERVER_RECV_TIMEOUT_MS / 1000,
        .tv_usec = (SERVER_RECV_TIMEOUT_MS % 1000) * 1000,
    };
    struct timeval send_timeout = {
        .tv_sec = SERVER_SEND_TIMEOUT_MS / 1000,
        .tv_usec = (SERVER_SEND_TIMEOUT_MS % 1000) * 1000,
    };
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout)) != 0) {
        return -1;
    }
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(send_timeout)) != 0) {
        return -1;
    }
    return 0;
}

static void handle_client(Server *server, int client_fd, const char *peer) {
    if (set_timeouts(client_fd) != 0) {
        LOG_ERROR_PEER(MODULE, "setsockopt timeout failed", peer);
        stats_bump(&server->stats.errors);
        return;
    }
    /* Stack buffer: zero allocs in hot path, cache-friendly, bounded. */
    char buffer[SERVER_IO_BUFFER_SIZE];
    for (;;) {
        ssize_t received = recv(client_fd, buffer, sizeof(buffer), 0);
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            /* Timeout/EAGAIN/ECONNRESET are normal peer conditions: count, do not crash. */
            stats_bump(&server->stats.errors);
            return;
        }
        if (received == 0) {
            return; /* Clean EOF. */
        }
        if (server_send_all(client_fd, buffer, (size_t)received) != 0) {
            stats_bump(&server->stats.errors);
            return;
        }
        stats_add(&server->stats.echo_bytes, (unsigned long)received);
    }
}

static void *worker_main(void *arg) {
    Server *server = (Server *)arg;
    for (;;) {
        int client_fd = queue_pop(&server->queue);
        if (client_fd < 0) {
            return NULL; /* Queue closed + drained. */
        }
        struct sockaddr_in peer_addr;
        socklen_t peer_len = sizeof(peer_addr);
        char peer[64] = "unknown";
        if (getpeername(client_fd, (struct sockaddr *)&peer_addr, &peer_len) == 0) {
            format_peer(&peer_addr, peer, sizeof(peer));
        }
        handle_client(server, client_fd, peer);
        close(client_fd);
        stats_bump(&server->stats.connections_handled);
    }
}

static int create_listener(const ServerConfig *config) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    int reuse = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) != 0) {
        int saved = errno;
        close(fd);
        errno = saved;
        return -1;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config->port);
    if (config->bind_ip == NULL || config->bind_ip[0] == '\0' || strcmp(config->bind_ip, "0.0.0.0") == 0) {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        if (inet_pton(AF_INET, config->bind_ip, &addr.sin_addr) != 1) {
            close(fd);
            errno = EINVAL;
            return -1;
        }
    }
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        int saved = errno;
        close(fd);
        errno = saved;
        return -1;
    }
    if (listen(fd, config->backlog) != 0) {
        int saved = errno;
        close(fd);
        errno = saved;
        return -1;
    }
    return fd;
}

Server *server_create(const ServerConfig *config) {
    if (!server_config_validate(config)) {
        return NULL;
    }
    Server *server = calloc(1, sizeof(Server));
    if (server == NULL) {
        return NULL;
    }
    server->config = *config;
    server->listen_fd = -1;
    atomic_init(&server->stop_requested, false);
    atomic_init(&server->stats.connections_accepted, 0UL);
    atomic_init(&server->stats.connections_handled, 0UL);
    atomic_init(&server->stats.connections_dropped_queue_full, 0UL);
    atomic_init(&server->stats.echo_bytes, 0UL);
    atomic_init(&server->stats.errors, 0UL);
    if (queue_init(&server->queue, config->queue_size) != 0) {
        free(server);
        return NULL;
    }
    return server;
}

void server_stop(Server *server) {
    if (server == NULL) {
        return;
    }
    atomic_store_explicit(&server->stop_requested, true, memory_order_relaxed);
    queue_close(&server->queue);
    /* Closing the listener wakes a blocking poll/accept on Linux. */
    if (server->listen_fd >= 0) {
        /* shutdown() before close() to interrupt accept() deterministically. */
        shutdown(server->listen_fd, SHUT_RDWR);
    }
}

void server_destroy(Server *server) {
    if (server == NULL) {
        return;
    }
    queue_destroy(&server->queue);
    free(server->workers);
    server->workers = NULL;
    if (server->listen_fd >= 0) {
        close(server->listen_fd);
    }
    free(server);
}

void server_stats(Server *server, struct ServerStats *out) {
    if (server == NULL || out == NULL) {
        return;
    }
    out->connections_accepted = atomic_load_explicit(&server->stats.connections_accepted, memory_order_relaxed);
    out->connections_handled = atomic_load_explicit(&server->stats.connections_handled, memory_order_relaxed);
    out->connections_dropped_queue_full =
        atomic_load_explicit(&server->stats.connections_dropped_queue_full, memory_order_relaxed);
    out->echo_bytes = atomic_load_explicit(&server->stats.echo_bytes, memory_order_relaxed);
    out->errors = atomic_load_explicit(&server->stats.errors, memory_order_relaxed);
}

int server_run(Server *server) {
    if (server == NULL) {
        errno = EINVAL;
        return -1;
    }
    server->listen_fd = create_listener(&server->config);
    if (server->listen_fd < 0) {
        LOG_ERROR(MODULE, "bind/listen failed");
        return -1;
    }
    server->workers = calloc((size_t)server->config.thread_count, sizeof(pthread_t));
    if (server->workers == NULL) {
        close(server->listen_fd);
        server->listen_fd = -1;
        return -1;
    }
    server->worker_count = server->config.thread_count;
    for (int i = 0; i < server->worker_count; i++) {
        if (pthread_create(&server->workers[i], NULL, worker_main, server) != 0) {
            /* Fail fast: stop what started, join, cleanup listener. */
            server_stop(server);
            for (int j = 0; j < i; j++) {
                pthread_join(server->workers[j], NULL);
            }
            close(server->listen_fd);
            server->listen_fd = -1;
            return -1;
        }
    }
    LOG_INFO(MODULE, "serving (thread-pool ready)");

    /* Accept loop with poll timeout so server_stop() is honored promptly. */
    while (!atomic_load_explicit(&server->stop_requested, memory_order_relaxed)) {
        struct pollfd pfd = {.fd = server->listen_fd, .events = POLLIN};
        int ready = poll(&pfd, 1, SERVER_ACCEPT_POLL_MS);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            LOG_ERROR(MODULE, "poll failed");
            stats_bump(&server->stats.errors);
            break;
        }
        if (ready == 0) {
            continue; /* Timeout: re-check stop flag. */
        }
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server->listen_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            if (atomic_load_explicit(&server->stop_requested, memory_order_relaxed)) {
                break; /* Expected during shutdown. */
            }
            /* Transient (EMFILE/ENFILE/ECONNABORTED): count and continue, do not crash. */
            stats_bump(&server->stats.errors);
            continue;
        }
        stats_bump(&server->stats.connections_accepted);
        if (queue_try_push(&server->queue, client_fd) != 0) {
            /* Backpressure: queue full or closing. Drop fast, do not block acceptor. */
            stats_bump(&server->stats.connections_dropped_queue_full);
            close(client_fd);
            continue;
        }
    }

    /* Graceful drain: close queue, join workers, close listener. */
    queue_close(&server->queue);
    for (int i = 0; i < server->worker_count; i++) {
        pthread_join(server->workers[i], NULL);
    }
    close(server->listen_fd);
    server->listen_fd = -1;
    LOG_INFO(MODULE, "stopped cleanly");
    return 0;
}
