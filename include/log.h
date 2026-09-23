#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <time.h>

/*
 * Structured stderr logging: level, time (UTC), module, message.
 * Not thread-safe for a single line interleaving beyond atomic write;
 * uses one fprintf per line (practically atomic for short lines on stderr).
 * Log without PII: never print raw client payloads, only sizes/peers.
 */

static inline void log_line(const char *level, const char *module, const char *msg, const char *peer) {
    char time_buf[32];
    time_t now = time(NULL);
    struct tm tm_utc;
    gmtime_r(&now, &tm_utc);
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
    if (peer != NULL) {
        fprintf(stderr, "[%s] %s module=%s peer=%s msg=%s\n", level, time_buf, module, peer, msg);
    } else {
        fprintf(stderr, "[%s] %s module=%s msg=%s\n", level, time_buf, module, msg);
    }
}

#define LOG_INFO(module, msg) log_line("INFO", (module), (msg), NULL)
#define LOG_INFO_PEER(module, msg, peer) log_line("INFO", (module), (msg), (peer))
#define LOG_ERROR(module, msg) log_line("ERROR", (module), (msg), NULL)
#define LOG_ERROR_PEER(module, msg, peer) log_line("ERROR", (module), (msg), (peer))

#endif
