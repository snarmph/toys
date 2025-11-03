#include "src/colla/colla.c"

#define UTIL_DIR "Desktop\\utils\\"

str_t error(void) {
    return os_get_error_string(os_get_last_error());
}

int main() {
    colla_init(COLLA_OS);
    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));
    os_log_set_options(OS_LOG_NOFILE);
    os_file_delete(strv("build/toys.exe"));

    os_cmd_t *cmd = NULL;
    darr_push(&arena, cmd, strv("nob"));
    darr_push(&arena, cmd, strv("-O"));
    darr_push(&arena, cmd, strv("fast"));
    darr_push(&arena, cmd, strv("-w "));
    darr_push(&arena, cmd, strv("all"));
    darr_push(&arena, cmd, strv("src\\toys.c"));

    if (!os_run_cmd(arena, cmd, NULL)) {
        fatal("could not build toys.c");
    }

    info("built toys.c");

    str_t home_dir = os_get_env_var(&arena, strv("USERPROFILE"));
    str_t util_dir = str_fmt(&arena, "%v\\" UTIL_DIR, home_dir);

    str_t path = str_fmt(&arena, "%vtoys.exe", util_dir);
    info("path: %v", path);
    if (!os_file_delete(strv(path))) {
        fatal("couldn't delete %v: %v", path, error());
    }

    info("deleted %v", path);
    
    cmd = NULL;
    darr_push(&arena, cmd, strv("cmd"));
    darr_push(&arena, cmd, strv("/C"));
    darr_push(&arena, cmd, strv("move"));
    darr_push(&arena, cmd, strv("build\\toys.exe"));
    darr_push(&arena, cmd, strv(path));

    if (!os_run_cmd(arena, cmd, NULL)) {
        fatal("could not move toys.exe");
    }

    info("moved toys to %v", path);

    cmd = NULL;
    darr_push(&arena, cmd, strv("toys"));
    darr_push(&arena, cmd, strv("-l"));
    oshandle_t hout = os_handle_zero();

    if (!os_run_cmd(arena, cmd, &(os_cmd_options_t){ .out = &hout })) {
        fatal("could not run toys -l to get a list of toys");
    }

    outstream_t ostr = ostr_init(&arena);
    char buf[KB(10)];
    while (true) {
        usize read = os_file_read(hout, buf, sizeof(buf));
        if (read == 0) break;
        ostr_puts(&ostr, strv(buf, read));
    }

    str_t list = ostr_to_str(&ostr);
    instream_t in = istr_init(strv(list));
    while (!istr_is_finished(&in)) {
        strview_t toy = istr_get_line(&in);
        str_t path = str_fmt(&arena, "%v%v.bat", util_dir, toy);
        oshandle_t fp = os_file_open(strv(path), OS_FILE_WRITE);
        if (!os_handle_valid(fp)) {
            err("couldn't create %v.bat: %v", toy, error());
            continue;
        }
        os_file_print(arena, fp, "@echo off\n", toy);
        os_file_print(arena, fp, "toys %v %%*", toy);
        os_file_close(fp);
        info("created %v.bat", toy);
    }
}
