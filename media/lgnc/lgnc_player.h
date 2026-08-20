/*
 * Playing elementary streams through LGNC - the LG NetCast Open API.
 *
 * This is the oldest of the three routes to the decoder, and the only one that reaches
 * webOS 1. It works unchanged from webOS 1 to webOS 4: the function set is identical on
 * every one of those firmwares, so unlike the starfish samples there are no build variants
 * here at all. One binary, five generations.
 *
 * webOS 5 still exports every one of these symbols, so an ipk built for it links and
 * verifies clean - but LGNC playback is broken there in practice. Symbols resolving is not
 * the same as the API working, and no amount of checking the symbol tables would have
 * caught this one. The library is then gone outright in webOS 6.
 *
 * It is also the plainest API of the three. Open a decoder with a struct, push bytes at
 * it, close it. There is no pipeline, no JSON, no media id, no video plane to arrange
 * separately - and no timestamps either, which is the one thing that really shapes how a
 * player has to use it. See lgnc_player_feed().
 *
 * The library disappears in webOS 6.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef enum lgnc_stream {
    LGNC_STREAM_AUDIO,
    LGNC_STREAM_VIDEO,
} lgnc_stream;

typedef enum lgnc_feed_result {
    LGNC_FEED_OK,
    /* The decoder's buffer is full. Retry the same access unit shortly. */
    LGNC_FEED_BUFFER_FULL,
    LGNC_FEED_ERROR,
} lgnc_feed_result;

typedef struct lgnc_params {
    int video_width;
    int video_height;
    int audio_channels;
    int audio_sample_rate;
} lgnc_params;

typedef struct lgnc_player lgnc_player;

/* LGNC_PLUGIN_Initialize + SetAppId. Must happen before either decoder is opened. */
lgnc_player *lgnc_player_create(const char *app_id);
void lgnc_player_destroy(lgnc_player *player);

/* Opens both decoders and places the video window. */
bool lgnc_player_open(lgnc_player *player, const lgnc_params *params);

/*
 * Hands one access unit to a decoder.
 *
 * Note there is no timestamp parameter - LGNC_DIRECTVIDEO_Play and LGNC_DIRECTAUDIO_Play
 * take only a pointer and a length. The decoders play what they are given at their own
 * rate, so keeping audio and video together is entirely the caller's problem: feed in
 * presentation order, at the right wall-clock moment, and do not run ahead.
 */
lgnc_feed_result lgnc_player_feed(lgnc_player *player, const void *data, size_t size,
                                  lgnc_stream stream);

void lgnc_player_close(lgnc_player *player);
