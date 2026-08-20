/*
 * Playing elementary streams through NDL DirectMedia - the webOS 3.5+ media API.
 *
 * This replaced libndl-directmedia2 (see ../esplayer) and kept nothing but the name. Where
 * Esplayer hands out a handle, DirectMedia is global state: one video decoder and one
 * audio decoder per process, opened and fed through free functions.
 *
 * It exists in two incompatible versions, selected by NDL_DIRECTMEDIA_API_VERSION, and the
 * SDK header switches between two completely different declaration sets on that macro:
 *
 *   v1  webOS 3.5 - 4.x   NDL_DirectVideoOpen / Play / Close, one call per stream.
 *                         Play() takes no timestamp. Audio can be PCM, AAC or AC3.
 *   v2  webOS 5+          NDL_DirectMediaLoad takes both streams in one struct and a
 *                         callback. Play() takes a PTS. Audio can be PCM, MP3 or Opus -
 *                         there is no AAC at all.
 *
 * That audio difference is why the two builds of this sample do not play the same files:
 * v1 gets sample.aac, v2 gets sample.pcm. Everything else behind this header is the same
 * shape, so main.c does not care which one it is linked against.
 *
 * Both versions live in one library, libNDL_directmedia, which the NDK does ship.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum directmedia_stream {
    DIRECTMEDIA_STREAM_AUDIO,
    DIRECTMEDIA_STREAM_VIDEO,
} directmedia_stream;

typedef enum directmedia_feed_result {
    DIRECTMEDIA_FEED_OK,
    /* The decoder's buffer is full. Retry the same access unit shortly. */
    DIRECTMEDIA_FEED_BUFFER_FULL,
    DIRECTMEDIA_FEED_ERROR,
} directmedia_feed_result;

typedef enum directmedia_audio_codec {
    DIRECTMEDIA_AUDIO_PCM,
    DIRECTMEDIA_AUDIO_AAC,
    DIRECTMEDIA_AUDIO_AC3,
} directmedia_audio_codec;

typedef struct directmedia_params {
    int video_width;
    int video_height;

    /* Audio can be left out entirely. Opening one decoder at a time is the first thing
     * worth trying when a platform crashes inside its own HAL: it says whether the
     * problem is the stream, the API usage, or that path specifically. */
    bool has_audio;
    /* v1 accepts all three; v2 has only PCM (no AAC, no AC-3) and ignores this. Worth
     * being able to switch: some MStar sets crash inside the audio HAL on one codec and
     * are perfectly happy on another. */
    directmedia_audio_codec audio_codec;
    int audio_channels;
    int audio_sample_rate;
} directmedia_params;

typedef struct directmedia directmedia;

/* NDL_DirectMediaInit + foreground. */
directmedia *directmedia_create(const char *app_id);
void directmedia_destroy(directmedia *player);

/* Opens both decoders and places the video area. */
bool directmedia_open(directmedia *player, const directmedia_params *params,
                      int display_width, int display_height);

directmedia_feed_result directmedia_feed(directmedia *player, const void *data, size_t size,
                                         int64_t pts_ns, directmedia_stream stream);

void directmedia_close(directmedia *player);

/* True once the pipeline reported end of stream. v1 has no such notification and always
 * answers false. */
bool directmedia_ended(directmedia *player);

/* Whether this API version can only do PCM. v2 can; v1 accepts AAC too, so it merely
 * prefers AAC and lets the caller override. */
bool directmedia_requires_pcm_audio(void);

/* Whether PCM is the sensible default even where other codecs are accepted. See the long
 * note in directmedia_v1.c: compressed audio crashes the audio HAL on some MStar sets. */
bool directmedia_prefers_pcm_audio(void);
