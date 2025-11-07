#include "colla/colla.h" 
#include "common.h"
#include "toys.h"

TOY_SHORT_DESC(strings, "Display printable strings in a binary file.");

#define SS_MAX_FILES 1024

typedef enum {
    OFFSET_NONE,
    OFFSET_OCT,
    OFFSET_DEC,
    OFFSET_HEX,
} ss_offset_type_e;

typedef struct {
    bool show_filename;
    i64 min_len;
    ss_offset_type_e off;
    strview_t files[SS_MAX_FILES];
    i64 file_count;
} strings_opt_t;

void strings_parse_opts(int argc, char **argv, strings_opt_t *opt) {
    bool decimal = false, octal = false, hex = false;
    strview_t radix = {0};

    usage_helper(
        "strings [options] [FILE...]",
        "Display printable strings in file/s",
        USAGE_DEFAULT,
        USAGE_EXTRA_PARAMS(opt->files, opt->file_count),
        argc, argv,
        {
            'f', "print-file-name",
            "Print the name of the file before each string.",
            USAGE_BOOL(opt->show_filename),
        },
        {
            'n', "bytes",
            "Print sequences of displayable characters that "
            "are at least {} characters long."
            "If not specified a default {} of 4 is used.",
            "min-len",
            USAGE_INT(opt->min_len)
        },
        {
            'd', "decimal",
            "Like -t d, show offset in decimal.",
            USAGE_BOOL(decimal),
        },
        {
            'o', "octal",
            "Like -t o, show offset in octal.",
            USAGE_BOOL(octal),
        },
        {
            'x', "hex",
            "Like -t x, show offset in hex.",
            USAGE_BOOL(hex),
        },
        {
            't', "radix",
            "Print the offset within the file before each string."
            "The single character argument specifies the {} of "
            "the offset---o for octal, x for hexadecimal, or d for decimal",
            "radix",
        },
    );

    int radix_all = (int)decimal + (int)octal + (int)hex;

    if (radix_all > 1 || radix_all > 0 && radix.len > 0) {
        fatal("cannot set more than one radix type");
    }

    if (radix.len > 1) {
        fatal("unrecognized radix type %v", radix);
    }

    if (decimal) opt->off = OFFSET_DEC;
    if (octal)   opt->off = OFFSET_OCT;
    if (hex)     opt->off = OFFSET_HEX;
    if (radix.len) {
        switch (radix.buf[0]) {
            case 'd': opt->off = OFFSET_DEC; break;
            case 'o': opt->off = OFFSET_OCT; break;
            case 'x': opt->off = OFFSET_HEX; break;
            default:
                fatal("unrecognized radix type: %v", radix);
                break;
        }
    }

    if (opt->min_len == 0) {
        opt->min_len = 4;
    }
}

struct {
    arena_t arena;
    bool start_of_line;
    char printbuf[KB(1)];
    usize offset;
    int count;
    int total_count;
    int digits;
} ss_state = {0};

void ss_print_buf(strview_t fname, strings_opt_t *opt) {
    if (ss_state.start_of_line) {
        ss_state.start_of_line = false;
        if (opt->show_filename) {
            print("%v: ", fname);
        }
        switch (opt->off) {
            case OFFSET_DEC:
                print( TERM_FG_DARK_GREY "%*zu" TERM_RESET, ss_state.digits, ss_state.offset);
                break;
            case OFFSET_OCT:
                print( TERM_FG_DARK_GREY "%*zi" TERM_RESET, ss_state.digits, ss_state.offset);
                break;
            case OFFSET_HEX:
                print( TERM_FG_DARK_GREY "%*zx" TERM_RESET, ss_state.digits, ss_state.offset);
                break;
            default: break;
        }
    }
    print("%.*s", ss_state.count, ss_state.printbuf);
}

int ss_get_digits(usize size, strings_opt_t *opt) {
    int digits = 1;
    usize size_rem = size;
    int base = 10;
    switch (opt->off) {
        case OFFSET_HEX: base = 16; break;
        case OFFSET_OCT: base = 8; break;
        default: break;
    }
    while (size_rem >= base) {
        size_rem /= base;
        digits++;
    }
    return digits;
}

void TOY(strings)(int argc, char **argv) {
    strings_opt_t opt = {0};

    strings_parse_opts(argc, argv, &opt);

    ss_state.arena = arena_make(ARENA_VIRTUAL, GB(1));

    for (i64 i = 0; i < opt.file_count; ++i) {
        oshandle_t fp = os_file_open(opt.files[i], OS_FILE_READ);
        usize file_size = os_file_size(fp);
        if (opt.off) {
            ss_state.digits = ss_get_digits(file_size, &opt);
        }

        char buf[KB(10)] = {0};

        while (true) {
            usize read = os_file_read(fp, buf, sizeof(buf));
            if (read == 0) {
                break;
            }

            for (usize k = 0; k < read; ++k, ++ss_state.offset) {
                if (buf[k] >= 32 && buf[k] <= 126) {
                    ss_state.printbuf[ss_state.count++] = buf[k];
                    ss_state.total_count++;
                    if (ss_state.count >= sizeof(ss_state.printbuf)) {
                        ss_print_buf(opt.files[k], &opt);
                        ss_state.count = 0;
                    }
                }
                else if (ss_state.total_count >= opt.min_len) {
                    ss_print_buf(opt.files[k], &opt);
                    println("");
                    ss_state.total_count = ss_state.count = 0;
                    ss_state.start_of_line = true;
                }
                else {
                    ss_state.total_count = ss_state.count = 0;
                    ss_state.start_of_line = true;
                }
            }

        }
    }
}
