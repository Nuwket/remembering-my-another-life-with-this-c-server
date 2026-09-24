/*
 * test_console.c - Unit tests for the terminal presentation layer.
 * All assertions run headlessly: no TTY, so color/width stay deterministic.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "console.h"

static int g_passed = 0;
static int g_failed = 0;

#define RUN_TEST(fn)                                                                                                   \
    do {                                                                                                               \
        if ((fn)() == 0) {                                                                                             \
            g_passed++;                                                                                                \
        } else {                                                                                                       \
            g_failed++;                                                                                                \
            printf("  FAIL %s\n", #fn);                                                                                \
        }                                                                                                              \
    } while (0)

#define EXPECT(cond)                                                                                                   \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            printf("  ASSERT %s:%d: %s\n", __FILE__, __LINE__, #cond);                                                 \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (0)

static int test_visible_width_plain(void) {
    EXPECT(console_visible_width("") == 0);
    EXPECT(console_visible_width("abc") == 3);
    EXPECT(console_visible_width(NULL) == 0);
    return 0;
}

static int test_visible_width_ignores_ansi(void) {
    /* A colored string must measure as its bare text. */
    EXPECT(console_visible_width("\033[38;5;42mgreen\033[0m") == 5);
    EXPECT(console_visible_width("\033[1;38;5;51mcyan\033[0m") == 4);
    return 0;
}

static int test_visible_width_counts_codepoints(void) {
    /* U+2622 RADIATION SIGN is three bytes but one column. */
    EXPECT(console_visible_width("\xE2\x98\xA2") == 1);
    EXPECT(console_visible_width("a\xE2\x98\xA2" "b") == 3);
    /* Two dot leaders from the banner. */
    EXPECT(console_visible_width("\xE2\x80\xA2\xE2\x80\xA2") == 2);
    return 0;
}

static int test_center_is_mathematically_centered(void) {
    const char *odd = console_center("abc", 11);
    EXPECT(console_visible_width(odd) == 11);
    /* abc is 3 wide, 8 spare -> 4 left, 4 right. */
    EXPECT(strncmp(odd, "    abc", 7) == 0);

    const char *even = console_center("abcd", 12);
    EXPECT(console_visible_width(even) == 12);
    EXPECT(strncmp(even, "    abcd", 8) == 0);
    return 0;
}

static int test_center_handles_oversized_text(void) {
    const char *wide = console_center("0123456789", 4);
    EXPECT(console_visible_width(wide) == 10);
    return 0;
}

static int test_pad_right_fills_to_width(void) {
    const char *padded = console_pad_right("ab", 6);
    EXPECT(console_visible_width(padded) == 6);
    EXPECT(strncmp(padded, "ab    ", 6) == 0);
    return 0;
}

static int test_pad_right_does_not_truncate(void) {
    const char *padded = console_pad_right("abcdef", 3);
    EXPECT(console_visible_width(padded) == 6);
    return 0;
}

static int test_width_is_clamped(void) {
    size_t width = console_width();
    EXPECT(width >= CONSOLE_MIN_WIDTH);
    EXPECT(width <= CONSOLE_MAX_WIDTH);
    return 0;
}

static int test_colorize_without_tty_is_identity(void) {
    /* Tests run without a TTY, so no escape codes may be emitted. */
    const char *colored = console_colorize("green", "ok");
    EXPECT(strcmp(colored, "ok") == 0);
    EXPECT(console_colorize("green", NULL) != NULL);
    return 0;
}

static int test_utf8_detection_is_stable(void) {
    bool first = console_is_utf8();
    bool second = console_is_utf8();
    EXPECT(first == second);
    return 0;
}

static int test_banner_runs_without_tty(void) {
    ConsoleInfo info = {
        .bind_ip = "127.0.0.1",
        .link_host = "127.0.0.1",
        .db_path = "./data/test.db",
        .version = "0.0.0-test",
        .port = 8080,
        .workers = 8,
        .auth_protected = false,
    };
    /* Must not crash or hang when stdout is redirected. */
    console_print_banner(&info);
    console_print_banner(NULL);
    return 0;
}

int main(void) {
    RUN_TEST(test_visible_width_plain);
    RUN_TEST(test_visible_width_ignores_ansi);
    RUN_TEST(test_visible_width_counts_codepoints);
    RUN_TEST(test_center_is_mathematically_centered);
    RUN_TEST(test_center_handles_oversized_text);
    RUN_TEST(test_pad_right_fills_to_width);
    RUN_TEST(test_pad_right_does_not_truncate);
    RUN_TEST(test_width_is_clamped);
    RUN_TEST(test_colorize_without_tty_is_identity);
    RUN_TEST(test_utf8_detection_is_stable);
    RUN_TEST(test_banner_runs_without_tty);

    printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
