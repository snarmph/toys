#include "colla/colla.h" 
#include "common.h"
#include "toys.h"
#include "tui.h"

TOY_SHORT_DESC(less, "Page contents of a file.");

typedef struct {
    strview_t file;
    bool in_piped;

    bool no_border;
    bool no_linenum;

    str_t data;
    i64 offset;
    i64 line_count;
} less_options_t;

void less_parse_options(int argc, char **argv, less_options_t *opt) {
    opt->in_piped = common_is_piped(os_stdin());
    strview_t files[1];
    i64 count = 0;

    usage_helper(
        "less [FILE]", 
        "Page contents of a file.", 
        opt->in_piped ? USAGE_ALLOW_NO_ARGS : USAGE_DEFAULT, 
        USAGE_EXTRA_PARAMS(files, count), 
        argc, argv,
        {
            'b', "no-border",
            "Don't display the border with the filename and the scrollbar.",
            USAGE_BOOL(opt->no_border),
        },
        {
            'n', "no-line-num",
            "Don't display the line number.",
            USAGE_BOOL(opt->no_linenum),
        },
    );

    opt->file = opt->in_piped ? strv("stdin") : files[0];
    if (opt->in_piped && count) {
        fatal("cannot page both stdin and input file");
    }
}

#define IS(v) strv_equals(key, strv(v))

bool less_update(arena_t *arena, float dt, void *userdata) {
    COLLA_UNUSED(arena); COLLA_UNUSED(dt);

    less_options_t *opt = userdata;

    tui_ver_split((float)tui_height() - 1.f);
        tuielem_t *p = tui_begin();
            p->list_current_item = (int)opt->offset;
            if (!opt->no_border) {
                p->title = opt->file;
                p->border = TUI_BORDER_ROUND | TUI_BORDER_ALL;
            }
            p->list_type = TUI_LIST_LINE;
            if (!opt->no_linenum) p->list_type |= TUI_LIST_DISPLAY_NUMBER;
            tui_set_max_elements(p, (int)opt->line_count);
            instream_t in = istr_init(strv(opt->data));
            for (i64 i = 0; i < opt->line_count; ++i) {
                strview_t line = istr_get_line(&in);
                if (strv_back(line) == '\r') {
                    line.len--;
                }
                tui_print("%v", line);
            }
        tui_end();

        tui_begin();
            tui_print("<dark_grey>quit: q, up: k, down: j, next page: ctrl+d, prev page: ctrl+u");
        tui_end();
    tui_end();

    return false;
}

bool less_event(arena_t *arena, strview_t key, void *userdata) {
    COLLA_UNUSED(arena);

    less_options_t *opt = userdata;
    int th = tui_height() - (2 * !opt->no_border) - 1;
    int half = th / 2;

    if (IS("ctrl+c")) {
        return true;
    }

    if (IS("ctrl+d")) {
        opt->offset += th;
    }
    else if (IS("ctrl+u")) {
        opt->offset -= th;
    }

    if (key.len == 1) {
        switch (key.buf[0]) {
            case 'j':
                opt->offset++;
                break;
            case 'k':
                opt->offset--;
                break;

            case 'q': return true;
        }
    }

    if (opt->offset < half) {
        opt->offset = half;
    }
    else if (opt->offset >= opt->line_count - half) {
        opt->offset = opt->line_count - half;
    }

    return false;
}

#undef IS

void TOY(less)(int argc, char **argv) {
    less_options_t opt = {0};
    less_parse_options(argc, argv, &opt);

    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));

    oshandle_t fp = os_stdin();
    if (!opt.in_piped) {
        fp = os_file_open(opt.file, OS_FILE_READ);
        if (!os_handle_valid(fp)) {
            fatal("could not open %v for reading: %v", opt.file, os_get_error_string(os_get_last_error()));
        }
    }
    opt.data = common_read_buffered(&arena, fp);
    if (!opt.in_piped) {
        os_file_close(fp);
    }

    print("%v", opt.data);
    // return;

    for (usize i = 0; i < opt.data.len; ++i) {
        opt.line_count += opt.data.buf[i] == '\n';
    }

    opt.offset = (tui_height() - (2 * !opt.no_border) - 1) / 2;

    tui_init(&(tui_desc_t){
        .update = less_update,
        .event = less_event,
        .userdata = &opt,
    });
    tui_run();
}
