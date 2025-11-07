#include "colla/colla.h"
#include "common.h"
#include "toys.h"
#include "tui.h"

TOY_SHORT_DESC(xxd, "Dump or edit files in hexidecimal.");

#define XXD_UNDO_BUF_SIZE 1024

#define ringbuf(T, N) struct { int head, tail; T data[N]; }

#define rb_push(rb, ...) \
    do { \
        (rb).tail  = (rb).tail + 1 % (arrlen((rb).data)); \
        if ((rb).tail == (rb).head) { \
            (rb).head = (rb).head + 1 % (arrlen((rb).data)); \
        } \
        (rb).data[(rb).tail] = (__VA_ARGS__); \
    } while (0)

#define rb_pop(rb, v) \
    do { \
        if ((rb).tail == (rb).head) break; \
        (v) = (rb).data[(rb).tail]; \
        (rb).tail = (rb).tail - 1; \
        if ((rb).tail < 0) (rb).tail = arrlen((rb).data) - 1; \
    } while(0)

typedef enum {
    XXD_MODE_HEX,
    XXD_MODE_INSERT_ASCII,
    XXD_MODE_SEARCH,
    XXD_MODE_GOTO,
} xxd_modes_e;

typedef struct xxd_undo_t xxd_undo_t;
struct xxd_undo_t {
    i64 pos;
    u8 byte;
};

struct {
    strview_t filename;
    buffer_t data;
    float write_timer;
    usize line_count;

    i64 cursor;
    i64 yoff;

    bool is_half;
    xxd_modes_e mode;
    bool is_editing_ascii;

    char search_buf[64];
    usize search_buf_pos;
    i64 search_cursor;
    bool is_last_search;
    bool is_first_search;
    float search_warn_timer;

    char goto_buf[32];
    usize goto_buf_pos;
    float goto_warn_timer;

    ringbuf(xxd_undo_t, XXD_UNDO_BUF_SIZE) undo;
} xxd_state = {
    .search_cursor = -1,
};

TOY_OPTION_DEFINE(xxd) {
    bool dump;
    bool nocolors;
    strview_t filename;
};

void TOY_OPTION_PARSE(xxd)(int argc, char **argv, TOY_OPTION(xxd) *opt) {
    strview_t files[1] = {0};
    i64 file_count = 0;

    bool out_piped = common_is_piped(os_stdout());

    usage_helper(
        "xxd [options] FILE",
        "Dump or edit files in hexidecimal.\n\n"
        "Keys:\n"
        "  any mode:\n"
        "    " TERM_FG_ORANGE "C-c" TERM_FG_DARK_GREY " ............... " TERM_RESET "exit\n"
        "  main mode:\n"
        "    " TERM_FG_ORANGE "q" TERM_FG_DARK_GREY " ................. " TERM_RESET "exit\n"
        "    " TERM_FG_ORANGE "w" TERM_FG_DARK_GREY " ................. " TERM_RESET "save\n"
        "    " TERM_FG_ORANGE "i" TERM_FG_DARK_GREY " ................. " TERM_RESET "(if in ascii tab) change mode to ascii insert\n"
        "    " TERM_FG_ORANGE "h/j/k/l" TERM_FG_DARK_GREY " ........... " TERM_RESET "move cursor\n"
        "    " TERM_FG_ORANGE "n/p" TERM_FG_DARK_GREY " ............... " TERM_RESET "next/previous search result\n"
        "    " TERM_FG_ORANGE "u" TERM_FG_DARK_GREY " ................. " TERM_RESET "undo\n"
        "    " TERM_FG_ORANGE "/" TERM_FG_DARK_GREY " ................. " TERM_RESET "search\n"
        "    " TERM_FG_ORANGE ":" TERM_FG_DARK_GREY " ................. " TERM_RESET "goto (+/- for relative, otherwise absolute)\n"
        "    " TERM_FG_ORANGE "0-9/a-f" TERM_FG_DARK_GREY " ........... " TERM_RESET "insert half-byte\n"
        "    " TERM_FG_ORANGE "C-j/C-k" TERM_FG_DARK_GREY " ........... " TERM_RESET "move to the next/previous page\n"
        "    " TERM_FG_ORANGE "C-h/C-l" TERM_FG_DARK_GREY " ........... " TERM_RESET "move to beginning/end of line\n"
        "    " TERM_FG_ORANGE "tab" TERM_FG_DARK_GREY " ............... " TERM_RESET "go to ascii tab\n"
        "  ascii insert mode:\n"
        "    " TERM_FG_ORANGE "up/down/left/right" TERM_FG_DARK_GREY "  " TERM_RESET "move cursor\n"
        "    " TERM_FG_ORANGE "C-left/C-right" TERM_FG_DARK_GREY " .... " TERM_RESET "move to beginning/end of line\n"
        "    " TERM_FG_ORANGE "C-down/C-up" TERM_FG_DARK_GREY " ....... " TERM_RESET "move to next/previous of line",
        out_piped ? USAGE_ALLOW_NO_ARGS : USAGE_DEFAULT,
        USAGE_EXTRA_PARAMS(files, file_count),
        argc, argv,
        {
            'd', "dump",
            "Don't open the editor, simply dump it to stdout.",
            USAGE_BOOL(opt->dump),
        },
        {
            'c', "no-colors",
            "Dump without colors.",
            USAGE_BOOL(opt->nocolors),
        },
    );

    if (!file_count) {
        fatal("no file passed");
    }

    if (out_piped) {
        opt->dump = true;
        opt->nocolors = true;
    }

    opt->filename = files[0];
}

void xxd_search_next(void) {
    strview_t haystack = strv((char*)xxd_state.data.data, xxd_state.data.len);
    usize from = xxd_state.search_cursor > 0 ? xxd_state.search_cursor + 1 : 0;
    strview_t needle = strv(xxd_state.search_buf, xxd_state.search_buf_pos);
    usize new_cursor = strv_find_view(haystack, needle, from);
    if (new_cursor != STR_NONE) {
        xxd_state.search_cursor = new_cursor;
        xxd_state.cursor = xxd_state.search_cursor;
    }
    else {
        xxd_state.is_last_search = true;
        xxd_state.search_warn_timer = 2.f;
    }
}

void xxd_search_prev(void) {
    strview_t haystack = strv((char*)xxd_state.data.data, xxd_state.data.len);
    if (xxd_state.search_cursor < 0) {
        xxd_state.is_first_search = true;
        xxd_state.search_warn_timer = 2.f;
        return;
    }

    haystack = strv_sub(haystack, 0, xxd_state.search_cursor);
    strview_t needle = strv(xxd_state.search_buf, xxd_state.search_buf_pos);
    usize new_cursor = strv_rfind_view(haystack, needle, 0);
    if (new_cursor != STR_NONE) {
        xxd_state.search_cursor = new_cursor;
        xxd_state.cursor = xxd_state.search_cursor;
    }
    else {
        xxd_state.is_first_search = true;
        xxd_state.search_warn_timer = 2.f;
    }
}

bool xxd_update(arena_t *arena, float dt, void *userdata) {
    COLLA_UNUSED(userdata);
    i64 start = xxd_state.yoff * 16;
    i64 end = start + tui_height() * 16;
    i64 count = end - start;
    xxd_state.write_timer -= dt;
    xxd_state.search_warn_timer -= dt;
    xxd_state.goto_warn_timer -= dt;

    tui_ver_split((float)tui_height() - 1);
        tui_hor_split(6.f);
            tuielem_t *left = tui_begin();
                tui_set_max_elements(left, (int)count);
                left->padding_x = 1;
                left->padding_y = 1;
                for (i64 off = start; off < end; off += 16) {
                    tui_print("<yellow>%04x</>", off);
                }
            tui_end();

            tui_hor_split(47.f);
                tuielem_t *hex = tui_begin();
                    tui_set_max_elements(hex, (int)count);
                    hex->padding_y = 1;
                    for (i64 off = start; off < end; off += 16) {
                        outstream_t line = ostr_init(arena);
                        for (i64 i = 0; i < 16; ++i) {
                            i64 index = off + i;
                            if (index >= (i64)xxd_state.data.len) {
                                ostr_puts(&line, strv("   "));
                                continue;
                            }
                            u8 byte = xxd_state.data.data[index];

                            if (index == xxd_state.cursor) {
                                if (xxd_state.is_half) {
                                    tui_set_colour(&line, TUI_WHITE, TUI_BLACK);
                                    ostr_print(&line, "%x", (byte & 0xF0) >> 4);
                                    tui_set_colour(&line, TUI_GREEN, TUI_BLACK);
                                    ostr_print(&line, "%x", byte & 0x0F);
                                }
                                else if (xxd_state.is_editing_ascii) {
                                    tui_set_colour(&line, TUI_WHITE, TUI_BLACK);
                                    ostr_print(&line, "%02x", byte);
                                }
                                else {
                                    tui_set_colour(&line, TUI_GREEN, TUI_BLACK);
                                    ostr_print(&line, "%02x", byte);
                                }
                                tui_reset_colour(&line);
                                ostr_putc(&line, ' ');
                            }
                            else if (index == xxd_state.search_cursor) {
                                tui_set_colour(&line, TUI_YELLOW, TUI_BLACK);
                                ostr_print(&line, "%02x", byte);
                                tui_reset_colour(&line);
                                ostr_putc(&line, ' ');
                            }
                            else if (byte == 0) {
                                ostr_print(&line, "<dark_grey>%02x</> ", byte);
                            }
                            else {
                                ostr_print(&line, "%02x ", byte);
                            }
                        }
                        tui_puts(strv(ostr_to_str(&line)));
                    }
                tui_end();

                tuielem_t *ascii = tui_begin();
                    tui_set_max_elements(ascii, (int)count);
                    ascii->padding_x = 1;
                    ascii->padding_y = 1;
                    for (i64 off = start; off < end; off += 16) {
                        outstream_t line = ostr_init(arena);
                        for (int i = 0; i < 16; ++i) {
                            i64 index = off + i;
                            if (index >= (i64)xxd_state.data.len) {
                                ostr_putc(&line, ' ');
                                continue;
                            }
                            u8 byte = xxd_state.data.data[index];

                            tui_colour_e fg = TUI_WHITE;
                            tui_colour_e bg = TUI_DEFAULT_COLOUR;

                            if (byte == 32) {
                                fg = TUI_DARK_GREY;
                                byte = '.';
                            }
                            else if (byte < 32 || byte > 126) {
                                if (byte < ' ') byte += '@';
                                else            byte = '?';
                                fg = TUI_RED;
                            }

                            if (index == xxd_state.cursor) {
                                if (xxd_state.is_editing_ascii) {
                                    bg = xxd_state.mode == XXD_MODE_INSERT_ASCII ? TUI_GREEN : TUI_YELLOW;
                                    fg = TUI_BLACK;
                                }
                                else {
                                    tui_colour_e tmp = fg;
                                    fg = bg == TUI_DEFAULT_COLOUR ? TUI_BLACK : bg;
                                    bg = tmp;
                                }
                            }
                            else if (index == xxd_state.search_cursor) {
                                bg = TUI_YELLOW;
                                fg = TUI_BLACK;
                            }

                            tui_set_colour(&line, bg, fg);
                            if (byte == '<') ostr_putc(&line, '\\');
                            ostr_putc(&line, byte);
                            tui_reset_colour(&line);
                        }
                        tui_puts(strv(ostr_to_str(&line)));
                    }
                tui_end();
            tui_end();
        tui_end();
        tuielem_t *h = tui_begin();
        {
            h->padding_x = 1;
            outstream_t line = ostr_init(arena);
            if (xxd_state.mode == XXD_MODE_SEARCH) {
                tui_print("/%v", strv(xxd_state.search_buf, xxd_state.search_buf_pos));
            }
            else if (xxd_state.mode == XXD_MODE_GOTO) {
                tui_print(":%v", strv(xxd_state.goto_buf, xxd_state.goto_buf_pos));
            }
            else if (xxd_state.goto_warn_timer > 0) {
                tui_print("<red>invalid position, valid range is [0, %zi]", xxd_state.data.len - 1);
            }
            else if ((xxd_state.is_first_search || xxd_state.is_last_search) && xxd_state.search_warn_timer > 0) {
                tui_print("<red>no more result in this direction</>");
            }
            else {
                ostr_print(
                    &line, 
                    "\"%v\", 0x%x/0x%x", 
                    xxd_state.filename,
                    xxd_state.cursor, xxd_state.data.len
                );
                usize w = tui_width() - 2 - ostr_tell(&line);
                if (xxd_state.mode == XXD_MODE_INSERT_ASCII) {
                    ostr_puts(&line, strv("<yellow> -- INSERT -- </>"));
                    w += sizeof("-- INSERT --") - 1;
                }
                if (xxd_state.write_timer > 0) {
                    ostr_print(&line, "%*s", w, "file written to disk");
                }
                tui_print("<dark_grey>%v</>", ostr_to_str(&line));
            }
        }
        tui_end();
    tui_end();
    return false;
}

#define cursor_advance(n) \
    do { \
        xxd_state.is_half = false; \
        xxd_state.cursor += (n); \
        if (xxd_state.cursor < 0 || xxd_state.cursor >= (i64)xxd_state.data.len) { \
            xxd_state.cursor -= (n); \
        } \
        i64 ypos = xxd_state.cursor / 16; \
        if (ypos >= tui_height() + xxd_state.yoff - 3) { \
            xxd_state.yoff++; \
        } \
        else if (ypos < xxd_state.yoff) { \
            xxd_state.yoff--; \
        } \
    } while (0)

#define cursor_page(n) \
    do { \
        xxd_state.is_half = false; \
        i64 page_size = (tui_height() - 3) * 16; \
        xxd_state.cursor += page_size * n; \
        if (xxd_state.cursor < 0) xxd_state.cursor = 0; \
        if (xxd_state.cursor >= (i64)xxd_state.data.len) xxd_state.cursor = xxd_state.data.len - 1; \
        xxd_state.yoff += (tui_height() - 3) * n; \
        if (xxd_state.yoff < 0) xxd_state.yoff = 0; \
        if (xxd_state.yoff >= ((i64)xxd_state.data.len / 16)) xxd_state.yoff = (xxd_state.data.len - 2) / 16; \
    } while (0)

#define IS(v) strv_equals(key, strv(v))
#define IS_RANGE(f, t) (key.buf[0] >= f && key.buf[0] <= t)

bool xxd_hex_event(strview_t key) {
    if (IS("tab")) {
        xxd_state.is_editing_ascii = !xxd_state.is_editing_ascii;
    }
    else if (IS("ctrl+j")) {
        cursor_page(+1);
    }
    else if (IS("ctrl+k")) {
        cursor_page(-1);
    }
    else if (IS("ctrl+h")) {
        xxd_state.is_half = false;
        xxd_state.cursor = xxd_state.cursor - (xxd_state.cursor % 16);
        if (xxd_state.cursor < 0) xxd_state.cursor = 0;
    }
    else if (IS("ctrl+l")) {
        xxd_state.is_half = false;
        xxd_state.cursor = xxd_state.cursor + (15 - (xxd_state.cursor % 16));
        if (xxd_state.cursor >= (i64)xxd_state.data.len) xxd_state.cursor = xxd_state.data.len - 1;
    }

    if (key.len > 1) return false;

    switch (key.buf[0]) {
        case 'q': 
            return true;
        case 'w':
            os_file_write_all(xxd_state.filename, xxd_state.data);
            xxd_state.write_timer = 2.f;
            break;
        case 'i':
            if (xxd_state.is_editing_ascii) {
                xxd_state.mode = XXD_MODE_INSERT_ASCII;
            }
            break;
        case 'h':
            cursor_advance(-1);
            break;
        case 'l':
            cursor_advance(+1);
            break;
        case 'j':
            cursor_advance(+16);
            break;
        case 'k':
            cursor_advance(-16);
            break;
        case 'p':
            xxd_state.is_first_search = false;
            xxd_search_prev();
            break;
        case 'n':
            xxd_state.is_last_search = false;
            xxd_search_next();
            break;
        case 'u':
        {
            xxd_state.is_half = false;
            xxd_undo_t undo = { .pos = -1 };
            rb_pop(xxd_state.undo, undo);
            if (undo.pos >= 0) {
                xxd_state.data.data[undo.pos] = undo.byte;
            }
            break;
        }
        case '/':
            xxd_state.search_buf_pos = 0;
            xxd_state.is_half = false;
            xxd_state.mode = XXD_MODE_SEARCH;
            break;
        case ':':
            xxd_state.is_half = false;
            xxd_state.mode = XXD_MODE_GOTO;
            break;
        default:
            if (IS_RANGE('0', '9') || IS_RANGE('a', 'f')) {
                u8 half_byte = 0;
                if (key.buf[0] < '9') half_byte = key.buf[0] - '0';
                else                  half_byte = key.buf[0] - 'a' + 10;

                u8 *byte = &xxd_state.data.data[xxd_state.cursor];

                if (xxd_state.is_half) {
                    *byte = (*byte & 0xF0) | (half_byte & 0x0F);
                    cursor_advance(+1);
                }
                else {
                    rb_push(xxd_state.undo, (xxd_undo_t){ .pos = xxd_state.cursor, .byte = *byte });
                    *byte = (half_byte << 4) | (*byte & 0x0F);
                    xxd_state.is_half = true;
                }
            }
    }
    
    return false;
}

bool xxd_ascii_event(strview_t key) {
    if (IS("escape")) {
        xxd_state.mode = XXD_MODE_HEX;
    }
    else if (IS("up")) {
        cursor_advance(-16);
    }
    else if (IS("down")) {
        cursor_advance(+16);
    }
    else if (IS("left")) {
        cursor_advance(-1);
    }
    else if (IS("right")) {
        cursor_advance(+1);
    }
    else if (IS("ctrl+up")) {
        cursor_page(-1);
    }
    else if (IS("ctrl+down")) {
        cursor_page(+1);
    }
    else if (IS("ctrl+left")) {
        xxd_state.is_half = false;
        xxd_state.cursor = xxd_state.cursor - (xxd_state.cursor % 16);
        if (xxd_state.cursor < 0) xxd_state.cursor = 0;
    }
    else if (IS("ctrl+right")) {
        xxd_state.is_half = false;
        xxd_state.cursor = xxd_state.cursor + (15 - (xxd_state.cursor % 16));
        if (xxd_state.cursor >= (i64)xxd_state.data.len) xxd_state.cursor = xxd_state.data.len - 1;
    }

    if (key.len > 1 || !IS_RANGE(' ', '~')) {
        return false;
    }

    u8 *byte = &xxd_state.data.data[xxd_state.cursor];
    rb_push(xxd_state.undo, (xxd_undo_t){ .pos = xxd_state.cursor, .byte = *byte });
    *byte = (u8)key.buf[0];
    cursor_advance(+1);

    return false;
}

bool xxd_search_event(strview_t key) {
    if (IS("escape")) {
        xxd_state.mode = XXD_MODE_HEX;
    }
    else if (IS("delete")) {
        xxd_state.search_buf_pos--;
        if (xxd_state.search_buf_pos < 0) xxd_state.search_buf_pos = 0;
    }
    else if (IS("enter")) {
        xxd_state.mode = XXD_MODE_HEX;
        xxd_search_next();
    }

    if (key.len > 1 || !IS_RANGE(' ', '~')) {
        return false;
    }

    if (xxd_state.search_buf_pos < arrlen(xxd_state.search_buf)) {
        xxd_state.search_buf[xxd_state.search_buf_pos++] = key.buf[0];
    }

    return false;
}

bool xxd_goto_event(strview_t key) {
    if (IS("escape")) {
        xxd_state.mode = XXD_MODE_HEX;
    }
    else if (IS("backspace")) {
        xxd_state.goto_buf_pos--;
        if (xxd_state.goto_buf_pos < 0) xxd_state.goto_buf_pos = 0;
    }
    else if (IS("enter")) {
        xxd_state.mode = XXD_MODE_HEX;
        char first = xxd_state.goto_buf_pos > 0 ? xxd_state.goto_buf[0] : '\0';
        bool relative = first == '-' || first == '+';
        int offset = common_strv_to_int(strv(xxd_state.goto_buf, xxd_state.goto_buf_pos));
        xxd_state.goto_buf_pos = 0;
        xxd_state.goto_warn_timer = 0;
        memset(xxd_state.goto_buf, 0, sizeof(xxd_state.goto_buf));

        i64 new_cursor = xxd_state.cursor;
        if (relative) new_cursor += offset;
        else          new_cursor  = offset;

        if (new_cursor < 0 || new_cursor >= (i64)xxd_state.data.len) {
            xxd_state.goto_warn_timer = 2.0;
            return false;
        }

        xxd_state.cursor = new_cursor;
    }

    if (key.len > 1 || !(IS_RANGE(' ', '~'))) {
        return false;
    }

    if (xxd_state.goto_buf_pos < arrlen(xxd_state.goto_buf)) {
        xxd_state.goto_buf[xxd_state.goto_buf_pos++] = key.buf[0];
    }

    return false;
}

bool xxd_event(arena_t *arena, strview_t key, void *userdata) {
    COLLA_UNUSED(arena); COLLA_UNUSED(userdata);

    if (IS("ctrl+c")) return true;

    switch (xxd_state.mode) {
        case XXD_MODE_HEX:          return xxd_hex_event(key);
        case XXD_MODE_INSERT_ASCII: return xxd_ascii_event(key);
        case XXD_MODE_SEARCH:       return xxd_search_event(key);
        case XXD_MODE_GOTO:         return xxd_goto_event(key);
    }

    return false;
}

#undef IS
#undef IS_RANGE
#undef cursor_advance
#undef cursor_page

void xxd_dump(TOY_OPTION(xxd) *opt) {
    buffer_t buf = xxd_state.data;
    strview_t pos_col = opt->nocolors ? STRV_EMPTY : strv(TERM_FG_YELLOW);
    for (usize i = 0; i < buf.len; i += 16) {
        // print position
        print("%v%04x " TERM_RESET, pos_col, i);
        // print bytes
        for (int k = 0; k < 16; ++k) {
            usize index = i + k;
            if (index >= buf.len) {
                print("%*s", (16-k) * 3, "");
                break;
            }
            print("%02x ", buf.data[index]);
        }
        // print ascii
        for (int k = 0; k < 16; ++k) {
            usize index = i + k;
            if (index >= buf.len) {
                print("%*s", (16-k) * 3, "");
                break;
            }
            u8 byte = buf.data[index];
            strview_t col = STRV_EMPTY;
            if (byte == 32) {
                col = strv(TERM_FG_DARK_GREY);
                byte = '.';
            }
            else if (byte < 32 || byte > 126) {
                if (byte < ' ') byte += '@';
                else            byte = '?';
                col = strv(TERM_FG_RED);
            }
            if (opt->nocolors) {
                print("%c", byte);
            }
            else {
                print("%v%c"TERM_RESET, col, byte);
            }
        }
        println("");
    }
}

void TOY(xxd)(int argc, char **argv) {
    TOY_OPTION(xxd) opt = {0};
    TOY_OPTION_PARSE(xxd)(argc, argv, &opt);

    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));

    xxd_state.filename = opt.filename;

    if (!os_file_exists(xxd_state.filename)) {
        fatal("file %v does not exist", xxd_state.filename);
    }

    xxd_state.data = os_file_read_all(&arena, xxd_state.filename);

    if (opt.dump || opt.nocolors) {
        xxd_dump(&opt);
        return;
    }

    tui_init(&(tui_desc_t){
        .update = xxd_update,
        .event = xxd_event,
    });
    tui_run();
}

