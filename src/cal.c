#include <time.h>

#include "colla/colla.h"
#include "common.h"
#include "toys.h"

#define CAL_LINE_LEN 50

TOY_SHORT_DESC(cal, "Print a calendar.");

TOY_OPTION_DEFINE(cal) {
    int day;
    int month;
    int year;
    int monthlen;
    struct tm current;
};

typedef struct {
    char lines[10][CAL_LINE_LEN];
} month_t;

strview_t month_names[] = {
    cstrv("January"),
    cstrv("February"),
    cstrv("March"),
    cstrv("April"),
    cstrv("May"),
    cstrv("June"),
    cstrv("July"),
    cstrv("August"),
    cstrv("September"),
    cstrv("October"),
    cstrv("November"),
    cstrv("December"),
};

void TOY_OPTION_PARSE(cal)(int argc, char **argv, TOY_OPTION(cal) *opt) {
    strview_t args[3];
    i64 arg_count = 0;

    usage_helper(
        "cal [[[DAY] MONTH] YEAR]", 
        "Print a calendar.\n"
        "With one argument, prints all months of the specified year.\n"
        "With two arguments, prints calendar for month and year.\n"
        "With three arguments, highlights day within month and year.",
        USAGE_ALLOW_NO_ARGS,
        USAGE_EXTRA_PARAMS(args, arg_count),
        argc, argv,
    );

    switch (arg_count) {
        case 1: 
            opt->year  = common_strv_to_int(args[0]);
            break;
        case 2: 
            opt->month = common_strv_to_int(args[0]);
            opt->year  = common_strv_to_int(args[1]);
            break;
        case 3: 
            opt->day   = common_strv_to_int(args[0]);
            opt->month = common_strv_to_int(args[1]);
            opt->year  = common_strv_to_int(args[2]);
            break;
    }
}

int month_length(int month, int year) {
    int len = 31;

    if (month == 1) {
        len = 28;
        if (
            !(year & 3) &&
            !((year % 100) && !(year % 400))
        ) {
            len = 29;
        }
    }
    else if ((month + (month > 6)) & 1) {
        len = 30;
    }

    return len;
}

void month_header(char *line, int month, int year) {
    strview_t month_name = month_names[month];
    int total_width = 21;
    int space_left = total_width - (int)month_name.len - 5; 
    int leftpad = space_left / 2;
    int rightpad = space_left - leftpad;

    fmt_buffer(line, CAL_LINE_LEN, "%*s%v %-4d%*s", leftpad, "", month_name, year, rightpad, "");
}

void week_header(char *line) {
    fmt_buffer(
        line, CAL_LINE_LEN, 
        "\x1b[90m"
        "Su Mo Tu We Th Fr Sa"
        "\x1b[m"
    );
}

int month_first_line(char *line, int wday) {
    arena_t arena = arena_make(ARENA_STATIC, CAL_LINE_LEN, (u8*)line);
    outstream_t out = ostr_init(&arena);

    int d = 1;
    for (int i = 0; i < wday; ++i) {
        ostr_puts(&out, strv("   "));
    }

    for (int i = wday; i < 7; ++i) {
        if (i == 6) ostr_puts(&out, strv("\x1b[33m"));
        ostr_print(&out, "%2d ", d++);
        if (i == 6) ostr_puts(&out, strv("\x1b[m"));
    }

    return d;
}

void month_days(char lines[10][CAL_LINE_LEN], struct tm* tm, TOY_OPTION(cal) *opt) {
    int month = tm->tm_mon;
    int year  = tm->tm_year + 1900;
    int monthlen = month_length(month, year);
    int wday = tm->tm_wday;

    bool is_cur_month = tm->tm_mon == opt->current.tm_mon &&
                        tm->tm_year == opt->current.tm_year;
    int curd = opt->current.tm_mday;

    int d = 1;
    for (int i = 0; i < 6; ++i) {
        char *line = lines[i];
        for (int k = 0; k < 7; ++k) {

            bool is_curd = is_cur_month && d == curd;

            if ((i != 0 || k >= wday) && d <= monthlen) {
                if (is_curd)     line += fmt_buffer(line, CAL_LINE_LEN, "\x1b[42;30m");
                else if (k == 6) line += fmt_buffer(line, CAL_LINE_LEN, "\x1b[33m");

                line += fmt_buffer(line, CAL_LINE_LEN, "%2d", d++);

                if (is_curd || k == 6) line += fmt_buffer(line, CAL_LINE_LEN, "\x1b[m");
            }
            else {
                memcpy(line, "  ", 2);
                line += 2;
            }

            *line++ = ' ';
        }
    }
}

void month_render(month_t *m, struct tm *tm, TOY_OPTION(cal) *opt) {
    int month = tm->tm_mon;
    int year  = tm->tm_year + 1900;

    month_header(m->lines[0], month, year);
    week_header(m->lines[1]);
    month_days(&m->lines[2], tm, opt);
}

void print_month(int mon, int year, TOY_OPTION(cal) *opt) {
    struct tm tm = {
        .tm_year = year - 1900,
        .tm_mon = mon - 1,
        .tm_mday = 1,
    };
    mktime(&tm);
    month_t month = {0};
    month_render(&month, &tm, opt);
    for (int i = 0; i < 10; ++i) {
        char *line = month.lines[i];
        bool is_empty = true;
        for (char *l = line; *l; ++l) {
            if (*l != ' ') {
                is_empty = false;
                break;
            }
        }
        if (is_empty) break;
        // println("%s", line);
        print("%s\n", line);
        print("");
    }
}

void print_year(int year, TOY_OPTION(cal) *opt) {
    struct tm tm = {
        .tm_year = year - 1900,
    };

    month_t months[12] = {0};
    
    tm.tm_mday = 1;
    for (int i = 0; i < 12; ++i) {
        tm.tm_mon = i;
        mktime(&tm);
        
        month_render(&months[i], &tm, opt);
    }

    for (int row = 0; row < 4; ++row) {
        for (int i = 0; i < 10; ++i) {
            bool all_empty = true;
            for (int col = 0; col < 3; ++col) {
                int mon = col + row * 3;
                char *line = months[mon].lines[i];
                print("%s  \x1b[m", line);
                while (*line) {
                    if (*line++ != ' ') {
                        all_empty = false;
                        break;
                    }
                }
            }
            println("");
            if (all_empty) {
                break;
            }
        }
    }
}

void TOY(cal)(int argc, char **argv) {
    TOY_OPTION(cal) opt = {0};
    TOY_OPTION_PARSE(cal)(argc, argv, &opt);

    time_t now = time(0);
    localtime_s(&opt.current, &now);

    if (opt.month) {
        if (opt.month < 1 || opt.month > 12) {
            fatal("month range: [1,12]");
        }
        opt.monthlen = month_length(opt.month, opt.current.tm_year);
        if (opt.day && (opt.day < 1 || opt.day > opt.monthlen)) {
            fatal("day range for %v: [1:%d]", month_names[opt.month-1], opt.monthlen);
        }
    }

    struct tm tm = {0};
    localtime_s(&tm, &now);

    oshandle_t input = os_win_conin();

    DWORD prev = 0;
    GetConsoleMode((HANDLE)input.data, &prev);

    SetConsoleMode((HANDLE)input.data, ENABLE_VIRTUAL_TERMINAL_INPUT);

    if (opt.day) {
        opt.current.tm_mday = opt.day;
        opt.current.tm_mon  = opt.month - 1;
        opt.current.tm_year = opt.year  - 1900;

        print_month(opt.month, opt.year, &opt);
    }
    else if (opt.month) {
        print_month(opt.month, opt.year, &opt);
    }
    else if (opt.year) {
        print_year(opt.year, &opt);
    }
    else {
        print_month(opt.current.tm_mon + 1, opt.current.tm_year + 1900, &opt);
    }

    SetConsoleMode((HANDLE)input.data, (DWORD)(~ENABLE_VIRTUAL_TERMINAL_INPUT));
}
