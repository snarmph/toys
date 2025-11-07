#include "colla/colla.h" 
#include "common.h"
#include "toys.h"
#include "tui.h"

#include <stdlib.h>

TOY_SHORT_DESC(pwgen, "Generate human-readable random passwords.");

typedef struct {
    bool no_numbers;
    bool one_per_line;
    bool no_capital;
    bool avoid_ambiguous;
    bool output_columns;
    bool no_vowels;
    bool symbols;
    strview_t avoid;
} pwgen_opt_t;

void pwgen_parse_opts(int argc, char **argv, pwgen_opt_t *opt) {
    usage_helper(
        "pwgen [option]",
        "Generate human-readable random passwords. Default "
        "output to tty fills screen with passwords to defeat "
        "shoulder surfing (pick one and clear the screen).",
        USAGE_ALLOW_NO_ARGS,
        USAGE_NO_EXTRA(),
        argc, argv,
        {
            '0', "no-numerals",
            "No numbers.",
            USAGE_BOOL(opt->no_numbers),
        },
        {
            '1', "one-per-line",
            "Output one per line.",
            USAGE_BOOL(opt->one_per_line),
        },
        {
            'A', "no-capitalize",
            "No capital letters.",
            USAGE_BOOL(opt->no_capital),
        },
        {
            'B', "ambiguous",
            "Avoid ambiguous characters like 0O and 1lI.",
            USAGE_BOOL(opt->avoid_ambiguous),
        },
        {
            'r', "remove",
            "Don't include the given {}.",
            "chars",
            USAGE_VALUE(opt->avoid)
        },
        {
            'v', "no-vowels",
            "No vowels.",
            USAGE_BOOL(opt->no_vowels),
        },
        {
            'y', "symbols",
            "Add punctuation.",
            USAGE_BOOL(opt->symbols),
        },
    );
}

void TOY(pwgen)(int argc, char **argv) {
    pwgen_opt_t opt = {0};
    pwgen_parse_opts(argc, argv, &opt);

    char chars[1024] = {0};
    int char_count = 0;

    strview_t numbers = cstrv("0123456789");
    strview_t lowercase = cstrv("abcdefghijklmnopqrstuvwxyz");
    strview_t uppercase = cstrv("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    strview_t symbols = cstrv("!@#$%^&()_-+=<>?");

    strview_t ambiguous = opt.avoid_ambiguous ? strv("0Oil1") : STRV_EMPTY;
    strview_t vowels = opt.no_vowels ? strv("aeiou") : STRV_EMPTY;
    strview_t extra = opt.avoid;

#define copy(v) \
    for (usize i = 0; i < v.len; ++i) { \
        char c = v.buf[i]; \
        if (strv_contains(ambiguous, c) || \
            strv_contains(vowels, c) || \
            strv_contains(extra, c) \
        ) { \
            continue; \
        } \
        chars[char_count++] = c; \
    }

    copy(lowercase);
    if (!opt.no_numbers) copy(numbers);
    if (!opt.no_capital) copy(uppercase);
    if (opt.symbols)     copy(symbols);

#undef copy

    int tw = opt.one_per_line ? 8 : tui_width();
    int th = tui_height() - 2;
    int pw_len = 9;
    int cols = tw / pw_len;
    for (int y = 0; y < th; ++y) {
        for (int x = 0; x < cols; ++x) {
            char pwd[8];
            for (int i = 0; i < 8; ++i) {
                int k = rand() % char_count;
                pwd[i] = chars[k];
            }
            print("%v ", strv(pwd, 8));
        }
        if (y + 1 < th) println("");
    }
}
