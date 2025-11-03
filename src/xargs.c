#include "colla/colla.h" 
#include "common.h"
#include "toys.h"
#include "tui.h"

TOY_SHORT_DESC(xargs, "Build and execute command lines from standard input.");

#define XARGS_MAX_INITIAL_ARGS 1024

TOY_OPTION_DEFINE(xargs) {
    strview_t arg_file;
    char delimiter;
    bool use_delim;
    strview_t replace;
    i64 thread_count;
    i64 max_args;
    bool exit;
    bool interactive; 
    bool dont_run_on_empty;
    bool verbose;
    strview_t command;
    strview_t initial_args[XARGS_MAX_INITIAL_ARGS];
    i64 initial_args_count;

    oshandle_t print_mtx;
    strv_list_t *args;
    strview_t *args_shared;
    i64 total_args_shared;
    i64 top_job;
    os_barrier_t thread_barrier;
};

void parse_opts(int argc, char **argv, TOY_OPTION(xargs) *opt) {
    strview_t delimiter = STRV_EMPTY;
    bool null = false;

    usage_helper(
        "xargs [options] [command [initial-arguments]]", 
        "Build and execute command lines from standard input", 
        USAGE_DEFAULT,
        USAGE_EXTRA_PARAMS(opt->initial_args, opt->initial_args_count),
        argc, argv,
        { 
            '0', "null",
            "Input items are terminated by a null character instead "
            "of by whitespace, and the quotes and backslash are not "
            "special (every character is taken literally).",
            USAGE_BOOL(null),
        },
        {
            'a', "arg-file",
            "Read items from {} instead of standard input.",
            "file",
            USAGE_VALUE(opt->arg_file),
        },
        {
            'd', "delimiter",
            "Input items are terminated by {}.",
            "delim",
            USAGE_VALUE(delimiter),
        },
        {
            'n', "max-args",
            "Use at most max-args arguments per command line. "
            "Fewer than max-args arguments will be used if the "
            "size (see the -s option) is exceeded, unless the "
            "-x option is given, in which case xargs will exit.",
            "max-args",
            USAGE_INT(opt->max_args)
        },
        {
            'x', "exit",
            "Exit if the size (see the -n option) is exceeded.",
            USAGE_BOOL(opt->exit)
        },
        {
            'i', "replace",
            "Replace occurrences of {} in the initial-arguments with "
            "names read from standard input.",
            "replace-str",
            USAGE_VALUE(opt->replace),
        },
        {
            'j', "max-procs",
            "Run up to {} processes at a time; the default is 1. "
            "If {} is 0, xargs will run as many processes as possible "
            "at a time.",
            "max-procs",
            USAGE_INT(opt->thread_count),
        },
        {
            'p', "interactive",
            "Prompt the user about wheter to run each command line.",
            USAGE_BOOL(opt->interactive),
        },
        {
            'r', "no-run-if-empty",
            "If the standard input does not contain any nonblanks, "
            "do not run the command.",
            USAGE_BOOL(opt->dont_run_on_empty),
        },
        {
            'v', "verbose",
            "Print the command line on the standard error output "
            "before executing it.",
            USAGE_BOOL(opt->verbose),
        },
    );

    if (opt->initial_args_count == 0) {
        fatal("no command passed to xargs");
    }

    if (null) {
        opt->delimiter = '\0';
        opt->use_delim = true;
    }

    if (delimiter.len) {
        opt->delimiter = delimiter.buf[0];
        opt->use_delim = true;
    }

    opt->command = opt->initial_args[0];

    if (opt->thread_count && opt->interactive) {
        fatal("option -j and -p are mutually exculsive");
    }

    if (!opt->thread_count) {
        opt->thread_count = 1;
    }

    if (opt->max_args == 0) {
        opt->thread_count = 0;
    }
}

strv_list_t *xargs_grab_input(arena_t *arena, TOY_OPTION(xargs) *opt) {
    str_t args = STR_EMPTY;
    if (opt->arg_file.len) {
        args = os_file_read_all_str(arena, opt->arg_file);
    }
    else {
        args = common_read_buffered(arena, os_stdin());
    }

    strv_list_t *out = NULL;
    usize start = 0;

    bool in_double_quotes = false;
    bool in_single_quotes = false;
    bool in_backtick = false;

#define ADD_ARG(off) do { \
        strview_t cur = str_sub(args, start, i + off); \
        darr_push(arena, out, cur); \
        ++i; \
        start = i + 1; \
    } while (0)

    for (usize i = 0; i < args.len; ++i) {
        char c = args.buf[i];
        switch (c) {
            case '"':
                if (!(in_single_quotes || in_backtick)) {
                    in_double_quotes = !in_double_quotes;
                    if (!in_double_quotes) {
                        ADD_ARG(1);
                    }
                    continue;
                }
                break;
            case '\'':
                if (!(in_double_quotes || in_backtick)) {
                    in_single_quotes = !in_single_quotes;
                    if (!in_single_quotes) {
                        ADD_ARG(1);
                    }
                    continue;
                }
                break;
            case '`':
                if (!(in_double_quotes || in_single_quotes)) {
                    in_backtick = !in_backtick;
                    if (!in_backtick) {
                        ADD_ARG(1);
                    }
                    continue;
                }
                break;
            case '\\':
                // skip next char
                ++i;
                continue;
            default: break;
        }

        if (
            (opt->use_delim && c == opt->delimiter) || 
            (!opt->use_delim && char_is_space(c))
        ){
            ADD_ARG(0);
            // skip \n
            if (c == '\r') {
                ++i;
            }
            start = i;
        }
    }

    if (start < (args.len - 1)) {
        i64 i = args.len;
        ADD_ARG(0);
    }

    return out;
}

void xargs_run(arena_t scratch, strv_list_t *args, TOY_OPTION(xargs) *opt) {
    bool replace = opt->replace.len > 0;

    os_cmd_t *cmd = NULL;
    for (i64 i = 0; i < opt->initial_args_count; ++i) {
        strview_t arg = opt->initial_args[i];
        if (replace) {
            outstream_t ostr = ostr_init(&scratch);
            usize from = 0;
            while (from < arg.len) {
                usize pos = strv_find_view(arg, opt->replace, from);
                ostr_puts(&ostr, strv_sub(arg, from, pos));
                from = pos + opt->replace.len;
                if (pos == STR_END) {
                    break;
                }
                bool space = false;
                for_each (a, args) {
                    for (usize k = 0; k < a->count; ++k) {
                        if (space) ostr_putc(&ostr, ' ');
                        ostr_puts(&ostr, a->items[k]);
                        space = true;
                    }
                }
            }
            arg = strv(ostr_to_str(&ostr));
        }
        darr_push(&scratch, cmd, arg);
    }

    str_t command = STR_EMPTY;

    if (opt->interactive || opt->verbose) {
        outstream_t o = ostr_init(&scratch);
        
        for_each(c, cmd) {
            for (usize i = 0; i < c->count; ++i) {
                if (ostr_tell(&o)) ostr_putc(&o, ' ');
                ostr_puts(&o, c->items[i]);
            }
        }

        if (!replace) {
            for_each (a, args) {
                for (usize i = 0; i < a->count; ++i) {
                    ostr_putc(&o, ' ');
                    ostr_puts(&o, a->items[i]);
                }
            }
        }
        command = ostr_to_str(&o);
        if (opt->interactive) {
            str_t prompt = str_fmt(&scratch, "run \"%v\"", command);
            if (!common_prompt(strv(prompt))) {
                return;
            }
        }
    }

    os_cmd_t *full_cmd = NULL;
    darr_push(&scratch, full_cmd, strv("cmd.exe"));
    darr_push(&scratch, full_cmd, strv("/C"));

    if (!replace) {
        cmd->next = args;
    }
    full_cmd->next = cmd;

    oshandle_t hout = os_handle_zero();
    os_run_cmd(
        scratch, 
        full_cmd, 
        &(os_cmd_options_t){ .out = &hout }
    );

    str_t out = common_read_buffered(&scratch, hout);

    os_mutex_lock(opt->print_mtx);
        if (opt->verbose) {
            print("running \"%v\"\n", command);
        }
        if (out.len) {
            print("%v", out);
        }
    os_mutex_unlock(opt->print_mtx);
}

void xargs_entry_point(void *udata) {
    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));
    TOY_OPTION(xargs) *opt = udata;

    strview_t *args = NULL;
    i64 args_count = 0;

    if (os_thread_id == 0) {
        for_each (a, opt->args) {
            args_count += a->count;
        }

        if (opt->exit && (args_count % opt->max_args) != 0) {
            fatal(
                "%d arguments passed in, but with %d max "
                "arguments, there are %d arguments remaining",
                args_count, opt->max_args, args_count % opt->max_args
            );
        }

        args = alloc(&arena, strview_t, args_count);

        i64 cur = 0;
        for_each (a, opt->args) {
            for (usize i = 0; i < a->count; ++i) {
                args[cur++] = a->items[i];
            }
        }

        opt->args_shared = args;
        opt->total_args_shared = args_count;
    }

    os_barrier_sync(&opt->thread_barrier);

    if (os_thread_id != 0) {
        args = opt->args_shared;
        args_count = opt->total_args_shared;
    }

    i64 jobs_count = args_count / opt->max_args;
    i64 rem = args_count % opt->max_args;
    if (rem) {
        jobs_count++;
    }

    while (opt->top_job < jobs_count) {
        // should be safe to read on win64
        i64 cur_job = atomic_add_i64(&opt->top_job, 1);

        i64 base = cur_job * opt->max_args;
        i64 count = opt->max_args;
        if (rem && cur_job + 1 >= jobs_count) {
            count = rem;
        }
        i64 end = base + count;

        arena_t scratch = arena;
        strv_list_t *cur_args = NULL;
        for (i64 i = base; i < end; ++i) {
            darr_push(&scratch, cur_args, args[i]);
        }
        xargs_run(scratch, cur_args, opt);
    }
}

int xargs_thread_entry_point(u64 id, void *udata) {
    COLLA_UNUSED(id); 
    xargs_entry_point(udata);
    return 0;
}

void TOY(xargs)(int argc, char **argv) {
    // git submodule foreach pwd | xargs -i {} -j 8 "git -C {} pull"
    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));
    TOY_OPTION(xargs) opt = {0};
    parse_opts(argc, argv, &opt);

    opt.args = xargs_grab_input(&arena, &opt);
    if (opt.dont_run_on_empty && !opt.args) {
        fatal("no arguments passed");
    }

    if (opt.max_args == 0) {
        xargs_run(arena, opt.args, &opt);
        return;
    }

    opt.thread_barrier.thread_count = opt.thread_count;
    opt.print_mtx = os_mutex_create();
    oshandle_t *threads = alloc(&arena, oshandle_t, opt.thread_count);

    for (int i = 0; i < opt.thread_count; ++i) {
        threads[i] = os_thread_launch(xargs_thread_entry_point, &opt);
    }

    for (int i = 0; i < opt.thread_count; ++i) {
        os_thread_join(threads[i], NULL);
    }
}
