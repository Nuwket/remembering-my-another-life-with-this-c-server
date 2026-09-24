/*
 * log.h - Structured logging with aligned columns and optional detail lines.
 *
 * Format:
 *   [INFO] 04:53:17  module=main    listening
 *          bind=127.0.0.1 port=8080 workers=8
 *
 * Details are printed verbatim under the event, so machine-readable fields
 * stay parseable and nothing is hidden. Log to stderr only, keeping stdout
 * reserved for the reactor screen.
 */
#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <time.h>

#define LOG_MODULE_WIDTH 10

/*
 * log_event - Emit one structured log record.
 * @level: "INFO", "WARN" or "ERR" (uppercase, printed as-is).
 * @module: subsystem name, left-padded to LOG_MODULE_WIDTH.
 * @event: short event name.
 * @detail: optional second line printed verbatim, or NULL to omit.
 *
 * Thread-safe: one fprintf per line; ordering under concurrency is best-effort.
 */
static inline void log_event(const char *level, const char *module, const char *event, const char *detail) {
    char stamp[16];
    time_t now = time(NULL);
    struct tm parts;
    if (gmtime_r(&now, &parts) != NULL) {
        strftime(stamp, sizeof(stamp), "%H:%M:%S", &parts);
    } else {
        snprintf(stamp, sizeof(stamp), "00:00:00");
    }
    fprintf(stderr, "[%-4s] %s  module=%-*s %s\n", level, stamp, LOG_MODULE_WIDTH, module, event);
    if (detail != NULL && detail[0] != '\0') {
        fprintf(stderr, "         %s\n", detail);
    }
    fflush(stderr);
}

#define LOG_INFO(module, event) log_event("INFO", (module), (event), NULL)
#define LOG_INFO_D(module, event, detail) log_event("INFO", (module), (event), (detail))
#define LOG_WARN(module, event) log_event("WARN", (module), (event), NULL)
#define LOG_WARN_D(module, event, detail) log_event("WARN", (module), (event), (detail))
#define LOG_ERROR(module, event) log_event("ERR", (module), (event), NULL)
#define LOG_ERROR_D(module, event, detail) log_event("ERR", (module), (event), (detail))

#endif
