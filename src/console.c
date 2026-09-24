/*
 * console.c - Terminal presentation layer. See console.h for the contract.
 *
 * Layout math is explicit everywhere: every visible cell is accounted for by
 * console_visible_width(), which strips ANSI escapes and counts UTF-8
 * codepoints instead of bytes. No hardcoded padding anywhere.
 */

#include "console.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define SLOT_COUNT 8
#define SLOT_SIZE 512
#define RULE_BUFFER (CONSOLE_MAX_WIDTH * 4 + 16)

/* ---------- capabilities ---------- */

bool console_is_tty(void) {
    return isatty(STDOUT_FILENO) == 1;
}

static bool env_flag_set(const char *name) {
    const char *value = getenv(name);
    return value != NULL && value[0] != '\0' && strcmp(value, "0") != 0;
}

static bool color_enabled(void) {
    static int cached = -1;
    if (cached < 0) {
        cached = (env_flag_set("NO_COLOR") || !console_is_tty()) ? 0 : 1;
    }
    return cached == 1;
}

bool console_is_utf8(void) {
    static int cached = -1;
    if (cached >= 0) {
        return cached == 1;
    }
    const char *vars[] = {"LC_ALL", "LC_CTYPE", "LANG"};
    bool utf8 = false;
    for (size_t i = 0; i < sizeof(vars) / sizeof(vars[0]); i++) {
        const char *value = getenv(vars[i]);
        if (value == NULL || value[0] == '\0') {
            continue;
        }
        const char *cursor = value;
        while (*cursor != '\0') {
            if ((cursor[0] == 'U' || cursor[0] == 'u') && (cursor[1] == 'T' || cursor[1] == 't') &&
                (cursor[2] == 'F' || cursor[2] == 'f') && cursor[3] == '-' && cursor[4] == '8') {
                utf8 = true;
                break;
            }
            cursor++;
        }
        if (utf8) {
            break;
        }
    }
    cached = utf8 ? 1 : 0;
    return utf8;
}

static bool unicode_enabled(void) {
    return console_is_utf8() && console_is_tty();
}

size_t console_width(void) {
    size_t columns = 0;
    struct winsize size;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col > 0) {
        columns = (size_t)size.ws_col;
    }
    if (columns == 0) {
        const char *env_columns = getenv("COLUMNS");
        if (env_columns != NULL && env_columns[0] != '\0') {
            char *end = NULL;
            long parsed = strtol(env_columns, &end, 10);
            if (end != env_columns && *end == '\0' && parsed > 0) {
                columns = (size_t)parsed;
            }
        }
    }
    if (columns == 0) {
        columns = CONSOLE_FALLBACK_WIDTH;
    }
    /* Leave breathing room so the box never touches the window edge. */
    if (columns > 4) {
        columns -= 4;
    }
    if (columns < CONSOLE_MIN_WIDTH) {
        columns = CONSOLE_MIN_WIDTH;
    }
    if (columns > CONSOLE_MAX_WIDTH) {
        columns = CONSOLE_MAX_WIDTH;
    }
    return columns;
}

/* ---------- text measurement ---------- */

static bool is_escape_start(char c) {
    return c == '\033';
}

size_t console_visible_width(const char *text) {
    if (text == NULL) {
        return 0;
    }
    size_t width = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; p++) {
        if (is_escape_start((char)*p) && p[1] == '[') {
            p++;
            while (*p != '\0' && (isalpha((char)*p) == 0)) {
                p++;
            }
            continue;
        }
        if ((*p & 0xC0) != 0x80) {
            width++;
        }
    }
    return width;
}

static char *next_slot(void) {
    static char slots[SLOT_COUNT][SLOT_SIZE];
    static int index = 0;
    char *slot = slots[index];
    index = (index + 1) % SLOT_COUNT;
    slot[0] = '\0';
    return slot;
}

const char *console_pad_right(const char *text, size_t width) {
    char *out = next_slot();
    if (text == NULL) {
        text = "";
    }
    int written = snprintf(out, SLOT_SIZE, "%s", text);
    if (written < 0) {
        return out;
    }
    size_t used = console_visible_width(out);
    while (used < width) {
        size_t at = strlen(out);
        if (at + 1 >= SLOT_SIZE) {
            break;
        }
        out[at++] = ' ';
        out[at] = '\0';
        used++;
    }
    return out;
}

const char *console_center(const char *text, size_t width) {
    char *out = next_slot();
    if (text == NULL) {
        text = "";
    }
    size_t used = console_visible_width(text);
    size_t left = used >= width ? 0 : (width - used) / 2;
    size_t right = used >= width ? 0 : width - used - left;
    int written = snprintf(out, SLOT_SIZE, "%*s%s%*s", (int)left, "", text, (int)right, "");
    if (written < 0) {
        out[0] = '\0';
    }
    return out;
}

const char *console_colorize(const char *name, const char *text) {
    if (!color_enabled() || text == NULL) {
        return text == NULL ? "" : text;
    }
    const char *code = "0";
    if (strcmp(name, "bold") == 0) {
        code = "1";
    } else if (strcmp(name, "white") == 0) {
        code = "1;38;5;255";
    } else if (strcmp(name, "dim") == 0) {
        code = "38;5;244";
    } else if (strcmp(name, "amber") == 0) {
        code = "38;5;220";          /* primary: nuclear yellow  */
    } else if (strcmp(name, "hot") == 0) {
        code = "1;38;5;220";        /* primary + bold          */
    } else if (strcmp(name, "gold") == 0) {
        code = "38;5;178";          /* secondary: dark amber   */
    } else if (strcmp(name, "green") == 0) {
        code = "38;5;71";           /* success only, sparing   */
    } else if (strcmp(name, "cyan") == 0) {
        code = "38;5;215";          /* links, warm not cold    */
    } else if (strcmp(name, "yellow") == 0) {
        code = "38;5;220";
    } else if (strcmp(name, "red") == 0) {
        code = "38;5;203";          /* faults only             */
    } else if (strcmp(name, "magenta") == 0) {
        code = "38;5;170";
    }
    char *out = next_slot();
    snprintf(out, SLOT_SIZE, "\033[%sm%s\033[0m", code, text);
    return out;
}

/* ---------- glyphs: unicode when safe, ASCII otherwise ---------- */

typedef struct {
    const char *unicode;
    const char *ascii;
} Glyph;

static const char *glyph(const Glyph *g) {
    return unicode_enabled() ? g->unicode : g->ascii;
}

static const Glyph G_HZ = {"\u2500", "-"};       /* horizontal  */
static const Glyph G_VZ = {"\u2502", "|"};       /* vertical    */
static const Glyph G_TL = {"\u256d", "+"};       /* top-left    */
static const Glyph G_TR = {"\u256e", "+"};       /* top-right   */
static const Glyph G_BL = {"\u2570", "+"};       /* bottom-left */
static const Glyph G_BR = {"\u256f", "+"};       /* bottom-right*/
static const Glyph G_ARROW = {"\u2192", "->"};   /* arrow       */
static const Glyph G_OK = {"\u2713", "OK"};      /* check       */
static const Glyph G_RAD = {"\u2622", "[RADIATION]"}; /* trefoil  */
static const Glyph G_TREE = {"\u251c\u2500", "|-"};

static void repeat(char *out, size_t capacity, const char *unit, size_t count) {
    size_t unit_len = strlen(unit);
    size_t at = 0;
    for (size_t i = 0; i < count; i++) {
        if (at + unit_len + 1 >= capacity) {
            break;
        }
        memcpy(out + at, unit, unit_len);
        at += unit_len;
    }
    out[at] = '\0';
}

/* ---------- box drawing ---------- */

/*
 * NOTE: console_colorize/console_center hand out rotating static buffers, so
 * they are never nested inside one printf. Argument evaluation order is
 * unspecified and would let one call overwrite another's buffer. Each row
 * copies into a local first.
 */
static void draw_rule(size_t inner, const char *color) {
    char line[RULE_BUFFER];
    char painted[RULE_BUFFER + 32];
    repeat(line, sizeof(line), glyph(&G_HZ), inner);
    snprintf(painted, sizeof(painted), "%s", console_colorize(color, line));
    printf("%s%s%s\n", console_colorize("dim", glyph(&G_TL)), painted, console_colorize("dim", glyph(&G_TR)));
}

static void draw_row(const char *text, size_t inner, const char *color) {
    char safe[CONSOLE_MAX_WIDTH * 2 + 16];
    size_t used = console_visible_width(text);
    size_t room = (inner >= 3) ? inner - 2 : inner;

    if (used > room) {
        /* Truncate on a codepoint boundary so we never cut a glyph in half. */
        const unsigned char *p = (const unsigned char *)text;
        size_t at = 0;
        size_t cells = 0;
        while (*p != '\0' && cells + 1 < room) {
            size_t seq = 1;
            if ((*p & 0xF8) == 0xF0) {
                seq = 4;
            } else if ((*p & 0xF0) == 0xE0) {
                seq = 3;
            } else if ((*p & 0xE0) == 0xC0) {
                seq = 2;
            }
            for (size_t i = 0; i < seq && *p != '\0' && at + seq < sizeof(safe); i++) {
                safe[at++] = (char)*p++;
            }
            cells++;
        }
        safe[at++] = '.';
        safe[at] = '\0';
        text = safe;
    }

    const char *padded = console_center(text, inner);
    char painted[CONSOLE_MAX_WIDTH * 2 + 16];
    snprintf(painted, sizeof(painted), "%s", console_colorize(color, padded));
    printf("%s%s%s\n", console_colorize("dim", glyph(&G_VZ)), painted, console_colorize("dim", glyph(&G_VZ)));
}

static void draw_blank(size_t inner) {
    const char *padded = console_center("", inner);
    char painted[CONSOLE_MAX_WIDTH * 2 + 16];
    snprintf(painted, sizeof(painted), "%s", padded);
    printf("%s%s%s\n", console_colorize("dim", glyph(&G_VZ)), painted, console_colorize("dim", glyph(&G_VZ)));
}

/* Bottom border with a short reactor gap, still exactly `inner` cells wide. */
static void draw_closing(size_t inner) {
    const char *hz = glyph(&G_HZ);
    size_t gap = 6;
    size_t lead = (inner > gap) ? (inner - gap) / 2 : 0;
    size_t trail = (inner > gap) ? inner - gap - lead : 0;
    char left[RULE_BUFFER];
    char right[RULE_BUFFER];
    repeat(left, sizeof(left), hz, lead);
    repeat(right, sizeof(right), hz, trail);
    char left_p[RULE_BUFFER + 32];
    char right_p[RULE_BUFFER + 32];
    snprintf(left_p, sizeof(left_p), "%s", console_colorize("dim", left));
    snprintf(right_p, sizeof(right_p), "%s", console_colorize("dim", right));
    printf("%s%s%s%s%s\n", console_colorize("dim", glyph(&G_BL)), left_p,
           console_colorize("amber", unicode_enabled() ? "\u2580\u2580\u2580\u2580\u2580\u2580" : "======"), right_p,
           console_colorize("dim", glyph(&G_BR)));
}

void console_clear(void) {
    if (!console_is_tty()) {
        return;
    }
    printf("\033[H\033[2J");
    fflush(stdout);
}

/* ---------- compact reactor mark ---------- */

/*
 * A trefoil drawn from Unicode when available, ASCII otherwise. Kept small
 * so it reads as a brand mark rather than decoration.
 */
static void draw_reactor_mark(void) {
    const char *rad = glyph(&G_RAD);
    const char *mark = console_center(rad, 16);
    char mark_p[64];
    snprintf(mark_p, sizeof(mark_p), "%s", console_colorize("hot", mark));
    printf("%s\n", mark_p);
    const char *word = console_center("REACTOR", 16);
    char word_p[64];
    snprintf(word_p, sizeof(word_p), "%s", console_colorize("gold", word));
    printf("%s\n\n", word_p);
}

/* ---------- boot sequence ---------- */

typedef struct {
    const char *tag;
    const char *message;
} BootStep;

static void boot_line(size_t inner, const char *tag, const char *message, const char *status,
                      const char *color) {
    char left[192];
    snprintf(left, sizeof(left), "  [%-9s] %s", tag, message);
    char right[48];
    snprintf(right, sizeof(right), "[%s]", status);
    size_t left_width = console_visible_width(left);
    size_t right_width = console_visible_width(right);
    size_t gap = (inner > left_width + right_width) ? (inner - left_width - right_width) / 2 : 1;
    char left_p[256];
    char right_p[64];
    snprintf(left_p, sizeof(left_p), "%s", console_colorize("gold", left));
    snprintf(right_p, sizeof(right_p), "%s", console_colorize(color, right));
    printf("\r  %s%*s%s\033[K", left_p, (int)gap, "", right_p);
    fflush(stdout);
}

void console_print_boot(void) {
    if (!console_is_tty() || env_flag_set("CONSOLE_NO_ANIMATION") || env_flag_set("CI")) {
        return;
    }
    static const BootStep steps[] = {
        {"CORE", "reactor initialization"},
        {"DB", "sqlite store online"},
        {"HTTP", "listener armed"},
        {"POOL", "thread pool initialized"},
        {"RAD", "thermal systems nominal"},
    };
    const size_t inner = console_width() - 2;
    const char *hz = glyph(&G_HZ);
    const char *ok = glyph(&G_OK);
    const char *rad = glyph(&G_RAD);

    printf("\n");
    draw_reactor_mark();
    const char *hdr = console_center("NUCLEAR REACTOR - CORE BOOT", inner);
    char hdr_p[RULE_BUFFER + 32];
    snprintf(hdr_p, sizeof(hdr_p), "%s", console_colorize("gold", hdr));
    printf("  %s\n", hdr_p);
    const char *rule = console_center(hz, inner);
    char rule_p[RULE_BUFFER + 32];
    snprintf(rule_p, sizeof(rule_p), "%s", console_colorize("dim", rule));
    printf("  %s\n\n", rule_p);

    for (size_t i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
        struct timespec pause = {.tv_sec = 0, .tv_nsec = 80000000L};
        nanosleep(&pause, NULL);
        boot_line(inner, steps[i].tag, steps[i].message, ok, "amber");
    }

    /* Amber progress bar. */
    const size_t bar_width = (inner > 30) ? inner - 30 : 16;
    for (int percent = 0; percent <= 100; percent += 20) {
        char filled[RULE_BUFFER];
        char rest[RULE_BUFFER];
        size_t done_cells = (bar_width * (size_t)percent) / 100;
        repeat(filled, sizeof(filled), unicode_enabled() ? "\u2588" : "#", done_cells);
        repeat(rest, sizeof(rest), unicode_enabled() ? "\u2591" : ".", bar_width - done_cells);
        char f_p[RULE_BUFFER + 32];
        char r_p[RULE_BUFFER + 32];
        snprintf(f_p, sizeof(f_p), "%s", console_colorize("amber", filled));
        snprintf(r_p, sizeof(r_p), "%s", console_colorize("dim", rest));
        printf("\r  %s%s  %3d%%\033[K", f_p, r_p, percent);
        fflush(stdout);
        struct timespec pause = {.tv_sec = 0, .tv_nsec = 35000000L};
        nanosleep(&pause, NULL);
    }
    const char *online = console_center("CORE ONLINE", inner);
    char online_p[RULE_BUFFER + 32];
    snprintf(online_p, sizeof(online_p), "%s", console_colorize("hot", online));
    printf("\n\n  %s\n\n", online_p);
    (void)rad;
}

/* ---------- final screen ---------- */

void console_print_banner(const ConsoleInfo *info) {
    if (info == NULL) {
        return;
    }
    const size_t inner = console_width() - 2;
    const char *dot = unicode_enabled() ? " \u00b7 " : " - ";
    const char *rad = glyph(&G_RAD);

    char stack[160];
    snprintf(stack, sizeof(stack), "HTTP/1.1%sJSON%sSQLITE WAL", dot, dot);

    char version_line[192];
    snprintf(version_line, sizeof(version_line), "v%s%s%s", info->version == NULL ? "?" : info->version, dot,
             stack);

    char title[64];
    snprintf(title, sizeof(title), "%s REACTOR CORE %s", rad, rad);
    char brand[80];
    snprintf(brand, sizeof(brand), "%s NUCLEAR C API SERVER %s", rad, rad);

    draw_rule(inner, "dim");
    draw_blank(inner);
    draw_row(title, inner, "hot");
    draw_blank(inner);
    draw_row(brand, inner, "amber");
    draw_blank(inner);
    draw_row(version_line, inner, "gold");
    draw_blank(inner);
    draw_row("RADIATION STATUS: NOMINAL", inner, "amber");
    draw_blank(inner);
    draw_closing(inner);

    /* Lab. */
    char url[192];
    snprintf(url, sizeof(url), "http://%s:%u/", info->link_host, (unsigned)info->port);
    printf("\n  %s%s%s\n", console_colorize("hot", "LAB"), console_colorize("dim", " / INTERACTIVE PLAYGROUND"),
           "");
    printf("    %s %s\n", console_colorize("amber", glyph(&G_ARROW)), console_colorize("cyan", url));

    /* Endpoints. */
    printf("\n  %s%s%s\n", console_colorize("hot", "ENDPOINTS"), console_colorize("dim", " click to open"), "");
    static const struct {
        const char *name;
        const char *path;
    } endpoints[] = {
        {"HEALTH", "/health"},
        {"METRICS", "/metrics"},
        {"KV", "/api/kv?limit=50"},
        {"NOTES", "/api/notes?limit=50"},
    };
    for (size_t i = 0; i < sizeof(endpoints) / sizeof(endpoints[0]); i++) {
        snprintf(url, sizeof(url), "http://%s:%u%s", info->link_host, (unsigned)info->port, endpoints[i].path);
        printf("    %s %s %s %s\n", console_colorize("gold", glyph(&G_TREE)), console_colorize("bold", endpoints[i].name),
               console_colorize("dim", "->"), console_colorize("cyan", url));
    }

    /* Configuration. */
    char workers[32];
    snprintf(workers, sizeof(workers), "%d", info->workers);
    printf("\n  %s%s%s\n", console_colorize("hot", "CORE CONFIGURATION"), "", "");
    printf("    %s %s\n", console_colorize("gold", console_pad_right("bind", 10)),
           info->bind_ip == NULL ? "?" : info->bind_ip);
    printf("    %s %s\n", console_colorize("gold", console_pad_right("workers", 10)), workers);
    printf("    %s %s\n", console_colorize("gold", console_pad_right("database", 10)),
           info->db_path == NULL ? "?" : info->db_path);
    printf("    %s %s\n", console_colorize("gold", console_pad_right("auth", 10)),
           info->auth_protected ? console_colorize("red", "PROTECTED") : console_colorize("amber", "OPEN"));

    /* Hints. */
    printf("\n  %s%s%s\n", console_colorize("hot", "HINTS"), console_colorize("dim", " things to try"), "");
    printf("    %s %s\n", console_colorize("gold", glyph(&G_ARROW)),
           console_colorize("dim", "every lab section ships with prefilled examples"));
    printf("    %s %s\n", console_colorize("gold", glyph(&G_ARROW)),
           console_colorize("dim", "make run RUN_API_KEY=demo  ->  enables the vault (401 / 403)"));
    printf("    %s %s\n", console_colorize("gold", glyph(&G_ARROW)),
           console_colorize("dim", "Ctrl-C  ->  reactor shutdown sequence"));

    /* System status. */
    printf("\n  %s %s%s\n", console_colorize("hot", rad), console_colorize("hot", "SYSTEM STATUS"), "");
    printf("    %s %s %s\n", console_colorize("gold", console_pad_right("core", 12)),
           console_colorize("amber", "ONLINE"), console_colorize("dim", " thread pool accepting"));
    printf("    %s %s %s\n", console_colorize("gold", console_pad_right("radiation", 12)),
           console_colorize("amber", "NOMINAL"), console_colorize("dim", " thematic indicator, not a measured value"));
    printf("    %s %s\n", console_colorize("gold", console_pad_right("auth", 12)),
           info->auth_protected ? console_colorize("red", "PROTECTED") : console_colorize("amber", "OPEN"));
    printf("\n");
    fflush(stdout);
}

/*
 * console_print_shutdown - Post-signal checklist. Printed after server_run
 * returns (never inside the handler) so all I/O stays async-signal-safe.
 */
void console_print_shutdown(void) {
    if (!console_is_tty()) {
        return;
    }
    const char *ok = glyph(&G_OK);
    static const char *steps[] = {"HTTP server stopped", "thread pool drained", "database checkpointed",
                                  "reactor offline"};
    printf("\n  %s %s\n", console_colorize("hot", glyph(&G_RAD)), console_colorize("hot", "SHUTTING DOWN REACTOR"));
    for (size_t i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
        printf("  [%s] %s%s%s\n", console_colorize("amber", ok), console_colorize("dim", steps[i]), "", "");
    }
    printf("\n");
    fflush(stdout);
}

