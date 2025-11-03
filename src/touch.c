#include "colla/colla.h" 
#include "toys.h"

TOY_SHORT_DESC(touch, "Create a file/s.");

void touch_create_intermediate(strview_t path) {
    strview_t dirs;
    os_file_split_path(path, &dirs, NULL, NULL);

    usize index = 0;
    strview_t make_path = { path.buf, 0 };

    while (index != STR_NONE) {
        index = strv_find_either(dirs, strv("\\/"), 0);

        strview_t newdir = strv_sub(dirs, 0, index);
        dirs = strv_remove_prefix(dirs, index != STR_NONE ? index + 1 : 0);

        make_path.len += newdir.len + 1;
        if (os_dir_exists(make_path)) {
            continue;
        }
        
        if (!os_dir_create(make_path)) {
            fatal("couldn't create folder (%v): %v", make_path, os_get_error_string(os_get_last_error()));
        }
    }
}

void TOY(touch)(int argc, char **argv) {
    strview_t files[1024] = {0};
    i64 file_count = 0;
    bool create_intermediate = false;
    bool interactive = false;

    usage_helper(
        "touch FILE...", 
        "Create a file/s.", 
        USAGE_DEFAULT, 
        USAGE_EXTRA_PARAMS(files, file_count), 
        argc, argv,
        {
            'c', "create-intermediate",
            "Create intermediate directories needed to create the file",
            USAGE_BOOL(create_intermediate),
        },
        {
            'i', "interactive",
            "Ask before creating each file",
            USAGE_BOOL(interactive),
        },
    );

    for (i64 i = 0; i < file_count; ++i) {
        strview_t f = files[i];
        if (os_file_exists(f)) {
            warn("file %v already exists", f);
            continue;
        }

        if (interactive) {
            u8 buf[KB(1)];
            arena_t scratch = arena_make(ARENA_STATIC, sizeof(buf), buf);
            str_t prompt = str_fmt(&scratch, "create %v", f);
            if (!common_prompt(strv(prompt))) {
                continue;
            }
        }

        oshandle_t fp = os_file_open(f, OS_FILE_WRITE);
        if (os_handle_valid(fp)) {
            goto finish;
        }
        // if we couldn't create the file, we need to create all 
        // the folders before it
        if (!os_handle_valid(fp) && !create_intermediate) {
            warn("couldn't create %v: %v", f, os_get_error_string(os_get_last_error()));
            continue;
        }

        touch_create_intermediate(f);

finish:
        println("created " TERM_FG_GREEN "%v" TERM_RESET, f);
        os_file_close(fp);
    }
}
