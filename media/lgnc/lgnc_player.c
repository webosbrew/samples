#include "lgnc_player.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lgnc_directaudio.h>
#include <lgnc_directvideo.h>
#include <lgnc_plugin.h>

/* LGNC places the video window in a fixed 1920x1080 coordinate space, whatever the panel
 * actually is. Passing real pixel sizes from a 4K display puts the picture in a corner. */
#define LGNC_WINDOW_WIDTH 1920
#define LGNC_WINDOW_HEIGHT 1080

struct lgnc_player {
    bool initialized;
    bool video_opened;
    bool audio_opened;
};

/* Letterboxes the picture inside the 1920x1080 space, preserving aspect ratio. */
static void fit_video(int width, int height) {
    int x = 0;
    int y = 0;
    int w = LGNC_WINDOW_WIDTH;
    int h = LGNC_WINDOW_WIDTH * height / width;

    if (h > LGNC_WINDOW_HEIGHT) {
        w = LGNC_WINDOW_HEIGHT * width / height;
        h = LGNC_WINDOW_HEIGHT;
        x = (LGNC_WINDOW_WIDTH - w) / 2;
    } else {
        y = (LGNC_WINDOW_HEIGHT - h) / 2;
    }

    fprintf(stderr, "[lgnc] display window %d,%d %dx%d\n", x, y, w, h);
    _LGNC_DIRECTVIDEO_SetDisplayWindow(x, y, w, h);
}

lgnc_player *lgnc_player_create(const char *app_id) {
    lgnc_player *player = calloc(1, sizeof(lgnc_player));
    if (player == NULL) {
        return NULL;
    }

    /* msgHandler and the input callbacks stay NULL: this sample does its input through
     * SDL, and LGNC is used only as a decoder. The struct still has to be passed. */
    LGNC_CALLBACKS_T callbacks = { .msgHandler = NULL };
    int rc = LGNC_PLUGIN_Initialize(&callbacks);
    if (rc != 0) {
        fprintf(stderr, "[lgnc] LGNC_PLUGIN_Initialize failed: %d\n", rc);
        free(player);
        return NULL;
    }
    /* Documented as having to come after Initialize, never before. */
    LGNC_PLUGIN_SetAppId(app_id);
    player->initialized = true;
    fprintf(stderr, "[lgnc] plugin initialized as %s\n", app_id);
    return player;
}

bool lgnc_player_open(lgnc_player *player, const lgnc_params *params) {
    LGNC_VDEC_DATA_INFO_T video = {
            .width = params->video_width,
            .height = params->video_height,
            .vdecFmt = LGNC_VDEC_FMT_H264,
            .trid_type = LGNC_VDEC_3D_TYPE_NONE,
    };
    int rc = LGNC_DIRECTVIDEO_Open(&video);
    if (rc != 0) {
        fprintf(stderr, "[lgnc] LGNC_DIRECTVIDEO_Open failed: %d\n", rc);
        return false;
    }
    player->video_opened = true;
    fprintf(stderr, "[lgnc] video opened: H264 %dx%d\n", video.width, video.height);
    fit_video(video.width, video.height);

    LGNC_ADEC_DATA_INFO_T audio = {
            .codec = LGNC_ADEC_FMT_AAC,
            .AChannel = LGNC_ADEC_CH_INDEX_MAIN,
            /* The enum is kHz-ish rather than a rate, hence the helper in the header. */
            .samplingFreq = LGNC_ADEC_SAMPLING_FREQ_OF(params->audio_sample_rate),
            .numberOfChannel = (unsigned int) params->audio_channels,
            .bitPerSample = 16,
    };
    if ((rc = LGNC_DIRECTAUDIO_Open(&audio)) != 0) {
        fprintf(stderr, "[lgnc] LGNC_DIRECTAUDIO_Open failed: %d\n", rc);
        return false;
    }
    player->audio_opened = true;
    fprintf(stderr, "[lgnc] audio opened: AAC %uch %dHz\n", audio.numberOfChannel,
            params->audio_sample_rate);
    return true;
}

lgnc_feed_result lgnc_player_feed(lgnc_player *player, const void *data, size_t size,
                                  lgnc_stream stream) {
    int rc;
    if (stream == LGNC_STREAM_VIDEO) {
        if (!player->video_opened) {
            return LGNC_FEED_ERROR;
        }
        rc = LGNC_DIRECTVIDEO_Play(data, (unsigned int) size);
    } else {
        if (!player->audio_opened) {
            return LGNC_FEED_ERROR;
        }
        rc = LGNC_DIRECTAUDIO_Play(data, (unsigned int) size);
    }

    if (rc == 0) {
        return LGNC_FEED_OK;
    }
    /* Back-pressure has its own status here, unlike the starfish pipeline where it is a
     * word inside a reply string. */
    if (rc == LGNC_BUFFER_FULL) {
        return LGNC_FEED_BUFFER_FULL;
    }
    fprintf(stderr, "[lgnc] %s Play returned %d\n",
            stream == LGNC_STREAM_VIDEO ? "video" : "audio", rc);
    return LGNC_FEED_ERROR;
}

void lgnc_player_close(lgnc_player *player) {
    if (player->video_opened) {
        LGNC_DIRECTVIDEO_Close();
        player->video_opened = false;
    }
    if (player->audio_opened) {
        LGNC_DIRECTAUDIO_Close();
        player->audio_opened = false;
    }
}

void lgnc_player_destroy(lgnc_player *player) {
    if (player == NULL) {
        return;
    }
    lgnc_player_close(player);
    if (player->initialized) {
        LGNC_PLUGIN_Finalize();
    }
    free(player);
}
