#include "colla.h"

#include <windows.h>

#if COLLA_TCC
    #include "colla_tcc.h"
#elif !COLLA_NO_NET
    #include <wininet.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>

    #if COLLA_CMT_LIB
        #pragma comment(lib, "Wininet")
        #pragma comment(lib, "Ws2_32")
    #endif
#endif

#ifndef PROCESSOR_ARCHITECTURE_ARM64
#define PROCESSOR_ARCHITECTURE_ARM64            12
#endif

const char win32__fg_colours[LOG_COL__COUNT][6] = {
    [LOG_COL_RESET]         = "\x1b[39m",
    [LOG_COL_BLACK]         = "\x1b[30m",
    [LOG_COL_RED]           = "\x1b[31m",
    [LOG_COL_GREEN]         = "\x1b[32m",
    [LOG_COL_YELLOW]        = "\x1b[33m",
    [LOG_COL_BLUE]          = "\x1b[34m",
    [LOG_COL_MAGENTA]       = "\x1b[35m",
    [LOG_COL_CYAN]          = "\x1b[36m",
    [LOG_COL_WHITE]         = "\x1b[37m",
    [LOG_COL_DARK_GREY]     = "\x1b[90m",
    [LOG_COL_LIGHT_RED]     = "\x1b[91m",
    [LOG_COL_LIGHT_GREEN]   = "\x1b[92m",
    [LOG_COL_LIGHT_YELLOW]  = "\x1b[93m",
    [LOG_COL_LIGHT_BLUE]    = "\x1b[94m",
    [LOG_COL_LIGHT_MAGENTA] = "\x1b[95m",
    [LOG_COL_LIGHT_CYAN]    = "\x1b[96m",
};

const char win32__bg_colours[LOG_COL__COUNT][7] = {
    [LOG_COL_RESET]         = "\x1b[49m",
    [LOG_COL_BLACK]         = "\x1b[40m",
    [LOG_COL_RED]           = "\x1b[41m",
    [LOG_COL_GREEN]         = "\x1b[42m",
    [LOG_COL_YELLOW]        = "\x1b[43m",
    [LOG_COL_BLUE]          = "\x1b[44m",
    [LOG_COL_MAGENTA]       = "\x1b[45m",
    [LOG_COL_CYAN]          = "\x1b[46m",
    [LOG_COL_WHITE]         = "\x1b[47m",
    [LOG_COL_DARK_GREY]     = "\x1b[100m",
    [LOG_COL_LIGHT_RED]     = "\x1b[101m",
    [LOG_COL_LIGHT_GREEN]   = "\x1b[102m",
    [LOG_COL_LIGHT_YELLOW]  = "\x1b[103m",
    [LOG_COL_LIGHT_BLUE]    = "\x1b[104m",
    [LOG_COL_LIGHT_MAGENTA] = "\x1b[105m",
    [LOG_COL_LIGHT_CYAN]    = "\x1b[106m",
};

void os__log_init(void);

str_t str_os_from_str16(arena_t *arena, str16_t src) {
    str_t out = {0};
    
    int outlen = WideCharToMultiByte(
        CP_UTF8, 0,
        src.buf, (int)src.len,
        NULL, 0,
        NULL, NULL
    );

    if (outlen == 0) {
        unsigned long error = GetLastError();
        if (error == ERROR_NO_UNICODE_TRANSLATION) {
            err("couldn't translate wide string (%S) to utf8, no unicode translation", src.buf);
        }
        else {
            err("couldn't translate wide string (%S) to utf8, %v", src.buf, os_get_error_string(os_get_last_error()));
        }

        return STR_EMPTY;
    }

    out.buf = alloc(arena, char, outlen + 1);
    WideCharToMultiByte(
        CP_UTF8, 0,
        src.buf, (int)src.len,
        out.buf, outlen,
        NULL, NULL
    );

    out.len = outlen;

    return out;
}

str16_t strv_os_to_str16(arena_t *arena, strview_t src) {
    str16_t out = {0};

    if (strv_is_empty(src)) {
        return out;
    }

    int len = MultiByteToWideChar(
        CP_UTF8, 0,
        src.buf, (int)src.len,
        NULL, 0
    );

    if (len == 0) {
        unsigned long error = GetLastError();
        if (error == ERROR_NO_UNICODE_TRANSLATION) {
            err("couldn't translate string (%v) to a wide string, no unicode translation", src);
        }
        else {
            err("couldn't translate string (%v) to a wide string, %v", src, os_get_error_string(os_get_last_error()));
        }

        return out;
    }

    out.buf = alloc(arena, wchar_t, len + 1);

    MultiByteToWideChar(
        CP_UTF8, 0,
        src.buf, (int)src.len,
        out.buf, len
    );

    out.len = len;

    return out;
}

typedef enum os_entity_kind_e {
    OS_KIND_NULL,
    OS_KIND_THREAD,
    OS_KIND_MUTEX,
    OS_KIND_CONDITION_VARIABLE,
} os_entity_kind_e;

typedef struct os_entity_t os_entity_t;
struct os_entity_t {
    os_entity_t *next;
    os_entity_kind_e kind;
    union {
        struct {
            HANDLE handle;
            thread_func_t *func;
            void *userdata;
            DWORD id;
        } thread;
        CRITICAL_SECTION mutex;
        CONDITION_VARIABLE cv;
    };
};

struct {
    arena_t arena;
    os_system_info_t info;
    os_entity_t *entity_free;
    oshandle_t hstdout;
    oshandle_t hstdin;
    oshandle_t hconin;
    oshandle_t hconout;
    WORD default_fg;
    WORD default_bg;
} w32_data = {0};

os_entity_t *os__win_alloc_entity(os_entity_kind_e kind) {
    os_entity_t *entity = w32_data.entity_free;
    if (entity) {
        list_pop(w32_data.entity_free);
    }
    else {
        entity = alloc(&w32_data.arena, os_entity_t);
    }
    entity->kind = kind;
    return entity;
}

void os__win_free_entity(os_entity_t *entity) {
    entity->kind = OS_KIND_NULL;
    list_push(w32_data.entity_free, entity);
}

HANDLE os__win_get_handle(oshandle_t handle) {
    os_entity_t *e = (os_entity_t *)handle.data;
    switch (e->kind) {
        case OS_KIND_THREAD:
            return e->thread.handle;
        default:
            return e;
    }
}

void os_init(void) {
    SetConsoleOutputCP(CP_UTF8);

    SYSTEM_INFO sysinfo = {0};
    GetSystemInfo(&sysinfo);

    os_arch_e architectures[] = {
        [PROCESSOR_ARCHITECTURE_INTEL] = OS_ARCH_X86,
        [PROCESSOR_ARCHITECTURE_ARM]   = OS_ARCH_ARM,
        [PROCESSOR_ARCHITECTURE_IA64]  = OS_ARCH_IA64,
        [PROCESSOR_ARCHITECTURE_AMD64] = OS_ARCH_AMD64,
        [PROCESSOR_ARCHITECTURE_ARM64] = OS_ARCH_ARM64,
    };

    os_system_info_t *info = &w32_data.info;
    info->processor_count = (u64)sysinfo.dwNumberOfProcessors;
    info->page_size = sysinfo.dwPageSize;
    info->architecture = architectures[sysinfo.wProcessorArchitecture];

    w32_data.arena = arena_make(ARENA_VIRTUAL, COLLA_OS_ARENA_SIZE);

    TCHAR namebuf[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD namebuflen = sizeof(namebuf);
    BOOL result = GetComputerName(namebuf, &namebuflen);
    if (!result) {
        err("failed to get computer name: %v", os_get_error_string(os_get_last_error()));
    }

    info->machine_name = str_from_tstr(&w32_data.arena, (tstr_t){ namebuf, namebuflen});

    HANDLE hconout = CreateFile(TEXT("CONOUT$"), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    HANDLE hconin  = CreateFile(TEXT("CONIN$"), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    HANDLE hstdout = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hstdin  = GetStdHandle(STD_INPUT_HANDLE);

    if (hstdout == INVALID_HANDLE_VALUE) err("couldn't open STD_OUTPUT_HANDLE");
    else                                 w32_data.hstdout.data = (uptr)hstdout;

    if (hstdin == INVALID_HANDLE_VALUE)  err("couldn't open STD_INPUT_HANDLE");
    else                                 w32_data.hstdin.data = (uptr)hstdin;

    if (hconout == INVALID_HANDLE_VALUE) err("couldn't open CONOUT$");
    else                                 w32_data.hconout.data = (uptr)hconout;

    if (hconin == INVALID_HANDLE_VALUE)  err("couldn't open CONIN$");
    else                                 w32_data.hconin.data = (uptr)hconin;

    CONSOLE_SCREEN_BUFFER_INFO console_info = {0};
    if (GetConsoleScreenBufferInfo(hstdout, &console_info)) {
        w32_data.default_fg = console_info.wAttributes & 0x0F;
        w32_data.default_bg = (console_info.wAttributes & 0xF0) >> 4;
    }
    else {
        err("couldn't get console screen buffer info: %v", os_get_error_string(os_get_last_error()));
    }
    os__log_init();
}

void os_cleanup(void) {
    os_file_close(w32_data.hstdout);
    os_file_close(w32_data.hstdin);

    arena_cleanup(&w32_data.arena);
}

void os_abort(int code) {
#if COLLA_DEBUG
    if (code != 0) {
        __debugbreak();
    }
#endif
    ExitProcess(code);
}

iptr os_get_last_error(void) {
    return (iptr)GetLastError();
}

str_t os_get_error_string(iptr error) {
    static u8 tmpbuf[1024] = {0};
    arena_t arena = arena_make(ARENA_STATIC, sizeof(tmpbuf), tmpbuf);
    DWORD code = LOWORD(error);

    WCHAR msgbuf[512];
    DWORD chars;
    chars = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, code, 0, msgbuf, arrlen(msgbuf), NULL);
    if (chars == 0) {
        iptr fmt_error = os_get_last_error();
        if (fmt_error == ERROR_MR_MID_NOT_FOUND) {
            return str_fmt(&arena, "(unknown error: 0x%04x)", fmt_error);
        }
        err("FormatMessageW: %ld", fmt_error);
        return STR_EMPTY;
    }

    // remove \r\n at the end
    return str_from_str16(&arena, str16_init(msgbuf, chars - 2));
}

os_wait_t os_wait_on_handles(oshandle_t *handles, int count, bool wait_all, u32 milliseconds) {
    HANDLE win_handles[COLLA_OS_MAX_WAITABLE_HANDLES] = {0};
    colla_assert(count < COLLA_OS_MAX_WAITABLE_HANDLES);

    for (int i = 0; i < count; ++i) {
        win_handles[i] = os__win_get_handle(handles[i]);
    }

    DWORD result = WaitForMultipleObjects(count, win_handles, wait_all, milliseconds);

    os_wait_t out = {0};

    if (result == WAIT_FAILED) {
        out.result = OS_WAIT_FAILED;
    }
    else if (result == WAIT_TIMEOUT) {
        out.result = OS_WAIT_TIMEOUT;
    }
    else if (result >= WAIT_ABANDONED_0) {
        out.result = OS_WAIT_ABANDONED;
        out.index = result - WAIT_ABANDONED_0;
    }
    else {
        out.result = OS_WAIT_FINISHED;
        out.index = result - WAIT_OBJECT_0;
    }

    return out;
}

os_system_info_t os_get_system_info(void) {
	return w32_data.info;
}

void os_log_set_colour(os_log_colour_e colour) {
    WriteFile((HANDLE)w32_data.hstdout.data, win32__fg_colours[colour], arrlen(win32__fg_colours[colour]), NULL, NULL);
}

void os_log_set_colour_bg(os_log_colour_e foreground, os_log_colour_e background) {
    WriteFile((HANDLE)w32_data.hstdout.data, win32__fg_colours[foreground], arrlen(win32__fg_colours[foreground]), NULL, NULL);
    WriteFile((HANDLE)w32_data.hstdout.data, win32__bg_colours[background], arrlen(win32__bg_colours[background]), NULL, NULL);
}

oshandle_t os_stdout(void) {
    return w32_data.hstdout;
}

oshandle_t os_stdin(void) {
    return w32_data.hstdin;
}

oshandle_t os_win_conout(void) {
    return w32_data.hconout;
}

oshandle_t os_win_conin(void) {
    return w32_data.hconin;
}

// == FILE ======================================

#define OS_SMALL_SCRATCH() \
u8 tmpbuf[KB(1)]; \
arena_t scratch = arena_make(ARENA_STATIC, sizeof(tmpbuf), tmpbuf) 

DWORD os__win_mode_to_access(filemode_e mode) {
    DWORD out = 0;
    if (mode & OS_FILE_READ)  out |= GENERIC_READ;
    if (mode & OS_FILE_WRITE) out |= GENERIC_WRITE;
    return out;
}

DWORD os__win_mode_to_creation(filemode_e mode) {
    if (mode == OS_FILE_READ)  return OPEN_EXISTING;
    if (mode == OS_FILE_WRITE) return CREATE_ALWAYS;
    return OPEN_ALWAYS;
}

str_t os_path_join(arena_t *arena, strview_t left, strview_t right) {
    if (left.len == 0) {
        return str(arena, right);
    }

    char a = strv_back(left);
    char b = strv_front(right);

    if (a == '/' || a == '\\') {
        left.len--;
    }

    if (b == '/' || b == '\\') {
        right = strv_remove_prefix(right, 1);
    }

    return str_fmt(arena, "%v/%v", left, right);
}

bool os_file_exists(strview_t path) {
    OS_SMALL_SCRATCH();
    tstr_t name = strv_to_tstr(&scratch, path);
    DWORD attributes = GetFileAttributes(name.buf);
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

bool os_dir_exists(strview_t folder) {
    OS_SMALL_SCRATCH();
    tstr_t name = strv_to_tstr(&scratch, folder);
    DWORD attributes = GetFileAttributes(name.buf);
    return attributes != INVALID_FILE_ATTRIBUTES && attributes & FILE_ATTRIBUTE_DIRECTORY;
}

bool os_file_or_dir_exists(strview_t path) {
    OS_SMALL_SCRATCH();
    tstr_t name = strv_to_tstr(&scratch, path);
    DWORD attributes = GetFileAttributes(name.buf);
    return attributes != INVALID_FILE_ATTRIBUTES;
}

bool os_dir_create(strview_t folder) {
    OS_SMALL_SCRATCH();
    tstr_t name = strv_to_tstr(&scratch, folder);
    return CreateDirectory(name.buf, NULL);
}

tstr_t os_file_fullpath(arena_t *arena, strview_t filename) {
    OS_SMALL_SCRATCH();

    TCHAR long_path_prefix[] = TEXT("\\\\?\\");
    const usize prefix_len = arrlen(long_path_prefix) - 1;

    tstr_t rel_path = strv_to_tstr(&scratch, filename);
    DWORD pathlen = GetFullPathName(rel_path.buf, 0, NULL, NULL);

    tstr_t full_path = {
        .buf = alloc(arena, TCHAR, pathlen + prefix_len + 1),
        .len = pathlen + prefix_len,
    };
    memcpy(full_path.buf, long_path_prefix, prefix_len * sizeof(TCHAR));

    GetFullPathName(rel_path.buf, pathlen + 1, full_path.buf + prefix_len, NULL);

    return full_path;
}

bool os_file_delete(strview_t path) {
    OS_SMALL_SCRATCH();
    tstr_t fname = strv_to_tstr(&scratch, path);
    return DeleteFile(fname.buf);
}

bool os_dir_delete(strview_t path) {
    OS_SMALL_SCRATCH();
    tstr_t fname = strv_to_tstr(&scratch, path);
    return RemoveDirectory(fname.buf);
}

oshandle_t os_file_open(strview_t path, filemode_e mode) {
    OS_SMALL_SCRATCH();

    tstr_t full_path = os_file_fullpath(&scratch, path);

    HANDLE handle = CreateFile(
        full_path.buf,
        os__win_mode_to_access(mode),
        FILE_SHARE_READ,
        NULL,
        os__win_mode_to_creation(mode),
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (handle == INVALID_HANDLE_VALUE) {
        handle = NULL;
    }

    return (oshandle_t){
        .data = (uptr)handle
    };
}

void os_file_close(oshandle_t handle) {
    if (!os_handle_valid(handle)) return;
    CloseHandle((HANDLE)handle.data);    
}

usize os_file_read(oshandle_t handle, void *buf, usize len) {
    if (!os_handle_valid(handle)) return 0;
    DWORD read = 0;
    ReadFile((HANDLE)handle.data, buf, (DWORD)len, &read, NULL);
    return (usize)read;
}

usize os_file_write(oshandle_t handle, const void *buf, usize len) {
    if (!os_handle_valid(handle)) return 0;
    DWORD written = 0;
    WriteFile((HANDLE)handle.data, buf, (DWORD)len, &written, NULL);
    return (usize)written;
}

bool os_file_seek(oshandle_t handle, usize offset) {
    if (!os_handle_valid(handle)) return false;
    LARGE_INTEGER offset_large = {
        .QuadPart = offset,
    };
    DWORD result = SetFilePointer((HANDLE)handle.data, offset_large.LowPart, &offset_large.HighPart, FILE_BEGIN);
    return result != INVALID_SET_FILE_POINTER;
}

bool os_file_seek_end(oshandle_t handle) {
    if (!os_handle_valid(handle)) return false;
    DWORD result = SetFilePointer((HANDLE)handle.data, 0, NULL, FILE_END);
    return result != INVALID_SET_FILE_POINTER;
}

void os_file_rewind(oshandle_t handle) {
    if (!os_handle_valid(handle)) return;
    SetFilePointer((HANDLE)handle.data, 0, NULL, FILE_BEGIN);
}

usize os_file_tell(oshandle_t handle) {
    if (!os_handle_valid(handle)) return 0;
    LARGE_INTEGER tell = {0};
    BOOL result = SetFilePointerEx((HANDLE)handle.data, (LARGE_INTEGER){0}, &tell, FILE_CURRENT);
    return result == TRUE ? (usize)tell.QuadPart : 0;
}

usize os_file_size(oshandle_t handle) {
    if (!os_handle_valid(handle)) return 0;
    LARGE_INTEGER size = {0};
    BOOL result = GetFileSizeEx((HANDLE)handle.data, &size);
    return result == TRUE ? (usize)size.QuadPart : 0;
}

bool os_file_is_finished(oshandle_t handle) {
    if (!os_handle_valid(handle)) return 0;

    char tmp = 0;
    DWORD read = 0;
    BOOL success = ReadFile((HANDLE)handle.data, &tmp, sizeof(tmp), &read, NULL);
    bool is_finished = success && read == 0;
    
    if (!is_finished) {
        SetFilePointer((HANDLE)handle.data, -1, NULL, FILE_CURRENT);
    }

    return is_finished;
}

u64 os_file_time_fp(oshandle_t handle) {
    if (!os_handle_valid(handle)) return 0;
    FILETIME time = {0};
    GetFileTime((HANDLE)handle.data, NULL, NULL, &time);
    ULARGE_INTEGER utime = {
        .HighPart = time.dwHighDateTime,
        .LowPart = time.dwLowDateTime,
    };
    return (u64)utime.QuadPart;
}

// == DIR WALKER ================================

typedef struct dir_t {
    WIN32_FIND_DATAW find_data;
    HANDLE handle;
    dir_entry_t cur_entry;
    dir_entry_t next_entry;
} dir_t;

dir_entry_t os__dir_entry_from_find_data(arena_t *arena, WIN32_FIND_DATAW *fd) {
    dir_entry_t out = {0};

    out.name = str_from_str16(arena, str16_init(fd->cFileName, 0));

    if (strv_equals(strv(out.name), strv("cygwin"))) {
        out.type = 0;
    }

    if (fd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        out.type = DIRTYPE_DIR;
    }
    else {
        LARGE_INTEGER filesize = {
            .LowPart  = fd->nFileSizeLow,
            .HighPart = fd->nFileSizeHigh,
        };
        out.file_size = filesize.QuadPart;
    }

    return out;
}

dir_t *os_dir_open(arena_t *arena, strview_t path) {
    arena_t scratch = *arena;
    str16_t winpath = strv_to_str16(&scratch, path);
    DWORD pathlen = GetFullPathNameW(winpath.buf, 0, NULL, NULL);
    
    WCHAR *fullpath = alloc(&scratch, WCHAR, pathlen + 10);

    pathlen = GetFullPathNameW(winpath.buf, pathlen + 1, fullpath, NULL);

    if (fullpath[pathlen] != L'\\' && fullpath[pathlen] != L'/') {
        fullpath[pathlen++] = L'\\';
    }
    fullpath[pathlen++] = L'*';

    WIN32_FIND_DATAW first = {0};
    HANDLE handle = FindFirstFileW(fullpath, &first);

    if (handle == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    dir_t *ctx = alloc(arena, dir_t);
    ctx->handle = handle;
    ctx->find_data = first;

    ctx->next_entry = os__dir_entry_from_find_data(arena, &ctx->find_data);

    return ctx;
}

void os_dir_close(dir_t *dir) {
    FindClose(dir->handle);
    dir->handle = INVALID_HANDLE_VALUE;
}

bool os_dir_is_valid(dir_t *dir) {
    return dir && dir->handle != INVALID_HANDLE_VALUE;
}

dir_entry_t *os_dir_next(arena_t *arena, dir_t *dir) {
    if (!os_dir_is_valid(dir)) {
        return NULL;
    }

    dir->cur_entry = dir->next_entry;

    while (true) {
        dir->next_entry = (dir_entry_t){0};
        if (!FindNextFileW(dir->handle, &dir->find_data)) {
            os_dir_close(dir);
            break;
        }
        // HACK: skip system files/directories
        if (dir->find_data.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) {
            continue;
        }
        dir->next_entry = os__dir_entry_from_find_data(arena, &dir->find_data);
        break;
    }
    
    return &dir->cur_entry;
}

// == PROCESS ===================================

struct os_env_t {
    void *data;
};

void os_set_env_var(arena_t scratch, strview_t key, strview_t value) {
    str16_t k = strv_to_str16(&scratch, key);
    str16_t v = strv_to_str16(&scratch, value);
    SetEnvironmentVariableW(k.buf, v.buf);
}

str_t os_get_env_var(arena_t *arena, strview_t key) {
    u8 tmpbuf[KB(10)];
    arena_t scratch = arena_make(ARENA_STATIC, sizeof(tmpbuf), tmpbuf);

    wchar_t static_buf[1024] = {0};
    wchar_t *buf = static_buf;

    str16_t k = strv_to_str16(&scratch, key);
    DWORD len = GetEnvironmentVariableW(k.buf, static_buf, arrlen(static_buf));

    if (len > arrlen(static_buf)) {
        buf = alloc(&scratch, wchar_t, len);
        len = GetEnvironmentVariableW(k.buf, buf, len);
    }

    return str_from_str16(arena, str16_init(buf, len));
}

os_env_t *os_get_env(arena_t *arena) {
    os_env_t *out = alloc(arena, os_env_t);
    out->data = GetEnvironmentStringsW();
    return out;
}

oshandle_t os_run_cmd_async(arena_t scratch, os_cmd_t *cmd, os_cmd_options_t *options) {
    HANDLE hstdout_read  = NULL;
    HANDLE hstderr_read  = NULL;
    HANDLE hstdin_write  = NULL;
    HANDLE hstdout_write = NULL;
    HANDLE hstderr_write = NULL;
    HANDLE hstdin_read   = NULL;

    SECURITY_ATTRIBUTES sa_attr = {
        .nLength = sizeof(SECURITY_ATTRIBUTES),
        .bInheritHandle = TRUE,
    };

    if (options && options->out) {
        CreatePipe(&hstdout_read, &hstdout_write, &sa_attr, 0);
        options->out->data = (uptr)hstdout_read;
        SetHandleInformation(hstdout_read, HANDLE_FLAG_INHERIT, 0);
    }

    if (options && options->error) {
        CreatePipe(&hstderr_read, &hstderr_write, &sa_attr, 0);
        options->error->data = (uptr)hstderr_read;
        SetHandleInformation(hstderr_read, HANDLE_FLAG_INHERIT, 0);
    }

    if (options && options->in) {
        CreatePipe(&hstdin_read, &hstdin_write, &sa_attr, 0);
        options->in->data = (uptr)hstdin_read;
        SetHandleInformation(hstdin_write, HANDLE_FLAG_INHERIT, 0);
    }

    STARTUPINFOW start_info = {
        .cb = sizeof(STARTUPINFO),
        .hStdError  = hstderr_write  ? hstderr_write  : GetStdHandle(STD_ERROR_HANDLE),
        .hStdOutput = hstdout_write  ? hstdout_write  : GetStdHandle(STD_OUTPUT_HANDLE),
        .hStdInput  = hstdin_read    ? hstdin_read    : GetStdHandle(STD_INPUT_HANDLE),
        .dwFlags = STARTF_USESTDHANDLES,
    };
    
    PROCESS_INFORMATION proc_info = {0};

    outstream_t cmdline = ostr_init(&scratch);

    for_each (cur, cmd->head ? cmd->head : cmd) {
        for (int i = 0; i < cur->count; ++i) {
            strview_t arg = cur->items[i];
            if (strv_contains(arg, ' ')) {
                ostr_print(&cmdline, "\"%v\"", arg);
            }
            else {
                ostr_puts(&cmdline, arg);
            }
            ostr_putc(&cmdline, ' ');
        }
    }

    ostr_pop(&cmdline, 1);
    ostr_putc(&cmdline, '\0');

    str_t cmd_str = ostr_to_str(&cmdline);
    str16_t command = strv_to_str16(&scratch, strv(cmd_str));

    WCHAR* env = (options && options->env) ? options->env->data : NULL;

    BOOL success = CreateProcessW(
        NULL,
        command.buf,
        NULL,
        NULL,
        TRUE,
        0,
        env,
        NULL,
        &start_info, 
        &proc_info
    );

    if (hstdout_write) {
        CloseHandle(hstdout_write);
    }

    if (hstderr_write) {
        CloseHandle(hstderr_write);
    }

    if (hstdin_read) {
        CloseHandle(hstdin_read);
    }

    if (env) {
        FreeEnvironmentStringsW(env);
        options->env->data = NULL;
    }

    if (!success) {
        err("couldn't create process (%v): %v", cmd_str, os_get_error_string(os_get_last_error()));
        return os_handle_zero();
    }

    CloseHandle(proc_info.hThread);

    return (oshandle_t) {
        .data = (uptr)proc_info.hProcess
    };
}

bool os_process_wait(oshandle_t proc, uint time, int *out_exit) {
    if (!os_handle_valid(proc)) {
        err("waiting on invalid handle");
        return false;
    }

    DWORD result = WaitForSingleObject((HANDLE)proc.data, (DWORD)time);

    if (result == WAIT_TIMEOUT) {
        return false;
    }

    if (result != WAIT_OBJECT_0) {
        err("could not wait for proces: %v", os_get_error_string(os_get_last_error()));
        return false;
    }

    DWORD exit_status;
    if (!GetExitCodeProcess((HANDLE)proc.data, &exit_status)) {
        err("could not get exit status from process: %v", os_get_error_string(os_get_last_error()));
        return false;
    }

    CloseHandle((HANDLE)proc.data);

    if (out_exit) {
        *out_exit = exit_status;
    }

    return exit_status == 0;
}

// == MEMORY ====================================

void *os_alloc(usize size) {
    return VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void os_free(void *ptr) {
    VirtualFree(ptr, 0, MEM_RELEASE);
}

void *os_reserve(usize size, usize *out_padded_size) {
    usize alloc_size = os_pad_to_page(size);
    void *ptr = VirtualAlloc(NULL, alloc_size, MEM_RESERVE, PAGE_NOACCESS);
    if (out_padded_size) {
        *out_padded_size = alloc_size;
    }
    return ptr;
}

bool os_commit(void *ptr, usize num_of_pages) {
    usize page_size = os_get_system_info().page_size;
    void *new_ptr = VirtualAlloc(ptr, num_of_pages * page_size, MEM_COMMIT, PAGE_READWRITE);
    return new_ptr != NULL;
}

bool os_release(void *ptr, usize size) {
    COLLA_UNUSED(size);
    return VirtualFree(ptr, 0, MEM_RELEASE);
}

// == THREAD ====================================

thread_local i64 os_thread_id = 0;
i64 os_thread_count = 0;

DWORD WINAPI os__win_thread_entry_point(void *ptr) {
    os_entity_t *entity = (os_entity_t *)ptr;
    thread_func_t *func = entity->thread.func;
    void *userdata = entity->thread.userdata;
    u64 id = entity->thread.id;

    os_thread_id = atomic_inc_i64(&os_thread_count) - 1;

    return func(id, userdata);
}

oshandle_t os_thread_launch(thread_func_t func, void *userdata) {
    os_entity_t *entity = os__win_alloc_entity(OS_KIND_THREAD);

    entity->thread.func = func;
    entity->thread.userdata = userdata;
    entity->thread.handle = CreateThread(NULL, 0, os__win_thread_entry_point, entity, 0, &entity->thread.id);

    return (oshandle_t){ (uptr)entity };
}

bool os_thread_detach(oshandle_t thread) {
    if (!os_handle_valid(thread)) return false;
    os_entity_t *entity = (os_entity_t *)thread.data;
    BOOL result = CloseHandle(entity->thread.handle);
    os__win_free_entity(entity);
    return result;
}

bool os_thread_join(oshandle_t thread, int *code) {
    if (!os_handle_valid(thread)) return false;
    os_entity_t *entity = (os_entity_t *)thread.data;
    DWORD wait_result = WaitForSingleObject(entity->thread.handle, INFINITE);
    if (code) {
        DWORD exit_code = 0;
        GetExitCodeThread(entity->thread.handle, &exit_code);
        *code = exit_code;
    }
    BOOL result = CloseHandle(entity->thread.handle);
    os__win_free_entity(entity);
    return wait_result != WAIT_FAILED && result;
}   

u64 os_thread_get_id(oshandle_t thread) {
    if (!os_handle_valid(thread)) return 0;
    os_entity_t *entity = (os_entity_t *)thread.data;
    return entity->thread.id;
}

// == MUTEX =====================================

oshandle_t os_mutex_create(void) {
    os_entity_t *entity = os__win_alloc_entity(OS_KIND_MUTEX);

    InitializeCriticalSection(&entity->mutex);

    return (oshandle_t){ (uptr)entity };
}

void os_mutex_free(oshandle_t mutex) {
    if (!os_handle_valid(mutex)) return;
    os_entity_t *entity = (os_entity_t *)mutex.data;
    DeleteCriticalSection(&entity->mutex);
    os__win_free_entity(entity);
}

void os_mutex_lock(oshandle_t mutex) {
    if (!os_handle_valid(mutex)) return;
    os_entity_t *entity = (os_entity_t *)mutex.data;
    EnterCriticalSection(&entity->mutex);
}

void os_mutex_unlock(oshandle_t mutex) {
    if (!os_handle_valid(mutex)) return;
    os_entity_t *entity = (os_entity_t *)mutex.data;
    LeaveCriticalSection(&entity->mutex);
}

bool os_mutex_try_lock(oshandle_t mutex) {
    if (!os_handle_valid(mutex)) return false;
    os_entity_t *entity = (os_entity_t *)mutex.data;
    return TryEnterCriticalSection(&entity->mutex);
}

#if !COLLA_NO_CONDITION_VARIABLE

// == CONDITION VARIABLE ========================

oshandle_t os_cond_create(void) {
    os_entity_t *entity = os__win_alloc_entity(OS_KIND_CONDITION_VARIABLE);

    InitializeConditionVariable(&entity->cv);

    return (oshandle_t){ (uptr)entity };
}

void os_cond_free(oshandle_t cond) {
    if (!os_handle_valid(cond)) return;
    os_entity_t *entity = (os_entity_t *)cond.data;
    os__win_free_entity(entity);
}

void os_cond_signal(oshandle_t cond) {
    if (!os_handle_valid(cond)) return;
    os_entity_t *entity = (os_entity_t *)cond.data;
    WakeConditionVariable(&entity->cv);
}

void os_cond_broadcast(oshandle_t cond) {
    if (!os_handle_valid(cond)) return;
    os_entity_t *entity = (os_entity_t *)cond.data;
    WakeAllConditionVariable(&entity->cv);
}

void os_cond_wait(oshandle_t cond, oshandle_t mutex, int milliseconds) {
    if (!os_handle_valid(cond)) return;
    os_entity_t *entity_cv  = (os_entity_t *)cond.data;
    os_entity_t *entity_mtx = (os_entity_t *)mutex.data;
    SleepConditionVariableCS(&entity_cv->cv, &entity_mtx->mutex, milliseconds);
}

#endif

// == ATOMICS ========================================

#if COLLA_TCC
#undef InterlockedExchangeAdd64
#define InterlockedExchangeAdd64(dst, val) *dst += val 
#endif

i64 atomic_set_i64(i64 *dest, i64 val) {
    return InterlockedExchange64(dest, val);
}

i64 atomic_add_i64(i64 *dest, i64 val) {
    return InterlockedExchangeAdd64(dest, val);
}

i64 atomic_and_i64(i64 *dest, i64 val) {
    return InterlockedAnd64(dest, val);
}

i64 atomic_cmp_i64(i64 *dest, i64 val, i64 cmp) {
    return InterlockedCompareExchange64(dest, val, cmp);
}

i64 atomic_inc_i64(i64 *dest) {
    return InterlockedIncrement64(dest);
}

i64 atomic_dec_i64(i64 *dest) {
    return InterlockedDecrement64(dest);
}

i64 atomic_or_i64(i64 *dest, i64 val) {
    return InterlockedOr64(dest, val);
}

i64 atomic_xor_i64(i64 *dest, i64 val) {
    return InterlockedXor64(dest, val);
}



#if !COLLA_NO_NET

struct {
    HINTERNET internet;
} http_win = {0};

void net_init(void) {
    if (http_win.internet) return;
    http_win.internet = InternetOpen(
        TEXT("COLLA_WININET"),
        INTERNET_OPEN_TYPE_PRECONFIG,
        NULL,
        NULL,
        0
    );

    WSADATA wsdata = {0};
    if (WSAStartup(0x0202, &wsdata)) {
        fatal("couldn't startup sockets: %v", os_get_error_string(WSAGetLastError()));
    }
}

void net_cleanup(void) {
    if (!http_win.internet) return;
    InternetCloseHandle(http_win.internet);
    http_win.internet = NULL;
    WSACleanup();
}

iptr net_get_last_error(void) {
    return WSAGetLastError();
}

http_res_t http_request(http_request_desc_t *req) {
    return http_request_cb(req, NULL, NULL);
}

http_res_t http_request_cb(http_request_desc_t *req, http_request_callback_fn callback, void *userdata) {
    HINTERNET connection = NULL;
    HINTERNET request = NULL;
    BOOL result = FALSE;
    bool success = false;
    http_res_t res = {0};
    arena_t arena_before = *req->arena;

    if (!http_win.internet) {
        err("net_init has not been called");
        goto failed;
    }

    http_url_t split = http_split_url(req->url);
    strview_t server = split.host;
    strview_t page = split.uri;

    if (strv_starts_with_view(server, strv("http://"))) {
        server = strv_remove_prefix(server, 7);
    }
    
    if (strv_starts_with_view(server, strv("https://"))) {
        server = strv_remove_prefix(server, 8);
    }

    {
        arena_t scratch = *req->arena;

        if (req->version.major == 0) req->version.major = 1;
        if (req->version.minor == 0) req->version.minor = 1;

        const TCHAR *accepted_types[] = { TEXT("*/*"), NULL };
        const char *method = http_get_method_string(req->request_type);
        str_t http_ver = str_fmt(&scratch, "HTTP/%u.%u", req->version.major, req->version.minor);

        tstr_t tserver = strv_to_tstr(&scratch, server);
        tstr_t tpage = strv_to_tstr(&scratch, page);
        tstr_t tmethod = strv_to_tstr(&scratch, strv(method));
        tstr_t thttp_ver = strv_to_tstr(&scratch, strv(http_ver));

        connection = InternetConnect(
            http_win.internet,
            tserver.buf,
            INTERNET_DEFAULT_HTTPS_PORT,
            NULL,
            NULL,
            INTERNET_SERVICE_HTTP,
            0,
            (DWORD_PTR)NULL // userdata
        );
        if (!connection) {
            err("call to InternetConnect failed: %u", os_get_last_error());
            goto failed;
        }

        request = HttpOpenRequest(
            connection,
            tmethod.buf,
            tpage.buf,
            thttp_ver.buf,
            NULL,
            accepted_types,
            INTERNET_FLAG_SECURE,
            (DWORD_PTR)NULL // userdata
        );
        if (!request) {
            err("call to HttpOpenRequest failed: %v", os_get_error_string(os_get_last_error()));
            goto failed;
        }
    }

    for (int i = 0; i < req->header_count; ++i) {
        http_header_t *h = &req->headers[i];
        arena_t scratch = *req->arena;

        str_t header = str_fmt(&scratch, "%v: %v\r\n", h->key, h->value);
        tstr_t theader = strv_to_tstr(&scratch, strv(header));
        HttpAddRequestHeaders(request, theader.buf, (DWORD)theader.len, 0);
    }

    result = HttpSendRequest(
        request,
        NULL,
        0,
        (void *)req->body.buf,
        (DWORD)req->body.len
    );
    if (!result) {
        iptr error = os_get_last_error();
        if (error == ERROR_INTERNET_NAME_NOT_RESOLVED) {
            err("invalid url: %v", req->url);
        }
        else {
            err("call to HttpSendRequest failed: %lld", error);
            // os_get_error_string(error));
        }
        goto failed;
    }

    u8 smallbuf[KB(5)];
    DWORD bufsize = sizeof(smallbuf);

    u8 *buffer = smallbuf;

    // try and read it into a static buffer
    result = HttpQueryInfo(request, HTTP_QUERY_RAW_HEADERS_CRLF, smallbuf, &bufsize, NULL);
    
    // buffer is not big enough, allocate one with the arena instead
    if (!result && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        buffer = alloc(req->arena, u8, bufsize + 1);
        result = HttpQueryInfo(request, HTTP_QUERY_RAW_HEADERS_CRLF, buffer, &bufsize, NULL);
    }

    if (!result) {
        err("couldn't get headers");
        goto failed;
    }

    tstr_t theaders = { (TCHAR *)buffer, bufsize };
    str_t headers = str_from_tstr(req->arena, theaders);

    res.headers = http_parse_headers(req->arena, strv(headers));
    res.version = req->version;

    DWORD status_code = 0;
    DWORD status_code_len = sizeof(status_code);
    result = HttpQueryInfo(request, HTTP_QUERY_FLAG_NUMBER | HTTP_QUERY_STATUS_CODE, &status_code, &status_code_len, 0);
    if (!result) {
        err("couldn't get status code");
        goto failed;
    }

    res.status_code = status_code;

    outstream_t body = ostr_init(req->arena);

    while (true) {
        DWORD read = 0;
        char read_buffer[4096];
        BOOL read_success = InternetReadFile(
            request, read_buffer, sizeof(read_buffer), &read
        );
        if (!read_success || read == 0) {
            break;
        }
    
        strview_t chunk = strv(read_buffer, read);
        if (callback) {
            callback(res.headers, chunk, userdata);
        }
        ostr_puts(&body, chunk);
    }

    res.body = strv(ostr_to_str(&body));

    success = true;

failed:
    if (request) InternetCloseHandle(request);
    if (connection) InternetCloseHandle(connection);
    if (!success) *req->arena = arena_before;
    return res;
}

// SOCKETS //////////////////////////

SOCKADDR_IN sk__addrin_in(const char *ip, u16 port) {
    SOCKADDR_IN sk_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
    };

    if (!inet_pton(AF_INET, ip, &sk_addr.sin_addr)) {
        err("inet_pton failed: %v", os_get_error_string(net_get_last_error()));
        return (SOCKADDR_IN){0};
    }

    return sk_addr;
}

socket_t sk_open(sktype_e type) {
    int sock_type = 0;

    switch(type) {
        case SOCK_TCP: sock_type = SOCK_STREAM; break;
        case SOCK_UDP: sock_type = SOCK_DGRAM;  break;
        default: fatal("skType not recognized: %d", type); break;
    }

    return socket(AF_INET, sock_type, 0);
}

socket_t sk_open_protocol(const char *protocol) {
    struct protoent *proto = getprotobyname(protocol);
    if(!proto) {
        return INVALID_SOCKET;
    }
    return socket(AF_INET, SOCK_STREAM, proto->p_proto);
}

bool sk_is_valid(socket_t sock) {
    return sock != INVALID_SOCKET;
}

bool sk_close(socket_t sock) {
    return closesocket(sock) != SOCKET_ERROR;
}

bool sk_bind(socket_t sock, const char *ip, u16 port) {
    SOCKADDR_IN sk_addr = sk__addrin_in(ip, port);
    if (sk_addr.sin_family == 0) {
        return false;
    }
    return bind(sock, (SOCKADDR*)&sk_addr, sizeof(sk_addr)) != SOCKET_ERROR;
}

bool sk_listen(socket_t sock, int backlog) {
    return listen(sock, backlog) != SOCKET_ERROR;
}

socket_t sk_accept(socket_t sock) {
    SOCKADDR_IN addr = {0};
    int addr_size = sizeof(addr);
    return accept(sock, (SOCKADDR*)&addr, &addr_size);
}

bool sk_connect(socket_t sock, const char *server, u16 server_port) {
    u8 tmpbuf[1024] = {0};
    arena_t scratch = arena_make(ARENA_STATIC, sizeof(tmpbuf), tmpbuf);

    str16_t wserver = strv_to_str16(&scratch, strv(server));

    ADDRINFOW *addrinfo = NULL;
    int result = GetAddrInfoW(wserver.buf, NULL, NULL, &addrinfo);
    if (result) {
        return false;
    }

    char ip_str[1024] = {0};
    inet_ntop(addrinfo->ai_family, addrinfo->ai_addr, ip_str, sizeof(ip_str));

    SOCKADDR_IN sk_addr = sk__addrin_in(ip_str, server_port);
    if (sk_addr.sin_family == 0) {
        return false;
    }

    return connect(sock, (SOCKADDR*)&sk_addr, sizeof(sk_addr)) != SOCKET_ERROR;
}

int sk_send(socket_t sock, const void *buf, int len) {
    return send(sock, (const char *)buf, len, 0);
}

int sk_recv(socket_t sock, void *buf, int len) {
    return recv(sock, (char *)buf, len, 0);
}

int sk_poll(skpoll_t *to_poll, int num_to_poll, int timeout) {
    return WSAPoll((WSAPOLLFD*)to_poll, (ULONG)num_to_poll, timeout);
}

oshandle_t sk_bind_event(socket_t sock, skevent_e event) {
    if (event == SOCK_EVENT_NONE) {
        return os_handle_zero();
    }

    HANDLE handle = WSACreateEvent();
    if (handle == WSA_INVALID_EVENT) {
        return os_handle_zero();
    }

    int wsa_event = 0;
    if (event & SOCK_EVENT_READ)    wsa_event |= FD_READ;
    if (event & SOCK_EVENT_WRITE)   wsa_event |= FD_WRITE;
    if (event & SOCK_EVENT_ACCEPT)  wsa_event |= FD_ACCEPT;
    if (event & SOCK_EVENT_CONNECT) wsa_event |= FD_CONNECT;
    if (event & SOCK_EVENT_CLOSE)   wsa_event |= FD_CLOSE;

    if (WSAEventSelect(sock, handle, wsa_event) != 0) {
        WSACloseEvent(handle);
        return os_handle_zero();
    }

    return (oshandle_t){ .data = (uptr)handle };
}

void sk_reset_event(oshandle_t handle) {
    if (!os_handle_valid(handle)) {
        warn("invalid handle");
        return;
    }
    WSAResetEvent((HANDLE)handle.data);
}

void sk_destroy_event(oshandle_t handle) {
    if (!os_handle_valid(handle)) return;
    WSACloseEvent((HANDLE)handle.data); 
}

#endif
