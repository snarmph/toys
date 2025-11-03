#include "colla/colla.h"
#include "tui.h"
#include "toys.h"

TOY_SHORT_DESC(colors, "Print colors.");

#define COLORS_MAX_COL 1024

typedef enum {
    COLOR_TYPE_BASIC,
    COLOR_TYPE_256,
} color_type_e;

TOY_OPTION_DEFINE(colors) {
    color_type_e type;
    strview_t toprint[COLORS_MAX_COL];
    i64 count;
};

void TOY_OPTION_PARSE(colors)(int argc, char **argv, TOY_OPTION(colors) *opt) {
    bool is_extended = false;

    usage_helper(
        "colors [options] [HEX...]",
        "Print color/s. if an hex value is passed,"
        "it will print some text to show it's color",
        USAGE_ALLOW_NO_ARGS,
        USAGE_EXTRA_PARAMS(opt->toprint, opt->count),
        argc, argv,
        {
            'e', "extended",
            "Print 256 colors.",
            USAGE_BOOL(is_extended),
        },
    );

    if (is_extended) {
        opt->type = COLOR_TYPE_256;
    }
}

void colors_print_basic(void) {
    println("use them like this: \\x1b[" TERM_FG_ORANGE "%%" TERM_RESET "m");
    println("where %% is one of the following numbers:");
    
    strview_t names[8] = {
        cstrv("black"),
        cstrv("red"),
        cstrv("green"),
        cstrv("yellow"),
        cstrv("blue"),
        cstrv("magenta"),
        cstrv("cyan"),
        cstrv("white"),
    };

    i64 maxlen = 18;

    println(
        TERM_FG_ORANGE 
        "foregrounds dark  foregrounds light  "
        "backgrounds dark  backgrounds light"
        TERM_RESET
    );


    for (int i = 0; i < arrlen(names); ++i) {
        i64 spaces = maxlen - names[i].len - 3;
        println(
            // fg dark
            "\x1b[3%dm"
                "3%d %v"
            "\x1b[m"
            "%*s"
            // fg light
            "\x1b[9%dm"
                "9%d %v"
            "\x1b[m" 
            "%*s"
            // bg dark
            TERM_FG_BLACK
            "\x1b[4%dm"
                "4%d %v"
            "\x1b[m"
            "%*s"
            TERM_FG_BLACK
            // bg light
            "\x1b[10%dm"
                "10%d %v"
            "\x1b[m", 
            i, i, names[i], 
            spaces, "", 
            i, i, names[i],
            spaces+1, "", // idk don't ask
            i, i, names[i], 
            spaces, "", 
            i, i, names[i]
        );
    }
}

void colors_print_256(void) {
    println("use them like this: \\x1b[" TERM_FG_ORANGE "%%1" TERM_RESET ";5;" TERM_FG_ORANGE "%%2" TERM_RESET "m");
    println("where " TERM_FG_ORANGE "%%1" TERM_RESET " is 38 for the foreground and 48 for the background");
    println("and " TERM_FG_ORANGE "%%2" TERM_RESET " is one of the following numbers:");

    int tw = tui_width();
    int cols = tw / 4;
    
    int cur = 0;

    while (cur < 256) {
        for (int c = 0; c < cols && cur < 256; ++c, ++cur) {
            print("\x1b[48;5;%dm %3d \x1b[m", cur, cur);
        }
    }
}

void color_print(strview_t hex) {
    int r = 0, g = 0, b = 0;
    
    instream_t in = istr_init(hex);
    if (istr_peek(&in) == '#') {
        istr_skip(&in, 1);
    }
    strview_t red = istr_get_view_len(&in, 2);
    strview_t green = istr_get_view_len(&in, 2);
    strview_t blue = istr_get_view_len(&in, 2);

#define STRV_TO_HEX(v, out) do { \
        char left = v.buf[0]; \
        char right = v.buf[1]; \
        if (char_is_num(left)) left = left - '0'; \
        if (left >= 'a' && left <= 'f') { \
            left = (left - 'a') + 10; \
        } \
        if (left >= 'A' && left <= 'F') { \
            left = (left - 'A') + 10; \
        } \
        if (char_is_num(right)) right = right - '0'; \
        if (right >= 'a' && right <= 'f') { \
            right = (right - 'a') + 10; \
        } \
        if (right >= 'A' && right <= 'F') { \
            right = (right - 'A') + 10; \
        } \
        out = (left & 0x0F) | ((right << 4) & 0xF0); \
    } while (0)

    STRV_TO_HEX(red, r);
    STRV_TO_HEX(green, g);
    STRV_TO_HEX(blue, b);

    print("foreground: \\x1b[38;3;%d;%d;%dm\n", r, g, b);
    print("background: \\x1b[48;3;%d;%d;%dm\n", r, g, b);

    print("\x1b[38;2;%d;%d;%dm", r, g, b);
    print("Hello World" TERM_RESET);
    print("    ");
    print("\x1b[48;2;%d;%d;%dm" TERM_FG_BLACK, r, g, b);
    print("Hello World");
    print(TERM_RESET " ");
    print("\x1b[48;2;%d;%d;%dm" TERM_FG_WHITE, r, g, b);
    print("Hello World");
    print(TERM_RESET "\n");
}

void TOY(colors)(int argc, char **argv) {
    TOY_OPTION(colors) opt = {0};

    TOY_OPTION_PARSE(colors)(argc, argv, &opt);

    if (opt.count > 0) {
        for (int i = 0; i < opt.count; ++i) {
            color_print(opt.toprint[i]);
        }
        return;
    }

    switch (opt.type) {
        case COLOR_TYPE_BASIC:
            colors_print_basic();
            break;
        case COLOR_TYPE_256:
            colors_print_256();
            break;
    }
}
