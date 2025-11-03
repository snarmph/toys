#include "colla/colla.h"
#include "common.h"
#include "toys.h"

#define TAIL_MAX_FILES 1024

TOY_SHORT_DESC(tail, "Output the last 10 lines of a file/s.");

TOY_OPTION_DEFINE(tail) {
    bool in_piped;
    i64 bytes;
    i64 lines;
    bool quiet;
    bool verbose;
    bool follow;
    bool retry;
    char line_delim;
    strview_t files[TAIL_MAX_FILES];
    i64 files_count;
};

void TOY_OPTION_PARSE(tail)(int argc, char **argv, TOY_OPTION(tail) *opt) {
    opt->in_piped = common_is_piped(os_stdin());

    usage_helper(
        "tail [options] [FILE...]",
        "Output the last 10 lines of a file/s.\n"
        "Print the last 10 lines of each FILE to standard output. "
        "With more than one FILE, precede each with a header giving "
        "the file name.\n"
        "With no FILE, read standard input.\n",
        opt->in_piped ? USAGE_ALLOW_NO_ARGS : USAGE_DEFAULT,
        USAGE_EXTRA_PARAMS(opt->files, opt->files_count),
        argc, argv,
        {
            'c', "bytes",
            "Print the last {} bytes of each file.",
            "NUM",
            USAGE_INT(opt->bytes),
        },
        {
            'n', "lines",
            "Print the last {} lines of each file",
            "NUM",
            USAGE_INT(opt->lines),
        },
        {
            'f', "follow",
            "Output appended data as the file grows.",
            USAGE_BOOL(opt->follow),
        },
        {
            'q', "quiet",
            "Never print headers giving file names.",
            USAGE_BOOL(opt->quiet),
        },
        {
            'r', "retry",
            "Keep trying to open a file if it is inaccessible.",
            USAGE_BOOL(opt->retry),
        },
        {
            'v', "verbose",
            "Always print headers giving file names.",
            USAGE_BOOL(opt->verbose),
        },
        // {
        //     'z', "zero-terminated",
        //     "Line delimiter is NUL, not newline.",
        //     USAGE_BOOL(opt->zero_terminated),
        // },
    );

    if (!opt->bytes && !opt->lines) {
        opt->lines = 10;
    }
}

void tail_impl(arena_t scratch, oshandle_t fp, TOY_OPTION(tail) *opt) {
    i64 lines = opt->lines;
    i64 bytes = opt->bytes;

    // read file buffered as fp could be stdin
    str_t file_data = common_read_buffered(&scratch, fp);

    if (lines > 0) {
        strview_t data = strv(file_data);
        i64 line_count = 1;
        for (usize i = 0; i < data.len; ++i) {
            if (data.buf[i] == opt->line_delim) {
                line_count++;
            }
        }; 

        for (i64 to_skip = line_count - lines; to_skip > 0; --to_skip) {
            usize pos = strv_find(data, opt->line_delim, 0);
            if (pos == STR_NONE) break;
            data = strv_sub(data, pos + 1, STR_END);
        }

        print("%v", data);
    }
    else {
        strview_t data = str_sub(file_data, file_data.len - bytes, STR_END);
        println("%v", data);
    }
}

void TOY(tail)(int argc, char **argv) {
    TOY_OPTION(tail) opt = { .line_delim = '\n' };

    TOY_OPTION_PARSE(tail)(argc, argv, &opt);

    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));

    if (opt.files_count) {
        bool should_print_names = (opt.files_count > 1 && !opt.quiet) || opt.verbose;
        for (int i = 0; i < opt.files_count; ++i) {
            oshandle_t fp = os_file_open(opt.files[i], OS_FILE_READ);
            if (!os_handle_valid(fp)) {
                err("can't open %v: %v", opt.files[i], os_get_error_string(os_get_last_error()));
                continue;
            }
            if (should_print_names) {
                if (i > 0) println("\n");
                println(TERM_FG_ORANGE "==> %v <==" TERM_FG_DEFAULT, opt.files[i]);
            }
            tail_impl(arena, fp, &opt);
            os_file_close(fp);
        }
    }
    else {
        tail_impl(arena, os_stdin(), &opt);
    }
}
