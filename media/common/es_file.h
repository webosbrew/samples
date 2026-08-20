/*
 * Reading elementary streams straight off disk.
 *
 * The media samples do not demux anything. They take two files produced by
 * assets/make-sample.sh - a raw Annex-B H.264 stream and a raw ADTS AAC stream - and walk
 * them one access unit / frame at a time. That is exactly the granularity the webOS
 * pipeline wants: one Feed() call per access unit.
 *
 * The file is mmapped, so a sample points directly into the mapping. That is not just an
 * optimisation: the pipeline's Feed() payload passes the buffer *address* as a string and
 * dereferences it in-process, so the bytes have to be somewhere stable anyway.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct es_file es_file;

typedef struct es_sample {
    const uint8_t *data;
    size_t size;
    /* Presentation timestamp in nanoseconds, counted from the start of the file.
     * Nanoseconds because that is what the Feed() payload's "pts" field expects. */
    int64_t pts_ns;
} es_sample;

/* Open an Annex-B H.264 stream. Timestamps are synthesised from the frame rate, because a
 * raw elementary stream carries no timing of its own. */
es_file *es_file_open_h264(const char *path, int fps_num, int fps_den);

/* Open an ADTS AAC stream. Timing comes from the sample rate in each frame header, so
 * nothing needs to be passed in. */
es_file *es_file_open_adts(const char *path);

/* Open raw interleaved signed 16-bit little-endian PCM, chopped into fixed-size chunks.
 *
 * Not every media API takes compressed audio: NDL DirectMedia v2 (webOS 5+) has no AAC in
 * its audio types at all, only PCM, MP3 and Opus. PCM is the one that needs no demuxing
 * and no decoder, which makes it the right fallback for a sample.
 *
 * `frames_per_chunk` is how many sample frames go into each es_sample - 1024 keeps the
 * cadence close to an AAC frame, so the feed loop behaves the same either way. */
es_file *es_file_open_pcm_s16le(const char *path, int sample_rate, int channels,
                                int frames_per_chunk);

void es_file_close(es_file *file);

/* Fills `out` with the next access unit / frame. Returns false at end of stream.
 * `out->data` stays valid until es_file_close(). */
bool es_file_next(es_file *file, es_sample *out);

/* Re-read from the beginning, restarting timestamps at zero. */
void es_file_rewind(es_file *file);

/* ADTS only - parsed from the first frame header, so valid right after open.
 * Both are zero for an H.264 stream. */
int es_file_sample_rate(const es_file *file);
int es_file_channels(const es_file *file);

/* ADTS only - MPEG-4 audio object type (2 == AAC-LC), which is what the pipeline's
 * "aacInfo.profile" field wants. Note this is the ADTS `profile` field plus one. */
int es_file_aac_object_type(const es_file *file);
