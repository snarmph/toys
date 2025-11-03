#include "colla/colla.h" 
#include "common.h"
#include "toys.h"

#include <wininet.h>
#include <winsock2.h>
#include <ws2tcpip.h>

TOY_SHORT_DESC(serve, "Serve contents of directory as static web pages.");

TOY_OPTION_DEFINE(serve) {
    strview_t dir;
    i64 threads;
    u16 port;
    bool verbose;
    os_barrier_t thread_barrier;
    socket_t server_socket;
};

void TOY_OPTION_PARSE(serve)(arena_t scratch, int argc, char **argv, TOY_OPTION(serve) *opt) {
    strview_t dirs[1] = {0};
    i64 dircount = 0;

    strview_t encode = STRV_EMPTY;
    strview_t decode = STRV_EMPTY;
    i64 port = 80;

    usage_helper(
        "serve [options] [DIR]",
        "Serve contents of directory as static web pages.",
        USAGE_ALLOW_NO_ARGS,
        USAGE_EXTRA_PARAMS(dirs, dircount),
        argc, argv,
        {
            'e', "encode",
            "Escape {} to make it url-safe, printing result and exiting.",
            "string",
            USAGE_VALUE(encode),
        },
        {
            'd', "decode",
            "Decode escaped {}, printing result and exiting.",
            "string",
            USAGE_VALUE(decode),
        },
        {
            'p', "port",
            "Which {} to serve the folder one, default is 80.",
            "port",
            USAGE_INT(port),
        },
        {
            'j', "threads",
            "Thread {}, if 0 it will use as many as possible.",
            "count",
            USAGE_INT(opt->threads),
        },
        {
            'v', "verbose",
            "",
            USAGE_BOOL(opt->verbose),
        },
    );

    if (encode.len && decode.len) {
        fatal("encode and decode flags are mutually exclusive");
    }

    if (encode.len) {
        // stdin
        if (strv_equals(encode, strv("-"))) {
            str_t data = common_read_buffered(&scratch, os_stdin());
            encode = strv(data);
        }
        str_t encoded = http_make_url_safe(&scratch, encode);
        println("%v", encoded);
        os_abort(0);
    }
    if (decode.len) {
        // stdin
        if (strv_equals(decode, strv("-"))) {
            str_t data = common_read_buffered(&scratch, os_stdin());
            decode = strv(data);
        }
        str_t decoded = http_decode_url_safe(&scratch, decode);
        println("%v", decoded);
        os_abort(0);
    }

    if (port > UINT16_MAX) {
        fatal("port outside of range: %d", port);
    }

    opt->port = (u16)port;

    opt->dir = dircount > 0 ? dirs[0] : strv(".");
    if (opt->threads == 0) {
        opt->threads = os_get_system_info().processor_count;
    }
}

strview_t mime(strview_t filename) {
    struct {
        strview_t ext;
        strview_t val;
    } types[] = {
        { cstrv(".asc"),  cstrv("text/plain"), },
        { cstrv(".bin"),  cstrv("application/octet-stream"), },
        { cstrv(".bmp"),  cstrv("image/bmp"), },
        { cstrv(".cpio"), cstrv("application/x-cpio"), },
        { cstrv(".css"),  cstrv("text/css"), },
        { cstrv(".doc"),  cstrv("application/msword"), },
        { cstrv(".dtd"),  cstrv("text/xml"), },
        { cstrv(".dvi"),  cstrv("application/x-dvi"), },
        { cstrv(".gif"),  cstrv("image/gif"), },
        { cstrv(".htm"),  cstrv("text/html"), },
        { cstrv(".html"), cstrv("text/html"), },
        { cstrv(".jar"),  cstrv("application/x-java-archive"), },
        { cstrv(".jpeg"), cstrv("image/jpeg"), },
        { cstrv(".jpg"),  cstrv("image/jpeg"), },
        { cstrv(".js"),   cstrv("application/x-javascript"), },
        { cstrv(".mp3"),  cstrv("audio/mpeg"), },
        { cstrv(".mp4"),  cstrv("video/mp4"), },
        { cstrv(".mpg"),  cstrv("video/mpeg"), },
        { cstrv(".ogg"),  cstrv("application/ogg"), },
        { cstrv(".pbm"),  cstrv("image/x-portable-bitmap"), },
        { cstrv(".pdf"),  cstrv("application/pdf"), },
        { cstrv(".png"),  cstrv("image/png"), },
        { cstrv(".ppt"),  cstrv("application/vnd.ms-powerpoint"), },
        { cstrv(".ps"),   cstrv("application/postscript"), },
        { cstrv(".rtf"),  cstrv("text/rtf"), },
        { cstrv(".sgml"), cstrv("text/sgml"), },
        { cstrv(".svg"),  cstrv("image/svg+xml"), },
        { cstrv(".tar"),  cstrv("application/x-tar"), },
        { cstrv(".tex"),  cstrv("application/x-tex"), },
        { cstrv(".tiff"), cstrv("image/tiff"), },
        { cstrv(".txt"),  cstrv("text/plain"), },
        { cstrv(".wav"),  cstrv("audio/x-wav"), },
        { cstrv(".xls"),  cstrv("application/vnd.ms-excel"), },
        { cstrv(".xml"),  cstrv("text/xml"), },
        { cstrv(".zip"),  cstrv("application/zip") },
    };
    strview_t ext;
    os_file_split_path(filename, NULL, NULL, &ext);
    for (int i = 0; i < arrlen(types); ++i) {
        if (strv_equals(types[i].ext, ext)) {
            return types[i].val;
        }
    }
    return STRV_EMPTY;
}

void send_file(arena_t scratch, oshandle_t fp, usize size, socket_t client) {
    // if too big, send in chunks
    if (size > arena_remaining(&scratch)) {
        usize bufsize = arena_remaining(&scratch);
        u8 *buf = alloc(&scratch, u8, bufsize);
        while (true) {
            usize read = os_file_read(fp, buf, bufsize);
            sk_send(client, buf, (int)read);
            if (read < bufsize) break;
        }
    }
    // send all at once
    else {
        buffer_t buf = os_file_read_all_fp(&scratch, fp);
        sk_send(client, buf.data, (int)buf.len);
    }
}

void serve_entry_point(void *udata) {
    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));
    TOY_OPTION(serve) *opt = udata;

    if (os_thread_id == 0) {
        opt->server_socket = sk_open(SOCK_TCP);
        struct sockaddr_in addr = {
            .sin_family = AF_INET,
            .sin_port = htons(opt->port),
            .sin_addr.s_addr = INADDR_ANY,
        };

        if (bind(opt->server_socket, (struct sockaddr *)&addr, sizeof(addr))) {
            fatal("failed to bind on port %u", opt->port);
        }

        sk_listen(opt->server_socket, 100);

        if (opt->verbose) {
            info("server open at localhost:%d using %d threads", opt->port, opt->threads);
        }
    }
    
    os_barrier_sync(&opt->thread_barrier);

    while (true) {
        socket_t client = sk_accept(opt->server_socket);
        if (!sk_is_valid(client)) {
            continue;
        }

        arena_t scratch = arena;
        outstream_t out = ostr_init(&scratch);
        u8 buf[KB(5)] = {0};
        while (true) {
            int read = sk_recv(client, buf, sizeof(buf));
            if (read < 0) {
                break;
            }
            ostr_puts(&out, strv((char*)buf, read));
            if (read < sizeof(buf)) {
                break;
            }
        }

        str_t req_str = ostr_to_str(&out);
        http_req_t req = http_parse_req(&scratch, strv(req_str));
        int code = 200;
        strview_t mime_type = STRV_EMPTY;
        strview_t filename = STRV_EMPTY;

        if (req.method != HTTP_GET) {
            if (opt->verbose) warn("501: method %s is not supported", http_get_method_string(req.method));
            code = 501;
            goto close;
        }

        str_t path = os_path_join(&scratch, opt->dir, req.url);
        if (os_file_exists(strv(path))) {
            filename = strv(path);
        }
        else {
            str_t index = os_path_join(&scratch, strv(path), strv("index.html"));
            if (!os_file_exists(strv(index))) {
                if (opt->verbose) warn("404: file %v not found", path);
                code = 404;
                goto close;
            }
            filename = strv(index);
        }

close:
        http_res_t res = {
            .version = { 1, 1 },
            .status_code = code,
            .headers = NULL,
        };
        http_add_header(&scratch, res.headers, strv("Connection"), strv("close"));
        if (filename.len > 0) {
            oshandle_t fp = os_file_open(filename, OS_FILE_READ);
            usize file_size = os_file_size(fp);
            str_t content_length = str_fmt(&scratch, "%zu", file_size);
            http_add_header(&scratch, res.headers, strv("Content-Length"), strv(content_length));
            mime_type = mime(filename);
            if (mime_type.len > 0) {
                http_add_header(&scratch, res.headers, strv("Content-Type"), mime_type);
            }
            str_t response = http_res_to_str(&scratch, &res);
            sk_send(client, response.buf, (int)response.len);

            if (opt->verbose) info("200: requesting %v", filename);
            send_file(scratch, fp, file_size, client);
            os_file_close(fp);
        }
        else {
            str_t response = http_res_to_str(&scratch, &res);
            sk_send(client, response.buf, (int)response.len);
        }
        sk_close(client);
    }
}

int serve_thread_entry_point(u64 id, void *udata) {
    COLLA_UNUSED(id); 
    serve_entry_point(udata);
    // unreachable
    return 0;
}

void TOY(serve)(int argc, char **argv) {
    net_init();

    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));

    TOY_OPTION(serve) opt = {0};
    TOY_OPTION_PARSE(serve)(arena, argc, argv, &opt);

    if (opt.verbose) {
        os_log_set_options(OS_LOG_NOFILE);
    }

    opt.thread_barrier.thread_count = opt.threads;
    oshandle_t *threads = alloc(&arena, oshandle_t, opt.threads);

    for (int i = 0; i < opt.threads; ++i) {
        threads[i] = os_thread_launch(serve_thread_entry_point, &opt);
    }

    for (int i = 0; i < opt.threads; ++i) {
        os_thread_join(threads[i], NULL);
    }
}

