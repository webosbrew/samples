/*
 * Walks an elementary stream and prints what es_file made of it.
 *
 * This is the only part of a media sample that can be checked without a TV, so it is worth
 * having: the access-unit count and duration it reports should match
 *
 *   ffprobe -v error -count_frames -select_streams v:0 \
 *           -show_entries stream=nb_read_frames,duration -of default=nw=1 sample.h264
 *
 * If the counts disagree, the access-unit grouping is wrong and the pipeline will get
 * malformed buffers - a failure that is very hard to diagnose from the TV side.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "es_file.h"

static void usage(const char *argv0) {
    fprintf(stderr,
            "usage: %s h264 <file.h264> [fps]\n"
            "       %s adts <file.aac>\n",
            argv0, argv0);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        usage(argv[0]);
        return 2;
    }

    es_file *file;
    bool is_audio = strcmp(argv[1], "adts") == 0;
    if (is_audio) {
        file = es_file_open_adts(argv[2]);
    } else if (strcmp(argv[1], "h264") == 0) {
        int fps = argc > 3 ? atoi(argv[3]) : 30;
        file = es_file_open_h264(argv[2], fps, 1);
    } else {
        usage(argv[0]);
        return 2;
    }
    if (file == NULL) {
        return 1;
    }

    if (is_audio) {
        printf("adts: %d Hz, %d channel(s), audioObjectType %d\n", es_file_sample_rate(file),
               es_file_channels(file), es_file_aac_object_type(file));
    }

    long count = 0;
    size_t total = 0;
    int64_t last_pts = 0;
    es_sample sample;
    while (es_file_next(file, &sample)) {
        if (count < 8) {
            printf("  [%3ld] %8zu bytes  pts %10.4f s\n", count, sample.size,
                   (double) sample.pts_ns / 1e9);
        }
        last_pts = sample.pts_ns;
        total += sample.size;
        count++;
    }

    printf("%s: %ld units, %zu bytes, last pts %.4f s\n", argv[2], count, total,
           (double) last_pts / 1e9);

    es_file_close(file);
    return 0;
}
