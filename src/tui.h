#pragma once
#include "colla/colla.h"
// #include "term.h"

#define TUI_MAX_LINES   30
#define TUI_MAX_LINELEN 256

typedef enum {
    TUI_DEFAULT_COLOUR,
    TUI_BLACK,
    TUI_BLUE,
    TUI_GREEN,
    TUI_CYAN,
    TUI_RED,
    TUI_MAGENTA,
    TUI_YELLOW,
    TUI_GREY,
    TUI_DARK_GREY,
    TUI_LIGHT_BLUE,
    TUI_LIGHT_GREEN,
    TUI_LIGHT_CYAN,
    TUI_LIGHT_RED,
    TUI_LIGHT_MAGENTA,
    TUI_LIGHT_YELLOW,
    TUI_WHITE,
    TUI_COLOUR__COUNT,
} tui_colour_e;

typedef enum {
    TUI_BORDER_NONE,
    TUI_BORDER_TRANSPARENT,
    TUI_BORDER_RECT,   // ┌┐└┘─│
    TUI_BORDER_TICK,   // ┏┓┗┛━┃
    TUI_BORDER_DOUBLE, // ╔╗╚╝═║
    TUI_BORDER_ROUND,  // ╭╮╰╯─│
    TUI_BORDER_ASCII,  // ****-|
    TUI_BORDER__COUNT,
    TUI_BORDER__MASK  = (1 << 5) - 1,
    TUI_BORDER_TOP    = 1 << 5,
    TUI_BORDER_RIGHT  = 1 << 6,
    TUI_BORDER_BOTTOM = 1 << 7,
    TUI_BORDER_LEFT   = 1 << 8,
    TUI_BORDER_ALL    = TUI_BORDER_TOP | TUI_BORDER_RIGHT | TUI_BORDER_BOTTOM | TUI_BORDER_LEFT,
} tui_border_e;

typedef enum {
    TUI_SPINNER_LINE,     // | / - \\ |
    TUI_SPINNER_DOT,      // ⣾ ⣽ ⣻ ⢿ ⡿ ⣟ ⣯ ⣷
    TUI_SPINNER_MINIDOT,  // ⠋ ⠙ ⠹ ⠸ ⠼ ⠴ ⠦ ⠧ ⠇ ⠏
    TUI_SPINNER_JUMP,     // ⢄ ⢂ ⢁ ⡁ ⡈ ⡐ ⡠
    TUI_SPINNER_PULSE,    // █ ▓ ▒ ░
    TUI_SPINNER_POINTS,   // ∙∙∙ ●∙∙ ∙●∙ ∙∙●
    TUI_SPINNER_GLOBE,    // 🌍 🌎 🌏
    TUI_SPINNER_MOON,     // 🌑 🌒 🌓 🌔 🌕 🌖 🌗 🌘
    TUI_SPINNER_METER,    // ▱▱▱ ▰▱▱ ▰▰▱ ▰▰▰ ▰▰▱ ▰▱▱ ▱▱▱
    TUI_SPINNER_ELLIPSIS, // . .. ...
    TUI_SPINNER__COUNT,
} tui_spinner_e;

typedef enum {
    // types
    TUI_LIST_LINE = 0,
    TUI_LIST_PAGE = 1,
    TUI_LIST__MASK = 1,
    // options
    TUI_LIST_DISPLAY_NUMBER = 1 << 4,
} tui_list_e;

typedef struct tuielem_t tuielem_t;
struct tuielem_t {
    float x, y;
    float w, h;
    float hor_ratio;
    float ver_ratio;
    int padding_x;
    int padding_y;
    str_t content[TUI_MAX_LINES];
    str_t *lines;
    int line_count;
    int list_current_item;
    tui_colour_e list_odd_colour;
    tui_list_e list_type;

    tui_border_e border;
    tui_colour_e fg_border;
    tui_colour_e bg_border;
    strview_t title;
    tuielem_t *parent;
    tuielem_t *children_head;
    tuielem_t *children_tail;
    tuielem_t *next;
    tuielem_t *prev;
};

typedef struct tui_desc_t tui_desc_t;
struct tui_desc_t {
    // called every frame for rendering and updating, return true to quit
    bool (*update)(arena_t *arena, float dt, void *userdata);
    // called whenever an event happened, return true to quit
    bool (*event)(arena_t *arena, strview_t key, void *userdata);
    void *userdata;
    int fps;
};

void tui_init(tui_desc_t *desc);
void tui_run(void);

void tui_update_size(void);
int tui_width(void);
int tui_height(void);

tuielem_t *tui_panel(const char *title);
tuielem_t *tui_list(const char *title, int current_item);
tuielem_t *tui_begin(void);
void tui_end(void);

tuielem_t *tui_hor_split(float ratio);
tuielem_t *tui_ver_split(float ratio);

strview_t tui_spinner(tui_spinner_e type);
void tui_puts(strview_t line);
void tui_print(const char *fmt, ...);
void tui_printv(const char *fmt, va_list args);

void tui_set_max_elements(tuielem_t *e, int max_elements);

tuielem_t *tui_get_top(void);
void tui_move(outstream_t *out, int offx, int offy);
void tui_reset_pos(outstream_t *out);
void tui_set_x(outstream_t *out, int x);
void tui_set_y(outstream_t *out, int y);
void tui_set_pos(outstream_t *out, int x, int y);
void tui_move_down_at_beg(outstream_t *out, int offy);

void tui_set_fg(outstream_t *out, tui_colour_e col);
void tui_set_bg(outstream_t *out, tui_colour_e col);
void tui_set_colour(outstream_t *out, tui_colour_e bg, tui_colour_e fg);
void tui_reset_colour(outstream_t *out);

