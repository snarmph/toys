#define COLLA_NO_CONDITION_VARIABLE 1
#define COLLA_NO_NET 1

#include "colla/colla.c"
#include "common.h"

#include <windows.h>
#include <direct.h>

#if COLLA_TCC

WINBASEAPI LPCH WINAPI GetEnvironmentStringsj(VOID);
WINBASEAPI LPWCH WINAPI GetEnvironmentStringsW(VOID);
WINBASEAPI BOOL WINAPI FreeEnvironmentStringsA(LPCH penv);
WINBASEAPI BOOL WINAPI FreeEnvironmentStringsW(LPWCH penv);
#ifdef UNICODE
    #define GetEnvironmentStrings GetEnvironmentStringsW
    #define FreeEnvironmentStrings FreeEnvironmentStringsW
#else
    #define GetEnvironmentStrings GetEnvironmentStringsA
    #define FreeEnvironmentStrings FreeEnvironmentStringsA
#endif

#endif

int strv_to_int(strview_t v) {
    instream_t in = istr_init(v);
    int out;
    return istr_get_i32(&in, &out) ? out : 0;
}

str_t find_vcvars_path(arena_t *arena) {
    strview_t base_path = strv("C:/Program Files/Microsoft Visual Studio");
    // find year
    int year = 0;
    {
        arena_t tmp = *arena;

        dir_t *dir = os_dir_open(&tmp, base_path);
        if (!os_dir_is_valid(dir)) {
            err("couldn't open directory (%v)", base_path);
            return STR_EMPTY;
        }

        dir_foreach(&tmp, entry, dir) {
            if (entry->type != DIRTYPE_DIR) continue;
            
            int number = strv_to_int(strv(entry->name));
            if (number > year) year = number;
        }
    }

    if (year == 0) {
        err("couldn't find visual studio year version");
        return STR_EMPTY;
    }

    str_t path_with_year = str_fmt(arena, "%v/%d", base_path, year);

    // find edition
    const char *editions[] = {
        "Enterprise",
        "Professional",
        "Community",
    };

    int edition = 0;

    for (; edition < arrlen(editions); ++edition) {
        arena_t tmp = *arena;
        str_t path = str_fmt(&tmp, "%v/%s", path_with_year, editions[edition]);
        if (os_dir_exists(strv(path))) {
            break;
        }
    }

    if (edition >= arrlen(editions)) {
        err("couldn't find visual studio edition");
        return STR_EMPTY;
    }

    str_t vcvars = str_fmt(arena, "%v/%s/VC/Auxiliary/Build/vcvars64.bat", path_with_year, editions[edition]);

    return vcvars;
}

bool load_cache(arena_t *arena) {
    if (!os_file_exists(strv("build/cache.ini"))) {
        err("build/cache.ini doesn't exist");
        return false;
    }

    {
        arena_t scratch = *arena;

        ini_t ini = ini_parse(&scratch, strv("build/cache.ini"), &(iniopt_t){ .comment_vals = strv("#") });
        initable_t *root = ini_get_table(&ini, INI_ROOT);
        if (!root) fatal("fail");
    
        for_each (val, root->values) {
            os_set_env_var(scratch, val->key, val->value);
        }
    }

    return true;
}

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

struct {
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
} opt = {0};

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

void parse_options(arena_t *arena, int argc, char **argv) {
#define GET_NEXT_ARG() (i + 1) < argc ? strv(argv[++i]) : STRV_EMPTY
#define IS(v) strv_equals(arg, strv("--" v))

    for (int i = 1; i < argc; ++i) {
        strview_t arg = strv(argv[i]);

        if (arg.buf[0] == '-') {
            if (arg.buf[1] == '-') {
                if (IS("help")) {
                    print_help_message();
                }
                else if (IS("out")) {
                    strview_t out_fname = GET_NEXT_ARG();
                    str_t out_fname_str = str_fmt(arena, "build/%v", out_fname);
                    opt.out_fname = strv(out_fname_str);
                }
                else if (IS("optimise")) {
                    opt.optimisation = get_optimisation_level(GET_NEXT_ARG());
                }
                else if (IS("warning")) {
                    opt.warnings = get_warning_level(GET_NEXT_ARG());
                }
                else if (IS("werror")) {
                    opt.warnings_as_error = true;
                }
                else if (IS("fsanitize")) {
                    opt.sanitiser = get_sanitiser(GET_NEXT_ARG());
                }
                else if (IS("fastmath")) {
                    opt.fast_math = true;
                }
                else if (IS("debug")) {
                    opt.debug = true;
                }
                else if (IS("define")) {
                    darr_push(arena, opt.defines, GET_NEXT_ARG());
                }
                else if (IS("std")) {
                    opt.cstd = get_cversion(GET_NEXT_ARG());
                }
                else if (IS("cpp")) {
                    opt.is_cpp = true;
                }
                else if (IS("run")) {
                    opt.run = true;
                    opt.input_fname = GET_NEXT_ARG();
                    for (i += 1; i < argc; ++i) {
                        darr_push(arena, opt.run_args, strv(argv[i]));
                    }
                }
            }
            else {
                for (int k = 1; k < arg.len; ++k) {
                    switch (arg.buf[k]) {
                        case 'h':
                            print_help_message();
                            break;
                        case 'o':
                        {
                            strview_t out_fname = GET_NEXT_ARG();
                            str_t out_fname_str = str_fmt(arena, "build/%v", out_fname);
                            opt.out_fname = strv(out_fname_str);
                            break;
                        }
                        case 'O':
                            opt.optimisation = get_optimisation_level(GET_NEXT_ARG());
                            break;
                        case 'w':
                            opt.warnings = get_warning_level(GET_NEXT_ARG());
                            break;
                        case 'g':
                            opt.debug = true;
                            break;
                        case 'D':
                            break;
                        case 'r':
                            opt.run = true;
                            opt.input_fname = GET_NEXT_ARG();
                            for (i += 1; i < argc; ++i) {
                                darr_push(arena, opt.run_args, strv(argv[i]));
                            }
                            break;
                    }
                }
            }
        }
        else {
            opt.input_fname = arg;
        }
    }

    if (strv_is_empty(opt.out_fname)) {
        strview_t name;
        os_file_split_path(opt.input_fname, NULL, &name, NULL);
        str_t out_fname = str_fmt(arena, "build\\%v", name);
        opt.out_fname = strv(out_fname);
    }

#undef GET_NEXT_ARG
#undef IS
}

void cl_print_line(arena_t scratch, strview_t line) {
    instream_t in = istr_init(line);

    strview_t file = istr_get_view(&in, '(');
    istr_skip(&in, 1);
    i64 filepos = 0;
    istr_get_i64(&in, &filepos);
    istr_skip(&in, 2);
    istr_skip_whitespace(&in);
    strview_t type = istr_get_view(&in, ' ');
    istr_skip_whitespace(&in);

    strview_t id = istr_get_view(&in, ':');
    istr_skip(&in, 1);
    strview_t msg = istr_get_line(&in);

    if (strv_equals(type, strv("warning"))) {
        print(TERM_FG_YELLOW() "warning ");
    }
    else if (strv_equals(type, strv("error"))) {
        print(TERM_FG_RED() "error ");
    }
    else {
        print("%v ", type);
    }

    print("%v " TERM_RESET() "@ " TERM_FG_ORANGE() "%v:%d" TERM_RESET() "\n", id, file, filepos);
    print("  %v\n", msg);

    return;

print_plain:
    print("%v\n", line);
}

int main(int argc, char **argv) {
    os_init();

    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));

    if (argc < 2) {
        print_help_message();
    }

    parse_options(&arena, argc, argv);

    if (!os_dir_exists(strv("build/"))) {
        info("creating build folder");
        _mkdir("build");
    }

    if (!os_file_exists(strv("build/cache.ini"))) {
        info("couldn't find cache.ini, creating it now");

        arena_t scratch = arena;
        str_t vcvars_path = find_vcvars_path(&scratch);

        os_cmd_t *cmd = NULL;
        darr_push(&scratch, cmd, strv(vcvars_path));
        darr_push(&scratch, cmd, strv("&&"));
        darr_push(&scratch, cmd, strv("set"));
        darr_push(&scratch, cmd, strv(">"));
        darr_push(&scratch, cmd, strv("build\\cache.ini"));

        if (!os_run_cmd(scratch, cmd, NULL)) { 
            fatal("failed to run vcvars64.bat");
            os_abort(1);
        }
    }

    {
        arena_t scratch = arena;

        if (!load_cache(&scratch)) {
            os_abort(1);
        }
        
        os_cmd_t *cmd = NULL;

        darr_push(&scratch, cmd, strv("cl"));
        darr_push(&scratch, cmd, strv("/nologo"));
        darr_push(&scratch, cmd, strv("/utf-8"));
        if (!opt.is_cpp) {
            darr_push(&scratch, cmd, strv("/TC"));
        }

        str_t output = str_fmt(&scratch, "/Fe:%v.exe", opt.out_fname);
        str_t object = str_fmt(&scratch, "/Fo:%v.obj", opt.out_fname);
        darr_push(&scratch, cmd, strv(output));
        darr_push(&scratch, cmd, strv(object));

        strview_t optimisations[OPTIMISE__COUNT] = {
            strv("/Od"), // disabled
            strv("/O2"), // fast code
            strv("/O1"), // small code
        };
        darr_push(&scratch, cmd, optimisations[opt.optimisation]);
        
        strview_t warnings[WARNING__COUNT] = {
            strv("/W0"),
            strv("/W3"),
            strv("/W4"),
        };
        darr_push(&scratch, cmd, warnings[opt.warnings]);

        if (opt.warnings_as_error) {
            darr_push(&scratch, cmd, strv("/WX"));
        }

        if (opt.sanitiser) {
            strview_t sanitisers[SANITISER__COUNT] = {
                strv(""),
                strv("/fsanitize=address"),
            };
            darr_push(&scratch, cmd, sanitisers[opt.sanitiser]);
        }

        if (opt.fast_math) {
            darr_push(&scratch, cmd, strv("/fp:fast"));
        }

        if (opt.debug) {
            darr_push(&scratch, cmd, strv("/Zi"));
            darr_push(&scratch, cmd, strv("/D_DEBUG"));
        }

        for_each (def, opt.defines) {
            for (int i = 0; i < def->count; ++i) {
                str_t define = str_fmt(&scratch, "/D%v", def->items[i]);
                darr_push(&scratch, cmd, strv(define));
            }
        }

        strview_t cversion[CVERSION__COUNT] = {
            strv("clatest"),
            strv("c17"),
            strv("c11"),
        };

        str_t cstd = str_fmt(&scratch, "/std:%v", cversion[opt.cstd]);
        darr_push(&scratch, cmd, strv(cstd));

        darr_push(&scratch, cmd, opt.input_fname);

        // /LD -> create dynamic lib
        // /LDd -> create debug dynamic lib
        // /link
        
        oshandle_t hout = os_handle_zero();

        bool compilation_result = os_run_cmd(
            scratch, 
            cmd,
            &(os_cmd_options_t){ .out = &hout, }
        );

        str_t result = common_read_buffered(&scratch, hout);

        instream_t in = istr_init(strv(result));

        strview_t basename;
        os_file_split_path(opt.input_fname, NULL, &basename, NULL);
        str_t filename = str_fmt(&scratch, "%v.c", basename);

        while (!istr_is_finished(&in)) {
            strview_t line = istr_get_line(&in);
            if (strv_equals(line, strv(filename))) {
                pretty_print(scratch, "<green>compiled:</> %v\n", line);
            }
            else if (strv_starts_with_view(line, strv("LINK"))) {
                pretty_print(scratch, "<blue>link:</> %v\n", line);
            }
            else {
                cl_print_line(scratch, line);
            }
        }

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
