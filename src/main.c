/*
 * main.c - CLI entry: parse args/env, handle signals, run server.
 * Validates all external input at the boundary (argv/env).
 */

#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "log.h"
#include "server.h"

static Server *g_server = NULL;

static void print_usage(const char *program) {
    fprintf(stderr,
            "Usage: %s [--bind IP] [--port PORT] [--threads N] [--db PATH] [--api-key KEY]\n"
            "Env: BIND_IP, PORT, THREADS, DB_PATH, API_KEY (CLI overrides env)\n"
            "Defaults: bind=0.0.0.0 port=8080 threads=8 db=./data/app.db api-key=(open mode)\n"
            "Auth: with --api-key set, PUT/POST/DELETE under /api/ require X-API-Key\n"
            "      (missing -> 401, wrong -> 403). Without it the server is open.\n",
            program);
}

static void on_signal(int signo) {
    (void)signo;
    /* Only async-signal-safe work here: stop() uses atomics + shutdown(),
     * which is safe enough to wake the accept loop. */
    if (g_server != NULL) {
        server_stop(g_server);
    }
}

static int parse_port(const char *text, uint16_t *out) {
    if (text == NULL || out == NULL) {
        return -1;
    }
    char *end = NULL;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value <= 0 || value > SERVER_MAX_PORT) {
        return -1;
    }
    *out = (uint16_t)value;
    return 0;
}

static int parse_positive(const char *text, int *out, int max_value) {
    if (text == NULL || out == NULL) {
        return -1;
    }
    char *end = NULL;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value <= 0 || value > max_value) {
        return -1;
    }
    *out = (int)value;
    return 0;
}

/* Create parent directories for the DB path (mkdir -p equivalent). */
static int ensure_db_dir(const char *db_path) {
    if (db_path == NULL || db_path[0] == '\0') {
        return -1;
    }
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s", db_path);
    char *slash = strrchr(dir, '/');
    if (slash == NULL) {
        return 0; /* Bare filename: current directory exists. */
    }
    *slash = '\0';
    if (dir[0] == '\0') {
        return 0;
    }
    /* Walk components so nested paths work. */
    for (char *p = dir + 1; *p != '\0'; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(dir, 0755);
            *p = '/';
        }
    }
    if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    const char *bind_ip = getenv("BIND_IP");
    const char *port_text = getenv("PORT");
    const char *threads_text = getenv("THREADS");
    const char *db_path = getenv("DB_PATH");
    const char *api_key = getenv("API_KEY");

    if (bind_ip == NULL || bind_ip[0] == '\0') {
        bind_ip = "0.0.0.0";
    }
    if (db_path == NULL || db_path[0] == '\0') {
        db_path = SERVER_DEFAULT_DB;
    }
    uint16_t port = SERVER_DEFAULT_PORT;
    int threads = SERVER_DEFAULT_THREADS;
    if (port_text != NULL && port_text[0] != '\0' && parse_port(port_text, &port) != 0) {
        fprintf(stderr, "error: invalid PORT=%s\n", port_text);
        return 2;
    }
    if (threads_text != NULL && threads_text[0] != '\0' && parse_positive(threads_text, &threads, 64) != 0) {
        fprintf(stderr, "error: invalid THREADS=%s\n", threads_text);
        return 2;
    }

    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "--bind") == 0) && i + 1 < argc) {
            bind_ip = argv[++i];
        } else if ((strcmp(argv[i], "--port") == 0) && i + 1 < argc) {
            if (parse_port(argv[++i], &port) != 0) {
                fprintf(stderr, "error: invalid --port value\n");
                print_usage(argv[0]);
                return 2;
            }
        } else if ((strcmp(argv[i], "--threads") == 0) && i + 1 < argc) {
            if (parse_positive(argv[++i], &threads, 64) != 0) {
                fprintf(stderr, "error: invalid --threads value\n");
                print_usage(argv[0]);
                return 2;
            }
        } else if ((strcmp(argv[i], "--db") == 0) && i + 1 < argc) {
            db_path = argv[++i];
            if (db_path[0] == '\0') {
                fprintf(stderr, "error: invalid --db value\n");
                print_usage(argv[0]);
                return 2;
            }
        } else if ((strcmp(argv[i], "--api-key") == 0) && i + 1 < argc) {
            api_key = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "error: unknown arg: %s\n", argv[i]);
            print_usage(argv[0]);
            return 2;
        }
    }

    /* Validate bind IP early (fail fast on bad external input). */
    struct in_addr probe;
    if (strcmp(bind_ip, "0.0.0.0") != 0 && inet_pton(AF_INET, bind_ip, &probe) != 1) {
        fprintf(stderr, "error: invalid bind IP: %s\n", bind_ip);
        return 2;
    }

    /* API key is optional; when present it must fit the fixed field. */
    if (api_key != NULL && api_key[0] != '\0' && strlen(api_key) >= HTTP_MAX_API_KEY_LEN) {
        fprintf(stderr, "error: --api-key too long (max %d chars)\n", HTTP_MAX_API_KEY_LEN - 1);
        return 2;
    }

    ServerConfig config = {
        .bind_ip = bind_ip,
        .port = port,
        .thread_count = threads,
        .backlog = SERVER_DEFAULT_BACKLOG,
        .queue_size = SERVER_DEFAULT_QUEUE_SIZE,
        .db_path = db_path,
        .api_key = api_key,
    };
    if (ensure_db_dir(db_path) != 0) {
        fprintf(stderr, "error: cannot create db directory for: %s\n", db_path);
        return 2;
    }
    Server *server = server_create(&config);
    if (server == NULL) {
        fprintf(stderr, "error: server_create failed: %s\n", strerror(errno));
        return 1;
    }
    g_server = server;

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = on_signal;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    /* Avoid SIGPIPE killing the process on peer reset; send() uses MSG_NOSIGNAL too. */
    signal(SIGPIPE, SIG_IGN);

    /* Pretty console banner with clickable shortcut links (stdout, flushed). */
    const char *link_host = strcmp(bind_ip, "0.0.0.0") == 0 ? "127.0.0.1" : bind_ip;
    const char *auth_mode = (api_key != NULL && api_key[0] != '\0') ? "protected (X-API-Key)" : "open (no key)";
    printf("\n"
           "  ==============================================================\n"
           "   C API Server v%s running\n"
           "  --------------------------------------------------------------\n"
           "   App:      http://%s:%u/\n"
           "   Health:   http://%s:%u/health\n"
           "   Metrics:  http://%s:%u/metrics\n"
           "   KV:       http://%s:%u/api/kv?limit=50\n"
           "   Notes:    http://%s:%u/api/notes?limit=50\n"
           "  --------------------------------------------------------------\n"
           "   bind=%s threads=%d db=%s\n"
           "   Auth: %s\n"
           "   Stop: Ctrl-C\n"
           "  ==============================================================\n\n",
           SERVER_VERSION, link_host, (unsigned)port, link_host, (unsigned)port, link_host, (unsigned)port,
           link_host, (unsigned)port, link_host, (unsigned)port, bind_ip, threads, db_path, auth_mode);
    fflush(stdout);

    char startup[256];
    snprintf(startup, sizeof(startup), "listening (bind=%s port=%u threads=%d db=%s)", bind_ip, (unsigned)port,
             threads, db_path);
    LOG_INFO("main", startup);

    int result = server_run(server);
    server_destroy(server);
    g_server = NULL;
    return result == 0 ? 0 : 1;
}
