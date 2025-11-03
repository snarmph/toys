#include "colla.h"

#pragma clang diagnostic ignored "-Winitializer-overrides"
#pragma clang diagnostic ignored "-Wswitch"
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"

#include <errno.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uchar.h>
#include <wchar.h>

#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#if !COLLA_NO_NET
    #include <arpa/inet.h>
    #include <curl/curl.h>
    #include <netdb.h>

    #define htonll(x) htobe64(x)
    #define ntohll(x) be64toh(x)
#endif

strview_t os__fg_colours[LOG_COL__COUNT] = {
    [LOG_COL_BLACK]         = cstrv("\x1b[30m"),
    [LOG_COL_BLUE]          = cstrv("\x1b[34m"),
    [LOG_COL_GREEN]         = cstrv("\x1b[32m"),
    [LOG_COL_CYAN]          = cstrv("\x1b[36m"),
    [LOG_COL_RED]           = cstrv("\x1b[31m"),
    [LOG_COL_MAGENTA]       = cstrv("\x1b[35m"),
    [LOG_COL_YELLOW]        = cstrv("\x1b[33m"),
    [LOG_COL_GREY]          = cstrv("\x1b[37m"),
    [LOG_COL_DARK_GREY]     = cstrv("\x1b[90m"),
    [LOG_COL_LIGHT_BLUE]    = cstrv("\x1b[94m"),
    [LOG_COL_LIGHT_GREEN]   = cstrv("\x1b[92m"),
    [LOG_COL_LIGHT_CYAN]    = cstrv("\x1b[96m"),
    [LOG_COL_LIGHT_RED]     = cstrv("\x1b[91m"),
    [LOG_COL_LIGHT_MAGENTA] = cstrv("\x1b[95m"),
    [LOG_COL_LIGHT_YELLOW]  = cstrv("\x1b[93m"),
    [LOG_COL_WHITE]         = cstrv("\x1b[97m"),
    [LOG_COL_RESET]         = cstrv("\x1b[39m"),
};

strview_t os__bg_colours[LOG_COL__COUNT] = {
    [LOG_COL_BLACK]         = cstrv("\x1b[40m"),
    [LOG_COL_BLUE]          = cstrv("\x1b[44m"),
    [LOG_COL_GREEN]         = cstrv("\x1b[42m"),
    [LOG_COL_CYAN]          = cstrv("\x1b[46m"),
    [LOG_COL_RED]           = cstrv("\x1b[41m"),
    [LOG_COL_MAGENTA]       = cstrv("\x1b[45m"),
    [LOG_COL_YELLOW]        = cstrv("\x1b[43m"),
    [LOG_COL_GREY]          = cstrv("\x1b[47m"),
    [LOG_COL_DARK_GREY]     = cstrv("\x1b[100m"),
    [LOG_COL_LIGHT_BLUE]    = cstrv("\x1b[104m"),
    [LOG_COL_LIGHT_GREEN]   = cstrv("\x1b[102m"),
    [LOG_COL_LIGHT_CYAN]    = cstrv("\x1b[106m"),
    [LOG_COL_LIGHT_RED]     = cstrv("\x1b[101m"),
    [LOG_COL_LIGHT_MAGENTA] = cstrv("\x1b[105m"),
    [LOG_COL_LIGHT_YELLOW]  = cstrv("\x1b[103m"),
    [LOG_COL_WHITE]         = cstrv("\x1b[107m"),
    [LOG_COL_RESET]         = cstrv("\x1b[49m"),
};

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
            pthread_t handle;
            thread_func_t *func;
            void *userdata;
        } thread;
        pthread_mutex_t mtx;
        pthread_cond_t cv;
    };
};

struct {
    arena_t arena;
    os_system_info_t info;

    os_entity_t *entity_free;
} lin_data = {0};

os_entity_t *os__lin_alloc_entity(os_entity_kind_e kind) {
    os_entity_t *entity = lin_data.entity_free;
    if (entity) {
        list_pop(lin_data.entity_free);
        memset(entity, 0, sizeof(os_entity_t));
    }
    else {
        entity = alloc(&lin_data.arena, os_entity_t);
    }
    entity->kind = kind;
    return entity;
}

void os__lin_free_entity(os_entity_t *entity) {
    entity->kind = OS_KIND_NULL;
    list_push(lin_data.entity_free, entity);
}

os_entity_t *os__handle_to_entity(oshandle_t handle, os_entity_kind_e expected_kind) {
    if (!os_handle_valid(handle)) return NULL;
    os_entity_t *entity = (os_entity_t *)handle.data;
    if (entity->kind != expected_kind) return NULL;
    return entity;
}

void os_init(void) {
    lin_data.info.page_size = sysconf(_SC_PAGESIZE);
    lin_data.info.processor_count = sysconf(_SC_NPROCESSORS_ONLN);

    lin_data.arena = arena_make(ARENA_VIRTUAL, MB(5));

    struct utsname name = {0};
    uname(&name);
    lin_data.info.machine_name = str(&lin_data.arena, name.nodename);
}

void os_cleanup(void) {
    arena_cleanup(&lin_data.arena);
}

void os_abort(int code) {
#if COLLA_DEBUG
    if (code == 1) {
        abort();
    }
#endif
    exit(code);
}

iptr os_get_last_error(void) {
    return (iptr)errno;
}

str_t os_get_error_string(iptr error) {
    char *error_string = strerror(error);
    return (str_t){ error_string, strlen(error_string) };
}

os_wait_t os_wait_on_handles(oshandle_t *handles, int count, bool wait_all, u32 milliseconds) {
    // TODO
    return (os_wait_t){0};
}

os_system_info_t os_get_system_info(void) {
    return lin_data.info;
}

void os_log_set_colour(os_log_colour_e colour) {
    strview_t view = os__fg_colours[colour];
    os_file_write(os_stdout(), view.buf, view.len);
}

void os_log_set_colour_bg(os_log_colour_e foreground, os_log_colour_e background) {
    strview_t fg = os__fg_colours[foreground];
    strview_t bg = os__bg_colours[background];
    os_file_write(os_stdout(), fg.buf, fg.len);
    os_file_write(os_stdout(), bg.buf, bg.len);
}

oshandle_t os_stdout(void) {
    return (oshandle_t){ (uptr)(stdout) };
}

oshandle_t os_stdin(void) {
    return (oshandle_t){ (uptr)(stdin)};
}

// == FILE ======================================

#define OS_SMALL_SCRATCH() \
u8 tmpbuf[KB(1)]; \
arena_t scratch = arena_make(ARENA_STATIC, sizeof(tmpbuf), tmpbuf) 

const char *os__mode_to_str(filemode_e mode) {
    switch ((u32)mode) {
        case OS_FILE_READ:  return "r";
        case OS_FILE_WRITE: return "w";
        case OS_FILE_READ | OS_FILE_WRITE: return "r+";
    }

    return "r";
}

bool os_file_exists(strview_t path) {
    struct stat st = {0};
    if (stat(path.buf, &st) == 0) {
        return st.st_mode & S_IFREG;
    }
    return false;
}

bool os_dir_exists(strview_t folder) {
    struct stat st = {0};
    if (stat(folder.buf, &st) == 0) {
        return st.st_mode & S_IFDIR;
    }
    return false;
}

bool os_file_or_dir_exists(strview_t path) {
    struct stat st = {0};
    if (stat(path.buf, &st) == 0) {
        return st.st_mode & (S_IFDIR | S_IFREG);
    }
    return false;
}

bool os_dir_create(strview_t folder) {
    return mkdir(folder.buf, 0777) == 0;
}

tstr_t os_file_fullpath(arena_t *arena, strview_t filename) {
    OS_SMALL_SCRATCH();

    str_t fname = str(&scratch, filename);

    char fullpath[PATH_MAX] = {0};
    if (!realpath(fname.buf, fullpath)) {
        return (tstr_t){0};
    }
    return strv_to_tstr(arena, strv(fullpath));
}

bool os_file_delete(strview_t path) {
    OS_SMALL_SCRATCH();
    str_t fname = str(&scratch, path);
    return unlink(fname.buf) == 0;
}

bool os_dir_delete(strview_t path) {
    OS_SMALL_SCRATCH();
    str_t folder = str(&scratch, path);
    return rmdir(folder.buf) == 0;
}

oshandle_t os_file_open(strview_t path, filemode_e mode) {
    FILE *fp = fopen(path.buf, os__mode_to_str(mode));
    return (oshandle_t){ (uptr)fp };
}

void os_file_close(oshandle_t handle) {
    if (!os_handle_valid(handle)) return;
    int fd = fileno((FILE*)handle.data);
    int res = fsync(fd);
    warn("res: %d", res);
    fclose((FILE*)handle.data);
    close(fd);
}

usize os_file_read(oshandle_t handle, void *buf, usize len) {
    if (!os_handle_valid(handle)) return 0;
    return fread(buf, 1, len, (FILE*)handle.data);
}

usize os_file_write(oshandle_t handle, const void *buf, usize len) {
    if (!os_handle_valid(handle)) return 0;
    return fwrite(buf, 1, len, (FILE*)handle.data);
}

bool os_file_seek(oshandle_t handle, usize offset) {
    if (!os_handle_valid(handle)) return false;
    int res = fseeko((FILE*)handle.data, offset, SEEK_SET);
    return res == 0;
}

bool os_file_seek_end(oshandle_t handle) {
    if (!os_handle_valid(handle)) return 0;
    int res = fseek((FILE*)handle.data, 0, SEEK_END);
    return res == 0;
}

void os_file_rewind(oshandle_t handle) {
    if (!os_handle_valid(handle)) return;
    fseek((FILE*)handle.data, 0, SEEK_SET);
}

usize os_file_tell(oshandle_t handle) {
    if (!os_handle_valid(handle)) return 0;
    off_t res = ftello((FILE*)handle.data);
    info("> %zu", sizeof(off_t));
    return res != (off_t)-1 ? res : 0;
}

usize os_file_size(oshandle_t handle) {
    if (!os_handle_valid(handle)) return 0;
    struct stat st = {0};
    int fd = fileno((FILE*)handle.data);
    if (fstat(fd, &st) == 0) {
        return st.st_size;
    }
    return 0;
}

bool os_file_is_finished(oshandle_t handle) {
    if (!os_handle_valid(handle)) return true;
    char c = '\0';
    return fread(&c, 1, 1, (FILE*)handle.data) == 0;
}

u64 os_file_time_fp(oshandle_t handle) {
    if (!os_handle_valid(handle)) return 0;
    struct stat st = {0};
    int fd = fileno((FILE*)handle.data);
    if (fstat(fd, &st) == 0) {
        return st.st_mtim.tv_nsec;
    }
    return 0;
}

// == DIR WALKER ================================

struct dir_t {
    DIR *ctx;
    dir_entry_t next;
};

dir_t *os_dir_open(arena_t *arena, strview_t path) {
    arena_t scratch = *arena;
    str_t folder = str(&scratch, path);
    
    DIR *ctx = opendir(folder.buf);
    if (!ctx) {
        return NULL;
    }

    dir_t *dir = alloc(arena, dir_t);
    dir->ctx = ctx;

    return dir;
}

void os_dir_close(dir_t *dir) {
    if (!dir || !dir->ctx) return;
    closedir(dir->ctx);
    dir->ctx = NULL;
}

bool os_dir_is_valid(dir_t *dir) {
    return dir && dir->ctx;
}

dir_entry_t *os_dir_next(arena_t *arena, dir_t *dir) {
    struct dirent *data = readdir(dir->ctx);
    if (!data) {
        os_dir_close(dir);
        return NULL;
    }

    struct stat st = {0};
    stat(data->d_name, &st);

    dir->next.name = (str_t){ data->d_name, strlen(data->d_name) };
    dir->next.type = S_ISDIR(st.st_mode) ? DIRTYPE_DIR : DIRTYPE_FILE;
    dir->next.file_size = st.st_size;

    return &dir->next;
}

// == PROCESS ===================================

void os_set_env_var(arena_t scratch, strview_t key, strview_t value) {
    str_t var = str_fmt(&scratch, "%v=%v");
    putenv(var.buf);
}

str_t os_get_env_var(arena_t *arena, strview_t key) {
    arena_t scratch = *arena;
    str_t key_str = str(&scratch, key);
    const char *val = getenv(key_str.buf);
    return str(arena, val);
}

os_env_t *os_get_env(arena_t *arena) {
    // TODO
    return NULL;
}

oshandle_t os_run_cmd_async(arena_t scratch, os_cmd_t *cmd, os_cmd_options_t *options) {
    // TODO
    return os_handle_zero();
}

bool os_process_wait(oshandle_t proc, uint time, int *out_exit) {
    // TODO
    return false;
}

// == MEMORY ====================================

void *os_alloc(usize size) {
    return calloc(1, size);
}

void os_free(void *ptr) {
    free(ptr);
}

void *os_reserve(usize size, usize *out_padded_size) {
    usize alloc_size = os_pad_to_page(size);

    void *ptr = mmap(
        NULL, 
        alloc_size, 
        PROT_NONE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );

    if (out_padded_size) {
        *out_padded_size = alloc_size;
    }

    return ptr;
}

bool os_commit(void *ptr, usize num_of_pages) {
    usize size = lin_data.info.page_size * num_of_pages;
    int res = mprotect(ptr, size, PROT_READ | PROT_WRITE);
    if (res == -1) {
        err("os_commit error: %s", strerror(errno));
    }
    return res != -1;
}

bool os_release(void *ptr, usize size) {
    if (!ptr) return false;

    int res = munmap(ptr, size);

    return res != -1;
}

// == THREAD ====================================

void *os__lin_thread_entry_point(void *ptr) {
    os_entity_t *entity = (os_entity_t *)ptr;
    colla_assert(entity);
    thread_func_t *func = entity->thread.func;
    void *userdata = entity->thread.userdata;
    int result = func(entity->thread.handle, userdata);
    return (void*)((iptr)result);
}

oshandle_t os_thread_launch(thread_func_t func, void *userdata) {
    os_entity_t *entity = os__lin_alloc_entity(OS_KIND_THREAD);

    entity->thread.func = func;
    entity->thread.userdata = userdata;

    int result = pthread_create(&entity->thread.handle, NULL, os__lin_thread_entry_point, entity);

    if (result) {
        os__lin_free_entity(entity);
        return os_handle_zero();
    }

    return (oshandle_t){ (uptr)entity };
}

bool os_thread_detach(oshandle_t thread) {
    os_entity_t *entity = os__handle_to_entity(thread, OS_KIND_THREAD);
    if (!entity) return false;

    bool result = pthread_detach(entity->thread.handle) == 0;
    os__lin_free_entity(entity);

    return result;
}

bool os_thread_join(oshandle_t thread, int *code) {
    os_entity_t *entity = os__handle_to_entity(thread, OS_KIND_THREAD);
    if (!entity) return false;

    void *outcode = NULL;
    bool success = pthread_join(entity->thread.handle, &outcode) == 0;

    if (success && code) {
        *code = (int)((iptr)outcode);
    }

    os__lin_free_entity(entity);

    return success;
}   

u64 os_thread_get_id(oshandle_t thread) {
    os_entity_t *entity = os__handle_to_entity(thread, OS_KIND_THREAD);
    if (!entity) return 0;

    return entity->thread.handle;
}

// == MUTEX =====================================

oshandle_t os_mutex_create(void) {
    os_entity_t *entity = os__lin_alloc_entity(OS_KIND_MUTEX);
    
    if (pthread_mutex_init(&entity->mtx, NULL)) {
        os__lin_free_entity(entity);
        return os_handle_zero();
    }

    return (oshandle_t){ (uptr)entity };
}

void os_mutex_free(oshandle_t mutex) {
    os_entity_t *entity = os__handle_to_entity(mutex, OS_KIND_MUTEX);
    if (!entity) return;

    pthread_mutex_destroy(&entity->mtx);
    os__lin_free_entity(entity);
}

void os_mutex_lock(oshandle_t mutex) {
    os_entity_t *entity = os__handle_to_entity(mutex, OS_KIND_MUTEX);
    if (!entity) return;

    pthread_mutex_lock(&entity->mtx);
}

void os_mutex_unlock(oshandle_t mutex) {
    os_entity_t *entity = os__handle_to_entity(mutex, OS_KIND_MUTEX);
    if (!entity) return;

    pthread_mutex_unlock(&entity->mtx);
}

bool os_mutex_try_lock(oshandle_t mutex) {
    os_entity_t *entity = os__handle_to_entity(mutex, OS_KIND_MUTEX);
    if (!entity) return false;

    return pthread_mutex_trylock(&entity->mtx) == 0;
}

#if !COLLA_NO_CONDITION_VARIABLE

// == CONDITION VARIABLE ========================

oshandle_t os_cond_create(void) {
    os_entity_t *entity = os__lin_alloc_entity(OS_KIND_CONDITION_VARIABLE);

    if (pthread_cond_init(&entity->cv, NULL)) {
        os__lin_free_entity(entity);
        return os_handle_zero();
    }

    return (oshandle_t){ (uptr)entity };
}

void os_cond_free(oshandle_t cond) {
    os_entity_t *entity = os__handle_to_entity(cond, OS_KIND_CONDITION_VARIABLE);
    if (!entity) return;

    pthread_cond_destroy(&entity->cv);
    os__lin_free_entity(entity);
}

void os_cond_signal(oshandle_t cond) {
    os_entity_t *entity = os__handle_to_entity(cond, OS_KIND_CONDITION_VARIABLE);
    if (!entity) return;

    pthread_cond_signal(&entity->cv);
}

void os_cond_broadcast(oshandle_t cond) {
    os_entity_t *entity = os__handle_to_entity(cond, OS_KIND_CONDITION_VARIABLE);
    if (!entity) return;

    pthread_cond_broadcast(&entity->cv);
}

void os_cond_wait(oshandle_t cond, oshandle_t mutex, int milliseconds) {
    os_entity_t *cond_entity  = os__handle_to_entity(cond, OS_KIND_CONDITION_VARIABLE);
    os_entity_t *mutex_entity = os__handle_to_entity(cond, OS_KIND_MUTEX);
    if (!cond_entity)  return;
    if (!mutex_entity) return;

    pthread_cond_wait(&cond_entity->cv, &mutex_entity->mtx);
}

#endif

str_t str_os_from_str16(arena_t *arena, str16_t src) {
    mbstate_t state = {0};

    usize maxlen = src.len * 4;
    char *buf = alloc(arena, char, maxlen);
    usize len = 0;

    for (usize i = 0; i < src.len; ++i) {
        usize to_add= c16rtomb(&buf[len], src.buf[i], &state);
        if (to_add == (usize)-1) {
            return STR_EMPTY;
        }

        len += to_add;

        if (len >= maxlen) {
            return STR_EMPTY;
        }
    }

    if (len > maxlen) {
        usize extra = len - maxlen;
        arena_pop(arena, extra - 1);
    }

    return (str_t){ buf, len };
}

str16_t strv_os_to_str16(arena_t *arena, strview_t src) {
    mbstate_t state = {0};

    usize maxlen = src.len;
    u16 *buf = alloc(arena, u16, maxlen);

    const char *in = src.buf;
    const char *end = src.buf + src.len;
    u16 *out = buf;

    for (usize rc; (rc = mbrtoc16(out, in, end - in, &state));) {
        if (rc == (usize)-1) {
            break;
        }
        else if(rc == (usize)-2) {
            break;
        }
        else if (rc == (usize)-3) {
            out += 1;
        }
        else {
            in += rc;
            out += 1;
        }
    }

    usize len = out - buf;

    if (len > maxlen) {
        usize extra = len - maxlen;
        arena_pop(arena, extra - 1);
    }

    return (str16_t){ buf, len };
}

#if !COLLA_NO_NET

// NETWORKING ///////////////////////////////////

struct {
    CURLcode last_error;
} net_lin = {0};

typedef struct lin_net_data_t lin_net_data_t;
struct lin_net_data_t {
    outstream_t ostr;
    http_request_callback_fn cb;
    void *udata;
};

void net_init(void) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

void net_cleanup(void) {
    curl_global_cleanup();
}

iptr net_get_last_error(void) {
    return net_lin.last_error;
}

size_t lin__write_callback(
    char *ptr, 
    size_t _, 
    size_t count, 
    void *userdata
) {
    COLLA_UNUSED(_);
    lin_net_data_t *data = userdata;
    strview_t chunk = strv(ptr, count);
    ostr_puts(&data->ostr, chunk);
    
    if (data->cb) {
        data->cb(chunk, data->udata);
    }

    return count;
}

http_res_t http_request_cb(
    http_request_desc_t *req, 
    http_request_callback_fn cb, 
    void *udata
) {
    CURL *curl = curl_easy_init();
    
    u8 tmpbuf[KB(5)];
    arena_t scratch = arena_make(ARENA_STATIC, sizeof(tmpbuf), tmpbuf);

    str_t url = str(&scratch, req->url);
    struct curl_slist *headers = NULL;

    if (req->header_count) {
        for (int i = 0; i < req->header_count; ++i) {
            http_header_t *h = &req->headers[i];

            struct curl_slist *header = alloc(&scratch, struct curl_slist);
            header->data = str_fmt(&scratch, "%v: %v", h->key, h->value).buf;
            list_push(headers, header);
        }
    }
    else {
        for_each(h, req->headers) {
            struct curl_slist *header = alloc(&scratch, struct curl_slist);
            header->data = str_fmt(&scratch, "%v: %v", h->key, h->value).buf;
            list_push(headers, header);
        }
    }

    const char *method = http_get_method_string(req->request_type);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
    
    if (!strv_is_empty(req->body)) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, req->body.len);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req->body.buf);
    }

    lin_net_data_t data = {
        .ostr = ostr_init(req->arena),
        .cb = cb,
        .udata = udata,
    };
    char *header_string = NULL;

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, lin__write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_string);

    char* url_out = NULL;
    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &url_out);

    net_lin.last_error = curl_easy_setopt(
        curl,
        CURLOPT_URL,
        url.buf
    );

    curl_easy_cleanup(curl);
    return (http_res_t){0};
}

// SOCKETS //////////////////////////

struct sockaddr_in sk__addrin_in(const char *ip, u16 port) {
    struct sockaddr_in sk_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
    };

    if (!inet_pton(AF_INET, ip, &sk_addr.sin_addr)) {
        err("inet_pton failed: %v", os_get_error_string(net_get_last_error()));
        return (struct sockaddr_in){0};
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
        return -1;
    }
    return socket(AF_INET, SOCK_STREAM, proto->p_proto);
}

bool sk_is_valid(socket_t sock) {
    return sock != -1;
}

bool sk_close(socket_t sock) {
    return close(sock) != SOCKET_ERROR;
}

bool sk_bind(socket_t sock, const char *ip, u16 port) {
    struct sockaddr_in sk_addr = sk__addrin_in(ip, port);
    if (sk_addr.sin_family == 0) {
        return false;
    }
    return bind(sock, (struct sockaddr*)&sk_addr, sizeof(sk_addr)) != SOCKET_ERROR;
}

bool sk_listen(socket_t sock, int backlog) {
    return listen(sock, backlog) != SOCKET_ERROR;
}

socket_t sk_accept(socket_t sock) {
    struct sockaddr_in addr = {0};
    uint addr_size = sizeof(addr);
    return accept(sock, (struct sockaddr*)&addr, &addr_size);
}

bool sk_connect(socket_t sock, const char *server, u16 server_port) {
    // TODO
    return false;
}

int sk_send(socket_t sock, const void *buf, int len) {
    return send(sock, (const char *)buf, len, 0);
}

int sk_recv(socket_t sock, void *buf, int len) {
    return recv(sock, (char *)buf, len, 0);
}

int sk_poll(skpoll_t *to_poll, int num_to_poll, int timeout) {
    // TODO
    return 0;
}

oshandle_t sk_bind_event(socket_t sock, skevent_e event) {
    // TODO
    return os_handle_zero();
}

void sk_destroy_event(oshandle_t handle) {
    // TODO
}

void sk_reset_event(oshandle_t handle) {
    // TODO
}

#endif
