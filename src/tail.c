#include "colla/colla.h"
#include "common.h"
#include "toys.h"

#define TAIL_MAX_FILES 1024

TOY_SHORT_DESC(tail, "Output the last 10 lines of a file/s.");

typedef struct {
    bool in_piped;
    i64 bytes;
    i64 lines;
    bool quiet;
    bool verbose;
    bool follow;
    i64 poll_time;
    bool retry;
    char line_delim;
    strview_t files[TAIL_MAX_FILES];
    i64 files_count;
} tail_opt_t;

bool tail__print_names = false;
i64 tail__count = 0;

void tail_parse_opts(int argc, char **argv, tail_opt_t *opt) {
    opt->in_piped = common_is_piped(os_stdin());

    usage_helper(
        "tail [options] [FILE...]",
        "Output the last 10 lines of a file/s.\n"
        "Print the last 10 lines of each FILE to standard output. "
        "With more than one FILE, precede each with a header giving "
        "the file name.\n"
        "With no FILE, read standard input.\n",
        opt->in_piped ? USAGE_ALLOW_NO_ARGS : USAGE_DEFAULT,
        USAGE_EXTRA_PARAMS(opt->files, opt->files_count),
        argc, argv,
        {
            'c', "bytes",
            "Print the last {} bytes of each file.",
            "NUM",
            USAGE_INT(opt->bytes),
        },
        {
            'n', "lines",
            "Print the last {} lines of each file",
            "NUM",
            USAGE_INT(opt->lines),
        },
        {
            'f', "follow",
            "Output appended data as the file grows.",
            USAGE_BOOL(opt->follow),
        },
        {
            'q', "quiet",
            "Never print headers giving file names.",
            USAGE_BOOL(opt->quiet),
        },
        {
            'r', "retry",
            "Keep trying to open a file if it is inaccessible.",
            USAGE_BOOL(opt->retry),
        },
        {
            'p', "poll",
            "Poll file every {} instead of using file changed events.",
            "milliseconds",
            USAGE_INT(opt->poll_time),
        },
        {
            'v', "verbose",
            "Always print headers giving file names.",
            USAGE_BOOL(opt->verbose),
        },
    );

    if (!opt->bytes && !opt->lines) {
        opt->lines = 10;
    }

    if (opt->follow && (opt->files_count > 1 || opt->in_piped)) {
        fatal("cannot follow more than one file nor stdin");
    }
}

str_t tail_impl(arena_t *arena, oshandle_t fp, tail_opt_t *opt) {
    i64 lines = opt->lines;
    i64 bytes = opt->bytes;

    // read file buffered as fp could be stdin
    str_t file_data = common_read_buffered(arena, fp);

    if (lines > 0) {
        strview_t data = strv(file_data);
        i64 line_count = 1;
        for (usize i = 0; i < data.len; ++i) {
            if (data.buf[i] == opt->line_delim) {
                line_count++;
            }
        }; 

        for (i64 to_skip = line_count - lines; to_skip > 0; --to_skip) {
            usize pos = strv_find(data, opt->line_delim, 0);
            if (pos == STR_NONE) break;
            data = strv_sub(data, pos + 1, STR_END);
        }

        return str(arena, data);
    }
    else {
        strview_t data = str_sub(file_data, file_data.len - bytes, STR_END);
        return str_fmt(arena, "%v\n", data);
    }
}

#define FW_MAX_FILES 1024

typedef struct file_watcher_t file_watcher_t;
struct file_watcher_t {
    HANDLE handle;
    OVERLAPPED overlapped;
    str_t watched;
    u8 change_buffer[1024];
    i64 poll_time;
    u64 last_change_time;
    i64 last_time;
    i64 tps;
};

file_watcher_t fw_init(arena_t *arena, strview_t fname, i64 optional_poll_time) {
    file_watcher_t fw = {0};

    fw.tps = common_get_tps();
    fw.last_time = common_get_ticks();

    if (optional_poll_time) {
        fw.poll_time = optional_poll_time;
        fw.watched = str(arena, fname);
        fw.last_change_time = os_file_time(fname);
        return fw;
    }

    strview_t dir = STRV_EMPTY;
    os_file_split_path(fname, &dir, NULL, NULL);
    strview_t name = strv_remove_prefix(fname, dir.len);
    fw.watched = str(arena, name);

    if (dir.len == 0) {
        dir = strv("./");
    }

    arena_t scratch = *arena;
    str_t fullpath = os_file_fullpath(&scratch, dir);
    str16_t wdir = strv_to_str16(&scratch, strv(fullpath));

    fw.handle = CreateFileW(
        wdir.buf, 
        FILE_LIST_DIRECTORY, 
        FILE_SHARE_READ | FILE_SHARE_WRITE, 
        NULL, 
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL
    );

    if (fw.handle == INVALID_HANDLE_VALUE) {
        fatal("couldn't open folder %v for watching: %v", dir, os_get_error_string(os_get_last_error()));
    }

    return fw;
}

void fw_cleanup(file_watcher_t *fw) {
    if (fw->handle && fw->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(fw->handle);
        fw->handle = INVALID_HANDLE_VALUE;
    }
}

// this blocks unless poll_time was set on init
bool fw_has_changed(arena_t scratch, file_watcher_t *fw) {
    bool changed = false;

    if (fw->poll_time) {
        i64 now = common_get_ticks();
        i64 passed = now - fw->last_time;
        i64 ms = (i64)(((double)passed / (double)fw->tps) * 1000);
        if (ms >= fw->poll_time) {
            u64 new_time = os_file_time(strv(fw->watched));
            changed = new_time > fw->last_change_time;
            fw->last_change_time = new_time;
            fw->last_time = now;
        }
    }
    else {
        DWORD bytes_transferred = 0;
        if (!GetOverlappedResult(fw->handle, &fw->overlapped, &bytes_transferred, TRUE)) {
            fatal("%v", os_get_error_string(os_get_last_error()));
        }

        FILE_NOTIFY_INFORMATION *event = (FILE_NOTIFY_INFORMATION *)fw->change_buffer;
        while (bytes_transferred) {
            if (event->Action == FILE_ACTION_MODIFIED) {
                str_t filename = str_from_str16(&scratch, str16_init(event->FileName, event->FileNameLength / sizeof(char16_t)));
                if (str_equals(fw->watched, filename)) {
                    changed = true;
                }
            }

            if (event->NextEntryOffset) {
                u8 *new_event = ((u8*)event) + event->NextEntryOffset;
                event = (FILE_NOTIFY_INFORMATION *)new_event;
            }
            else {
                break;
            }
        }

        BOOL success = ReadDirectoryChangesW(
            fw->handle, 
            fw->change_buffer, sizeof(fw->change_buffer), 
            FALSE, 
            FILE_NOTIFY_CHANGE_LAST_WRITE, 
            NULL, 
            &fw->overlapped, 
            NULL
        );
        if (!success) {
            fatal("could reset directory watcher for new notifications: %v", os_get_error_string(os_get_last_error()));
        }
    }

    return changed;
}

// wait a little when retrying
bool tail_wait(int ms) {
    Sleep(ms);
    return true;
}

void tail__glob(arena_t scratch, strview_t fname, void *udata) {
    tail_opt_t *opt = udata;

    oshandle_t fp = os_handle_zero();
    do {
        fp = os_file_open(fname, OS_FILE_READ);
    } while (opt->retry && !os_handle_valid(fp) && tail_wait(100));

    if (!os_handle_valid(fp)) {
        err("can't open %v: %v", fname, os_get_error_string(os_get_last_error()));
        return;
    }

    if (tail__print_names && !opt->quiet) {
        if (tail__count++ > 0) println("\n");
        println(TERM_FG_ORANGE "==> %v <==" TERM_FG_DEFAULT, fname);
    }

    str_t data = tail_impl(&scratch, fp, opt);
    print("%v", data);
    os_file_close(fp);
}

void TOY(tail)(int argc, char **argv) {
    tail_opt_t opt = { .line_delim = '\n' };

    tail_parse_opts(argc, argv, &opt);

    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));

    if (opt.follow) {
        arena_t scratch = arena_make(ARENA_VIRTUAL, GB(1));
        file_watcher_t fw = fw_init(&arena, opt.files[0], opt.poll_time);
        oshandle_t fp = os_file_open(opt.files[0], OS_FILE_READ);
        usize old_size = os_file_size(fp);
        str_t data = tail_impl(&arena, fp, &opt);
        os_file_close(fp);
        print("%v", data);

        while (true) {
            arena_rewind(&scratch, 0);
            if (fw_has_changed(scratch, &fw)) {
                fp = os_handle_zero();
                do {
                    fp = os_file_open(opt.files[0], OS_FILE_READ);
                } while (opt.retry && !os_handle_valid(fp) && tail_wait(100));
                if (!os_handle_valid(fp)) {
                    continue;
                }

                usize new_size = os_file_size(fp);
                if (new_size <= old_size) {
                    os_file_close(fp);
                    continue;
                }

                os_file_seek(fp, old_size);
                str_t new_data = common_read_buffered(&scratch, fp);
                os_file_close(fp);

                old_size = new_size;

                print("%v", new_data);
            }
            if (opt.poll_time) {
                // sleep so we don't use 100% cpu, this means
                // it will probably wait too long.
                Sleep((uint)opt.poll_time);
            }
        }
    }

    tail__print_names = opt.files_count > 1;
    glob_t glob_desc = {
        .recursive = true,
        .udata = &opt,
        .cb = tail__glob,
    };

    if (opt.files_count) {
        for (int i = 0; i < opt.files_count; ++i) {
            if (common_is_glob(opt.files[i])) {
                tail__print_names = true;
                glob_desc.exp = opt.files[i];
                common_glob(arena, &glob_desc);
            }
            else {
                tail__glob(arena, opt.files[i], &opt);
            }
        }
    }
    else {
        str_t data = tail_impl(&arena, os_stdin(), &opt);
        print("%v", data);
    }
}
