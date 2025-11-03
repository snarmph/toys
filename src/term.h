#pragma once

#include "colla/colla.h"

#define TERM_MAX_SPEED (-1)

typedef enum {
    TERM_COL_DEFAULT,
    TERM_COL_BLACK,
    TERM_COL_BLUE,
    TERM_COL_GREEN,
    TERM_COL_CYAN,
    TERM_COL_RED,
    TERM_COL_MAGENTA,
    TERM_COL_YELLOW,
    TERM_COL_GREY,
    TERM_COL_DARK_GREY,
    TERM_COL_LIGHT_BLUE,
    TERM_COL_LIGHT_GREEN,
    TERM_COL_LIGHT_CYAN,
    TERM_COL_LIGHT_RED,
    TERM_COL_LIGHT_MAGENTA,
    TERM_COL_LIGHT_YELLOW,
    TERM_COL_WHITE,
    TERM_COL__COUNT,
} term_colour_e;

typedef enum {
    TERM_EVENT_NONE,
    TERM_EVENT_KEY,
    TERM_EVENT_MOUSE,
    TERM_EVENT_RESIZE,
    TERM_EVENT__COUNT,
} termevent_type_e;

typedef struct termevent_t termevent_t;
struct termevent_t {
    termevent_type_e type;
    bool is_key_down;
    str_t value;
};

typedef struct termapp_t termapp_t;
struct termapp_t {
    bool (*update)(arena_t *arena, float dt, void *userdata);
    str_t (*view)(arena_t *arena, void *userdata);
    void (*event)(termevent_t *event, void *userdata);
    void *userdata;
};

typedef struct termdesc_t termdesc_t;
struct termdesc_t {
    int fps;
    bool fullscreen;
    bool truncate;
    bool quickrun;
    oshandle_t output;
    termapp_t app;
};

typedef struct term_t term_t;

void term_init(termdesc_t *desc);

void term_run(void);

void term_move(outstream_t *out, int offx, int offy);
void term_reset_pos(outstream_t *out);
void term_set_x(outstream_t *out, int x);
void term_set_y(outstream_t *out, int y);
void term_set_pos(outstream_t *out, int x, int y);
void term_move_down_at_beg(outstream_t *out, int offy);

void term_set_fg(outstream_t *out, term_colour_e col);
void term_set_bg(outstream_t *out, term_colour_e col);

void term_write(strview_t str);

int term_width(void);
int term_height(void);

void term_clear(void);
void term_clear_line(void);
void term_clear_rest_of_line(void);

/// COMPONENTS //////////////////////////////////////////

// -- spinner ---------------------------------------- //

typedef enum {
    SPINNER_LINE,
    SPINNER_DOT,
    SPINNER_MINIDOT,
    SPINNER_JUMP,
    SPINNER_PULSE,
    SPINNER_POINTS,
    SPINNER_GLOBE,
    SPINNER_MOON,
    SPINNER_METER,
    SPINNER_ELLIPSIS,
    
    SPINNER__COUNT,
} spinner_type_e;

typedef struct spinner_t spinner_t;
struct spinner_t {
    strview_t *frames;
    int count;
    int cur;
    float timer;
    float passed;
};

spinner_t spinner_init(spinner_type_e type);

void spinner_update(spinner_t *ctx, float dt);
strview_t spinner_view(spinner_t *ctx);
