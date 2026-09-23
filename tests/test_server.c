/*
 * test_server.c - Unit + integration tests (no external deps).
 * Naming: test_<module>_<behavior>_<expectation>.
 * Sync: readiness polling with monotonic deadline, no sleep-based assumptions.
 */

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "server.h"

static int g_passed = 0;
static int g_failed = 0;

#define RUN_TEST(fn)                                                                                                   \
    do {                                                                                                               \
        printf("[ RUN ] %s\n", #fn);                                                                                   \
        fflush(stdout);                                                                                                \
        if ((fn)() == 0) {                                                                                             \
            g_passed++;                                                                                                \
            printf("[ PASS ] %s\n", #fn);                                                                              \
        } else {                                                                                                       \
            g_failed++;                                                                                                \
            printf("[ FAIL ] %s\n", #fn);                                                                              \
        }                                                                                                              \
        fflush(stdout);                                                                                                \
    } while (0)

#define EXPECT_TRUE(cond)                                                                                              \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            printf("  ASSERT %s:%d: expected true: %s (errno=%d %s)\n", __FILE__, __LINE__, #cond, errno,                \
                   strerror(errno));                                                                                   \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (0)

#define EXPECT_EQ_INT(a, b)                                                                                            \
    do {                                                                                                               \
        if ((a) != (b)) {                                                                                              \
            printf("  ASSERT %s:%d: %s (%d) != %s (%d)\n", __FILE__, __LINE__, #a, (int)(a), #b, (int)(b));              \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (0)

#define EXPECT_EQ_MEM(a, b, n)                                                                                         \
    do {                                                                                                               \
        if (memcmp((a), (b), (n)) != 0) {                                                                               \
            printf("  ASSERT %s:%d: memory differs (%s vs %s, %zu bytes)\n", __FILE__, __LINE__, #a, #b,                 \
                   (size_t)(n));                                                                                       \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (0)

/* ---------------- unit: config ---------------- */

static ServerConfig valid_config(uint16_t port) {
    ServerConfig config;
    memset(&config, 0, sizeof(config));
    config.bind_ip = "127.0.0.1";
    config.port = port;
    config.thread_count = 4;
    config.backlog = 16;
    config.queue_size = 16;
    return config;
}

static int test_config_validate_null_returns_false(void) {
    errno = 0;
    EXPECT_TRUE(!server_config_validate(NULL));
    EXPECT_EQ_INT(errno, EINVAL);
    return 0;
}

static int test_config_validate_zero_port_returns_false(void) {
    ServerConfig config = valid_config(0);
    EXPECT_TRUE(!server_config_validate(&config));
    return 0;
}

static int test_config_validate_bad_threads_returns_false(void) {
    ServerConfig config = valid_config(18080);
    config.thread_count = 0;
    EXPECT_TRUE(!server_config_validate(&config));
    config.thread_count = 1000;
    EXPECT_TRUE(!server_config_validate(&config));
    return 0;
}

static int test_config_validate_bad_queue_returns_false(void) {
    ServerConfig config = valid_config(18080);
    config.queue_size = 0;
    EXPECT_TRUE(!server_config_validate(&config));
    config = valid_config(18080);
    config.backlog = 0;
    EXPECT_TRUE(!server_config_validate(&config));
    return 0;
}

static int test_config_validate_ok_returns_true(void) {
    ServerConfig config = valid_config(18080);
    EXPECT_TRUE(server_config_validate(&config));
    return 0;
}

static int test_server_create_invalid_returns_null(void) {
    EXPECT_TRUE(server_create(NULL) == NULL);
    ServerConfig bad = valid_config(0);
    EXPECT_TRUE(server_create(&bad) == NULL);
    return 0;
}

static int test_server_create_ok_returns_instance(void) {
    ServerConfig config = valid_config(18081);
    Server *server = server_create(&config);
    EXPECT_TRUE(server != NULL);
    struct ServerStats stats;
    server_stats(server, &stats);
    EXPECT_TRUE(stats.connections_accepted == 0UL);
    server_destroy(server);
    server_destroy(NULL); /* no-op, must not crash */
    server_stats(NULL, NULL); /* no-op, must not crash */
    return 0;
}

/* ---------------- unit: send_all ---------------- */

static int test_send_all_zero_len_returns_ok(void) {
    EXPECT_EQ_INT(server_send_all(-1, NULL, 0), 0);
    return 0;
}

static int test_send_all_null_buf_with_len_returns_error(void) {
    const char dummy = 'x';
    (void)dummy;
    EXPECT_EQ_INT(server_send_all(-1, NULL, 1), -1);
    return 0;
}

static int test_send_all_invalid_fd_returns_error(void) {
    const char data[] = "hello";
    EXPECT_EQ_INT(server_send_all(-1, data, sizeof(data)), -1);
    return 0;
}

static int test_send_all_socketpair_roundtrip(void) {
    int pair[2] = {-1, -1};
    EXPECT_TRUE(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    const char data[] = "roundtrip-payload-123";
    EXPECT_EQ_INT(server_send_all(pair[0], data, sizeof(data)), 0);
    char received[64];
    memset(received, 0, sizeof(received));
    size_t total = 0;
    while (total < sizeof(data)) {
        ssize_t got = recv(pair[1], received + total, sizeof(data) - total, 0);
        EXPECT_TRUE(got > 0);
        total += (size_t)got;
    }
    EXPECT_EQ_MEM(data, received, sizeof(data));
    close(pair[0]);
    close(pair[1]);
    return 0;
}

/* ---------------- integration helpers ---------------- */

typedef struct {
    Server *server;
    int result;
} ServerThreadArg;

static void *server_thread_main(void *arg) {
    ServerThreadArg *thread_arg = (ServerThreadArg *)arg;
    thread_arg->result = server_run(thread_arg->server);
    return NULL;
}

static int wait_until_connectable(uint16_t port, int timeout_ms) {
    struct timespec deadline;
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec += timeout_ms / 1000;
    deadline.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec += 1;
        deadline.tv_nsec -= 1000000000L;
    }
    for (;;) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            return -1;
        }
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        int connected = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
        close(fd);
        if (connected == 0) {
            return 0;
        }
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec > deadline.tv_sec || (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec)) {
            errno = ETIMEDOUT;
            return -1;
        }
        struct timespec sleep_time = {.tv_sec = 0, .tv_nsec = 10 * 1000000L};
        nanosleep(&sleep_time, NULL);
    }
}

static int client_connect(uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) {
        close(fd);
        return -1;
    }
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static int recv_exact(int fd, void *buf, size_t len, int timeout_ms) {
    uint8_t *cursor = (uint8_t *)buf;
    size_t remaining = len;
    struct timespec deadline;
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec += timeout_ms / 1000;
    deadline.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec += 1;
        deadline.tv_nsec -= 1000000000L;
    }
    while (remaining > 0) {
        ssize_t got = recv(fd, cursor, remaining, 0);
        if (got < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (got == 0) {
            errno = ECONNRESET;
            return -1;
        }
        cursor += (size_t)got;
        remaining -= (size_t)got;
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (remaining > 0 && (now.tv_sec > deadline.tv_sec ||
                              (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec))) {
            errno = ETIMEDOUT;
            return -1;
        }
    }
    return 0;
}

typedef struct {
    Server *server;
    pthread_t thread;
    uint16_t port;
} TestServer;

/* Simplify lifetime: keep heap args in a tiny static registry. */
#define MAX_TEST_SERVERS 8
static ServerThreadArg *g_args[MAX_TEST_SERVERS];
static int g_args_count = 0;

static int test_server_stop(TestServer *test_server) {
    server_stop(test_server->server);
    pthread_join(test_server->thread, NULL);
    server_destroy(test_server->server);
    test_server->server = NULL;
    return 0;
}

/* Patched start that registers args for cleanup. */
static int test_server_start_tracked(TestServer *test_server, uint16_t port) {
    ServerConfig config = valid_config(port);
    config.thread_count = 4;
    test_server->server = server_create(&config);
    if (test_server->server == NULL) {
        return -1;
    }
    test_server->port = port;
    ServerThreadArg *arg = calloc(1, sizeof(ServerThreadArg));
    if (arg == NULL) {
        server_destroy(test_server->server);
        return -1;
    }
    arg->server = test_server->server;
    if (g_args_count >= MAX_TEST_SERVERS) {
        free(arg);
        server_destroy(test_server->server);
        return -1;
    }
    g_args[g_args_count++] = arg;
    if (pthread_create(&test_server->thread, NULL, server_thread_main, arg) != 0) {
        g_args_count--;
        free(arg);
        server_destroy(test_server->server);
        return -1;
    }
    if (wait_until_connectable(port, 3000) != 0) {
        server_stop(test_server->server);
        pthread_join(test_server->thread, NULL);
        server_destroy(test_server->server);
        return -1;
    }
    return 0;
}

static void test_server_cleanup_args(void) {
    for (int i = 0; i < g_args_count; i++) {
        free(g_args[i]);
        g_args[i] = NULL;
    }
    g_args_count = 0;
}

/* ---------------- integration: behavior ---------------- */

static int test_echo_single_client_returns_same_bytes(void) {
    TestServer test_server;
    memset(&test_server, 0, sizeof(test_server));
    EXPECT_TRUE(test_server_start_tracked(&test_server, 18090) == 0);
    int fd = client_connect(18090);
    EXPECT_TRUE(fd >= 0);
    const char payload[] = "hello-echo-1";
    EXPECT_TRUE(server_send_all(fd, payload, sizeof(payload)) == 0);
    char echo[sizeof(payload)];
    EXPECT_TRUE(recv_exact(fd, echo, sizeof(echo), 2000) == 0);
    EXPECT_EQ_MEM(payload, echo, sizeof(payload));
    close(fd);
    EXPECT_TRUE(test_server_stop(&test_server) == 0);
    return 0;
}

static int test_echo_large_payload_spanning_buffers(void) {
    TestServer test_server;
    memset(&test_server, 0, sizeof(test_server));
    EXPECT_TRUE(test_server_start_tracked(&test_server, 18091) == 0);
    int fd = client_connect(18091);
    EXPECT_TRUE(fd >= 0);
    static char payload[65536];
    for (size_t i = 0; i < sizeof(payload); i++) {
        payload[i] = (char)('A' + (i % 26));
    }
    EXPECT_TRUE(server_send_all(fd, payload, sizeof(payload)) == 0);
    static char echo[65536];
    EXPECT_TRUE(recv_exact(fd, echo, sizeof(echo), 5000) == 0);
    EXPECT_EQ_MEM(payload, echo, sizeof(payload));
    close(fd);
    EXPECT_TRUE(test_server_stop(&test_server) == 0);
    return 0;
}

static int test_echo_peer_abrupt_disconnect_server_survives(void) {
    TestServer test_server;
    memset(&test_server, 0, sizeof(test_server));
    EXPECT_TRUE(test_server_start_tracked(&test_server, 18092) == 0);
    int rude = client_connect(18092);
    EXPECT_TRUE(rude >= 0);
    close(rude); /* no data, abrupt close */
    /* Server must still serve the next client. Readiness-poll again. */
    EXPECT_TRUE(wait_until_connectable(18092, 2000) == 0);
    int fd = client_connect(18092);
    EXPECT_TRUE(fd >= 0);
    const char payload[] = "after-disconnect";
    EXPECT_TRUE(server_send_all(fd, payload, sizeof(payload)) == 0);
    char echo[sizeof(payload)];
    EXPECT_TRUE(recv_exact(fd, echo, sizeof(echo), 2000) == 0);
    EXPECT_EQ_MEM(payload, echo, sizeof(payload));
    close(fd);
    EXPECT_TRUE(test_server_stop(&test_server) == 0);
    return 0;
}

typedef struct {
    uint16_t port;
    int index;
    int failed;
} ClientWorkerArg;

static void *client_worker(void *arg) {
    ClientWorkerArg *worker = (ClientWorkerArg *)arg;
    int fd = client_connect(worker->port);
    if (fd < 0) {
        worker->failed = 1;
        return NULL;
    }
    char payload[256];
    memset(payload, 0, sizeof(payload));
    snprintf(payload, sizeof(payload), "client-%d-payload", worker->index);
    size_t len = strlen(payload) + 1;
    if (server_send_all(fd, payload, len) != 0) {
        worker->failed = 1;
        close(fd);
        return NULL;
    }
    char echo[256];
    memset(echo, 0, sizeof(echo));
    if (recv_exact(fd, echo, len, 3000) != 0 || memcmp(payload, echo, len) != 0) {
        worker->failed = 1;
        close(fd);
        return NULL;
    }
    close(fd);
    return NULL;
}

static int test_echo_concurrent_clients_all_succeed(void) {
    TestServer test_server;
    memset(&test_server, 0, sizeof(test_server));
    EXPECT_TRUE(test_server_start_tracked(&test_server, 18093) == 0);
    const int client_count = 16;
    pthread_t threads[16];
    ClientWorkerArg args[16];
    for (int i = 0; i < client_count; i++) {
        args[i].port = 18093;
        args[i].index = i;
        args[i].failed = 0;
        EXPECT_TRUE(pthread_create(&threads[i], NULL, client_worker, &args[i]) == 0);
    }
    for (int i = 0; i < client_count; i++) {
        pthread_join(threads[i], NULL);
        EXPECT_TRUE(args[i].failed == 0);
    }
    EXPECT_TRUE(test_server_stop(&test_server) == 0);
    return 0;
}

int main(void) {
    RUN_TEST(test_config_validate_null_returns_false);
    RUN_TEST(test_config_validate_zero_port_returns_false);
    RUN_TEST(test_config_validate_bad_threads_returns_false);
    RUN_TEST(test_config_validate_bad_queue_returns_false);
    RUN_TEST(test_config_validate_ok_returns_true);
    RUN_TEST(test_server_create_invalid_returns_null);
    RUN_TEST(test_server_create_ok_returns_instance);
    RUN_TEST(test_send_all_zero_len_returns_ok);
    RUN_TEST(test_send_all_null_buf_with_len_returns_error);
    RUN_TEST(test_send_all_invalid_fd_returns_error);
    RUN_TEST(test_send_all_socketpair_roundtrip);
    RUN_TEST(test_echo_single_client_returns_same_bytes);
    RUN_TEST(test_echo_large_payload_spanning_buffers);
    RUN_TEST(test_echo_peer_abrupt_disconnect_server_survives);
    RUN_TEST(test_echo_concurrent_clients_all_succeed);
    test_server_cleanup_args();
    printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
