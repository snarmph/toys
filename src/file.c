#include "colla/colla.h" 
#include "common.h"
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

bool file__print_name = false;

TOY_SHORT_DESC(file, "Determine file type.");

void file__glob(arena_t scratch, strview_t fname, void *udata) {
    COLLA_UNUSED(udata);

    strview_t ext;
    os_file_split_path(fname, NULL, NULL, &ext);

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
        return;
    }

    str_t type = str_from_str16(&scratch, str16_init(info.szTypeName, 0));
    if (file__print_name) print(TERM_FG_DARK_GREY "%v: " TERM_RESET, fname);
    println("%v", type);
}

void TOY(file)(int argc, char **argv) {
    strview_t files[1024] = {0};
    i64 file_count = 0;

    usage_helper(
        "file FILE...",
        "Determine file type.",
        USAGE_DEFAULT,
        USAGE_EXTRA_PARAMS(files, file_count),
        argc, argv
    );

    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));

    glob_t glob_desc = {
        .recursive = true,
        .cb = file__glob,
    };

    file__print_name = file_count > 1;

    for (i64 i = 0; i < file_count; ++i) {
        if (common_is_glob(files[i])) {
            file__print_name = true;
            glob_desc.exp = files[i];
            common_glob(arena, &glob_desc);
        }
        else {
            file__glob(arena, files[i], NULL);
        }
    }
}
