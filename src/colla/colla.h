#ifndef COLLA_HEADER
#define COLLA_HEADER

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

// when where compiling with tcc, we need to do a couple things outselves
#ifdef __TINYC__
    typedef unsigned short char16_t;
    // NOT SAFE, afaik tcc doesn't support thread_local
    #ifndef thread_local
        #define thread_local 
    #endif
#else
    #include <uchar.h>
    #ifndef thread_local
        #define thread_local _Thread_local
    #endif
#endif

// LIBC FUNCTIONS ///////////////////////////////

extern void *memcpy(void *dst, const void *src, size_t size);
extern void *memmove(void *dst, const void *src, size_t size);

#define static_assert(cond, ...) _Static_assert(cond, "" __VA_ARGS__)

typedef enum {
    COLLA_DARRAY_BLOCK_SIZE       = 64,
    COLLA_RG_MAX_MATCHES          = 16,
    COLLA_OS_ARENA_SIZE           = 1 << 20, // MB(1)
    COLLA_OS_MAX_WAITABLE_HANDLES = 256,
    COLLA_LOG_MAX_CALLBACKS       = 22,
} colla_constants_e;

// CORE MODULES /////////////////////////////////

typedef enum {
    COLLA_CORE = 0,
    COLLA_OS   = 1 << 0,
    COLLA_NET  = 1 << 1,
    COLLA_ALL  = 0xff,
} colla_modules_e;

void colla_init(colla_modules_e modules);
void colla_cleanup(void);

/////////////////////////////////////////////////

// OS AND COMPILER MACROS ///////////////////////

#if defined(_DEBUG) 
    #define COLLA_DEBUG   1
    #define COLLA_RELEASE 0
#else
    #define COLLA_DEBUG   0
    #define COLLA_RELEASE 1
#endif

#if defined(_WIN32)
    #define COLLA_WIN 1
    #define COLLA_OSX 0
    #define COLLA_LIN 0
    #define COLLA_EMC 0
#elif defined(__EMSCRIPTEN__)
    #define COLLA_WIN 0
    #define COLLA_OSX 0
    #define COLLA_LIN 0
    #define COLLA_EMC 1
#elif defined(__linux__)
    #define COLLA_WIN 0
    #define COLLA_OSX 0
    #define COLLA_LIN 1
    #define COLLA_EMC 0
#elif defined(__APPLE__)
    #define COLLA_WIN 0
    #define COLLA_OSX 1
    #define COLLA_LIN 0
    #define COLLA_EMC 0
#endif

#if defined(__COSMOPOLITAN__)
    #define COLLA_COSMO 1
#else
    #define COLLA_COSMO 0
#endif

#define COLLA_POSIX (COLLA_OSX || COLLA_LIN || COLLA_COSMO)

#if defined(__clang__)
    #define COLLA_CLANG 1
    #define COLLA_MSVC  0
    #define COLLA_TCC   0
    #define COLLA_GCC   0
#elif defined(_MSC_VER)
    #define COLLA_CLANG 0
    #define COLLA_MSVC  1
    #define COLLA_TCC   0
    #define COLLA_GCC   0
#elif defined(__TINYC__)
    #define COLLA_CLANG 0
    #define COLLA_MSVC  0
    #define COLLA_TCC   1
    #define COLLA_GCC   0
#elif defined(__GNUC__)
    #define COLLA_CLANG 0
    #define COLLA_MSVC  0
    #define COLLA_TCC   0
    #define COLLA_GCC   1
#endif

#if   COLLA_CLANG
    #define COLLA_CMT_LIB 0
#elif COLLA_MSVC
    #define COLLA_CMT_LIB 1
#elif COLLA_TCC
    #define COLLA_CMT_LIB 1
#elif COLLA_GCC
    #define COLLA_CMT_LIB 0
#endif

#if COLLA_TCC || COLLA_CLANG
    #define alignof __alignof__
#endif

#if COLLA_WIN
    #undef  NOMINMAX
    #undef  WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX

    #ifdef UNICODE
        #define COLLA_UNICODE 1
    #else
        #define COLLA_UNICODE 0
    #endif

#endif

/////////////////////////////////////////////////

// USEFUL MACROS ////////////////////////////////

#define arrlen(a) (sizeof(a) / sizeof((a)[0]))
#define COLLA_UNUSED(v) (void)(v)

#define COLLA__STRINGIFY(x) #x
#define COLLA_STRINGIFY(x) COLLA__STRINGIFY(x)

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define KB(n) (((u64)n) << 10)
#define MB(n) (((u64)n) << 20)
#define GB(n) (((u64)n) << 30)
#define TB(n) (((u64)n) << 40)

#if COLLA_DEBUG
    #define colla__assert(file, line, cond, ...) do { if (!(cond)) fatal(file ":" line " ASSERT FAILED: (" COLLA__STRINGIFY(cond) ") " __VA_ARGS__); } while (0)
    #define colla_assert(...) colla__assert(__FILE__, COLLA__STRINGIFY(__LINE__), __VA_ARGS__)
#else
    #define colla_assert(...) (void)0
#endif

/////////////////////////////////////////////////

// BASIC TYPES //////////////////////////////////

#if COLLA_WIN && COLLA_UNICODE
    typedef wchar_t TCHAR;
#else
    typedef char TCHAR;
#endif

typedef unsigned char  uchar;
typedef unsigned short ushort;
typedef unsigned int   uint;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef size_t    usize;
typedef ptrdiff_t isize;

typedef uintptr_t uptr;
typedef intptr_t  iptr;

typedef struct {
    u8 *data;
    usize len;
} buffer_t;

typedef struct arena_t arena_t;

/////////////////////////////////////////////////

// LINKED LISTS /////////////////////////////////

// singly linked list
#define list_push(list, item) ((item)->next=(list), (list)=(item))
#define list_pop(list)        ((list) = (list) ? (list)->next : NULL) 

// double linked list
#define dlist_push(list, item) do { \
        if (item) (item)->next = (list); \
        if (list) (list)->prev = (item); \
        (list) = (item); \
    } while (0)

#define dlist_pop(list, item) do { \
        if (!(item)) break; \
        if ((item)->prev) (item)->prev->next = (item)->next; \
        if ((item)->next) (item)->next->prev = (item)->prev; \
        if ((item) == (list)) (list) = (item)->next; \
    } while (0)

// ordered linked list

#define olist_push(head, tail, item) do { \
        if (tail) { \
            (tail)->next = (item); \
            (tail) = (item); \
        } \
        else { \
            (head) = (tail) = (item); \
        } \
    } while (0)

#define for_each(it, list) for (typeof(list) it = list; it; it = it->next)

/////////////////////////////////////////////////

// FORMATTING ///////////////////////////////////

int fmt_print(const char *fmt, ...);
int fmt_printv(const char *fmt, va_list args);
int fmt_buffer(char *buf, usize len, const char *fmt, ...);
int fmt_bufferv(char *buf, usize len, const char *fmt, va_list args);

/////////////////////////////////////////////////


/*
dynamic chunked array which uses an arena to allocate,
the structure needs to follow this exact format, if you want
you can use the macro darr_define(struct_name, item_type) instead:

////////////////////////////////////

typedef struct arr_t arr_t;
struct arr_t {
    int *items;
    usize block_size;
    usize count;
    arr_t *next;
    arr_t *head;
};
// equivalent to

darr_define(arr_t, int);

////////////////////////////////////

by default a chunk is 64 items long, you can change this default
by modifying the arr.block_size value before adding to the array,
or by defining DARRAY_DEFAULT_BLOCK_SIZE

usage example:

////////////////////////////////////

darr_define(arr_t, int);

arr_t *arr = NULL;

for (int i = 0; i < 100; ++i) {
    darr_push(&arena, arr, i);
}

for_each (chunk, arr) {
    for (int i = 0; i < chunk->count; ++i) {
        info("%d -> %d", i, chunk->items[i]);
    }
}
*/

#define darr_define(struct_name, item_type) typedef struct struct_name struct_name; \
    struct struct_name { \
        item_type *items; \
        usize block_size; \
        usize count; \
        struct_name *next; \
        struct_name *head; \
    }

#define darr__alloc_first(arena, arr) do { \
        (arr) = (arr) ? (arr) : alloc(arena, typeof(*arr)); \
        (arr)->head = (arr)->head ? (arr)->head : (arr); \
        (arr)->block_size = (arr)->block_size ? (arr)->block_size : COLLA_DARRAY_BLOCK_SIZE; \
        (arr)->items = alloc(arena, typeof(*arr->items), arr->block_size); \
        colla_assert((arr)->count == 0); \
    } while (0)
#define darr__alloc_block(arena, arr) do { \
        typeof(arr) newarr = alloc(arena, typeof(*arr)); \
        newarr->block_size = arr->block_size; \
        newarr->items = alloc(arena, typeof(*arr->items), arr->block_size); \
        newarr->head = arr->head; \
        arr->next = newarr; \
        arr = newarr; \
    } while (0)

#define darr_push(arena, arr, item) do { \
        if (!(arr) || (arr)->items == NULL) darr__alloc_first(arena, arr); \
        if ((arr)->count >= (arr)->block_size) darr__alloc_block(arena, arr); \
        (arr)->items[(arr)->count++] = (item); \
    } while (0)

// STRING TYPES /////////////////////////////////

#define STR_NONE SIZE_MAX
#define STR_END  SIZE_MAX

typedef struct str_t str_t;
struct str_t {
    char *buf;
    usize len;
};

typedef struct str16_t str16_t;
struct str16_t {
    char16_t *buf;
    usize len;
};

#if COLLA_UNICODE
typedef str16_t tstr_t;
#else
typedef str_t tstr_t;
#endif

typedef struct strview_t strview_t;
struct strview_t {
    const char *buf;
    usize len;
};

darr_define(str_list_t, str_t);
darr_define(strv_list_t, strview_t);

// ALLOCATED STRING /////////////////////////////

#define str__1(arena, x)               \
    _Generic((x),                      \
        const char *:    str_init,     \
        char *:          str_init,     \
        strview_t:       str_init_view \
    )(arena, x)

#define str__2(arena, cstr, clen) str_init_len(arena, cstr, clen)
#define str__impl(_1, _2, n, ...) str__##n

// either:
//     arena_t arena, [const] char *cstr, [usize len]
//     arena_t arena, strview_t view
#define str(arena, ...) str__impl(__VA_ARGS__, 2, 1, 0)(arena, __VA_ARGS__)

#define STR_EMPTY (str_t){0}

str_t str_init(arena_t *arena, const char *buf);
str_t str_init_len(arena_t *arena, const char *buf, usize len);
str_t str_init_view(arena_t *arena, strview_t view);
str_t str_fmt(arena_t *arena, const char *fmt, ...);
str_t str_fmtv(arena_t *arena, const char *fmt, va_list args);

tstr_t tstr_init(TCHAR *str, usize optional_len);
str16_t str16_init(char16_t *str, usize optional_len);

str_t str_from_str16(arena_t *arena, str16_t src);
str_t str_from_tstr(arena_t *arena, tstr_t src);
str16_t str16_from_str(arena_t *arena, str_t src);
usize str16_len(char16_t *str);

bool str_equals(str_t a, str_t b);
int str_compare(str_t a, str_t b);

str_t str_dup(arena_t *arena, str_t src);
str_t str_cat(arena_t *arena, str_t a, str_t b);
bool str_is_empty(str_t ctx);

void str_lower(str_t *src);
void str_upper(str_t *src);

void str_replace(str_t *ctx, char from, char to);
// if len == SIZE_MAX, copies until end
strview_t str_sub(str_t ctx, usize from, usize to);

// STRING VIEW //////////////////////////////////

// these macros might be THE worst code ever written, but they work ig
// detects if you're trying to create a string view from either:
//  - a str_t          -> calls strv_init_str
//  - a string literal -> calls strv_init_len with comptime size
//  - a c string       -> calls strv_init with runtime size

#define STRV_EMPTY (strview_t){0}

// needed for strv__init_literal _Generic implementation, it's never actually called
strview_t strv__ignore(str_t s, size_t l);

#define strv__check(x, ...) ((#x)[0] == '"')
#define strv__init_literal(x, ...) \
    _Generic((x), \
        char *: strv_init_len, \
        const char *: strv_init_len, \
        str_t: strv__ignore \
    )(x, sizeof(x) - 1)

#define strv__1(x)                   \
    _Generic((x),                    \
        char *:       strv_init,     \
        const char *: strv_init,     \
        str_t:        strv_init_str  \
    )(x)

#define strv__2(cstr, clen) strv_init_len(cstr, clen)

#define strv__impl(_1, _2, n, ...) strv__##n

#define strv(...) strv__check(__VA_ARGS__) ? strv__init_literal(__VA_ARGS__) : strv__impl(__VA_ARGS__, 2, 1, 0)(__VA_ARGS__)

#define cstrv(cstr) { cstr, sizeof(cstr) - 1, }

strview_t strv_init(const char *cstr);
strview_t strv_init_len(const char *buf, usize size);
strview_t strv_init_str(str_t str);

bool strv_is_empty(strview_t ctx);
bool strv_equals(strview_t a, strview_t b);
int strv_compare(strview_t a, strview_t b);
usize strv_get_utf8_len(strview_t v);

char strv_front(strview_t ctx);
char strv_back(strview_t ctx);

str16_t strv_to_str16(arena_t *arena, strview_t src);
tstr_t strv_to_tstr(arena_t *arena, strview_t src);

str_t strv_to_upper(arena_t *arena, strview_t src);
str_t strv_to_lower(arena_t *arena, strview_t src);

strview_t strv_remove_prefix(strview_t ctx, usize n);
strview_t strv_remove_suffix(strview_t ctx, usize n);
strview_t strv_trim(strview_t ctx);
strview_t strv_trim_left(strview_t ctx);
strview_t strv_trim_right(strview_t ctx);

strview_t strv_sub(strview_t ctx, usize from, usize to);

bool strv_starts_with(strview_t ctx, char c);
bool strv_starts_with_view(strview_t ctx, strview_t view);

bool strv_ends_with(strview_t ctx, char c);
bool strv_ends_with_view(strview_t ctx, strview_t view);

bool strv_contains(strview_t ctx, char c);
bool strv_contains_view(strview_t ctx, strview_t view);
bool strv_contains_either(strview_t ctx, strview_t chars);

usize strv_find(strview_t ctx, char c, usize from);
usize strv_find_view(strview_t ctx, strview_t view, usize from);
usize strv_find_either(strview_t ctx, strview_t chars, usize from);

usize strv_rfind(strview_t ctx, char c, usize from_right);
usize strv_rfind_view(strview_t ctx, strview_t view, usize from_right);

// CTYPE ////////////////////////////////////////

bool char_is_space(char c);
bool char_is_alpha(char c);
bool char_is_num(char c);
char char_lower(char c);
char char_upper(char c);

// INPUT STREAM /////////////////////////////////

typedef struct instream_t instream_t;
struct instream_t {
    const char *beg;
    const char *cur;
    usize len;
};

instream_t istr_init(strview_t str);

// get the current character and advance
char istr_get(instream_t *ctx);
// get the current character but don't advance
char istr_peek(instream_t *ctx);
// get the next character but don't advance
char istr_peek_next(instream_t *ctx);
// returns the previous character
char istr_prev(instream_t *ctx);
// returns the character before the previous
char istr_prev_prev(instream_t *ctx);
// ignore characters until the delimiter
void istr_ignore(instream_t *ctx, char delim);
// ignore characters until the delimiter and skip it
void istr_ignore_and_skip(instream_t *ctx, char delim);
// skip n characters
void istr_skip(instream_t *ctx, usize n);
// skips whitespace (' ', '\\n', '\\t', '\\r')
void istr_skip_whitespace(instream_t *ctx);
// returns to the beginning of the stream
void istr_rewind(instream_t *ctx);
// returns back <amount> characters
void istr_rewind_n(instream_t *ctx, usize amount);
// returns the number of bytes read from beginning of stream
usize istr_tell(instream_t *ctx);
// returns the number of bytes left to read in the stream
usize istr_remaining(instream_t *ctx); 
// return true if the stream doesn't have any new bytes to read
bool istr_is_finished(instream_t *ctx);

bool istr_get_bool(instream_t *ctx, bool *val);
bool istr_get_u8(instream_t *ctx, u8 *val);
bool istr_get_u16(instream_t *ctx, u16 *val);
bool istr_get_u32(instream_t *ctx, u32 *val);
bool istr_get_u64(instream_t *ctx, u64 *val);
bool istr_get_i8(instream_t *ctx, i8 *val);
bool istr_get_i16(instream_t *ctx, i16 *val);
bool istr_get_i32(instream_t *ctx, i32 *val);
bool istr_get_i64(instream_t *ctx, i64 *val);
bool istr_get_num(instream_t *ctx, double *val);
bool istr_get_float(instream_t *ctx, float *val);
strview_t istr_get_view(instream_t *ctx, char delim);
strview_t istr_get_view_either(instream_t *ctx, strview_t chars);
strview_t istr_get_view_len(instream_t *ctx, usize len);
strview_t istr_get_line(instream_t *ctx);
strview_t istr_get_word(instream_t *ctx);

// OUTPUT STREAM ////////////////////////////////

typedef struct outstream_t outstream_t;
struct outstream_t {
    char *beg;
    arena_t *arena;
};

outstream_t ostr_init(arena_t *exclusive_arena);
void ostr_clear(outstream_t *ctx);

usize ostr_tell(outstream_t *ctx);

char ostr_back(outstream_t *ctx);
str_t ostr_to_str(outstream_t *ctx);
strview_t ostr_as_view(outstream_t *ctx);

void ostr_rewind(outstream_t *ctx, usize from_beg);
void ostr_pop(outstream_t *ctx, usize count);

void ostr_print(outstream_t *ctx, const char *fmt, ...);
void ostr_printv(outstream_t *ctx, const char *fmt, va_list args);
void ostr_putc(outstream_t *ctx, char c);
void ostr_puts(outstream_t *ctx, strview_t v);

void ostr_append_bool(outstream_t *ctx, bool val);
void ostr_append_uint(outstream_t *ctx, u64 val);
void ostr_append_int(outstream_t *ctx, i64 val);
void ostr_append_num(outstream_t *ctx, double val);

// INPUT BINARY STREAM //////////////////////////

typedef struct {
    const u8 *beg;
    const u8 *cur;
    usize len;
} ibstream_t;

ibstream_t ibstr_init(buffer_t buffer);

bool ibstr_is_finished(ibstream_t *ib);
usize ibstr_tell(ibstream_t *ib);
usize ibstr_remaining(ibstream_t *ib);
usize ibstr_read(ibstream_t *ib, void *buffer, usize len);
void ibstr_skip(ibstream_t *ib, usize count);

bool ibstr_get_u8(ibstream_t *ib, u8 *out);
bool ibstr_get_u16(ibstream_t *ib, u16 *out);
bool ibstr_get_u32(ibstream_t *ib, u32 *out);
bool ibstr_get_u64(ibstream_t *ib, u64 *out);

bool ibstr_get_i8(ibstream_t *ib, i8 *out);
bool ibstr_get_i16(ibstream_t *ib, i16 *out);
bool ibstr_get_i32(ibstream_t *ib, i32 *out);
bool ibstr_get_i64(ibstream_t *ib, i64 *out);

// REGEX ////////////////////////////////////////

// typedef struct rg_result_t rg_result_t;
// struct rg_result_t {
//     strview_t text;
//     i64 offset;
// };
//
// typedef struct rg_match_t rg_match_t;
// struct rg_match_t {
//     rg_result_t matches[COLLA_RG_MAX_MATCHES];
//     int count;
//     bool found_match;
// };

bool rg_matches(strview_t rg, strview_t text);
bool glob_matches(strview_t glob, strview_t text);

/////////////////////////////////////////////////

// ARENA ////////////////////////////////////////

#if COLLA_WIN && !COLLA_TCC
#define alignof __alignof
#endif

typedef enum arena_type_e {
    ARENA_TYPE_NONE, // only here so that a 0 initialised arena is valid
    ARENA_VIRTUAL,
    ARENA_MALLOC,
    ARENA_MALLOC_ALWAYS,
    ARENA_STATIC,
} arena_type_e;

typedef enum alloc_flags_e {
    ALLOC_FLAGS_NONE = 0,
    ALLOC_NOZERO     = 1 << 0,
    ALLOC_SOFT_FAIL  = 1 << 1,
} alloc_flags_e;

typedef struct arena_t arena_t;
struct arena_t {
    u8 *beg;
    u8 *cur;
    u8 *end;
    arena_type_e type;
};

typedef struct arena_desc_t arena_desc_t;
struct arena_desc_t {
    arena_type_e type;
    usize size;
    u8 *static_buffer;
};

typedef struct arena_alloc_desc_t arena_alloc_desc_t;
struct arena_alloc_desc_t {
    arena_t *arena;
    usize count;
    alloc_flags_e flags;
    usize align;
    usize size;
};

// arena_type_e type, usize allocation, [ byte *static_buffer ]
#define arena_make(...) arena_init(&(arena_desc_t){ __VA_ARGS__ })

// arena_t *arena, T type, [ usize count, alloc_flags_e flags, usize align, usize size ]
#define alloc(arenaptr, type, ...) arena_alloc(&(arena_alloc_desc_t){ .size = sizeof(type), .count = 1, .align = alignof(type), .arena = arenaptr, __VA_ARGS__ })

// simple arena that always calls malloc internally, this is useful if you need
// malloc for some reason but want to still use the arena interface
// WARN: most arena functions outside of alloc/scratch won't work!
//       you also need to each allocation afterwards! this is still
//       malloc 
extern arena_t malloc_arena;

arena_t arena_init(const arena_desc_t *desc);
void arena_cleanup(arena_t *arena);

arena_t arena_scratch(arena_t *arena, usize size);

void *arena_alloc(const arena_alloc_desc_t *desc);
usize arena_tell(arena_t *arena);
usize arena_remaining(arena_t *arena);
usize arena_capacity(arena_t *arena);
void arena_rewind(arena_t *arena, usize from_start);
void arena_pop(arena_t *arena, usize amount);

// OS LAYER /////////////////////////////////////

#define OS_WAIT_INFINITE (0xFFFFFFFF)

typedef struct oshandle_t oshandle_t;
struct oshandle_t {
    uptr data;
};

typedef enum {
    OS_ARCH_X86,
    OS_ARCH_ARM,
    OS_ARCH_IA64,
    OS_ARCH_AMD64,
    OS_ARCH_ARM64,
} os_arch_e;

typedef struct os_system_info_t os_system_info_t;
struct os_system_info_t {
    u32 processor_count;
    u64 page_size;
    str_t machine_name;
    os_arch_e architecture;
};

void os_init(void);
void os_cleanup(void);
os_system_info_t os_get_system_info(void);
void os_abort(int code);

iptr os_get_last_error(void);
// NOT thread safe
str_t os_get_error_string(iptr error);

// == HANDLE ====================================

oshandle_t os_handle_zero(void);
bool os_handle_match(oshandle_t a, oshandle_t b);
bool os_handle_valid(oshandle_t handle);

typedef enum {
    OS_WAIT_FINISHED,
    OS_WAIT_ABANDONED,
    OS_WAIT_TIMEOUT,
    OS_WAIT_FAILED
} os_wait_result_e;

typedef struct {
    os_wait_result_e result;
    u32 index;
} os_wait_t;

os_wait_t os_wait_on_handles(oshandle_t *handles, int count, bool wait_all, u32 milliseconds);

// == LOGGING ===================================

typedef enum os_log_level_e {
    LOG_BASIC,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERR,
    LOG_FATAL,
} os_log_level_e;

typedef enum os_log_colour_e {
    LOG_COL_BLACK     = 0,
    LOG_COL_BLUE      = 1,
    LOG_COL_GREEN     = 2,
    LOG_COL_CYAN      = LOG_COL_BLUE | LOG_COL_GREEN,
    LOG_COL_RED       = 4,
    LOG_COL_MAGENTA   = LOG_COL_RED | LOG_COL_BLUE,
    LOG_COL_YELLOW    = LOG_COL_RED | LOG_COL_GREEN,
    LOG_COL_GREY      = LOG_COL_RED | LOG_COL_BLUE | LOG_COL_GREEN,
    
    LOG_COL_LIGHT     = 8,
    
    LOG_COL_DARK_GREY     = LOG_COL_BLACK   | LOG_COL_LIGHT,
    LOG_COL_LIGHT_BLUE    = LOG_COL_BLUE    | LOG_COL_LIGHT,
    LOG_COL_LIGHT_GREEN   = LOG_COL_GREEN   | LOG_COL_LIGHT,
    LOG_COL_LIGHT_CYAN    = LOG_COL_CYAN    | LOG_COL_LIGHT,
    LOG_COL_LIGHT_RED     = LOG_COL_RED     | LOG_COL_LIGHT,
    LOG_COL_LIGHT_MAGENTA = LOG_COL_MAGENTA | LOG_COL_LIGHT,
    LOG_COL_LIGHT_YELLOW  = LOG_COL_YELLOW  | LOG_COL_LIGHT,
    LOG_COL_WHITE         = LOG_COL_GREY    | LOG_COL_LIGHT,

    LOG_COL_RESET,

    LOG_COL__COUNT,
} os_log_colour_e;

typedef struct log_event_t log_event_t;
struct log_event_t {
    va_list args;
    const char *fmt;
    const char *file;
    int line;
    struct tm *time;
    os_log_level_e level;
    void *udata;
};

typedef struct log_callback_t log_callback_t;
struct log_callback_t {
    void (*fn)(log_event_t *ev);
    void *udata;
    os_log_level_e level;
};

typedef enum {
    OS_LOG_DEFAULT         = 0,
    OS_LOG_NOTIME          = 1 << 0,
    OS_LOG_NOFILE          = 1 << 1,
    OS_LOG_NOCRASH         = 1 << 2,
    OS_LOG_SIMPLE          = OS_LOG_NOTIME | OS_LOG_NOFILE,
} os_log_options_e;

void os_log_set_options(os_log_options_e opt);
os_log_options_e os_log_get_options(void);
void os_log_add_callback(log_callback_t cb);
void os_log_add_fp(oshandle_t fp, os_log_level_e level);
void os_log_print(const char *file, int line, os_log_level_e level, const char *fmt, ...);
void os_log_printv(const char *file, int line, os_log_level_e level, const char *fmt, va_list args);
void os_log_set_colour(os_log_colour_e colour);
void os_log_set_colour_bg(os_log_colour_e foreground, os_log_colour_e background);

oshandle_t os_stdout(void);
oshandle_t os_stdin(void);

// windows specific
oshandle_t os_win_conout(void);
oshandle_t os_win_conin(void);

#define print(...)   fmt_print(__VA_ARGS__)
#define println(...) os_log_print(__FILE__, __LINE__, LOG_BASIC, __VA_ARGS__)
#define debug(...)   os_log_print(__FILE__, __LINE__, LOG_DEBUG, __VA_ARGS__)
#define info(...)    os_log_print(__FILE__, __LINE__, LOG_INFO,  __VA_ARGS__)
#define warn(...)    os_log_print(__FILE__, __LINE__, LOG_WARN,  __VA_ARGS__)
#define err(...)     os_log_print(__FILE__, __LINE__, LOG_ERR,   __VA_ARGS__)
#define fatal(...)   os_log_print(__FILE__, __LINE__, LOG_FATAL, __VA_ARGS__)

// == FILE ======================================

typedef enum filemode_e {
    OS_FILE_READ  = 1 << 0,
    OS_FILE_WRITE = 1 << 1,
} filemode_e;

str_t os_path_join(arena_t *arena, strview_t left, strview_t right);

bool os_file_exists(strview_t filename);
bool os_dir_exists(strview_t folder);
bool os_file_or_dir_exists(strview_t path);
bool os_dir_create(strview_t folder);
tstr_t os_file_fullpath(arena_t *arena, strview_t filename);
void os_file_split_path(strview_t path, strview_t *dir, strview_t *name, strview_t *ext);
bool os_file_delete(strview_t path);
bool os_dir_delete(strview_t path);

oshandle_t os_file_open(strview_t path, filemode_e mode);
void os_file_close(oshandle_t handle);

bool os_file_putc(oshandle_t handle, char c);
bool os_file_puts(oshandle_t handle, strview_t str);
bool os_file_print(arena_t scratch, oshandle_t handle, const char *fmt, ...);
bool os_file_printv(arena_t scratch, oshandle_t handle, const char *fmt, va_list args);

usize os_file_read(oshandle_t handle, void *buf, usize len);
usize os_file_write(oshandle_t handle, const void *buf, usize len);

usize os_file_read_buf(oshandle_t handle, buffer_t *buf);
usize os_file_write_buf(oshandle_t handle, buffer_t buf);

bool os_file_seek(oshandle_t handle, usize offset);
bool os_file_seek_end(oshandle_t handle);
void os_file_rewind(oshandle_t handle);
usize os_file_tell(oshandle_t handle);
usize os_file_size(oshandle_t handle);
bool os_file_is_finished(oshandle_t handle);

buffer_t os_file_read_all(arena_t *arena, strview_t path);
buffer_t os_file_read_all_fp(arena_t *arena, oshandle_t handle);

str_t os_file_read_all_str(arena_t *arena, strview_t path);
str_t os_file_read_all_str_fp(arena_t *arena, oshandle_t handle);

bool os_file_write_all(strview_t name, buffer_t buffer);
bool os_file_write_all_fp(oshandle_t handle, buffer_t buffer);

bool os_file_write_all_str(strview_t name, strview_t data);
bool os_file_write_all_str_fp(oshandle_t handle, strview_t data);

u64 os_file_time(strview_t path);
u64 os_file_time_fp(oshandle_t handle);
bool os_file_has_changed(strview_t path, u64 last_change);

// == DIR WALKER ================================

typedef enum dir_type_e {
    DIRTYPE_FILE,
    DIRTYPE_DIR,
} dir_type_e;

typedef struct dir_entry_t dir_entry_t;
struct dir_entry_t {
    str_t name;
    dir_type_e type;
    usize file_size;
};

#define dir_foreach(arena, it, dir) for (dir_entry_t *it = os_dir_next(arena, dir); it; it = os_dir_next(arena, dir))

typedef struct dir_t dir_t;
dir_t *os_dir_open(arena_t *arena, strview_t path);
bool os_dir_is_valid(dir_t *dir);
// optional, only call this if you want to return before os_dir_next returns NULL
void os_dir_close(dir_t *dir);

dir_entry_t *os_dir_next(arena_t *arena, dir_t *dir);

// == PROCESS ===================================

typedef struct os_env_t os_env_t;
typedef strv_list_t os_cmd_t;
#define os_make_cmd(...) &(os_cmd_t){ .items = (strview_t[]){ __VA_ARGS__ }, .count = arrlen(((strview_t[]){ __VA_ARGS__ })) }

typedef struct os_cmd_options_t os_cmd_options_t;
struct os_cmd_options_t {
    os_env_t *env;
    // redirected if !NULL
    oshandle_t *error;
    oshandle_t *out;
    oshandle_t *in;
};

void os_set_env_var(arena_t scratch, strview_t key, strview_t value);
str_t os_get_env_var(arena_t *arena, strview_t key);
os_env_t *os_get_env(arena_t *arena);
bool os_run_cmd(arena_t scratch, os_cmd_t *cmd, os_cmd_options_t *options);
oshandle_t os_run_cmd_async(arena_t scratch, os_cmd_t *cmd, os_cmd_options_t *options);
bool os_process_wait(oshandle_t proc, uint time, int *out_exit);

// == MEMORY ====================================

void *os_alloc(usize size);
void os_free(void *ptr);

void *os_reserve(usize size, usize *out_padded_size);
bool os_commit(void *ptr, usize num_of_pages);
bool os_release(void *ptr, usize size);
usize os_pad_to_page(usize byte_count);

// == THREAD ====================================

#ifndef thread_local
    #define thread_local _Thread_local
#endif

typedef struct os_barrier_t os_barrier_t;
struct os_barrier_t {
    i64 thread_count;
    i64 thread_value;
    i64 has_completed;
};

extern thread_local i64 os_thread_id;
extern i64 os_thread_count;

void os_barrier_sync(os_barrier_t *b);

typedef int (thread_func_t)(u64 thread_id, void *userdata);

oshandle_t os_thread_launch(thread_func_t func, void *userdata);
bool os_thread_detach(oshandle_t thread);
bool os_thread_join(oshandle_t thread, int *code);

u64 os_thread_get_id(oshandle_t thread);

typedef struct i64range_t i64range_t;
struct i64range_t {
    i64 min;
    i64 max;
};

i64range_t os_lane_range(u64 values_count);

// == MUTEX =====================================

oshandle_t os_mutex_create(void);
void os_mutex_free(oshandle_t mutex);
void os_mutex_lock(oshandle_t mutex);
void os_mutex_unlock(oshandle_t mutex);
bool os_mutex_try_lock(oshandle_t mutex);

#if !COLLA_NO_CONDITION_VARIABLE
// == CONDITION VARIABLE ========================

oshandle_t os_cond_create(void);
void os_cond_free(oshandle_t cond);

void os_cond_signal(oshandle_t cond);
void os_cond_broadcast(oshandle_t cond);

void os_cond_wait(oshandle_t cond, oshandle_t mutex, int milliseconds);

// == JOB QUEUE =================================

typedef void (job_func_f)(void *userdata);

typedef struct job_t job_t;
struct job_t {
    job_t *next;
    job_func_f *func;
    void *userdata;
};

typedef struct job_queue_t job_queue_t;
struct job_queue_t {
    job_t *jobs;
    job_t *freelist;
    bool should_stop;
    bool stop_when_finished;
    int reader;
    int writer;
    oshandle_t mutex;
    oshandle_t condvar;
    oshandle_t *threads;
    i64 *thread_ids;
    int thread_count;
};

// pass 0 to worker count to use max workers (os_get_system_info().processor_count)
job_queue_t *jq_init(arena_t *arena, int worker_count);
void jq_stop(job_queue_t *queue);
// no need to call this if you call jq_stop
void jq_cleanup(job_queue_t *queue);
void jq_push(arena_t *arena, job_queue_t *queue, job_func_f *func, void *userdata);
job_t *jq_pop_job(job_queue_t *queue);

#endif

// == ATOMICS ========================================

i64 atomic_set_i64(i64 *dest, i64 val);
i64 atomic_add_i64(i64 *dest, i64 val);
i64 atomic_and_i64(i64 *dest, i64 val);
// if (*dest == cmp) *dest = val
i64 atomic_cmp_i64(i64 *dest, i64 val, i64 cmp);
i64 atomic_inc_i64(i64 *dest);
i64 atomic_dec_i64(i64 *dest);
i64 atomic_or_i64(i64 *dest, i64 val);
i64 atomic_xor_i64(i64 *dest, i64 val);

// PARSERS //////////////////////////////////////

// == INI ============================================

typedef struct inivalue_t inivalue_t;
struct inivalue_t {
    strview_t key;
    strview_t value;
    inivalue_t *next;
};

typedef struct initable_t initable_t;
struct initable_t {
    strview_t name;
    inivalue_t *values;
    inivalue_t *tail;
    initable_t *next;
};

typedef struct ini_t ini_t;
struct ini_t {
    strview_t text;
    initable_t *tables;
    initable_t *tail;
};

typedef struct iniopt_t iniopt_t;
struct iniopt_t {
    bool merge_duplicate_tables; // default false
    bool merge_duplicate_keys;   // default false
    char key_value_divider;      // default =
    strview_t comment_vals;      // default ;#
};

typedef struct iniarray_t iniarray_t;
struct iniarray_t {
    strview_t *values;
    usize count;
};

#define INI_ROOT strv("__ROOT__")

ini_t ini_parse(arena_t *arena, strview_t filename, iniopt_t *opt);
ini_t ini_parse_fp(arena_t *arena, oshandle_t file, iniopt_t *opt);
ini_t ini_parse_str(arena_t *arena, strview_t str, iniopt_t *opt);

bool ini_is_valid(ini_t *ini);

initable_t *ini_get_table(ini_t *ini, strview_t name);
inivalue_t *ini_get(initable_t *table, strview_t key);

iniarray_t ini_as_arr(arena_t *arena, inivalue_t *value, char delim);
u64 ini_as_uint(inivalue_t *value);
i64 ini_as_int(inivalue_t *value);
double ini_as_num(inivalue_t *value);
bool ini_as_bool(inivalue_t *value);

typedef enum {
    INI_PRETTY_COLOUR_KEY,
    INI_PRETTY_COLOUR_VALUE,
    INI_PRETTY_COLOUR_DIVIDER,
    INI_PRETTY_COLOUR_TABLE,
    INI_PRETTY_COLOUR__COUNT,
} ini_pretty_colours_e;

typedef struct ini_pretty_opts_t ini_pretty_opts_t;
struct ini_pretty_opts_t {
    oshandle_t custom_target;
    bool use_custom_colours;
    os_log_colour_e colours[INI_PRETTY_COLOUR__COUNT];
};

void ini_pretty_print(ini_t *ini, const ini_pretty_opts_t *options);

// == JSON ===========================================

typedef enum jsontype_e {
    JSON_NULL,
    JSON_ARRAY,
    JSON_STRING,
    JSON_NUMBER,
    JSON_BOOL,
    JSON_OBJECT,
} jsontype_e;

typedef enum jsonflags_e {
    JSON_DEFAULT            = 0,
    JSON_NO_TRAILING_COMMAS = 1 << 0,
    JSON_NO_COMMENTS        = 1 << 1,
    JSON_ONLY_OBJECT_START  = 1 << 2,
} jsonflags_e;

typedef struct json_t json_t;
struct json_t {
    json_t *next;
    json_t *prev;

    strview_t key;

    union {
        json_t *array;
        strview_t string;
        double number;
        bool boolean;
        json_t *object;
    };
    jsontype_e type;
};

json_t *json_parse(arena_t *arena, strview_t filename, jsonflags_e flags);
json_t *json_parse_str(arena_t *arena, strview_t str, jsonflags_e flags);

json_t *json_get(json_t *node, strview_t key);

#define json_check(val, js_type) ((val) && (val)->type == js_type)
#define json_for(it, arr) for (json_t *it = json_check(arr, JSON_ARRAY) ? arr->array : NULL; it; it = it->next)

typedef enum json_pretty_colours_e {
    JSON_PRETTY_COLOUR_KEY,
    JSON_PRETTY_COLOUR_STRING,
    JSON_PRETTY_COLOUR_NUM,
    JSON_PRETTY_COLOUR_NULL,
    JSON_PRETTY_COLOUR_TRUE,
    JSON_PRETTY_COLOUR_FALSE,
    JSON_PRETTY_COLOUR__COUNT,
} json_pretty_colours_e;

typedef struct json_pretty_opts_t json_pretty_opts_t;
struct json_pretty_opts_t {
    oshandle_t custom_target;
    bool use_custom_colours;
    os_log_colour_e colours[JSON_PRETTY_COLOUR__COUNT];
};

void json_pretty_print(json_t *root, const json_pretty_opts_t *options);

// == XML ============================================

typedef struct xmlattr_t xmlattr_t;
struct xmlattr_t {
    strview_t key;
    strview_t value;
    xmlattr_t *next;
};

typedef struct xmltag_t xmltag_t;
struct xmltag_t {
    strview_t key;
    xmlattr_t *attributes;
    strview_t content;
    xmltag_t *child;
    xmltag_t *tail;
    xmltag_t *next;
};

typedef struct xml_t xml_t;
struct xml_t {
    strview_t text;
    xmltag_t *root;
    xmltag_t *tail;
};

xml_t xml_parse(arena_t *arena, strview_t filename);
xml_t xml_parse_str(arena_t *arena, strview_t xmlstr);

xmltag_t *xml_get_tag(xmltag_t *parent, strview_t key, bool recursive);
strview_t xml_get_attribute(xmltag_t *tag, strview_t key);

// == HTML ===========================================

typedef struct htmltag_t htmltag_t;
struct htmltag_t {
    str_t key;
    xmlattr_t *attributes;
    strview_t content;
    htmltag_t *children;
    htmltag_t *tail;
    htmltag_t *next;
};

typedef struct html_t html_t;
struct html_t {
    strview_t text;
    htmltag_t *root;
    htmltag_t *tail;
};

html_t html_parse(arena_t *arena, strview_t filename);
html_t html_parse_str(arena_t *arena, strview_t str);

htmltag_t *html_get_tag(htmltag_t *parent, strview_t key, bool recursive);
strview_t html_get_attribute(htmltag_t *tag, strview_t key);

// NETWORKING ///////////////////////////////////

void net_init(void);
void net_cleanup(void);
iptr net_get_last_error(void);

typedef enum http_method_e {
    HTTP_GET,   
    HTTP_POST,    
    HTTP_HEAD,    
    HTTP_PUT,    
    HTTP_DELETE,
    HTTP_METHOD__COUNT,
} http_method_e;

const char *http_get_method_string(http_method_e method);
const char *http_get_status_string(int status);

typedef struct http_version_t http_version_t;
struct http_version_t {
    u8 major;
    u8 minor;
};

typedef struct http_header_t http_header_t;
struct http_header_t {
    strview_t key;
    strview_t value;
    http_header_t *next;
};

typedef struct http_req_t http_req_t;
struct http_req_t {
    http_method_e method;
    http_version_t version;
    http_header_t *headers;
    strview_t url;
    strview_t body;
};

typedef struct http_res_t http_res_t;
struct http_res_t {
    int status_code;
    http_version_t version;
    http_header_t *headers;
    strview_t body;
};

http_header_t *http_parse_headers(arena_t *arena, strview_t header_string);

http_req_t http_parse_req(arena_t *arena, strview_t request);
http_res_t http_parse_res(arena_t *arena, strview_t response);

str_t http_req_to_str(arena_t *arena, http_req_t *req);
str_t http_res_to_str(arena_t *arena, http_res_t *res);

http_header_t *http_add_header(arena_t *arena, http_header_t *headers, strview_t key, strview_t value);
bool http_has_header(http_header_t *headers, strview_t key);
void http_set_header(http_header_t *headers, strview_t key, strview_t value);
strview_t http_get_header(http_header_t *headers, strview_t key);

str_t http_make_url_safe(arena_t *arena, strview_t string);
str_t http_decode_url_safe(arena_t *arena, strview_t string);

typedef struct {
    strview_t host;
    strview_t uri;
} http_url_t;

http_url_t http_split_url(strview_t url);

typedef struct {
    arena_t *arena;
    strview_t url;
    http_version_t version; // 1.1 by default
    http_method_e request_type;
    http_header_t *headers; 
    int header_count; // optional, if set to 0 it traverses headers using h->next
    strview_t body;
} http_request_desc_t;

typedef void (*http_request_callback_fn)(http_header_t *headers, strview_t chunk, void *udata);

// arena_t *arena, strview_t url, [ http_header_t *headers, int header_count, strview_t body ]
#define http_get(arena, url, ...) http_request(&(http_request_desc_t){ arena, url, .request_type = HTTP_GET, .version = { 1, 1 }, __VA_ARGS__ })

http_res_t http_request(http_request_desc_t *request);
http_res_t http_request_cb(http_request_desc_t *request, http_request_callback_fn callback, void *userdata);

// SOCKETS //////////////////////////

typedef uintptr_t socket_t;
typedef struct skpoll_t skpoll_t;

#define SK_ADDR_LOOPBACK "127.0.0.1"
#define SK_ADDR_ANY "0.0.0.0"
#define SK_ADDR_BROADCAST "255.255.255.255"

struct skpoll_t {
    uintptr_t  socket;
    short events;
    short revents;
};

#define SOCKET_ERROR (-1)

typedef enum {
    SOCK_TCP,
    SOCK_UDP,
} sktype_e;

typedef enum {
    SOCK_EVENT_NONE,
    SOCK_EVENT_READ    = 1 << 0,
    SOCK_EVENT_WRITE   = 1 << 1,
    SOCK_EVENT_ACCEPT  = 1 << 2,
    SOCK_EVENT_CONNECT = 1 << 3,
    SOCK_EVENT_CLOSE   = 1 << 4,
} skevent_e;

// Opens a socket
socket_t sk_open(sktype_e type);
// Opens a socket using 'protocol', options are 
// ip, icmp, ggp, tcp, egp, pup, udp, hmp, xns-idp, rdp
socket_t sk_open_protocol(const char *protocol);

// Checks that a opened socket is valid, returns true on success
bool sk_is_valid(socket_t sock);

// Closes a socket, returns true on success
bool sk_close(socket_t sock);

// Fill out a sk_addrin_t structure with "ip" and "port"
// skaddrin_t sk_addrin_init(const char *ip, uint16_t port);

// Associate a local address with a socket
bool sk_bind(socket_t sock, const char *ip, u16 port);

// Place a socket in a state in which it is listening for an incoming connection
bool sk_listen(socket_t sock, int backlog);

// Permits an incoming connection attempt on a socket
socket_t sk_accept(socket_t sock);

// Connects to a server (e.g. "127.0.0.1" or "google.com") with a port(e.g. 1234), returns true on success
bool sk_connect(socket_t sock, const char *server, u16 server_port);

// Sends data on a socket, returns true on success
int sk_send(socket_t sock, const void *buf, int len);
// Receives data from a socket, returns byte count on success, 0 on connection close or -1 on error
int sk_recv(socket_t sock, void *buf, int len);

// Wait for an event on some sockets
int sk_poll(skpoll_t *to_poll, int num_to_poll, int timeout);

oshandle_t sk_bind_event(socket_t sock, skevent_e event);
void sk_destroy_event(oshandle_t handle);
void sk_reset_event(oshandle_t handle);

// WEBSOCKETS ///////////////////////

bool websocket_init(arena_t scratch, socket_t socket, strview_t key);
buffer_t websocket_encode(arena_t *arena, strview_t message);
str_t websocket_decode(arena_t *arena, buffer_t message);

// SHA 1 ////////////////////////////

typedef struct sha1_t sha1_t;
struct sha1_t {
    u32 digest[5];
    u8 block[64];
    usize block_index;
    usize byte_count;
};

typedef struct sha1_result_t sha1_result_t;
struct sha1_result_t {
    u32 digest[5];
};

sha1_t sha1_init(void);
sha1_result_t sha1(sha1_t *ctx, const void *buf, usize len);
str_t sha1_str(arena_t *arena, sha1_t *ctx, const void *buf, usize len);

// BASE 64 //////////////////////////

buffer_t base64_encode(arena_t *arena, buffer_t buffer);
buffer_t base64_decode(arena_t *arena, buffer_t buffer);

// PRETTY PRINTING //////////////////////////////

strview_t pretty_log_to_colour(os_log_colour_e colour);

void pretty_print(arena_t scratch, const char *fmt, ...);
void pretty_printv(arena_t scratch, const char *fmt, va_list args);

str_t pretty_print_get_string(arena_t *arena, const char *fmt, ...);
str_t pretty_print_get_stringv(arena_t *arena, const char *fmt, va_list args);
usize pretty_print_get_length(strview_t view);

#endif
