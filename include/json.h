#ifndef JSON_H
#define JSON_H

#include <stddef.h>

#include "http.h"

/*
 * Minimal strict JSON string-field helpers (no external deps).
 * Only flat {"field":"value"} objects are supported; anything else is
 * API_ERR_BAD_REQUEST explicitly. No lenient fallback.
 */

/*
 * json_escape - Escape a string for JSON output.
 * @dest/@dest_cap: output (always NUL-terminated on success).
 * @src: input (may be "").
 * Returns: 0 ok, -1 overflow (caller maps to 500, never truncates silently).
 * Thread-safe.
 */
int json_escape(char *dest, size_t dest_cap, const char *src);

/*
 * json_get_string - Extract a top-level string field from a flat object.
 * @body/@body_len: JSON bytes (need not be NUL-terminated).
 * @field: key name. @out/@out_cap: value output (unescaped, NUL-terminated).
 * Returns: API_OK, API_ERR_MISSING_FIELD, or API_ERR_BAD_REQUEST.
 * Rejects: non-objects, nested values, missing quotes, truncation.
 */
ApiError json_get_string(const char *body, size_t body_len, const char *field, char *out, size_t out_cap);

#endif
