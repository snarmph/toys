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

typedef struct {
    bool dry_run;
    bool recursive;
    bool force;
    bool interactive;
    bool ignore;
    strview_t rg[RM_MAX_ITEMS];
    i64 rg_count;
} rm_opt_t;

typedef struct {
    IFileOperation *pfo;
    int count;
    rm_opt_t *opt;
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

void rm_parse_opts(int argc, char **argv, rm_opt_t *opt) {
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

void remove_file_or_dir(arena_t scratch, strview_t path, void *udata) {
    str_t fullpath = os_file_fullpath(&scratch, path);
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
    rm_opt_t opt = {0};

    rm_parse_opts(argc, argv, &opt);
    
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

    glob_t glob_data = {
        .cb = remove_file_or_dir,
        .udata = &del_data,
        .recursive = opt.recursive,
        .add_hidden = true,
    };

    for (i64 i = 0; i < opt.rg_count; ++i) {
        if (common_is_glob(opt.rg[i])) {
            glob_data.exp = opt.rg[i];

            common_glob(
                arena, 
                &glob_data
            );
        }
        else {
            remove_file_or_dir(arena, opt.rg[i], &del_data);
        }
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


