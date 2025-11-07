#include "colla/colla.h"
#include "common.h"
#include "toys.h"
#include "tui.h"

TOY_SHORT_DESC(get, "Download a file from the internet.");

#define GET_MAX_URLS 10000

typedef struct {
    bool quiet;
    strview_t input;
    strview_t base;
    i64 thread_count;
    i64 tries; // default: 20
    i64 ms_update;
    strview_t output; 
    strview_t urls[GET_MAX_URLS];
    i64 progress[GET_MAX_URLS]; // in range 1 - 101, 200 means finished and printed, we use i64 for atomic operations
    i64 url_count;
    bool in_piped;
    bool out_piped;

    os_barrier_t barrier;
} get_opt_t;

void get_parse_opts(int argc, char **argv, get_opt_t *opt) {
    opt->in_piped = common_is_piped(os_stdin());
    opt->out_piped = common_is_piped(os_stdout());

    usage_helper(
        "wget [options] [URL]...", 
        "Download a file/s from the internet.", 
        opt->in_piped ? USAGE_ALLOW_NO_ARGS : USAGE_DEFAULT, 
        USAGE_EXTRA_PARAMS(opt->urls, opt->url_count), 
        argc, argv,
        {
            'q', "quiet",
            "Turn off output.",
            USAGE_BOOL(opt->quiet),
        },
        {
            'j', "thread-count",
            "Download using {} threads, default is 1.",
            "count",
            USAGE_INT(opt->thread_count)
        },
        {
            'i', "input-file",
            "Read URLs from a {}.",
            "file",
            USAGE_VALUE(opt->input)
        },
        {
            'B', "base",
            "Resolves relative links using {} as the point of reference. "
            "For instance, if you specify http://example.com for {}, "
            "and get reads /file.zip from the input file, it would be "
            "resolved to http://example.com/file.zip",
            "url",
            USAGE_VALUE(opt->base)
        },
        {
            't', "tries",
            "Set number of {} to number. Specify 0 for infinite."
            "{}. The default is to try 20 times",
            "tries",
            USAGE_INT(opt->tries),
        },
        {
            'o', "output",
            "The files will not be written to the appropriate files, but "
            "all will be concatenated together and written to {}, If - is "
            "used as {}, documents will be printed to stdout (Use ./- to "
            "print to a file literally named -.)",
            "file",
            USAGE_VALUE(opt->output),
        },
        {
            'u', "update",
            "Amount of {} between tui updates, in milliseconds. Default is 100",
            "time",
            USAGE_INT(opt->ms_update),
        },
    );

    if (!opt->tries) {
        opt->tries = 20;
    }
    if (!opt->thread_count) {
        opt->thread_count = 1;
    }
    if (!opt->ms_update) {
        opt->ms_update = 100;
    }
}

typedef struct get_info_t get_info_t;
struct get_info_t {
    i64 index;
    strview_t url;
    strview_t outfile;
    get_opt_t *opt;
    i64 file_size;
    i64 received;
};

void get_http_cb(http_header_t *headers, strview_t chunk, void *udata) {
    get_info_t *info = udata;
    if (info->file_size == 0) {
        for_each (h, headers) {
            if (strv_equals(h->key, strv("Content-Length"))) {
                info->file_size = common_strv_to_int(h->value);
                break;
            }
        }
    }
    info->received += chunk.len;

    i64 progress = (i64)(((double)info->received / (double)info->file_size) * 100.0);
    atomic_set_i64(&info->opt->progress[info->index], progress);
}

bool get_download_file(arena_t scratch, get_info_t *info) {
    http_res_t res = {0};
    get_opt_t *opt = info->opt;
    for (
        int i = 0; 
        res.status_code != 200 && (opt->tries == 0 || i < opt->tries);
        ++i
    ) {
        res = http_request_cb(
            &(http_request_desc_t){
                .arena = &scratch,
                .url = info->url,
                .request_type = HTTP_GET,
                .version = {1, 1},
            }, 
            get_http_cb, 
            info
        );
    }

    if (res.status_code == 200) {
        os_file_write_all_str(info->outfile, res.body);
        return true;
    }
    return false;
}

void get_entry_point(get_opt_t *opt) {
    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));

    i64range_t range = os_lane_range(opt->url_count);

    for (i64 i = range.min; i < range.max; ++i) {
        arena_t scratch = arena;
        strview_t url = opt->urls[i];
        strview_t name, ext;
        os_file_split_path(url, NULL, &name, &ext);

        str_t outname = str_fmt(&scratch, "%v%v", name, ext);

        get_info_t info = {
            .index   = i,
            .outfile = strv(outname),
            .opt     = opt,
            .url     = url,
        };

        if (!get_download_file(scratch, &info)) {
            opt->progress[i] = -1;
        }
        else {
            opt->progress[i] = 101;
        }
    }
}

int get_thread_entry(u64 id, void *udata) {
    COLLA_UNUSED(id);
    get_opt_t *opt = udata;
    os_barrier_sync(&opt->barrier);
    get_entry_point(opt);
    return 0;
}

void TOY(get)(int argc, char **argv) {
    get_opt_t opt = {0};
    get_parse_opts(argc, argv, &opt);

    net_init();

    if (!opt.quiet) {
        os_log_set_options(OS_LOG_NOFILE);
        HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD console_mode = 0;
        if (!GetConsoleMode(handle, &console_mode)) {
            fatal("couldn't get console mode: %v", os_get_error_string(os_get_last_error()));
        }

        console_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(handle, console_mode);
    }

    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));

    oshandle_t *threads = alloc(&arena, oshandle_t, opt.thread_count);
    opt.barrier.thread_count = opt.thread_count;

    for (i64 i = 0; i < opt.thread_count; ++i) {
        threads[i] = os_thread_launch(get_thread_entry, &opt);
    }

#define TERM_VT(...) print("\x1b" __VA_ARGS__)

#define TERM_SAVE_POS TERM_VT("7")
#define TERM_LOAD_POS TERM_VT("8")
#define TERM_HIDE_CURSOR TERM_VT("[?25l")

    strview_t finish = strv("finished");
    strview_t queue  = strv("waiting");
    strview_t fail   = strv("failed");

    TERM_HIDE_CURSOR;
    TERM_SAVE_POS;
    while (!opt.quiet) {
        int tw = tui_width();

        // term save position
        for (int i = 0; i < opt.url_count; ++i) {
            i64 prog = opt.progress[i];
            strview_t url = opt.urls[i];

            int spaces = tw - (int)url.len;
            print("%v", url);

            // error
            if (prog < 0) {
                spaces -= (int)fail.len;
                print("%*s" TERM_FG_RED "%v" TERM_RESET, spaces, "", fail);
            }
            // waiting
            else if (prog == 0) {
                spaces -= (int)queue.len;
                print("%*s" TERM_FG_YELLOW "%v" TERM_RESET, spaces, "", queue);
            }
            // finished
            else if (prog == 101) {
                spaces -= (int)finish.len;
                print("%*s" TERM_FG_GREEN "%v" TERM_RESET, spaces, "", finish);
            }
            // loading
            else {
                // - (space + brackets)
                int rem = tw - (int)url.len - 3;
                i64 done = (prog-1) * rem / 100;
                print(" [");
                for (i64 k = 0; k < done; ++k) {
                    print("#");
                }
                for (i64 k = done; k < rem; ++k) {
                    print("-");
                }
                print("]");
            }

            print("\n");
        }

        print("\x1b[%dA", opt.url_count);

        os_wait_t res = os_wait_on_handles(threads, (int)opt.thread_count, true, (int)opt.ms_update);
        if (res.result != OS_WAIT_TIMEOUT) {
            if (res.result != OS_WAIT_FINISHED) {
                err("couldn't wait on handles: %v", os_get_error_string(os_get_last_error()));
            }
            break;
        }
    }

    for (i64 i = 0; i < opt.thread_count; ++i) {
        os_thread_join(threads[i], NULL);
    }
}
