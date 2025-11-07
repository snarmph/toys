#include "colla/colla.h" 
#include "common.h"
#include "toys.h"

TOY_SHORT_DESC(cksum, "Compute and verify file checksums.");

#define CKSUM_MAX_FILES 1024

#if 0
Print or verify checksums.  By default use the 32 bit CRC algorithm.
With no FILE, or when FILE is -, read standard input.

-a, --algorithm=TYPE  select the digest type to use.
                      options are:
                       - sysv    
                       - bsd     
                       - crc     
                       - crc32b  
                       - md5     
                       - sha1    
                       - sha224  
                       - sha256  
                       - sha384  
                       - sha512  
                       - blake2b 
                       - sm3     
-b --base64           Emit base64-encoded digests, not hexadecimal
-r --raw              emit a raw binary digest, not hexadecimal
-z --zero             end each output line with NUL, not newline, and disable file name escaping
-q --quiet
#endif

typedef enum {
    CKSUM_OUT_HEX,
    CKSUM_OUT_B64,
    CKSUM_OUT_BIN,
} cksum_output_e;

typedef enum {
    CKSUM_ALGO_DEFAULT,
    CKSUM_ALGO_CRC32B,
    CKSUM_ALGO_MD5,
    CKSUM_ALGO_SHA1,
    CKSUM_ALGO_SHA224,
    CKSUM_ALGO_SHA256,
    CKSUM_ALGO_SHA384,
    CKSUM_ALGO_SHA512,
    CKSUM_ALGO__COUNT,
} cksum_algo_e;

strview_t cksum_algo_names[CKSUM_ALGO__COUNT] = {
    [CKSUM_ALGO_CRC32B]  = cstrv("crc32b"),
    [CKSUM_ALGO_MD5]     = cstrv("md5"),
    [CKSUM_ALGO_SHA1]    = cstrv("sha1"),
    [CKSUM_ALGO_SHA224]  = cstrv("sha224"),
    [CKSUM_ALGO_SHA256]  = cstrv("sha256"),
    [CKSUM_ALGO_SHA384]  = cstrv("sha384"),
    [CKSUM_ALGO_SHA512]  = cstrv("sha512"),
};

typedef struct {
    strview_t files[1024];
    i64 file_count;
    cksum_output_e output;
    cksum_algo_e algo;
    bool zero;
    bool quiet;
} cksum_opt_t;

void cksum_parse_opts(int argc, char **argv, cksum_opt_t *opt) {
    strview_t algo = STRV_EMPTY;

    bool base64 = false, raw = false;

    usage_helper(
        "cksum [option] [FILE]...", 
        "Print or verify checksums.  By default use the 32 bit CRC algorithm.\n"
        "With no FILE, or when FILE is -, read standard input.",
        USAGE_DEFAULT, 
        USAGE_EXTRA_PARAMS(opt->files, opt->file_count),
        argc, argv,
        {
            'a', "algorithm",
            "Select the {} of algorithm to use. "
            "Options are: (sysv, bsd, crc, crc32b, md5, sha1, sha224, sha256, sha384, sha512, blake2b, sm3)",
            "type",
            USAGE_VALUE(algo),
        },
        {
            'b', "base64",
            "Emit base64-encoded digests, not hexadecimal.",
            USAGE_BOOL(base64),
        },
        {
            'r', "raw",
            "Emit a raw binary digest, not hexadecimal.",
            USAGE_BOOL(raw),
        },
        {
            'z', "zero",
            "End each output line with NUL, not newline, and disable file name escaping.",
            USAGE_BOOL(opt->zero),
        },
        {
            'q', "quiet",
            "Don't output anything, status code shows success.",
            USAGE_BOOL(opt->quiet),
        },
    );

    if (raw && base64) {
        fatal("only one type of digest allowed");
    }
    if (base64) opt->output = CKSUM_OUT_B64;
    if (raw)    opt->output = CKSUM_OUT_BIN;

    if (algo.len) {
        for (int i = 0; i < arrlen(cksum_algo_names); ++i) {
            if (strv_equals(algo, cksum_algo_names[i])) {
                opt->algo = i;
            }
        }
        if (opt->algo == CKSUM_ALGO_DEFAULT) {
            fatal("unrecognized algorithm: %v", algo);
        }
    }
    else {
        opt->algo = CKSUM_ALGO_CRC32B;
    }
}

// void cksum_crc32b_init(u32 crc_table[256]) {
//     for (u32 i = 0; i < 256; ++i) {
//         u32 c = i << 24;
//         for (int k = 8; k > 0; --k) {
//             c = c & 0x80000000 ? 
//                 (c << 1) ^ 0x04c11db7 : 
//                 (c << 1);
//         }
//         crc_table[i] = c;
//     }
// }


void *cksum_crc32b_init(arena_t *arena, cksum_opt_t *opt) {
    u32 *crc_table = alloc(arena, u32, 256);
    for (u32 i = 0; i < 256; ++i) {
        u32 c = i << 24;
        for (int k = 8; k > 0; --k) {
            c = c & 0x80000000 ? 
                (c << 1) ^ 0x04c11db7 : 
                (c << 1);
        }
        crc_table[i] = c;
    }
    return crc_table;
}

u32 cksum_crc32b(arena_t scratch, buffer_t buf, void *udata, cksum_opt_t *opt) {
    u32 *crc_table = udata;
    u32 cksum = 0;
    for (usize i = 0; i < buf.len; ++i) {
        cksum = (cksum << 8) ^ crc_table[(cksum >> 24) ^ buf.data[i]];
    }
    // use size in calculation
    for (i64 i = (i64)buf.len, k = 0; i > 0; i >>= 8) {
        u8 byte = (u8)(i & 0xFF);
        cksum = (cksum << 8) ^ crc_table[(cksum >> 24) ^ byte];
    }
    // println("%u 0x%x", ~cksum, ~cksum);
    return ~cksum;
}


bool cksum_md5(cksum_opt_t *opt) {
    return true;
}

bool cksum_sha1(cksum_opt_t *opt) {
    return true;
}

bool cksum_sha224(cksum_opt_t *opt) {
    return true;
}

bool cksum_sha256(cksum_opt_t *opt) {
    return true;
}

bool cksum_sha384(cksum_opt_t *opt) {
    return true;
}

bool cksum_sha512(cksum_opt_t *opt) {
    return true;
}

void TOY(cksum)(int argc, char **argv) {
    cksum_opt_t opt = {0};
    cksum_parse_opts(argc, argv, &opt);
    
    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));

    void *udata = cksum_crc32b_init(&arena, &opt);
    for (i64 i = 0; i < opt.file_count; ++i) {
        arena_t scratch = arena;
        buffer_t buf = os_file_read_all(&scratch, opt.files[i]);
        u32 cksum = cksum_crc32b(scratch, buf, udata, &opt);
        if (opt.quiet) {
            continue;
        }
        switch (opt.output) {
            case CKSUM_OUT_HEX:
                print("%x", cksum);
                break;
            case CKSUM_OUT_B64:
                print("%v", base64_encode(&scratch, (buffer_t){ .data = (u8*)&cksum, .len = sizeof(cksum) }));
                break;
            case CKSUM_OUT_BIN:
                print("%b", cksum);
                break;
        } 

        print(" %_$$$zuB %v", buf.len, opt.files[i]);

        if (opt.zero) print("\0");
        else          print("\n");
    }
    // cksum_sysv(&opt);
}
