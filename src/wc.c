#include "colla/colla.h" 
#include "common.h"
#include "toys.h"

TOY_SHORT_DESC(wc, "Print newline, word, and byte counts for each file .");

#define WC_MAX_ITEMS 1024

TOY_OPTION_DEFINE(wc) {
    bool in_piped;
    strview_t files[WC_MAX_ITEMS];
    i64 file_count;
    bool print_bytes;
    bool print_chars;
    bool print_lines;
    bool print_words;
    bool print_fname;
    bool print_max_len;
};

void TOY_OPTION_PARSE(wc)(int argc, char **argv, TOY_OPTION(wc) *opt) {
    opt->in_piped = common_is_piped(os_stdin());

    usage_helper(
        "wc [options] [FILE]...", 
        "Print newline, word, byte counts and filename for "
        "each FILE, and a total line if more than one FILE is "
        "specified. With no FILE, or when FILE is -, read standard "
        "input.", 
        opt->in_piped ? USAGE_ALLOW_NO_ARGS : USAGE_DEFAULT, 
        USAGE_EXTRA_PARAMS(opt->files, opt->file_count), 
        argc, argv,
        {
            'b', "bytes",
            "Print the byte counts.",
            USAGE_BOOL(opt->print_bytes),
        },
        {
            'c', "chars",
            "Print the character counts.",
            USAGE_BOOL(opt->print_chars),
        },
        {
            'l', "lines",
            "Print the newline counts.",
            USAGE_BOOL(opt->print_lines),
        },
        {
            'w', "words",
            "Print the word counts.",
            USAGE_BOOL(opt->print_words),
        },
        {
            'f', "filename",
            "Print the filename.",
            USAGE_BOOL(opt->print_fname),
        },
        {
            'L', "max-line-length",
            "Print the length of the longest line.",
            USAGE_BOOL(opt->print_max_len),
        },
    );

    if (!(opt->print_bytes || opt->print_chars || opt->print_lines ||
          opt->print_words || opt->print_fname)
        ) {
        opt->print_lines = true;
        opt->print_words = true;
        opt->print_bytes = true;
        opt->print_fname = true;
    }
}

typedef struct wc_info_t wc_info_t;
struct wc_info_t {
    strview_t filename;
    i64 bytes;
    i64 chars;
    i64 lines;
    i64 words;
    i64 max_len;
};

typedef struct wc_desc_t wc_desc_t;
struct wc_desc_t {
    wc_info_t info[WC_MAX_ITEMS + 1];
    int count;
};

i64 wc_words(strview_t line) {
    i64 count = 0;
    instream_t in = istr_init(line);
    while (!istr_is_finished(&in)) {
        strview_t word = istr_get_word(&in);
        istr_skip_whitespace(&in);
        count += word.len > 0;
    }
    return count;
}

void wc_count(arena_t scratch, oshandle_t fp, wc_info_t *out, TOY_OPTION(wc) *opt) {
    str_t data = common_read_buffered(&scratch, fp);
    out->bytes = data.len;

    i64 max_len = 0;
    i64 word_count = 0;
    i64 line_count = 0;
    i64 chars_count = 0;

    instream_t in = istr_init(strv(data));
    while (!istr_is_finished(&in)) {
        strview_t line = istr_get_line(&in);
        line_count++;
        word_count += wc_words(line);
        max_len = MAX(max_len, (i64)line.len);
    }

    if (opt->print_chars) {
        out->chars = strv_get_utf8_len(strv(data));
    }

    out->chars = chars_count;
    out->words = word_count;
    out->lines = line_count;
    out->max_len = max_len;
}

int wc_count_digits(i64 number) {
    int digits = 1;
    while (number > 10) {
        number /= 10;
        digits++;
    }
    return digits;
}

void TOY(wc)(int argc, char **argv) {
    TOY_OPTION(wc) opt = {0};
    TOY_OPTION_PARSE(wc)(argc, argv, &opt);
    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));
        
    wc_info_t data[WC_MAX_ITEMS + 1];
    bool in_parsed = false;
    for (int i = 0; i < opt.file_count; ++i) {
        wc_info_t *info = &data[i];
        info->filename = opt.files[i];
        oshandle_t fp = os_handle_zero();
        if (!in_parsed && strv_equals(opt.files[i], strv("-"))) {
            info->filename = strv("stdin");
            fp = os_stdin();
            in_parsed = true;
        }
        else {
            fp = os_file_open(opt.files[i], OS_FILE_READ);
        }
        if (os_handle_valid(fp)) {
            wc_count(arena, fp, info, &opt);
            os_file_close(fp);
        }
    }

    i64 count = opt.file_count;
    if (opt.in_piped && !in_parsed) {
        wc_info_t *info = &data[count++];
        info->filename = strv("stdin");
        wc_count(arena, os_stdin(), info, &opt);
    }

    i64 max_bytes  = 0;
    i64 max_chars  = 0;
    i64 max_words  = 0;
    i64 max_lines  = 0;
    i64 max_length = 0;

    for (int i = 0; i < count; ++i) {
        max_bytes = MAX(max_bytes, data[i].bytes);
        max_chars = MAX(max_chars, data[i].chars);
        max_words = MAX(max_words, data[i].words);
        max_lines = MAX(max_lines, data[i].lines);
        max_length = MAX(max_length, data[i].max_len);
    }

    int bytes_digits  = wc_count_digits(max_bytes);
    int chars_digits  = wc_count_digits(max_chars);
    int words_digits  = wc_count_digits(max_words);
    int lines_digits  = wc_count_digits(max_lines);
    int length_digits = wc_count_digits(max_length);

    for (int i = 0; i < count; ++i) {
        wc_info_t *info = &data[i];
        if (opt.print_lines)   print("%*d ", lines_digits, info->lines);
        if (opt.print_words)   print("%*d ", words_digits, info->words);
        if (opt.print_bytes)   print("%*d ", bytes_digits, info->bytes);
        if (opt.print_chars)   print("%*d ", chars_digits, info->chars);
        if (opt.print_max_len) print("%*d ", length_digits, info->max_len);
        if (opt.print_fname)   print("%v", info->filename);
        print("\n");
    }
}
