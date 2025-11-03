#include "colla/colla.h"
#include "common.h"
#include "toys.h"

#define TEE_MAX_FILES 1024

TOY_SHORT_DESC(tee, "Copy standard input to each FILE, and also to standard output.");

TOY_OPTION_DEFINE(tee) {
    bool in_piped;
    strview_t files[TEE_MAX_FILES];
    i64 file_count;
};

void TOY_OPTION_PARSE(tee)(int argc, char **argv, TOY_OPTION(tee) *opt) {
    opt->in_piped = common_is_piped(os_stdin());

    if (!opt->in_piped) {
        fatal("nothing piped in");
    }

    usage_helper(
        "tee [options] [FILE]...", 
        "Copy standard input to each FILE, and also to standard output.", 
        USAGE_ALLOW_NO_ARGS, 
        USAGE_EXTRA_PARAMS(opt->files, opt->file_count), 
        argc, argv,
    );
}

void TOY(tee)(int argc, char **argv) {
    TOY_OPTION(tee) opt = {0};
    TOY_OPTION_PARSE(tee)(argc, argv, &opt);

    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));
    str_t input = common_read_buffered(&arena, os_stdin());

    bool out_printed = false;
    for (i64 i = 0; i < opt.file_count; ++i) {
        if (!out_printed && strv_equals(opt.files[i], strv("-"))) {
            out_printed = true;
            os_file_write_all_str_fp(os_stdout(), strv(input));
        }
        else {
            os_file_write_all_str(opt.files[i], strv(input));
        }
    }

    if (!out_printed) {
        os_file_write_all_str_fp(os_stdout(), strv(input));
    }
}
