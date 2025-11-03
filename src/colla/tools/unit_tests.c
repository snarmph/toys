#include "../colla.c"

#if COLLA_WIN
#pragma section(".CRT$XCU", read)
#endif

#include "../tests/runner.h"

#include "../tests/arena_tests.c"
#include "../tests/core_tests.c"
#include "../tests/net_tests.c"
#include "../tests/os_tests.c"
// #include "../tests/parsers_tests.c"
// #include "../tests/pretty_print_tests.c"
#include "../tests/str_tests.c"

unit_test_t *test_head = NULL;
unit_test_t *test_tail = NULL;
const char *last_fail_reason = NULL;
bool last_failed = false;

void ut_register(const char *file, const char *name, void (*fn)(void)) {
    strview_t fname;
    os_file_split_path(strv(file), NULL, &fname, NULL);

    fname = strv_remove_suffix(fname, arrlen("_tests") - 1);

    unit_test_t *test = calloc(1, sizeof(unit_test_t));
    test->name = strv(name);
    test->fn = fn;
    test->fname = fname;

    olist_push(test_head, test_tail, test);
}

int main() {
    colla_init(COLLA_ALL);
    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));

    strview_t last_file = STRV_EMPTY;
    int success = 0;
    int total = 0;

    unit_test_t *test = test_head;
    while (test) {
        if (!strv_equals(test->fname, last_file)) {
            last_file = test->fname;
            pretty_print(arena, "<blue>> %v</>\n", test->fname);
        }

        test->fn();

        total++;

        if (last_failed) {
            pretty_print(arena, "%4s<red>[X]</> %v: %s\n", "", test->name, last_fail_reason);
        }
        else {
            pretty_print(arena, "%4s<green>[V]</> %v\n", "", test->name);
            success++;
        }

        last_failed = false;

        test = test->next;
    }

    print("\n");

    strview_t colors[] = { 
        cstrv("red"), 
        cstrv("light_red"), 
        cstrv("yellow"), 
        cstrv("light_yellow"), 
        cstrv("light_green"), 
        cstrv("green"), 
    };

    usize col = success * (arrlen(colors) - 1) / total;

    pretty_print(arena, "<%v>%d</>/<blue>%d</> tests passed\n", colors[col], success, total);
}
