/*
 * NDL DirectMedia API version 2 - webOS 5 and newer.
 *
 * Both streams are described in one struct and handed over in a single
 * NDL_DirectMediaLoad, along with a callback that reports pipeline events. Play() gains a
 * timestamp, so unlike v1 the decoder can do its own audio/video synchronisation - the
 * feed loop's pacing is still what stops the buffers overrunning, but it is no longer the
 * only thing holding the two streams together.
 *
 * The audio types here are PCM, MP3 and Opus. There is no AAC, which is why this build
 * plays sample.pcm while the v1 build plays sample.aac. Opus would work too and would be
 * far smaller on disk, but it would need the sample to unpack Ogg pages to recover packet
 * boundaries - demuxing, which these samples deliberately do not do.
 */
#define NDL_DIRECTMEDIA_API_VERSION 2

#include "directmedia.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <NDL_directmedia.h>

struct directmedia {
    bool initialized;
    bool loaded;
    pthread_mutex_t lock;
    bool ended;
};

/* NDL_DirectMediaLoad's callback carries no user pointer, so the one player has to be
 * reachable from a global. The samples create exactly one, which is what makes that
 * acceptable here. */
static directmedia *g_only_player = NULL;

static void resource_released(const char *type) {
    fprintf(stderr, "[ndl] resource released by policy: %s\n", type ? type : "?");
}

static void media_load_callback(int type, long long num_value, const char *str_value) {
    fprintf(stderr, "[ndl] event type=%d num=%lld %s\n", type, num_value,
            str_value ? str_value : "");
    /* The event ids are the same PF_EVENT_T values the starfish pipeline uses - NDL v2 sits
     * on top of it - so 0x16 is load completed, 0x1a playing, 0x17 unload completed and
     * 0x1c end of stream. Only the first three were ever seen on webOS 10.3.1: with a
     * buffer-stream source there is no way to signal that the stream ended, so 0x1c does
     * not arrive and the sample finishes on its drain timeout instead. */
    if (type == 0x1c && g_only_player != NULL) {
        pthread_mutex_lock(&g_only_player->lock);
        g_only_player->ended = true;
        pthread_mutex_unlock(&g_only_player->lock);
    }
}

bool directmedia_requires_pcm_audio(void) { return true; }

bool directmedia_prefers_pcm_audio(void) { return true; }

directmedia *directmedia_create(const char *app_id) {
    directmedia *player = calloc(1, sizeof(directmedia));
    if (player == NULL) {
        return NULL;
    }
    pthread_mutex_init(&player->lock, NULL);

    if (NDL_DirectMediaInit(app_id, resource_released) != 0) {
        fprintf(stderr, "[ndl] NDL_DirectMediaInit failed: %s\n", NDL_DirectMediaGetError());
        pthread_mutex_destroy(&player->lock);
        free(player);
        return NULL;
    }
    player->initialized = true;
    g_only_player = player;
    NDL_DirectMediaSetAppState(NDL_DIRECTMEDIA_APP_STATE_FOREGROUND);
    fprintf(stderr, "[ndl] DirectMedia v2 initialized as %s\n", app_id);
    return player;
}

bool directmedia_open(directmedia *player, const directmedia_params *params,
                      int display_width, int display_height) {
    NDL_DIRECTMEDIA_DATA_INFO_T info = {
            .video = {
                    .width = params->video_width,
                    .height = params->video_height,
                    .type = NDL_VIDEO_TYPE_H264,
            },
            .audio = {
                    .pcm = {
                            .type = params->has_audio ? NDL_AUDIO_TYPE_PCM : 0,
                            .format = NDL_DIRECTMEDIA_AUDIO_PCM_FORMAT_S16LE,
                            .layout = "interleaved",
                            .channelMode = params->audio_channels == 1 ? "mono" : "stereo",
                            .sampleRate = NDL_DIRECTMEDIA_AUDIO_PCM_SAMPLE_RATE_OF(
                                    params->audio_sample_rate),
                    },
            },
    };

    fprintf(stderr, "[ndl] NDL_DirectMediaLoad(H264 %dx%d, PCM S16LE %dch %dHz)\n",
            params->video_width, params->video_height, params->audio_channels,
            params->audio_sample_rate);

    if (NDL_DirectMediaLoad(&info, media_load_callback) != 0) {
        fprintf(stderr, "[ndl] NDL_DirectMediaLoad failed: %s\n", NDL_DirectMediaGetError());
        return false;
    }
    player->loaded = true;

    NDL_DirectVideoSetArea(0, 0, display_width, display_height);
    fprintf(stderr, "[ndl] video area 0,0 %dx%d\n", display_width, display_height);
    return true;
}

directmedia_feed_result directmedia_feed(directmedia *player, const void *data, size_t size,
                                         int64_t pts_ns, directmedia_stream stream) {
    if (!player->loaded) {
        return DIRECTMEDIA_FEED_ERROR;
    }
    /* v2 takes a timestamp. It wants microseconds, not the nanoseconds the rest of the
     * samples carry around. */
    long long pts_us = pts_ns / 1000;

    int rc;
    if (stream == DIRECTMEDIA_STREAM_VIDEO) {
        rc = NDL_DirectVideoPlay((void *) data, (unsigned int) size, pts_us);
    } else {
        rc = NDL_DirectAudioPlay((void *) data, (unsigned int) size, pts_us);
    }
    if (rc == 0) {
        return DIRECTMEDIA_FEED_OK;
    }
    return DIRECTMEDIA_FEED_BUFFER_FULL;
}

bool directmedia_ended(directmedia *player) {
    pthread_mutex_lock(&player->lock);
    bool ended = player->ended;
    pthread_mutex_unlock(&player->lock);
    return ended;
}

void directmedia_close(directmedia *player) {
    if (player->loaded) {
        NDL_DirectMediaUnload();
        player->loaded = false;
    }
}

void directmedia_destroy(directmedia *player) {
    if (player == NULL) {
        return;
    }
    directmedia_close(player);
    if (player->initialized) {
        NDL_DirectMediaQuit();
    }
    if (g_only_player == player) {
        g_only_player = NULL;
    }
    pthread_mutex_destroy(&player->lock);
    free(player);
}
