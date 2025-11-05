#include "colla/colla.h" 
#include "common.h"
#include "toys.h"

TOY_SHORT_DESC(stat, "Display file/s status.");

int stat__count = 0;
char spacing[] = 
    "\n" TERM_FG_DARK_GREY 
    "-------------------------------"
    "\n" TERM_RESET;

void stat_impl(arena_t scratch, strview_t filename, void *userdata) {
    COLLA_UNUSED(scratch); COLLA_UNUSED(userdata); 

    if (stat__count++ > 0) {
        print(spacing);
    }

    if (!os_file_exists(filename)) {
        err("file %v doesn't exist", filename);
        return;
    }
    oshandle_t fp = os_file_open(filename, OS_FILE_READ);
    usize size = os_file_size(fp);

    FILETIME write = {0};
    GetFileTime((HANDLE)fp.data, NULL, NULL, &write);

    SYSTEMTIME system_time = {0};
    FileTimeToSystemTime(&write, &system_time);

    println(
        TERM_FG_ORANGE "name:       " TERM_RESET "%v\n"
        TERM_FG_ORANGE "size:       " TERM_RESET "%_$$$dB\n"
        TERM_FG_ORANGE "last write: " TERM_RESET "%02d/%02d/%04d %02d:%02d:%02d",
        filename, size,
        system_time.wDay, system_time.wMonth, system_time.wYear,
        system_time.wHour, system_time.wMinute, system_time.wSecond
    );
}

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

    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));

    glob_t glob_desc = {
        .recursive = true,
        .cb = stat_impl,
    };

    for (i64 i = 0; i < file_count; ++i) {
        if (common_is_glob(files[i])) {
            glob_desc.exp = files[i];
            common_glob(arena, &glob_desc);
        }
        else {
            stat_impl(arena, files[i], NULL);
        }
    }
}
