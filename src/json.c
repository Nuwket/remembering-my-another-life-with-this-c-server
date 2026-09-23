/*
 * json.c - Strict minimal JSON helpers. Rejects instead of guessing.
 */

#include "json.h"

#include <string.h>

int json_escape(char *dest, size_t dest_cap, const char *src) {
    if (dest == NULL || src == NULL || dest_cap == 0) {
        return -1;
    }
    size_t pos = 0;
    for (const char *p = src; *p != '\0'; p++) {
        const char *esc = NULL;
        char seq[2] = {0, 0};
        switch (*p) {
        case '"':
            esc = "\\\"";
            break;
        case '\\':
            esc = "\\\\";
            break;
        case '\n':
            esc = "\\n";
            break;
        case '\r':
            esc = "\\r";
            break;
        case '\t':
            esc = "\\t";
            break;
        default:
            seq[0] = *p;
            esc = seq;
            break;
        }
        size_t esc_len = strlen(esc);
        /* Control chars <0x20 without named escape are rejected as unsafe. */
        if ((unsigned char)*p < 0x20 && esc == seq) {
            return -1;
        }
        if (pos + esc_len + 1 > dest_cap) {
            return -1;
        }
        memcpy(dest + pos, esc, esc_len);
        pos += esc_len;
    }
    dest[pos] = '\0';
    return 0;
}

static const char *skip_spaces(const char *ptr, const char *end) {
    while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) {
        ptr++;
    }
    return ptr;
}

/* Parse "quoted" with \" and \\ escapes into @out. Returns next ptr or NULL. */
static const char *parse_quoted(const char *ptr, const char *end, char *out, size_t out_cap) {
    if (ptr >= end || *ptr != '"') {
        return NULL;
    }
    ptr++;
    size_t pos = 0;
    while (ptr < end) {
        if (*ptr == '"') {
            if (out_cap == 0) {
                return NULL;
            }
            if (pos >= out_cap) {
                return NULL;
            }
            out[pos] = '\0';
            return ptr + 1;
        }
        char decoded = *ptr;
        if (*ptr == '\\') {
            ptr++;
            if (ptr >= end) {
                return NULL;
            }
            switch (*ptr) {
            case '"':
                decoded = '"';
                break;
            case '\\':
                decoded = '\\';
                break;
            case 'n':
                decoded = '\n';
                break;
            case 'r':
                decoded = '\r';
                break;
            case 't':
                decoded = '\t';
                break;
            default:
                return NULL;
            }
        }
        if (pos + 1 >= out_cap) {
            return NULL;
        }
        out[pos++] = decoded;
        ptr++;
    }
    return NULL;
}

ApiError json_get_string(const char *body, size_t body_len, const char *field, char *out, size_t out_cap) {
    if (body == NULL || field == NULL || out == NULL || out_cap == 0) {
        return API_ERR_BAD_REQUEST;
    }
    const char *end = body + body_len;
    const char *ptr = skip_spaces(body, end);
    if (ptr >= end || *ptr != '{') {
        return API_ERR_BAD_REQUEST;
    }
    ptr++;
    int found = 0;
    char name[128];
    char value[8192];
    for (;;) {
        ptr = skip_spaces(ptr, end);
        if (ptr >= end) {
            return API_ERR_BAD_REQUEST;
        }
        if (*ptr == '}') {
            ptr++;
            ptr = skip_spaces(ptr, end);
            if (ptr != end) {
                return API_ERR_BAD_REQUEST;
            }
            break;
        }
        const char *after_name = parse_quoted(ptr, end, name, sizeof(name));
        if (after_name == NULL) {
            return API_ERR_BAD_REQUEST;
        }
        ptr = skip_spaces(after_name, end);
        if (ptr >= end || *ptr != ':') {
            return API_ERR_BAD_REQUEST;
        }
        ptr = skip_spaces(ptr + 1, end);
        if (ptr >= end) {
            return API_ERR_BAD_REQUEST;
        }
        if (*ptr == '"') {
            const char *after_value = parse_quoted(ptr, end, value, sizeof(value));
            if (after_value == NULL) {
                return API_ERR_BAD_REQUEST;
            }
            if (strcmp(name, field) == 0) {
                if (found) {
                    return API_ERR_BAD_REQUEST;
                }
                size_t value_len = strlen(value);
                if (value_len + 1 > out_cap) {
                    return API_ERR_PAYLOAD_TOO_LARGE;
                }
                memcpy(out, value, value_len + 1);
                found = 1;
            }
            ptr = after_value;
        } else {
            return API_ERR_BAD_REQUEST;
        }
        ptr = skip_spaces(ptr, end);
        if (ptr >= end) {
            return API_ERR_BAD_REQUEST;
        }
        if (*ptr == ',') {
            ptr++;
            continue;
        }
        if (*ptr == '}') {
            continue;
        }
        return API_ERR_BAD_REQUEST;
    }
    return found ? API_OK : API_ERR_MISSING_FIELD;
}
