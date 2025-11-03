#include "colla/colla.h" 
#include "toys.h"

TOY_SHORT_DESC(stat, "Display file/s status.");

void TOY(stat)(int argc, char **argv) {
    strview_t files[1024] = {0};
    i64 file_count = 0;

    usage_helper(
        "state FILE...",
        "Display file/s status",
        USAGE_DEFAULT,
        USAGE_EXTRA_PARAMS(files, file_count),
        argc, argv
    );

    for (i64 i = 0; i < file_count; ++i) {
        strview_t f = files[i];
        if (!os_file_exists(f)) {
            fatal("file %v doesn't exist", f);
            continue;
        }
        oshandle_t fp = os_file_open(f, OS_FILE_READ);
        usize size = os_file_size(fp);

        FILETIME write = {0};
        GetFileTime((HANDLE)fp.data, NULL, NULL, &write);

        SYSTEMTIME system_time = {0};
        FileTimeToSystemTime(&write, &system_time);

        println(
            TERM_FG_ORANGE "name:       " TERM_RESET "%v\n"
            TERM_FG_ORANGE "size:       " TERM_RESET "%_$$$dB\n"
            TERM_FG_ORANGE "last write: " TERM_RESET "%02d/%02d/%04d %02d:%02d:%02d",
            f, size,
            system_time.wDay, system_time.wMonth, system_time.wYear,
            system_time.wHour, system_time.wMinute, system_time.wSecond
        );
    }
}
