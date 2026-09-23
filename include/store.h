#ifndef STORE_H
#define STORE_H

#include <stddef.h>

#include "http.h"

#define STORE_MAX_KEY_LEN 128
#define STORE_MAX_VALUE_LEN 60000
#define STORE_MAX_TITLE_LEN 200
#define STORE_MAX_BODY_LEN 8192
#define STORE_MAX_LIST 100

typedef struct Store Store;

/*
 * store_open - Open (or create) the SQLite database. No fallback.
 * @path: filesystem path (must be non-NULL, e.g. "./data/app.db").
 * @errbuf/@err_cap: specific failure message output (always NUL-terminated).
 * Returns: new Store or NULL with @errbuf filled (open, pragma, or DDL error).
 * Ownership: caller must call store_close(). Thread-safe after open.
 */
Store *store_open(const char *path, char *errbuf, size_t err_cap);

/*
 * store_close - Checkpoint WAL and free. NULL is a no-op.
 * Ownership: do not use after.
 */
void store_close(Store *store);

/* All below are thread-safe via internal mutex. Every sqlite rc is checked;
 * failures return API_ERR_INTERNAL explicitly, never silent success. */

ApiError store_validate_key(const char *key);

ApiError store_kv_put(Store *store, const char *key, const char *value);
ApiError store_kv_get(Store *store, const char *key, char *value_out, size_t value_cap);
ApiError store_kv_del(Store *store, const char *key);

/*
 * store_kv_list - List keys with optional prefix into JSON array buffer.
 * @prefix: "" means all. @limit: 1..STORE_MAX_LIST (else INVALID_QUERY).
 * @out_json/@out_cap: e.g. [{"key":"a","value":"b"}]. Overflow -> PAYLOAD_TOO_LARGE.
 */
ApiError store_kv_list(Store *store, const char *prefix, int limit, char *out_json, size_t out_cap);

ApiError store_note_create(Store *store, const char *title, const char *body, long *id_out);
ApiError store_note_list(Store *store, int limit, int offset, char *out_json, size_t out_cap,
                         long *total_out);
ApiError store_note_get(Store *store, long id, char *out_json, size_t out_cap);
ApiError store_note_update(Store *store, long id, const char *title, const char *body);
ApiError store_note_del(Store *store, long id);
ApiError store_counts(Store *store, long *kv_count, long *note_count);

#endif
