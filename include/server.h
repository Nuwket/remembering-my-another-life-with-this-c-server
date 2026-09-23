#ifndef SERVER_H
#define SERVER_H

#include <netinet/in.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SERVER_DEFAULT_PORT 8080
#define SERVER_DEFAULT_THREADS 8
#define SERVER_DEFAULT_BACKLOG 64
#define SERVER_DEFAULT_QUEUE_SIZE 128
#define SERVER_RECV_TIMEOUT_MS 5000
#define SERVER_SEND_TIMEOUT_MS 5000
#define SERVER_IO_BUFFER_SIZE 4096
#define SERVER_ACCEPT_POLL_MS 100
#define SERVER_MAX_PORT 65535

#define SERVER_VERSION "0.2.0"
#define SERVER_DEFAULT_DB "./data/app.db"

typedef struct {
    const char *bind_ip; /* NULL or "0.0.0.0" means INADDR_ANY. Caller owns string. */
    uint16_t port;
    int thread_count; /* 1..64 */
    int backlog;      /* listen(2) backlog */
    int queue_size;   /* bounded pending-connection queue */
    const char *db_path; /* SQLite file, e.g. "./data/app.db". Required, caller owns. */
} ServerConfig;

typedef struct Server Server;

struct ServerStats {
    atomic_ulong connections_accepted;
    atomic_ulong connections_handled;
    atomic_ulong connections_dropped_queue_full;
    atomic_ulong echo_bytes;
    atomic_ulong errors;
    atomic_ulong http_requests;
    atomic_ulong http_errors;
};

/*
 * server_config_validate - Check external configuration at the boundary.
 * @config: pointer to user-supplied config (not thread-safe, caller owns).
 * Returns: true if valid, false with errno=EINVAL otherwise.
 */
bool server_config_validate(const ServerConfig *config);

/*
 * server_create - Allocate and initialize a server instance.
 * @config: validated config, copied internally (caller keeps ownership).
 *   Opens the SQLite store; on DB failure returns NULL (no memory fallback).
 * Returns: new Server, or NULL with errno set (EINVAL, ENOMEM, EIO).
 * Ownership: caller must call server_destroy(). Not yet running.
 * Thread-safety: not thread-safe (init only).
 */
Server *server_create(const ServerConfig *config);

/*
 * server_run - Block serving HTTP API clients until server_stop() is called.
 * @server: instance from server_create (thread-safe after create).
 * Returns: 0 on clean stop, -1 with errno on fatal listen/accept error.
 * Thread-safety: call once; server_stop() is thread-safe (signal handler
 *   should only set a flag and call server_stop_async_safe() instead).
 */
int server_run(Server *server);

/*
 * server_stop - Request a graceful stop (closes listener, drains queue).
 * @server: running instance (thread-safe, idempotent).
 */
void server_stop(Server *server);

/*
 * server_destroy - Free all resources. Stop first if running.
 * @server: may be NULL (no-op). Caller must not use after.
 */
void server_destroy(Server *server);

/*
 * server_stats - Snapshot counters for observability/tests.
 * @server: instance (thread-safe, lock-free atomic loads).
 * @out: caller-provided output (not NULL).
 */
void server_stats(Server *server, struct ServerStats *out);

/*
 * server_send_all - Send full buffer handling partial send + EINTR.
 * @fd: connected socket. @buf/@len: bytes to send.
 * Returns: 0 on success, -1 with errno on failure/timeout.
 * Why public: unit-tested pure-IO helper, reused by handler and tests.
 */
int server_send_all(int fd, const void *buf, size_t len);

/*
 * server_uptime_s - Seconds since server_create (monotonic wall time).
 * @server: instance (thread-safe). Returns: >= 0, or 0 when NULL.
 */
long server_uptime_s(Server *server);

#endif
