#include "colla/colla.h" 
#include "common.h"
#include "toys.h"

#include <conio.h>
#include <RestartManager.h>
#pragma comment(lib, "Rstrtmgr")

_Static_assert(sizeof(u32) == sizeof(DWORD), "");

#define UNLOCK_MAX_FILES 1024

TOY_SHORT_DESC(unlock, "Unlock files or folders from other programs.");

TOY_OPTION_DEFINE(unlock) {
    strview_t files[UNLOCK_MAX_FILES];
    i64 file_count;
    bool in_piped;
    bool dont_ask;
    bool null;
    bool quiet;

    DWORD session;
};

void TOY_OPTION_PARSE(unlock)(int argc, char **argv, TOY_OPTION(unlock) *opt) {
    opt->in_piped = common_is_piped(os_stdin());
    usage_helper(
        "unlock [options] [FILE]...", 
        "Unlock files ro folders from other programs. "
        "Files can be either passed directly or piped from stdin.", 
        opt->in_piped ? USAGE_ALLOW_NO_ARGS : USAGE_DEFAULT, 
        USAGE_EXTRA_PARAMS(opt->files, opt->file_count), 
        argc, argv,
        {
            'y', "yes",
            "Don't ask yes or no, just kill the program.",
            USAGE_BOOL(opt->dont_ask),
        },
        {
            '0', "null",
            "Files from stdin are divided by null instead of newlines.",
            USAGE_BOOL(opt->null),
        },
        {
            'q', "quiet",
            "Don't print anything outside of the prompt.",
            USAGE_BOOL(opt->quiet),
        },
    );
}

void unlock_file(arena_t scratch, strview_t filename, TOY_OPTION(unlock) *opt) {
    str16_t fname = strv_to_str16(&scratch, filename);
    const u16 *files[] = { fname.buf };
    DWORD error = RmRegisterResources(
        opt->session, 
        arrlen(files), files, 
        0, NULL, 0, NULL
    );
    if (error) {
        if (!opt->quiet) {
            err("failed to register resources for %v: %v",
                filename, os_get_error_string(os_get_last_error()));
        }
        return;
    }

    DWORD reason = 0;
    uint proc_info_count = 0;
    RM_PROCESS_INFO info[64];
    uint proc_info = arrlen(info);
    error = RmGetList(
        opt->session, 
        &proc_info_count, &proc_info, 
        info, 
        &reason
    );
    if (error) {
        if (!opt->quiet) {
            err("failed to get list of processes for %v: %v",
                filename, os_get_error_string(os_get_last_error()));
        }
        return;
    }
    
    if (proc_info == 0) {
        if (!opt->quiet) {
            println("no process is locking %v", filename);
        }
        return;
    }

    if (!opt->quiet) {
        println("processes locking %v:", filename);
        for (uint i = 0; i < proc_info; ++i) {
            arena_t tmp = scratch;
            str_t name = str_from_str16(
                &tmp, 
                str16_init(info[i].strAppName, 0)
            );
            println("  %v", name);
        }
    }

    bool is_plural = proc_info > 1;
    strview_t pron[2] = { strv("it"), strv("them") };
    
    str_t prompt = str_fmt(&scratch, "do you want to kill %v", pron[is_plural]);
    // don't kill
    if (!opt->dont_ask && !common_prompt(strv(prompt))) {
        return;
    }

    for (uint i = 0; i < proc_info; ++i) {
        HANDLE process = OpenProcess(
            PROCESS_TERMINATE, 
            false, 
            info[i].Process.dwProcessId
        );
        if (process == INVALID_HANDLE_VALUE) {
            if (!opt->quiet) {
                err("failed to get handle for process %v: %v",
                    str_from_str16(
                        &scratch, 
                        str16_init(info[i].strAppName, 0)
                    ), 
                    os_get_error_string(os_get_last_error())
                );
            }
            continue;
        }
        TerminateProcess(process, 1);
        CloseHandle(process);
    }
}

void TOY(unlock)(int argc, char **argv) {
    TOY_OPTION(unlock) opt = {0};
    TOY_OPTION_PARSE(unlock)(argc, argv, &opt);

    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));
    if (opt.in_piped) {
        str_t input = common_read_buffered(&arena, os_stdin());
        instream_t in = istr_init(strv(input));
        while (!istr_is_finished(&in)) {
            strview_t line = STRV_EMPTY;
            if (opt.null) {
                line = istr_get_view(&in, '\0');
            }
            else {
                line = istr_get_line(&in);
            }
            if (line.len == 0) {
                continue;
            }
            if (opt.file_count >= UNLOCK_MAX_FILES) {
                fatal("too many files passed to unlock");
            }
            opt.files[opt.file_count++] = line;
        }
    }

    wchar_t session_key[CCH_RM_SESSION_KEY+1] = {0};
    DWORD error = RmStartSession(&opt.session, 0, session_key);
    if (error) {
        fatal("failed RmStartSession: %v", os_get_error_string(os_get_last_error()));
    }
    
    for (i64 i = 0; i < opt.file_count; ++i) {
        unlock_file(arena, opt.files[i], &opt);
    }

    RmEndSession(opt.session);
}

#if 0
void check(DWORD e, const char *fn) {
    if (e) {
        fatal("%s failed: %v", fn, os_get_error_string(e));
    }
}

char get_option(strview_t options) {
    char chosen = '\0';
    while (true) {
        chosen = char_lower((char)_getch());
        if (strv_contains(options, chosen)) {
            break;
        }
        println("");
        warn("unreacognized option %c", chosen);
        print("> ");
    }
    println("");
    return chosen;
}

void kill(uint pid) {
    HANDLE process = OpenProcess(PROCESS_TERMINATE, false, pid);
    if (process == INVALID_HANDLE_VALUE) {
        warn("process %u not found", pid);
        return;
    }
    TerminateProcess(process, 1);
    CloseHandle(process);
}

int main(int argc, char **argv) {
    os_init();

    if (argc != 2) {
        print_usage();
    }

    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));

    strview_t fname = strv(argv[1]);
    str16_t filename = strv_to_str16(&arena, fname);

    wchar_t session_key[CCH_RM_SESSION_KEY+1] = {0};
    DWORD session = 0;

    DWORD error = RmStartSession(&session, 0, session_key);
    check(error, "RmStartSession");
    
    error = RmRegisterResources(
        session, 
        1, 
        (const u16**)&filename.buf, 
        0, 
        NULL, 
        0, 
        NULL); 
    check(error, "RmRegisterResources");

    DWORD reason = 0;
    uint proc_info_count;
    uint proc_info = 10;
    RM_PROCESS_INFO info[10];
    error = RmGetList(session, &proc_info_count, &proc_info, info, &reason);
    check(error, "RmGetList");

    if (proc_info == 0) {
        info("no apps are locking %v", fname);
        return 0;
    }

    println("apps locking %v:", fname);
    for (uint i = 0; i < proc_info; ++i) {
        arena_t scratch = arena;
        str_t app_name  = str_from_str16(&scratch, str16_init(info[i].strAppName, 0));

        println("  %v", app_name);
    }

    bool is_plural = proc_info > 1;
    strview_t pron[2] = { strv("it"), strv("them") };

    pretty_print(arena, "do you want to kill %v? (<green>Y</>es, <yellow>S</>ome, <red>N</>o)", pron[is_plural]);
    char choice = get_option(strv("ysn"));

    if (choice == 'n') {
        return 0;
    }

    if (choice == 'y' || choice == 's') {
        for (uint i = 0; i < proc_info; ++i) {
            bool should_kill = true;
            if (choice == 's') {
                arena_t scratch = arena;
                str_t app_name  = str_from_str16(&scratch, str16_init(info[i].strAppName, 0));

                pretty_print(scratch, "kill %v? (<green>y</>/<red>n</>)", app_name);
                char kill_choice = get_option(strv("yn"));
                should_kill = kill_choice == 'y';
            }

            if (should_kill) {
                kill(info[i].Process.dwProcessId);
            }
        }

    }

    RmEndSession(session);
}
#endif
