#include "colla/colla.h"
#include "common.h"
#include "icons.h"
#include "toys.h"

#if !COLLA_TCC
#include <shellapi.h>
#else
#define SHGFI_USEFILEATTRIBUTES 0x000000010     // use passed dwFileAttribute
#define SHGFI_TYPENAME          0x000000400     // get type name
typedef struct _SHFILEINFOW
{
        HICON       hIcon;                      // out: icon
        int         iIcon;                      // out: icon index
        DWORD       dwAttributes;               // out: SFGAO_ flags
        WCHAR       szDisplayName[MAX_PATH];    // out: display name (or path)
        WCHAR       szTypeName[80];             // out: type name
} SHFILEINFOW;

DWORD_PTR SHGetFileInfoW(LPCWSTR pszPath, DWORD dwFileAttributes, SHFILEINFOW *psfi, UINT cbFileInfo, UINT uFlags);
#endif

#pragma comment(lib, "Shell32.lib")

TOY_SHORT_DESC(ls, "List directory contents.");

TOY_OPTION_DEFINE(ls) {
    bool list_all;
    bool list_extra;
    bool list_dirs;
    bool print_tree;
    int max_depth;
    strview_t dir;
    icon_style_e style;
    bool is_out_piped;
};

typedef struct ls_entry_t ls_entry_t;
struct ls_entry_t {
    str_t path;
    dir_type_e type;
    usize size;
    str_t filetype;
    ls_entry_t *next;
    ls_entry_t *prev;
    ls_entry_t *children;
};

void TOY_OPTION_PARSE(ls)(int argc, char **argv, TOY_OPTION(ls) *opt) {
    strview_t dir[1] = { strv(".") };
    i64 dircount = 0;

    optional(strview_t) tree_depth = {0};

    bool emojis = false;
    bool noicons = false;

    usage_helper(
        "ls [options] [DIR]",
        "List information about the FILEs (the current directory "
        "by default).",
        USAGE_ALLOW_NO_ARGS,
        USAGE_EXTRA_PARAMS(dir, dircount),
        argc, argv,
        {
            'a', "all",
            "Do not ignore entries starting with '.'.",
            USAGE_BOOL(opt->list_all),
        },
        {
            'l', "list",
            "Use a long listing format.",
            USAGE_BOOL(opt->list_extra),
        },
        {
            'd', "dirs",
            "List directories themselves, not their contents.",
            USAGE_BOOL(opt->list_dirs),
        },
        {
            't', "tree",
            "List subdirectories recursively with max {}",
            "DEPTH",
            USAGE_OPT(tree_depth),
        },
        {
            'e', "emojis",
            "Use emojis instead of icons.",
            USAGE_BOOL(emojis),
        },
        {
            'c', "no-icons",
            "Don't use icons.",
            USAGE_BOOL(noicons),
        },
    );

    if (tree_depth.is_set) {
        opt->print_tree = true;
        if (tree_depth.value.len > 0) {
            i64 depth = common_strv_to_int(tree_depth.value);
            // if == 0 it might have failed, because -t has an optional
            // parameter, check if we're not actually asking for a folder
            if (depth == 0 && !char_is_num(tree_depth.value.buf[0])) {
                dir[0] = tree_depth.value;
                dircount = 1;
            }
            opt->max_depth = (int)depth;
        }
    }

    if (noicons) {
        opt->style = ICON_STYLE_NONE;
    }
    else if (emojis) {
        opt->style = ICON_STYLE_EMOJI;
    }
    else {
        opt->style = ICON_STYLE_NERD;
    }

    if (dircount) {
        opt->dir = dir[0];
    }
    
    opt->is_out_piped = common_is_piped(os_stdout());
}

str_t ls_get_file_info(arena_t *arena, strview_t fname) {
    strview_t ext;
    os_file_split_path(fname, NULL, NULL, &ext);
    u8 tmpbuf[64];
    arena_t scratch = arena_make(ARENA_STATIC, sizeof(tmpbuf), tmpbuf);

    str16_t ext_str = strv_to_str16(&scratch, ext);
    SHFILEINFOW info = {0};
    DWORD_PTR success = SHGetFileInfoW(
        ext_str.buf,
        FILE_ATTRIBUTE_NORMAL,
        &info,
        sizeof(info),
        SHGFI_USEFILEATTRIBUTES | SHGFI_TYPENAME
    );
    
    if (!success) {
        return STR_EMPTY;
    }

    str_t typename = str_from_str16(arena, str16_init(info.szTypeName, 0));

    return typename;
}

ls_entry_t *ls_add_dir(arena_t *arena, strview_t path, int depth, TOY_OPTION(ls) *opt) {
    ls_entry_t *head = NULL;
    dir_t *dir = os_dir_open(arena, path);

    dir_foreach (arena, entry, dir) {
        if (!opt->list_all && entry->name.buf[0] == '.') {
            continue;
        }

        if (opt->list_dirs && entry->type == DIRTYPE_FILE) {
            continue;
        }

        ls_entry_t *new_entry = alloc(arena, ls_entry_t);
        new_entry->type = entry->type;
        new_entry->path = entry->name;

        if (opt->list_extra) {
            new_entry->size = entry->file_size;
            new_entry->filetype = ls_get_file_info(arena, strv(entry->name));
        }
 
        if (
            opt->print_tree &&
            (opt->max_depth == 0 || opt->max_depth >= depth) &&
            entry->type == DIRTYPE_DIR
        ) {
            str_t fullpath = os_path_join(arena, path, strv(entry->name));
            new_entry->children = ls_add_dir(arena, strv(fullpath), depth + 1, opt); 
        }
       
        dlist_push(head, new_entry);
    }

    return head;
}

ls_entry_t *ls_order_entries(ls_entry_t *entries) {
    ls_entry_t *out = NULL;

    ls_entry_t *e = entries;
    while (e) {
        ls_entry_t *next = e->next;

        if (e->type == DIRTYPE_FILE) {
            dlist_pop(entries, e);
            dlist_push(out, e);
        }

        e = next;
    }

    e = entries;
    while (e) {
        ls_entry_t *next = e->next;
        dlist_pop(entries, e);
        dlist_push(out, e);

        ls_entry_t *children = ls_order_entries(e->children);
        e->children = children;

        e = next;
    }

    return out;
}

void ls_print_dir_piped(arena_t scratch, ls_entry_t *entries, strview_t parent) {
    for_each (e, entries) {
        if (e->type == DIRTYPE_DIR) {
            str_t fullpath = os_path_join(&scratch, parent, strv(e->path));
            print("%v/\n", fullpath);
            ls_print_dir_piped(scratch, e->children, strv(fullpath));
        }
        else {
            arena_t temp = scratch;
            str_t fullpath = os_path_join(&temp, parent, strv(e->path));
            print("%v\n", fullpath);
        }
    }
}

void ls_print_dir(arena_t scratch, ls_entry_t *entries, int indent, TOY_OPTION(ls) *opt) {
    for_each (e, entries) {
        print(" %*s", indent * 4, "");

        if (opt->list_extra) {
            if (e->size > 0) {
                const char *size_suf[] = {
                    " B",
                    "KB",
                    "MB",
                    "GB",
                    "TB"
                };

                double size = (double)e->size;
                int suf = 0;
                for (; suf < arrlen(size_suf) && size > 1024.0; ++suf) {
                    size /= 1024;
                }
                if (suf >= arrlen(size_suf)) suf = arrlen(size_suf) - 1;

                print(
                    TERM_ITALIC TERM_FG_DARK_GREY 
                        "%4.0f %s "
                    TERM_RESET, 
                    size, size_suf[suf]
                );
            }
            else {
                print("%*s ", 7, "");
            }
        }

        if (e->type == DIRTYPE_FILE) {
            strview_t ext;
            os_file_split_path(strv(e->path), NULL, NULL, &ext);
            strview_t icon = ext_to_ico(ext);

            println(
                TERM_FG_GREEN "%v  "
                TERM_RESET "%v "
                TERM_FG_DARK_GREY "%v"
                TERM_RESET,
                icon, e->path, e->filetype
            );
        }
        else {
            println(
                TERM_FG_BLUE "%v  %v", 
                icons[opt->style][ICON_FOLDER], e->path
            );

            ls_print_dir(scratch, e->children, indent + 1, opt);
        }
    }
    
    os_log_set_colour(LOG_COL_RESET);
}

void TOY(ls)(int argc, char **argv) {
    TOY_OPTION(ls) opt = {0};

    TOY_OPTION_PARSE(ls)(argc, argv, &opt);

    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));

    if (strv_is_empty(opt.dir)) {
        opt.dir = strv(".");
    }

    if (!opt.is_out_piped && opt.style) {
        icons_init(opt.style);
    }

    ls_entry_t *entries = ls_add_dir(&arena, opt.dir, 1, &opt);
    entries = ls_order_entries(entries);

    if (opt.is_out_piped) {
        ls_print_dir_piped(arena, entries, opt.dir);
    }
    else {
        ls_print_dir(arena, entries, 0, &opt);
    }
}
