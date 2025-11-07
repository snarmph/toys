#include "colla/colla.h"
#include "common.h"
#include "toys.h"
#include "tui.h"

#define CAT_MAX_FILES 1024
#define CAT_MAX_PRETTY_SIZE MB(10)
#define CAT_SORROUND_COL TERM_FG_ORANGE

#define CAT_INI_CATCOL TERM_FG_RED
#define CAT_INI_KEYCOL TERM_FG_GREEN
#define CAT_INI_VALCOL TERM_FG_YELLOW
#define CAT_INI_CMNCOL TERM_ITALIC TERM_FG_DARK_GREY

TOY_SHORT_DESC(cat, "Concatenate FILE(s) to standard output.");

typedef enum {
    HG_NONE,
    HG_CLIKE,
    HG_JSON,
    HG_INI,
    HG_XML,

    HG__COUNT,
} highlighters_e;

typedef struct cat_hg_state_t cat_hg_state_t;
struct cat_hg_state_t {
    i64 tw;
    i64 rem;
    i64 line_digits;
    i64 in_str;
};

typedef struct {
    bool plain;
    bool nocolor;
    bool in_piped;
    bool out_piped;
    highlighters_e lang;
    strview_t files[CAT_MAX_FILES];
    i64 file_count;
} cat_opt_t;

typedef struct cat_lang_t cat_lang_t;
struct cat_lang_t {
    highlighters_e highlighter;
    strview_t ext;
};

strview_t cat_ver_bar = cstrv(CAT_SORROUND_COL "│" TERM_RESET);
typedef void (*highlight_f)(arena_t scratch, strview_t line, cat_hg_state_t *state);

cat_lang_t cat_recognised_langs[] = {
    { HG_CLIKE, cstrv(".h") },
    { HG_CLIKE, cstrv(".hh") },
    { HG_CLIKE, cstrv(".hpp") },
    { HG_CLIKE, cstrv(".c") },
    { HG_CLIKE, cstrv(".cc") },
    { HG_CLIKE, cstrv(".cpp") },
    { HG_CLIKE, cstrv(".c++") },
    { HG_JSON,  cstrv(".json") },
    { HG_JSON,  cstrv(".cjson") },
    { HG_INI,   cstrv(".ini") },
    { HG_INI,   cstrv(".toml") },
    { HG_INI,   cstrv(".conf") },
    { HG_XML,   cstrv(".xml") },
};

void cat_parse_opts(int argc, char **argv, cat_opt_t *opt) {
    opt->in_piped = common_is_piped(os_stdin());
    opt->out_piped = common_is_piped(os_stdout());

    strview_t lang = STRV_EMPTY;

    usage_helper(
        "cat [options] FILE",
        "Concatenate FILE(s) to standard output.\n"
        "With no FILE, or if FILE is -, read standard input.",
        opt->in_piped ? USAGE_ALLOW_NO_ARGS : USAGE_DEFAULT,
        USAGE_EXTRA_PARAMS(opt->files, opt->file_count),
        argc, argv,
        {
            'p', "plain",
            "Print plain, without any highlighting nor line numbers.",
            USAGE_BOOL(opt->plain)
        },
        {
            'c', "no-color",
            "Print without any highlighting.",
            USAGE_BOOL(opt->nocolor)
        },
        {
            'l', "lang",
            "Color using {} parser, possible values: [c, json, ini].",
            "language",
            USAGE_VALUE(lang),
        },
    );

    // we only do pretty printing for one file, sorry!
    if (opt->file_count == 1) {
        strview_t ext;
        os_file_split_path(opt->files[0], NULL, NULL, &ext);
        for (int i = 0; i < arrlen(cat_recognised_langs); ++i) {
            if (strv_equals(ext, cat_recognised_langs[i].ext)) {
                opt->lang = cat_recognised_langs[i].highlighter;
                break;
            }
        }
    }

    if (lang.len) {
        if (strv_equals(lang, strv("c"))) {
            opt->lang = HG_CLIKE;
        }
        else if (strv_equals(lang, strv("json"))) {
            opt->lang = HG_JSON;
        }
        else if (strv_equals(lang, strv("ini"))) {
            opt->lang = HG_INI;
        }
        if (opt->lang == HG_NONE) {
            warn("unrecognised lang: %v", lang);
            opt->plain = true;
        }
    }

    if (opt->nocolor) {
        opt->lang = HG_NONE;
    }

    if (opt->out_piped) {
        opt->plain = true;
    }
}

void cat_print_with_colors(const char *colors, strview_t line, cat_hg_state_t *state) {
    instream_t in = istr_init(line);
    i64 rem = state->rem;
    while (!istr_is_finished(&in)) {
        strview_t word = istr_get_word(&in);
        if (!istr_is_finished(&in) && word.len == 0) {
            word = istr_get_view_len(&in, 1);
        }
        // if word is too long for line, split it
        while ((i64)word.len > state->rem) {
            strview_t prev = strv_remove_suffix(word, word.len - rem);
            print("%s%v\n %*s %v ", colors, prev, state->line_digits, "", cat_ver_bar);
            word = strv_remove_prefix(word, prev.len);
            rem = state->tw;
        }
        rem -= word.len;
        
        if (rem < 0) {
            print("\n %*s %v ", state->line_digits, "", cat_ver_bar);
            rem = state->tw;
        }
        print("%s%v", colors, word);
    }
}

void cat_print_plain(arena_t scratch, strview_t line, cat_hg_state_t *state) {
    COLLA_UNUSED(scratch);
    instream_t in = istr_init(line);
    i64 rem = state->tw;
    while (!istr_is_finished(&in)) {
        strview_t word = istr_get_word(&in);
        if (!istr_is_finished(&in) && word.len == 0) {
            word = istr_get_view_len(&in, 1);
        }
        // if word is too long for line, split it
        while ((i64)word.len > state->tw) {
            strview_t prev = strv_remove_suffix(word, word.len - rem);
            print("%v\n %*s %v ", prev, state->line_digits, "", cat_ver_bar);
            word = strv_remove_prefix(word, prev.len);
            rem = state->tw;
        }
        rem -= word.len;
        
        if (rem < 0) {
            print("\n %*s %v ", state->line_digits, "", cat_ver_bar);
            rem = state->tw;
        }
        print("%v", word);
    }
}

void cat_print_c(arena_t scratch, strview_t line, cat_hg_state_t *state) {
    COLLA_UNUSED(scratch); COLLA_UNUSED(line); COLLA_UNUSED(state);
    // TODO
}

#define CAT_JSON_OBJ_COL TERM_FG_DARK_GREY
#define CAT_JSON_ARR_COL TERM_FG_DARK_GREY
#define CAT_JSON_STR_COL TERM_FG_GREEN
#define CAT_JSON_NUM_COL TERM_FG_ORANGE
#define CAT_JSON_NIL_COL TERM_BOLD TERM_ITALIC TERM_FG_PURPLE
#define CAT_JSON_BOL_COL TERM_FG_RED
#define CAT_JSON_CMN_COL TERM_FG_DARK_GREY TERM_ITALIC

void cat_print_json(arena_t scratch, strview_t line, cat_hg_state_t *state) {
    COLLA_UNUSED(scratch);
    instream_t in = istr_init(line);

#define PUTC(col) \
    do { \
        if (--state->rem < 0) { \
            print("\n %*s %v ", state->line_digits, "", cat_ver_bar); \
            state->rem = state->tw; \
        } \
        print(col "%c", istr_get(&in)); \
    } while (0)
#define PUTC_RESET(col) \
    do { \
        if (--state->rem < 0) { \
            print("\n %*s %v ", state->line_digits, "", cat_ver_bar); \
            state->rem = state->tw; \
        } \
        print(col "%c" TERM_RESET, istr_get(&in)); \
    } while (0)

    state->rem = state->tw;

    while (!istr_is_finished(&in)) {
        char c = istr_peek(&in);

        if (state->in_str % 2 != 0 && c != '"') {
            PUTC(CAT_JSON_STR_COL);
            continue;
        }

        switch (c) {
            case '{':
            case '}':
                PUTC_RESET(CAT_JSON_OBJ_COL);
                break;
            case '[':
            case ']':
                PUTC_RESET(CAT_JSON_ARR_COL);
                break;
            case '/':
            {
                strview_t rest = istr_get_line(&in);
                cat_print_with_colors(CAT_JSON_CMN_COL, rest, state);
                break;
            }
            case '"':
                state->in_str++;
                PUTC(CAT_JSON_STR_COL);
                if (state->in_str % 2 == 0) {
                    print(TERM_RESET);
                }
                break;
            case 'n':
            {
                strview_t word = istr_get_view_either(&in, strv(" \n\r,/]}"));
                cat_print_with_colors(CAT_JSON_NIL_COL, word, state);
                print(TERM_RESET);
                break;
            }
            case 't':
            case 'f':
            {
                strview_t word = istr_get_view_either(&in, strv(" \n\r,/]}"));
                cat_print_with_colors(CAT_JSON_BOL_COL, word, state);
                print(TERM_RESET);
                break;
            }
            default:
                if (char_is_num(c)) {
                    print(CAT_JSON_NUM_COL "%c" TERM_RESET, istr_get(&in));
                }
                else {
                    PUTC("");
                }
                break;
        }
    }

#undef PUTC
#undef PUTC_RESET
}

void cat_print_ini(arena_t scratch, strview_t line, cat_hg_state_t *state) {
    state->rem = state->tw;

    instream_t in = istr_init(line);
    istr_skip_whitespace(&in);
    char p = istr_peek(&in);
    istr_rewind(&in);
    // category
    if (p == '[') {
        outstream_t out = ostr_init(&scratch);
        strview_t cat = istr_get_view(&in, ']');
        ostr_print(&out, "%v]", cat);
        str_t toprint = ostr_to_str(&out);
        cat_print_with_colors(TERM_FG_RED, strv(toprint), state);
    }
    else if (p == ';' || p == '#') {
        cat_print_with_colors(TERM_ITALIC TERM_FG_DARK_GREY, line, state);
    }
    else if (p != '\0') {
        istr_rewind(&in);
        strview_t key = istr_get_view_either(&in, strv("=#;"));
        istr_skip(&in, 1);
        strview_t val = istr_get_view_either(&in, strv("#;\n"));
        strview_t cmn = istr_get_view(&in, '\n');
        cat_print_with_colors(TERM_FG_GREEN, key, state);
        state->rem -= key.len;
        cat_print_with_colors(TERM_FG_DEFAULT, strv("="), state);
        state->rem -= 1;
        cat_print_with_colors(TERM_FG_YELLOW, val, state);
        state->rem -= val.len;
        cat_print_with_colors(TERM_ITALIC TERM_FG_DARK_GREY, cmn, state);
    }
}

void cat_print(arena_t scratch, oshandle_t fp, cat_opt_t *opt) {
    char buffer[KB(10)] = {0};

    if (!opt->plain && !os_handle_match(fp, os_stdin())) {
        usize size = os_file_size(fp);
        // too big, just print plain
        if (size >= CAT_MAX_PRETTY_SIZE) {
            opt->plain = true;
        }
    }

    // no lines or anything, just print away
    if(opt->plain) {
        while (true) {
            usize read = os_file_read(fp, buffer, sizeof(buffer));
            if (read == 0) break;
            print("%v", strv(buffer, read));
        }
        return;
    }

    // grab whole input
    outstream_t data_stream = ostr_init(&scratch);
    while (true) {
        usize read = os_file_read(fp, buffer, sizeof(buffer));
        if (read == 0) break;
        ostr_puts(&data_stream, strv(buffer, read));
    }

    str_t data = ostr_to_str(&data_stream);

    // count lines
    i64 total_line_count = 1;
    for (usize i = 0; i < data.len; ++i) {
        total_line_count += data.buf[i] == '\n';
    }

    i64 rem_line_count = total_line_count;
    int line_count_digits = 0;
    while (rem_line_count > 0) {
        line_count_digits++;
        rem_line_count /= 10;
    }

    int tw = tui_width();

    // print header

    print(CAT_SORROUND_COL);

    for (int i = -2; i < line_count_digits; ++i) {
        print("─");
    }
    print("┬");

    for (int i = line_count_digits + 3; i < tw; ++i) {
        print("─");
    }
    println(
        " %*s "
        CAT_SORROUND_COL "│" TERM_FG_DEFAULT
        " File: " CAT_SORROUND_COL TERM_ITALIC "%v" TERM_RESET, line_count_digits, "", opt->files[0]);

    print(CAT_SORROUND_COL);
    for (int i = -2; i < line_count_digits; ++i) {
        print("─");
    }

    print("┼");
    for (int i = line_count_digits + 3; i < tw; ++i) {
        print("─");
    }
    print(TERM_RESET);

    // print lines

    instream_t in = istr_init(strv(data));
    cat_hg_state_t state = {
        .line_digits = line_count_digits,
        .tw = tw - (line_count_digits + 4),
    };

    highlight_f highlight_functions[HG__COUNT] = {
        [HG_NONE]  = cat_print_plain,
        [HG_CLIKE] = cat_print_plain, // TODO 
        [HG_JSON]  = cat_print_json,
        [HG_INI]   = cat_print_ini,
        [HG_XML]   = cat_print_plain, // TODO
    };

    highlight_f highlight = highlight_functions[opt->lang];

    int line_count = 1;
    while (!istr_is_finished(&in)) {
        print(CAT_SORROUND_COL " %*d │ " TERM_RESET, line_count_digits, line_count++);
        highlight(scratch, istr_get_line(&in), &state);
        if (!istr_is_finished(&in)) println("");
    }
}

void TOY(cat)(int argc, char **argv) {
    cat_opt_t opt = {0};
    cat_parse_opts(argc, argv, &opt);

    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));

    bool pipe_stdin = opt.in_piped;

    if (opt.file_count > 1) {
        opt.plain = true;
    }

    for (i64 i = 0; !pipe_stdin && i < opt.file_count; ++i) {
        if (strv_equals(opt.files[i], strv("-"))) {
            pipe_stdin = true;
            break;
        }
    }

    if (pipe_stdin) {
        if (opt.file_count > 0) {
            opt.plain = true;
            opt.lang = HG_NONE;
        }
        else {
            opt.files[0] = strv("stdin");
            cat_print(arena, os_stdin(), &opt);
            return;
        }
    }

    for (int i = 0; i < opt.file_count; ++i) {
        strview_t f = opt.files[i];
        oshandle_t fp = os_handle_zero();

        bool is_stdin = strv_equals(f, strv("-"));

        if (is_stdin) {
            fp = os_stdin();
        }
        else {
            fp = os_file_open(f, OS_FILE_READ);
            if (!os_handle_valid(fp)) {
                fatal("couldn't open %v: %v", fp, os_get_error_string(os_get_last_error()));
            }
        }

        if (i == 0 && opt.in_piped && !is_stdin) {
            cat_print(arena, os_stdin(), &opt);
        }
        cat_print(arena, fp, &opt);
    }

}
