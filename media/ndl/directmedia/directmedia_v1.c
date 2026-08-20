/*
 * NDL DirectMedia API version 1 - webOS 3.5 to 4.x.
 *
 * Each stream is opened, fed and closed on its own. There is no combined load, no
 * callback, and - the part that shapes everything - no timestamp on Play(). The decoders
 * consume what they are given at their own rate, so the feed loop's pacing is the only
 * thing keeping audio and video together.
 */
#define NDL_DIRECTMEDIA_API_VERSION 1

#include "directmedia.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <NDL_directmedia.h>

struct directmedia {
    bool initialized;
    bool video_opened;
    bool audio_opened;
};

static void resource_released(const char *type) {
    /* The TV can take the decoder away - another app, or the system needing it. A real
     * player would tear down and re-acquire; the sample just says so. */
    fprintf(stderr, "[ndl] resource released by policy: %s\n", type ? type : "?");
}

bool directmedia_wants_pcm_audio(void) { return false; }

directmedia *directmedia_create(const char *app_id) {
    directmedia *player = calloc(1, sizeof(directmedia));
    if (player == NULL) {
        return NULL;
    }
    if (NDL_DirectMediaInit(app_id, resource_released) != 0) {
        fprintf(stderr, "[ndl] NDL_DirectMediaInit failed: %s\n", NDL_DirectMediaGetError());
        free(player);
        return NULL;
    }
    player->initialized = true;
    NDL_DirectMediaSetAppState(NDL_DIRECTMEDIA_APP_STATE_FOREGROUND);
    fprintf(stderr, "[ndl] DirectMedia v1 initialized as %s\n", app_id);
    return player;
}

bool directmedia_open(directmedia *player, const directmedia_params *params,
                      int display_width, int display_height) {
    NDL_DIRECTVIDEO_DATA_INFO_T video = {
            .width = params->video_width,
            .height = params->video_height,
            .source = NDL_DIRECTVIDEO_SRC_TYPE_H264,
    };
    if (NDL_DirectVideoOpen(&video) != 0) {
        fprintf(stderr, "[ndl] NDL_DirectVideoOpen failed: %s\n", NDL_DirectMediaGetError());
        return false;
    }
    player->video_opened = true;
    fprintf(stderr, "[ndl] video opened: H264 %dx%d\n", video.width, video.height);

    /* Unlike LGNC this one really is in panel pixels. */
    NDL_DirectVideoSetArea(0, 0, display_width, display_height);
    fprintf(stderr, "[ndl] video area 0,0 %dx%d\n", display_width, display_height);

    NDL_DIRECTAUDIO_DATA_INFO_T audio = {
            .number_of_channel = (unsigned int) params->audio_channels,
            .bit_per_sample = 16,
            .no_delay_mode = NDL_DIRECTAUDIO_NODELAY_MODE_DISABLED,
            .channel = NDL_DIRECTAUDIO_CH_MAIN,
            /* v1 does take AAC, so the same sample.aac the other samples use works here. */
            .source = NDL_DIRECTAUDIO_SRC_TYPE_AAC,
            .frequency = NDL_DIRECTAUDIO_SAMPLING_FREQ_48_KHZ,
    };
    if (NDL_DirectAudioOpen(&audio) != 0) {
        fprintf(stderr, "[ndl] NDL_DirectAudioOpen failed: %s\n", NDL_DirectMediaGetError());
        return false;
    }
    player->audio_opened = true;
    fprintf(stderr, "[ndl] audio opened: AAC %uch %dHz\n", audio.number_of_channel,
            params->audio_sample_rate);
    return true;
}

directmedia_feed_result directmedia_feed(directmedia *player, const void *data, size_t size,
                                         int64_t pts_ns, directmedia_stream stream) {
    /* v1's Play() has no pts parameter at all. */
    (void) pts_ns;

    int rc;
    if (stream == DIRECTMEDIA_STREAM_VIDEO) {
        if (!player->video_opened) {
            return DIRECTMEDIA_FEED_ERROR;
        }
        rc = NDL_DirectVideoPlay((void *) data, (unsigned int) size);
    } else {
        if (!player->audio_opened) {
            return DIRECTMEDIA_FEED_ERROR;
        }
        rc = NDL_DirectAudioPlay((void *) data, (unsigned int) size);
    }
    if (rc == 0) {
        return DIRECTMEDIA_FEED_OK;
    }
    /* v1 reports only success or failure, with no distinct back-pressure status. Treat a
     * failure as "too fast" rather than fatal: the caller retries, and a genuinely broken
     * decoder will simply keep saying no until the sample gives up on its own. */
    return DIRECTMEDIA_FEED_BUFFER_FULL;
}

bool directmedia_ended(directmedia *player) {
    (void) player;
    /* No end-of-stream notification exists in v1. */
    return false;
}

void directmedia_close(directmedia *player) {
    if (player->video_opened) {
        NDL_DirectVideoClose();
        player->video_opened = false;
    }
    if (player->audio_opened) {
        NDL_DirectAudioClose();
        player->audio_opened = false;
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
    free(player);
}
