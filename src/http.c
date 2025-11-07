#include "colla/colla.h" 
#include "common.h"
#include "toys.h"

TOY_SHORT_DESC(http, "Make an HTTP request.");

typedef struct {
    http_method_e method;
    strview_t url;
    http_header_t *headers;
    str_t body;

    bool body_only;
    bool clean;
    bool verbose;
} http_opt_t;

http_header_t *http_parse_header(arena_t *arena, strview_t arg);

void http_parse_opts(arena_t *arena, int argc, char **argv, http_opt_t *opt) {
    strview_t extra[1024] = {0};
    i64 extra_count = 0;

    usage_helper(
        "http [options] [method] URL [headers] [body]",
        "Make an HTTP request.\n"
        TERM_FG_GREEN TERM_ITALIC "method " TERM_RESET 
        "can be one of the following: [get, post, head, put, delete], by default it's GET\n"
        "To add " TERM_FG_GREEN TERM_ITALIC "headers" TERM_RESET ", add them after the url like so: header=value\n"
        "For instance: Content-Type=text/plain\n"
        "Everything after the headers will be sent as a " TERM_FG_GREEN TERM_ITALIC "body" TERM_RESET ".",
        USAGE_DEFAULT,
        USAGE_EXTRA_PARAMS(extra, extra_count),
        argc, argv,
        {
            'b', "body-only",
            "Only print the body.",
            USAGE_BOOL(opt->body_only),
        },
        {
            'c', "clean",
            "Don't do any kind of pretty printing.",
            USAGE_BOOL(opt->clean),
        },
        {
            'v', "verbose",
            "",
            USAGE_BOOL(opt->verbose),
        },
    );

    strview_t str_to_method[HTTP_METHOD__COUNT] = {
        cstrv("GET"),
        cstrv("POST"),
        cstrv("HEAD"),
        cstrv("PUT"),
        cstrv("DELETE"),
    };


    if (extra_count == 0) {
        fatal("no url passed");
    }

    bool found = false;
    if (extra_count > 1) {
        arena_t scratch = *arena;
        str_t method = strv_to_upper(&scratch, extra[0]);

        for (usize i = 0; i < arrlen(str_to_method); ++i) {
            if (strv_equals(strv(method), str_to_method[i])) {
                found = true;
                opt->method = i;
                break;
            }
        }
    }

    opt->url = found ? extra[1] : extra[0];
    i64 cur = 1 + found;

    for (; cur < extra_count; ++cur) {
        if (!strv_contains(extra[cur], '=')) {
            break;
        }
        http_header_t *new_header = http_parse_header(arena, extra[cur]);
        list_push(opt->headers, new_header);
    }

    outstream_t body = ostr_init(arena);
    for (; cur < extra_count; ++cur) {
        if (ostr_tell(&body) != 0) {
            ostr_putc(&body, ' ');
        }
        ostr_puts(&body, extra[cur]);
    }

    opt->body = ostr_to_str(&body);
}

http_header_t *http_parse_header(arena_t *arena, strview_t arg) {
    usize index = strv_find(arg, '=', 0);

    http_header_t *h = alloc(arena, http_header_t);
    h->key = strv_sub(arg, 0, index);
    h->value = strv_sub(arg, index + 1, SIZE_MAX);

    return h;
}

void TOY(http)(int argc, char **argv) {
    net_init();

    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));

    http_opt_t opt = {0};
    http_parse_opts(&arena, argc, argv, &opt);

    if (common_is_piped(os_stdout())) {
        opt.body_only = true;
        opt.clean = true;
    }

    http_request_desc_t req = {
        .arena = &arena,
        .url   = opt.url,
        .request_type = opt.method,
        .headers = opt.headers,
        .body = strv(opt.body),
    };

    if (opt.verbose) {
        println(TERM_FG_DARK_GREY "HTTP/1.1" TERM_RESET);

        for_each (h, req.headers) {
            println(TERM_FG_ORANGE "%v: " TERM_RESET "%v", h->key, h->value);
        }

        print("\n");

        strview_t content_type = http_get_header(req.headers, strv("Content-Type"));

        bool is_json = strv_contains_view(content_type, strv("json"));

        if (is_json) {
            if (!strv_starts_with(req.body, '{')) {
                str_t full = str_fmt(&arena, "{ \"data\": %v }", req.body);
                req.body = strv(full);
            }

            json_t *json = json_parse_str(&arena, req.body, JSON_DEFAULT);

            json_pretty_print(json, &(json_pretty_opts_t){0}); 
        }
        else {
            print("%v\n", req.body);
        }
    }

    http_res_t response = http_request(&req);

    if (response.status_code == 0) {
        os_abort(1);
    }

    if (!opt.body_only) {
        if (opt.clean) {
            print("HTTP/%d.%d, %d %s\n", 
                    response.version.major, response.version.minor,
                    response.status_code, http_get_status_string(response.status_code));

            for_each (h, response.headers) {
                print("%v: %v\n", h->key, h->value);
            }

            print("\n");
        }
        else {
            println(
                TERM_FG_DARK_GREY "HTTP/%d.%d "
                TERM_FG_GREEN "%d " TERM_ITALIC "%s" TERM_RESET,
                response.version.major, response.version.minor,
                response.status_code, http_get_status_string(response.status_code)
            );

            for_each (h, response.headers) {
                println(
                    TERM_FG_ORANGE "%v: " 
                    TERM_FG_YELLOW "%v"
                    TERM_RESET, 
                    h->key, h->value
                );
            }

            os_log_set_colour(LOG_COL_RESET);

            print("\n");
        }
    }

    strview_t content_type = http_get_header(response.headers, strv("Content-Type"));
    
    bool is_json = strv_contains_view(content_type, strv("json"));

    if (!opt.clean && is_json) {
        if (!strv_starts_with(response.body, '{')) {
            str_t full = str_fmt(&arena, "{ \"data\": %v }", response.body);
            response.body = strv(full);
        }

        json_t *json = json_parse_str(&arena, response.body, JSON_DEFAULT);

        json_pretty_print(json, &(json_pretty_opts_t){0}); 
    }
    else {
        println("%v", response.body);
    }
}
