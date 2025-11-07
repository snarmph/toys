#include "colla/colla.h" 
#include "common.h"
#include "toys.h"
#include "tui.h"

TOY_SHORT_DESC(time, "Time a simple command.");

#define TIME_MAX_ARGS 1024

typedef struct {
    strview_t args[TIME_MAX_ARGS];
    i64 arg_count;
} time_opt_t;

void time_print(double time_taken) {
    strview_t suffix[] = {
        cstrv("seconds"),
        cstrv("milliseconds"),
        cstrv("microseconds"),
        cstrv("nanoseconds"),
    };

    int suffix_index = 0;

    while (time_taken < 1.0 && (suffix_index + 1) < arrlen(suffix)) {
        time_taken *= 1000.0;
        suffix_index++;
    }

    print("time taken: %.2f %v\n", time_taken, suffix[suffix_index]);
}

void TOY(time)(int argc, char **argv) {
    time_opt_t opt = {0};
    usage_helper(
        "time COMMAND [ARGS]...", 
        "The time command runs the specified program command with "
        "the given arguments.  When command finishes, time writes "
        "a message to standard error giving timing statistics "
        "about this program run.", 
        USAGE_DEFAULT, 
        USAGE_EXTRA_PARAMS(opt.args, opt.arg_count), 
        argc, argv
    );

    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));
    os_cmd_t *cmd = NULL;
    darr_push(&arena, cmd, strv("cmd.exe"));
    darr_push(&arena, cmd, strv("/C"));
    for (i64 i = 0; i < opt.arg_count; ++i) {
        darr_push(&arena, cmd, opt.args[i]);
    }
    
    i64 tps = common_get_tps();
    i64 start = common_get_ticks();
    os_run_cmd(arena, cmd, NULL);
    i64 end = common_get_ticks();

    double time_taken = (double)(end - start) / (double)tps;
    time_print(time_taken);
}

#if 0
struct {
    i64 tps;
    i64 begin;
    i64 end;
    double time_passed;
    oshandle_t mtx;
    bool finished;
    bool finished_running;
    bool quit;
    spinner_t spinner;
} app = {0};

int run_thread(u64 id, void *udata) {
    COLLA_UNUSED(id);

    os_cmd_t *command = udata;

    u8 buf[KB(5)];

    arena_t tmp = arena_make(ARENA_STATIC, sizeof(buf), buf);

    oshandle_t hout = os_handle_zero();
    oshandle_t herr = os_handle_zero();

    app.begin = get_ticks();
        os_run_cmd(
            tmp, 
            command, 
            &(os_cmd_options_t){ .out = &hout, }
        );
    app.end = get_ticks();

    os_mutex_lock(app.mtx);
    app.finished_running = true;
    os_mutex_unlock(app.mtx);

    os_abort(0);

    return 0;
}

void app_print_time(outstream_t *out, double time_taken) {
    const char *suffix[] = {
        "seconds",
        "milliseconds",
        "microseconds"
        "nanoseconds",
    };

    int suffix_index = 0;

    while (time_taken < 1.0 && (suffix_index + 1) < arrlen(suffix)) {
        time_taken *= 1000.0;
        suffix_index++;
    }

    ostr_print(out, "%.2f %s\n", time_taken, suffix[suffix_index]);
}
bool app_update(arena_t *arena, float dt, void *udata) {
    COLLA_UNUSED(udata);
    COLLA_UNUSED(arena);

    spinner_update(&app.spinner, dt);

    app.time_passed += dt;

    if (os_mutex_try_lock(app.mtx)) {
        app.finished |= app.finished_running;
        os_mutex_unlock(app.mtx);
    }

    return app.finished || app.quit;
}

str_t app_view(arena_t *arena, void *udata) {
    COLLA_UNUSED(udata);

    outstream_t out = ostr_init(arena);
    
    if (app.quit) {
        ostr_print(&out, "<red>user quit after</> ");
        app_print_time(&out, app.time_passed);
    }
    else if (!app.finished) {
        ostr_print(&out, "<magenta>%v</> ", app.spinner.frames[app.spinner.cur]);
        ostr_puts(&out, strv("time taken: "));
        app_print_time(&out, app.time_passed);
    }
    else {
        double time_taken = (double)(app.end - app.begin) / (double)app.tps;
        ostr_puts(&out, strv("> time taken: "));
        app_print_time(&out, time_taken);
    }

    return ostr_to_str(&out);
}

void app_event(termevent_t *event, void *udata) {
    COLLA_UNUSED(udata);

#define IS(q) strv_equals(strv(event->value), (strview_t)cstrv(q))

    if (event->type != TERM_EVENT_KEY) {
        return;
    }

    if (IS("q") || IS("escape")) {
        app.quit = true;
    }

#undef IS
}

int main(int argc, char **argv) {
    colla_init(COLLA_OS);

    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));

    outstream_t cmd = ostr_init(&arena);
    // os_cmd_t *command = NULL;

    for (int i = 1; i < argc; ++i) {
        ostr_print(&cmd, "%s ", argv[i]);
        // darr_push(&arena, command, strv(argv[i]));
    }

    oshandle_t hout = os_handle_zero();

    app.begin = get_ticks();
        system(ostr_to_str(&cmd).buf);
        // os_run_cmd(
        //     arena, 
        //     command, 
        //     &(os_cmd_options_t){ .out = &hout, }
        // );
    app.end = get_ticks();

    app.tps = get_ticks_per_second();

    double time_taken = (double)(app.end - app.begin) / (double)app.tps;
    outstream_t out = ostr_init(&arena);
    ostr_puts(&out, strv("> time taken: "));
    app_print_time(&out, time_taken);
    println("\n%v", ostr_to_str(&out));

#if 0
    if (argc < 2) {
        print_usage();
        return 1;
    }

    os_init();

    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));

    os_cmd_t *command = NULL;

    for (int i = 1; i < argc; ++i) {
        darr_push(&arena, command, strv(argv[i]));
    }

    app.spinner = spinner_init(SPINNER_DOT);
    app.tps = get_ticks_per_second();
    app.mtx = os_mutex_create();

    oshandle_t thr = os_thread_launch(run_thread, command);
    os_thread_detach(thr);

    term_init(&(termdesc_t){
        .app = {
            .update = app_update,
            .view = app_view,
            .event = app_event,
        },
    });

    term_run();
#endif
}
#endif
