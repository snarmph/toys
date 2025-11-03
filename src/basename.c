#include "colla/colla.h"
#include "common.h"
#include "toys.h"

#define BASENAME_MAX_NAMES 256

TOY_SHORT_DESC(basename, "Return non-directory portion of a pathname removing suffix.");

TOY_OPTION_DEFINE(basename) {
    strview_t names[BASENAME_MAX_NAMES];
    i64 name_count;
    bool in_piped;
};

void TOY_OPTION_PARSE(basename)(int argc, char **argv, TOY_OPTION(basename) *opt) {
    opt->in_piped = common_is_piped(os_stdin());

    usage_helper(
        "basename [NAME]...",
        "Return non-directory portion of a pathname removing suffix.",
        opt->in_piped ? USAGE_ALLOW_NO_ARGS : USAGE_DEFAULT, 
        USAGE_EXTRA_PARAMS(opt->names, opt->name_count),
        argc, argv,
    );
}

void TOY(basename)(int argc, char **argv) {
    TOY_OPTION(basename) opt = {0};
    TOY_OPTION_PARSE(basename)(argc, argv, &opt);

    if (opt.in_piped) {
        arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));
        str_t input = common_read_buffered(&arena, os_stdin());
        instream_t in = istr_init(strv(input));
        strview_t name = STRV_EMPTY;

        while (!istr_is_finished(&in)) {
            strview_t line = istr_get_line(&in);
            if (line.len == 0) continue;
            os_file_split_path(line, NULL, &name, NULL);
            println("%v", name);
        }
    }

    for (i64 i = 0; i < opt.name_count; ++i) {
        strview_t name = STRV_EMPTY;
        os_file_split_path(opt.names[i], NULL, &name, NULL);
        println("%v", name);
    }
}
