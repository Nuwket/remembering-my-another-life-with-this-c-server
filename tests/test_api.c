/*
 * test_api.c - HTTP/JSON/store/API coverage with specific status codes.
 * Naming: test_<module>_<behavior>_<expectation>.
 */

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "api.h"
#include "http.h"
#include "json.h"
#include "server.h"
#include "store.h"

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
            printf("  ASSERT %s:%d: %s (errno=%d %s)\n", __FILE__, __LINE__, #cond, errno, strerror(errno));            \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (0)

#define EXPECT_STATUS(err, status)                                                                                     \
    do {                                                                                                               \
        if (api_error_status(err) != (status)) {                                                                        \
            printf("  ASSERT %s:%d: status %d != %d for %s\n", __FILE__, __LINE__, api_error_status(err), (status),      \
                   #err);                                                                                              \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (0)

/* ---------------- error mapping: no fallback ---------------- */

static int test_error_status_mapping_correct(void) {
    EXPECT_STATUS(API_OK, 200);
    EXPECT_STATUS(API_ERR_BAD_REQUEST, 400);
    EXPECT_STATUS(API_ERR_MISSING_FIELD, 400);
    EXPECT_STATUS(API_ERR_INVALID_KEY, 400);
    EXPECT_STATUS(API_ERR_INVALID_ID, 400);
    EXPECT_STATUS(API_ERR_INVALID_QUERY, 400);
    EXPECT_STATUS(API_ERR_NOT_FOUND, 404);
    EXPECT_STATUS(API_ERR_METHOD_NOT_ALLOWED, 405);
    EXPECT_STATUS(API_ERR_LENGTH_REQUIRED, 411);
    EXPECT_STATUS(API_ERR_PAYLOAD_TOO_LARGE, 413);
    EXPECT_STATUS(API_ERR_UNSUPPORTED_MEDIA, 415);
    EXPECT_STATUS(API_ERR_UNAUTHORIZED, 401);
    EXPECT_STATUS(API_ERR_FORBIDDEN, 403);
    EXPECT_STATUS(API_ERR_NOT_IMPLEMENTED, 501);
    EXPECT_STATUS(API_ERR_INTERNAL, 500);
    EXPECT_STATUS((ApiError)9999, 500); /* unknown never falls back to 200 */
    EXPECT_TRUE(strcmp(api_error_code(API_ERR_NOT_FOUND), "not_found") == 0);
    EXPECT_TRUE(api_error_code((ApiError)9999) != NULL);
    return 0;
}

/* ---------------- http parser ---------------- */

static int test_http_parse_get_ok(void) {
    const char *raw = "GET /health HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n";
    HttpRequest req;
    EXPECT_TRUE(http_parse_request(raw, strlen(raw), &req) == API_OK);
    EXPECT_TRUE(req.method == HTTP_GET);
    EXPECT_TRUE(strcmp(req.path, "/health") == 0);
    return 0;
}

static int test_http_parse_query_split_ok(void) {
    const char *raw = "GET /api/kv?prefix=a&limit=10 HTTP/1.1\r\nHost: x\r\n\r\n";
    HttpRequest req;
    EXPECT_TRUE(http_parse_request(raw, strlen(raw), &req) == API_OK);
    EXPECT_TRUE(strcmp(req.path, "/api/kv") == 0);
    EXPECT_TRUE(strcmp(req.query, "prefix=a&limit=10") == 0);
    char limit[32];
    EXPECT_TRUE(http_query_get(req.query, "limit", limit, sizeof(limit)) == 0);
    EXPECT_TRUE(strcmp(limit, "10") == 0);
    EXPECT_TRUE(http_query_get(req.query, "missing", limit, sizeof(limit)) == -1);
    return 0;
}

static int test_http_parse_unknown_method_is_405(void) {
    const char *raw = "PATCH /health HTTP/1.1\r\nHost: x\r\n\r\n";
    HttpRequest req;
    EXPECT_STATUS(http_parse_request(raw, strlen(raw), &req), 405);
    return 0;
}

static int test_http_parse_chunked_is_501(void) {
    const char *raw =
        "POST /api/echo HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n";
    HttpRequest req;
    EXPECT_STATUS(http_parse_request(raw, strlen(raw), &req), 501);
    return 0;
}

static int test_http_parse_garbage_is_400(void) {
    HttpRequest req;
    EXPECT_STATUS(http_parse_request("GARBAGE\r\n\r\n", 11, &req), 400);
    EXPECT_STATUS(http_parse_request("", 0, &req), 400);
    EXPECT_STATUS(http_parse_request("GET nope HTTP/1.1\r\n\r\n", 21, &req), 400);
    return 0;
}

static int test_http_parse_api_key_header_captured(void) {
    const char *raw = "PUT /api/kv/k HTTP/1.1\r\nHost: x\r\nX-API-Key: s3cret\r\nContent-Length: 0\r\n\r\n";
    HttpRequest req;
    EXPECT_TRUE(http_parse_request(raw, strlen(raw), &req) == API_OK);
    EXPECT_TRUE(strcmp(req.api_key, "s3cret") == 0);
    const char *nokey = "GET /health HTTP/1.1\r\nHost: x\r\n\r\n";
    EXPECT_TRUE(http_parse_request(nokey, strlen(nokey), &req) == API_OK);
    EXPECT_TRUE(req.api_key[0] == '\0');
    return 0;
}

/* ---------------- json ---------------- */

static int test_json_escape_ok(void) {
    char dest[64];
    EXPECT_TRUE(json_escape(dest, sizeof(dest), "a\"b\\c") == 0);
    EXPECT_TRUE(strcmp(dest, "a\\\"b\\\\c") == 0);
    return 0;
}

static int test_json_escape_overflow_is_error(void) {
    char dest[4];
    EXPECT_TRUE(json_escape(dest, sizeof(dest), "toolongvalue") != 0);
    EXPECT_TRUE(json_escape(NULL, 0, "x") != 0);
    return 0;
}

static int test_json_get_string_ok_and_missing(void) {
    const char *body = "{\"value\":\"dark\"}";
    char out[64];
    EXPECT_TRUE(json_get_string(body, strlen(body), "value", out, sizeof(out)) == API_OK);
    EXPECT_TRUE(strcmp(out, "dark") == 0);
    EXPECT_STATUS(json_get_string(body, strlen(body), "nope", out, sizeof(out)), 400);
    return 0;
}

static int test_json_get_string_malformed_is_400(void) {
    char out[64];
    EXPECT_STATUS(json_get_string("{oops", 5, "value", out, sizeof(out)), 400);
    EXPECT_STATUS(json_get_string("[1,2]", 5, "value", out, sizeof(out)), 400);
    EXPECT_STATUS(json_get_string("{\"value\":42}", 12, "value", out, sizeof(out)), 400);
    return 0;
}

/* ---------------- store unit (:memory:) ---------------- */

static int test_store_validate_key_specific(void) {
    EXPECT_TRUE(store_validate_key("theme") == API_OK);
    EXPECT_TRUE(store_validate_key("a-b_c.d9") == API_OK);
    EXPECT_STATUS(store_validate_key(""), 400);
    EXPECT_STATUS(store_validate_key(NULL), 400);
    EXPECT_STATUS(store_validate_key("bad/key"), 400);
    EXPECT_STATUS(store_validate_key("bad key"), 400);
    return 0;
}

static int test_store_kv_crud_roundtrip(void) {
    char err[256] = "";
    Store *store = store_open(":memory:", err, sizeof(err));
    EXPECT_TRUE(store != NULL);
    EXPECT_TRUE(store_kv_put(store, "theme", "dark") == API_OK);
    char value[64];
    EXPECT_TRUE(store_kv_get(store, "theme", value, sizeof(value)) == API_OK);
    EXPECT_TRUE(strcmp(value, "dark") == 0);
    EXPECT_STATUS(store_kv_get(store, "missing", value, sizeof(value)), 404);
    EXPECT_TRUE(store_kv_del(store, "theme") == API_OK);
    EXPECT_STATUS(store_kv_del(store, "theme"), 404);
    store_close(store);
    return 0;
}

static int test_store_note_crud_roundtrip(void) {
    char err[256] = "";
    Store *store = store_open(":memory:", err, sizeof(err));
    EXPECT_TRUE(store != NULL);
    long id = 0;
    EXPECT_TRUE(store_note_create(store, "t1", "b1", &id) == API_OK);
    EXPECT_TRUE(id > 0);
    char one[2048];
    EXPECT_TRUE(store_note_get(store, id, one, sizeof(one)) == API_OK);
    EXPECT_TRUE(strstr(one, "\"t1\"") != NULL);
    EXPECT_TRUE(store_note_update(store, id, "t2", "b2") == API_OK);
    EXPECT_TRUE(store_note_get(store, id, one, sizeof(one)) == API_OK);
    EXPECT_TRUE(strstr(one, "\"t2\"") != NULL);
    EXPECT_STATUS(store_note_get(store, 999999, one, sizeof(one)), 404);
    EXPECT_TRUE(store_note_del(store, id) == API_OK);
    EXPECT_STATUS(store_note_del(store, id), 404);
    store_close(store);
    return 0;
}

static int test_store_validation_specific(void) {
    char err[256] = "";
    Store *store = store_open(":memory:", err, sizeof(err));
    EXPECT_TRUE(store != NULL);
    long id = 0;
    EXPECT_STATUS(store_note_create(store, "", "b", &id), 400);
    EXPECT_STATUS(store_note_create(store, NULL, "b", &id), 400);
    char one[64];
    EXPECT_STATUS(store_note_get(store, 0, one, sizeof(one)), 400);
    EXPECT_STATUS(store_kv_put(store, "bad/key", "v"), 400);
    EXPECT_STATUS(store_kv_list(store, "", 0, one, sizeof(one)), 400);
    EXPECT_STATUS(store_kv_list(store, "", 999, one, sizeof(one)), 400);
    long kv_count = 0;
    long note_count = 0;
    EXPECT_TRUE(store_counts(store, &kv_count, &note_count) == API_OK);
    store_close(store);
    EXPECT_TRUE(store_open(NULL, err, sizeof(err)) == NULL);
    return 0;
}

/* ---------------- integration helpers ---------------- */

typedef struct {
    Server *server;
    int result;
} ServerThreadArg;

static void *server_thread_main(void *arg) {
    ServerThreadArg *t = (ServerThreadArg *)arg;
    t->result = server_run(t->server);
    return NULL;
}

static int wait_ready(uint16_t port, int timeout_ms) {
    struct timespec deadline;
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec += timeout_ms / 1000;
    deadline.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    for (;;) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd >= 0) {
            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
            int ok = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
            close(fd);
            if (ok == 0) {
                return 0;
            }
        }
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec > deadline.tv_sec) {
            return -1;
        }
        struct timespec nap = {.tv_sec = 0, .tv_nsec = 10 * 1000000L};
        nanosleep(&nap, NULL);
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
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static int http_call(uint16_t port, const char *request, char *resp, size_t cap) {
    int fd = client_connect(port);
    if (fd < 0) {
        return -1;
    }
    size_t len = strlen(request);
    const char *cursor = request;
    size_t remaining = len;
    while (remaining > 0) {
        ssize_t sent = send(fd, cursor, remaining, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(fd);
            return -1;
        }
        cursor += (size_t)sent;
        remaining -= (size_t)sent;
    }
    size_t total = 0;
    for (;;) {
        if (total + 1 >= cap) {
            close(fd);
            return -1;
        }
        ssize_t got = recv(fd, resp + total, cap - 1 - total, 0);
        if (got < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(fd);
            return -1;
        }
        if (got == 0) {
            break;
        }
        total += (size_t)got;
    }
    resp[total] = '\0';
    close(fd);
    return 0;
}

static int g_db_seq = 0;

static void make_db_path(char *out, size_t cap) {
    snprintf(out, cap, "/tmp/capi_%d_%d.db", (int)getpid(), g_db_seq++);
    unlink(out);
}

static void cleanup_db(const char *path) {
    char extra[512];
    unlink(path);
    snprintf(extra, sizeof(extra), "%s-wal", path);
    unlink(extra);
    snprintf(extra, sizeof(extra), "%s-shm", path);
    unlink(extra);
}

typedef struct {
    Server *server;
    pthread_t thread;
    ServerThreadArg *arg;
    char db[256];
} TestServer;

static int test_server_start_key(TestServer *ts, uint16_t port, const char *api_key);

static int test_server_start(TestServer *ts, uint16_t port) {
    return test_server_start_key(ts, port, NULL);
}

static int test_server_start_key(TestServer *ts, uint16_t port, const char *api_key) {
    make_db_path(ts->db, sizeof(ts->db));
    ServerConfig config;
    memset(&config, 0, sizeof(config));
    config.bind_ip = "127.0.0.1";
    config.port = port;
    config.thread_count = 4;
    config.backlog = 16;
    config.queue_size = 16;
    config.db_path = ts->db;
    config.api_key = api_key;
    ts->server = server_create(&config);
    if (ts->server == NULL) {
        return -1;
    }
    ts->arg = calloc(1, sizeof(ServerThreadArg));
    if (ts->arg == NULL) {
        server_destroy(ts->server);
        return -1;
    }
    ts->arg->server = ts->server;
    if (pthread_create(&ts->thread, NULL, server_thread_main, ts->arg) != 0) {
        free(ts->arg);
        server_destroy(ts->server);
        return -1;
    }
    if (wait_ready(port, 3000) != 0) {
        server_stop(ts->server);
        pthread_join(ts->thread, NULL);
        server_destroy(ts->server);
        free(ts->arg);
        return -1;
    }
    return 0;
}

static int test_server_stop(TestServer *ts) {
    server_stop(ts->server);
    pthread_join(ts->thread, NULL);
    server_destroy(ts->server);
    free(ts->arg);
    cleanup_db(ts->db);
    return 0;
}

/* ---------------- integration: status codes ---------------- */

static int test_api_health_and_metrics(void) {
    TestServer ts;
    memset(&ts, 0, sizeof(ts));
    EXPECT_TRUE(test_server_start(&ts, 18110) == 0);
    char resp[4096];
    EXPECT_TRUE(http_call(18110, "GET /health HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", resp,
                          sizeof(resp)) == 0);
    EXPECT_TRUE(strstr(resp, "200 OK") != NULL && strstr(resp, "\"uptime_s\"") != NULL);
    EXPECT_TRUE(strstr(resp, "\"open\"") != NULL);
    EXPECT_TRUE(http_call(18110, "GET /metrics HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", resp,
                          sizeof(resp)) == 0);
    EXPECT_TRUE(strstr(resp, "200 OK") != NULL && strstr(resp, "\"kv_count\"") != NULL);
    EXPECT_TRUE(http_call(18110, "POST /health HTTP/1.1\r\nHost: x\r\nContent-Length: 0\r\nConnection: "
                                 "close\r\n\r\n",
                          resp, sizeof(resp)) == 0);
    EXPECT_TRUE(strstr(resp, "405") != NULL && strstr(resp, "Allow:") != NULL);
    EXPECT_TRUE(test_server_stop(&ts) == 0);
    return 0;
}

static int test_api_kv_errors_specific(void) {
    TestServer ts;
    memset(&ts, 0, sizeof(ts));
    EXPECT_TRUE(test_server_start(&ts, 18111) == 0);
    char resp[8192];
    /* Invalid key charset -> 400 invalid_key. */
    EXPECT_TRUE(http_call(18111, "GET /api/kv/bad$key HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", resp,
                          sizeof(resp)) == 0);
    EXPECT_TRUE(strstr(resp, "400") != NULL && strstr(resp, "invalid_key") != NULL);
    /* PUT without Content-Length -> 411. */
    EXPECT_TRUE(http_call(18111,
                          "PUT /api/kv/k1 HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\nConnection: "
                          "close\r\n\r\n{\"value\":\"v\"}",
                          resp, sizeof(resp)) == 0);
    EXPECT_TRUE(strstr(resp, "411") != NULL);
    /* Wrong content type -> 415. */
    const char *plain = "hello";
    char req[1024];
    snprintf(req, sizeof(req),
             "PUT /api/kv/k1 HTTP/1.1\r\nHost: x\r\nContent-Type: text/plain\r\nContent-Length: %zu\r\nConnection: "
             "close\r\n\r\n%s",
             strlen(plain), plain);
    EXPECT_TRUE(http_call(18111, req, resp, sizeof(resp)) == 0);
    EXPECT_TRUE(strstr(resp, "415") != NULL);
    /* Missing field -> 400 missing_field. */
    const char *nobody = "{\"other\":\"x\"}";
    snprintf(req, sizeof(req),
             "PUT /api/kv/k1 HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n"
             "Connection: close\r\n\r\n%s",
             strlen(nobody), nobody);
    EXPECT_TRUE(http_call(18111, req, resp, sizeof(resp)) == 0);
    EXPECT_TRUE(strstr(resp, "400") != NULL && strstr(resp, "missing_field") != NULL);
    /* Bad limit -> 400 invalid_query. */
    EXPECT_TRUE(http_call(18111, "GET /api/kv?limit=999 HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", resp,
                          sizeof(resp)) == 0);
    EXPECT_TRUE(strstr(resp, "400") != NULL);
    EXPECT_TRUE(test_server_stop(&ts) == 0);
    return 0;
}

static int test_auth_protected_mode_401_403_200(void) {
    TestServer ts;
    memset(&ts, 0, sizeof(ts));
    EXPECT_TRUE(test_server_start_key(&ts, 18116, "s3cret-play") == 0);
    char resp[8192];
    /* Health stays public and reports the mode honestly. */
    EXPECT_TRUE(http_call(18116, "GET /health HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", resp,
                          sizeof(resp)) == 0);
    EXPECT_TRUE(strstr(resp, "200 OK") != NULL && strstr(resp, "\"protected\"") != NULL);
    /* Reads stay public even in protected mode. */
    EXPECT_TRUE(http_call(18116, "GET /api/kv/nope HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", resp,
                          sizeof(resp)) == 0);
    EXPECT_TRUE(strstr(resp, "404") != NULL);
    const char *body = "{\"value\":\"v\"}";
    char req[1024];
    /* PUT without key -> 401 unauthorized. */
    snprintf(req, sizeof(req),
             "PUT /api/kv/k1 HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n"
             "Connection: close\r\n\r\n%s",
             strlen(body), body);
    EXPECT_TRUE(http_call(18116, req, resp, sizeof(resp)) == 0);
    EXPECT_TRUE(strstr(resp, "401") != NULL && strstr(resp, "unauthorized") != NULL);
    /* PUT with wrong key -> 403 forbidden. */
    snprintf(req, sizeof(req),
             "PUT /api/kv/k1 HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\nX-API-Key: wrong\r\n"
             "Content-Length: %zu\r\nConnection: close\r\n\r\n%s",
             strlen(body), body);
    EXPECT_TRUE(http_call(18116, req, resp, sizeof(resp)) == 0);
    EXPECT_TRUE(strstr(resp, "403") != NULL && strstr(resp, "forbidden") != NULL);
    /* PUT with the right key -> 200. */
    snprintf(req, sizeof(req),
             "PUT /api/kv/k1 HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\nX-API-Key: s3cret-play\r\n"
             "Content-Length: %zu\r\nConnection: close\r\n\r\n%s",
             strlen(body), body);
    EXPECT_TRUE(http_call(18116, req, resp, sizeof(resp)) == 0);
    EXPECT_TRUE(strstr(resp, "200 OK") != NULL);
    /* POST notes without key -> 401 as well. */
    const char *note = "{\"title\":\"t\",\"body\":\"b\"}";
    snprintf(req, sizeof(req),
             "POST /api/notes HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n"
             "Connection: close\r\n\r\n%s",
             strlen(note), note);
    EXPECT_TRUE(http_call(18116, req, resp, sizeof(resp)) == 0);
    EXPECT_TRUE(strstr(resp, "401") != NULL);
    EXPECT_TRUE(test_server_stop(&ts) == 0);
    return 0;
}

static int test_api_notes_crud(void) {
    TestServer ts;
    memset(&ts, 0, sizeof(ts));
    EXPECT_TRUE(test_server_start(&ts, 18112) == 0);
    char resp[16384];
    const char *create = "{\"title\":\"t1\",\"body\":\"b1\"}";
    char req[2048];
    snprintf(req, sizeof(req),
             "POST /api/notes HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n"
             "Connection: close\r\n\r\n%s",
             strlen(create), create);
    EXPECT_TRUE(http_call(18112, req, resp, sizeof(resp)) == 0);
    EXPECT_TRUE(strstr(resp, "201") != NULL && strstr(resp, "\"t1\"") != NULL);
    EXPECT_TRUE(http_call(18112, "GET /api/notes?limit=10 HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", resp,
                          sizeof(resp)) == 0);
    EXPECT_TRUE(strstr(resp, "200 OK") != NULL && strstr(resp, "\"total\":1") != NULL);
    EXPECT_TRUE(http_call(18112, "GET /api/notes/1 HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", resp,
                          sizeof(resp)) == 0);
    EXPECT_TRUE(strstr(resp, "200 OK") != NULL);
    const char *update = "{\"title\":\"t2\",\"body\":\"b2\"}";
    snprintf(req, sizeof(req),
             "PUT /api/notes/1 HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n"
             "Connection: close\r\n\r\n%s",
             strlen(update), update);
    EXPECT_TRUE(http_call(18112, req, resp, sizeof(resp)) == 0);
    EXPECT_TRUE(strstr(resp, "200 OK") != NULL && strstr(resp, "\"t2\"") != NULL);
    EXPECT_TRUE(http_call(18112, "DELETE /api/notes/1 HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", resp,
                          sizeof(resp)) == 0);
    EXPECT_TRUE(strstr(resp, "204") != NULL);
    EXPECT_TRUE(http_call(18112, "GET /api/notes/1 HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", resp,
                          sizeof(resp)) == 0);
    EXPECT_TRUE(strstr(resp, "404") != NULL);
    /* Validation: empty title 400, bad id 400. */
    const char *bad = "{\"title\":\"\",\"body\":\"b\"}";
    snprintf(req, sizeof(req),
             "POST /api/notes HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n"
             "Connection: close\r\n\r\n%s",
             strlen(bad), bad);
    EXPECT_TRUE(http_call(18112, req, resp, sizeof(resp)) == 0);
    EXPECT_TRUE(strstr(resp, "400") != NULL);
    EXPECT_TRUE(http_call(18112, "GET /api/notes/abc HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", resp,
                          sizeof(resp)) == 0);
    EXPECT_TRUE(strstr(resp, "400") != NULL && strstr(resp, "invalid_id") != NULL);
    EXPECT_TRUE(test_server_stop(&ts) == 0);
    return 0;
}

static int test_api_unknown_route_and_echo(void) {
    TestServer ts;
    memset(&ts, 0, sizeof(ts));
    EXPECT_TRUE(test_server_start(&ts, 18113) == 0);
    char resp[4096];
    EXPECT_TRUE(http_call(18113, "GET /nope HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", resp,
                          sizeof(resp)) == 0);
    EXPECT_TRUE(strstr(resp, "404") != NULL && strstr(resp, "not_found") != NULL);
    const char *echo = "{\"data\":\"hi\"}";
    char req[1024];
    snprintf(req, sizeof(req),
             "POST /api/echo HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n"
             "Connection: close\r\n\r\n%s",
             strlen(echo), echo);
    EXPECT_TRUE(http_call(18113, req, resp, sizeof(resp)) == 0);
    EXPECT_TRUE(strstr(resp, "200 OK") != NULL && strstr(resp, "\"hi\"") != NULL);
    EXPECT_TRUE(http_call(18113, "GET /api/echo HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", resp,
                          sizeof(resp)) == 0);
    EXPECT_TRUE(strstr(resp, "405") != NULL);
    EXPECT_TRUE(test_server_stop(&ts) == 0);
    return 0;
}

static int test_api_root_serves_html(void) {
    TestServer ts;
    memset(&ts, 0, sizeof(ts));
    EXPECT_TRUE(test_server_start(&ts, 18115) == 0);
    char resp[131072];
    EXPECT_TRUE(http_call(18115, "GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", resp, sizeof(resp)) == 0);
    EXPECT_TRUE(strstr(resp, "200 OK") != NULL);
    EXPECT_TRUE(strstr(resp, "text/html") != NULL);
    EXPECT_TRUE(strstr(resp, "C API Server") != NULL);
    EXPECT_TRUE(http_call(18115, "POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 0\r\nConnection: close\r\n\r\n",
                          resp, sizeof(resp)) == 0);
    EXPECT_TRUE(strstr(resp, "405") != NULL);
    EXPECT_TRUE(test_server_stop(&ts) == 0);
    return 0;
}

typedef struct {
    uint16_t port;
    int index;
    int failed;
} WorkerArg;

static void *concurrent_worker(void *arg) {
    WorkerArg *w = (WorkerArg *)arg;
    char req[1024];
    char resp[8192];
    snprintf(req, sizeof(req),
             "PUT /api/kv/c%d HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\nContent-Length: 13\r\n"
             "Connection: close\r\n\r\n{\"value\":\"v\"}",
             w->index);
    if (http_call(w->port, req, resp, sizeof(resp)) != 0 || strstr(resp, "200 OK") == NULL) {
        w->failed = 1;
    }
    return NULL;
}

static int test_api_concurrent_puts_all_succeed(void) {
    TestServer ts;
    memset(&ts, 0, sizeof(ts));
    EXPECT_TRUE(test_server_start(&ts, 18114) == 0);
    pthread_t threads[16];
    WorkerArg args[16];
    for (int i = 0; i < 16; i++) {
        args[i].port = 18114;
        args[i].index = i;
        args[i].failed = 0;
        if (pthread_create(&threads[i], NULL, concurrent_worker, &args[i]) != 0) {
            return 1;
        }
    }
    for (int i = 0; i < 16; i++) {
        pthread_join(threads[i], NULL);
        EXPECT_TRUE(args[i].failed == 0);
    }
    char resp[16384];
    EXPECT_TRUE(http_call(18114, "GET /metrics HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", resp,
                          sizeof(resp)) == 0);
    EXPECT_TRUE(strstr(resp, "\"kv_count\":16") != NULL);
    EXPECT_TRUE(test_server_stop(&ts) == 0);
    return 0;
}

int main(void) {
    RUN_TEST(test_error_status_mapping_correct);
    RUN_TEST(test_http_parse_get_ok);
    RUN_TEST(test_http_parse_query_split_ok);
    RUN_TEST(test_http_parse_unknown_method_is_405);
    RUN_TEST(test_http_parse_chunked_is_501);
    RUN_TEST(test_http_parse_api_key_header_captured);
    RUN_TEST(test_http_parse_garbage_is_400);
    RUN_TEST(test_json_escape_ok);
    RUN_TEST(test_json_escape_overflow_is_error);
    RUN_TEST(test_json_get_string_ok_and_missing);
    RUN_TEST(test_json_get_string_malformed_is_400);
    RUN_TEST(test_store_validate_key_specific);
    RUN_TEST(test_store_kv_crud_roundtrip);
    RUN_TEST(test_store_note_crud_roundtrip);
    RUN_TEST(test_store_validation_specific);
    RUN_TEST(test_api_health_and_metrics);
    RUN_TEST(test_api_kv_errors_specific);
    RUN_TEST(test_auth_protected_mode_401_403_200);
    RUN_TEST(test_api_notes_crud);
    RUN_TEST(test_api_unknown_route_and_echo);
    RUN_TEST(test_api_root_serves_html);
    RUN_TEST(test_api_concurrent_puts_all_succeed);
    printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
