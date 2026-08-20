/*
 * Playing elementary streams through NDL Esplayer - the webOS 2.x to 3.4 media API.
 *
 * This is the other way to reach the TV's decoder. Where starfish-media-pipeline takes a
 * JSON payload and a pointer-as-string, Esplayer is an ordinary C API with structs, and it
 * owns the video plane itself - there is no ACB call and no exported window, just
 * NDL_EsplayerSetVideoDisplayWindow.
 *
 * It lives in libndl-directmedia2, which exists only on webOS 2.x through 3.4. From
 * webOS 3.5 the library is replaced by libNDL_directmedia with a completely different
 * API (NDL_Direct*), which is why that generation gets its own sample rather than a
 * build variant of this one.
 *
 * Neither the headers nor a link stub ship in the buildroot NDK; both come from
 * webosbrew/webos-userland - see cmake/WebOSUserland.cmake.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Mirrors NDL_ESP_STREAM_T without dragging the LG headers into every caller. */
typedef enum esplayer_stream {
    ESPLAYER_STREAM_AUDIO,
    ESPLAYER_STREAM_VIDEO,
} esplayer_stream;

typedef enum esplayer_feed_result {
    ESPLAYER_FEED_OK,
    /* The decoder's buffers are full. Retry the same access unit shortly. */
    ESPLAYER_FEED_BUFFER_FULL,
    ESPLAYER_FEED_ERROR,
} esplayer_feed_result;

typedef struct esplayer_params {
    const char *app_id;

    int video_width;
    int video_height;
    int video_fps_num;
    int video_fps_den;

    int audio_channels;
    int audio_sample_rate;
} esplayer_params;

typedef struct esplayer esplayer;

esplayer *esplayer_create(const char *app_id);
void esplayer_destroy(esplayer *player);

/* Load, then Play. The display window is set here too, since Esplayer owns the plane. */
bool esplayer_load(esplayer *player, const esplayer_params *params, int display_width,
                   int display_height);

esplayer_feed_result esplayer_feed(esplayer *player, const void *data, size_t size,
                                   int64_t pts_ns, esplayer_stream stream);

/* Feeds a zero-length buffer flagged END_OF_STREAM on both streams, so the decoder drains
 * instead of stopping on the last full buffer. */
void esplayer_push_eos(esplayer *player);

void esplayer_unload(esplayer *player);

/* Set once the decoder reported NDL_ESP_END_OF_STREAM. */
bool esplayer_ended(esplayer *player);
