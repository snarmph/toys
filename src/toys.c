#define COBJMACROS 1

#include "colla/colla.c"
#include "toys.h"
#include "common.h"
#include "tui.c"

// toys include
#include "basename.c"
#include "cal.c"
#include "cat.c"
#include "fd.c"
#include "head.c"
#include "tail.c"
#include "ls.c"
#include "colors.c"
#include "rm.c"
#include "stat.c"
#include "strings.c"
#include "touch.c"
#include "xxd.c"
#include "http.c"
#include "serve.c"
#include "neofetch.c"
#include "xargs.c"
// #include "get.c" // TODO 
#include "base64.c"
#include "uptime.c"
#include "pwgen.c"
// #include "cksum.c" // TODO
#include "acpi.c"
#include "wc.c"
#include "tee.c"
#include "time.c"
#include "unlock.c"

typedef struct toy_t toy_t;
struct toy_t {
    strview_t name;
    strview_t desc;
    void (*main_fn)(int argc, char **argv);
};

int main(int argc, char **argv) {
    colla_init(COLLA_OS);

    toy_t toys[] = {
        TOY_DEFINE(acpi),
        TOY_DEFINE(base64),
        TOY_DEFINE(basename),
        TOY_DEFINE(cal),
        TOY_DEFINE(cat),
        // TOY_DEFINE(cksum), // TODO implement correct checksum algorithims
        TOY_DEFINE(colors),
        TOY_DEFINE(fd),
        // TOY_DEFINE(get), // TODO seems to only use ~2 threads instead of N, is this a winapi limitation?
        TOY_DEFINE(head),
        TOY_DEFINE(http),
        TOY_DEFINE(ls),
        TOY_DEFINE(neo),
        TOY_DEFINE(pwgen),
        TOY_DEFINE(rm),
        TOY_DEFINE(serve),
        TOY_DEFINE(stat),
        TOY_DEFINE(strings),
        TOY_DEFINE(tail),
        TOY_DEFINE(tee),
        TOY_DEFINE(time),
        TOY_DEFINE(touch),
        TOY_DEFINE(unlock),
        TOY_DEFINE(uptime),
        TOY_DEFINE(wc),
        TOY_DEFINE(xargs),
        TOY_DEFINE(xxd),
    };

    tui_update_size();

    if (argc > 1) {
        strview_t toy_name = strv(argv[1]);

        if (strv_equals(toy_name, strv("-l")) ||
            strv_equals(toy_name, strv("--list"))
        ) {
            for (int i = 0; i < arrlen(toys); ++i) {
                print("%v", toys[i].name);
                // if (i + 1 < arrlen(toys)) print("\n");
                if (i + 1 < arrlen(toys)) println("");
            }
            return 0;
        }

        for (int i = 0; i < arrlen(toys); ++i) {
            if (strv_equals(toys[i].name, toy_name)) {
                toys[i].main_fn(argc - 1, argv + 1);
                return 0;
            }
        }
    }

    int tw = tui_width();

    i64 max_length = 1;
    for (int i = 0; i < arrlen(toys); ++i) {
        i64 cur_len = toys[i].name.len + 4;
        max_length = MAX(cur_len, max_length);
    }

    println("usage: toys [options] [command] TOY [toy arguments]\n");
    println("Run one of following toys:");
    for (int i = 0; i < arrlen(toys); ++i) {
        i64 spaces = (i64)(max_length - toys[i].name.len - 3);
        print(
            TERM_ITALIC TERM_FG_LIGHT_ORANGE
                "  %v"
            TERM_RESET
            ":%*s",
            toys[i].name, spaces, ""
        );
        i64 rem = tw - max_length;
        print_usage__words(toys[i].desc, max_length, rem, rem);
        println("");
    }
    println("");
    i64 spaces = max_length - strlen("-l, --list");
    println("-l, --list%*sPrint all the toys as a newline-delimited list.", spaces, "");
}

// usage helpers

i64 print_usage__words(strview_t v, i64 spaces, i64 rem, i64 size) {
    instream_t in = istr_init(v);
    i64 cur = rem;
    while (!istr_is_finished(&in)) {
        strview_t word = istr_get_view(&in, ' ');
        istr_skip(&in, 1);
        cur -= word.len + 1;
        if (cur < 0) {
            cur = size - (word.len + 1);
            print("\n%*s", spaces, "");
        }
        print("%v ", word);
    }
    return cur;
}

void print_usage__impl(print_usage_desc_t *desc) {
    println("usage: %s\n", desc->usage);
    println("%s", desc->desc);

    if (desc->items_count == 1 && desc->items[0].__no_params) {
        goto finish;
    }

    print("\n");

    i64 max_flag_len = 0;
    i64 width = tui_width();

    for (int i = 0; i < desc->items_count; ++i) {
        print_item_t *it = &desc->items[i];
        i64 flag_len = 0;
        if (it->flag) flag_len += 2;
        if (it->long_flag) {
            flag_len += strlen(it->long_flag) + 2;
            if (it->flag) flag_len += 2;
        }
        if (it->param) flag_len += strlen(it->param) + 1;
        it->__flag_len = flag_len;
        // spaces
        flag_len += 2;
        max_flag_len = MAX(max_flag_len, flag_len);
    }

    i64 rem = width - max_flag_len;

    for (int i = 0; i < desc->items_count; ++i) {
        print_item_t *it = &desc->items[i];
        strview_t param = strv(it->param);

        if (it->flag) {
            print("-%c", it->flag);
        }
        if (it->long_flag) {
            if (it->flag) print(", ");
            print("--%s", it->long_flag);
        }
        if (it->param) {
            print(
                TERM_FG_LIGHT_ORANGE TERM_ITALIC " %v" TERM_RESET,
                param
            );
        }
        i64 spaces = max_flag_len - it->__flag_len;
        print("%*s", spaces, "");
        strview_t it_desc = strv(it->desc);
        i64 cur_rem = rem;
        while (it_desc.len > 0) {
            usize index = strv_find_view(it_desc, strv("{}"), 0);
            if (index == STR_NONE) {
                break;
            }
            strview_t prev = strv_sub(it_desc, 0, index);
            it_desc = strv_sub(it_desc, index + 3, STR_END);
            cur_rem = print_usage__words(prev, max_flag_len, cur_rem, rem);
            print(TERM_FG_LIGHT_ORANGE TERM_ITALIC);
            cur_rem = print_usage__words(param, max_flag_len, cur_rem, rem);
            print(TERM_RESET);
        }
        print_usage__words(it_desc, max_flag_len, cur_rem, rem);
        println("");
    }

finish:
    os_abort(0);
}

void usage_help__impl(print_usage_desc_t *desc) {
#define SET_ARG() \
    do { \
        if (it->is_help) { \
            print_usage__impl(desc); \
        } \
        if (it->bool_param) { \
            *it->bool_param = true; \
        } \
        if (it->int_param) { \
            *it->int_param = common_strv_to_int(next); \
            ++i; \
        } \
        if (it->value_param) { \
            *it->value_param = next; \
            ++i; \
        } \
        if (it->opt_param) { \
            it->opt_param->value = next;\
            it->opt_param->is_set = true;\
            ++i; \
        }\
        if (it->callback) { \
            bool valid = it->callback(next, it->userdata); \
            i += valid; \
        } \
        if (it->array_param_maxlen) { \
            if (*it->array_param_len >= it->array_param_maxlen) { \
                fatal("too many arguments for %v, max is %d", it->__long_flag, it->array_param_maxlen); \
            } \
            it->array_param[*it->array_param_len++] = next; \
            ++i; \
        } \
    } while (0)
    
    if (desc->argc == 1 && !(desc->options & USAGE_ALLOW_NO_ARGS)) {
        print_usage__impl(desc);
    }

    for (int i = 0; i < desc->items_count; ++i) {
        desc->items[i].__long_flag = strv(desc->items[i].long_flag);
    }

    for (int i = 1; i < desc->argc; ++i) {
        strview_t arg = strv(desc->argv[i]);
        strview_t next = i + 1 < desc->argc ? strv(desc->argv[i+1]) : STRV_EMPTY;
        if (arg.buf[0] == '-') {
            if (arg.buf[1] == '-') {
                bool found = false;
                strview_t flag = strv_remove_prefix(arg, 2);
                for (int k = 0; k < desc->items_count; ++k) {
                    print_item_t *it = &desc->items[k];
                    if (strv_equals(flag, it->__long_flag)) {
                        found = true;
                        SET_ARG();
                        break;
                    }
                }
                if (!found) {
                    err("uknown flag: %v", arg);
                    print_usage__impl(desc);
                }
            }
            else {
                for (usize k = 1; k < arg.len; ++k) {
                    bool found = false;
                    char flag = arg.buf[k];
                    for (int h = 0; h < desc->items_count; ++h) {
                        print_item_t *it = &desc->items[h];
                        if (it->flag == flag) {
                            found = true;
                            SET_ARG();
                            break;
                        }
                    }
                    if (!found) {
                        err("uknown flag: -%c", arg.buf[k]);
                        print_usage__impl(desc);
                    }
                }
            }
        }
        else if (!(desc->options & USAGE_NO_EXTRA)) {
            if (*desc->extra_count >= desc->extra_max) {
                fatal("too many arguments, max is %d", desc->extra_max);
            }
            desc->extra_args[(*desc->extra_count)++] = arg;
        }
        else {
            err("unknown extra param: %v", arg);
            print_usage__impl(desc);
        }
    }
#undef SET_ARG
}
