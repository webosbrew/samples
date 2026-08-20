#define _GNU_SOURCE

#include "m3_vdec_fix.h"

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/*
 * Thumb encoding of the guard to remove:
 *
 *     cmp  codecType, #0x0b   ; H264
 *     it   ne
 *     cmp  codecType, #0x10   ; HEVC
 *
 * The first two instructions become nops, which drops the codec check.
 */
static const unsigned char kGuard[] = {
        0x0b, 0x2a,
        0x18, 0xbf,
        0x10, 0x2a,
};
/* MS_VDEC_Init is roughly 1600 bytes, so the search does not need to run past that. */
#define M3_SEARCH_BYTES 1600

int webos_m3_vdec_fix(void) {
    /* libkadaptor is already loaded by the media stack; this resolves through it. */
    void *fn = dlsym(RTLD_DEFAULT, "MS_VDEC_Init");
    if (fn == NULL) {
        /* Not an MStar set, or the decoder is not loaded - nothing to do. */
        return 0;
    }

    long page_size = sysconf(_SC_PAGESIZE);
    void *page_start = (void *) ((size_t) fn & ~((size_t) page_size - 1));
    if (mprotect(page_start, (size_t) page_size, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        fprintf(stderr, "[m3] cannot make MS_VDEC_Init writable\n");
        return -1;
    }

    int patched = 0;
    size_t offset = 0;
    while (offset < M3_SEARCH_BYTES) {
        unsigned char *found = memmem((unsigned char *) fn + offset, M3_SEARCH_BYTES - offset,
                                      kGuard, sizeof(kGuard));
        if (found == NULL) {
            break;
        }
        found[0] = 0x00;
        found[1] = 0xbf;/* nop */
        found[2] = 0x00;/* clears the following 'it ne' */
        patched++;
        offset = (size_t) (found - (unsigned char *) fn) + sizeof(kGuard);
    }

    mprotect(page_start, (size_t) page_size, PROT_READ | PROT_EXEC);
    fprintf(stderr, "[m3] MS_VDEC_Init codec guard: %d site(s) patched\n", patched);
    return 0;
}
