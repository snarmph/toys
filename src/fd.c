#include "colla/colla.h" 
#include "icons.h"
#include "common.h"
#include "toys.h"

TOY_SHORT_DESC(fd, "Find files.");

typedef struct jobdata_t jobdata_t;
struct jobdata_t {
    str_t dir;
};

TOY_OPTION_DEFINE(fd) {
    bool case_sensitive;
    bool extended;
    bool exact_name;
    bool all_dirs;
    bool vim_mode;
    strview_t dir;
    strview_t tofind;
    str_t tofind_original;

    bool is_piped;
    bool is_regex;
    i64 thread_count;
};

typedef struct worker_t worker_t;
struct {
    job_queue_t *jq;
    i64 checked;
    i64 found;
    i64 finished;

    strview_t curdir;
    strview_t prevdir;

    arena_t *worker_arenas;
    arena_t *scratch_arenas;
    oshandle_t print_mtx;

    TOY_OPTION(fd) opt;
} fd_data = {
    .curdir = cstrv("."),
    .prevdir = cstrv(".."),
};

// == globals =============

void fd_job(void *);

void TOY_OPTION_PARSE(fd)(int argc, char **argv, TOY_OPTION(fd) *opt) {
    strview_t filename[1];
    i64 fname_count = 0;

    usage_helper(
        "fd [options] NEEDLE",
        "Search for file. Needle can either be a filename "
        "(e.g. file.txt) or a simple regex (e.g. hello.*)",
        USAGE_DEFAULT,
        USAGE_EXTRA_PARAMS(filename, fname_count),
        argc, argv,
        {
            'd', "dir",
            "Search in {}.",
            "folder",
            USAGE_VALUE(opt->dir),
        },
        {
            'r', "regex",
            "Use extended regex instead of glob, so * isn't valid anymore, but .* does the same.",
            USAGE_BOOL(opt->extended),
        },
        {
            's', "sensitive",
            "Use case sensitive search.",
            USAGE_BOOL(opt->case_sensitive)
        },
        {
            'e', "exact",
            "Check for exact filename (e.g. hello will match "
            "folder/hello but not folder/hello.txt)",
            USAGE_BOOL(opt->exact_name),
        },
        {
            'a', "all",
            "Check in hidden directory too.",
            USAGE_BOOL(opt->all_dirs),
        },
        {
            'v', "vim",
            "Output in a format that can be used in vim",
            USAGE_BOOL(opt->vim_mode),
        },
        {
            'j', "threads",
            "Number of {} to use, if 0 it will use as many as possible.",
            "threads",
            USAGE_INT(opt->thread_count),
        },
    );

    if (opt->thread_count == 0) {
        opt->thread_count = os_get_system_info().processor_count;
    }

    opt->tofind = filename[0];
}
 
void fd_check_name(arena_t scratch, strview_t name, bool is_dir) {
    atomic_inc_i64(&fd_data.checked);

    strview_t current = name;
    if (!fd_data.opt.case_sensitive) {
        str_t filename = str(&scratch, current);
        str_upper(&filename);
        current = strv(filename);
    }

    usize index = 0;
    usize match_len = fd_data.opt.tofind.len;
    if (fd_data.opt.exact_name) {
        if (!strv_ends_with_view(current, fd_data.opt.tofind)) {
            return;
        }
        
        index = current.len - fd_data.opt.tofind.len;
    }
    else if (fd_data.opt.is_regex) {
        if (!rg_matches(fd_data.opt.tofind, current, !fd_data.opt.extended)) {
            return;
        }
        index = STR_END;
        match_len = 0;
    }
    else {
        index = strv_find_view(current, fd_data.opt.tofind, 0);
        if (index == STR_NONE) {
            return;
        }
    }

    atomic_inc_i64(&fd_data.found);

    strview_t icon = STRV_EMPTY;

    if (is_dir) {
        icon = icons[ICON_STYLE_NERD][ICON_FOLDER];
    }
    else {
        strview_t dir, filename, ext;
        os_file_split_path(name, &dir, &filename, &ext);
        icon = ext_to_ico(ext);
    }

    strview_t before = strv_sub(name, 0, index);
    strview_t match  = strv_sub(name, index, index + match_len);
    strview_t after  = strv_sub(name, index + match_len, STR_END);

    strview_t dir;
    os_file_split_path(before, &dir, NULL, NULL);

    strview_t filename = strv_sub(before, dir.len+1, STR_END);

    if (strv_equals(dir, strv("."))) {
        dir = STRV_EMPTY;
    }
    else if (dir.len > 0) {
        if (strv_starts_with_view(dir, strv("./"))) {
            dir = strv_remove_prefix(dir, 2);
        }
        str_t fmt = str_fmt(&scratch, "%v/", dir);
        dir = strv(fmt);
    }

    if (fd_data.opt.is_regex) {
        match = filename;
        filename = STRV_EMPTY;
    }

    os_mutex_lock(fd_data.print_mtx);
        if (fd_data.opt.is_piped) {
            println("%v%v%v%v", dir, filename, match, after);
        }
        else {
            println(
                TERM_FG_DARK_GREY "%v" TERM_RESET
                TERM_FG_YELLOW "%v" TERM_RESET
                TERM_FG_GREEN "%v" TERM_RESET
                TERM_FG_YELLOW "%v" TERM_RESET,
                dir, filename, match, after
            );
        }
    os_mutex_unlock(fd_data.print_mtx);
}

void iter_dir(arena_t scratch, strview_t path) {
    dir_t *dir = os_dir_open(&scratch, path);

    dir_foreach(&scratch, entry, dir) {
        strview_t name = strv(entry->name);

        if (strv_equals(name, fd_data.curdir) ||
            strv_equals(name, fd_data.prevdir))
        {
            continue;
        }

        arena_t *arena = &fd_data.worker_arenas[os_thread_id];
        str_t *fullpath = alloc(arena, str_t);
        *fullpath = os_path_join(arena, path, strv(entry->name));

        strview_t fullname = strv(*fullpath);
        if (strv_starts_with_view(fullname, strv("./"))) {
            fullname = strv_remove_prefix(fullname, 2);
        }
        
        fd_check_name(scratch, fullname, entry->type == DIRTYPE_DIR);

        if (entry->type == DIRTYPE_DIR) {
            if (!fd_data.opt.all_dirs && entry->name.buf[0] == '.') {
                continue;
            }

            jq_push(arena, fd_data.jq, fd_job, fullpath);
        }
    }
}

void fd_job(void *userdata) {
    arena_t scratch = fd_data.scratch_arenas[os_thread_id];
    str_t *dir = userdata;
    iter_dir(scratch, strv(*dir));
}

void TOY(fd)(int argc, char **argv) {
    TOY_OPTION_PARSE(fd)(argc, argv, &fd_data.opt);

    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));

    icons_init(ICON_STYLE_NERD);

    if (strv_is_empty(fd_data.opt.dir)) {
        fd_data.opt.dir = strv(".");
    }

    fd_data.opt.tofind_original = str(&arena, fd_data.opt.tofind);

    if (!fd_data.opt.case_sensitive) {
        str_t uppercase = strv_to_upper(&arena, fd_data.opt.tofind);
        fd_data.opt.tofind = strv(uppercase);
    }

    fd_data.opt.is_piped = common_is_piped(os_stdout());
    fd_data.opt.is_piped |= fd_data.opt.vim_mode;

    if (strv_contains(fd_data.opt.tofind, '*')) {
        fd_data.opt.is_regex = true;
    }

    fd_data.print_mtx = os_mutex_create();

    fd_data.worker_arenas = alloc(&arena, arena_t, fd_data.opt.thread_count);
    fd_data.scratch_arenas = alloc(&arena, arena_t, fd_data.opt.thread_count);
    for (int i = 0; i < fd_data.opt.thread_count; ++i) {
        fd_data.worker_arenas[i] = arena_make(ARENA_VIRTUAL, GB(1));
        fd_data.scratch_arenas[i] = arena_make(ARENA_VIRTUAL, GB(1));
    }

    fd_data.jq = jq_init(&arena, (int)fd_data.opt.thread_count);
    jq_push(&arena, fd_data.jq, fd_job, &fd_data.opt.dir);
    jq_cleanup(fd_data.jq);

    if (!fd_data.opt.is_piped) {
        print(">> files found: %lld/%lld\n", fd_data.found, fd_data.checked);
    }
}

