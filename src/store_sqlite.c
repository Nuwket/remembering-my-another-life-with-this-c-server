/*
 * store_sqlite.c - SQLite WAL-backed kv + notes. Single connection + mutex.
 * Every sqlite3 rc is checked; errors propagate as API_ERR_INTERNAL with
 * no silent fallback. Statements are prepared per call (no cross-thread sharing).
 */

#include "store.h"

#include <pthread.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "json.h"

struct Store {
    sqlite3 *db;
    pthread_mutex_t mutex;
};

static void set_err(char *errbuf, size_t cap, const char *msg) {
    if (errbuf == NULL || cap == 0) {
        return;
    }
    snprintf(errbuf, cap, "%s", msg);
}

static int exec_checked(sqlite3 *db, const char *sql) {
    char *msg = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &msg);
    if (rc != SQLITE_OK) {
        sqlite3_free(msg);
        return -1;
    }
    return 0;
}

Store *store_open(const char *path, char *errbuf, size_t err_cap) {
    if (path == NULL || path[0] == '\0') {
        set_err(errbuf, err_cap, "db path is required");
        return NULL;
    }
    Store *store = calloc(1, sizeof(Store));
    if (store == NULL) {
        set_err(errbuf, err_cap, "out of memory");
        return NULL;
    }
    if (pthread_mutex_init(&store->mutex, NULL) != 0) {
        set_err(errbuf, err_cap, "mutex init failed");
        free(store);
        return NULL;
    }
    int rc = sqlite3_open(path, &store->db);
    if (rc != SQLITE_OK) {
        snprintf(errbuf, err_cap, "sqlite open failed: %s", sqlite3_errmsg(store->db));
        sqlite3_close(store->db);
        pthread_mutex_destroy(&store->mutex);
        free(store);
        return NULL;
    }
    sqlite3_busy_timeout(store->db, 5000);
    if (exec_checked(store->db, "PRAGMA journal_mode=WAL;") != 0 ||
        exec_checked(store->db, "PRAGMA synchronous=NORMAL;") != 0 ||
        exec_checked(store->db, "PRAGMA foreign_keys=ON;") != 0 ||
        exec_checked(store->db, "CREATE TABLE IF NOT EXISTS kv(key TEXT PRIMARY KEY, value TEXT NOT NULL, "
                                "updated_at INTEGER NOT NULL);") != 0 ||
        exec_checked(store->db, "CREATE TABLE IF NOT EXISTS notes(id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                "title TEXT NOT NULL, body TEXT NOT NULL, created_at INTEGER NOT NULL, "
                                "updated_at INTEGER NOT NULL);") != 0) {
        snprintf(errbuf, err_cap, "sqlite init failed: %s", sqlite3_errmsg(store->db));
        sqlite3_close(store->db);
        pthread_mutex_destroy(&store->mutex);
        free(store);
        return NULL;
    }
    return store;
}

void store_close(Store *store) {
    if (store == NULL) {
        return;
    }
    pthread_mutex_lock(&store->mutex);
    sqlite3_exec(store->db, "PRAGMA wal_checkpoint(TRUNCATE);", NULL, NULL, NULL);
    sqlite3_close(store->db);
    store->db = NULL;
    pthread_mutex_unlock(&store->mutex);
    pthread_mutex_destroy(&store->mutex);
    free(store);
}

ApiError store_validate_key(const char *key) {
    if (key == NULL || key[0] == '\0') {
        return API_ERR_INVALID_KEY;
    }
    size_t len = strlen(key);
    if (len > STORE_MAX_KEY_LEN) {
        return API_ERR_INVALID_KEY;
    }
    for (size_t i = 0; i < len; i++) {
        char c = key[i];
        int ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' ||
                 c == '_' || c == '-';
        if (!ok) {
            return API_ERR_INVALID_KEY;
        }
    }
    return API_OK;
}

static ApiError validate_value(const char *value) {
    if (value == NULL) {
        return API_ERR_MISSING_FIELD;
    }
    if (strlen(value) > STORE_MAX_VALUE_LEN) {
        return API_ERR_PAYLOAD_TOO_LARGE;
    }
    return API_OK;
}

static ApiError validate_note_fields(const char *title, const char *body) {
    if (title == NULL || body == NULL) {
        return API_ERR_MISSING_FIELD;
    }
    size_t title_len = strlen(title);
    size_t body_len = strlen(body);
    if (title_len == 0 || title_len > STORE_MAX_TITLE_LEN) {
        return API_ERR_BAD_REQUEST;
    }
    if (body_len > STORE_MAX_BODY_LEN) {
        return API_ERR_PAYLOAD_TOO_LARGE;
    }
    return API_OK;
}

ApiError store_kv_put(Store *store, const char *key, const char *value) {
    if (store == NULL) {
        return API_ERR_INTERNAL;
    }
    ApiError valid = store_validate_key(key);
    if (valid != API_OK) {
        return valid;
    }
    valid = validate_value(value);
    if (valid != API_OK) {
        return valid;
    }
    pthread_mutex_lock(&store->mutex);
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(store->db,
                                "INSERT INTO kv(key,value,updated_at) VALUES(?,?,?) "
                                "ON CONFLICT(key) DO UPDATE SET value=excluded.value, updated_at=excluded.updated_at;",
                                -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&store->mutex);
        return API_ERR_INTERNAL;
    }
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64)time(NULL));
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&store->mutex);
    return rc == SQLITE_DONE ? API_OK : API_ERR_INTERNAL;
}

ApiError store_kv_get(Store *store, const char *key, char *value_out, size_t value_cap) {
    if (store == NULL || value_out == NULL || value_cap == 0) {
        return API_ERR_INTERNAL;
    }
    if (store_validate_key(key) != API_OK) {
        return API_ERR_INVALID_KEY;
    }
    pthread_mutex_lock(&store->mutex);
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(store->db, "SELECT value FROM kv WHERE key=?;", -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&store->mutex);
        return API_ERR_INTERNAL;
    }
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    ApiError result = API_ERR_NOT_FOUND;
    if (rc == SQLITE_ROW) {
        const unsigned char *text = sqlite3_column_text(stmt, 0);
        int bytes = sqlite3_column_bytes(stmt, 0);
        if (text == NULL) {
            result = API_ERR_INTERNAL;
        } else if ((size_t)bytes + 1 > value_cap) {
            result = API_ERR_PAYLOAD_TOO_LARGE;
        } else {
            memcpy(value_out, text, (size_t)bytes);
            value_out[bytes] = '\0';
            result = API_OK;
        }
    } else if (rc != SQLITE_DONE) {
        result = API_ERR_INTERNAL;
    }
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&store->mutex);
    return result;
}

ApiError store_kv_del(Store *store, const char *key) {
    if (store == NULL) {
        return API_ERR_INTERNAL;
    }
    if (store_validate_key(key) != API_OK) {
        return API_ERR_INVALID_KEY;
    }
    pthread_mutex_lock(&store->mutex);
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(store->db, "DELETE FROM kv WHERE key=?;", -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&store->mutex);
        return API_ERR_INTERNAL;
    }
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    int changes = sqlite3_changes(store->db);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&store->mutex);
    if (rc != SQLITE_DONE) {
        return API_ERR_INTERNAL;
    }
    return changes > 0 ? API_OK : API_ERR_NOT_FOUND;
}

ApiError store_kv_list(Store *store, const char *prefix, int limit, char *out_json, size_t out_cap) {
    if (store == NULL || out_json == NULL || prefix == NULL) {
        return API_ERR_INTERNAL;
    }
    if (limit <= 0 || limit > STORE_MAX_LIST) {
        return API_ERR_INVALID_QUERY;
    }
    pthread_mutex_lock(&store->mutex);
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(store->db,
                                "SELECT key,value FROM kv WHERE key LIKE ? ESCAPE '\\' ORDER BY key LIMIT ?;",
                                -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&store->mutex);
        return API_ERR_INTERNAL;
    }
    char pattern[STORE_MAX_KEY_LEN + 2];
    snprintf(pattern, sizeof(pattern), "%s%%", prefix);
    sqlite3_bind_text(stmt, 1, pattern, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, limit);
    size_t pos = 0;
    int first = 1;
    ApiError result = API_OK;
    if (out_cap < 3) {
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&store->mutex);
        return API_ERR_INTERNAL;
    }
    out_json[pos++] = '[';
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const unsigned char *k = sqlite3_column_text(stmt, 0);
        const unsigned char *v = sqlite3_column_text(stmt, 1);
        if (k == NULL || v == NULL) {
            result = API_ERR_INTERNAL;
            break;
        }
        char ek[STORE_MAX_KEY_LEN * 2 + 1];
        char ev[STORE_MAX_VALUE_LEN + 1024];
        if (json_escape(ek, sizeof(ek), (const char *)k) != 0 ||
            json_escape(ev, sizeof(ev), (const char *)v) != 0) {
            result = API_ERR_INTERNAL;
            break;
        }
        int written = snprintf(out_json + pos, out_cap - pos, "%s{\"key\":\"%s\",\"value\":\"%s\"}",
                               first ? "" : ",", ek, ev);
        if (written < 0 || (size_t)written >= out_cap - pos) {
            result = API_ERR_PAYLOAD_TOO_LARGE;
            break;
        }
        pos += (size_t)written;
        first = 0;
    }
    if (result == API_OK && rc != SQLITE_DONE) {
        result = API_ERR_INTERNAL;
    }
    sqlite3_finalize(stmt);
    if (result == API_OK) {
        if (pos + 2 > out_cap) {
            pthread_mutex_unlock(&store->mutex);
            return API_ERR_PAYLOAD_TOO_LARGE;
        }
        out_json[pos++] = ']';
        out_json[pos] = '\0';
    }
    pthread_mutex_unlock(&store->mutex);
    return result;
}

ApiError store_note_create(Store *store, const char *title, const char *body, long *id_out) {
    if (store == NULL || id_out == NULL) {
        return API_ERR_INTERNAL;
    }
    ApiError valid = validate_note_fields(title, body);
    if (valid != API_OK) {
        return valid;
    }
    pthread_mutex_lock(&store->mutex);
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(store->db,
                                "INSERT INTO notes(title,body,created_at,updated_at) VALUES(?,?,?,?);", -1, &stmt,
                                NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&store->mutex);
        return API_ERR_INTERNAL;
    }
    long now = (long)time(NULL);
    sqlite3_bind_text(stmt, 1, title, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, body, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64)now);
    sqlite3_bind_int64(stmt, 4, (sqlite3_int64)now);
    rc = sqlite3_step(stmt);
    long id = (long)sqlite3_last_insert_rowid(store->db);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&store->mutex);
    if (rc != SQLITE_DONE) {
        return API_ERR_INTERNAL;
    }
    *id_out = id;
    return API_OK;
}

static ApiError note_row_to_json(sqlite3_stmt *stmt, char *out_json, size_t out_cap, size_t *pos) {
    long id = (long)sqlite3_column_int64(stmt, 0);
    const unsigned char *title = sqlite3_column_text(stmt, 1);
    const unsigned char *body = sqlite3_column_text(stmt, 2);
    long created = (long)sqlite3_column_int64(stmt, 3);
    long updated = (long)sqlite3_column_int64(stmt, 4);
    if (title == NULL || body == NULL) {
        return API_ERR_INTERNAL;
    }
    char et[STORE_MAX_TITLE_LEN * 2 + 1];
    char eb[STORE_MAX_BODY_LEN * 2 + 1];
    if (json_escape(et, sizeof(et), (const char *)title) != 0 ||
        json_escape(eb, sizeof(eb), (const char *)body) != 0) {
        return API_ERR_INTERNAL;
    }
    int written =
        snprintf(out_json + *pos, out_cap - *pos,
                 "{\"id\":%ld,\"title\":\"%s\",\"body\":\"%s\",\"created_at\":%ld,\"updated_at\":%ld}", id, et, eb,
                 created, updated);
    if (written < 0 || (size_t)written >= out_cap - *pos) {
        return API_ERR_PAYLOAD_TOO_LARGE;
    }
    *pos += (size_t)written;
    return API_OK;
}

ApiError store_note_list(Store *store, int limit, int offset, char *out_json, size_t out_cap, long *total_out) {
    if (store == NULL || out_json == NULL || total_out == NULL) {
        return API_ERR_INTERNAL;
    }
    if (limit <= 0 || limit > STORE_MAX_LIST || offset < 0) {
        return API_ERR_INVALID_QUERY;
    }
    pthread_mutex_lock(&store->mutex);
    sqlite3_stmt *count_stmt = NULL;
    ApiError result = API_OK;
    if (sqlite3_prepare_v2(store->db, "SELECT COUNT(*) FROM notes;", -1, &count_stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&store->mutex);
        return API_ERR_INTERNAL;
    }
    long total = 0;
    if (sqlite3_step(count_stmt) == SQLITE_ROW) {
        total = (long)sqlite3_column_int64(count_stmt, 0);
    }
    sqlite3_finalize(count_stmt);
    *total_out = total;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(store->db,
                            "SELECT id,title,body,created_at,updated_at FROM notes ORDER BY id LIMIT ? OFFSET ?;",
                            -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&store->mutex);
        return API_ERR_INTERNAL;
    }
    sqlite3_bind_int(stmt, 1, limit);
    sqlite3_bind_int(stmt, 2, offset);
    if (out_cap < 16) {
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&store->mutex);
        return API_ERR_INTERNAL;
    }
    size_t pos = 0;
    int written = snprintf(out_json + pos, out_cap - pos, "{\"items\":[");
    if (written < 0 || (size_t)written >= out_cap - pos) {
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&store->mutex);
        return API_ERR_PAYLOAD_TOO_LARGE;
    }
    pos += (size_t)written;
    int first = 1;
    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (!first) {
            if (pos + 1 >= out_cap) {
                result = API_ERR_PAYLOAD_TOO_LARGE;
                break;
            }
            out_json[pos++] = ',';
        }
        result = note_row_to_json(stmt, out_json, out_cap, &pos);
        if (result != API_OK) {
            break;
        }
        first = 0;
    }
    if (result == API_OK && rc != SQLITE_DONE) {
        result = API_ERR_INTERNAL;
    }
    sqlite3_finalize(stmt);
    if (result == API_OK) {
        written = snprintf(out_json + pos, out_cap - pos, "],\"total\":%ld}", total);
        if (written < 0 || (size_t)written >= out_cap - pos) {
            pthread_mutex_unlock(&store->mutex);
            return API_ERR_PAYLOAD_TOO_LARGE;
        }
    }
    pthread_mutex_unlock(&store->mutex);
    return result;
}

ApiError store_note_get(Store *store, long id, char *out_json, size_t out_cap) {
    if (store == NULL || out_json == NULL) {
        return API_ERR_INTERNAL;
    }
    if (id <= 0) {
        return API_ERR_INVALID_ID;
    }
    pthread_mutex_lock(&store->mutex);
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(store->db, "SELECT id,title,body,created_at,updated_at FROM notes WHERE id=?;", -1,
                            &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&store->mutex);
        return API_ERR_INTERNAL;
    }
    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)id);
    int rc = sqlite3_step(stmt);
    ApiError result = API_ERR_NOT_FOUND;
    if (rc == SQLITE_ROW) {
        size_t pos = 0;
        result = note_row_to_json(stmt, out_json, out_cap, &pos);
    } else if (rc != SQLITE_DONE) {
        result = API_ERR_INTERNAL;
    }
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&store->mutex);
    return result;
}

ApiError store_note_update(Store *store, long id, const char *title, const char *body) {
    if (store == NULL) {
        return API_ERR_INTERNAL;
    }
    if (id <= 0) {
        return API_ERR_INVALID_ID;
    }
    ApiError valid = validate_note_fields(title, body);
    if (valid != API_OK) {
        return valid;
    }
    pthread_mutex_lock(&store->mutex);
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(store->db, "UPDATE notes SET title=?,body=?,updated_at=? WHERE id=?;", -1, &stmt,
                            NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&store->mutex);
        return API_ERR_INTERNAL;
    }
    sqlite3_bind_text(stmt, 1, title, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, body, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64)time(NULL));
    sqlite3_bind_int64(stmt, 4, (sqlite3_int64)id);
    int rc = sqlite3_step(stmt);
    int changes = sqlite3_changes(store->db);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&store->mutex);
    if (rc != SQLITE_DONE) {
        return API_ERR_INTERNAL;
    }
    return changes > 0 ? API_OK : API_ERR_NOT_FOUND;
}

ApiError store_note_del(Store *store, long id) {
    if (store == NULL) {
        return API_ERR_INTERNAL;
    }
    if (id <= 0) {
        return API_ERR_INVALID_ID;
    }
    pthread_mutex_lock(&store->mutex);
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(store->db, "DELETE FROM notes WHERE id=?;", -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&store->mutex);
        return API_ERR_INTERNAL;
    }
    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)id);
    int rc = sqlite3_step(stmt);
    int changes = sqlite3_changes(store->db);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&store->mutex);
    if (rc != SQLITE_DONE) {
        return API_ERR_INTERNAL;
    }
    return changes > 0 ? API_OK : API_ERR_NOT_FOUND;
}

ApiError store_counts(Store *store, long *kv_count, long *note_count) {
    if (store == NULL || kv_count == NULL || note_count == NULL) {
        return API_ERR_INTERNAL;
    }
    pthread_mutex_lock(&store->mutex);
    sqlite3_stmt *stmt = NULL;
    ApiError result = API_OK;
    *kv_count = 0;
    *note_count = 0;
    if (sqlite3_prepare_v2(store->db, "SELECT COUNT(*) FROM kv;", -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&store->mutex);
        return API_ERR_INTERNAL;
    }
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        *kv_count = (long)sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
    if (sqlite3_prepare_v2(store->db, "SELECT COUNT(*) FROM notes;", -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&store->mutex);
        return API_ERR_INTERNAL;
    }
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        *note_count = (long)sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&store->mutex);
    return result;
}
