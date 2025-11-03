#define COLLA_NO_CONDITION_VARIABLE 1
#define COLLA_NO_NET 1

#include "../colla.c"

typedef enum optimise_level_e {
    OPTIMISE_NONE,
    OPTIMISE_FAST,
    OPTIMISE_SMALL,
    OPTIMISE__COUNT,
} optimise_level_e;

typedef enum warning_level_e {
    WARNING_NONE,
    WARNING_DEFAULT,
    WARNING_ALL,
    WARNING__COUNT,
} warning_level_e;

typedef enum sanitiser_e {
    SANITISER_NONE,
    SANITISER_ADDRESS,
    SANITISER__COUNT,
} sanitiser_e;

typedef enum cversion_e {
    CVERSION_LATEST,
    CVERSION_17,
    CVERSION_11,
    CVERSION__COUNT
} cversion_e;

typedef struct options_t options_t;
struct options_t {
    strview_t input_fname;
    strview_t out_fname;
    optimise_level_e optimisation;
    warning_level_e warnings;
    bool warnings_as_error;
    sanitiser_e sanitiser;
    bool fast_math;
    bool debug;
    strv_list_t *defines;
    cversion_e cstd;
    bool run;
    strv_list_t *run_args;
    bool is_cpp;
};

void print_help_message(void) {
    puts("usage:");
    puts("    -r / -run [input.c] [args...]  compiles and runs <input.c>, forwards <args...>");
    puts("    -h / -help                     print this message");
    puts("    -o / -out [filename]           output filename (default: build/<file>.exe)");
    puts("    -O / -optimise [fast,small]    optimisation level");
    puts("    -w / -warning [default,all]    warning level");
    puts("         -werror                   treat warnings as errors");
    puts("         -fsanitize [address]      turn on sanitiser");
    puts("         -fastmath                 turn on fast math");
    puts("    -g / -debug                    generate debug information");
    puts("    -D / -define [key=value,key]   add a preprocessor define ");
    puts("         -std [c11,c17,clatest]    select c standard (default: clatest)");
    puts("         -cpp                      compile c++ instead of c");
    exit(0);
}

optimise_level_e get_optimisation_level(strview_t arg) {
    if (strv_equals(arg, strv("fast"))) {
        return OPTIMISE_FAST;
    }
    else if (strv_equals(arg, strv("small"))) {
        return OPTIMISE_SMALL;
    }
    warn("unrecognised optimisation level: (%v)", arg);
    return OPTIMISE_NONE;
}

warning_level_e get_warning_level(strview_t arg) {
    if (strv_equals(arg, strv("default"))) {
        return WARNING_DEFAULT;
    }
    else if (strv_equals(arg, strv("all"))) {
        return WARNING_ALL;
    }
    warn("unrecognised warning level: (%v)", arg);
    return WARNING_NONE;
}

sanitiser_e get_sanitiser(strview_t arg) {
    if (strv_equals(arg, strv("address"))) {
        return SANITISER_ADDRESS;
    }
    warn("unrecognised sanitiser: (%v)", arg);
    return SANITISER_NONE;
}

cversion_e get_cversion(strview_t arg) {
    if (strv_equals(arg, strv("clatest"))) {
        return CVERSION_LATEST;
    }
    else if (strv_equals(arg, strv("c17"))) {
        return CVERSION_17;
    }
    else if (strv_equals(arg, strv("c11"))) {
        return CVERSION_11;
    }
    warn("unrecognised c std version: (%v)", arg);
    return CVERSION_LATEST;
}

options_t parse_options(arena_t *arena, int argc, char **argv) {
    options_t out = {0};

    for (int i = 1; i < argc; ++i) {
        strview_t arg = strv(argv[i]);

#define CHECK_OPT_BEG()        if (false) {}
#define CHECK_OPT1(opt)        else if (strv_equals(arg, strv("-" opt)))
#define CHECK_OPT2(small, big) else if (strv_equals(arg, strv("-" small)) || strv_equals(arg, strv("-" big)))

#define GET_NEXT_ARG() (i + 1) < argc ? strv(argv[++i]) : STRV_EMPTY

        CHECK_OPT_BEG()
        CHECK_OPT2("h", "help") {
            print_help_message();
        }
        CHECK_OPT2("o", "out") {
            strview_t out_fname = GET_NEXT_ARG();
            str_t out_fname_str = str_fmt(arena, "build/%v", out_fname);
            out.out_fname = strv(out_fname_str);
        }
        CHECK_OPT2("O", "optimise") {
            out.optimisation = get_optimisation_level(GET_NEXT_ARG());
        }
        CHECK_OPT2("w", "warning") {
            out.warnings = get_warning_level(GET_NEXT_ARG());
        }
        CHECK_OPT1("werror") {
            out.warnings_as_error = true;
        }
        CHECK_OPT1("fsanitize") {
            out.sanitiser = get_sanitiser(GET_NEXT_ARG());
        }
        CHECK_OPT1("fastmath") {
            out.fast_math = true;
        }
        CHECK_OPT2("g", "debug") {
            out.debug = true;
        }
        CHECK_OPT2("D", "define") {
            darr_push(arena, out.defines, GET_NEXT_ARG());
        }
        CHECK_OPT1("std") {
            out.cstd = get_cversion(GET_NEXT_ARG());
        }
        CHECK_OPT1("cpp") {
            out.is_cpp = true;
        }
        CHECK_OPT2("r", "run") {
            out.run = true;
            out.input_fname = GET_NEXT_ARG();
            for (i += 1; i < argc; ++i) {
                darr_push(arena, out.run_args, strv(argv[i]));
            }
        }
        else {
            out.input_fname = arg;
        }
    }

#undef CHECK_OPT_BEG
#undef CHECK_OPT1
#undef CHECK_OPT2
#undef GET_NEXT_ARG

    if (strv_is_empty(out.out_fname)) {
        strview_t name;
        os_file_split_path(out.input_fname, NULL, &name, NULL);
        str_t out_fname = str_fmt(arena, "build\\%v", name);
        out.out_fname = strv(out_fname);
    }

    return out;
}

int main(int argc, char **argv) {
    os_init();

    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));

    if (argc < 2) {
        print_help_message();
    }

    options_t opt = parse_options(&arena, argc, argv);

    if (!os_dir_exists(strv("build/"))) {
        info("creating build folder");
        mkdir("build", 755);
    }

    {
        arena_t scratch = arena;
        os_cmd_t *cmd = NULL;

        if (opt.is_cpp) {
            darr_push(&scratch, cmd, strv("c++"));
        }
        else {
            darr_push(&scratch, cmd, strv("cc"));
        }

        str_t output = str_fmt(&scratch, "-o %v", opt.out_fname);
        darr_push(&scratch, cmd, strv(output));

        strview_t optimisations[OPTIMISE__COUNT] = {
            strv("-O0"), // disabled
            strv("-O3"), // fast code
            strv("-Os"), // small code
        };
        darr_push(&scratch, cmd, optimisations[opt.optimisation]);
        
        strview_t warnings[WARNING__COUNT] = {
            strv("-w"),
            strv("-Weverything"),
            strv("-Wpedantic"),
        };
        darr_push(&scratch, cmd, warnings[opt.warnings]);

        if (opt.warnings_as_error) {
            darr_push(&scratch, cmd, strv("-Werror"));
        }

        if (opt.sanitiser) {
            strview_t sanitisers[SANITISER__COUNT] = {
                strv(""),
                strv("-fsanitize=address"),
            };
            darr_push(&scratch, cmd, sanitisers[opt.sanitiser]);
        }

        if (opt.fast_math) {
            darr_push(&scratch, cmd, strv("-ffast-math"));
        }

        if (opt.debug) {
            darr_push(&scratch, cmd, strv("-g"));
            darr_push(&scratch, cmd, strv("-D_DEBUG"));
        }

        for_each (def, opt.defines) {
            for (int i = 0; i < def->count; ++i) {
                str_t define = str_fmt(&scratch, "-D%v", def->items[i]);
                darr_push(&scratch, cmd, strv(define));
            }
        }

        strview_t cversion[CVERSION__COUNT] = {
            strv("c23"),
            strv("c17"),
            strv("c11"),
        };

        str_t cstd = str_fmt(&scratch, "-std %v", cversion[opt.cstd]);
        darr_push(&scratch, cmd, strv(cstd));

        darr_push(&scratch, cmd, opt.input_fname);

        oshandle_t hout = os_handle_zero();

        bool compilation_result = os_run_cmd(
            scratch, 
            cmd,
            &(os_cmd_options_t){ .out = &hout, }
        );
       
        str_t result = os_file_read_all_str_fp(&scratch, hout);

        info("result:\n%v\n", result);

        if (!compilation_result) {

            return 1;
        }
    }

    if (opt.run) {
        arena_t scratch = arena;
        os_cmd_t *cmd = NULL;

        darr_push(&scratch, cmd, opt.out_fname);

        for_each (arg, opt.run_args) {
            for (int i = 0; i < arg->count; ++i) {
                darr_push(&scratch, cmd, arg->items[i]);
            }
        }

        if (!os_run_cmd(scratch, cmd, NULL)) {
            return 1;
        }
    }

    arena_cleanup(&arena);

    os_cleanup();

}
