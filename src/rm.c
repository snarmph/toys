#include "colla/colla.h" 
#include "common.h"
#include "toys.h"

#include <windows.h>
#include <shellapi.h>
#include <shobjidl.h>  
#include <conio.h>

#pragma comment(lib, "Shell32")
#pragma comment(lib, "Ole32")

TOY_SHORT_DESC(rm, "Move files or directories to trash bin.");

#define RM_MAX_ITEMS 1024

TOY_OPTION_DEFINE(rm) {
    bool dry_run;
    bool recursive;
    bool force;
    bool interactive;
    bool ignore;
    strview_t rg[RM_MAX_ITEMS];
    i64 rg_count;
};

typedef struct {
    IFileOperation *pfo;
    int count;
    TOY_OPTION(rm) *opt;
} rm_delete_data_t;

typedef struct rm_sink_t rm_sink_t;
struct rm_sink_t {
    IFileOperationProgressSinkVtbl *lpVtbl;
    LONG ref_count;
};

HRESULT STDMETHODCALLTYPE rm_query_interface(IFileOperationProgressSink *self, REFIID riid, void **ptr);
ULONG STDMETHODCALLTYPE rm_addref(IFileOperationProgressSink *self);
ULONG STDMETHODCALLTYPE rm_release(IFileOperationProgressSink *self);
HRESULT STDMETHODCALLTYPE rm_post_delete_item(
    IFileOperationProgressSink *self,
    DWORD flags,
    IShellItem *psi,
    HRESULT hr,
    IShellItem *psi_new
);

IFileOperationProgressSinkVtbl rm_sink_vtable = {
    .QueryInterface = rm_query_interface,
    .AddRef = rm_addref,
    .Release = rm_release,
    .PostDeleteItem = rm_post_delete_item,
};
rm_sink_t gsink = { .ref_count = 1, .lpVtbl = &rm_sink_vtable };

#define check(hr, fn) if (FAILED(hr)) fatal("call to " #fn " failed: %u", hr)

void TOY_OPTION_PARSE(rm)(int argc, char **argv, TOY_OPTION(rm) *opt) {
    usage_helper(
        "rm FILE/FOLDER...",
        "Move files or directories to trash bin",
        USAGE_DEFAULT,
        USAGE_EXTRA_PARAMS(opt->rg, opt->rg_count),
        argc, argv,
        {
            'f', "force",
            "Delete file permanently.",
            USAGE_BOOL(opt->force),
        },
        {
            'i', "interactive",
            "Prompt before any removal",
            USAGE_BOOL(opt->interactive),
        },
        {
            'r', "recursive",
            "Check for file existance inside folders.",
            USAGE_BOOL(opt->recursive),
        },
        {
            'd', "dry",
            "Don't actually delete, only print the names.",
            USAGE_BOOL(opt->dry_run),
        },
        {
            'p', "ignore",
            "Ignore nonexistent files and arguments.",
            USAGE_BOOL(opt->ignore),
        }
    );
}

void remove_file_or_dir(arena_t scratch, str_t path, void *udata) {
    str_t fullpath = os_file_fullpath(&scratch, strv(path));
    // remove \\?\ from beginning of fullpath
    strview_t final_path = str_sub(fullpath, 4, STR_END);
    str16_t winpath = strv_to_str16(&scratch, final_path);

    HRESULT hr;

    rm_delete_data_t *del_data = udata;

    IShellItem *psi = NULL;
    hr = SHCreateItemFromParsingName(winpath.buf, NULL, &IID_IShellItem, (void**)&psi);
    check(hr, SHCreateItemFromParsingName);

    bool should_delete = true;

    if (del_data->opt->interactive) {
        str_t prompt = str_fmt(&scratch, "do you want to delete %v", path);
        should_delete = common_prompt(strv(prompt));
    }

    if (should_delete) {
        hr = IFileOperation_DeleteItem(del_data->pfo, psi, NULL);
        check(hr, IFileOperation_DeleteItem);
        println(TERM_FG_RED "deleting: " TERM_RESET "%v", path);
        del_data->count++;
    }

    IShellItem_Release(psi);
}

void TOY(rm)(int argc, char **argv) {
    TOY_OPTION(rm) opt = {0};

    TOY_OPTION_PARSE(rm)(argc, argv, &opt);
    
    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));

    HRESULT hr = CoInitializeEx(
        NULL, 
        COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE
    );
    check(hr, CoInitializeEx);

    IFileOperation *pfo = NULL;
    hr = CoCreateInstance(
        &CLSID_FileOperation, 
        NULL, 
        CLSCTX_ALL,
        &IID_IFileOperation, 
        (void**)&pfo
    );
    check(hr, CoCreateInstance);

    hr = IFileOperation_SetOperationFlags(
        pfo,
        FOF_ALLOWUNDO | FOF_NO_UI
    );
    check(hr, IFileOperation_SetOperationFlags);

    rm_delete_data_t del_data = { .pfo = pfo, .opt = &opt };

    str_t rg[RM_MAX_ITEMS] = {0};
    for (i64 i = 0; i < opt.rg_count; ++i) {
        rg[i] = str(&arena, opt.rg[i]);
        for (usize k = 0; k < rg[i].len; ++k) {
            if (rg[i].buf[k] == '\\' && !(k + 1 >= rg[i].len || rg[i].buf[k+1] == '*')) {
                rg[i].buf[k] = '/';
            }
        }

        strview_t rgv = strv(rg[i]);
        if (!strv_contains(rgv, '*')) {
            if (!os_file_or_dir_exists(rgv)) {
                if (opt.ignore) {
                    println(TERM_FG_RED "deleting: " TERM_RESET "%v", rgv);
                    continue;
                }
                err("file or folder \"%v\" don't exist", rgv);
                os_abort(1);
            }
            remove_file_or_dir(arena, rg[i], &del_data);
            continue;
        }

        strview_t dir;
        os_file_split_path(rgv, &dir, NULL, NULL);

        if (dir.len > 0) {
            // include slash
            dir.len += 1;
        }

        fd_search(arena, &(fd_desc_t){
            .cb = remove_file_or_dir,
            .udata = &del_data,
            .path = dir,
            .rg = rgv,
            .recursive = opt.recursive,
            .add_hidden = true,
        });
    }

    if (!opt.dry_run && del_data.count > 0) {
        hr = IFileOperation_PerformOperations(pfo);
        check(hr, IFileOperation_PerformOperations);

        BOOL any_aborted = FALSE;
        hr = IFileOperation_GetAnyOperationsAborted(pfo, &any_aborted);
        check(hr, IFileOperation_GetAnyOperationsAborted);
        if (any_aborted) {
            println(TERM_FG_RED "error:" TERM_RESET " rm failed");
        }
    }

    if (del_data.count == 0 && !(opt.interactive || opt.ignore)) {
        warn("could not find any files or folder");
    }

    IFileOperation_Release(pfo);
}

HRESULT STDMETHODCALLTYPE rm_query_interface(IFileOperationProgressSink *self, REFIID riid, void **ptr) {
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IFileOperationProgressSink)) {
        rm_addref(self);
        *ptr = self;
        return S_OK;
    }
    *ptr = NULL;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE rm_addref(IFileOperationProgressSink *self) {
    rm_sink_t *sink = (rm_sink_t*)self;
    return InterlockedIncrement(&sink->ref_count);
}

ULONG STDMETHODCALLTYPE rm_release(IFileOperationProgressSink *self) {
    rm_sink_t *sink = (rm_sink_t*)self;
    LONG count = InterlockedDecrement(&sink->ref_count);
    return count;
}

HRESULT STDMETHODCALLTYPE rm_post_delete_item(
    IFileOperationProgressSink *self,
    DWORD flags,
    IShellItem *psi,
    HRESULT hr,
    IShellItem *psi_new
) {
    COLLA_UNUSED(psi); COLLA_UNUSED(flags); COLLA_UNUSED(self); COLLA_UNUSED(psi_new);
    if (FAILED(hr)) {
        if (HRESULT_CODE(hr) == ERROR_SHARING_VIOLATION) {
            fatal("File was locked!");
        }
    }
    return S_OK;
}


