#include "src/colla/colla.c"
// #include "src/common.h"

char readme_template[] = 
    "# toys\n"
    "\n"
    "Various linux utilities in a single exe file for windows\n"
    "\n"
    "## Usage\n"
    "```cmd\n"
    "toys [toys options] CMD [cmd options]\n"
    "```\n"
    "\n"
    "## Toys:\n"
    "";

int main() {
    colla_init(COLLA_OS);
    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));

    os_cmd_t *cmd = NULL;
    darr_push(&arena, cmd, strv("toys"));
    darr_push(&arena, cmd, strv("-d"));

    oshandle_t hout = os_handle_zero();

    os_run_cmd(arena, cmd, &(os_cmd_options_t){ .out = &hout });

    str_t data = os_file_read_all_str_fp(&arena, hout);
    instream_t in = istr_init(strv(data));
    outstream_t out = ostr_init(&arena);
    ostr_puts(&out, strv(readme_template));

    while (!istr_is_finished(&in)) {
        strview_t toy = istr_get_word(&in);
        strview_t desc = strv_trim(istr_get_line(&in));
        ostr_print(&out, " - ***%v***: %v\n", toy, desc);
    }

    str_t readme = ostr_to_str(&out);
    os_file_write_all_str(strv("readme.md"), strv(readme));
}
