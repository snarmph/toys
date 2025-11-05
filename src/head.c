#include "colla/colla.h"
#include "common.h"
#include "toys.h"

#define HEAD_MAX_FILES 1024

TOY_SHORT_DESC(head, "Output the first 10 lines of a file/s.");

TOY_OPTION_DEFINE(head) {
    bool in_piped;
    i64 bytes;
    i64 lines;
    bool quiet;
    bool verbose;
    char line_delim;
    strview_t files[HEAD_MAX_FILES];
    i64 files_count;
}; 

i64 head__count = 0;
bool head__print_names = false;

void TOY_OPTION_PARSE(head)(int argc, char **argv, TOY_OPTION(head) *opt) {
    bool zero_terminated = false;

    opt->in_piped = common_is_piped(os_stdin());

    usage_helper(
        "head [options] [FILE...]", 
        "Output the first 10 lines of a file/s", 
        opt->in_piped ? USAGE_ALLOW_NO_ARGS : USAGE_DEFAULT, 
        USAGE_EXTRA_PARAMS(opt->files, opt->files_count), 
        argc, argv,
        {
            'c', "bytes",
            "Print the first NUM bytes of each file; with the "
            "leading '-', print all but the last NUM bytes of "
            "each file.",
            "NUM",
            USAGE_INT(opt->bytes)
        },
        {
            'n', "lines",
            "Print the first NUM lines instead of the first 10; "
            "with the leading '-', print all but the last NUM "
            "lines of each file.",
            "NUM",
            USAGE_INT(opt->lines)
        },
        {
            'q', "quiet",
            "Never print headers giving file names.",
            USAGE_BOOL(opt->quiet),
        },
        {
            'v', "verbose",
            "Always print headers giving file names.",
            USAGE_BOOL(opt->verbose),
        },
        {
            'z', "zero-terminated",
            "Line delimiter is NUL, not newline.",
            USAGE_BOOL(zero_terminated),
        },
    );

    if (!opt->bytes && !opt->lines) {
        opt->lines = 10;
    }

    if (zero_terminated) {
        opt->line_delim = '\0';
    }
}

void head_tail(arena_t scratch, oshandle_t fp, TOY_OPTION(head) *opt) {
    i64 lines = -opt->lines;
    i64 bytes = -opt->bytes;

    // read file buffered as fp could be stdin
    outstream_t ostr = ostr_init(&scratch);
    char buf[KB(10)] = {0};
    while (true) {
        usize read = os_file_read(fp, buf, sizeof(buf));
        ostr_puts(&ostr, strv(buf, read));
        if (read < sizeof(buf)) {
            break;
        }
    }
    str_t data = ostr_to_str(&ostr);

    if (lines > 0) {
        i64 line_count = 0;
        for (usize i = 0; i < data.len; ++i) {
            if (data.buf[i] == opt->line_delim) {
                line_count++;
            }
        }; 
        if (line_count < lines) {
            return;
        }
        instream_t in = istr_init(strv(data));
        i64 to_print = line_count - lines;
        for (i64 i = 0; i < to_print; ++i) {
            strview_t line = istr_get_view(&in, opt->line_delim);
            istr_skip(&in, 1);
            println("%v", line);
        }
    }
    else {
        strview_t view = str_sub(data, 0, data.len - bytes);
        println("%v", view);
    }
}

void head_impl(arena_t scratch, oshandle_t fp, TOY_OPTION(head) *opt) {
    i64 lines_rem = opt->lines;
    i64 bytes_rem = opt->bytes;

    if (lines_rem < 0 || bytes_rem < 0) {
        head_tail(scratch, fp, opt);
        return;
    }

    bool lines = opt->lines > 0;

    u8 buffer[KB(10)] = {0};
    while (lines_rem > 0 || bytes_rem > 0) {
        usize read = os_file_read(fp, buffer, sizeof(buffer));
        if (lines) {
            for (usize i = 0; i < read && lines_rem > 0; ++i) {
                if (opt->line_delim == '\n' && buffer[i] == '\r') continue;
                if (buffer[i] == opt->line_delim) {
                    lines_rem--;
                    if (lines_rem <= 0) break;
                }
                print("%c", buffer[i]);
            }
        }
        else {
            i64 todo = MIN((i64)read, bytes_rem);
            bytes_rem -= todo;
            for (i64 i = 0; i < todo; ++i) {
                if (buffer[i] == '\r') continue;
                print("%c", buffer[i]);
            }
        }

        if (read == 0) {
            break;
        }
    }
}

void head__glob(arena_t scratch, strview_t fname, void *udata) {
    TOY_OPTION(head) *opt = udata;
   
    oshandle_t fp = os_file_open(fname, OS_FILE_READ);
    if (!os_handle_valid(fp)) {
        err("can't open %v: %v", fname, os_get_error_string(os_get_last_error()));
        return;
    }

    if (head__print_names && !opt->quiet) {
        if (head__count++ > 0) println("\n");
        println(TERM_FG_ORANGE "==> %v <==" TERM_FG_DEFAULT, fname);
    }

    head_impl(scratch, fp, opt);
    os_file_close(fp);
}

void TOY(head)(int argc, char **argv) {
    TOY_OPTION(head) opt = { .line_delim = '\n' };

    TOY_OPTION_PARSE(head)(argc, argv, &opt);

    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));
    glob_t glob_desc = {
        .recursive = true,
        .udata = &opt,
        .cb = head__glob,
    };

    head__print_names = opt.files_count > 1;

    if (opt.files_count) {
        for (int i = 0; i < opt.files_count; ++i) {
            if (common_is_glob(opt.files[i])) {
                head__print_names = true;
                glob_desc.exp = opt.files[i];
                common_glob(arena, &glob_desc);
            }
            else {
                head__glob(arena, opt.files[i], &opt);
            }
        }
    }
    else {
        head_impl(arena, os_stdin(), &opt);
    }
}
