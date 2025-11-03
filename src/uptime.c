#include "colla/colla.h" 
#include "common.h"
#include "toys.h"

TOY_SHORT_DESC(uptime, "Tell how long the system has been running.");

void TOY(uptime)(int argc, char **argv) {
    COLLA_UNUSED(argc); COLLA_UNUSED(argv);

    arena_t arena = arena_make(ARENA_VIRTUAL, MB(1));
    
    os_time_t t = common_convert_time(GetTickCount64() / 1000);
    str_t uptime = common_print_time(&arena, &t);
    print("%v", uptime);
}
