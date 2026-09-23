/*
 * http.c - Minimal HTTP/1.1 request parsing + responses.
 * Single request per connection (Connection: close). No silent fallback:
 * every malformed input yields a specific ApiError and status code.
 */

#include "http.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "server.h"

#define MODULE "http"

int api_error_status(ApiError err) {
    switch (err) {
    case API_OK:
        return 200;
    case API_ERR_BAD_REQUEST:
    case API_ERR_MISSING_FIELD:
    case API_ERR_INVALID_KEY:
    case API_ERR_INVALID_ID:
    case API_ERR_INVALID_QUERY:
        return 400;
    case API_ERR_NOT_FOUND:
        return 404;
    case API_ERR_METHOD_NOT_ALLOWED:
        return 405;
    case API_ERR_LENGTH_REQUIRED:
        return 411;
    case API_ERR_PAYLOAD_TOO_LARGE:
        return 413;
    case API_ERR_UNSUPPORTED_MEDIA:
        return 415;
    case API_ERR_UNAUTHORIZED:
        return 401;
    case API_ERR_FORBIDDEN:
        return 403;
    case API_ERR_NOT_IMPLEMENTED:
        return 501;
    case API_ERR_INTERNAL:
    default:
        return 500;
    }
}

const char *api_error_code(ApiError err) {
    switch (err) {
    case API_OK:
        return "ok";
    case API_ERR_BAD_REQUEST:
        return "bad_request";
    case API_ERR_MISSING_FIELD:
        return "missing_field";
    case API_ERR_INVALID_KEY:
        return "invalid_key";
    case API_ERR_INVALID_ID:
        return "invalid_id";
    case API_ERR_INVALID_QUERY:
        return "invalid_query";
    case API_ERR_NOT_FOUND:
        return "not_found";
    case API_ERR_METHOD_NOT_ALLOWED:
        return "method_not_allowed";
    case API_ERR_LENGTH_REQUIRED:
        return "length_required";
    case API_ERR_PAYLOAD_TOO_LARGE:
        return "payload_too_large";
    case API_ERR_UNSUPPORTED_MEDIA:
        return "unsupported_media_type";
    case API_ERR_UNAUTHORIZED:
        return "unauthorized";
    case API_ERR_FORBIDDEN:
        return "forbidden";
    case API_ERR_NOT_IMPLEMENTED:
        return "not_implemented";
    case API_ERR_INTERNAL:
    default:
        return "internal_error";
    }
}

static HttpMethod method_from_string(const char *text) {
    if (strcmp(text, "GET") == 0) {
        return HTTP_GET;
    }
    if (strcmp(text, "POST") == 0) {
        return HTTP_POST;
    }
    if (strcmp(text, "PUT") == 0) {
        return HTTP_PUT;
    }
    if (strcmp(text, "DELETE") == 0) {
        return HTTP_DELETE;
    }
    return HTTP_UNKNOWN;
}

static const char *reason_for(int status) {
    switch (status) {
    case 200:
        return "OK";
    case 201:
        return "Created";
    case 204:
        return "No Content";
    case 400:
        return "Bad Request";
    case 401:
        return "Unauthorized";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    case 411:
        return "Length Required";
    case 413:
        return "Payload Too Large";
    case 415:
        return "Unsupported Media Type";
    case 500:
        return "Internal Server Error";
    case 501:
        return "Not Implemented";
    default:
        return "Error";
    }
}

static void to_lower_in_place(char *text) {
    for (; *text != '\0'; text++) {
        *text = (char)tolower((unsigned char)*text);
    }
}

static ApiError parse_request_line(const char *line, HttpRequest *out) {
    char method[HTTP_MAX_METHOD_LEN];
    char target[HTTP_MAX_PATH_LEN];
    char version[16];
    /* sscanf is bounded; trailing bytes rejected below. */
    if (sscanf(line, "%7s %2047s %15s", method, target, version) != 3) {
        return API_ERR_BAD_REQUEST;
    }
    if (strncmp(version, "HTTP/1.", 7) != 0) {
        return API_ERR_BAD_REQUEST;
    }
    if (strlen(target) == 0 || target[0] != '/') {
        return API_ERR_BAD_REQUEST;
    }
    if (strlen(target) >= HTTP_MAX_PATH_LEN) {
        return API_ERR_BAD_REQUEST;
    }
    snprintf(out->method_text, sizeof(out->method_text), "%s", method);
    out->method = method_from_string(method);
    /* Split path and query explicitly; empty query stays "". */
    const char *question = strchr(target, '?');
    if (question == NULL) {
        snprintf(out->path, sizeof(out->path), "%s", target);
        out->query[0] = '\0';
    } else {
        size_t path_len = (size_t)(question - target);
        if (path_len >= sizeof(out->path)) {
            return API_ERR_BAD_REQUEST;
        }
        memcpy(out->path, target, path_len);
        out->path[path_len] = '\0';
        snprintf(out->query, sizeof(out->query), "%s", question + 1);
    }
    return API_OK;
}

static ApiError parse_header_line(const char *line, HttpRequest *out) {
    const char *colon = strchr(line, ':');
    if (colon == NULL) {
        return API_ERR_BAD_REQUEST;
    }
    size_t name_len = (size_t)(colon - line);
    if (name_len == 0 || name_len >= 64) {
        return API_ERR_BAD_REQUEST;
    }
    char name[64];
    memcpy(name, line, name_len);
    name[name_len] = '\0';
    to_lower_in_place(name);
    const char *value = colon + 1;
    while (*value == ' ' || *value == '\t') {
        value++;
    }
    if (strcmp(name, "content-length") == 0) {
        char *end = NULL;
        errno = 0;
        long parsed = strtol(value, &end, 10);
        if (errno != 0 || end == value || parsed < 0 || parsed > HTTP_MAX_BODY_SIZE) {
            return parsed > HTTP_MAX_BODY_SIZE ? API_ERR_PAYLOAD_TOO_LARGE : API_ERR_BAD_REQUEST;
        }
        /* Reject trailing garbage explicitly (no lenient parse). */
        while (*end == ' ' || *end == '\t' || *end == '\r') {
            end++;
        }
        if (*end != '\0') {
            return API_ERR_BAD_REQUEST;
        }
        out->content_length = parsed;
    } else if (strcmp(name, "content-type") == 0) {
        snprintf(out->content_type, sizeof(out->content_type), "%s", value);
    } else if (strcmp(name, "x-api-key") == 0) {
        snprintf(out->api_key, sizeof(out->api_key), "%s", value);
    } else if (strcmp(name, "transfer-encoding") == 0) {
        char lowered[128];
        snprintf(lowered, sizeof(lowered), "%s", value);
        to_lower_in_place(lowered);
        if (strstr(lowered, "chunked") != NULL) {
            out->has_chunked = 1;
        }
    }
    return API_OK;
}

ApiError http_parse_request(const char *buf, size_t len, HttpRequest *out) {
    if (buf == NULL || out == NULL || len == 0) {
        return API_ERR_BAD_REQUEST;
    }
    memset(out, 0, sizeof(*out));
    out->content_length = -1;
    /* Find end of headers. Missing terminator is a bad request, not a retry. */
    const char *end = NULL;
    for (size_t i = 0; i + 3 < len; i++) {
        if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n') {
            end = buf + i;
            break;
        }
    }
    if (end == NULL) {
        return API_ERR_BAD_REQUEST;
    }
    size_t header_len = (size_t)(end - buf);
    if (header_len >= HTTP_MAX_HEADERS_SIZE) {
        return API_ERR_PAYLOAD_TOO_LARGE;
    }
    /* Copy headers to a mutable scratch for line splitting. */
    char *scratch = malloc(header_len + 1);
    if (scratch == NULL) {
        return API_ERR_INTERNAL;
    }
    memcpy(scratch, buf, header_len);
    scratch[header_len] = '\0';
    ApiError result = API_OK;
    char *saveptr = NULL;
    char *line = strtok_r(scratch, "\n", &saveptr);
    int line_no = 0;
    while (line != NULL && result == API_OK) {
        size_t line_len = strlen(line);
        if (line_len > 0 && line[line_len - 1] == '\r') {
            line[line_len - 1] = '\0';
        }
        if (line_no == 0) {
            result = parse_request_line(line, out);
        } else if (line[0] != '\0') {
            result = parse_header_line(line, out);
        }
        line = strtok_r(NULL, "\n", &saveptr);
        line_no++;
    }
    free(scratch);
    if (result != API_OK) {
        return result;
    }
    if (line_no == 0) {
        return API_ERR_BAD_REQUEST;
    }
    if (out->method == HTTP_UNKNOWN) {
        return API_ERR_METHOD_NOT_ALLOWED;
    }
    if (out->has_chunked) {
        return API_ERR_NOT_IMPLEMENTED;
    }
    return API_OK;
}

static ssize_t recv_one(int fd, char *buf, size_t cap) {
    ssize_t got = recv(fd, buf, cap, 0);
    if (got < 0 && errno == EINTR) {
        return recv_one(fd, buf, cap);
    }
    return got;
}

ApiError http_read_request(int fd, char *header_buf, size_t header_cap, char *body_buf, size_t body_cap,
                           HttpRequest *out) {
    if (header_buf == NULL || body_buf == NULL || out == NULL) {
        return API_ERR_INTERNAL;
    }
    if (header_cap < 512 || body_cap < 1) {
        return API_ERR_INTERNAL;
    }
    size_t total = 0;
    size_t header_end = 0;
    int found = 0;
    while (total + 1 < header_cap) {
        ssize_t got = recv_one(fd, header_buf + total, header_cap - 1 - total);
        if (got < 0) {
            return API_ERR_BAD_REQUEST;
        }
        if (got == 0) {
            return total == 0 ? API_ERR_BAD_REQUEST : API_ERR_BAD_REQUEST;
        }
        total += (size_t)got;
        header_buf[total] = '\0';
        for (size_t i = 0; i + 3 < total; i++) {
            if (header_buf[i] == '\r' && header_buf[i + 1] == '\n' && header_buf[i + 2] == '\r' &&
                header_buf[i + 3] == '\n') {
                header_end = i + 4;
                found = 1;
                break;
            }
        }
        if (found) {
            break;
        }
        if (total >= HTTP_MAX_HEADERS_SIZE) {
            return API_ERR_PAYLOAD_TOO_LARGE;
        }
    }
    if (!found) {
        return API_ERR_BAD_REQUEST;
    }
    ApiError parsed = http_parse_request(header_buf, total, out);
    if (parsed != API_OK) {
        return parsed;
    }
    size_t already = total - header_end;
    long need = out->content_length < 0 ? 0 : out->content_length;
    if (need > 0 && (size_t)need > body_cap) {
        return API_ERR_PAYLOAD_TOO_LARGE;
    }
    if ((size_t)need > HTTP_MAX_BODY_SIZE) {
        return API_ERR_PAYLOAD_TOO_LARGE;
    }
    if (already > body_cap) {
        return API_ERR_PAYLOAD_TOO_LARGE;
    }
    /* Bytes after headers pipelined in the same recv already belong to the body. */
    size_t copy_now = already < (size_t)need ? already : (size_t)need;
    if (copy_now > 0) {
        memcpy(body_buf, header_buf + header_end, copy_now);
    }
    size_t have = copy_now;
    while (have < (size_t)need) {
        ssize_t got = recv_one(fd, body_buf + have, (size_t)need - have);
        if (got < 0) {
            return API_ERR_BAD_REQUEST;
        }
        if (got == 0) {
            return API_ERR_BAD_REQUEST;
        }
        have += (size_t)got;
    }
    out->body = need == 0 ? NULL : body_buf;
    out->body_len = have;
    return API_OK;
}

int http_respond(int fd, int status, const char *content_type, const char *body, size_t body_len,
                 const char *allow) {
    if (content_type == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (body == NULL && body_len != 0) {
        errno = EINVAL;
        return -1;
    }
    char header[1024];
    int written;
    if (allow != NULL) {
        written = snprintf(header, sizeof(header),
                           "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nAllow: %s\r\n"
                           "Connection: close\r\n\r\n",
                           status, reason_for(status), content_type, body_len, allow);
    } else {
        written = snprintf(header, sizeof(header),
                           "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
                           "Connection: close\r\n\r\n",
                           status, reason_for(status), content_type, body_len);
    }
    if (written < 0 || (size_t)written >= sizeof(header)) {
        errno = EMSGSIZE;
        return -1;
    }
    if (server_send_all(fd, header, (size_t)written) != 0) {
        return -1;
    }
    if (body_len > 0 && server_send_all(fd, body, body_len) != 0) {
        return -1;
    }
    return 0;
}

int http_respond_error(int fd, ApiError err, const char *detail) {
    if (err == API_OK) {
        err = API_ERR_INTERNAL;
    }
    if (detail == NULL) {
        detail = "error";
    }
    int status = api_error_status(err);
    const char *code = api_error_code(err);
    char body[1024];
    /* Detail is bounded and quoted safely: escape quotes/backslash minimally. */
    char safe_detail[512];
    size_t pos = 0;
    for (const char *p = detail; *p != '\0' && pos + 2 < sizeof(safe_detail); p++) {
        if (*p == '"' || *p == '\\') {
            safe_detail[pos++] = '\\';
        }
        safe_detail[pos++] = *p;
    }
    safe_detail[pos] = '\0';
    int written = snprintf(body, sizeof(body), "{\"error\":\"%s\",\"message\":\"%s\"}", code, safe_detail);
    if (written < 0 || (size_t)written >= sizeof(body)) {
        const char *fallback = "{\"error\":\"internal_error\",\"message\":\"encode overflow\"}";
        return http_respond(fd, 500, "application/json", fallback, strlen(fallback), NULL);
    }
    const char *allow = err == API_ERR_METHOD_NOT_ALLOWED ? "GET, POST, PUT, DELETE" : NULL;
    return http_respond(fd, status, "application/json", body, (size_t)written, allow);
}

int http_query_get(const char *query, const char *key, char *out, size_t out_cap) {
    if (query == NULL || key == NULL || out == NULL || out_cap == 0) {
        return -1;
    }
    size_t key_len = strlen(key);
    const char *cursor = query;
    while (*cursor != '\0') {
        const char *amp = strchr(cursor, '&');
        size_t pair_len = amp == NULL ? strlen(cursor) : (size_t)(amp - cursor);
        const char *eq = memchr(cursor, '=', pair_len);
        size_t name_len = eq == NULL ? pair_len : (size_t)(eq - cursor);
        if (name_len == key_len && memcmp(cursor, key, key_len) == 0) {
            const char *value = eq == NULL ? "" : eq + 1;
            size_t value_len = eq == NULL ? 0 : pair_len - name_len - 1;
            if (value_len + 1 > out_cap) {
                return -2;
            }
            for (size_t i = 0; i < value_len; i++) {
                out[i] = value[i] == '+' ? ' ' : value[i];
            }
            out[value_len] = '\0';
            return 0;
        }
        if (amp == NULL) {
            break;
        }
        cursor = amp + 1;
    }
    return -1;
}
