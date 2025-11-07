#include "colla.h"

#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if COLLA_TCC 
#define COLLA_NO_CONDITION_VARIABLE 1
#define COLLA_NO_NET 1
#endif

#if COLLA_WIN
    #include "colla_win32.c"
#else
    #include "colla_lin.c"
#endif


#if COLLA_CLANG
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Weverything"
#endif

#define STB_SPRINTF_DECORATE(name) colla_stb_##name
#define STB_SPRINTF_NOUNALIGNED
#define STB_SPRINTF_IMPLEMENTATION
#include "stb/stb_sprintf.h"

#if COLLA_CLANG
    #pragma clang diagnostic pop
#endif

colla_modules_e colla__initialised_modules = 0;

extern void os_init(void);
extern void os_cleanup(void);
#if !COLLA_NO_NET
extern void net_init(void);
extern void net_cleanup(void);
#endif

static char *colla_fmt__stb_callback(const char *buf, void *ud, int len) {
    // TODO maybe use os_write?
    fflush(stdout);
    fwrite(buf, 1, len, stdout);
    return (char *)ud;
}

void colla_init(colla_modules_e modules) {
    colla__initialised_modules = modules;
    if (modules & COLLA_OS) {
        os_init();
    }
#if !COLLA_NO_NET
    if (modules & COLLA_NET) {
        net_init();
    }
#endif
}

void colla_cleanup(void) {
    colla_modules_e modules = colla__initialised_modules;
    if (modules & COLLA_OS) {
        os_cleanup();
    }
#if !COLLA_NO_NET
    if (modules & COLLA_NET) {
        net_cleanup();
    }
#endif
}

int fmt_print(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int out = fmt_printv(fmt, args);
    va_end(args);
    return out;
}

int fmt_printv(const char *fmt, va_list args) {
    char buffer[STB_SPRINTF_MIN] = {0};
    return colla_stb_vsprintfcb(colla_fmt__stb_callback, buffer, buffer, fmt, args);
}

int fmt_buffer(char *buf, usize len, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int out = fmt_bufferv(buf, len, fmt, args);
    va_end(args);
    return out;
}

int fmt_bufferv(char *buf, usize len, const char *fmt, va_list args) {
    return colla_stb_vsnprintf(buf, (int)len, fmt, args);
}

// == STR_T ========================================================

strview_t strv__ignore(str_t s, size_t l) {
    COLLA_UNUSED(s); COLLA_UNUSED(l); 
    return STRV_EMPTY; 
}

str_t str_init(arena_t *arena, const char *buf) {
	return str_init_len(arena, buf, buf ? strlen(buf) : 0);
}

str_t str_init_len(arena_t *arena, const char *buf, usize len) {
	if (!buf || !len) return STR_EMPTY;
	char *str = alloc(arena, char, len + 1);
    memmove(str, buf, len);
	return (str_t){ str, len };
}

str_t str_init_view(arena_t *arena, strview_t view) {
	return str_init_len(arena, view.buf, view.len);
}

str_t str_fmt(arena_t *arena, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	str_t out = str_fmtv(arena, fmt, args);
	va_end(args);
	return out;
}

str_t str_fmtv(arena_t *arena, const char *fmt, va_list args) {
    va_list vcopy;
    va_copy(vcopy, args);
    // stb_vsnprintf returns the length + null_term
    int len = fmt_bufferv(NULL, 0, fmt, vcopy);
    va_end(vcopy);

    char *buffer = alloc(arena, char, len + 1);
    fmt_bufferv(buffer, len + 1, fmt, args);

    return (str_t) { .buf = buffer, .len = (usize)len };
}

tstr_t tstr_init(TCHAR *str, usize optional_len) {
    if (str && !optional_len) {
#if COLLA_UNICODE
        optional_len = wcslen(str);
#else
        optional_len = strlen(str);
#endif
    }
    return (tstr_t){
        .buf = str,
        .len = optional_len,
    };
}

str16_t str16_init(char16_t *str, usize optional_len) {
    if (str && !optional_len) {
        optional_len = str16_len(str);
    }
    return (str16_t){
        .buf = str,
        .len = optional_len,
    };
}

str_t str_from_str16(arena_t *arena, str16_t src) {
    if (!src.buf) return STR_EMPTY;
    if (!src.len) return STR_EMPTY;

    str_t out = str_os_from_str16(arena, src);

    return out;
}

usize str16_len(char16_t *str) {
#if COLLA_WIN
    return wcslen(str);
#else
    usize len = 0;
    while (*str) {
        str++;
        len++;
    }
    return len;
#endif
}

str_t str_from_tstr(arena_t *arena, tstr_t src) {
#if COLLA_UNICODE
    return str_from_str16(arena, src);
#else
    return str(arena, strv(src));
#endif
}

str16_t str16_from_str(arena_t *arena, str_t src) {
    return strv_to_str16(arena, strv(src));
}

bool str_equals(str_t a, str_t b) {
	return str_compare(a, b) == 0;
}

int str_compare(str_t a, str_t b) {
	// TODO unsinged underflow if a.len < b.len
	return a.len == b.len ? memcmp(a.buf, b.buf, a.len) : (int)(a.len - b.len);
}

str_t str_dup(arena_t *arena, str_t src) {
	return str_init_len(arena, src.buf, src.len);
}

str_t str_cat(arena_t *arena, str_t a, str_t b) {
    str_t out = STR_EMPTY;

    out.len += a.len + b.len;
    out.buf = alloc(arena, char, out.len + 1);
    memcpy(out.buf, a.buf, a.len);
    memcpy(out.buf + a.len, b.buf, b.len);

    return out;
}

bool str_is_empty(str_t ctx) {
	return !ctx.buf || !ctx.len;
}

void str_lower(str_t *src) {
    for (usize i = 0; i < src->len; ++i) {
        if (src->buf[i] >= 'A' && src->buf[i] <= 'Z') {
            src->buf[i] += 'a' - 'A';
        }
    }
}

void str_upper(str_t *src) {
    for (usize i = 0; i < src->len; ++i) {
        if (src->buf[i] >= 'a' && src->buf[i] <= 'z') {
            src->buf[i] -= 'a' - 'A';
        }
    }
}

void str_replace(str_t *ctx, char from, char to) {
    if (!ctx) return;
    char *buf = ctx->buf;
    for (usize i = 0; i < ctx->len; ++i) {
        buf[i] = buf[i] == from ? to : buf[i];
    }
}

strview_t str_sub(str_t ctx, usize from, usize to) {
    if (to > ctx.len) to = ctx.len;
    if (from > to)    from = to;
    return (strview_t){ ctx.buf + from, to - from };
}

// == STRVIEW_T ====================================================

strview_t strv_init(const char *cstr) {
	return strv_init_len(cstr, cstr ? strlen(cstr) : 0);
}

strview_t strv_init_len(const char *buf, usize size) {
    return (strview_t){
        .buf = buf,
        .len = size,
    };
}

strview_t strv_init_str(str_t str) {
    return (strview_t){
        .buf = str.buf,
        .len = str.len
    };
}

bool strv_is_empty(strview_t ctx) {
    return ctx.len == 0 || !ctx.buf;
}

bool strv_equals(strview_t a, strview_t b) {
    return strv_compare(a, b) == 0;
}

int strv_compare(strview_t a, strview_t b) {
	// TODO unsinged underflow if a.len < b.len
    return a.len == b.len ?
        memcmp(a.buf, b.buf, a.len) :
        (int)(a.len - b.len);
}

usize strv_get_utf8_len(strview_t v) {
    usize len = 0;

    for (usize i = 0; i < v.len; ++i) {
        if ((v.buf[i] & 0xC0) != 0x80) {
            len++;
        }
    }

    return len;
}

char strv_front(strview_t ctx) {
    return ctx.len > 0 ? ctx.buf[0] : '\0';
}

char strv_back(strview_t ctx) {
    return ctx.len > 0 ? ctx.buf[ctx.len - 1] : '\0';
}

str16_t strv_to_str16(arena_t *arena, strview_t src) {
    return strv_os_to_str16(arena, src);
}

tstr_t strv_to_tstr(arena_t *arena, strview_t src) {
#if UNICODE
    return strv_to_str16(arena, src);
#else
	return str(arena, src);
#endif
}

str_t strv_to_upper(arena_t *arena, strview_t src) {
    str_t out = str(arena, src);
    str_upper(&out);
    return out;
}

str_t strv_to_lower(arena_t *arena, strview_t src) {
    str_t out = str(arena, src);
    str_lower(&out);
    return out;
}

strview_t strv_remove_prefix(strview_t ctx, usize n) {
    if (n > ctx.len) n = ctx.len;
    return (strview_t){
        .buf = ctx.buf + n,
        .len = ctx.len - n,
    };
}

strview_t strv_remove_suffix(strview_t ctx, usize n) {
    if (n > ctx.len) n = ctx.len;
    return (strview_t){
        .buf = ctx.buf,
        .len = ctx.len - n,
    };
}

strview_t strv_trim(strview_t ctx) {
	return strv_trim_left(strv_trim_right(ctx));
}

strview_t strv_trim_left(strview_t ctx) {
    strview_t out = ctx;
    for (usize i = 0; i < ctx.len; ++i) {
        char c = ctx.buf[i];
        if (c != ' ' && (c < '\t' || c > '\r')) {
            break;
        }
        out.buf++;
        out.len--;
    }
    return out;
}

strview_t strv_trim_right(strview_t ctx) {
    strview_t out = ctx;
    for (isize i = ctx.len - 1; i >= 0; --i) {
        char c = ctx.buf[i];
        if (c != ' ' && (c < '\t' || c > '\r')) {
            break;
        }
        out.len--;
    }
    return out;
}

strview_t strv_sub(strview_t ctx, usize from, usize to) {
    if (ctx.len == 0) return STRV_EMPTY;
    if (to > ctx.len) to = ctx.len;
    if (from > to) from = to;
    return (strview_t){ ctx.buf + from, to - from };
}

bool strv_starts_with(strview_t ctx, char c) {
    return ctx.len > 0 && ctx.buf[0] == c;
}

bool strv_starts_with_view(strview_t ctx, strview_t view) {
    return ctx.len >= view.len && memcmp(ctx.buf, view.buf, view.len) == 0;
}

bool strv_ends_with(strview_t ctx, char c) {
    return ctx.len > 0 && ctx.buf[ctx.len - 1] == c;
}

bool strv_ends_with_view(strview_t ctx, strview_t view) {
    return ctx.len >= view.len && memcmp(ctx.buf + ctx.len - view.len, view.buf, view.len) == 0;
}

bool strv_contains(strview_t ctx, char c) {
    for(usize i = 0; i < ctx.len; ++i) {
        if(ctx.buf[i] == c) {
            return true;
        }
    }
    return false;
}

bool strv_contains_view(strview_t ctx, strview_t view) {
    if (ctx.len < view.len) return false;
    usize end = (ctx.len - view.len) + 1;

    for (usize i = 0; i < end; ++i) {
        if (memcmp(ctx.buf + i, view.buf, view.len) == 0) {
            return true;
        }
    }
    return false;
}

bool strv_contains_either(strview_t ctx, strview_t chars) {
    for (usize i = 0; i < ctx.len; ++i) {
        if (strv_contains(chars, ctx.buf[i])) {
            return true;
        }
    }

    return false;
}

usize strv_find(strview_t ctx, char c, usize from) {
    for (usize i = from; i < ctx.len; ++i) {
        if (ctx.buf[i] == c) {
            return i;
        }
    }
    return STR_NONE;
}

usize strv_find_view(strview_t ctx, strview_t view, usize from) {
    if (view.len > ctx.len) return STR_NONE;

    usize end = (ctx.len - view.len) + 1;

    for (usize i = from; i < end; ++i) {
        if (memcmp(ctx.buf + i, view.buf, view.len) == 0) {
            return i;
        }
    }
    return STR_NONE;
}

usize strv_find_either(strview_t ctx, strview_t chars, usize from) {
    if (from > ctx.len) from = ctx.len;
    
    for (usize i = from; i < ctx.len; ++i) {
        if (strv_contains(chars, ctx.buf[i])) {
            return i;
        }
    }

    return STR_NONE;
}

usize strv_rfind(strview_t ctx, char c, usize from_right) {
    if (ctx.len == 0) return STR_NONE;
    if (from_right > ctx.len) from_right = ctx.len;
    isize end = (isize)(ctx.len - from_right);
    for (isize i = end - 1; i >= 0; --i) {
        if (ctx.buf[i] == c) {
            return (usize)i;
        }
    }
    return STR_NONE;
}

usize strv_rfind_view(strview_t ctx, strview_t view, usize from_right) {
    if (ctx.len == 0) return STR_NONE;
    if (from_right > ctx.len) from_right = ctx.len;
    isize end = (isize)(ctx.len - from_right);
    if (end < (isize)view.len) return STR_NONE;
    for (isize i = end - view.len; i >= 0; --i) {
        if (memcmp(ctx.buf + i, view.buf, view.len) == 0) {
            return (usize)i;
        }
    }
    return STR_NONE;
}

// == CTYPE ========================================================

bool char_is_space(char c) {
    return (c >= '\t' && c <= '\r') || c == ' ';
}

bool char_is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool char_is_num(char c) {
    return c >= '0' && c <= '9';
}

bool char_is_hex(char c) {
    c = char_lower(c);
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

char char_lower(char c) {
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

char char_upper(char c) {
    return c <= 'a' && c >= 'z' ? c - 32 : c;
}

// == INPUT STREAM =================================================

instream_t istr_init(strview_t str) {
	return (instream_t) {
		.beg = str.buf,
        .cur = str.buf,
        .len = str.len,
	};
}

char istr_get(instream_t *ctx) {
    return istr_remaining(ctx) ? *ctx->cur++ : '\0';
}

char istr_peek(instream_t *ctx) {
    return istr_remaining(ctx) ? *ctx->cur : '\0';
}

char istr_peek_next(instream_t *ctx) {
    return istr_remaining(ctx) > 1 ? *(ctx->cur + 1) : '\0';
}

char istr_prev(instream_t *ctx) {
    return istr_tell(ctx) ? *(ctx->cur - 1) : '\0';
}

char istr_prev_prev(instream_t *ctx) {
    return istr_tell(ctx) > 1 ? *(ctx->cur - 2) : '\0';
}

void istr_ignore(instream_t *ctx, char delim) {
    while (!istr_is_finished(ctx) && *ctx->cur != delim) {
        ctx->cur++;
    }
}

void istr_ignore_and_skip(instream_t *ctx, char delim) {
    istr_ignore(ctx, delim);
    istr_skip(ctx, 1);
}

void istr_skip(instream_t *ctx, usize n) {
    if (!ctx) return;
    usize rem = istr_remaining(ctx);
    if (n > rem) n = rem;
    ctx->cur += n;
}

void istr_skip_whitespace(instream_t *ctx) {
    while (!istr_is_finished(ctx) && char_is_space(*ctx->cur)) {
        ctx->cur++;
    }
}

void istr_rewind(instream_t *ctx) {
    if (ctx) ctx->cur = ctx->beg;
}

void istr_rewind_n(instream_t *ctx, usize amount) {
    if (!ctx) return;
    usize rem = istr_remaining(ctx);
    ctx->cur -= MIN(amount, rem);
}

usize istr_tell(instream_t *ctx) {
    return ctx ? ctx->cur - ctx->beg : 0;
}

usize istr_remaining(instream_t *ctx) {
    return ctx ? ctx->len - (ctx->cur - ctx->beg) : 0;
}
 
bool istr_is_finished(instream_t *ctx) {
    return !(ctx && istr_remaining(ctx) > 0);
}

bool istr_get_bool(instream_t *ctx, bool *val) {
    if (!ctx || !ctx->cur || !val) return false;
    usize rem = istr_remaining(ctx);
    if (rem >= 4 && memcmp(ctx->cur, "true", 4) == 0) {
        *val = true;
        ctx->cur += 4;
        return true;
    }
    if (rem >= 5 && memcmp(ctx->cur, "false", 5) == 0) {
        *val = false;
        ctx->cur += 5;
        return true;
    }
    return false;
}

bool istr_get_u8(instream_t *ctx, u8 *val) {
    u64 out = 0;
    bool result = istr_get_u64(ctx, &out);
    if (result && out < UINT8_MAX) {
        *val = (u8)out;
    }
    return result;
}

bool istr_get_u16(instream_t *ctx, u16 *val) {
    u64 out = 0;
    bool result = istr_get_u64(ctx, &out);
    if (result && out < UINT16_MAX) {
        *val = (u16)out;
    }
    return result;
}

bool istr_get_u32(instream_t *ctx, u32 *val) {
    u64 out = 0;
    bool result = istr_get_u64(ctx, &out);
    if (result && out < UINT32_MAX) {
        *val = (u32)out;
    }
    return result;
}


bool istr_get_u64(instream_t *ctx, u64 *val) {
    if (!ctx || !ctx->cur || !val) return false;
    char *end = NULL;
    *val = strtoull(ctx->cur, &end, 0);

    if (ctx->cur == end) {
        return false;
    }
    else if (*val == ULLONG_MAX) {
        return false;
    }

    ctx->cur = end;
    return true;
}

bool istr_get_i8(instream_t *ctx, i8 *val) {
    i64 out = 0;
    bool result = istr_get_i64(ctx, &out);
    if (result && out > INT8_MIN && out < INT8_MAX) {
        *val = (i8)out;
    }
    return result;
}

bool istr_get_i16(instream_t *ctx, i16 *val) {
    i64 out = 0;
    bool result = istr_get_i64(ctx, &out);
    if (result && out > INT16_MIN && out < INT16_MAX) {
        *val = (i16)out;
    }
    return result;
}

bool istr_get_i32(instream_t *ctx, i32 *val) {
    i64 out = 0;
    bool result = istr_get_i64(ctx, &out);
    if (result && out > INT32_MIN && out < INT32_MAX) {
        *val = (i32)out;
    }
    return result;
}

bool istr_get_i64(instream_t *ctx, i64 *val) {
    if (!ctx || !ctx->cur || !val) return false;
    char *end = NULL;
    i64 out = strtoll(ctx->cur, &end, 0);

    if (ctx->cur == end) {
        return false;
    }
    else if(*val == INT64_MAX || *val == INT64_MIN) {
        return false;
    }

    ctx->cur = end;
    *val = out;
    return true;
}

bool istr_get_num(instream_t *ctx, double *val) {
    if (!ctx || !ctx->cur || !val) return false;
    char *end = NULL;
    *val = strtod(ctx->cur, &end);
    
    if(ctx->cur == end) {
        warn("istrGetDouble: no valid conversion could be performed (%.5s)", ctx->cur);
        return false;
    }
    else if(*val == HUGE_VAL || *val == -HUGE_VAL) {
        warn("istrGetDouble: value read is out of the range of representable values");
        return false;
    }

    ctx->cur = end;
    return true;
}

bool istr_get_float(instream_t *ctx, float *val) {
    double v = 0;
    if (!istr_get_num(ctx, &v)) {
        return false;
    }
    if (v >= HUGE_VALF || v <= -HUGE_VALF) {
        return false;
    }
    *val = (float)v;
    return true;
}

strview_t istr_get_view(instream_t *ctx, char delim) {
    if (!ctx || !ctx->cur) return STRV_EMPTY;
    const char *from = ctx->cur;
    istr_ignore(ctx, delim);
    usize len = ctx->cur - from;
    return strv(from, len);
}

strview_t istr_get_view_either(instream_t *ctx, strview_t chars) {
    if (!ctx || !ctx->cur) return STRV_EMPTY;
    const char *from = ctx->cur;
    while (!istr_is_finished(ctx) && !strv_contains(chars, *ctx->cur)) {
        ctx->cur++;
    }

    usize len = ctx->cur - from;
    return strv(from, len);
}

strview_t istr_get_view_len(instream_t *ctx, usize len) {
    if (!ctx || !ctx->cur) return STRV_EMPTY;
    const char *from = ctx->cur;
    istr_skip(ctx, len);
    usize buflen = ctx->cur - from;
    return (strview_t){ from, buflen };
}

strview_t istr_get_line(instream_t *ctx) {
    strview_t line = istr_get_view(ctx, '\n');
    istr_skip(ctx, 1);
    if (strv_ends_with(line, '\r')) {
        line = strv_remove_suffix(line, 1);
    }
    return line;
}

strview_t istr_get_word(instream_t *ctx) {
    strview_t word = istr_get_view_either(ctx, strv(" \t\v\r\n"));
    return word;
}

// == OUTPUT STREAM ================================================

outstream_t ostr_init(arena_t *exclusive_arena) {
    return (outstream_t) {
        .beg = (char *)(exclusive_arena ? exclusive_arena->cur : NULL),
        .arena = exclusive_arena,
    };
}

void ostr_clear(outstream_t *ctx) {
    arena_pop(ctx->arena, ostr_tell(ctx));
}

usize ostr_tell(outstream_t *ctx) {
    return ctx->arena ? (char *)ctx->arena->cur - ctx->beg : 0;
}

char ostr_back(outstream_t *ctx) {
    usize len = ostr_tell(ctx);
    return len ? ctx->beg[len - 1] : '\0';
}

str_t ostr_to_str(outstream_t *ctx) {
    ostr_putc(ctx, '\0');

    usize len = ostr_tell(ctx);

    str_t out = {
        .buf = ctx->beg,
        .len = len ? len - 1 : 0,
    };

    memset(ctx, 0, sizeof(outstream_t));
    return out;
}

strview_t ostr_as_view(outstream_t *ctx) {
    return strv(ctx->beg, ostr_tell(ctx));
}

void ostr_rewind(outstream_t *ctx, usize from_beg) {
    if (!ctx->arena) return;
    ctx->arena->cur = (u8*)ctx->beg + from_beg;
}

void ostr_pop(outstream_t *ctx, usize count) {
    if (!ctx->arena) return;
    arena_pop(ctx->arena, count);
}

void ostr_print(outstream_t *ctx, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    ostr_printv(ctx, fmt, args);
    va_end(args);
}

void ostr_printv(outstream_t *ctx, const char *fmt, va_list args) {
    if (!ctx->arena) return;
    str_fmtv(ctx->arena, fmt, args);
    // remove null terminator
    arena_pop(ctx->arena, 1);
}

void ostr_putc(outstream_t *ctx, char c) {
    if (!ctx->arena) return;
    char *newc = alloc(ctx->arena, char);
    *newc = c;
}

void ostr_puts(outstream_t *ctx, strview_t v) {
    if (strv_is_empty(v)) return;
    str(ctx->arena, v);
    // remove null terminator
    arena_pop(ctx->arena, 1);
}

void ostr_append_bool(outstream_t *ctx, bool val) {
    ostr_puts(ctx, val ? strv("true") : strv("false"));
}

void ostr_append_uint(outstream_t *ctx, u64 val) {
    ostr_print(ctx, "%I64u", val);
}

void ostr_append_int(outstream_t *ctx, i64 val) {
    ostr_print(ctx, "%I64d", val);
}

void ostr_append_num(outstream_t *ctx, double val) {
    ostr_print(ctx, "%g", val);
}

// == INPUT BINARY STREAM ==========================================

ibstream_t ibstr_init(buffer_t buffer) {
    return (ibstream_t){
        .beg = buffer.data,
        .cur = buffer.data,
        .len = buffer.len,
    };
}

bool ibstr_is_finished(ibstream_t *ib) {
    return !(ib && ibstr_remaining(ib) > 0);
}

usize ibstr_tell(ibstream_t *ib) {
    return ib && ib->cur ? ib->cur - ib->beg : 0;
}

usize ibstr_remaining(ibstream_t *ib) {
    return ib ? ib->len - ibstr_tell(ib) : 0;
}

usize ibstr_read(ibstream_t *ib, void *buffer, usize len) {
    usize rem = ibstr_remaining(ib);
    if (len > rem) len = rem;
    memmove(buffer, ib->cur, len);
    ib->cur += len;
    return len;
}

void ibstr_skip(ibstream_t *ib, usize count) {
    usize rem = ibstr_remaining(ib);
    if (count > rem) count = rem;
    ib->cur += count;
}

bool ibstr_get_u8(ibstream_t *ib, u8 *out) {
    return ibstr_read(ib, out, sizeof(*out)) == sizeof(*out);
}

bool ibstr_get_u16(ibstream_t *ib, u16 *out) {
    return ibstr_read(ib, out, sizeof(*out)) == sizeof(*out);
}

bool ibstr_get_u32(ibstream_t *ib, u32 *out) {
    return ibstr_read(ib, out, sizeof(*out)) == sizeof(*out);
}

bool ibstr_get_u64(ibstream_t *ib, u64 *out) {
    return ibstr_read(ib, out, sizeof(*out)) == sizeof(*out);
}

bool ibstr_get_i8(ibstream_t *ib, i8 *out) {
    return ibstr_read(ib, out, sizeof(*out)) == sizeof(*out);
}

bool ibstr_get_i16(ibstream_t *ib, i16 *out) {
    return ibstr_read(ib, out, sizeof(*out)) == sizeof(*out);
}

bool ibstr_get_i32(ibstream_t *ib, i32 *out) {
    return ibstr_read(ib, out, sizeof(*out)) == sizeof(*out);
}

bool ibstr_get_i64(ibstream_t *ib, i64 *out) {
    return ibstr_read(ib, out, sizeof(*out)) == sizeof(*out);
}

// == REGEX ========================================================

// adapted from rob pike regular expression matcher

bool rg__match_here(instream_t r, instream_t t);

bool rg__match_star(char c, instream_t r, instream_t t) {
    do {
        if (rg__match_here(r, t)) {
            return true;
        }
    } while (!istr_is_finished(&t) && (istr_get(&t) == c || c == '.'));
    return false;
}

bool rg__match_here(instream_t r, instream_t t) {
    char rc  = istr_peek(&r);
    char rcn = istr_peek_next(&r);
    if (rc == '\0') {
        return true;
    }
    if (rcn == '*') {
        istr_skip(&r, 2);
        return rg__match_star(rc, r, t);
    }
    if (rc == '$' && rcn == '\0') {
        return istr_peek(&t) == '\0';
    }
    if (!istr_is_finished(&t) && (rc == '.' || rc == istr_peek(&t))) {
        istr_skip(&r, 1);
        istr_skip(&t, 1);
        return rg__match_here(r, t);
    }
    return false;
}

bool rg__matches_impl(instream_t r, instream_t t) {
    do {
        if (rg__match_here(r, t)) {
            return true;
        }
    } while (istr_get(&t) != '\0');
    return false;
}

bool rg_matches(strview_t rg, strview_t text) {
    if (strv_contains(rg, '*')) {
        instream_t r = istr_init(rg);
        instream_t t = istr_init(text);
        return rg__matches_impl(r, t);
    }
    else {
        return strv_equals(rg, text);
    }
}

///////////////////////////////////////////////////
// glob has the following special characters:
//  - * matches any string
//  - ? matches any character
//  - [ start a match group
//    - cannot be empty, so this matches either
//      ] or [:
//      [][]
//      []]
//    - if theres a - between two characters, it
//      matches a range:
//      [A-Za-z0-9] is equal to 
//      [ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789]
//    - if there's a !, it negates the match,
//      so [!abc] matches anything but a, b, or c
//    - if there's a + after the last square bracket,
//      it matches 1+ times

// g: abc*_d?f[0-9]+c
// t: abcdef_def901c
// ---------------- 
// g[0] == t[0] -> ++g, ++t
// g: bc*_d?f[0-9]+c
// t: bcdef_def901c
// ---------------- 
// g[0] == t[0] -> ++g, ++t
// g: c*_d?f[0-9]+c
// t: cdef_def901c
// ---------------- 
// g[0] == t[0] -> ++g, ++t
// g: *_d?f[0-9]+c
// t: def_def901c
// ---------------- 
// g[0] == * -> ++g, star match
//   g: _d?f[0-9]+c
//   t: def_def901c
//   c = _
// ---------------- 
//   g[0] != t[0]
//   star: ++t
//   g: _d?f[0-9]+c
//   t: ef_def901c
// ---------------- 
//   g[0] != t[0]
//   star: ++t
//   g: _d?f[0-9]+c
//   t: f_def901c
// ---------------- 
//   g[0] != t[0]
//   star: ++t
//   g: _d?f[0-9]+c
//   t: _def901c
// ---------------- 
//   g[0] == t[0]
//   ++g, ++t
//   g: d?f[0-9]+c
//   t: def901c
//   return true from star
// ---------------- 
// g[0] == t[0] -> ++g, ++t
// g: ?f[0-9]+c
// t: ef901c
// ---------------- 
// g[0] == ? -> ++g, ++t
// g: f[0-9]+c
// t: f901c
// ---------------- 
// g[0] == t[0] -> ++g, ++t
// g: [0-9]+c
// t: 901c
// ---------------- 
// g[0] == [
// begin square:
//   -- grab pattern
//   s = strv_empty 
//   while (s == strv_empty)
//      s += g.getUntil(])
//   matches_multi = g.peek() == '+'
//   s: 0-9 
//   -- parse pattern
//   type: match
//   multi: matches_multi
//   ranges[MAX_RANGES] = {
//      { from: 0, to: 9 }
//   }
// ---------------- 
//   matched_once = false
//   do {
//     if (!s.inRange(t[0])) {
//        break;
//     }
//     matched_once true
//     ++t
//   } while (s.multi);
//   return matched_atleast_once
// ---------------- 
//     t: 901c
//     t[0] == 9 -> in range = true
//     ++t
//     t: 01c
//     t[0] == 0 -> in rage
//     ++t
//     t: 1c
//     t[0] == 1 -> in rage
//     ++t
//     t: c
//     t[0] == 1 -> not in rage
//     return true
// ---------------- 
// g: c
// t: c
// ---------------- 
// g[0] == t[0] -> ++g, ++t
// g:
// t:
// ---------------- 
// g is empty -> return true
//

bool glob__match_here(instream_t *g, instream_t *t);

bool glob__match_star(instream_t *g, instream_t *t) {
    char c = istr_get(g);
    do {
        if (istr_get(t) == c) {
            return true;
        }
    } while (!istr_is_finished(t));
    return false;
}

#define GLOB_MAX_RANGES 128

typedef struct {
    char from;
    char to;
} glob_range_t;

typedef struct {
    bool multi;
    bool exclude;
    glob_range_t ranges[GLOB_MAX_RANGES];
    int range_count;
} glob_pat_t;

bool glob__pat_is_in_range(glob_pat_t *pat, char c) {
    for (int i = 0; i < pat->range_count; ++i) {
        if (c >= pat->ranges[i].from &&
            c <= pat->ranges[i].to
        ) {
            return true;
        }
    }
    return false;
}

bool glob__match_pattern(instream_t *t, strview_t pat, bool multi) {
    glob_pat_t p = {
        .multi = multi,
        .exclude = pat.buf[0] == '!',
    };
    for (usize i = 0; i < pat.len; ++i) {
        char from = '\0', to = '\0';
        if (i > 0 && pat.buf[i] == '-' && (i + 1) < pat.len) {
            from = pat.buf[i - 1];
            to = pat.buf[i + 1];
            ++i;
        }
        else if ((i + 1) >= pat.len || pat.buf[i+1] != '-') {
            from = to = pat.buf[i];
        }
        if (from && to) {
            p.ranges[p.range_count++] = (glob_range_t){ from, to };
        }
    }

    bool matched_atleast_once = false;
    do {
        char c = istr_peek(t);
        bool is_in_range = glob__pat_is_in_range(&p, c);
        if ((!is_in_range && !p.exclude) || (is_in_range && p.exclude)) {
            break;
        }
        matched_atleast_once = true;
        istr_skip(t, 1);
    } while (p.multi);

    return matched_atleast_once;
}

bool glob__match_here(instream_t *g, instream_t *t) {
    char gc  = istr_peek(g);
    if (gc == '*') {
        istr_skip(g, 1);
        if (istr_is_finished(g)) {
            // set t (text) to empty, as the rest of the patter is sure to match
            *t = istr_init(STRV_EMPTY);
            return true;
        }
        return glob__match_star(g, t);
    }
    if (gc == '[') {
        // skip [
        istr_skip(g, 1);
        strview_t pattern = istr_get_view(g, ']');
        if (pattern.len == 0) {
            istr_skip(g, 1);
            pattern = istr_get_view(g, ']');
            // add first ]
            pattern.buf--;
            pattern.len++;
        }
        // skip ]
        istr_skip(g, 1);
        bool multi = false;
        if (istr_peek(g) == '+') {
            istr_skip(g, 1);
            multi = true;
        }
        return glob__match_pattern(t, pattern, multi);
    }
    if (!istr_is_finished(t) && (gc == '?' || gc == istr_peek(t))) {
        istr_skip(g, 1);
        istr_skip(t, 1);
        return true;
    }

    return false;
}

bool glob__impl(instream_t *g, instream_t *t) {
    while (!istr_is_finished(g) && !istr_is_finished(t)) {
        if (!glob__match_here(g, t)) {
            return false;
        }
    }
    return istr_get(g) == '\0' && istr_get(t) == '\0';
}

bool glob_matches(strview_t glob, strview_t text) {
    instream_t g = istr_init(glob);
    instream_t t = istr_init(text);
    return glob__impl(&g, &t);
}

// == ARENA ========================================================

static uptr arena__align(uptr ptr, usize align) {
    return (ptr + (align - 1)) & ~(align - 1);
}

static arena_t arena__make_virtual(usize size);
static arena_t arena__make_malloc(usize size);
static arena_t arena__make_static(u8 *buf, usize len);

static void *arena__alloc_common(const arena_alloc_desc_t *desc);
static void *arena__alloc_malloc_always(const arena_alloc_desc_t *desc);

static void arena__free_virtual(arena_t *arena);
static void arena__free_malloc(arena_t *arena);

arena_t malloc_arena = {
    .type = ARENA_MALLOC_ALWAYS,
};

arena_t arena_init(const arena_desc_t *desc) {
    arena_t out = {0};

    if (desc) {
        switch (desc->type) {
            case ARENA_VIRTUAL:       out = arena__make_virtual(desc->size); break;
            case ARENA_MALLOC:        out = arena__make_malloc(desc->size); break;
            case ARENA_STATIC:        out = arena__make_static(desc->static_buffer, desc->size); break;
            case ARENA_MALLOC_ALWAYS: out = malloc_arena; break;
		    default: break;  
        }
    }

    return out;
}

void arena_cleanup(arena_t *arena) {
    if (!arena) {
        return;
    }
    
    switch (arena->type) {
        case ARENA_VIRTUAL: arena__free_virtual(arena); break;
        case ARENA_MALLOC:  arena__free_malloc(arena);  break;
        // ARENA_STATIC does not need to be freed
        default: break;  
    }
    
    memset(arena, 0, sizeof(arena_t));
}

arena_t arena_scratch(arena_t *arena, usize size) {
    u8 *buffer = alloc(arena, u8, size, ALLOC_SOFT_FAIL | ALLOC_NOZERO);
    return arena__make_static(buffer, buffer ? size : 0);
}

void *arena_alloc(const arena_alloc_desc_t *desc) {
    if (!desc || !desc->arena || desc->arena->type == ARENA_TYPE_NONE) {
        return NULL;
    }

    arena_t *arena = desc->arena;

    u8 *ptr = NULL;

    switch (arena->type) {
        case ARENA_MALLOC_ALWAYS:
            ptr = arena__alloc_malloc_always(desc);
            break;
        default:
            ptr = arena__alloc_common(desc);
            break; 
    }

    if (!ptr && desc->flags & ALLOC_SOFT_FAIL) {
        return NULL;
    }

    usize total = desc->size * desc->count;

    return desc->flags & ALLOC_NOZERO ? ptr : memset(ptr, 0, total);
}

usize arena_tell(arena_t *arena) {
    return arena ? arena->cur - arena->beg : 0;
}

usize arena_remaining(arena_t *arena) {
    return arena && (arena->cur < arena->end) ? arena->end - arena->cur : 0;
}

usize arena_capacity(arena_t *arena) {
	return arena ? arena->end - arena->beg : 0;
}

void arena_rewind(arena_t *arena, usize from_start) {
    if (!arena) {
        return;
    }

    colla_assert(arena_tell(arena) >= from_start);

    arena->cur = arena->beg + from_start;
}

void arena_pop(arena_t *arena, usize amount) {
    if (!arena) {
        return;
    }
    usize position = arena_tell(arena);
    if (!position) {
        return;
    }
    arena_rewind(arena, position - amount);
}

// == VIRTUAL ARENA ====================================================================================================

static arena_t arena__make_virtual(usize size) {
    usize alloc_size = 0;
    u8 *ptr = os_reserve(size, &alloc_size);
    if (!os_commit(ptr, 1)) {
        os_release(ptr, alloc_size);
        ptr = NULL;
    }

    return (arena_t){
        .beg = ptr,
        .cur = ptr,
        .end = ptr ? ptr + alloc_size : NULL,
        .type = ARENA_VIRTUAL,
    };
}

static void arena__free_virtual(arena_t *arena) {
    if (!arena->beg) {
        return;
    }

    os_release(arena->beg, arena_capacity(arena));
}

// == MALLOC ARENA =====================================================================================================

static arena_t arena__make_malloc(usize size) {
    u8 *ptr = os_alloc(size);
    colla_assert(ptr);
    return (arena_t) {
        .beg = ptr,
        .cur = ptr,
        .end = ptr ? ptr + size : NULL,
        .type = ARENA_MALLOC,
    };
}

static void arena__free_malloc(arena_t *arena) {
    os_free(arena->beg);
}

// == ARENA ALLOC ======================================================================================================

static void *arena__alloc_common(const arena_alloc_desc_t *desc) {
    usize total = desc->size * desc->count;
    arena_t *arena = desc->arena;

    arena->cur = (u8 *)arena__align((uptr)arena->cur, desc->align);
    bool soft_fail = desc->flags & ALLOC_SOFT_FAIL;

    if (total > arena_remaining(arena)) {
        if (!soft_fail) {
            fatal("finished space in arena, tried to allocate %_$$$dB out of %_$$$dB (total: %_$$$dB)\n", total, arena_remaining(arena), (usize)(arena->end - arena->beg));
        }
        return NULL;
    }

    if (arena->type == ARENA_VIRTUAL) {
        usize allocated = arena_tell(arena);
        usize page_end = os_pad_to_page(allocated);
        usize new_cur = allocated + total;

        if (new_cur > page_end) {
            usize page_size = os_get_system_info().page_size;
            usize prev_page = os_pad_to_page(allocated ? allocated - page_size : 0);
            usize next_page = os_pad_to_page(new_cur);
            usize num_of_pages = (next_page - prev_page) / page_size;
            colla_assert(num_of_pages > 0);

            if (!os_commit(arena->beg + prev_page, num_of_pages)) {
                if (!soft_fail) {
                    fatal("failed to commit memory for virtual arena, tried to commit %zu pages\n", num_of_pages);
                }
                return NULL;
            }
        }
    }

    u8 *ptr = arena->cur;
    arena->cur += total;

    return ptr;
}

static void *arena__alloc_malloc_always(const arena_alloc_desc_t *desc) {
    usize total = desc->size * desc->count;

    u8 *ptr = os_alloc(total);
    if (!ptr && !(desc->flags & ALLOC_SOFT_FAIL)) {
        fatal("alloc call failed for %_$$$dB", total);
    }

    return ptr;
}

// == STATIC ARENA =====================================================================================================

static arena_t arena__make_static(u8 *buf, usize len) {
    return (arena_t) {
        .beg = buf,
        .cur = buf,
        .end = buf ? buf + len : NULL,
        .type = ARENA_STATIC,
    };
}

// == HANDLE ====================================

oshandle_t os_handle_zero(void) {
	return (oshandle_t){0};
}

bool os_handle_match(oshandle_t a, oshandle_t b) {
	return a.data == b.data;
}

bool os_handle_valid(oshandle_t handle) {
	return !os_handle_match(handle, os_handle_zero());
}

// == LOGGING ===================================

const char* os_log_level_strings[LOG_COL__COUNT] = {
    [LOG_DEBUG] = "[DEBUG]:",
    [LOG_INFO]  = "[INFO]:",
    [LOG_WARN]  = "[WARN]:",
    [LOG_ERR]   = "[ERR]:",
    [LOG_FATAL] = "[FATAL]:",
};

os_log_colour_e os_log_level_colours[LOG_COL__COUNT] = {
    [LOG_DEBUG] = LOG_COL_BLUE,
    [LOG_INFO]  = LOG_COL_GREEN,
    [LOG_WARN]  = LOG_COL_YELLOW,
    [LOG_ERR]   = LOG_COL_RED,
    [LOG_FATAL] = LOG_COL_RED,
};

os_log_options_e os__log_opts = OS_LOG_SIMPLE;
log_callback_t os__log_cbs[COLLA_LOG_MAX_CALLBACKS] = {0};
i64 os__log_cbs_count = 0;

void os_log__stdout(log_event_t *ev) {
    bool notime = os__log_opts & OS_LOG_NOTIME;
    bool nofile = os__log_opts & OS_LOG_NOFILE;

    if (!notime) {
        os_log_set_colour(LOG_COL_DARK_GREY);
        fmt_print(
            "%02d:%02d:%02d ", 
            ev->time->tm_hour, 
            ev->time->tm_min, 
            ev->time->tm_sec
        );
        os_log_set_colour(LOG_COL_RESET);
    }

    if (ev->level != LOG_BASIC) {
        os_log_set_colour(os_log_level_colours[ev->level]);
        fmt_print("%-8s ", os_log_level_strings[ev->level]);
        os_log_set_colour(LOG_COL_RESET);
    }

    if (!nofile) {
        os_log_set_colour(LOG_COL_DARK_GREY);
        fmt_print("%s:%d ", ev->file, ev->line);
        os_log_set_colour(LOG_COL_RESET);
    }

    fmt_printv(ev->fmt, ev->args);
    fmt_print("\n");
}

void os_log__fp(log_event_t *ev) {
    u8 tmpbuf[KB(1)] = {0};
    arena_t scratch = arena_make(ARENA_STATIC, sizeof(tmpbuf), tmpbuf);

    oshandle_t fp = {0};
    fp.data = (uptr)ev->udata;

    bool notime = os__log_opts & OS_LOG_NOTIME;
    bool nofile = os__log_opts & OS_LOG_NOFILE;

    if (!notime) {
        os_file_print(
            scratch, 
            fp, 
            "%02d:%02d:%02d ", 
            ev->time->tm_hour, 
            ev->time->tm_min, 
            ev->time->tm_sec
        );
    }

    if (ev->level != LOG_BASIC) {
        os_file_print(scratch, fp, "%-8s ", os_log_level_strings[ev->level]);
    }

    if (!nofile) {
        os_file_print(scratch, fp, "%s:%d ", ev->file, ev->line);
    }

    os_file_printv(scratch, fp, ev->fmt, ev->args);
    os_file_putc(fp, '\n');
}

void os_log_set_options(os_log_options_e opt) {
    os__log_opts = opt;
}

os_log_options_e os_log_get_options(void) {
    return os__log_opts;
}

void os_log_add_callback(log_callback_t cb) {
    colla_assert(os__log_cbs_count < arrlen(os__log_cbs));
    os__log_cbs[os__log_cbs_count++] = cb;
}

void os_log_add_fp(oshandle_t fp, os_log_level_e level) {
    os_log_add_callback((log_callback_t){ 
        .fn = os_log__fp, 
        .udata = (void*)fp.data, 
        .level = level 
    });
}

void os__log_init(void) {
    os_log_add_callback((log_callback_t){ .fn = os_log__stdout });
}

void os_log_print(const char *file, int line, os_log_level_e level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    os_log_printv(file, line, level, fmt, args);
    va_end(args);
}

void os_log_printv(const char *file, int line, os_log_level_e level, const char *fmt, va_list args) {
    time_t t = time(NULL);
    struct tm log_time = { 0 };
#define gettime(src, dst) localtime_s(&dst, &src)
    log_event_t ev = {
        .level = level,
        .line = line,
        .args = args,
        .fmt = fmt,
        .file = file,
        .time = &log_time,
    };

    localtime_s(&log_time, &t);
    for (int i = 0; i < os__log_cbs_count; ++i) {
        if (os__log_cbs[i].level > level) {
            continue;
        }
        ev.udata = os__log_cbs[i].udata;
        os__log_cbs[i].fn(&ev);
    }

    bool nocrash = os__log_opts & OS_LOG_NOCRASH;

    if (!nocrash && level == LOG_FATAL) {
        os_abort(1);
    }
}

// == FILE ======================================

void os_file_split_path(strview_t path, strview_t *dir, strview_t *name, strview_t *ext) {
	usize dir_lin = strv_rfind(path, '/', 0);
	usize dir_win = strv_rfind(path, '\\', 0);
	dir_lin = dir_lin != STR_NONE ? dir_lin : 0;
	dir_win = dir_win != STR_NONE ? dir_win : 0;
	usize dir_pos = MAX(dir_lin, dir_win);

	usize ext_pos = strv_rfind(path, '.', 0);

	if (dir) {
		*dir = strv_sub(path, 0, dir_pos);
	}
	if (name) {
		*name = strv_sub(path, dir_pos ? dir_pos + 1 : 0, ext_pos);
	}
	if (ext) {
		*ext = strv_sub(path, ext_pos, SIZE_MAX);
	}
}

bool os_file_putc(oshandle_t handle, char c) {
	return os_file_write(handle, &c, sizeof(c)) == sizeof(c);
}

bool os_file_puts(oshandle_t handle, strview_t str) {
	return os_file_write(handle, str.buf, str.len) == str.len;
}

bool os_file_print(arena_t scratch, oshandle_t handle, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	bool result = os_file_printv(scratch, handle, fmt, args);
	va_end(args);
	return result;
}	

bool os_file_printv(arena_t scratch, oshandle_t handle, const char *fmt, va_list args) {
	str_t s = str_fmtv(&scratch, fmt, args);
	return os_file_puts(handle, strv(s));
}

usize os_file_read_buf(oshandle_t handle, buffer_t *buf) {
	return os_file_read(handle, buf->data, buf->len);
}

usize os_file_write_buf(oshandle_t handle, buffer_t buf) {
	return os_file_write(handle, buf.data, buf.len);
}

buffer_t os_file_read_all(arena_t *arena, strview_t path) {
	oshandle_t fp = os_file_open(path, OS_FILE_READ);
	if (!os_handle_valid(fp)) {
		err("could not open file: %v", path);
		return (buffer_t){0};
	}
	buffer_t out = os_file_read_all_fp(arena, fp);
	os_file_close(fp);
	return out;
}

buffer_t os_file_read_all_fp(arena_t *arena, oshandle_t handle) {
	if (!os_handle_valid(handle)) return (buffer_t){0};
	buffer_t out = {0};
	
	out.len = os_file_size(handle);
	out.data = alloc(arena, u8, out.len);
	usize read = os_file_read_buf(handle, &out);
	
	if (read != out.len) {
		err("os_file_read_all_fp: read failed, should be %zu but is %zu", out.len, read);
		arena_pop(arena, out.len);
		return (buffer_t){0};
	}
	
	return out;
}

str_t os_file_read_all_str(arena_t *arena, strview_t path) {
	oshandle_t fp = os_file_open(path, OS_FILE_READ);
	if (!os_handle_valid(fp)) {
		err("could not open file %v: %v", path, os_get_error_string(os_get_last_error()));
		return STR_EMPTY;
	}
	str_t out = os_file_read_all_str_fp(arena, fp);
	os_file_close(fp);
	return out;
}

str_t os_file_read_all_str_fp(arena_t *arena, oshandle_t handle) {
	if (!os_handle_valid(handle)) {
		return STR_EMPTY;	
	}
	
	str_t out = STR_EMPTY;
	
	out.len = os_file_size(handle);
	out.buf = alloc(arena, u8, out.len + 1);
	
	usize read = os_file_read(handle, out.buf, out.len);
	if (read != out.len) {
		err("os_file_read_all_str_fp: read failed, should be %zu but is %zu", out.len, read);
		arena_pop(arena, out.len + 1);	
		return STR_EMPTY;
	}
	
	return out;
}

bool os_file_write_all(strview_t name, buffer_t buffer) {
	oshandle_t fp = os_file_open(name, OS_FILE_WRITE);
	bool result = os_file_write_all_fp(fp, buffer);
	os_file_close(fp);
	return result;
}

bool os_file_write_all_fp(oshandle_t handle, buffer_t buffer) {
	return os_file_write(handle, buffer.data, buffer.len) == buffer.len;
}

bool os_file_write_all_str(strview_t name, strview_t data) {
	oshandle_t fp = os_file_open(name, OS_FILE_WRITE);
	bool result = os_file_write_all_str_fp(fp, data);
	os_file_close(fp);
	return result;
}

bool os_file_write_all_str_fp(oshandle_t handle, strview_t data) {
	return os_file_write(handle, data.buf, data.len) == data.len;
}

u64 os_file_time(strview_t path) {
	oshandle_t fp = os_file_open(path, OS_FILE_READ);
	u64 result = os_file_time_fp(fp);
	os_file_close(fp);
	return result;
}

bool os_file_has_changed(strview_t path, u64 last_change) {
	u64 timestamp = os_file_time(path);
	return timestamp > last_change;
}

// == PROCESS ===================================

bool os_run_cmd(arena_t scratch, os_cmd_t *cmd, os_cmd_options_t *options) {
	oshandle_t proc = os_run_cmd_async(scratch, cmd, options);
	return os_handle_valid(proc) ? os_process_wait(proc, OS_WAIT_INFINITE, NULL) : false;
}

// == VMEM ======================================

usize os_pad_to_page(usize byte_count) {
	usize page_size = os_get_system_info().page_size;

    if (byte_count == 0) {
        return page_size;
    }

    usize padding = page_size - (byte_count & (page_size - 1));
    if (padding == page_size) {
        padding = 0;
    }
    return byte_count + padding;
}

// == THREAD ====================================

void os_barrier_sync(os_barrier_t *b) {
    atomic_inc_i64(&b->thread_value);

    while (true) {
        if (atomic_cmp_i64(&b->has_completed, 1, 1)) {
            break;
        }
        i64 completed = 
            atomic_cmp_i64(
                &b->thread_value, 
                b->thread_count, 
                b->thread_count
            ) == b->thread_count;
        if (completed) {
            atomic_set_i64(&b->has_completed, completed);
            break;
        }
    }

    if (atomic_dec_i64(&b->thread_value) == 0) {
        b->has_completed = 0;
    }
}

i64range_t os_lane_range(u64 values_count) {
    i64 thread_id = os_thread_id;
    i64 thread_count = os_thread_count;

    i64 values_per_thread = values_count / thread_count;
    i64 leftover_values_count = values_count % thread_count;
    bool thread_has_leftover = thread_id < leftover_values_count;
    i64 leftover_before_this_thread_id = thread_has_leftover ?
        thread_id : leftover_values_count;
    i64 thread_first_value = 
        values_per_thread * thread_id + leftover_before_this_thread_id;
    i64 thread_last_value =
        thread_first_value + values_per_thread + thread_has_leftover;

    return (i64range_t){ thread_first_value, thread_last_value };
}

#if !COLLA_NO_CONDITION_VARIABLE

// == JOB QUEUE =================================

int jq__worker_function(u64 thread_id, void *udata) {
    COLLA_UNUSED(thread_id);
    job_queue_t *q = udata;

    // TODO: is this safe to not have an atomic variable?
    while (!q->should_stop) {
        job_t *job = jq_pop_job(q);
        if (!job) {
            if (q->stop_when_finished) {
                break;
            }
            continue;
        }
        job->func(job->userdata);
        os_mutex_lock(q->mutex);
            list_push(q->freelist, job);
        os_mutex_unlock(q->mutex);
    }

    return 0;
}

job_queue_t *jq_init(arena_t *arena, int worker_count) {
    job_queue_t *q = alloc(arena, job_queue_t);
    q->mutex = os_mutex_create();
    q->condvar = os_cond_create();
    q->thread_count = worker_count ? worker_count : os_get_system_info().processor_count;
    q->threads = alloc(arena, oshandle_t, q->thread_count);

    for (int i = 0; i < q->thread_count; ++i) {
        q->threads[i] = os_thread_launch(jq__worker_function, q);
    }

    return q;
}

void jq_stop(job_queue_t *queue) {
    os_mutex_lock(queue->mutex);
        job_t *remaining = queue->jobs;
        list_push(queue->freelist, remaining);
        queue->jobs = NULL;
        queue->should_stop = true;
        os_cond_broadcast(queue->condvar);
    os_mutex_unlock(queue->mutex);

    for (int i = 0; i < queue->thread_count; ++i) {
        os_thread_join(queue->threads[i], NULL);
    }

    os_mutex_free(queue->mutex);
    os_cond_free(queue->condvar);
}

void jq_cleanup(job_queue_t *queue) {
    os_mutex_lock(queue->mutex);
        queue->stop_when_finished = true;
        os_cond_broadcast(queue->condvar);
    os_mutex_unlock(queue->mutex);

    for (int i = 0; i < queue->thread_count; ++i) {
        os_thread_join(queue->threads[i], NULL);
    }

    os_mutex_free(queue->mutex);
    os_cond_free(queue->condvar);
}

void jq_push(arena_t *arena, job_queue_t *queue, job_func_f *func, void *userdata) {
    os_mutex_lock(queue->mutex);
        job_t *job = queue->freelist;
        list_pop(queue->freelist);
        if (!job) {
            job = alloc(arena, job_t);
        }
        job->func = func;
        job->userdata = userdata;
        list_push(queue->jobs, job);
        os_cond_broadcast(queue->condvar);
    os_mutex_unlock(queue->mutex);
}

job_t *jq_pop_job(job_queue_t *queue) {
    job_t *job = NULL;
    os_mutex_lock(queue->mutex);
        while (!queue->jobs && !queue->should_stop && !queue->stop_when_finished) {
            os_cond_wait(queue->condvar, queue->mutex, OS_WAIT_INFINITE);
        }
        job = queue->jobs ;
        list_pop(queue->jobs);
    os_mutex_unlock(queue->mutex);
    return job;
}

#endif

// == INI ============================================

void ini__parse(arena_t *arena, ini_t *ini, const iniopt_t *options);

ini_t ini_parse(arena_t *arena, strview_t filename, iniopt_t *opt) {
    oshandle_t fp = os_file_open(filename, OS_FILE_READ);
    ini_t out = ini_parse_fp(arena, fp, opt);
    os_file_close(fp);
    return out;
}

ini_t ini_parse_fp(arena_t *arena, oshandle_t file, iniopt_t *opt) {
    str_t data = os_file_read_all_str_fp(arena, file);
    return ini_parse_str(arena, strv(data), opt);
}

ini_t ini_parse_str(arena_t *arena, strview_t str, iniopt_t *opt) {
    ini_t out = {
        .text = str,
        .tables = NULL,
    };
    ini__parse(arena, &out, opt);
    return out;
}

bool ini_is_valid(ini_t *ini) {
    return ini && !strv_is_empty(ini->text);
}

initable_t *ini_get_table(ini_t *ini, strview_t name) {
    initable_t *t = ini ? ini->tables : NULL;
    while (t) {
        if (strv_equals(t->name, name)) {
            return t;
        }
        t = t->next;
    }
    return NULL;
}

inivalue_t *ini_get(initable_t *table, strview_t key) {
    inivalue_t *v = table ? table->values : NULL;
    while (v) {
        if (strv_equals(v->key, key)) {
            return v;
        }
        v = v->next;
    }
    return NULL;
}

iniarray_t ini_as_arr(arena_t *arena, inivalue_t *value, char delim) {
    strview_t v = value ? value->value : STRV_EMPTY;
    if (!delim) delim = ' ';

    strview_t *beg = (strview_t *)arena->cur;
    usize count = 0;

    usize start = 0;
    for (usize i = 0; i < v.len; ++i) {
        if (v.buf[i] == delim) {
            strview_t arrval = strv_trim(strv_sub(v, start, i));
            if (!strv_is_empty(arrval)) {
                strview_t *newval = alloc(arena, strview_t);
                *newval = arrval;
                ++count;
            }
            start = i + 1;
        }
    }

    strview_t last = strv_trim(strv_sub(v, start, SIZE_MAX));
    if (!strv_is_empty(last)) {
        strview_t *newval = alloc(arena, strview_t);
        *newval = last;
        ++count;
    }

    return (iniarray_t){
        .values = beg,
        .count = count,
    };
}

u64 ini_as_uint(inivalue_t *value) {
    strview_t v = value ? value->value : STRV_EMPTY;
    instream_t in = istr_init(v);
    u64 out = 0;
    if (!istr_get_u64(&in, &out)) {
        out = 0;
    }
    return out;
}

i64 ini_as_int(inivalue_t *value) {
    strview_t v = value ? value->value : STRV_EMPTY;
    instream_t in = istr_init(v);
    i64 out = 0;
    if (!istr_get_i64(&in, &out)) {
        out = 0;
    }
    return out;
}

double ini_as_num(inivalue_t *value) {
    strview_t v = value ? value->value : STRV_EMPTY;
    instream_t in = istr_init(v);
    double out = 0;
    if (!istr_get_num(&in, &out)) {
        out = 0;
    }
    return out;
}

bool ini_as_bool(inivalue_t *value) {
    strview_t v = value ? value->value : STRV_EMPTY;
    instream_t in = istr_init(v);
    bool out = 0;
    if (!istr_get_bool(&in, &out)) {
        out = 0;
    }
    return out;
}

void ini_pretty_print(ini_t *ini, const ini_pretty_opts_t *options) {
    ini_pretty_opts_t opt = {0};
    if (options) {
        memmove(&opt, options, sizeof(ini_pretty_opts_t));
    }

    if (!os_handle_valid(opt.custom_target)) {
        opt.custom_target = os_stdout();
    }

    if (!opt.use_custom_colours) {
        os_log_colour_e default_col[INI_PRETTY_COLOUR__COUNT] = {
            LOG_COL_YELLOW, // INI_PRETTY_COLOUR_KEY,
            LOG_COL_GREEN,  // INI_PRETTY_COLOUR_VALUE,
            LOG_COL_WHITE,  // INI_PRETTY_COLOUR_DIVIDER,
            LOG_COL_RED,    // INI_PRETTY_COLOUR_TABLE,
        };
        memmove(opt.colours, default_col, sizeof(default_col));
    }

    for_each (t, ini->tables) {
        if (!strv_equals(t->name, INI_ROOT)) {
            os_log_set_colour(opt.colours[INI_PRETTY_COLOUR_TABLE]);
            os_file_puts(opt.custom_target, strv("["));
            os_file_puts(opt.custom_target, t->name);
            os_file_puts(opt.custom_target, strv("]\n"));
        }

        for_each (pair, t->values) {
            if (strv_is_empty(pair->key) || strv_is_empty(pair->value)) continue;
            os_log_set_colour(opt.colours[INI_PRETTY_COLOUR_KEY]);
            os_file_puts(opt.custom_target, pair->key);

            os_log_set_colour(opt.colours[INI_PRETTY_COLOUR_DIVIDER]);
            os_file_puts(opt.custom_target, strv(" = "));

            os_log_set_colour(opt.colours[INI_PRETTY_COLOUR_VALUE]);
            os_file_puts(opt.custom_target, pair->value);
            
            os_file_puts(opt.custom_target, strv("\n"));
        }
    }

    os_log_set_colour(LOG_COL_RESET);
}

///// ini-private ////////////////////////////////////

iniopt_t ini__get_options(const iniopt_t *options) {
    iniopt_t out = {
        .key_value_divider = '=',
        .comment_vals = strv(";#"),
    };

#define SETOPT(v) out.v = options->v ? options->v : out.v

    if (options) {
        SETOPT(key_value_divider);
        SETOPT(merge_duplicate_keys);
        SETOPT(merge_duplicate_tables);
        out.comment_vals = strv_is_empty(options->comment_vals) ? out.comment_vals : options->comment_vals;
    }

#undef SETOPT

    return out;
}

void ini__add_value(arena_t *arena, initable_t *table, instream_t *in, iniopt_t *opts) {
    colla_assert(table);

    strview_t key = strv_trim(istr_get_view(in, opts->key_value_divider));
    istr_skip(in, 1);

    strview_t value = strv_trim(istr_get_view(in, '\n'));
    usize comment_pos = strv_find_either(value, opts->comment_vals, 0);
    if (comment_pos != STR_NONE) {
        value = strv_sub(value, 0, comment_pos);
    }
    istr_skip(in, 1);
    inivalue_t *newval = NULL;
    
    if (opts->merge_duplicate_keys) {
        newval = table->values;
        while (newval) {
            if (strv_equals(newval->key, key)) {
                break;
            }
            newval = newval->next;
        }
    }

    if (newval) {
        newval->value = value;
    }
    else {
        newval = alloc(arena, inivalue_t);
        newval->key = key;
        newval->value = value;

        if (!table->values) {
            table->values = newval;
        }
        else {
            table->tail->next = newval;
        }

        table->tail = newval;
    }
}

void ini__add_table(arena_t *arena, ini_t *ctx, instream_t *in, iniopt_t *options) {
    istr_skip(in, 1); // skip [
    strview_t name = istr_get_view(in, ']');
    istr_skip(in, 1); // skip ]
    initable_t *table = NULL;

    if (options->merge_duplicate_tables) {
        table = ctx->tables;
        while (table) {
            if (strv_equals(table->name, name)) {
                break;
            }
            table = table->next;
        }
    }

    if (!table) {
        table = alloc(arena, initable_t);

        table->name = name;

        if (!ctx->tables) {
            ctx->tables = table;
        }
        else {
            ctx->tail->next = table;
        }

        ctx->tail = table;
    }

    istr_ignore_and_skip(in, '\n');
    while (!istr_is_finished(in)) {
        switch (istr_peek(in)) {
            case '\n': // fallthrough
            case '\r':
                return;
            case '#':  // fallthrough
            case ';':
                istr_ignore_and_skip(in, '\n');
                break;
            default:
                ini__add_value(arena, table, in, options);
                break;
        }
    }
}

void ini__parse(arena_t *arena, ini_t *ini, const iniopt_t *options) {
    iniopt_t opts = ini__get_options(options);

    initable_t *root = alloc(arena, initable_t);
    root->name = INI_ROOT;
    ini->tables = root;
    ini->tail = root;

    instream_t in = istr_init(ini->text);

    while (!istr_is_finished(&in)) {
        istr_skip_whitespace(&in);
        switch (istr_peek(&in)) {
            case '[':
                ini__add_table(arena, ini, &in, &opts);
                break;
            case '#': // fallthrough
            case ';':
                istr_ignore_and_skip(&in, '\n');
                break;
            default:
                ini__add_value(arena, ini->tables, &in, &opts);
                break;
        }
    }
}

// == JSON ===========================================

bool json__parse_obj(arena_t *arena, instream_t *in, jsonflags_e flags, json_t **out);
bool json__parse_value(arena_t *arena, instream_t *in, jsonflags_e flags, json_t **out);

json_t *json_parse(arena_t *arena, strview_t filename, jsonflags_e flags) {
    str_t data = os_file_read_all_str(arena, filename);
    return json_parse_str(arena, strv(data), flags);
}

json_t *json_parse_str(arena_t *arena, strview_t str, jsonflags_e flags) {
    arena_t before = *arena;

    json_t *root = alloc(arena, json_t);
    root->type = JSON_OBJECT;

    instream_t in = istr_init(str);

    if (flags & JSON_ONLY_OBJECT_START) {
        if (!json__parse_obj(arena, &in, flags, &root->object)) {
            // reset arena
            *arena = before;
            return NULL;
        }
    }
    else {
        if (!json__parse_value(arena, &in, flags, &root)) {
            *arena = before;
            return NULL;
        }
    }
    
    return root;
}

json_t *json_get(json_t *node, strview_t key) {
    if (!node) return NULL;

    if (node->type != JSON_OBJECT) {
        return NULL;
    }

    node = node->object;

    while (node) {
        if (strv_equals(node->key, key)) {
            return node;
        }
        node = node->next;
    }

    return NULL;
}

void json__pretty_print_value(json_t *value, int indent, const json_pretty_opts_t *options);

void json_pretty_print(json_t *root, const json_pretty_opts_t *options) {
    json_pretty_opts_t default_options = { 0 };
    if (options) {
        memmove(&default_options, options, sizeof(json_pretty_opts_t));
    }

    if (!os_handle_valid(default_options.custom_target)) {
        default_options.custom_target = os_stdout();
    }
    if (!default_options.use_custom_colours) {
        os_log_colour_e default_col[JSON_PRETTY_COLOUR__COUNT] = {
            LOG_COL_YELLOW,    // JSON_PRETTY_COLOUR_KEY,
            LOG_COL_CYAN,      // JSON_PRETTY_COLOUR_STRING,
            LOG_COL_BLUE,      // JSON_PRETTY_COLOUR_NUM,
            LOG_COL_DARK_GREY, // JSON_PRETTY_COLOUR_NULL,
            LOG_COL_GREEN,     // JSON_PRETTY_COLOUR_TRUE,
            LOG_COL_RED,       // JSON_PRETTY_COLOUR_FALSE,
        };
        memmove(default_options.colours, default_col, sizeof(default_col));
    }

    json__pretty_print_value(root, 0, &default_options);
    os_file_putc(default_options.custom_target, '\n');
}

///// json-private ///////////////////////////////////

#define json__ensure(c) json__check_char(in, c)

bool json__check_char(instream_t *in, char c) {
    if (istr_get(in) == c) {
        return true;
    }
    istr_rewind_n(in, 1);
    return false;
}

bool json__is_value_finished(instream_t *in) {
    usize old_pos = istr_tell(in);
    
    istr_skip_whitespace(in);
    switch(istr_peek(in)) {
        case '}': // fallthrough
        case ']': // fallthrough
        case ',':
            return true;
    }

    in->cur = in->beg + old_pos;
    return false;
}

bool json__parse_null(instream_t *in) {
    strview_t null_view = istr_get_view_len(in, 4);
    bool is_valid = true;
    
    if (!strv_equals(null_view, strv("null"))) {
        is_valid = false;
    }

    if (!json__is_value_finished(in)) {
        is_valid = false;
    }

    return is_valid;
}

bool json__parse_array(arena_t *arena, instream_t *in, jsonflags_e flags, json_t **out) {
    json_t *head = NULL;
    
    if (!json__ensure('[')) {
        goto fail;
    }

    istr_skip_whitespace(in);

    // if it is an empty array
    if (istr_peek(in) == ']') {
        istr_skip(in, 1);
        goto success;
    }
    
    if (!json__parse_value(arena, in, flags, &head)) {
        goto fail;
    }

    json_t *cur = head;
    
    while (true) {
        istr_skip_whitespace(in);
        switch (istr_get(in)) {
            case ']':
                goto success;
            case ',':
            {
                istr_skip_whitespace(in);
                // trailing comma
                if (istr_peek(in) == ']') {
                    if (flags & JSON_NO_TRAILING_COMMAS) {
                        goto fail;
                    }
                    else {
                        continue;
                    }
                }

                json_t *next = NULL;
                if (!json__parse_value(arena, in, flags, &next)) {
                    goto fail;
                }
                cur->next = next;
                next->prev = cur;
                cur = next;
                break;
            }
            default:
                istr_rewind_n(in, 1);
                goto fail;
        }
    }

success:
    *out = head;
    return true;
fail:
    *out = NULL;
    return false;
}

bool json__parse_string(arena_t *arena, instream_t *in, strview_t *out) {
    COLLA_UNUSED(arena);
    *out = STRV_EMPTY;

    istr_skip_whitespace(in); 

    if (!json__ensure('"')) {
        goto fail;
    }

    const char *from = in->cur;
    
    for (; !istr_is_finished(in) && *in->cur != '"'; ++in->cur) {
        if (istr_peek(in) == '\\') {
            ++in->cur;
        }
    }
    
    usize len = in->cur - from;

    *out = strv(from, len);

    if (!json__ensure('"')) {
        goto fail;
    }

    return true;
fail:
    return false;
}

bool json__parse_pair(arena_t *arena, instream_t *in, jsonflags_e flags, json_t **out) {
    strview_t key = {0};
    if (!json__parse_string(arena, in, &key)) {
        goto fail;
    }

    // skip preamble
    istr_skip_whitespace(in);
    if (!json__ensure(':')) {
        goto fail;
    }

    if (!json__parse_value(arena, in, flags, out)) {
        goto fail;
    }
    
    (*out)->key = key;
    return true;

fail: 
    *out = NULL;
    return false;
}

bool json__parse_obj(arena_t *arena, instream_t *in, jsonflags_e flags, json_t **out) {
    if (!json__ensure('{')) {
        goto fail;
    }

    istr_skip_whitespace(in);

    // if it is an empty object
    if (istr_peek(in) == '}') {
        istr_skip(in, 1);
        *out = NULL;
        return true;
    }

    json_t *head = NULL;
    if (!json__parse_pair(arena, in, flags, &head)) {
        goto fail;
    }
    json_t *cur = head;

    while (true) {
        istr_skip_whitespace(in);
        switch (istr_get(in)) {
            case '}':
                goto success;
            case ',':
            {
                istr_skip_whitespace(in);
                // trailing commas
                if (!(flags & JSON_NO_TRAILING_COMMAS) && istr_peek(in) == '}') {
                    goto success;
                }

                json_t *next = NULL;
                if (!json__parse_pair(arena, in, flags, &next)) {
                    goto fail;
                }
                cur->next = next;
                next->prev = cur;
                cur = next;
                break;
            }
            default:
                istr_rewind_n(in, 1);
                goto fail;
        }
    }

success:
    *out = head;
    return true;
fail:
    *out = NULL;
    return false;
}

bool json__parse_value(arena_t *arena, instream_t *in, jsonflags_e flags, json_t **out) {
    json_t *val = alloc(arena, json_t);

    istr_skip_whitespace(in);

    switch (istr_peek(in)) {
        // object
        case '{':
            if (!json__parse_obj(arena, in, flags, &val->object)) {
                goto fail;
            }
            val->type = JSON_OBJECT;
            break;
        // array
        case '[':
            if (!json__parse_array(arena, in, flags, &val->array)) {
                goto fail;
            }
            val->type = JSON_ARRAY;
            break;
        // string
        case '"':
            if (!json__parse_string(arena, in, &val->string)) {
                goto fail;
            }
            val->type = JSON_STRING;
            break;
        // boolean
        case 't': // fallthrough
        case 'f':
            if (!istr_get_bool(in, &val->boolean)) {
                goto fail;
            }
            val->type = JSON_BOOL;
            break;
        // null
        case 'n': 
            if (!json__parse_null(in)) {
                goto fail;
            }
            val->type = JSON_NULL;
            break;
        // comment
        case '/':
            err("TODO comments");
            break;
        // number
        default:
            if (!istr_get_num(in, &val->number)) {
                goto fail;
            }
            val->type = JSON_NUMBER;
            break;
    }

    *out = val;
    return true;
fail:
    *out = NULL;
    return false;
}

#undef json__ensure

#define JSON_PRETTY_INDENT(ind) for (int i = 0; i < ind; ++i) os_file_puts(options->custom_target, strv("    "))

void json__pretty_print_value(json_t *value, int indent, const json_pretty_opts_t *options) {
    switch (value->type) {
        case JSON_NULL:
            os_log_set_colour(options->colours[JSON_PRETTY_COLOUR_NULL]);
            os_file_puts(options->custom_target, strv("null"));
            os_log_set_colour(LOG_COL_RESET);
            break;
        case JSON_ARRAY:
            os_file_puts(options->custom_target, strv("[\n"));
            for_each (node, value->array) {
                JSON_PRETTY_INDENT(indent + 1);
                json__pretty_print_value(node, indent + 1, options);
                if (node->next) {
                    os_file_putc(options->custom_target, ',');
                }
                os_file_putc(options->custom_target, '\n');
            }
            JSON_PRETTY_INDENT(indent);
            os_file_putc(options->custom_target, ']');
            break;
        case JSON_STRING: 
            os_log_set_colour(options->colours[JSON_PRETTY_COLOUR_STRING]);
            os_file_putc(options->custom_target, '\"');
            os_file_puts(options->custom_target, value->string);
            os_file_putc(options->custom_target, '\"');
            os_log_set_colour(LOG_COL_RESET);
            break;
        case JSON_NUMBER:
        {
            os_log_set_colour(options->colours[JSON_PRETTY_COLOUR_NUM]);
            u8 scratchbuf[256];
            arena_t scratch = arena_make(ARENA_STATIC, sizeof(scratchbuf), scratchbuf);
            const char *fmt = "%g";
            if (round(value->number) == value->number) {
                fmt = "%.0f";
            }
            os_file_print(
                scratch, 
                options->custom_target, 
                fmt, 
                value->number
            );
            os_log_set_colour(LOG_COL_RESET);
            break;
        } 
        case JSON_BOOL:
            os_log_set_colour(options->colours[value->boolean ? JSON_PRETTY_COLOUR_TRUE : JSON_PRETTY_COLOUR_FALSE]);
            os_file_puts(options->custom_target, value->boolean ? strv("true") : strv("false"));
            os_log_set_colour(LOG_COL_RESET);
            break;
        case JSON_OBJECT:
            os_file_puts(options->custom_target, strv("{\n"));
            for_each(node, value->object) {
                JSON_PRETTY_INDENT(indent + 1);
                os_log_set_colour(options->colours[JSON_PRETTY_COLOUR_KEY]);
                os_file_putc(options->custom_target, '\"');
                os_file_puts(options->custom_target, node->key);
                os_file_putc(options->custom_target, '\"');
                os_log_set_colour(LOG_COL_RESET);

                os_file_puts(options->custom_target, strv(": "));

                json__pretty_print_value(node, indent + 1, options);
                if (node->next) {
                    os_file_putc(options->custom_target, ',');
                }
                os_file_putc(options->custom_target, '\n');
            }
            JSON_PRETTY_INDENT(indent);
            os_file_putc(options->custom_target, '}');
            break;
    }
}

#undef JSON_PRETTY_INDENT


// == XML ============================================

xmltag_t *xml__parse_tag(arena_t *arena, instream_t *in);

xml_t xml_parse(arena_t *arena, strview_t filename) {
    str_t str = os_file_read_all_str(arena, filename);
    return xml_parse_str(arena, strv(str));
}

xml_t xml_parse_str(arena_t *arena, strview_t xmlstr) {
    xml_t out = {
        .text = xmlstr,
        .root = alloc(arena, xmltag_t),
    };
    
    instream_t in = istr_init(xmlstr);

    while (!istr_is_finished(&in)) {
        xmltag_t *tag = xml__parse_tag(arena, &in);

        if (out.tail) out.tail->next = tag;
        else          out.root->child = tag;

        out.tail = tag;
    }

    return out;
}

xmltag_t *xml_get_tag(xmltag_t *parent, strview_t key, bool recursive) {
    xmltag_t *t = parent ? parent->child : NULL;
    while (t) {
        if (strv_equals(key, t->key)) {
            return t;
        }
        if (recursive && t->child) {
            xmltag_t *out = xml_get_tag(t, key, recursive);
            if (out) {
                return out;
            }
        }
        t = t->next;
    }
    return NULL;
}

strview_t xml_get_attribute(xmltag_t *tag, strview_t key) {
    xmlattr_t *a = tag ? tag->attributes : NULL;
    while (a) {
        if (strv_equals(key, a->key)) {
            return a->value;
        }
        a = a->next;
    }
    return STRV_EMPTY;
}

///// xml-private ////////////////////////////////////

xmlattr_t *xml__parse_attr(arena_t *arena, instream_t *in) {
    if (istr_peek(in) != ' ') {
        return NULL;
    }

    strview_t key = strv_trim(istr_get_view(in, '='));
    istr_skip(in, 1); // skip =
    strview_t val = strv_trim(istr_get_view_either(in, strv("\">")));
    if (istr_peek(in) != '>') {
        istr_skip(in, 1); // skip "
    }
    
    if (strv_is_empty(key) || strv_is_empty(val)) {
        warn("key or value empty");
        return NULL;
    }
    
    xmlattr_t *attr = alloc(arena, xmlattr_t);
    attr->key = key;
    attr->value = val;
    return attr;
}

xmltag_t *xml__parse_tag(arena_t *arena, instream_t *in) {
    istr_skip_whitespace(in);

    // we're either parsing the body, or we have finished the object
    if (istr_peek(in) != '<' || istr_peek_next(in) == '/') {
        return NULL;
    }

    istr_skip(in, 1); // skip <

    // meta tag, we don't care about these
    if (istr_peek(in) == '?') {
        istr_ignore_and_skip(in, '\n');
        return NULL;
    }

    xmltag_t *tag = alloc(arena, xmltag_t);

    tag->key = strv_trim(istr_get_view_either(in, strv(" >")));

    xmlattr_t *attr = xml__parse_attr(arena, in);
    while (attr) {
        attr->next = tag->attributes;
        tag->attributes = attr;
        attr = xml__parse_attr(arena, in);
    }

    // this tag does not have children, return
    if (istr_peek(in) == '/') {
        istr_skip(in, 2); // skip / and >
        return tag;
    }

    istr_skip(in, 1); // skip >

    xmltag_t *child = xml__parse_tag(arena, in);
    while (child) {
        if (tag->tail) {
            tag->tail->next = child;
            tag->tail = child;
        }
        else {
            tag->child = tag->tail = child;
        }
        child = xml__parse_tag(arena, in);
    }

    // parse content
    istr_skip_whitespace(in);
    tag->content = istr_get_view(in, '<');

    // closing tag
    istr_skip(in, 2); // skip < and /
    strview_t closing = strv_trim(istr_get_view(in, '>'));
    if (!strv_equals(tag->key, closing)) {
        warn("opening and closing tags are different!: (%v) != (%v)", tag->key, closing);
    }
    istr_skip(in, 1); // skip >
    return tag;
}

// == HTML ===========================================

htmltag_t *html__parse_tag(arena_t *arena, instream_t *in);

html_t html_parse(arena_t *arena, strview_t filename) {
   str_t str = os_file_read_all_str(arena, filename);
   return html_parse_str(arena, strv(str));
}

html_t html_parse_str(arena_t *arena, strview_t str) {
    html_t out = {
        .text = str,
        .root = alloc(arena, xmltag_t),
    };

    instream_t in = istr_init(str);

    while (!istr_is_finished(&in)) {
        htmltag_t *tag = html__parse_tag(arena, &in);

        if (out.tail) out.tail->next = tag;
        else          out.root->children = tag;

        out.tail = tag;
    }

    return out;
}

htmltag_t *html__get_tag_internal(htmltag_t *parent, str_t key, bool recursive) {
    htmltag_t *t = parent ? parent->children : NULL;
    while (t) {
        if (str_equals(key, t->key)) {
            return t;
        }
        if (recursive && t->children) {
            htmltag_t *out = html__get_tag_internal(t, key, recursive);
            if (out) {
                return out;
            }
        }
        t = t->next;
    }
    return NULL;
}

htmltag_t *html_get_tag(htmltag_t *parent, strview_t key, bool recursive) {
    u8 tmpbuf[KB(1)];
    arena_t scratch = arena_make(ARENA_STATIC, sizeof(tmpbuf), tmpbuf);
    str_t upper = strv_to_upper(&scratch, key);
    return html__get_tag_internal(parent, upper, recursive);
}

strview_t html_get_attribute(htmltag_t *tag, strview_t key) {
    xmlattr_t *a = tag ? tag->attributes : NULL;
    while (a) {
        if (strv_equals(key, a->key)) {
            return a->value;
        }
        a = a->next;
    }
    return STRV_EMPTY;
}

///// html-private ///////////////////////////////////

/*

special rules:
 <p> tag does not need to be closed when followed by
    address, article, aside, blockquote, details, dialog, div,
    dl, fieldset, figcaption, figure, footer, form, h1, h2, h3,
    h4, h5, h6, header, hgroup, hr, main, menu, nav, ol, p, pre,
    search, section, table, or ul
*/

strview_t html_closing_p_tags[] = {
    cstrv("ADDRESS"),
    cstrv("ARTICLE"),
    cstrv("ASIDE"),
    cstrv("BLOCKQUOTE"),
    cstrv("DETAILS"),
    cstrv("DIALOG"),
    cstrv("DIV"),
    cstrv("DL"),
    cstrv("FIELDSET"),
    cstrv("FIGCAPTION"),
    cstrv("FIGURE"),
    cstrv("FOOTER"),
    cstrv("FORM"),
    cstrv("H1"),
    cstrv("H2"),
    cstrv("H3"),
    cstrv("H4"),
    cstrv("H5"),
    cstrv("H6"),
    cstrv("HEADER"),
    cstrv("HGROUP"),
    cstrv("HR"),
    cstrv("MAIN"),
    cstrv("MENU"),
    cstrv("NAV"),
    cstrv("OL"),
    cstrv("P"),
    cstrv("PRE"),
    cstrv("SEARCH"),
    cstrv("SECTION"),
    cstrv("TABLE"),
    cstrv("UL"),
};

bool html__closes_p_tag(strview_t tag) {
    for (int i = 0; i < arrlen(html_closing_p_tags); ++i) {
        if (strv_equals(html_closing_p_tags[i], tag)) {
            return true;
        }
    }

    return false;
}

htmltag_t *html__parse_tag(arena_t *arena, instream_t *in) {
    istr_skip_whitespace(in);

    // we're either parsing the body, or we have finished the object
    if (istr_peek(in) != '<' || istr_peek_next(in) == '/') {
        return NULL;
    }

    istr_skip(in, 1); // skip <

    // meta tag, we don't care about these
    if (istr_peek(in) == '?') {
        istr_ignore_and_skip(in, '\n');
        return NULL;
    }

    htmltag_t *tag = alloc(arena, htmltag_t);

    tag->key = strv_to_upper(
        arena, 
        strv_trim(istr_get_view_either(in, strv(" >")))
    );

    xmlattr_t *attr = xml__parse_attr(arena, in);
    while (attr) {
        attr->next = tag->attributes;
        tag->attributes = attr;
        attr = xml__parse_attr(arena, in);
    }

    // this tag does not have children, return
    if (istr_peek(in) == '/') {
        istr_skip(in, 2); // skip / and >
        return tag;
    }

    istr_skip(in, 1); // skip >

    bool is_p_tag = strv_equals(strv(tag->key), strv("P"));
    while (!istr_is_finished(in)) {
        istr_skip_whitespace(in);
        strview_t content = strv_trim(istr_get_view(in, '<'));
    
        // skip <
        istr_skip(in, 1);
    
        bool is_closing = istr_peek(in) == '/';
        if (is_closing) {
            istr_skip(in, 1);
        }

 
        arena_t scratch = *arena;
        instream_t scratch_in = *in;
        str_t next_tag = strv_to_upper(&scratch, strv_trim(istr_get_view_either(&scratch_in, strv(" >"))));
        
        // rewind <
        istr_rewind_n(in, 1);

        // if we don't have children, it means this is the only content
        // otherwise, it means this is content in-between other tags,
        // if so: create an empty tag with the content and add it as a child
        if (!strv_is_empty(content)) {
            if (tag->children == NULL) {
                tag->content = content;
            }
            else {
                htmltag_t *empty = alloc(arena, htmltag_t);
                empty->content = content;
                olist_push(tag->children, tag->tail, empty);
            }
        }

        bool close_tag = 
            (is_closing && str_equals(tag->key, next_tag)) ||
            (is_p_tag   && html__closes_p_tag(strv(next_tag)));

        if (close_tag) {
            if (is_closing) {
                istr_skip(in, 2 + next_tag.len);
            }
            break;
        }

        htmltag_t *child = html__parse_tag(arena, in);
        if (tag->tail) {
            (tag->tail)->next = (child);
            (tag->tail) = (child);
        } 
        else {
            (tag->children) = (tag->tail) = (child);
        }
    }

    return tag;
}

const char *http_get_method_string(http_method_e method) {
    switch (method) {
        case HTTP_GET: return "GET";
        case HTTP_POST: return "POST";
        case HTTP_HEAD: return "HEAD";
        case HTTP_PUT: return "PUT";
        case HTTP_DELETE: return "DELETE"; 
    }
    return "GET";
}

const char *http_get_status_string(int status) {
    switch (status) {
        case 200: return "OK";              
        case 201: return "CREATED";         
        case 202: return "ACCEPTED";        
        case 204: return "NO CONTENT";      
        case 205: return "RESET CONTENT";   
        case 206: return "PARTIAL CONTENT"; 

        case 300: return "MULTIPLE CHOICES";    
        case 301: return "MOVED PERMANENTLY";   
        case 302: return "MOVED TEMPORARILY";   
        case 304: return "NOT MODIFIED";        

        case 400: return "BAD REQUEST";             
        case 401: return "UNAUTHORIZED";            
        case 403: return "FORBIDDEN";               
        case 404: return "NOT FOUND";               
        case 407: return "RANGE NOT SATISFIABLE";   

        case 500: return "INTERNAL SERVER ERROR";   
        case 501: return "NOT IMPLEMENTED";         
        case 502: return "BAD GATEWAY";             
        case 503: return "SERVICE NOT AVAILABLE";   
        case 504: return "GATEWAY TIMEOUT";         
        case 505: return "VERSION NOT SUPPORTED";   
    }
    
    return "UNKNOWN";
}

http_header_t *http__parse_headers_instream(arena_t *arena, instream_t *in) {
    http_header_t *head = NULL;

    while (!istr_is_finished(in)) {
        strview_t line = istr_get_line(in);
        
        // end of headers
        if (strv_is_empty(line)) {
            break;
        }

        usize pos = strv_find(line, ':', 0);
        if (pos != STR_NONE) {
            http_header_t *new_head = alloc(arena, http_header_t);

            new_head->key = strv_sub(line, 0, pos);
            new_head->value = strv_sub(line, pos + 2, SIZE_MAX);

            list_push(head, new_head);
        }
    }

    return head;
}

http_header_t *http_parse_headers(arena_t *arena, strview_t header_string) {
    instream_t in = istr_init(header_string);
    return http__parse_headers_instream(arena, &in);
}

http_req_t http_parse_req(arena_t *arena, strview_t request) {
    http_req_t req = {0};
    instream_t in = istr_init(request);

    strview_t method = strv_trim(istr_get_view(&in, '/'));
    istr_skip(&in, 1); // skip /
    req.url          = strv_trim(istr_get_view(&in, ' '));
    strview_t http   = strv_trim(istr_get_view(&in, '\n'));

    istr_skip(&in, 1); // skip \n

    req.headers = http__parse_headers_instream(arena, &in);

    req.body = strv_trim(istr_get_view_len(&in, SIZE_MAX));

    strview_t methods[5] = { strv("GET"), strv("POST"), strv("HEAD"), strv("PUT"), strv("DELETE") };
    usize methods_count = arrlen(methods);

    for (usize i = 0; i < methods_count; ++i) {
        if (strv_equals(method, methods[i])) {
            req.method = (http_method_e)i;
            break;
        }
    }

    in = istr_init(http);
    istr_ignore_and_skip(&in, '/'); // skip HTTP/
    istr_get_u8(&in, &req.version.major);
    istr_skip(&in, 1); // skip .
    istr_get_u8(&in, &req.version.minor);

    return req;
}

http_res_t http_parse_res(arena_t *arena, strview_t response) {
    http_res_t res = {0};
    instream_t in = istr_init(response);

    strview_t http = istr_get_view_len(&in, 4);
    if (!strv_equals(http, strv("HTTP"))) {
        err("response doesn't start with 'HTTP', instead with %v", http);
        return (http_res_t){0};
    }
    istr_skip(&in, 1); // skip /
    istr_get_u8(&in, &res.version.major);
    istr_skip(&in, 1); // skip .
    istr_get_u8(&in, &res.version.minor);
    istr_get_i32(&in, (i32*)&res.status_code);

    istr_ignore(&in, '\n');
    istr_skip(&in, 1); // skip \n

    res.headers = http__parse_headers_instream(arena, &in);

    strview_t encoding = http_get_header(res.headers, strv("transfer-encoding"));
    if (!strv_equals(encoding, strv("chunked"))) {
        res.body = istr_get_view_len(&in, SIZE_MAX);
    }
    else {
        err("chunked encoding not implemented yet! body ignored");
    }

    return res;
}

str_t http_req_to_str(arena_t *arena, http_req_t *req) {
    outstream_t out = ostr_init(arena);

    const char *method = NULL;
    switch (req->method) {
        case HTTP_GET:    method = "GET";       break;
        case HTTP_POST:   method = "POST";      break;
        case HTTP_HEAD:   method = "HEAD";      break;
        case HTTP_PUT:    method = "PUT";       break;
        case HTTP_DELETE: method = "DELETE";    break;
        default: err("unrecognised method: %d", method); return STR_EMPTY;
    }

    ostr_print(
        &out, 
        "%s /%v HTTP/%hhu.%hhu\r\n",
        method, req->url, req->version.major, req->version.minor
    );

    http_header_t *h = req->headers;
    while (h) {
        ostr_print(&out, "%v: %v\r\n", h->key, h->value);
        h = h->next;
    }

    ostr_puts(&out, strv("\r\n"));
    ostr_puts(&out, req->body);

    return ostr_to_str(&out);
}

str_t http_res_to_str(arena_t *arena, http_res_t *res) {
    outstream_t out = ostr_init(arena);

    ostr_print(
        &out,
        "HTTP/%hhu.%hhu %d %s\r\n",
        res->version.major, 
        res->version.minor,
        res->status_code, 
        http_get_status_string(res->status_code)
    );
    ostr_puts(&out, strv("\r\n"));
    ostr_puts(&out, res->body);

    return ostr_to_str(&out);
}

http_header_t *http_add_header(arena_t *arena, http_header_t *headers, strview_t key, strview_t value) {
    http_header_t *h = alloc(arena, http_header_t);
    h->key = key;
    h->value = value;
    list_push(headers, h);
    return headers;
}

bool http_has_header(http_header_t *headers, strview_t key) {
    for_each(h, headers) {
        if (strv_equals(h->key, key)) {
            return true;
        }
    }
    return false;
}

void http_set_header(http_header_t *headers, strview_t key, strview_t value) {
    http_header_t *h = headers;
    while (h) {
        if (strv_equals(h->key, key)) {
            h->value = value;
            break;
        }
        h = h->next;
    }
}

strview_t http_get_header(http_header_t *headers, strview_t key) {
    http_header_t *h = headers;
    while (h) {
        if (strv_equals(h->key, key)) {
            return h->value;
        }
        h = h->next;
    }
    return STRV_EMPTY;
}

str_t http_make_url_safe(arena_t *arena, strview_t string) {
    strview_t chars = strv(" !\"#$%%&'()*+,/:;=?@[]");
    usize final_len = string.len;

    // find final string length first
    for (usize i = 0; i < string.len; ++i) {
        if (strv_contains(chars, string.buf[i])) {
            final_len += 2;
        }
    }
    
    str_t out = {
        .buf = alloc(arena, char, final_len + 1),
        .len = final_len
    };
    usize cur = 0;
    // substitute characters
    for (usize i = 0; i < string.len; ++i) {
        if (strv_contains(chars, string.buf[i])) {
            fmt_buffer(out.buf + cur, 4, "%%%X", string.buf[i]);
            cur += 3;
        }
        else {
            out.buf[cur++] = string.buf[i];
        }
    }

    return out;
}

str_t http_decode_url_safe(arena_t *arena, strview_t string) {
    usize final_len = string.len;

    for (usize i = 0; i < string.len; ++i) {
        if (string.buf[i] == '%') {
            final_len -= 2;
            i += 2;
        }
    }

    colla_assert(final_len <= string.len);

    str_t out = {
        .buf = alloc(arena, char, final_len + 1),
        .len = final_len
    };

    usize k = 0;

    for (usize i = 0; i < string.len; ++i) {
        if (string.buf[i] == '%') {
            // skip %
            ++i;

            unsigned int ch = 0;
            int result = sscanf(string.buf + i, "%02X", &ch);
            if (result != 1 || ch > UINT8_MAX) {
                err("malformed url at %zu (%s)", i, string.buf + i);
                return STR_EMPTY;
            }
            out.buf[k++] = (char)ch;
            
            // skip first char of hex
            ++i;
        }
        else {
            out.buf[k++] = string.buf[i];
        }
    }

    return out;
}

http_url_t http_split_url(strview_t url) {
    http_url_t out = {0};

    if (strv_starts_with_view(url, strv("https://"))) {
        url = strv_remove_prefix(url, 8);
    }
    else if (strv_starts_with_view(url, strv("http://"))) {
        url = strv_remove_prefix(url, 7);
    }

    out.host = strv_sub(url, 0, strv_find(url, '/', 0));
    out.uri = strv_sub(url, out.host.len, SIZE_MAX);

    return out;
}

#if !COLLA_NO_NET
// WEBSOCKETS ///////////////////////

#define WEBSOCKET_MAGIC    "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WEBSOCKET_HTTP_KEY "Sec-WebSocket-Key"

bool websocket_init(arena_t scratch, socket_t socket, strview_t key) {
    str_t full_key = str_fmt(&scratch, "%v" WEBSOCKET_MAGIC, key);
    
    sha1_t sha1_ctx = sha1_init();
    sha1_result_t sha1_data = sha1(&sha1_ctx, full_key.buf, full_key.len);

    // convert to big endian for network communication
    for (int i = 0; i < 5; ++i) {
        sha1_data.digest[i] = htonl(sha1_data.digest[i]);
    }

    buffer_t encoded_key = base64_encode(&scratch, (buffer_t){ (u8 *)sha1_data.digest, sizeof(sha1_data.digest) });
    
    str_t response = str_fmt(
        &scratch,
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Connection: Upgrade\r\n"
        "Upgrade: websocket\r\n"
        "Sec-WebSocket-Accept: %v\r\n"
        "\r\n",
        encoded_key
    );

    int result = sk_send(socket, response.buf, (int)response.len);
    return result != SOCKET_ERROR;
}

buffer_t websocket_encode(arena_t *arena, strview_t message) {
    int extra = 6;
    if (message.len > UINT16_MAX)     extra += sizeof(u64);
    else if (message.len > UINT8_MAX) extra += sizeof(u16);
    u8 *bytes = alloc(arena, u8, message.len + extra);
    bytes[0] = 0x81; // 0b10000001
    bytes[1] = 0x80; // 0b10000000;
    int offset = 2;
    if (message.len > UINT16_MAX) {
        bytes[1] |= 0x7F; // 0b01111111;
        u64 len = htonll(message.len);
        memmove(bytes + 2, &len, sizeof(len));
        offset += sizeof(u64);
    }
    else if (message.len > UINT8_MAX) {
        bytes[1] |= 0x7E; // 0b01111110;
        u16 len = htons((u16)message.len);
        memmove(bytes + 2, &len, sizeof(len));
        offset += sizeof(u16);
    }
    else {
        bytes[1] |= (u8)message.len;
    }

    u32 mask = 0;
    memmove(bytes + offset, &mask, sizeof(mask));
    offset += sizeof(mask);
    memmove(bytes + offset, message.buf, message.len);

    return (buffer_t){ bytes, message.len + extra };
}

str_t websocket_decode(arena_t *arena, buffer_t message) {
    str_t out = STR_EMPTY;
    u8 *bytes = message.data;

    bool mask  = bytes[1] & 0x80; // 0b10000000;
    int offset = 2;
    u64 msglen = bytes[1] & 0x7F; // 0b01111111;

    // 16bit msg len
    if (msglen == 126) {
        u16 be_len = 0;
        memmove(&be_len, bytes + 2, sizeof(be_len));
        msglen = ntohs(be_len);
        offset += sizeof(u16);
    }
    // 64bit msg len
    else if (msglen == 127) {
        u64 be_len = 0;
        memmove(&be_len, bytes + 2, sizeof(be_len));
        msglen = ntohll(be_len);
        offset += sizeof(u64);
    }

    if (msglen == 0) {
        warn("message length = 0");
    }
    else if (mask) {
        u8 *decoded = alloc(arena, u8, msglen + 1);
        u8 masks[4] = {0};
        memmove(masks, bytes + offset, sizeof(masks));
        offset += 4;

        for (u64 i = 0; i < msglen; ++i) {
            decoded[i] = bytes[offset + i] ^ masks[i % 4];
        }

        out = (str_t){ (char *)decoded, msglen };
    }
    else {
        warn("mask bit not set!");
    }

    return out;
}

#endif

// SHA 1 ////////////////////////////

sha1_t sha1_init(void) {
    return (sha1_t) {
        .digest = {
			0x67452301,
			0xEFCDAB89,
			0x98BADCFE,
			0x10325476,
			0xC3D2E1F0,
        },
    };
}

u32 sha1__left_rotate(u32 value, u32 count) {
    return (value << count) ^ (value >> (32 - count));
}

void sha1__process_block(sha1_t *ctx) {
    u32 w [80];
    for (usize i = 0; i < 16; ++i) {
        w[i]  = ctx->block[i * 4 + 0] << 24;
        w[i] |= ctx->block[i * 4 + 1] << 16;
        w[i] |= ctx->block[i * 4 + 2] << 8;
        w[i] |= ctx->block[i * 4 + 3] << 0;
    }

    for (usize i = 16; i < 80; ++i) {
        w[i] = sha1__left_rotate(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    u32 a = ctx->digest[0];
    u32 b = ctx->digest[1];
    u32 c = ctx->digest[2];
    u32 d = ctx->digest[3];
    u32 e = ctx->digest[4];

    for (usize i = 0; i < 80; ++i) {
        u32 f = 0;
        u32 k = 0;

        if (i<20) {
            f = (b & c) | (~b & d);
            k = 0x5A827999;
        } else if (i<40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i<60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        u32 temp = sha1__left_rotate(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = sha1__left_rotate(b, 30);
        b = a;
        a = temp;
    }
	
    ctx->digest[0] += a;
    ctx->digest[1] += b;
    ctx->digest[2] += c;
    ctx->digest[3] += d;
    ctx->digest[4] += e;
}

void sha1__process_byte(sha1_t *ctx, u8 b) {
    ctx->block[ctx->block_index++] = b;
    ++ctx->byte_count;
    if (ctx->block_index == 64) {
        ctx->block_index = 0;
        sha1__process_block(ctx);
    }
}

sha1_result_t sha1(sha1_t *ctx, const void *buf, usize len) {
    const u8 *block = buf;

    for (usize i = 0; i < len; ++i) {
        sha1__process_byte(ctx, block[i]);
    }

    usize bitcount = ctx->byte_count * 8;
    sha1__process_byte(ctx, 0x80);
    
    if (ctx->block_index > 56) {
        while (ctx->block_index != 0) {
            sha1__process_byte(ctx, 0);
        }
        while (ctx->block_index < 56) {
            sha1__process_byte(ctx, 0);
        }
    } else {
        while (ctx->block_index < 56) {
            sha1__process_byte(ctx, 0);
        }
    }
    sha1__process_byte(ctx, 0);
    sha1__process_byte(ctx, 0);
    sha1__process_byte(ctx, 0);
    sha1__process_byte(ctx, 0);
    sha1__process_byte(ctx, (uchar)((bitcount >> 24) & 0xFF));
    sha1__process_byte(ctx, (uchar)((bitcount >> 16) & 0xFF));
    sha1__process_byte(ctx, (uchar)((bitcount >> 8 ) & 0xFF));
    sha1__process_byte(ctx, (uchar)((bitcount >> 0 ) & 0xFF));

    sha1_result_t result = {0};
    memcpy(result.digest, ctx->digest, sizeof(result.digest));
    return result;
}

str_t sha1_str(arena_t *arena, sha1_t *ctx, const void *buf, usize len) {
    sha1_result_t result = sha1(ctx, buf, len);
    return str_fmt(arena, "%08x%08x%08x%08x%08x", result.digest[0], result.digest[1], result.digest[2], result.digest[3], result.digest[4]);
}

// BASE 64 //////////////////////////

unsigned char b64__encoding_table[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',           
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/'
};

u8 b64__decoding_table[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 62, 0, 0, 0, 63, 
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0, 0, 0, 
    0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
    12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 
    24, 25, 0, 0, 0, 0, 0, 0, 26, 27, 28, 29, 30, 31, 
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 
    44, 45, 46, 47, 48, 49, 50, 51, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
};

buffer_t base64_encode(arena_t *arena, buffer_t buffer) {
    usize outlen = ((buffer.len + 2) / 3) * 4;
    u8 *out = alloc(arena, u8, outlen);

    for (usize i = 0, j = 0; i < buffer.len;) {
        u32 a = i < buffer.len ? buffer.data[i++] : 0;
        u32 b = i < buffer.len ? buffer.data[i++] : 0;
        u32 c = i < buffer.len ? buffer.data[i++] : 0;

        u32 triple = (a << 16) | (b << 8) | c;

        out[j++] = b64__encoding_table[(triple >> 18) & 0x3F];
        out[j++] = b64__encoding_table[(triple >> 12) & 0x3F];
        out[j++] = b64__encoding_table[(triple >>  6) & 0x3F];
        out[j++] = b64__encoding_table[(triple >>  0) & 0x3F];
    }

    usize mod = buffer.len % 3;
    if (mod) {
        mod = 3 - mod;
        for (usize i = 0; i < mod; ++i) {
            out[outlen - 1 - i] = '=';
        }
    }

    return (buffer_t){
        .data = out,
        .len = outlen
    };
}

buffer_t base64_decode(arena_t *arena, buffer_t buffer) {
    u8 *out = arena->cur;
    usize start = arena_tell(arena);

    for (usize i = 0; i < buffer.len; i += 4) {
        u8 a = b64__decoding_table[buffer.data[i + 0]];
        u8 b = b64__decoding_table[buffer.data[i + 1]];
        u8 c = b64__decoding_table[buffer.data[i + 2]];
        u8 d = b64__decoding_table[buffer.data[i + 3]];

        u32 triple =
            ((u32)a << 18) |
            ((u32)b << 12) |
            ((u32)c << 6)  |
            ((u32)d);

        u8 *bytes = alloc(arena, u8, 3);

        bytes[0] = (triple >> 16) & 0xFF;
        bytes[1] = (triple >>  8) & 0xFF;
        bytes[2] = (triple >>  0) & 0xFF;
    }

    usize spaces_count = 0;
    for (isize i = buffer.len - 1; i >= 0; --i) {
        if (buffer.data[i] == '=') {
            spaces_count++;
        }
        else {
            break;
        }
    }

    usize outlen = arena_tell(arena) - start;

    return (buffer_t){
        .data = out,
        .len = outlen - spaces_count,
    };
}

strview_t pretty__colour[LOG_COL__COUNT] = {
    [LOG_COL_BLACK]         = cstrv("black"),
    [LOG_COL_BLUE]          = cstrv("blue"),
    [LOG_COL_GREEN]         = cstrv("green"),
    [LOG_COL_CYAN]          = cstrv("cyan"),
    [LOG_COL_RED]           = cstrv("red"),
    [LOG_COL_MAGENTA]       = cstrv("magenta"),
    [LOG_COL_YELLOW]        = cstrv("yellow"),
    [LOG_COL_GREY]          = cstrv("grey"),
    
    [LOG_COL_DARK_GREY]     = cstrv("dark_grey"),
    [LOG_COL_WHITE]         = cstrv("white"),
    [LOG_COL_LIGHT_BLUE]    = cstrv("light_blue"),
    [LOG_COL_LIGHT_GREEN]   = cstrv("light_green"),
    [LOG_COL_LIGHT_CYAN]    = cstrv("light_cyan"),
    [LOG_COL_LIGHT_RED]     = cstrv("light_red"),
    [LOG_COL_LIGHT_MAGENTA] = cstrv("light_magenta"),
    [LOG_COL_LIGHT_YELLOW]  = cstrv("light_yellow"),

    [LOG_COL_RESET]         = cstrv("/"),
};

strview_t pretty_log_to_colour(os_log_colour_e colour) {
    return pretty__colour[colour];
}

void pretty_print(arena_t scratch, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    pretty_printv(scratch, fmt, args);
    va_end(args);
}

void pretty_printv(arena_t scratch, const char *fmt, va_list args) {
    va_list tmp_args;
    va_copy(tmp_args, args);
    int len = fmt_bufferv(NULL, 0, fmt, tmp_args);
    va_end(tmp_args);

    char *buf = alloc(&scratch, char, len + 1);

    fmt_bufferv(buf, len + 1, fmt, args);

    oshandle_t out = os_stdout();
    instream_t in = istr_init(strv(buf, len));
    while (!istr_is_finished(&in)) {
        strview_t part = istr_get_view(&in, '<');
        bool has_escape = strv_ends_with(part, '\\');
        
        os_file_write(out, part.buf, part.len - has_escape);
        istr_skip(&in, 1);

        if (has_escape) {
            os_file_putc(out, istr_prev(&in));
            continue;
        }

        strview_t tag = istr_get_view(&in, '>');

        for (usize i = 0; i < arrlen(pretty__colour); ++i) {
            if (strv_equals(tag, pretty__colour[i])) {
                os_log_set_colour(i);
                break;
            }
        }

        istr_skip(&in, 1);
    }
}

str_t pretty_print_get_string(arena_t *arena, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    str_t out = pretty_print_get_stringv(arena, fmt, args);
    va_end(args);
    return out;
}

str_t pretty_print_get_stringv(arena_t *arena, const char *fmt, va_list args) {
    va_list tmp_args;
    va_copy(tmp_args, args);
    int len = fmt_bufferv(NULL, 0, fmt, tmp_args);
    va_end(tmp_args);

    char *buf = alloc(arena, char, len + 1);

    fmt_bufferv(buf, len + 1, fmt, args);

    outstream_t out = ostr_init(arena);
 
    instream_t in = istr_init(strv(buf, len));
    while (!istr_is_finished(&in)) {
        strview_t part = istr_get_view(&in, '<');
        bool has_escape = strv_ends_with(part, '\\');
        
        if (has_escape) {
            part.len -= 1;
        }

        ostr_puts(&out, part);
        istr_skip(&in, 1);

        if (has_escape) {
            ostr_putc(&out, '<');
            continue;
        }

        istr_get_view(&in, '>');

        istr_skip(&in, 1);
    }

    return ostr_to_str(&out);
}

usize pretty_print_get_length(strview_t view) {
    usize size = 0;

    instream_t in = istr_init(view);
    while (!istr_is_finished(&in)) {
        strview_t part = istr_get_view(&in, '<');
        bool has_escape = strv_ends_with(part, '\\');
        
        if (has_escape) {
            part.len -= 1;
        }

        size += strv_get_utf8_len(part);
        istr_skip(&in, 1);

        if (has_escape) {
            size++;
            continue;
        }

        istr_ignore_and_skip(&in, '>');
    }

    return size;
}
