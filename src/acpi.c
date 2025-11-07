#include "colla/colla.h"
#include "common.h"
#include "toys.h"

#include <time.h>

TOY_SHORT_DESC(acpi, "Show battery status.");

typedef enum {
    ACPI_SHOW_REMAINING_TIME = 1 << 3,
    ACPI_SHOW_END_TIME       = 1 << 4,
} acpi_flags_e;

typedef struct {
    acpi_flags_e flags;
} acpi_opt_t;

void acpi_parse_opts(int argc, char **argv, acpi_opt_t *opt) {
    bool rem        = false;
    bool end        = false;
    bool everything = false;

    usage_helper(
        "acpi [options]", 
        "Shows battery status.", 
        USAGE_ALLOW_NO_ARGS, 
        USAGE_NO_EXTRA(), 
        argc, argv,
        {
            'r', "remaining-time",
            "Show remaining time of (dis)charge.",
            USAGE_BOOL(rem),
        },
        {
            'e', "end-time",
            "Show end time of (dis)charge.",
            USAGE_BOOL(end),
        },
       {
            'V', "everything",
            "Show every device, overrides above options.",
            USAGE_BOOL(everything),
        },
    );

    if (rem)     opt->flags |= ACPI_SHOW_REMAINING_TIME;
    if (end)     opt->flags |= ACPI_SHOW_END_TIME;
    if (everything) opt->flags = 0xFFFFFF;
}

void TOY(acpi)(int argc, char **argv) {
    acpi_opt_t opt = {0};
    acpi_parse_opts(argc, argv, &opt);
    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));

    SYSTEM_POWER_STATUS ps = {0};
    if (!GetSystemPowerStatus(&ps)) {
        fatal("failed to get system power status: %v", os_get_error_string(os_get_last_error()));
    }

    if (ps.BatteryLifePercent <= 100) {
        println("Battery: %02d%%", ps.BatteryLifePercent);
    }

    if (opt.flags & ACPI_SHOW_REMAINING_TIME && ps.BatteryLifeTime != -1) {
        os_time_t tm = common_convert_time(ps.BatteryLifeTime);
        str_t rem = common_print_time(&arena, &tm);
        println("Battery time left: %v", rem);
    }

    if (opt.flags & ACPI_SHOW_REMAINING_TIME && ps.BatteryLifeTime != -1) {
        time_t now_time = time(NULL);
        struct tm now = {0};
        localtime_s(&now, &now_time);
        os_time_t left = common_convert_time(ps.BatteryLifeTime);
        u64 end_sec = 
            (left.seconds + now.tm_sec) +
            (left.minutes + now.tm_min) * 60 +
            (left.hours + now.tm_hour) * 3600;

        os_time_t end = common_convert_time(end_sec);
        end.hours %= 24;
        println("Battery should die at: %02d:%02d", end.hours, end.minutes);
    }
}
