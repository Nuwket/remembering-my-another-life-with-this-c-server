/*
 * api.c - Router + handlers. Every path returns an explicit status;
 * unknown routes are 404, wrong methods are 405 with Allow. No fallback to 200.
 */

#include "api.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json.h"
#include "log.h"

#define MODULE "api"
#define ALLOW_ALL "GET, POST, PUT, DELETE"
#define ALLOW_GET "GET"
#define ALLOW_GET_POST "GET, POST"
#define ALLOW_GET_PUT_DELETE "GET, PUT, DELETE"

ApiError api_require_json(const HttpRequest *req, int body_required) {
    if (req == NULL) {
        return API_ERR_BAD_REQUEST;
    }
    if (body_required && req->content_length < 0) {
        return API_ERR_LENGTH_REQUIRED;
    }
    if (body_required && req->body == NULL) {
        return API_ERR_BAD_REQUEST;
    }
    char lowered[128];
    snprintf(lowered, sizeof(lowered), "%s", req->content_type);
    for (char *p = lowered; *p != '\0'; p++) {
        *p = (char)tolower((unsigned char)*p);
    }
    if (strstr(lowered, "application/json") == NULL) {
        return API_ERR_UNSUPPORTED_MEDIA;
    }
    return API_OK;
}

static int send_json(int fd, int status, const char *body) {
    size_t len = body == NULL ? 0 : strlen(body);
    return http_respond(fd, status, "application/json", body, len, NULL);
}

static int send_no_content(int fd) {
    return http_respond(fd, 204, "application/json", NULL, 0, NULL);
}

static int send_method_not_allowed(int fd, const char *allow) {
    char body[256];
    int written = snprintf(body, sizeof(body), "{\"error\":\"method_not_allowed\",\"message\":\"use %s\"}", allow);
    if (written < 0 || (size_t)written >= sizeof(body)) {
        return http_respond_error(fd, API_ERR_INTERNAL, "encode overflow");
    }
    return http_respond(fd, 405, "application/json", body, (size_t)written, allow);
}

static ApiError parse_limit(const char *query, int *limit_out) {
    char text[32] = "";
    int found = http_query_get(query, "limit", text, sizeof(text));
    if (found == -2) {
        return API_ERR_INVALID_QUERY;
    }
    if (found != 0) {
        *limit_out = 50;
        return API_OK;
    }
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || value <= 0 || value > STORE_MAX_LIST) {
        return API_ERR_INVALID_QUERY;
    }
    *limit_out = (int)value;
    return API_OK;
}

static ApiError parse_offset(const char *query, int *offset_out) {
    char text[32] = "";
    int found = http_query_get(query, "offset", text, sizeof(text));
    if (found == -2) {
        return API_ERR_INVALID_QUERY;
    }
    if (found != 0) {
        *offset_out = 0;
        return API_OK;
    }
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || value < 0 || value > 1000000) {
        return API_ERR_INVALID_QUERY;
    }
    *offset_out = (int)value;
    return API_OK;
}

static ApiError parse_note_id(const char *path, long *id_out) {
    const char *prefix = "/api/notes/";
    size_t base = strlen(prefix);
    if (strncmp(path, prefix, base) != 0 || path[base] == '\0') {
        return API_ERR_INVALID_ID;
    }
    if (strchr(path + base, '/') != NULL) {
        return API_ERR_NOT_FOUND;
    }
    char *end = NULL;
    long value = strtol(path + base, &end, 10);
    if (end == path + base || *end != '\0' || value <= 0) {
        return API_ERR_INVALID_ID;
    }
    *id_out = value;
    return API_OK;
}

static ApiError handle_health(int fd, Server *server) {
    long uptime = server_uptime_s(server);
    char body[256];
    int written =
        snprintf(body, sizeof(body), "{\"status\":\"ok\",\"version\":\"%s\",\"uptime_s\":%ld}", SERVER_VERSION, uptime);
    if (written < 0 || (size_t)written >= sizeof(body)) {
        http_respond_error(fd, API_ERR_INTERNAL, "encode overflow");
        return API_ERR_INTERNAL;
    }
    if (send_json(fd, 200, body) != 0) {
        return API_ERR_INTERNAL;
    }
    return API_OK;
}

static ApiError handle_metrics(int fd, Store *store, Server *server) {
    struct ServerStats stats;
    server_stats(server, &stats);
    long kv_count = 0;
    long note_count = 0;
    ApiError counted = store_counts(store, &kv_count, &note_count);
    if (counted != API_OK) {
        http_respond_error(fd, counted, "store count failed");
        return counted;
    }
    char body[512];
    int written = snprintf(body, sizeof(body),
                           "{\"connections_accepted\":%lu,\"connections_handled\":%lu,"
                           "\"connections_dropped_queue_full\":%lu,\"http_requests\":%lu,\"http_errors\":%lu,"
                           "\"kv_count\":%ld,\"notes_count\":%ld}",
                           stats.connections_accepted, stats.connections_handled,
                           stats.connections_dropped_queue_full, stats.http_requests, stats.http_errors, kv_count,
                           note_count);
    if (written < 0 || (size_t)written >= sizeof(body)) {
        http_respond_error(fd, API_ERR_INTERNAL, "encode overflow");
        return API_ERR_INTERNAL;
    }
    if (send_json(fd, 200, body) != 0) {
        return API_ERR_INTERNAL;
    }
    return API_OK;
}

static ApiError handle_echo(int fd, const HttpRequest *req) {
    ApiError checked = api_require_json(req, 1);
    if (checked != API_OK) {
        http_respond_error(fd, checked, checked == API_ERR_UNSUPPORTED_MEDIA ? "use application/json"
                                                                             : "body required");
        return checked;
    }
    char data[HTTP_MAX_BODY_SIZE];
    ApiError parsed = json_get_string(req->body, req->body_len, "data", data, sizeof(data));
    if (parsed != API_OK) {
        http_respond_error(fd, parsed, parsed == API_ERR_MISSING_FIELD ? "missing field: data" : "invalid JSON");
        return parsed;
    }
    char escaped[HTTP_MAX_BODY_SIZE + 128];
    if (json_escape(escaped, sizeof(escaped), data) != 0) {
        http_respond_error(fd, API_ERR_INTERNAL, "encode overflow");
        return API_ERR_INTERNAL;
    }
    char body[HTTP_MAX_BODY_SIZE + 256];
    int written = snprintf(body, sizeof(body), "{\"data\":\"%s\"}", escaped);
    if (written < 0 || (size_t)written >= sizeof(body)) {
        http_respond_error(fd, API_ERR_INTERNAL, "encode overflow");
        return API_ERR_INTERNAL;
    }
    if (send_json(fd, 200, body) != 0) {
        return API_ERR_INTERNAL;
    }
    return API_OK;
}

static ApiError handle_kv_put(int fd, const HttpRequest *req, Store *store, const char *key) {
    ApiError checked = api_require_json(req, 1);
    if (checked != API_OK) {
        http_respond_error(fd, checked, "body with {\"value\"} required as application/json");
        return checked;
    }
    char value[STORE_MAX_VALUE_LEN + 1];
    ApiError parsed = json_get_string(req->body, req->body_len, "value", value, sizeof(value));
    if (parsed != API_OK) {
        http_respond_error(fd, parsed, parsed == API_ERR_MISSING_FIELD ? "missing field: value" : "invalid JSON");
        return parsed;
    }
    ApiError stored = store_kv_put(store, key, value);
    if (stored != API_OK) {
        http_respond_error(fd, stored, stored == API_ERR_INVALID_KEY ? "invalid key" : "store write failed");
        return stored;
    }
    char escaped[STORE_MAX_VALUE_LEN * 2 + 1];
    char ekey[STORE_MAX_KEY_LEN * 2 + 1];
    if (json_escape(escaped, sizeof(escaped), value) != 0 || json_escape(ekey, sizeof(ekey), key) != 0) {
        http_respond_error(fd, API_ERR_INTERNAL, "encode overflow");
        return API_ERR_INTERNAL;
    }
    char body[STORE_MAX_VALUE_LEN * 2 + 512];
    int written = snprintf(body, sizeof(body), "{\"key\":\"%s\",\"value\":\"%s\"}", ekey, escaped);
    if (written < 0 || (size_t)written >= sizeof(body)) {
        http_respond_error(fd, API_ERR_INTERNAL, "encode overflow");
        return API_ERR_INTERNAL;
    }
    if (send_json(fd, 200, body) != 0) {
        return API_ERR_INTERNAL;
    }
    return API_OK;
}

static ApiError handle_kv_get(int fd, Store *store, const char *key) {
    char value[STORE_MAX_VALUE_LEN + 1];
    ApiError got = store_kv_get(store, key, value, sizeof(value));
    if (got != API_OK) {
        http_respond_error(fd, got, got == API_ERR_NOT_FOUND ? "key not found" : "store read failed");
        return got;
    }
    char escaped[STORE_MAX_VALUE_LEN * 2 + 1];
    char ekey[STORE_MAX_KEY_LEN * 2 + 1];
    if (json_escape(escaped, sizeof(escaped), value) != 0 || json_escape(ekey, sizeof(ekey), key) != 0) {
        http_respond_error(fd, API_ERR_INTERNAL, "encode overflow");
        return API_ERR_INTERNAL;
    }
    char body[STORE_MAX_VALUE_LEN * 2 + 512];
    int written = snprintf(body, sizeof(body), "{\"key\":\"%s\",\"value\":\"%s\"}", ekey, escaped);
    if (written < 0 || (size_t)written >= sizeof(body)) {
        http_respond_error(fd, API_ERR_INTERNAL, "encode overflow");
        return API_ERR_INTERNAL;
    }
    if (send_json(fd, 200, body) != 0) {
        return API_ERR_INTERNAL;
    }
    return API_OK;
}

static ApiError handle_kv_del(int fd, Store *store, const char *key) {
    ApiError deleted = store_kv_del(store, key);
    if (deleted != API_OK) {
        http_respond_error(fd, deleted, deleted == API_ERR_NOT_FOUND ? "key not found" : "store delete failed");
        return deleted;
    }
    if (send_no_content(fd) != 0) {
        return API_ERR_INTERNAL;
    }
    return API_OK;
}

static ApiError handle_kv_list(int fd, const HttpRequest *req, Store *store) {
    char prefix[STORE_MAX_KEY_LEN + 1] = "";
    int found = http_query_get(req->query, "prefix", prefix, sizeof(prefix));
    if (found == -2) {
        http_respond_error(fd, API_ERR_INVALID_QUERY, "prefix too long");
        return API_ERR_INVALID_QUERY;
    }
    if (found == 0 && store_validate_key(prefix) != API_OK && prefix[0] != '\0') {
        http_respond_error(fd, API_ERR_INVALID_QUERY, "invalid prefix charset");
        return API_ERR_INVALID_QUERY;
    }
    int limit = 50;
    ApiError parsed = parse_limit(req->query, &limit);
    if (parsed != API_OK) {
        http_respond_error(fd, parsed, "limit must be 1..100");
        return parsed;
    }
    static char items[HTTP_MAX_BODY_SIZE];
    ApiError listed = store_kv_list(store, prefix, limit, items, sizeof(items));
    if (listed != API_OK) {
        http_respond_error(fd, listed, "store list failed");
        return listed;
    }
    char body[HTTP_MAX_BODY_SIZE];
    int written = snprintf(body, sizeof(body), "{\"items\":%s}", items);
    if (written < 0 || (size_t)written >= sizeof(body)) {
        http_respond_error(fd, API_ERR_INTERNAL, "encode overflow");
        return API_ERR_INTERNAL;
    }
    if (send_json(fd, 200, body) != 0) {
        return API_ERR_INTERNAL;
    }
    return API_OK;
}

static ApiError handle_note_create(int fd, const HttpRequest *req, Store *store) {
    ApiError checked = api_require_json(req, 1);
    if (checked != API_OK) {
        http_respond_error(fd, checked, "body with {\"title\",\"body\"} required");
        return checked;
    }
    char title[STORE_MAX_TITLE_LEN + 1];
    char note_body[STORE_MAX_BODY_LEN + 1];
    ApiError parsed = json_get_string(req->body, req->body_len, "title", title, sizeof(title));
    if (parsed != API_OK) {
        http_respond_error(fd, parsed, parsed == API_ERR_MISSING_FIELD ? "missing field: title" : "invalid JSON");
        return parsed;
    }
    parsed = json_get_string(req->body, req->body_len, "body", note_body, sizeof(note_body));
    if (parsed != API_OK) {
        http_respond_error(fd, parsed, parsed == API_ERR_MISSING_FIELD ? "missing field: body" : "invalid JSON");
        return parsed;
    }
    long id = 0;
    ApiError created = store_note_create(store, title, note_body, &id);
    if (created != API_OK) {
        const char *detail = created == API_ERR_PAYLOAD_TOO_LARGE ? "title 1..200, body 0..8192"
                             : created == API_ERR_BAD_REQUEST      ? "title must be 1..200 chars"
                                                                   : "store write failed";
        http_respond_error(fd, created, detail);
        return created;
    }
    static char created_json[STORE_MAX_BODY_LEN * 2 + 1024];
    ApiError got = store_note_get(store, id, created_json, sizeof(created_json));
    if (got != API_OK) {
        http_respond_error(fd, got, "created but re-read failed");
        return got;
    }
    if (http_respond(fd, 201, "application/json", created_json, strlen(created_json), NULL) != 0) {
        return API_ERR_INTERNAL;
    }
    return API_OK;
}

static ApiError handle_note_list(int fd, const HttpRequest *req, Store *store) {
    int limit = 50;
    int offset = 0;
    if (parse_limit(req->query, &limit) != API_OK || parse_offset(req->query, &offset) != API_OK) {
        http_respond_error(fd, API_ERR_INVALID_QUERY, "limit 1..100, offset >= 0");
        return API_ERR_INVALID_QUERY;
    }
    static char body[HTTP_MAX_BODY_SIZE];
    long total = 0;
    ApiError listed = store_note_list(store, limit, offset, body, sizeof(body), &total);
    if (listed != API_OK) {
        http_respond_error(fd, listed, "store list failed");
        return listed;
    }
    if (send_json(fd, 200, body) != 0) {
        return API_ERR_INTERNAL;
    }
    return API_OK;
}

static ApiError handle_note_get(int fd, Store *store, long id) {
    static char body[STORE_MAX_BODY_LEN * 2 + 1024];
    ApiError got = store_note_get(store, id, body, sizeof(body));
    if (got != API_OK) {
        http_respond_error(fd, got, got == API_ERR_NOT_FOUND ? "note not found" : "invalid id");
        return got;
    }
    if (send_json(fd, 200, body) != 0) {
        return API_ERR_INTERNAL;
    }
    return API_OK;
}

static ApiError handle_note_update(int fd, const HttpRequest *req, Store *store, long id) {
    ApiError checked = api_require_json(req, 1);
    if (checked != API_OK) {
        http_respond_error(fd, checked, "body with {\"title\",\"body\"} required");
        return checked;
    }
    char title[STORE_MAX_TITLE_LEN + 1];
    char note_body[STORE_MAX_BODY_LEN + 1];
    if (json_get_string(req->body, req->body_len, "title", title, sizeof(title)) != API_OK ||
        json_get_string(req->body, req->body_len, "body", note_body, sizeof(note_body)) != API_OK) {
        http_respond_error(fd, API_ERR_BAD_REQUEST, "missing field: title/body");
        return API_ERR_BAD_REQUEST;
    }
    ApiError updated = store_note_update(store, id, title, note_body);
    if (updated != API_OK) {
        const char *detail = updated == API_ERR_NOT_FOUND ? "note not found" : "invalid title/body";
        http_respond_error(fd, updated, detail);
        return updated;
    }
    return handle_note_get(fd, store, id);
}

static ApiError handle_note_del(int fd, Store *store, long id) {
    ApiError deleted = store_note_del(store, id);
    if (deleted != API_OK) {
        http_respond_error(fd, deleted, deleted == API_ERR_NOT_FOUND ? "note not found" : "invalid id");
        return deleted;
    }
    if (send_no_content(fd) != 0) {
        return API_ERR_INTERNAL;
    }
    return API_OK;
}

static ApiError dispatch_kv(int fd, const HttpRequest *req, Store *store) {
    const char *base = "/api/kv";
    if (strcmp(req->path, base) == 0 || strcmp(req->path, "/api/kv/") == 0) {
        if (req->method != HTTP_GET) {
            send_method_not_allowed(fd, ALLOW_GET);
            return API_ERR_METHOD_NOT_ALLOWED;
        }
        return handle_kv_list(fd, req, store);
    }
    const char *prefix = "/api/kv/";
    if (strncmp(req->path, prefix, strlen(prefix)) != 0) {
        http_respond_error(fd, API_ERR_NOT_FOUND, "unknown route");
        return API_ERR_NOT_FOUND;
    }
    const char *key = req->path + strlen(prefix);
    if (strchr(key, '/') != NULL || store_validate_key(key) != API_OK) {
        http_respond_error(fd, API_ERR_INVALID_KEY, "key must match [A-Za-z0-9._-]{1,128}");
        return API_ERR_INVALID_KEY;
    }
    switch (req->method) {
    case HTTP_GET:
        return handle_kv_get(fd, store, key);
    case HTTP_PUT:
        return handle_kv_put(fd, req, store, key);
    case HTTP_DELETE:
        return handle_kv_del(fd, store, key);
    default:
        send_method_not_allowed(fd, ALLOW_GET_PUT_DELETE);
        return API_ERR_METHOD_NOT_ALLOWED;
    }
}

static ApiError dispatch_notes(int fd, const HttpRequest *req, Store *store) {
    if (strcmp(req->path, "/api/notes") == 0 || strcmp(req->path, "/api/notes/") == 0) {
        if (req->method == HTTP_POST) {
            return handle_note_create(fd, req, store);
        }
        if (req->method == HTTP_GET) {
            return handle_note_list(fd, req, store);
        }
        send_method_not_allowed(fd, ALLOW_GET_POST);
        return API_ERR_METHOD_NOT_ALLOWED;
    }
    long id = 0;
    ApiError parsed = parse_note_id(req->path, &id);
    if (parsed == API_ERR_NOT_FOUND) {
        http_respond_error(fd, parsed, "unknown route");
        return parsed;
    }
    if (parsed != API_OK) {
        http_respond_error(fd, parsed, "id must be a positive integer");
        return parsed;
    }
    switch (req->method) {
    case HTTP_GET:
        return handle_note_get(fd, store, id);
    case HTTP_PUT:
        return handle_note_update(fd, req, store, id);
    case HTTP_DELETE:
        return handle_note_del(fd, store, id);
    default:
        send_method_not_allowed(fd, ALLOW_GET_PUT_DELETE);
        return API_ERR_METHOD_NOT_ALLOWED;
    }
}

ApiError api_dispatch(int fd, const HttpRequest *req, Store *store, Server *server) {
    if (req == NULL || store == NULL || server == NULL) {
        http_respond_error(fd, API_ERR_INTERNAL, "missing context");
        return API_ERR_INTERNAL;
    }
    if (strcmp(req->path, "/health") == 0) {
        if (req->method != HTTP_GET) {
            send_method_not_allowed(fd, ALLOW_GET);
            return API_ERR_METHOD_NOT_ALLOWED;
        }
        return handle_health(fd, server);
    }
    if (strcmp(req->path, "/metrics") == 0) {
        if (req->method != HTTP_GET) {
            send_method_not_allowed(fd, ALLOW_GET);
            return API_ERR_METHOD_NOT_ALLOWED;
        }
        return handle_metrics(fd, store, server);
    }
    if (strcmp(req->path, "/api/echo") == 0) {
        if (req->method != HTTP_POST) {
            send_method_not_allowed(fd, "POST");
            return API_ERR_METHOD_NOT_ALLOWED;
        }
        return handle_echo(fd, req);
    }
    if (strncmp(req->path, "/api/kv", 7) == 0) {
        return dispatch_kv(fd, req, store);
    }
    if (strncmp(req->path, "/api/notes", 10) == 0) {
        return dispatch_notes(fd, req, store);
    }
    LOG_INFO(MODULE, "unknown route");
    http_respond_error(fd, API_ERR_NOT_FOUND, "unknown route");
    return API_ERR_NOT_FOUND;
}
