#pragma once

#include "colla/colla.h"
#include "common.h"

#define TOY(name) toy_##name

#define TOY_SHORT_DESC(name, desc) strview_t toy_##name##_short_desc = cstrv(desc)

// return true if 'next' argument is consumed
typedef bool (usage_callback_f)(strview_t next, void *userdata);

typedef enum {
    USAGE_DEFAULT       = 0,
    USAGE_NO_EXTRA      = 1 << 1,
    USAGE_ALLOW_NO_ARGS = 1 << 2,
} usage_options_e;

#define optional(T) struct { T value; bool is_set; }

#define USAGE_EXTRA_PARAMS(arr, len) \
    .extra_args = (arr), \
    .extra_count = &(len), \
    .extra_max = arrlen(arr)
#define USAGE_NO_EXTRA() .extra_max=0
#define USAGE_BOOL(b) .bool_param = &(b)
#define USAGE_VALUE(v) .value_param = &(v)
#define USAGE_OPT(v) .opt_param = (void*)&(v)
#define USAGE_INT(i) .int_param = &(i)
#define USAGE_CALLBACK(cb, ud) .callback = (cb), userdata = (ud),
#define USAGE_ARRAY(arr, len) \
    .array_param = (arr), \
    .array_param_len = &(len), \
    .array_param_maxlen = arrlen(arr),
#define USAGE_HELP() { 'h', "help", "Print this message.", .is_help = true }

#define usage_helper(usage_str, desc_str, options_in, extra, argc_in, argv_in, ...) \
    usage_help__impl(&(print_usage_desc_t){ \
        .usage = usage_str, \
        .desc = desc_str, \
        .argc = argc_in, \
        .argv = argv_in, \
        options_in, \
        extra, \
        .items = (print_item_t[]){ \
            USAGE_HELP(), \
            __VA_ARGS__ \
        }, \
        .items_count = arrlen(((print_item_t[]){ USAGE_HELP(), __VA_ARGS__ })), \
    })

typedef struct print_item_t print_item_t;
struct print_item_t {
    char flag;
    const char *long_flag;
    const char *desc;
    const char *param;

    bool *bool_param;
    strview_t *value_param;
    optional(strview_t) *opt_param;
    i64 *int_param;
    strview_t *array_param;
    usage_callback_f *callback;
    void *userdata;
    i64 *array_param_len;
    i64 array_param_maxlen;
    bool is_help;

    strview_t __long_flag;
    i64 __flag_len;
    bool __no_params;
};

typedef struct print_usage_desc_t print_usage_desc_t;
struct print_usage_desc_t {
    int argc;
    char **argv;
    usage_options_e options;
    strview_t *extra_args;
    i64 *extra_count;
    i64 extra_max;
    const char *usage;
    const char *desc;
    print_item_t *items;
    int items_count;
};

i64 print_usage__words(strview_t v, i64 spaces, i64 rem, i64 size);
void usage_help__impl(print_usage_desc_t *desc);
