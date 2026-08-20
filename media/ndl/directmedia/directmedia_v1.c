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

#include "m3_vdec_fix.h"

struct directmedia {
    bool initialized;
    bool video_opened;
    bool audio_opened;
};

/*
 * Called back once the decoder is done with a buffer, carrying whatever userdata was
 * passed to NDL_DirectVideoPlayWithCallback. A real player uses it to time frames; the
 * sample only needs it to exist.
 */
static void video_play_callback(unsigned long long userdata) {
    (void) userdata;
}

static void resource_released(const char *type) {
    /* The TV can take the decoder away - another app, or the system needing it. A real
     * player would tear down and re-acquire; the sample just says so. */
    fprintf(stderr, "[ndl] resource released by policy: %s\n", type ? type : "?");
}

bool directmedia_requires_pcm_audio(void) { return false; }

/*
 * v1 accepts PCM, AAC and AC-3 - but PCM is the default here, and deliberately.
 *
 * On an MStar "m3" set (49LK5900, webOS 4.4) both compressed formats crash the platform's
 * own audio HAL on the very first write: HAL_AUDIO_DIRECT_Write dereferences a null
 * pES3BufInfo, the elementary-stream buffer that never got allocated. The reason is
 * visible in the jail: /dev/adsp, /dev/audio and /dev/dsp are not exposed to a jailed app,
 * so the audio DSP that decodes compressed formats cannot be reached. PCM does not need
 * it and plays fine.
 *
 * No jail configuration fixes this, and it is worth being precise about that because the
 * instinct is to go looking for a more privileged jail type. On a 49LK5900 all eleven
 * templates in /etc - native, native_builtin, native_devmode, native_game, native_mvpd,
 * triton and the rest - expose zero audio DSP nodes. They only conditionally mount
 * /dev/snd. The dev-mode jail an ares-install'd app gets is in fact the most permissive of
 * them: it is the only one carrying /dev/ion and /dev/cmapool, and forcing `jailer -t
 * native` makes things worse by dropping those, so the *video* decoder then fails too.
 *
 * Which means a store-shipped app would hit the same wall. Compressed audio decoding on
 * this SoC belongs to the media pipeline process, not to any app - which is why the
 * starfish samples play AAC here quite happily while this one cannot. It is the same class
 * of limitation ss4s handles by feeding PCM here and disabling audio outright on m16p.
 *
 * Pass {"audio":"aac"} or {"audio":"ac3"} to try them anyway - on a non-MStar set they may
 * well work, and finding out is the point of a sample.
 */
bool directmedia_prefers_pcm_audio(void) { return true; }

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
    /* Must happen before the video decoder is opened. */
    webos_m3_vdec_fix();
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

    /* Required before feeding. Plain NDL_DirectVideoPlay is declared in the header but
     * crashes on webOS 4.4 when no callback has been registered - the library dispatches
     * to it unconditionally. Register one and use the WithCallback variant instead. */
    NDL_DirectVideoSetCallback(video_play_callback);

    /* Unlike LGNC this one really is in panel pixels. */
    NDL_DirectVideoSetArea(0, 0, display_width, display_height);
    fprintf(stderr, "[ndl] video area 0,0 %dx%d\n", display_width, display_height);

    if (!params->has_audio) {
        fprintf(stderr, "[ndl] audio disabled, video only\n");
        return true;
    }

    NDL_DIRECTAUDIO_DATA_INFO_T audio = {
            .number_of_channel = (unsigned int) params->audio_channels,
            .bit_per_sample = 16,
            .no_delay_mode = NDL_DIRECTAUDIO_NODELAY_MODE_DISABLED,
            .channel = NDL_DIRECTAUDIO_CH_MAIN,
            /* v1 takes either. AAC means the same sample.aac the other samples use; PCM
             * means raw S16LE, which is what a streaming client would feed after decoding
             * and what the platform's own players exercise most. */
            .source = params->audio_codec == DIRECTMEDIA_AUDIO_PCM
                              ? NDL_DIRECTAUDIO_SRC_TYPE_PCM
                      : params->audio_codec == DIRECTMEDIA_AUDIO_AC3
                              ? NDL_DIRECTAUDIO_SRC_TYPE_AC3
                              : NDL_DIRECTAUDIO_SRC_TYPE_AAC,
            .frequency = NDL_DIRECTAUDIO_SAMPLING_FREQ_OF(params->audio_sample_rate),
    };
    if (NDL_DirectAudioOpen(&audio) != 0) {
        fprintf(stderr, "[ndl] NDL_DirectAudioOpen failed: %s\n", NDL_DirectMediaGetError());
        return false;
    }
    player->audio_opened = true;
    static const char *const kNames[] = { "PCM S16LE", "AAC", "AC-3" };
    fprintf(stderr, "[ndl] audio opened: %s %uch %dHz\n", kNames[params->audio_codec],
            audio.number_of_channel, params->audio_sample_rate);
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
        /* The userdata is handed straight back to video_play_callback when the decoder
         * releases the buffer; nothing here needs it. */
        rc = NDL_DirectVideoPlayWithCallback(data, (unsigned int) size, 0);
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
