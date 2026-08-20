#include "esplayer.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ndl-directmedia2/esplayer-api.h>

struct esplayer {
    NDL_EsplayerHandle handle;
    pthread_mutex_t lock;
    bool loaded;
    bool ended;
};

static const char *event_name(NDL_ESP_EVENT event) {
    switch (event) {
        case NDL_ESP_FIRST_FRAME_PRESENTED: return "FIRST_FRAME_PRESENTED";
        case NDL_ESP_LOW_THRESHOLD_CROSSED_VIDEO: return "LOW_THRESHOLD_VIDEO";
        case NDL_ESP_HIGH_THRESHOLD_CROSSED_VIDEO: return "HIGH_THRESHOLD_VIDEO";
        case NDL_ESP_STREAM_DRAINED_VIDEO: return "STREAM_DRAINED_VIDEO";
        case NDL_ESP_LOW_THRESHOLD_CROSSED_AUDIO: return "LOW_THRESHOLD_AUDIO";
        case NDL_ESP_HIGH_THRESHOLD_CROSSED_AUDIO: return "HIGH_THRESHOLD_AUDIO";
        case NDL_ESP_STREAM_DRAINED_AUDIO: return "STREAM_DRAINED_AUDIO";
        case NDL_ESP_END_OF_STREAM: return "END_OF_STREAM";
        case NDL_ESP_VIDEO_INFO: return "VIDEO_INFO";
        case NDL_ESP_RESOURCE_RELEASED_BY_POLICY: return "RESOURCE_RELEASED_BY_POLICY";
        case NDL_ESP_VIDEOCONFIG_DECODED: return "VIDEOCONFIG_DECODED";
        case NDL_ESP_AUDIOCONFIG_DECODED: return "AUDIOCONFIG_DECODED";
        case NDL_ESP_VIDEO_PORT_CHANGED: return "VIDEO_PORT_CHANGED";
        case NDL_ESP_AUDIO_PORT_CHANGED: return "AUDIO_PORT_CHANGED";
        default: return "?";
    }
}

/* Runs on the player's own thread. */
static void esplayer_callback(NDL_ESP_EVENT event, void *playerdata, void *userdata) {
    esplayer *player = userdata;

    if (event == NDL_ESP_VIDEO_INFO && playerdata != NULL) {
        const NDL_ESP_VIDEO_INFO_T *info = playerdata;
        fprintf(stderr, "[ndl] videoInfo %ux%u @ %u/%u\n", info->width, info->height,
                info->framerateNum, info->framerateDen);
        return;
    }

    fprintf(stderr, "[ndl] event %s\n", event_name(event));

    if (event == NDL_ESP_END_OF_STREAM) {
        pthread_mutex_lock(&player->lock);
        player->ended = true;
        pthread_mutex_unlock(&player->lock);
    }
}

esplayer *esplayer_create(const char *app_id) {
    esplayer *player = calloc(1, sizeof(esplayer));
    if (player == NULL) {
        return NULL;
    }
    pthread_mutex_init(&player->lock, NULL);

    player->handle = NDL_EsplayerCreate(app_id, esplayer_callback, player);
    if (player->handle == NULL) {
        fprintf(stderr, "[ndl] NDL_EsplayerCreate failed\n");
        pthread_mutex_destroy(&player->lock);
        free(player);
        return NULL;
    }
    return player;
}

void esplayer_destroy(esplayer *player) {
    if (player == NULL) {
        return;
    }
    if (player->handle != NULL) {
        NDL_EsplayerDestroy(player->handle);
    }
    pthread_mutex_destroy(&player->lock);
    free(player);
}

bool esplayer_load(esplayer *player, const esplayer_params *params, int display_width,
                   int display_height) {
    char connection_id[32] = "";
    if (NDL_EsplayerGetConnectionId(player->handle, connection_id, sizeof(connection_id)) == 0) {
        fprintf(stderr, "[ndl] connectionId %s\n", connection_id);
    }

    /* Same rule as every other webOS media API: nothing loads or plays unless the app has
     * declared itself foreground. */
    NDL_EsplayerSetAppForegroundState(player->handle, NDL_ESP_APP_STATE_FOREGROUND);

    NDL_ESP_META_DATA meta = {
            .video_codec = NDL_ESP_VIDEO_CODEC_H264,
            .audio_codec = NDL_ESP_AUDIO_CODEC_AAC,
            .width = (uint32_t) params->video_width,
            .height = (uint32_t) params->video_height,
            .framerate = (uint32_t) (params->video_fps_num / params->video_fps_den),
            .channels = (uint32_t) params->audio_channels,
            .samplerate = (uint32_t) params->audio_sample_rate,
            .bitspersample = 16,
    };

    fprintf(stderr, "[ndl] NDL_EsplayerLoad(H264 %dx%d @%u, AAC %uch %uHz)\n", params->video_width,
            params->video_height, meta.framerate, meta.channels, meta.samplerate);

    int rc = NDL_EsplayerLoad(player->handle, &meta);
    if (rc != 0) {
        fprintf(stderr, "[ndl] NDL_EsplayerLoad failed: %d\n", rc);
        return false;
    }

    pthread_mutex_lock(&player->lock);
    player->loaded = true;
    pthread_mutex_unlock(&player->lock);

    /* Esplayer owns the video plane, so the window is set right here - no ACB, no exported
     * window. isFullScreen lets the TV scale the picture to the panel for us. */
    NDL_EsplayerSetVideoDisplayWindow(player->handle, 0, 0, display_width, display_height, 1);

    if ((rc = NDL_EsplayerPlay(player->handle)) != 0) {
        fprintf(stderr, "[ndl] NDL_EsplayerPlay failed: %d\n", rc);
        return false;
    }
    return true;
}

esplayer_feed_result esplayer_feed(esplayer *player, const void *data, size_t size,
                                   int64_t pts_ns, esplayer_stream stream) {
    NDL_ESP_STREAM_BUFFER buffer = {
            .data = (uint8_t *) data,
            .data_len = (uint32_t) size,
            .offset = 0,
            .stream_type = stream == ESPLAYER_STREAM_VIDEO ? NDL_ESP_VIDEO_ES : NDL_ESP_AUDIO_ES,
            /* This build of the library exports no NDL_EsplayerLoadEx, so the timestamp
             * unit is the default NDL_ESP_PTS_TICKS - 90 kHz MPEG ticks, not nanoseconds
             * and not microseconds. */
            .timestamp = pts_ns * 9 / 100000,
            .flags = 0,
    };

    int rc = NDL_EsplayerFeedData(player->handle, &buffer);
    if (rc >= 0) {
        return ESPLAYER_FEED_OK;
    }
    if (rc == NDL_ESP_RESULT_FEED_FULL) {
        return ESPLAYER_FEED_BUFFER_FULL;
    }
    fprintf(stderr, "[ndl] NDL_EsplayerFeedData returned %d\n", rc);
    return ESPLAYER_FEED_ERROR;
}

void esplayer_push_eos(esplayer *player) {
    pthread_mutex_lock(&player->lock);
    bool loaded = player->loaded;
    pthread_mutex_unlock(&player->lock);
    if (!loaded) {
        return;
    }

    /* There is no pushEOS() here as there is on the starfish pipeline. End of stream is
     * signalled in-band, as a zero-length buffer carrying the EOS flag, once per stream.
     * The pointer is still a valid address rather than NULL - the library dereferences it
     * on the way to OMX even when the length is zero. */
    static const uint8_t eos_marker = 0;
    static const NDL_ESP_STREAM_T streams[] = { NDL_ESP_VIDEO_ES, NDL_ESP_AUDIO_ES };
    for (size_t i = 0; i < sizeof(streams) / sizeof(streams[0]); i++) {
        NDL_ESP_STREAM_BUFFER eos = {
                .data = (uint8_t *) &eos_marker,
                .data_len = 0,
                .offset = 0,
                .stream_type = streams[i],
                .timestamp = 0,
                .flags = NDL_ESP_FLAG_END_OF_STREAM,
        };
        int rc = NDL_EsplayerFeedData(player->handle, &eos);
        fprintf(stderr, "[ndl] EOS on %s stream -> %d\n",
                streams[i] == NDL_ESP_VIDEO_ES ? "video" : "audio", rc);
    }
}

void esplayer_unload(esplayer *player) {
    pthread_mutex_lock(&player->lock);
    bool loaded = player->loaded;
    player->loaded = false;
    pthread_mutex_unlock(&player->lock);

    if (loaded) {
        NDL_EsplayerUnload(player->handle);
    }
}

bool esplayer_ended(esplayer *player) {
    pthread_mutex_lock(&player->lock);
    bool ended = player->ended;
    pthread_mutex_unlock(&player->lock);
    return ended;
}
