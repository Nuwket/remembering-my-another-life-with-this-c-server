#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

#define HTTP_MAX_HEADERS_SIZE 16384
#define HTTP_MAX_BODY_SIZE 65536
#define HTTP_MAX_PATH_LEN 2048
#define HTTP_MAX_METHOD_LEN 8

/* Typed API errors. Every failure maps to exactly one value; no silent fallback.
 * api_error_status() is total: unknown values map to 500 explicitly. */

typedef enum {
    API_OK = 0,
    API_ERR_BAD_REQUEST,
    API_ERR_MISSING_FIELD,
    API_ERR_INVALID_KEY,
    API_ERR_INVALID_ID,
    API_ERR_INVALID_QUERY,
    API_ERR_NOT_FOUND,
    API_ERR_METHOD_NOT_ALLOWED,
    API_ERR_LENGTH_REQUIRED,
    API_ERR_PAYLOAD_TOO_LARGE,
    API_ERR_UNSUPPORTED_MEDIA,
    API_ERR_NOT_IMPLEMENTED,
    API_ERR_INTERNAL
} ApiError;

typedef enum {
    HTTP_GET,
    HTTP_POST,
    HTTP_PUT,
    HTTP_DELETE,
    HTTP_UNKNOWN
} HttpMethod;

typedef struct {
    HttpMethod method;
    char method_text[HTTP_MAX_METHOD_LEN];
    char path[HTTP_MAX_PATH_LEN]; /* path without query string */
    char query[HTTP_MAX_PATH_LEN]; /* empty string when absent */
    long content_length; /* -1 when header absent */
    char content_type[128];
    int has_chunked; /* 1 when Transfer-Encoding: chunked detected */
    const char *body; /* points into caller-owned buffer, may be NULL */
    size_t body_len;
} HttpRequest;

/*
 * api_error_status - Map typed error to HTTP status. Total function.
 * @err: typed error (any int; out-of-range yields 500, never 200).
 * Returns: 200, 400, 404, 405, 411, 413, 415, 500, or 501.
 */
int api_error_status(ApiError err);

/*
 * api_error_code - Stable machine-readable code for JSON bodies.
 * @err: typed error. Returns: e.g. "bad_request", never NULL.
 */
const char *api_error_code(ApiError err);

/*
 * http_parse_request - Parse a complete header block (pure, testable).
 * @buf: bytes containing headers + optional partial body. @len: byte count.
 * @out: filled on success (body pointer aliases @buf).
 * Returns: API_OK, or specific error (BAD_REQUEST, LENGTH_REQUIRED, etc).
 * Ownership: @buf stays with caller. Thread-safe (no shared state).
 */
ApiError http_parse_request(const char *buf, size_t len, HttpRequest *out);

/*
 * http_read_request - Read one full HTTP request from a socket.
 * @fd: connected socket. @header_buf/@header_cap: caller stack buffer.
 * @body_buf/@body_cap: caller stack buffer for the body.
 * @out: filled request (body aliases @body_buf when present).
 * Returns: API_OK or specific error. Handles partial recv + EINTR.
 * Thread-safe (per-connection state only).
 */
ApiError http_read_request(int fd, char *header_buf, size_t header_cap, char *body_buf, size_t body_cap,
                           HttpRequest *out);

/*
 * http_respond - Send a complete HTTP response.
 * @fd: socket. @status: e.g. 200. @content_type: e.g. "application/json".
 * @body/@body_len: payload (may be NULL/0). @allow: Allow header or NULL.
 * Returns: 0 on success, -1 with errno on send failure. No fallback.
 */
int http_respond(int fd, int status, const char *content_type, const char *body, size_t body_len,
                 const char *allow);

/*
 * http_respond_error - Send a JSON error with the correct status code.
 * @fd: socket. @err: typed error (never API_OK; API_OK yields 500 defensively).
 * @detail: human message (validated non-NULL, caller-owned).
 * Returns: 0/-1 like http_respond. Always sends a body, never empty fallback.
 */
int http_respond_error(int fd, ApiError err, const char *detail);

/*
 * http_query_get - Extract a query param value (no decoding except '+'->space).
 * @query: string after '?' (may be ""). @key: param name.
 * @out/@out_cap: destination (always NUL-terminated on success).
 * Returns: 0 found, -1 absent, -2 truncated (caller maps to 400, no silent cut).
 */
int http_query_get(const char *query, const char *key, char *out, size_t out_cap);

#endif
