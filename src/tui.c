#include "tui.h"
#include <windows.h>
#include <stdarg.h>
#include <math.h>

#include "common.h"

#define TUI_CLEAR_SCREEN() print("\x1b[2J")
#define TUI_HIDE_CURSOR()  print("\x1b[?25l")
#define TUI_SHOW_CURSOR()  print("\x1b[?25h")
#define TUI_ERASE_SCREEN() print("\x1b[2J")
#define TUI_ENTER_ALT()    print("\x1b[?1049h")
#define TUI_EXIT_ALT()     print("\x1b[?1049l")

int tui__fg_colours[TUI_COLOUR__COUNT] = {
    [TUI_DEFAULT_COLOUR] = 39,
    [TUI_BLACK]          = 30,
    [TUI_RED]            = 31,
    [TUI_GREEN]          = 32,
    [TUI_YELLOW]         = 33,
    [TUI_BLUE]           = 34,
    [TUI_MAGENTA]        = 35,
    [TUI_CYAN]           = 36,
    [TUI_WHITE]          = 37,
    [TUI_DARK_GREY]      = 90,
    [TUI_LIGHT_RED]      = 91,
    [TUI_LIGHT_GREEN]    = 92,
    [TUI_LIGHT_YELLOW]   = 93,
    [TUI_LIGHT_BLUE]     = 94,
    [TUI_LIGHT_MAGENTA]  = 95,
    [TUI_LIGHT_CYAN]     = 96,
};

int tui__bg_colours[TUI_COLOUR__COUNT] = {
    [TUI_DEFAULT_COLOUR] = 49,
    [TUI_BLACK]          = 40,
    [TUI_RED]            = 41,
    [TUI_GREEN]          = 42,
    [TUI_YELLOW]         = 43,
    [TUI_BLUE]           = 44,
    [TUI_MAGENTA]        = 45,
    [TUI_CYAN]           = 46,
    [TUI_WHITE]          = 47,
    [TUI_DARK_GREY]      = 100,
    [TUI_LIGHT_RED]      = 101,
    [TUI_LIGHT_GREEN]    = 102,
    [TUI_LIGHT_YELLOW]   = 103,
    [TUI_LIGHT_BLUE]     = 104,
    [TUI_LIGHT_MAGENTA]  = 105,
    [TUI_LIGHT_CYAN]     = 106,
};

strview_t tui__borders[TUI_BORDER__COUNT][6] = {
    [TUI_BORDER_TRANSPARENT] = {
        cstrv(" "), cstrv(" "),
        cstrv(" "), cstrv(" "),
        cstrv(" "), cstrv(" "),
    },
    [TUI_BORDER_RECT]   = {
        cstrv("┌"), cstrv("┐"),
        cstrv("└"), cstrv("┘"),
        cstrv("─"), cstrv("│"),
    },
    [TUI_BORDER_TICK]   = {
        cstrv("┏"), cstrv("┓"),
        cstrv("┗"), cstrv("┛"),
        cstrv("━"), cstrv("┃"),
    },
    [TUI_BORDER_DOUBLE] = {
        cstrv("╔"), cstrv("╗"),
        cstrv("╚"), cstrv("╝"),
        cstrv("═"), cstrv("║"),
    },
    [TUI_BORDER_ROUND]  = {
        cstrv("╭"), cstrv("╮"),
        cstrv("╰"), cstrv("╯"),
        cstrv("─"), cstrv("│"),
    },
    [TUI_BORDER_ASCII]  = {
        cstrv("*"), cstrv("*"),
        cstrv("*"), cstrv("*"),
        cstrv("-"), cstrv("|"),
    },
};

typedef struct tui_spinner_t tui_spinner_t;
struct tui_spinner_t {
    strview_t frames[12];
    int count;
    double frame_time;
};


tui_spinner_t tui__spinners[TUI_SPINNER__COUNT] = {0};

typedef struct tui_t tui_t;
struct tui_t {
    arena_t arena;
    arena_t frame_arena;
    arena_t app_frame_arena;
    u32 buffer[200*200];
    tuielem_t *root;
    tuielem_t *tail;
    tui_desc_t app;
    int width, height;
    bool should_quit;
    ticker_t ticker;
    u64 spinner_frame;
    double spinner_time;
};
tui_t tui = {0};
tuielem_t *tui__new_elem(void);

void tui__cleanup(void) {
    arena_cleanup(&tui.frame_arena);
}

void tui_update_size(void) {
    HANDLE conout = (HANDLE)os_win_conout().data;
    CONSOLE_SCREEN_BUFFER_INFO console_info = {0};
    if (!GetConsoleScreenBufferInfo(conout, &console_info)) {
        fatal("couldn't update terminal size");
    }

    tui.width  = console_info.dwSize.X;
    tui.height = console_info.dwSize.Y;
}

strview_t tui__label_strip(strview_t label, int max_len) {
    usize stripped_len = pretty_print_get_length(label);
    if (stripped_len <= max_len) return label;

    bool inside_tag = false;
    usize end = 0;
    for (; end < label.len && max_len > 0; ++end) {
        char c = label.buf[end];
        switch (c) {
            case '<':
                inside_tag = true;
                break;
            case '>':
                inside_tag = false;
                break;
            case '\\':
                max_len--;
                if (end+1 < label.len && 
                   (label.buf[end+1] == '<' || label.buf[end+1] == '>')
                ) {
                    max_len--;
                    // skip < or >
                    end++;
                }
                break;
            case '\x1b':
                for (; end < label.len && !char_is_alpha(label.buf[end]); ++end) {
                }
                max_len++;
                break;
            default:
                max_len -= !inside_tag;
        }
    }

    if (max_len < 0) end -= -max_len;

    return strv_sub(label, 0, end);
}

void tui__render_elem(outstream_t *o, tuielem_t *e, int xoff, int yoff, int width, int height) {
    int x = xoff + (e->x <= 1 ? (int)(e->x * (float)width)  : (int)e->x);
    int y = yoff + (e->y <= 1 ? (int)(e->y * (float)height) : (int)e->y);
    int w = (int)e->w;
    int h = (int)e->h;
    if (w < 0) {
        w = width + w;
    }
    if (h < 0) {
        h = height + h;
    }
    if (e->x < 1 && e->w <= 1) {
        w = (int)round(e->w * width);
    }
    if (e->y < 1 && e->h <= 1) {
        h = (int)round(e->h * height);
    }
    int padx = e->padding_x;
    int pady = e->padding_y;

    bool has_border = e->border > 0;

    int content_x = x + padx + has_border;
    int content_y = y + pady + has_border;
    int view_w = w - (padx + has_border) * 2;
    int view_h = h - (pady + has_border) * 2;
    
    int view_off = 0;
    switch (e->list_type & TUI_LIST__MASK) {
        case TUI_LIST_LINE:
            if (e->list_current_item > (view_h / 2)) {
                view_off = MAX(0, e->list_current_item - (view_h / 2));
                if (e->line_count - view_off < view_h) {
                    view_off = MAX(0, e->line_count - view_h);
                }
            }
            break;
        case TUI_LIST_PAGE:
        {
            view_off = e->list_current_item;
            int maxy = e->line_count - view_h;
            
            if (view_off >= maxy) {
                view_off = view_off % maxy;
            }
            else if (view_off < 0) {
                view_off = maxy - ((-view_off) % maxy) + 1;
            }

            break;
        }
    }

    if (has_border) {
        int bar_height = 0;
        int bar_offset = 0;

        if (e->line_count > view_h) {
            int rem = e->line_count - view_h;
            bar_height = view_h - rem;
            if (bar_height < 1) {
                bar_height = 1;
                double small_sz = (double)view_h / (double)rem;
                bar_offset = (int)((double)view_off * small_sz);
            }
            else {
                bar_offset = view_off;
            }
        }

        strview_t *b = tui__borders[e->border & TUI_BORDER__MASK];
        tui_set_pos(o, x, y);
        
        tui_set_bg(o, e->bg_border);
        tui_set_fg(o, e->fg_border);

        // -- TOP --
        if (e->border & TUI_BORDER_TOP) {
            ostr_puts(o, b[0]);
            ostr_puts(o, b[4]);
        }

        int rest = w - 3;
        if (e->title.len > 0) {
            int max_title_len = rest - 6;
            usize title_len = pretty_print_get_length(e->title);
            strview_t label = tui__label_strip(e->title, max_title_len);
            usize label_len = pretty_print_get_length(label);
            if (label_len < title_len) {
                ostr_print(o, " %v<dark_grey>...</> ", label);
                label_len += 3;
            }
            else {
                ostr_print(o, " %v</> ", label);
            }
       
            tui_set_bg(o, e->bg_border);
            tui_set_fg(o, e->fg_border);

            rest -= (int)label_len + 2;
        }

        if (e->border & TUI_BORDER_TOP) {
            for (int i = 0; i < rest; ++i) {
                ostr_puts(o, b[4]);
            }
            ostr_puts(o, b[1]);
        }

        // -- SIDES --
        for (int i = 2; i < h; ++i) {
            tui_move_down_at_beg(o, 1);

            if (e->border & TUI_BORDER_LEFT) {
                tui_set_x(o, x);
                ostr_puts(o, b[5]);
            }
            tui_set_x(o, x + w - 1);
            int pos = i - 2;
            if (pos >= bar_offset && (pos-bar_offset) < bar_height) {
                ostr_puts(o, strv("</>▐"));

                tui_set_bg(o, e->bg_border);
                tui_set_fg(o, e->fg_border);
            }
            else if (e->border & TUI_BORDER_RIGHT) {
                ostr_puts(o, b[5]);
            }
        }
        tui_move_down_at_beg(o, 1);

        // -- BOTTOM --
        if (e->border & TUI_BORDER_BOTTOM) {
            tui_set_x(o, x);
            ostr_puts(o, b[2]);
            for (int i = 2; i < w; ++i) {
                ostr_puts(o, b[4]);
            }
            ostr_puts(o, b[3]);
        }
    }

    tui_set_pos(o, content_x, content_y);
    tui_reset_colour(o);

    bool print_lines = e->list_type & TUI_LIST_DISPLAY_NUMBER;
    int num_width = 0;
    int left_bar_w = 0;
    if (print_lines) {
        int lines = e->line_count;
        while (lines > 0) {
            lines /= 10;
            num_width++;
        }
        left_bar_w = num_width + 4;
    }

    tui_set_bg(o, TUI_DEFAULT_COLOUR);
    int lines = MIN(view_h, e->line_count);
    for (int i = 0; i < lines; ++i) {
        int index = i + view_off;
        if (index >= e->line_count) break;
        strview_t line = strv(e->lines[index]);

        int max_w = view_w - left_bar_w;
        line = tui__label_strip(line, max_w);
        usize line_len = pretty_print_get_length(line);
        int left_count = max_w - (int)line_len + padx;

        tui_set_x(o, x + has_border);
        ostr_print(o, "%*s", padx, "");
        if (print_lines) {
            ostr_print(o, "<dark_grey> %*d │ </>", num_width, index + 1);
        }
        ostr_print(o, "%v%*s", line, left_count, "");
        tui_reset_colour(o);
        tui_move_down_at_beg(o, 1);
    }
    tui_set_bg(o, TUI_DEFAULT_COLOUR);

    if (!e->children_head) {
        for (int i = lines; i < view_h; ++i) {
            tui_set_x(o, x + has_border);
            int count = w - 2 * has_border;
            ostr_print(o, "%*s", count, "");
            tui_move_down_at_beg(o, 1);
        }
    }
    
    for_each (c, e->children_head) {
        // tui__render_elem(o, c, x, y, w, h);
        tui__render_elem(o, c, content_x, content_y, view_w, view_h);
    }
}

void tui__begin_frame(void) {
    arena_rewind(&tui.frame_arena, 0);
    tui.tail = tui.root = tui__new_elem();
    tui.root->w = tui.root->h = 1;
}

void tui__end_frame(void) {
    colla_assert(tui.tail == tui.root, "forgot a tui_end!");
    outstream_t out = ostr_init(&tui.frame_arena);
    tui__render_elem(&out, tui.root, 1, 1, tui_width(), tui_height());
    str_t screen = ostr_to_str(&out);
    pretty_print(tui.frame_arena, "%v", screen);
}

bool tui__has_input(void) {
    oshandle_t conin = os_win_conin();

    INPUT_RECORD rec;
    DWORD read;
    
    while (PeekConsoleInput((HANDLE)conin.data, &rec, 1, &read) && read > 0) {
        if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown) {
            return true;
        }

        ReadConsoleInput((HANDLE)conin.data, &rec, 1, &read);
    }

    return false;
}

typedef enum {
    VT_STATE_BEGIN,
    VT_STATE_ESC,
    VT_STATE_CSI,   // control sequence introduces ([)
    VT_STATE_OSC,   // operating system command (])
    VT_STATE_DCS,   // device control string (P)
    VT_STATE_SS3,   // single shift 3 (O)
    VT_STATE_MOD,   // modifier (;)
    VT_STATE__COUNT,
} tui_vt_state_e;

typedef struct tui_vt_parser_t tui_vt_parser_t;
struct tui_vt_parser_t {
    tui_vt_state_e state;
    int num;
    int modifier;
};

void tui_send_key_event(strview_t key, strview_t mod) {
    arena_t *arena = &tui.frame_arena;
    str_t value = STR_EMPTY;
    if (strv_is_empty(mod)) {
        value = str(arena, key);
    }
    else {
        value = str_fmt(arena, "%v+%v", mod, key);
    }
    bool should_quit = tui.app.event(&tui.app_frame_arena, strv(value), tui.app.userdata);
    tui.should_quit |= should_quit;
}

void tui_vt_process(tui_vt_parser_t *ctx, char c) {
    static strview_t keys[] = {
        [2]   = cstrv("insert"),
        [3]   = cstrv("delete"),
        [5]   = cstrv("page-up"),
        [6]   = cstrv("page-down"),
        [15]  = cstrv("F5"),
        [17]  = cstrv("F6"),
        [18]  = cstrv("F7"),
        [19]  = cstrv("F8"),
        [20]  = cstrv("F9"),
        [21]  = cstrv("F10"),
        [23]  = cstrv("F11"),
        [24]  = cstrv("F12"),

        ['A'] = cstrv("up"),
        ['B'] = cstrv("down"),
        ['C'] = cstrv("right"),
        ['D'] = cstrv("left"),
        ['H'] = cstrv("home"),
        ['F'] = cstrv("end"),
 
        ['P'] = cstrv("F1"),
        ['Q'] = cstrv("F2"),
        ['R'] = cstrv("F3"),
        ['S'] = cstrv("F4"),
    };

    static strview_t mod[] = {
        [2] = cstrv("shift"),
        [3] = cstrv("alt"),
        [4] = cstrv("shift+alt"),
        [5] = cstrv("ctrl"),
        [6] = cstrv("shift+ctrl"),
        [7] = cstrv("alt+ctrl"),
        [8] = cstrv("shift+alt+ctrl"),
    };

    switch (ctx->state) {
        case VT_STATE_BEGIN:
            memset(ctx, 0, sizeof(*ctx));

            // escape
            if (c == '\x1b') {
                ctx->state = VT_STATE_ESC;
            }
            // tab
            else if (c == '\t') {
                tui_send_key_event(strv("tab"), mod[ctx->modifier]);
            }
            // backspace
            else if (c == '\x7f') {
                tui_send_key_event(strv("backspace"), mod[ctx->modifier]);
            }
            // enter
            else if (c == '\x0d') {
                tui_send_key_event(strv("enter"), mod[ctx->modifier]);
            }
            // ctrl + ?
            else if (c <= '\x1f' && c & '\x1f') {
                strview_t modifier = mod[ctx->modifier];
                if (strv_is_empty(modifier)) {
                    modifier = strv("ctrl");
                }
                else {
                    str_t tmp = str_fmt(&tui.frame_arena, "%v+ctrl", modifier);
                    modifier = strv(tmp);
                }
                c = char_lower(c | 0x40);
                tui_send_key_event(strv(&c, 1), modifier);
            }
            else {
                tui_send_key_event(strv(&c, 1), mod[ctx->modifier]);
            }
            break;

        case VT_STATE_ESC:
            if (c == '[') {
                ctx->state = VT_STATE_CSI;
            }
            else if (c == 'O') {
                ctx->state = VT_STATE_SS3;
            }
            break;
            
        case VT_STATE_CSI:
            switch (c) {
                case '~':
                    tui_send_key_event(keys[ctx->num], mod[ctx->modifier]);
                    ctx->state = VT_STATE_BEGIN;
                    break;

                case ';':
                    ctx->state = VT_STATE_MOD;
                    break;
                
                default:
                    if (char_is_num(c)) {
                        ctx->num = (ctx->num * 10) + (c - '0');
                    }
                    else if (char_is_alpha(c)) {
                        tui_send_key_event(keys[c], mod[ctx->modifier]);
                        ctx->state = VT_STATE_BEGIN;
                    }
                    break;
            }
            break;

        case VT_STATE_OSC:
            break;

        case VT_STATE_DCS:
            break;

        case VT_STATE_SS3:
            tui_send_key_event(keys[c], mod[ctx->modifier]);
            ctx->state = VT_STATE_BEGIN;
            break;

        case VT_STATE_MOD:
            if (char_is_num(c)) {
                ctx->modifier = c - '0';
                ctx->state = VT_STATE_CSI;
            }
            break;

        default: break;
    }
}

void tui__poll_input(void) {
    oshandle_t conin = os_win_conin();

    if (!tui__has_input()) {
        return;
    }

    u8 buffer[64];
    usize read = os_file_read(conin, buffer, sizeof(buffer)); 
    if (read == 0) {
        tui.should_quit = true;
    }

    tui.should_quit = false;

    tui_vt_parser_t vt = {0};
    for (usize i = 0; i < read; ++i) {
        tui_vt_process(&vt, (char)buffer[i]);
    }

    if (vt.state == VT_STATE_ESC) {
        tui_send_key_event(strv("escape"), STRV_EMPTY);
    }
    else if (vt.state != VT_STATE_BEGIN) {
        colla_assert(false);
    }
}

void tui__update_internal(float dt, void *udata) {
    COLLA_UNUSED(udata);

    tui_update_size();

    tui.spinner_time += dt;

    // arena_t scratch = arena_scratch(&tui.frame_arena, MB(1));
    arena_rewind(&tui.app_frame_arena, 0);

    tui__poll_input();
    tui__begin_frame();
    bool update_quit = tui.app.update(&tui.app_frame_arena, dt, tui.app.userdata);
    tui__end_frame();

    tui.should_quit |= update_quit;
}

void tui_init(tui_desc_t *desc) {
    tui.frame_arena = arena_make(ARENA_VIRTUAL, GB(1));
    tui.app_frame_arena = arena_make(ARENA_VIRTUAL, GB(1));
    tui.app = *desc;
    if (tui.app.fps == 0) {
        tui.app.fps = 30;
    }
    tui.ticker = ticker_init(tui.app.fps, tui__update_internal, NULL);

    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD console_mode = 0;
    if (!GetConsoleMode(handle, &console_mode)) {
        fatal("couldn't get console mode: %v", os_get_error_string(os_get_last_error()));
    }

    console_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(handle, console_mode);

    TUI_HIDE_CURSOR();
    TUI_ENTER_ALT();
    TUI_CLEAR_SCREEN();

#define SPIN(t, ...) (tui_spinner_t){ .frame_time = (t), .frames = { __VA_ARGS__ }, .count = arrlen(((strview_t[]){ __VA_ARGS__ })) } 
    tui__spinners[TUI_SPINNER_LINE]     = SPIN(0.10, cstrv("|"), cstrv("/"), cstrv("-"), cstrv("\\"));
    tui__spinners[TUI_SPINNER_DOT]      = SPIN(0.10, cstrv("⣾"), cstrv("⣽"), cstrv("⣻"), cstrv("⢿"), cstrv("⡿"), cstrv("⣟"), cstrv("⣯"), cstrv("⣷"));
    tui__spinners[TUI_SPINNER_MINIDOT]  = SPIN(0.08, cstrv("⠋"), cstrv("⠙"), cstrv("⠹"), cstrv("⠸"), cstrv("⠼"), cstrv("⠴"), cstrv("⠦"), cstrv("⠧"), cstrv("⠇"), cstrv("⠏"));
    tui__spinners[TUI_SPINNER_JUMP]     = SPIN(0.10, cstrv("⢄"), cstrv("⢂"), cstrv("⢁"), cstrv("⡁"), cstrv("⡈"), cstrv("⡐"), cstrv("⡠"));
    tui__spinners[TUI_SPINNER_PULSE]    = SPIN(0.12, cstrv("█"), cstrv("▓"), cstrv("▒"), cstrv("░"));
    tui__spinners[TUI_SPINNER_POINTS]   = SPIN(0.14, cstrv("∙∙∙"), cstrv("●∙∙"), cstrv("∙●∙"), cstrv("∙∙●"));
    tui__spinners[TUI_SPINNER_GLOBE]    = SPIN(0.25, cstrv("🌍"), cstrv("🌎"), cstrv("🌏"));
    tui__spinners[TUI_SPINNER_MOON]     = SPIN(0.12, cstrv("🌑"), cstrv("🌒"), cstrv("🌓"), cstrv("🌔"), cstrv("🌕"), cstrv("🌖"), cstrv("🌗"), cstrv("🌘"));
    tui__spinners[TUI_SPINNER_METER]    = SPIN(0.14, cstrv("▱▱▱"), cstrv("▰▱▱"), cstrv("▰▰▱"), cstrv("▰▰▰"), cstrv("▰▰▱"), cstrv("▰▱▱"), cstrv("▱▱▱"));
    tui__spinners[TUI_SPINNER_ELLIPSIS] = SPIN(0.33, cstrv(""), cstrv("."), cstrv(".."), cstrv("..."));
#undef SPIN
}

void tui_run(void) {
    // setup input and save previous state
    oshandle_t input = os_win_conin();

    DWORD prev = 0; // 0x01f7
    GetConsoleMode((HANDLE)input.data, &prev);

    SetConsoleMode((HANDLE)input.data, ENABLE_VIRTUAL_TERMINAL_INPUT);

    tui_update_size();

    while (!tui.should_quit) {
        ticker_tick(&tui.ticker);
    }

    tui__cleanup();
    SetConsoleMode((HANDLE)input.data, (DWORD)(~ENABLE_VIRTUAL_TERMINAL_INPUT));
}

int tui_width(void) {
    return tui.width;
}

int tui_height(void) {
    return tui.height;
}

tuielem_t *tui_panel(strview_t title) {
    tuielem_t *e = tui__new_elem();
    e->title = title;
    e->border = TUI_BORDER_ROUND | TUI_BORDER_ALL;
    return e;
}

tuielem_t *tui_list(strview_t title, int current_item) {
    tuielem_t *e = tui__new_elem();
    e->title = title;
    e->border = TUI_BORDER_ROUND | TUI_BORDER_ALL;
    e->list_current_item = current_item;
    return e;
}

tuielem_t *tui_begin(void) {
    tuielem_t *e = tui__new_elem();
    return e;
}

void tui_end(void) {
    colla_assert(tui.tail && tui.tail->parent, "one too many tui_end");
    tuielem_t *e = tui.tail;
    tuielem_t *p = e->parent;
    if (!p->children_head) {
        p->children_head = e;
        p->children_tail = e;
    }
    else {
        p->children_tail->next = e;
        p->children_tail = e;
    }
        
    tui.tail = p;
}

tuielem_t *tui_hor_split(float ratio) {
    tuielem_t *e = tui__new_elem();
    e->hor_ratio = ratio;
    return e;
}

tuielem_t *tui_ver_split(float ratio) {
    tuielem_t *e = tui__new_elem();
    e->ver_ratio = ratio;
    return e;
}

strview_t tui_spinner(tui_spinner_e type) {
    tui_spinner_t *s = &tui__spinners[type];
    i64 frame = (i64)(tui.spinner_time / s->frame_time);
    int cur = frame % s->count;
    return s->frames[cur];
}

void tui_puts(strview_t line) {
    tuielem_t *e = tui.tail;
    e->lines[e->line_count++] = str(&tui.frame_arena, line);
}

void tui_print(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    tui_printv(fmt, args);
    va_end(args);
}

void tui_printv(const char *fmt, va_list args) {
    tuielem_t *e = tui.tail;
    str_t line = str_fmtv(&tui.frame_arena, fmt, args);
    e->lines[e->line_count++] = line;
}

void tui_set_max_elements(tuielem_t *e, int max_elements) {
    if (max_elements > arrlen(e->content)) {
        e->lines = alloc(&tui.frame_arena, str_t, max_elements);
    }
}

tuielem_t *tui_get_top(void) {
    return tui.tail;
}

void tui_move(outstream_t *out, int offx, int offy) {
    if (offx > 0) ostr_print(out, "\x1b[%dC", offx);
    else if (offx < 0) ostr_print(out, "\x1b[%dD", -offx);

    if (offy > 0) ostr_print(out, "\x1b[%dB", offy);
    else if (offy < 0) ostr_print(out, "\x1b[%dA", -offy);
}

void tui_reset_pos(outstream_t *out) {
    ostr_puts(out, strv("\x1b[H"));
}

void tui_set_x(outstream_t *out, int x) {
    ostr_print(out, "\x1b[%dG", x);
}

void tui_set_y(outstream_t *out, int y) {
    tui_reset_pos(out);
    ostr_print(out, "\x1b[%dB", y);
}

void tui_set_pos(outstream_t *out, int x, int y) {
    ostr_print(out, "\x1b[%d;%dH", y, x);
}

void tui_move_down_at_beg(outstream_t *out, int offy) {
    if (offy > 0) ostr_print(out, "\x1b[%dE", offy);
    if (offy < 0) ostr_print(out, "\x1b[%dF", offy);
}

void tui_set_fg(outstream_t *out, tui_colour_e col) {
    ostr_print(out, "\x1b[%dm", tui__fg_colours[col]);
}

void tui_set_bg(outstream_t *out, tui_colour_e col) {
    ostr_print(out, "\x1b[%dm", tui__bg_colours[col]);
}

void tui_set_colour(outstream_t *out, tui_colour_e bg, tui_colour_e fg) {
    ostr_print(out, "\x1b[%d;%dm", tui__bg_colours[bg], tui__fg_colours[fg]);
}

void tui_reset_colour(outstream_t *out) {
    ostr_puts(out, strv("\x1b[m"));
}

// -- private functions --------------------

tuielem_t *tui__new_elem(void) {
    tuielem_t *e = alloc(&tui.frame_arena, tuielem_t);
    e->w = e->h = 1.0;
    tuielem_t *p = tui.tail;
    e->lines = e->content;
    e->parent = p;
    tui.tail = e;

    if (p) {
        if (p->ver_ratio > 0) {
            if (p->children_head) {
                if (p->ver_ratio >= 1) {
                    e->h = -p->ver_ratio;
                    e->y = p->ver_ratio;
                }
                else {
                    e->h = 1.0f - p->ver_ratio;
                    e->y = p->ver_ratio;
                }
            }
            else {
                e->h = p->ver_ratio;
            }
        }

        if (p->hor_ratio > 0) {
            if (p->children_head) {
                if (p->hor_ratio >= 1) {
                    e->w = -p->hor_ratio;
                    e->x = p->hor_ratio;
                }
                else {
                    e->w = 1.0f - p->hor_ratio;
                    e->x = p->hor_ratio;
                }
            }
            else {
                e->w = p->hor_ratio;
            }
        }
    }

    return e;
}

