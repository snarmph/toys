#pragma once

#include "colla/colla.h"
#include <windows.h>
#include <conio.h> // _getch

#if COLLA_TCC
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING  0x0004
#define ENABLE_VIRTUAL_TERMINAL_INPUT       0x0200
typedef struct _REASON_CONTEXT *PREASON_CONTEXT;
extern HANDLE CreateWaitableTimerExW(LPSECURITY_ATTRIBUTES lpTimerAttributes, LPCWSTR lpTimerName, DWORD dwFlags, DWORD dwDesiredAccess);
extern BOOL SetWaitableTimerEx(HANDLE hTimer, const LARGE_INTEGER * lpDueTime, LONG lPeriod, PTIMERAPCROUTINE pfnCompletionRoutine, LPVOID lpArgToCompletionRoutine, PREASON_CONTEXT WakeContext, ULONG TolerableDelay);
#endif

// header ====================================================

#define TERM_RESET            "\x1b[m"
#define TERM_BOLD             "\x1b[1m" 
#define TERM_ITALIC           "\x1b[3m" 
// "\x1b[0m"

#define TERM_FG_RGB(r, g, b)    "\x1b[38;2;"#r";"#g";"#b"m"

#define TERM_FG_DEFAULT       "\x1b[39m"

#define TERM_FG_BLACK         "\x1b[38;5;16m"
#define TERM_FG_RED           "\x1b[31m"
#define TERM_FG_GREEN         "\x1b[32m"
#define TERM_FG_YELLOW        "\x1b[33m"
#define TERM_FG_BLUE          "\x1b[38;5;27m"
#define TERM_FG_MAGENTA       "\x1b[35m"
#define TERM_FG_CYAN          "\x1b[36m"
#define TERM_FG_WHITE         "\x1b[37m"

#define TERM_FG_DARK_GREY     "\x1b[90m"
#define TERM_FG_LIGHT_RED     "\x1b[91m"
#define TERM_FG_LIGHT_GREEN   "\x1b[92m"
#define TERM_FG_LIGHT_YELLOW  "\x1b[93m"
#define TERM_FG_LIGHT_BLUE    "\x1b[94m"
#define TERM_FG_LIGHT_MAGENTA "\x1b[95m"
#define TERM_FG_LIGHT_CYAN    "\x1b[96m"

// term 256 colors

#define TERM_FG_ORANGE        "\x1b[38;5;202m"
#define TERM_FG_LIGHT_ORANGE  "\x1b[38;5;208m"
#define TERM_FG_PURPLE        "\x1b[38;5;93m"
#define TERM_FG_LIGHT_PURPLE  "\x1b[38;5;99m"

// background

#define TERM_BG_DEFAULT       "\x1b[49m"

#define TERM_BG_BLACK         "\x1b[40m"
#define TERM_BG_RED           "\x1b[41m"
#define TERM_BG_GREEN         "\x1b[42m"
#define TERM_BG_YELLOW        "\x1b[43m"
#define TERM_BG_BLUE          "\x1b[44m"
#define TERM_BG_MAGENTA       "\x1b[45m"
#define TERM_BG_CYAN          "\x1b[46m"
#define TERM_BG_WHITE         "\x1b[47m"

#define TERM_BG_DARK_GREY     "\x1b[100m"
#define TERM_BG_LIGHT_RED     "\x1b[101m"
#define TERM_BG_LIGHT_GREEN   "\x1b[102m"
#define TERM_BG_LIGHT_YELLOW  "\x1b[103m"
#define TERM_BG_LIGHT_BLUE    "\x1b[104m"
#define TERM_BG_LIGHT_MAGENTA "\x1b[105m"
#define TERM_BG_LIGHT_CYAN    "\x1b[106m"

// term 256 colors

#define TERM_BG_ORANGE        "\x1b[48;5;202m"
#define TERM_BG_LIGHT_ORANGE  "\x1b[48;5;208m"
#define TERM_BG_PURPLE        "\x1b[48;5;93m"
#define TERM_BG_LIGHT_PURPLE  "\x1b[48;5;99m"

#define TERM_BG_LIGHT_GRAY    "\x1b[48;5;234m"

typedef struct os_time_t os_time_t;
struct os_time_t {
    i32 seconds;
    i32 minutes;
    i32 hours;
    i32 days;
    i32 months;
    i32 years;
};

os_time_t common_convert_time(i64 seconds);
str_t common_print_time(arena_t *arena, os_time_t *time);
str_t common_get_local_folder(arena_t *arena);
ini_t common_get_config(arena_t *arena, str_t *out_conf_fname);
bool common_is_piped(oshandle_t handle);

i64 common_get_tps(void);
i64 common_get_ticks(void);

str_t common_read_buffered(arena_t *arena, oshandle_t fp);

int common_strv_to_int(strview_t v);

bool common_prompt(strview_t question);

typedef void (fd_f)(arena_t scratch, str_t path, void *udata);

typedef struct fd_desc_t fd_desc_t;
struct fd_desc_t {
    fd_f *cb;
    void *udata;
    strview_t path;
    strview_t rg;
    bool add_hidden;
    bool recursive;
};

void fd_search(arena_t scratch, const fd_desc_t *desc);

typedef struct glob_t glob_t;
struct glob_t {
    fd_f *cb;
    void *udata;
    strview_t exp;
    bool add_hidden;
    bool recursive;
};

void common_glob(arena_t scratch, glob_t *desc);

typedef struct ticker_t ticker_t;
struct ticker_t {
    int fps;
    float dt;
    i64 accumulated;
    i64 ticks;
    i64 prev;
    HANDLE timer;

    void (*update)(float dt, void *userdata);
    void *userdata;
};

ticker_t ticker_init(int fps, void (*update)(float dt, void *userdata), void *userdata);
void ticker_tick(ticker_t *ctx);

#define COMMON_TERM_UP_Y(y)           "\x1b["#y"A"
#define COMMON_TERM_DOWN_Y(y)         "\x1b["#y"B"
#define COMMON_TERM_DOWN_X(x)         "\x1b["#x"C"
#define COMMON_TERM_UP_X(x)           "\x1b["#x"D"
#define COMMON_TERM_RESET_POS         "\x1b[H"
#define COMMON_TERM_SET_X(x)          "\x1b["#x"G"
#define COMMON_TERM_SET_Y(y)          COMMON_TERM_RESET_POS "\x1b["#y"B"
#define COMMON_TERM_SET_POS(x, y)     "\x1b["#y";"#x"H"
// #define COMMON_TERM_DOWN_AT_BEG(offy) "\x1b["#offy"E"
#define COMMON_TERM_DOWN_AT_BEG(offy) "\n"
#define COMMON_TERM_UP_AT_BEG(offy)   "\x1b["#offy"F"

// implementation ============================================

os_time_t common_convert_time(i64 seconds) {
    i64 minutes = seconds / 60;
    i64 hours   = minutes / 60;
    i64 days    = hours   / 24;
    i64 months  = days    / 30;
    i64 years   = months  / 12;

    return (os_time_t) {
        .seconds = (i32)(seconds % 60),
        .minutes = (i32)(minutes % 60),
        .hours   = (i32)(hours % 24),
        .days    = (i32)(days % 30),
        .months  = (i32)(months % 12),
        .years   = (i32)(years),
    };
}

str_t common_print_time(arena_t *arena, os_time_t *time) {
    outstream_t out = ostr_init(arena);
    
    if (time->years)   ostr_print(&out, "%lld year%s",   time->years,   time->years > 1   ? "s " : " ");
    if (time->months)  ostr_print(&out, "%lld month%s",  time->months,  time->months > 1  ? "s " : " ");
    if (time->days)    ostr_print(&out, "%lld day%s",    time->days,    time->days > 1    ? "s " : " ");
    if (time->hours)   ostr_print(&out, "%lld hour%s",   time->hours,   time->hours > 1   ? "s " : " ");
    if (time->minutes) ostr_print(&out, "%lld minute%s", time->minutes, time->minutes > 1 ? "s " : " ");
    
    return ostr_to_str(&out);
}

str_t common_get_local_folder(arena_t *arena) {
#if COLLA_DEBUG
    return str(arena, "debug");
#else
    u8 tmpbuf[KB(5)];
    arena_t scratch = arena_make(ARENA_STATIC, sizeof(tmpbuf), tmpbuf);

    TCHAR tpath[512] = {0};

    DWORD len = GetModuleFileName(NULL, tpath, arrlen(tpath));

    str_t path = str_from_tstr(&scratch, tstr_init(tpath, len));
    
    strview_t dir;
    os_file_split_path(strv(path), &dir, NULL, NULL);

    return str(arena, dir);
#endif
}

ini_t common_get_config(arena_t *arena, str_t *out_conf_fname) {
    u8 tmpbuf[KB(5)];
    arena_t scratch = arena_make(ARENA_STATIC, sizeof(tmpbuf), tmpbuf);

    str_t local_dir = common_get_local_folder(&scratch);
    str_t conf_fname = str_fmt(&scratch, "%v/conf.ini", local_dir);
    ini_t conf = ini_parse(arena, strv(conf_fname), NULL);

    if (out_conf_fname) {
        *out_conf_fname = str_dup(arena, conf_fname);
    }

    return conf;
}

bool common_is_piped(oshandle_t handle) {
    return GetFileType((HANDLE)handle.data) != FILE_TYPE_CHAR;
}

str_t common_read_buffered(arena_t *arena, oshandle_t fp) {
    outstream_t out = ostr_init(arena);
    char buffer[KB(10)] = {0};
    while (true) {
        usize read = os_file_read(fp, buffer, sizeof(buffer));
        if (read == 0) {
            break;
        }
        ostr_puts(&out, strv(buffer, read));
    }
    return ostr_to_str(&out);
}

int common_strv_to_int(strview_t v) {
    instream_t in = istr_init(v);
    int out;
    return istr_get_i32(&in, &out) ? out : 0;
}

bool common_prompt(strview_t question) {
    while (true) {
        print("%v? (y/n): ", question);
        int c = _getch();
        println("");
        if (c == 'y') return true;
        if (c == 'n') return false;
        // ctrl+c
        if (c == 3) os_abort(0);
        println("\"%c\" (%d) is not a valid response, retry", c, c);
    }
    return false;
}

void fd__iter_dir(arena_t scratch, strview_t path, const fd_desc_t *desc) {
    dir_t *dir = NULL;
    if (path.len == 0) {
        dir = os_dir_open(&scratch, strv("./"));
    }
    else {
        dir = os_dir_open(&scratch, path);
    }

    dir_foreach(&scratch, entry, dir) {
        strview_t name = strv(entry->name);

        if (strv_equals(name, strv(".")) ||
            strv_equals(name, strv("..")))
        {
            continue;
        }

        str_t fullpath = str_fmt(&scratch, "%v%v/", path, entry->name);
        strview_t fname_only = str_sub(fullpath, 0, fullpath.len - 1);

        if (rg_matches(desc->rg, fname_only, true)) {
            desc->cb(scratch, str(&scratch, fname_only), desc->udata);
        }

        if (entry->type == DIRTYPE_DIR && desc->recursive) {
            if (!desc->add_hidden && entry->name.buf[0] == '.') {
                continue;
            }
            
            fd__iter_dir(scratch, strv(fullpath), desc);
        }
    }
}

void fd_search(arena_t scratch, const fd_desc_t *desc) {
    fd__iter_dir(scratch, desc->path, desc);
}

i64 common_get_tps(void) {
    LARGE_INTEGER tps = {0};
    if (!QueryPerformanceFrequency(&tps)) {
        fatal("failed to query performance frequency");
    }
    return tps.QuadPart;
}

i64 common_get_ticks(void) {
    LARGE_INTEGER ticks = {0};
    if (!QueryPerformanceCounter(&ticks)) {
        fatal("failed to query performance counter");
    }
    return ticks.QuadPart;
}

ticker_t ticker_init(int fps, void (*update)(float dt, void *userdata), void *userdata) {
    i64 tps = common_get_tps();

    float dt = 0;
    i64 ticks = 0;
    if (fps == 0) {
        ticks = 1;
        fps = (int)tps;
    }
    else {
        dt = 1.f / (float)fps;
        ticks = (i64)((float)tps / (float)fps);
    }

    HANDLE timer = NULL;
    if (!timer) {
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
    #define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
        timer = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);    
    }

    return (ticker_t) {
        .fps = fps,
        .dt = dt,
        .ticks = ticks,
        .prev = common_get_ticks(),
        .timer = timer,
        .update = update,
        .userdata = userdata,
    };

}

void ticker_tick(ticker_t *ctx) {
    i64 now = common_get_ticks();
    i64 passed = now - ctx->prev;
    if (passed >= ctx->ticks) {
        ctx->prev = now;

        while (passed >= ctx->ticks) {
            passed -= ctx->ticks;
            ctx->update(ctx->dt, ctx->userdata);
        }

        ctx->accumulated += passed;

        while (ctx->accumulated >= ctx->ticks) {
            ctx->accumulated -= ctx->ticks;
            ctx->update(ctx->dt, ctx->userdata);
        }
    }
    
    LARGE_INTEGER due = {
        .QuadPart = -(i64)(ctx->ticks),
    };
    SetWaitableTimerEx(ctx->timer, &due, 0, NULL, NULL, NULL, 0);
    WaitForSingleObject(ctx->timer, INFINITE);
}

