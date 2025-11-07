#include "colla/colla.h" 
#include "common.h"
#include "toys.h"

TOY_SHORT_DESC(base64, "base64 encode/decode data and print to standard output");

#define BASE64_MAX_ARGS 10000

typedef struct {
    strview_t file;
    strview_t args[BASE64_MAX_ARGS];
    i64 arg_count;
    bool decode;
    bool in_piped;
} base64_opt_t;

void base64_parse_opts(int argc, char **argv, base64_opt_t *opt) {
    opt->in_piped = common_is_piped(os_stdin());

    usage_helper(
        "base64 [options] [FILE]", 
        "Base64 encode or decode from or to base64 from standard input.", 
        opt->in_piped ? USAGE_ALLOW_NO_ARGS : USAGE_DEFAULT, 
        USAGE_EXTRA_PARAMS(opt->args, opt->arg_count), 
        argc, argv,
        {
            'd', "decode",
            "Decode data.",
            USAGE_BOOL(opt->decode),
        },
        {
            'f', "file",
            "Decode from {} instead of STDIN.",
            "file",
            USAGE_VALUE(opt->file)
        },
    );

    if (!opt->in_piped &&
         opt->file.len == 0 &&
         opt->arg_count == 0
    ) {
        fatal("nothing passed in");
    }
}

void TOY(base64)(int argc, char **argv) {
    base64_opt_t opt = {0};
    base64_parse_opts(argc, argv, &opt);

    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));

    str_t data = {0};
    if (opt.in_piped) {
        data = common_read_buffered(&arena, os_stdin());
    }
    else if (opt.file.len) {
        data = os_file_read_all_str(&arena, opt.file);
    }
    else {
        outstream_t out = ostr_init(&arena);
        for (i64 i = 0; i < opt.arg_count; ++i) {
            if (i > 0) ostr_putc(&out, ' ');
            ostr_puts(&out, opt.args[i]);
        }
        data = ostr_to_str(&out);
    }

    buffer_t in = { .data = (u8*)data.buf, .len = data.len };
    buffer_t out = {0};
    if (opt.decode) {
        out = base64_decode(&arena, in);
    }
    else {
        out = base64_encode(&arena, in);
    }
    print("%v", out);
}
