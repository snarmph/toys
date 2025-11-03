#include "colla/colla.c"
#include "tui.c"

#include <shellapi.h>
#pragma comment(lib, "shell32")

#define APP_ID "a44c221148af0d627d745d35c3089e955aafd1df36642635d44cc5e761c89615"
#define SECRET "gloas-25bcefc0ddd96a4a17d23eb1f792826d80c20eeeae327391240949161070130b"
#define REDIRECT_URI "http://localhost:"
#define PORT 5000
#define BIM_ID "23447726"

#define ATOMIC_SET(v, x) (InterlockedExchange(&v, (x)))
#define ATOMIC_CHECK(v)  (InterlockedCompareExchange(&v, 1, 1))
#define ATOMIC_INC(v) (InterlockedIncrement(&v))
#define ATOMIC_DEC(v) (InterlockedDecrement(&v))
#define ATOMIC_GET(v) (InterlockedOr(&v, 0))

typedef volatile long atomic_int_t;

typedef struct gitlab_t gitlab_t;
struct gitlab_t {
    arena_t arena;
    str_t token;
    str_t refresh_token;
    oshandle_t token_mtx;
};

typedef struct gbparam_t gbparam_t;
struct gbparam_t {
    strview_t key;
    strview_t val;
};

typedef struct gbreq_t gbreq_t;
struct gbreq_t {
    arena_t *arena;
    arena_t scratch;
    strview_t page;
    gbparam_t *params;
    int param_count;
};

typedef enum {
    GB_MAX_PIPELINES = 256,
    GB_MAX_JOBS      = 256,
    GB_MAX_WORKERS   = 6,
} gb_consts_e;

typedef enum {
    GB_STATUS_NONE,
    GB_STATUS_CANCELED,             // was manually canceled or automatically aborted.
    GB_STATUS_CANCELING,            // is being canceled but after_script is running.
    GB_STATUS_CREATED,              // has been created but not yet processed.
    GB_STATUS_FAILED,               // execution failed.
    GB_STATUS_MANUAL,               // requires manual action to start.
    GB_STATUS_PENDING,              // is in the queue waiting for a runner.
    GB_STATUS_PREPARING,            // is preparing the execution environment.
    GB_STATUS_RUNNING,              // is executing on a runner.
    GB_STATUS_SCHEDULED,            // has been scheduled but execution hasn’t started.
    GB_STATUS_SKIPPED,              // was skipped due to conditions or dependencies.
    GB_STATUS_SUCCESS,              // completed successfully.
    GB_STATUS_WAITING_FOR_RESOURCE, // is waiting for resources to become available.
    GB_STATUS__COUNT
} gb_status_e;

typedef enum {
    LOADING_FREE,
    LOADING_INFO,
    LOADING_JOBS,
    LOADING_FINISHED,
} loading_status_e;

typedef enum {
    JOB_LOADING_NONE,
    JOB_LOADING_LOG,
    JOB_LOADING_FINISHED,
} job_loading_statue_e;

typedef enum {
    PANEL_GLOBAL,
    PANEL_LIST,
    PANEL_CONTENT,
    PANEL_LOG,
    PANEL_HELP,
    PANEL__COUNT,
} panel_e;

typedef enum {
    JOB_STAGE_NONE,
    JOB_STAGE_PRE,
    JOB_STAGE_MAINTENANCE,
    JOB_STAGE_STATIC_ANALYSIS,
    JOB_STAGE_BUILD,
    JOB_STAGE_TEST,
    JOB_STAGE_PACKAGE,
    JOB_STAGE_RELEASE,
    JOB_STAGE_CLEANUP,
    JOB_STAGE__COUNT,
} job_stage_e;

typedef struct line_t line_t;
struct line_t {
    str_t text;
    int selection;
};

typedef struct gb_job_t gb_job_t;
struct gb_job_t {
    u64 id;
    str_t name;
    gb_status_e status;
    atomic_int_t loading_status;
    line_t *lines;
    int cur_log_line;
    int log_lines_count;
    int max_line_len;
};

typedef struct jobstage_t jobstage_t;
struct jobstage_t {
    gb_job_t jobs[GB_MAX_JOBS];
    int count;
    int selected;
    atomic_int_t loading_status;
};

typedef struct pipeline_t pipeline_t;
struct pipeline_t {
    u64 id;
    str_t sha;
    gb_status_e status;
    str_t title;
    atomic_int_t loading_status;
    jobstage_t stages[JOB_STAGE__COUNT];
    job_stage_e cur_stage;
};

typedef struct worker_data_t worker_data_t;
struct worker_data_t {
    arena_t arena;
    arena_t scratch;
    struct state_t *state;
};

typedef void (gb_job_func_f)(worker_data_t *data, void *udata);

typedef struct worker_job_t worker_job_t;
struct worker_job_t {
    gb_job_func_f *job;
    void *userdata;
};

typedef struct gb_job_queue_t gb_job_queue_t;
struct gb_job_queue_t {
    worker_job_t jobs[128];
    int reader;
    int writer;
    oshandle_t mutex;
    oshandle_t condvar;
};

typedef struct state_t state_t;
struct state_t {
    gitlab_t gb;
    arena_t arena;
    oshandle_t gb_mtx;
    panel_e panel;
    panel_e prev_panel;
    int selected;
    int help_off;
    bool is_searching;
    char search[128];
    int search_len;

    pipeline_t pipelines[GB_MAX_PIPELINES];
    atomic_int_t pipeline_count;
    atomic_int_t pipeline_fetched;
    oshandle_t workers[GB_MAX_WORKERS];
    int worker_count;
    gb_job_queue_t queue;
};

typedef struct help_key_t help_key_t;
struct help_key_t {
    strview_t desc;
    strview_t keys;
};

typedef struct help_t help_t;
struct help_t {
    help_key_t keys[PANEL__COUNT][20];
};

help_key_t help[PANEL__COUNT][20] = {
    [PANEL_GLOBAL] = {
        { cstrv("Quit:"),          cstrv("q/ctrl-c") },
        { cstrv("Pipeline list:"), cstrv("1/h/←") },
        { cstrv("Pipeline info:"), cstrv("2/l/→") },
        { cstrv("Help:"),          cstrv("?") },
    },
    [PANEL_LIST] = {
        { cstrv("Up:"),    cstrv("k/↑") },
        { cstrv("Down:"),  cstrv("j/↓") },
        { cstrv("Retry:"), cstrv("r") },
        { cstrv("Stop:"),  cstrv("s") },
    },
    [PANEL_CONTENT] = {
        { cstrv("Up:"),           cstrv("k/↑") },
        { cstrv("Down:"),         cstrv("j/↓") },
        { cstrv("Next tab:"),     cstrv("]") },
        { cstrv("Previous tab:"), cstrv("[") },
        { cstrv("Open log:"),     cstrv("enter") },
        { cstrv("Retry:"),        cstrv("r") },
        { cstrv("Stop:"),         cstrv("s") },
    },
    [PANEL_LOG] = {
        { cstrv("Up:"),        cstrv("k/↑") },
        { cstrv("Down:"),      cstrv("j/↓") },
        { cstrv("Page up:"),   cstrv("ctrl-u") },
        { cstrv("Page down:"), cstrv("ctrl-d") },
    },
};

strview_t job_stage_names[JOB_STAGE__COUNT] = {
    [JOB_STAGE_PRE]             = cstrv("pre"),
    [JOB_STAGE_MAINTENANCE]     = cstrv("maintenance"),
    [JOB_STAGE_STATIC_ANALYSIS] = cstrv("static analysis"),
    [JOB_STAGE_BUILD]           = cstrv("build"),
    [JOB_STAGE_TEST]            = cstrv("test"),
    [JOB_STAGE_PACKAGE]         = cstrv("package"),
    [JOB_STAGE_RELEASE]         = cstrv("release"),
    [JOB_STAGE_CLEANUP]         = cstrv("cleanup"),
};
strview_t gb_status_names[GB_STATUS__COUNT] = {
    [GB_STATUS_CANCELED]             = cstrv("canceled"),
    [GB_STATUS_CANCELING]            = cstrv("cencling"),
    [GB_STATUS_CREATED]              = cstrv("created"),
    [GB_STATUS_FAILED]               = cstrv("failed"),
    [GB_STATUS_MANUAL]               = cstrv("manual"),
    [GB_STATUS_PENDING]              = cstrv("pending"),
    [GB_STATUS_PREPARING]            = cstrv("preparing"),
    [GB_STATUS_RUNNING]              = cstrv("running"),
    [GB_STATUS_SCHEDULED]            = cstrv("scheduled"),
    [GB_STATUS_SKIPPED]              = cstrv("skipped"),
    [GB_STATUS_SUCCESS]              = cstrv("success"),
    [GB_STATUS_WAITING_FOR_RESOURCE] = cstrv("waiting"),
};

strview_t gb_status_colours[GB_STATUS__COUNT] = {
    [GB_STATUS_CANCELED]             = cstrv("magenta"), 
    [GB_STATUS_CANCELING]            = cstrv("light_magenta"), 
    [GB_STATUS_CREATED]              = cstrv("light_blue"), 
    [GB_STATUS_FAILED]               = cstrv("red"), 
    [GB_STATUS_MANUAL]               = cstrv("cyan"), 
    [GB_STATUS_PENDING]              = cstrv("yellow"), 
    [GB_STATUS_PREPARING]            = cstrv("light_green"), 
    [GB_STATUS_RUNNING]              = cstrv("blue"), 
    [GB_STATUS_SCHEDULED]            = cstrv("light_yellow"), 
    [GB_STATUS_SKIPPED]              = cstrv("dark_grey"), 
    [GB_STATUS_SUCCESS]              = cstrv("green"), 
    [GB_STATUS_WAITING_FOR_RESOURCE] = cstrv("light_red"), 
};

gitlab_t gb_init(void);
str_t gb_make_request(gitlab_t *ctx, gbreq_t *req);
strview_t gb_token(gitlab_t *ctx);

void job_clear_search(gb_job_t *job) {
    for (int i = 0; i < job->log_lines_count; ++i) {
        job->lines[i].selection = -1;
    }
}

void job_search(gb_job_t *job, strview_t needle) {
    for (int i = 0; i < job->log_lines_count; ++i) {
        line_t* line = &job->lines[i];
        usize index = strv_find_view(strv(line->text), needle, 0);
        if (index == STR_NONE) {
            job->lines[i].selection = -1;
            continue;
        }
        job->lines[i].selection = index;
    }
}

void job_search_next(gb_job_t *job) {
    for (int i = job->cur_log_line; i < job->log_lines_count; ++i) {
        if (job->lines[i].selection >= 0) {
            job->cur_log_line = i;
            return;
        }
    }
    for (int i = 0; i < job->cur_log_line; ++i) {
        if (job->lines[i].selection >= 0) {
            job->cur_log_line = i;
            return;
        }
    }
}

void job_search_prev(gb_job_t *job) {
    for (int i = job->cur_log_line - 1; i > 0; --i) {
        if (job->lines[i].selection >= 0) {
            job->cur_log_line = i;
            return;
        }
    }

    for (int i = job->log_lines_count - 1; i > job->cur_log_line; --i) {
        if (job->lines[i].selection >= 0) {
            job->cur_log_line = i;
            return;
        }
    }
}

void gb_jq_push(gb_job_queue_t *queue, gb_job_func_f *func, void *udata) {
    os_mutex_lock(queue->mutex);
        int next = (queue->writer + 1) % arrlen(queue->jobs);
        colla_assert(next != queue->reader);
        queue->jobs[queue->writer] = (worker_job_t){ .job = func, .userdata = udata };
        queue->writer = next;
        os_cond_broadcast(queue->condvar);
    os_mutex_unlock(queue->mutex);
}

void gb_jq_push_high_priority(gb_job_queue_t *queue, gb_job_func_f *func, void *udata) {
    os_mutex_lock(queue->mutex);
        int new_reader = (queue->reader == 0 ? arrlen(queue->jobs) - 1 : queue->reader - 1);
        colla_assert(new_reader != queue->writer);
        queue->reader = new_reader;
        queue->jobs[queue->reader] = (worker_job_t){ .job = func, .userdata = udata };
        os_cond_broadcast(queue->condvar);
    os_mutex_unlock(queue->mutex);
}

worker_job_t *gb_jq_pop(gb_job_queue_t *queue) {
    worker_job_t *job = NULL;
    os_mutex_lock(queue->mutex);
        while (queue->writer == queue->reader) {
            os_cond_wait(queue->condvar, queue->mutex, OS_WAIT_INFINITE);
        }
        job = &queue->jobs[queue->reader++];
        if (queue->reader >= arrlen(queue->jobs)) {
            queue->reader = 0;
        }
    os_mutex_unlock(queue->mutex);

    return job;
}

void worker_load_pipeline_job(worker_data_t *data, void *userdata) {
    uptr index = (uptr)userdata;
    pipeline_t *pip = &data->state->pipelines[index];
    str_t url = str_fmt(&data->scratch, "projects/"BIM_ID"/repository/commits/%v", pip->sha);
    arena_t tmp = arena_scratch(&data->scratch, MB(5));
    str_t res = gb_make_request(
        &data->state->gb,
        &(gbreq_t) {
            .arena = &data->scratch, 
            .scratch = tmp,
            .page = strv(url),
        }
    );
    json_t *doc = json_parse_str(&data->scratch, strv(res), JSON_DEFAULT);
    json_t *title = json_get(doc, strv("title"));
    pip->title = str(&data->arena, title->string);
    ATOMIC_SET(pip->loading_status, LOADING_JOBS);
    ATOMIC_INC(data->state->pipeline_fetched);

    url = str_fmt(&data->scratch, "projects/"BIM_ID"/pipelines/%llu/jobs", pip->id);
    res = gb_make_request(
        &data->state->gb,
        &(gbreq_t) {
            .arena = &data->scratch,
            .scratch = tmp,
            .page = strv(url),
        }
    );
    doc = json_parse_str(&data->scratch, strv(res), JSON_DEFAULT);
        
    pip->cur_stage = JOB_STAGE__COUNT;

    for_each (json_job, doc->array) {
        json_t *json_id     = json_get(json_job, strv("id"));
        json_t *json_name   = json_get(json_job, strv("name"));
        json_t *json_status = json_get(json_job, strv("status"));
        json_t *json_stage  = json_get(json_job, strv("stage"));

        job_stage_e stage   = JOB_STAGE_NONE;
        gb_status_e status = 0;

        for (int i = 1; i < JOB_STAGE__COUNT; ++i) {
            if (strv_equals(json_stage->string, job_stage_names[i])) {
                stage = i;
                if (pip->cur_stage > i) {
                    pip->cur_stage = i;
                }
                break;
            }
        }
        
        for (int i = 0; i < GB_STATUS__COUNT; ++i) {
            if (strv_equals(json_status->string, gb_status_names[i])) {
                status = i;
                break;
            }
        }

        jobstage_t *job_stage = &pip->stages[stage];
        gb_job_t *j = &job_stage->jobs[job_stage->count++];

        j->id = json_id->number;
        j->name = str(&data->arena, json_name->string);
        j->status = status;
    }

    ATOMIC_SET(pip->loading_status, LOADING_FINISHED);
}

void worker_load_log(worker_data_t *data, void *userdata) {
    gb_job_t *job = userdata;
    ATOMIC_SET(job->loading_status, JOB_LOADING_LOG);

    arena_t tmp = arena_scratch(&data->scratch, MB(1));
    str_t path = str_fmt(&data->scratch, ".cache/%llu.log", job->id);
    int cur = 0;

    if (os_file_exists(strv(path))) {
        str_t log = os_file_read_all_str(&data->scratch, strv(path));
        for (usize i = 0; i < log.len; ++i) {
            job->log_lines_count += log.buf[i] == '\n';
        }
        job->lines = alloc(&data->arena, line_t, job->log_lines_count);
        instream_t in = istr_init(strv(log));
        while (!istr_is_finished(&in)) {
            strview_t line = istr_get_line(&in);
            job->lines[cur].text = str(&data->arena, line);
            job->lines[cur++].selection = -1;
        }
        goto finish;
    }

    str_t url = str_fmt(&data->scratch, "projects/"BIM_ID"/jobs/%llu/trace", job->id);
    str_t log = gb_make_request(
        &data->state->gb,
        &(gbreq_t) {
            .arena = &data->scratch, 
            .scratch = tmp,
            .page = strv(url),
        }
    );

    struct { strview_t code, color; } fg_codes[TUI_COLOUR__COUNT] = {
        [TUI_DEFAULT_COLOUR] = { cstrv("0"), cstrv("/") },
        [TUI_BLACK]          = { cstrv("30"), cstrv("black") },
        [TUI_RED]            = { cstrv("31"), cstrv("red") },
        [TUI_GREEN]          = { cstrv("32"), cstrv("green") },
        [TUI_YELLOW]         = { cstrv("33"), cstrv("yellow") },
        [TUI_BLUE]           = { cstrv("34"), cstrv("blue") },
        [TUI_MAGENTA]        = { cstrv("35"), cstrv("magenta") },
        [TUI_CYAN]           = { cstrv("36"), cstrv("cyan") },
        [TUI_WHITE]          = { cstrv("37"), cstrv("white") },
        [TUI_DARK_GREY]      = { cstrv("90"), cstrv("dark_grey") },
        [TUI_LIGHT_RED]      = { cstrv("91"), cstrv("light_red") },
        [TUI_LIGHT_GREEN]    = { cstrv("92"), cstrv("light_green") },
        [TUI_LIGHT_YELLOW]   = { cstrv("93"), cstrv("light_yellow") },
        [TUI_LIGHT_BLUE]     = { cstrv("94"), cstrv("light_blue") },
        [TUI_LIGHT_MAGENTA]  = { cstrv("95"), cstrv("light_magenta") },
        [TUI_LIGHT_CYAN]     = { cstrv("96"), cstrv("light_cyan") },
    };

    tui_colour_e last_colur = TUI_DEFAULT_COLOUR;

    instream_t in = istr_init(strv(log));

    // first count lines
    for (usize i = 0; i < log.len; ++i) {
        job->log_lines_count += log.buf[i] == '\n';
    }

    job->lines = alloc(&data->arena, line_t, job->log_lines_count);
    int longest = 0;

    while (!istr_is_finished(&in)) {
        outstream_t out = ostr_init(&data->arena);

        strview_t line = istr_get_line(&in);

        while (strv_starts_with_view(line, strv("section_start")) ||
               strv_starts_with_view(line, strv("section_end"))
        ) {
            usize end = strv_rfind(line, '\r', 0);
            line = strv_sub(line, end+1, STR_END);
        }

        for (usize i = 0; i < line.len; ++i) {
            switch (line.buf[i]) {
                case '<':
                case '>':
                    ostr_putc(&out, '\\');
                    ostr_putc(&out, line.buf[i]);
                    break;
                case '\r':
                    continue;
                case '\x1b':
                {
                    if (line.buf[i+3] == 'K') {
                        i += 3;
                        continue;
                    }
                    i += 2;
                    usize index = strv_find(line, ';', i);
                    strview_t code = strv_sub(line, i, index);
                    for (int i = 0; i < TUI_COLOUR__COUNT; ++i) {
                        if (strv_equals(code, fg_codes[i].code)) {
                            ostr_print(&out, "<%v>", fg_codes[i].color);
                            break;
                        }
                    }
                    i = strv_find(line, 'm', index);
                    break;
                }
                default:
                    ostr_putc(&out, line.buf[i]);
                    break;
            }
        }
        
        str_t text = ostr_to_str(&out);
        if (text.len > longest) {
            longest = text.len;
        }

        job->lines[cur].text = text;
        job->lines[cur++].selection = -1;
    }

    job->max_line_len = longest;

    if (!os_dir_exists(strv(".cache"))) {
        os_dir_create(strv(".cache"));
    }

    oshandle_t fp = os_file_open(strv(path), OS_FILE_WRITE);
    for (int i = 0; i < cur; ++i) {
        str_t s = job->lines[i].text;
        os_file_write(fp, s.buf, s.len);
        os_file_putc(fp, '\n');
    }
    os_file_close(fp);

finish:
    ATOMIC_SET(job->loading_status, JOB_LOADING_FINISHED);
}

void worker_retry_pipeline(worker_data_t *data, void *userdata) {
    pipeline_t *pip = userdata;

    str_t url = str_fmt(&data->scratch, "projects/"BIM_ID"/pipelines/%llu/retry", pip->id);
    arena_t tmp = arena_scratch(&data->scratch, MB(1));

    gb_make_request(
        &data->state->gb, 
        &(gbreq_t){
            .arena = &data->scratch,
            .scratch = tmp,
            .page = strv(url),
        }
    );
}

void worker_stop_pipeline(worker_data_t *data, void *userdata) {
    pipeline_t *pip = userdata;

    str_t url = str_fmt(&data->scratch, "projects/"BIM_ID"/pipelines/%llu/cancel", pip->id);
    arena_t tmp = arena_scratch(&data->scratch, MB(1));

    gb_make_request(
        &data->state->gb, 
        &(gbreq_t){
            .arena = &data->scratch,
            .scratch = tmp,
            .page = strv(url),
        }
    );
}

void worker_retry_job(worker_data_t *data, void *userdata) {
    gb_job_t *job = userdata;

    str_t url = str_fmt(&data->scratch, "projects/"BIM_ID"/jobs/%llu/retry", job->id);
    arena_t tmp = arena_scratch(&data->scratch, MB(1));

    gb_make_request(
        &data->state->gb, 
        &(gbreq_t){
            .arena = &data->scratch,
            .scratch = tmp,
            .page = strv(url),
        }
    );
}

void worker_stop_job(worker_data_t *data, void *userdata) {
    gb_job_t *job = userdata;

    str_t url = str_fmt(&data->scratch, "projects/"BIM_ID"/jobs/%llu/cancel", job->id);
    arena_t tmp = arena_scratch(&data->scratch, MB(1));

    gb_make_request(
        &data->state->gb, 
        &(gbreq_t){
            .arena = &data->scratch,
            .scratch = tmp,
            .page = strv(url),
        }
    );
}


int worker_thread(u64 id, void *udata) {
    state_t *s = udata;
    worker_data_t data = {
        .arena   = arena_make(ARENA_VIRTUAL, GB(1)),
        .scratch = arena_make(ARENA_VIRTUAL, GB(1)),
        .state   = s,
    };
    while (true) {
        worker_job_t *job = gb_jq_pop(&s->queue);
        if (!job) continue;
        arena_rewind(&data.scratch, 0);
        job->job(&data, job->userdata);
    }
    return 0;
}

str_t gb__get_token(arena_t *arena, str_t *refresh_token, bool force);
void gb__refresh_token(gitlab_t *ctx);

gitlab_t gb_init(void) {
    gitlab_t out = {
        .arena = arena_make(ARENA_VIRTUAL, GB(1)),
    };
    out.token = gb__get_token(&out.arena, &out.refresh_token, false);
    return out;
}

str_t gb__get_token(arena_t *arena, str_t *refresh_token, bool force) {
    arena_t scratch = arena_make(ARENA_VIRTUAL, MB(5));
    str_t home_dir = os_get_env_var(&scratch, strv("UserProfile"));
    str_t cache_path = str_fmt(&scratch, "%v/.ci_cache.ini", home_dir);

    strview_t token = STRV_EMPTY;
    strview_t new_refresh = STRV_EMPTY;

    if (!force && os_file_exists(strv(cache_path))) {
        ini_t ini = ini_parse(&scratch, strv(cache_path), NULL);
        initable_t *root = ini_get_table(&ini, INI_ROOT);
        inivalue_t *ini_token = ini_get(root, strv("token"));
        inivalue_t *ini_refresh = ini_get(root, strv("refresh"));
        token = ini_token->value;
        new_refresh = ini_refresh->value;
        goto finish;
    }
 
    u8 state_buf[45] = {0};
    for (int i = 0; i < arrlen(state_buf); ++i) {
        state_buf[i] = rand() % 255;
    }
    buffer_t state = base64_encode(&scratch, (buffer_t){ state_buf, sizeof(state_buf) });

    str_t callback = http_make_url_safe(&scratch, strv(REDIRECT_URI));
    str_t url = str_fmt(
        &scratch, 
        "https://gitlab.com/oauth/authorize?"
        "client_id=" APP_ID "&"
        "response_type=code&"
        "redirect_uri=%v%d&"
        "state=%v",
        callback, PORT, state
    );

    HINSTANCE result = ShellExecuteA(NULL, "open", url.buf, NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)result == 32) {
        println("failed to open default browser, please go to this link:");
        println("%v", url);
    }
    
    socket_t server_sock = sk_open(SOCK_TCP);

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = INADDR_ANY,
    };

    if (bind(server_sock, (struct sockaddr *)&addr, sizeof(addr))) {
        fatal("failed to bind on port 5000");
    }

    sk_listen(server_sock, 5);

    str_t token_string = STR_EMPTY;
    str_t new_refresh_token = STR_EMPTY;
    int expires_in = 0;

    for (int tries = 0; tries < 5; ++tries) {
        socket_t client = sk_accept(server_sock);

        info("client connected");

        u8 buf[KB(10)] = {0};
        outstream_t out = ostr_init(&scratch);
        while (true) {
            int read = sk_recv(client, buf, sizeof(buf));
            ostr_puts(&out, strv((char*)buf, read));
            if (read <= sizeof(buf)) {
                break;
            }
        }

        str_t res_str = ostr_to_str(&out);
        http_req_t req = http_parse_req(&scratch, strv(res_str));
        instream_t in = istr_init(req.url);
        istr_ignore_and_skip(&in, '=');
        strview_t code = istr_get_view(&in, '\0');

        str_t req_url = str_fmt(
            &scratch, 
            "https://gitlab.com/oauth/token?"
            "client_id=" APP_ID "&"
            "client_secret=" SECRET "&"
            "code=%v&"
            "grant_type=authorization_code&"
            "redirect_uri=%v%d",
            code, callback, PORT
        );

        http_res_t res = http_request(&(http_request_desc_t){
            .arena = &scratch,
            .url = strv(req_url),
            .request_type = HTTP_POST,
        });

        if (res.status_code != 200) {
            err("token request returned %d", res.status_code);
            println("%v", http_res_to_str(&scratch, &res));
            continue;
        }

        json_t *doc = json_parse_str(&scratch, res.body, JSON_DEFAULT);
        json_t *js_token   = json_get(doc, strv("access_token"));
        json_t *js_expiry  = json_get(doc, strv("expires_in"));
        json_t *js_refresh = json_get(doc, strv("refresh_token"));

        if (!(
            json_check(js_token, JSON_STRING) &&
            json_check(js_expiry, JSON_NUMBER) &&
            json_check(js_refresh, JSON_STRING)
        )) {
            err("json response is in the wrong format:");
            json_pretty_print(doc, NULL);
            continue;
        }

        token_string  = str(&scratch, js_token->string);
        new_refresh_token = str(&scratch, js_refresh->string);
        expires_in    = (int)js_expiry->number;

        http_res_t http_res = {
            .status_code = 200,
            .version = { 1, 1 },
            .headers = (http_header_t[]) { 
                { strv("Content-Type"), strv("text/html") },
            },
            .body = strv(
                "<html>"
                    "<body>"
                        "<h1>Successfully authorized!</h1>"
                        "<p>You can close this tab and return to the application now</p>"
                    "</body>"
                "</html>"
            ),
        };

        str_t success_res = http_res_to_str(&scratch, &http_res);
        sk_send(client, success_res.buf, success_res.len);

        sk_close(client);

        break;
    }

    sk_close(server_sock);

    if (token_string.len == 0) {
        fatal("failed to get access token");
    }

    outstream_t cache = ostr_init(&scratch);

    ostr_print(&cache, "token=%v\n", token_string);
    ostr_print(&cache, "refresh=%v\n", new_refresh_token);
    ostr_print(&cache, "expiry=%d\n", expires_in);

    str_t cache_str = ostr_to_str(&cache);

    os_file_write_all(strv(cache_path), (buffer_t){ (u8*)cache_str.buf, cache_str.len });

    token = strv(token_string);
    new_refresh = strv(new_refresh_token);

finish:
    str_t out = str(arena, token);
    if (refresh_token && new_refresh.len > 0) {
        *refresh_token = str(arena, new_refresh);
    }
    arena_cleanup(&scratch);
    return out;
}

void gb__refresh_token(gitlab_t *ctx) {
    arena_t scratch = arena_make(ARENA_MALLOC, MB(1));
    str_t callback = http_make_url_safe(&scratch, strv(REDIRECT_URI));

    os_mutex_lock(ctx->token_mtx);
        str_t url = str_fmt(
            &scratch, 
            "https://gitlab.com/oauth/token"
            "?client_id="APP_ID
            "&client_secret="SECRET
            "&refresh_token=%v"
            "&grant_type=refresh_token"
            "&redirect_uri=%v%d",
            ctx->refresh_token,
            callback, PORT
        );
        info("url: %v", url);

        str_t token_header = str_fmt(&scratch, "Bearer %v", ctx->token);
    os_mutex_unlock(ctx->token_mtx);

    http_header_t headers[] = {
        { strv("Authorization"), strv(token_header) }, 
    };

    http_res_t res = http_get(
        &scratch,
        strv(url),
        .request_type = HTTP_POST,
        .headers = headers,
        .header_count = arrlen(headers),
    );
    json_t *doc = json_parse_str(&scratch, res.body, JSON_DEFAULT);
    if (res.status_code != 200) {
        err("failed to refresh token");
        json_pretty_print(doc, NULL);

        os_mutex_lock(ctx->token_mtx);
            ctx->token = gb__get_token(&ctx->arena, &ctx->refresh_token, true);
        os_mutex_unlock(ctx->token_mtx);
        goto finish;
    }
    json_t *token   = json_get(doc, strv("access_token"));
    json_t *refresh = json_get(doc, strv("refresh_token"));

    os_mutex_lock(ctx->token_mtx);
        ctx->token         = str(&ctx->arena, token->string);
        ctx->refresh_token = str(&ctx->arena, refresh->string);
    os_mutex_unlock(ctx->token_mtx);

finish:
    arena_cleanup(&scratch);
}

str_t gb_make_request(gitlab_t *ctx, gbreq_t *req) {
    os_mutex_lock(ctx->token_mtx);
    str_t token_header = str_fmt(&req->scratch, "Bearer %v", ctx->token);
    os_mutex_unlock(ctx->token_mtx);

    outstream_t url = ostr_init(&req->scratch);

    ostr_print(&url, "gitlab.com/api/v4/%v", req->page);

    for (int i = 0; i < req->param_count; ++i) {
        ostr_putc(&url, i > 0 ? '&' : '?');
        ostr_print(&url, "%v=%v", req->params[i].key, req->params[i].val);
    }

    str_t full_url = ostr_to_str(&url);

    http_header_t headers[] = {
        { strv("Authorization"), strv(token_header) }, 
    };

    http_res_t res = http_get(
        &req->scratch, 
        strv(full_url), 
        .headers = headers,
        .header_count = arrlen(headers),
    );

    str_t out = STR_EMPTY;

    if (res.status_code != 200) {
        json_t *doc = json_parse_str(&req->scratch, res.body, JSON_DEFAULT);

        json_t *error = json_get(doc, strv("error"));
        json_t *desc  = json_get(doc, strv("error_description"));
        if (strv_equals(error->string, strv("invalid_token")) && 
            strv_equals(desc->string, strv("Token is expired. You can either do re-authorization or token refresh."))
        ) {
            gb__refresh_token(ctx);
            return gb_make_request(ctx, req);
        }

        err("[gitlab] request returned %d:", res.status_code);
        json_pretty_print(doc, NULL);
    }
    else {
        out = str(req->arena, res.body);
    }

    return out;
}

str_t gb_current_username(gitlab_t *ctx, arena_t *arena) {
    str_t username = STR_EMPTY;

    arena_t before = ctx->arena;
    arena_t scratch = arena_scratch(&ctx->arena, KB(5));
    arena_t tmp = arena_scratch(&ctx->arena, KB(5));
    str_t res = gb_make_request(ctx, &(gbreq_t){ &scratch, tmp, strv("user"), });
    if (res.len == 0) {
        goto finish;
    }

    json_t *doc = json_parse_str(&scratch, strv(res), JSON_DEFAULT);
    json_t *js_username = json_get(doc, strv("username"));
    if (!json_check(js_username, JSON_STRING)) {
        goto finish;
    }

    username = str(arena, js_username->string);
finish:
    ctx->arena = before;
    return username;
}

void print_status(gb_status_e status, strview_t text, bool selected) {
    tui_print(
        "<%v>%-9v</> <%s>%v</>", 
        gb_status_colours[status],
        gb_status_names[status], 
        selected ? "white" : "dark_grey", 
        text
    );
}

void print_help(const char *name, panel_e panel, bool top, bool left) {
    u8 tmpbuf[32];

    tuielem_t *p = tui_panel(name);
    p->border = TUI_BORDER_DOUBLE | TUI_BORDER_TOP | TUI_BORDER_LEFT;
    if (!top || tui_height() % 2 != 0) p->border |= TUI_BORDER_BOTTOM;
    if (left || tui_width() % 2 == 0) p->border |= TUI_BORDER_RIGHT;
    p->fg_border = TUI_BLUE;
    p->padding_x = 1;

    // tui_print("<blue># %s</>", name);
    for (int i = 0; i < arrlen(help[panel]); ++i) {
        help_key_t *key = &help[panel][i];
        if (key->desc.len == 0) break;

        arena_t scratch = arena_make(ARENA_STATIC, arrlen(tmpbuf), tmpbuf);
        outstream_t o = ostr_init(&scratch);
        for (int i = key->desc.len + 1; i < 15; ++i) {
            ostr_putc(&o, '.');
        }
    
        tui_print("%v <dark_grey>%v</> %v", key->desc, ostr_to_str(&o), key->keys);
    }

    tui_end();
}

bool update(arena_t *arena, float dt, void *userdata) {
    state_t *s = userdata;

    int count = ATOMIC_GET(s->pipeline_fetched);
    
    pipeline_t *selected = &s->pipelines[s->selected];
    bool loaded = ATOMIC_GET(selected->loading_status) == LOADING_FINISHED;

    double left_ratio = s->panel == PANEL_LIST ? 0.6 : 0.2;
    if (!loaded) left_ratio = 1;

    tui_ver_split(tui_height() - 1);
        if (s->panel == PANEL_HELP) {
            int w = tui_width() - 4;
            char buf[256] = {0};
            strview_t line = strv(buf, w);
            if (w < arrlen(buf)) {
                memset(buf, '-', w);
            }
            else {
                line.buf = alloc(arena, char, w);
            }

            int total_lines = 19 + 4;

            tuielem_t *p = tui_panel("help");
            p->fg_border = TUI_GREEN;
                tui_ver_split(0.5);
                    tui_hor_split(0.5);
                        print_help("GLOBAL", PANEL_GLOBAL, true, true);
                        print_help("PIPELINES", PANEL_LIST, true, true);
                    tui_end();
                    tui_hor_split(0.5);
                        print_help("JOBS", PANEL_CONTENT, false, false);
                        print_help("LOG", PANEL_LOG, false, false);
                    tui_end();
                tui_end();
            tui_end();
        }
        else if (s->panel == PANEL_LOG) {
            jobstage_t *stage = &selected->stages[selected->cur_stage];
            gb_job_t *job = &stage->jobs[stage->selected];
            tuielem_t *p = NULL;
            if (ATOMIC_GET(job->loading_status) != JOB_LOADING_FINISHED) {
                p = tui_panel("log");
                tui_print("<magenta>%v </> loading log", tui_spinner(TUI_SPINNER_LINE));
            }
            else {
                p = tui_list("log", job->cur_log_line);
                p->list_type = TUI_LIST_PAGE | TUI_LIST_DISPLAY_NUMBER;
                tui_set_max_elements(p, job->log_lines_count);
                int max_height = tui_height();
                for (int i = 0; i < job->log_lines_count; ++i) {
                    line_t *line = &job->lines[i];
                    if (line->selection >= 0) {
                        arena_t scratch = *arena;
                        outstream_t out = ostr_init(&scratch);
                        strview_t bef = str_sub(line->text, 0, line->selection);
                        strview_t res = str_sub(line->text, line->selection, line->selection + s->search_len);
                        strview_t aft = str_sub(line->text, line->selection + s->search_len, STR_END);
                        ostr_puts(&out, bef);
                        tui_set_bg(&out, TUI_YELLOW);
                        tui_set_fg(&out, TUI_BLACK);
                        ostr_puts(&out, res);
                        tui_set_bg(&out, TUI_DEFAULT_COLOUR);
                        tui_set_fg(&out, TUI_DEFAULT_COLOUR);
                        ostr_puts(&out, aft);
                        tui_print("%v", ostr_to_str(&out));
                    }
                    else {
                        tui_print("%v", job->lines[i].text);
                    }
                }
            }
            p->fg_border = TUI_GREEN;
            tui_end();
        }
        else {

            tui_hor_split(left_ratio);

            {
                tuielem_t *p = tui_list("[1] pipelines", s->selected);
                tui_set_max_elements(p, s->pipeline_count);
                    if (s->panel == PANEL_LIST) {
                        p->fg_border = TUI_GREEN;
                    }
                    if (count < s->pipeline_count) {
                        tui_print("<magenta>%v </>loading %d/%d", tui_spinner(TUI_SPINNER_LINE), count, s->pipeline_count);
                    }

                    for (int i = 0; i < s->pipeline_count; ++i) {
                        pipeline_t *p = &s->pipelines[i];
                        if (ATOMIC_GET(p->loading_status) <= LOADING_INFO) {
                            continue;
                        }
                        print_status(p->status, strv(p->title), s->selected == i);
                    }
                tui_end();
            }
            if (loaded) {
                bool active = s->panel == PANEL_CONTENT;
                        
                outstream_t title = ostr_init(arena);
                ostr_puts(&title, strv("[2] "));
                bool first = true;
                for (int i = 1; i < JOB_STAGE__COUNT; ++i) {
                    if (selected->stages[i].count > 0) {
                        if (!first) {
                            ostr_print(
                                &title, 
                                "<%s>-</>", 
                                active ? "green" : "white"
                            );
                        }
                        bool cur_tab = i == selected->cur_stage;
                        ostr_print(
                            &title, 
                            "<%s>%v</>", 
                            cur_tab 
                                ? active ? "green" : "white" 
                                : "dark_grey",
                            job_stage_names[i]
                        );
                        first = false;
                    }
                }

                jobstage_t *stage = &selected->stages[selected->cur_stage];
                tuielem_t *p = tui_list(ostr_to_str(&title).buf, 0);
                    tui_set_max_elements(p, stage->count);
                    if (active) p->fg_border = TUI_GREEN;
                    p->padding_x = 1;
                    for (int i = 0; i < stage->count; ++i) {
                        bool cur_job = stage->selected == i;
                        gb_job_t *job = &stage->jobs[i];
                        print_status(job->status, strv(job->name), stage->selected == i);
                    }
                tui_end();
            }
            tui_end();
        }
        
        tui_begin();
            if (s->is_searching) {
                tui_print("/%v", strv(s->search, s->search_len));
            }
            else {
                tui_print("<dark_grey>Quit: q, Toggle help: ?");
            }
        tui_end();

    tui_end();
 
    return false;
}

bool event(arena_t *arena, strview_t key, void *userdata) {
    state_t *s = userdata;

    pipeline_t *selected = &s->pipelines[s->selected];
    bool loaded = ATOMIC_GET(selected->loading_status) == LOADING_FINISHED;

#define IS(v) strv_equals(key, strv(v))

    bool down = IS("j") || IS("down");
    bool up   = IS("k") || IS("up");

    if (IS("q") || IS("ctrl+c")) {
        return true;
    }
    else if (s->is_searching) {
        jobstage_t *stage = &selected->stages[selected->cur_stage];
        gb_job_t *job = &stage->jobs[stage->selected];
        if (IS("escape")) {
            job_clear_search(job);
            s->is_searching = false;
        }
        else if (IS("enter")) {
            job_search(job, strv(s->search, s->search_len));
            job_search_next(job);
            s->is_searching = false;
        }
        else if (IS("backspace")) {
            s->search_len--;
            if (s->search_len < 0) s->search_len = 0;
        }
        else {
            colla_assert((s->search_len + key.len) < arrlen(s->search));
            memcpy(s->search + s->search_len, key.buf, key.len);
            s->search_len += key.len;
        }
    }
    else if (IS("1") || IS("h") || IS("left")) {
        s->panel = PANEL_LIST;
    }
    else if (loaded && (IS("2") || IS("l") || IS("right"))) {
        s->panel = PANEL_CONTENT;
    }
    else if (IS("?")) {
        if (s->panel == PANEL_HELP) {
            s->panel = s->prev_panel;
        }
        else {
            s->prev_panel = s->panel;
            s->panel = PANEL_HELP;
        }
    }
    else if (s->panel == PANEL_LIST) {
        int off = 0;
        if (down) {
            off += 1;
        }
        else if (up) {
            off -= 1;
        }
        else if (IS("r")) {
            gb_jq_push_high_priority(&s->queue, worker_retry_pipeline, selected);
        }
        else if (IS("s")) {
            gb_jq_push_high_priority(&s->queue, worker_stop_pipeline, selected);
        }
        s->selected += off;
        if (s->selected >= ATOMIC_GET(s->pipeline_fetched)) {
            s->selected = 0;
        }
        if (s->selected < 0) {
            s->selected = ATOMIC_GET(s->pipeline_fetched) - 1;
        }
    }
    else if (s->panel == PANEL_CONTENT) {
        if (IS("]")) {
            for (int i = selected->cur_stage + 1; i < JOB_STAGE__COUNT; ++i) {
                if (selected->stages[i].count > 0) {
                    selected->cur_stage = i;
                    break;
                }
            }
        }
        else if(IS("[")) {
            for (int i = selected->cur_stage - 1; i > JOB_STAGE_NONE; --i) {
                if (selected->stages[i].count > 0) {
                    selected->cur_stage = i;
                    break;
                }
            }
        }
        else if (IS("enter")) {
            jobstage_t *stage = &selected->stages[selected->cur_stage];
            gb_job_t *job = &stage->jobs[stage->selected];
            if (ATOMIC_GET(job->loading_status) < JOB_LOADING_LOG) {
                gb_jq_push_high_priority(&s->queue, worker_load_log, job);
            }
            s->panel = PANEL_LOG;
        }
        else if (IS("r")) {
            jobstage_t *stage = &selected->stages[selected->cur_stage];
            gb_job_t *job = &stage->jobs[stage->selected];
            gb_jq_push_high_priority(&s->queue, worker_retry_job, job);
        }
        else if (IS("s")) {
            jobstage_t *stage = &selected->stages[selected->cur_stage];
            gb_job_t *job = &stage->jobs[stage->selected];
            gb_jq_push_high_priority(&s->queue, worker_stop_job, job);
        }
        else {
            jobstage_t *stage = &selected->stages[selected->cur_stage];
            if (down) {
                stage->selected = (stage->selected + 1) % stage->count;
            }
            else if (up) {
                stage->selected--;
                if (stage->selected < 0) stage->selected = stage->count - 1;
            }
        }
    }
    else if (s->panel == PANEL_LOG) {
        jobstage_t *stage = &selected->stages[selected->cur_stage];
        gb_job_t *job = &stage->jobs[stage->selected];
        if (ATOMIC_GET(job->loading_status) == JOB_LOADING_FINISHED) {
            int h = tui_height();
            int pady = 2;
            int hh = (int)roundf((float)h / 2.f);
            int miny = hh - pady;
            int maxy = job->log_lines_count - hh - pady - 1;

            if (down)              job->cur_log_line++;
            else if (up)           job->cur_log_line--;
            else if (IS("ctrl+u")) job->cur_log_line -= h;
            else if (IS("ctrl+d")) job->cur_log_line += h;
            else if (IS("/")) {
                s->is_searching = true;
                s->search_len = 0;
            }
            else if (s->search_len > 0) {
                if (IS("n")) {
                    job_search_next(job);
                }
                else if (IS("shift+n")) {
                    job_search_prev(job);
                }
            }
        }
    }
    else if (s->panel == PANEL_HELP) {
        if (down) s->help_off++;
        if (up) s->help_off--;
    }
    #undef IS

    return false;
}

void print_usage(void) {
    print("usage: ci [username]\n");
    os_abort(0);
}

int main(int argc, char **argv) {
    colla_init(COLLA_ALL);

    if (argc > 2) {
        print_usage();
    }

    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));
    arena_t scratch = arena_make(ARENA_VIRTUAL, GB(1));

    state_t *state = alloc(&arena, state_t);
    state->arena = arena;
    state->panel = PANEL_LIST;
    state->gb = gb_init();
    state->queue.mutex = os_mutex_create();
    state->queue.condvar = os_cond_create();

    strview_t username = STRV_EMPTY;
    if (argc == 2) {
        username = strv(argv[1]);
    }
    else {
        str_t username_string = gb_current_username(&state->gb, &state->arena);
        username = strv(username_string);
    }

    gbparam_t params[] = {
        { strv("username"), username },
        { strv("order_by"), strv("updated_at") },
    };
    str_t res = gb_make_request(
        &state->gb, 
        &(gbreq_t){
            .arena = &state->arena,
            .scratch = scratch,
            .page = strv("projects/"BIM_ID"/pipelines"),
            .params = params,
            .param_count = arrlen(params),
        }
    );

    json_t *root = json_parse_str(&state->arena, strv(res), JSON_DEFAULT);
    
    json_for(p, root) {
        json_t *json_id     = json_get(p, strv("id"));
        json_t *json_sha    = json_get(p, strv("sha"));
        json_t *json_status = json_get(p, strv("status"));

     
        gb_status_e status = GB_STATUS_NONE;
        for (int i = 0; i < GB_STATUS__COUNT; ++i) {
            if (strv_equals(json_status->string, gb_status_names[i])) {
                status = i;
                break;
            }
        }

        if (status == GB_STATUS_NONE) {
            fatal("status: %v", json_status->string);
        }

        colla_assert(state->pipeline_count < arrlen(state->pipelines), "too many pipelines");

        uptr index = state->pipeline_count++;

        state->pipelines[index] = (pipeline_t){
            .id     = (u64)json_id->number,
            .sha    = str(&arena, json_sha->string),
            .status = status,
        };

        gb_jq_push(&state->queue, worker_load_pipeline_job, (void*)index);
    }

    if (state->pipeline_count == 0) {
        err("no pipelines found for this user!");
        return 1;
    }

    state->worker_count = MIN(arrlen(state->workers), state->pipeline_count / 2);
    if (state->worker_count == 0) {
        state->worker_count = 1;
    }

    for (int i = 0; i < state->worker_count; ++i) {
        state->workers[i] = os_thread_launch(worker_thread, state);
    }

    tui_init(&(tui_desc_t){
        .update = update,
        .event = event,
        .userdata = state,
    });
    tui_run();
}

