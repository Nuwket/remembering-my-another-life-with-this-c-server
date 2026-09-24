/*
 * console.h - Terminal presentation layer for the server CLI.
 *
 * Owns every byte the server prints outside the structured log: the boot
 * sequence, the banner, boxes and endpoint lists. Keeping it here means
 * main.c stays about wiring, and the visual layer can be tested headlessly.
 *
 * All functions are safe on a non-TTY stdout: color and animation degrade to
 * plain text, and the output is still valid UTF-8-free ASCII when the locale
 * is not UTF-8. Not thread-safe (startup only, single caller).
 */
#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CONSOLE_MIN_WIDTH 48
#define CONSOLE_MAX_WIDTH 74
#define CONSOLE_FALLBACK_WIDTH 80

typedef struct {
    const char *bind_ip;
    const char *link_host;
    const char *db_path;
    const char *version;
    uint16_t port;
    int workers;
    bool auth_protected;
} ConsoleInfo;

/*
 * console_visible_width - Printable column count, ignoring ANSI escapes.
 * @text: UTF-8 or ASCII string (escape sequences allowed).
 * Returns: number of terminal cells the string occupies.
 */
size_t console_visible_width(const char *text);

/*
 * console_is_utf8 - Whether box-drawing glyphs will render correctly.
 * Decided from LANG/LC_ALL/LC_CTYPE. Returns false when unknown or non-UTF-8.
 */
bool console_is_utf8(void);

/*
 * console_is_tty - Whether stdout is an interactive terminal.
 * Colors and animation are only used when true.
 */
bool console_is_tty(void);

/*
 * console_width - Usable terminal width for the box.
 * Probes TIOCGWINSZ, then $COLUMNS, then CONSOLE_FALLBACK_WIDTH.
 * Always clamped to [CONSOLE_MIN_WIDTH, CONSOLE_MAX_WIDTH].
 */
size_t console_width(void);

/*
 * console_center - Render `text` centered inside `width` cells.
 * Odd remainders go to the right so the text sits one cell left of true
 * center, which reads better with monospace fonts.
 * Returns: pointer to a rotating static buffer; valid until 4 more calls.
 */
const char *console_center(const char *text, size_t width);

/*
 * console_pad_right - Render `text` left-aligned and padded to `width` cells.
 * Returns: pointer to a rotating static buffer.
 */
const char *console_pad_right(const char *text, size_t width);

/*
 * console_colorize - Wrap `text` in an ANSI color, or return it unchanged.
 * @name: one of "reset","bold","dim","green","cyan","yellow","red".
 * Returns: pointer to a rotating static buffer. Never returns NULL.
 */
const char *console_colorize(const char *name, const char *text);

/*
 * console_print_boot - Short startup animation with checklist and progress bar.
 * Skipped entirely when stdout is not a TTY, when NO_COLOR is set, or when
 * CONSOLE_NO_ANIMATION=1, so CI output stays clean and startup is not slowed.
 * Total animated time is bounded to roughly 0.6s.
 */
void console_print_boot(void);

/*
 * console_print_banner - Final "core online" screen: responsive box, ASCII
 * art, clickable endpoints, configuration and status line.
 * @info: server facts; strings are borrowed for the duration of the call.
 */
void console_print_banner(const ConsoleInfo *info);

/*
 * console_print_shutdown - Reactor shutdown checklist.
 * Call after server_run() returns, never from a signal handler.
 */
void console_print_shutdown(void);

/*
 * console_clear - Clear screen and home the cursor if the terminal supports it.
 * No-op when stdout is not a TTY, so redirected output is never mangled.
 */
void console_clear(void);

#endif
