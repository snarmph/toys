#include "colla/colla.h"
#include "common.h"
#include "toys.h"

#define SLEEP_MAX_ARGS 1024

TOY_SHORT_DESC(sleep, "Delay for a specified amount of time.");

bool glob_matches(strview_t glob, strview_t text);

void TOY(sleep)(int argc, char **argv) {
    strview_t time[SLEEP_MAX_ARGS] = {0};
    i64 count = 0;
    usage_helper(
        "sleep TIME",
        "Pause for NUMBER seconds, where NUMBER is an integer or "
        "floating-point.  SUFFIX may be 's','m','h', or 'd', for "
        "seconds, minutes, hours, days.  With multiple arguments, "
        "pause for the sum of their values.",
        USAGE_DEFAULT,
        USAGE_EXTRA_PARAMS(time, count),
        argc, argv,
    );

    u64 total_seconds = 0;
    for (i64 i = 0; i < count; ++i) {
        instream_t in = istr_init(time[i]);
        double seconds = 0;
        istr_get_num(&in, &seconds);
        switch (istr_get(&in)) {
            case 'm':
                seconds *= 60.0;
                break;
            case 'h':
                seconds *= 3600.0;
                break;
            case 'd':
                seconds *= 86400.0;
                break;
        }
        if (seconds < 0.0 || 
            ((double)total_seconds + seconds) >= (double)UINT64_MAX
        ) {
            fatal("waiting for too many seconds");
        }
        total_seconds += (u64)seconds;
    }

    u64 total_ms = total_seconds * 1000;
    while (total_ms >= UINT32_MAX) {
        DWORD ms = UINT32_MAX - 1;
        total_ms -= ms;
        Sleep(ms);
    }
    Sleep((DWORD)total_ms);
}
