#include "colla/colla.h"
#include "common.h"
#include "toys.h"

TOY_SHORT_DESC(tac, "Output lines in reverse order.");

void TOY(tac)(int argc, char **argv) {
    usage_helper(
        "tac", 
        "Output lines in reverse order.", 
        USAGE_ALLOW_NO_ARGS, 
        USAGE_NO_EXTRA(), 
        argc, argv
    );
    
    if (!common_is_piped(os_stdin())) {
        return;
    }

    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));
    str_t data = common_read_buffered(&arena, os_stdin());
    strview_t lines = strv(data);

    if (strv_back(lines) == '\r') {
        lines.len--;
    }
    if (strv_back(lines) == '\n') {
        lines.len--;
    }

    while (lines.len) {
        usize pos = strv_rfind(lines, '\n', 0);
        strview_t line = pos != STR_END ? strv_sub(lines, pos + 1, STR_END) : STRV_EMPTY;
        lines = strv_sub(lines, 0, pos - 1);
        if (line.len == 0) {
            break;
        }
        println("%v", line);
    }
}
