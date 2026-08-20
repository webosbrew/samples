/*
 * Playing an H.264 + AAC pair through starfish-media-pipeline on webOS 2 - 4.x.
 *
 * The whole of it:
 *
 *   1. bring up SDL and a fullscreen, transparent window
 *   2. create an ACB instance and bind it to the pipeline's media id
 *   3. Load() the pipeline with a BUFFERSTREAM payload
 *   4. wait for LOADCOMPLETED, at which point Play() is called for us
 *   5. Feed() access units in presentation order, in real time
 *   6. pushEOS() and Unload()
 *
 * The decoded video never passes through this process. It goes to a hardware plane behind
 * the SDL surface, which is why step 1 clears to transparent and then leaves the screen
 * alone.
 *
 * Compared with the webOS 5 sample next door, only step 2 differs - and the ABI variant of
 * media/smp/common this links against. Diffing the two mains is a good way to see exactly
 * how little the generations differ once the video plane is factored out.
 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app_launch.h"
#include "es_file.h"
#include "acb_plane.h"
#include "pacer.h"
#include "sdl_shell.h"
#include "smp_player.h"

#define DEFAULT_FPS 30
#define DEFAULT_WIDTH 1280
#define DEFAULT_HEIGHT 720

/* How long to wait before offering a buffer the pipeline has just refused. */
#define BUFFER_FULL_BACKOFF_NS (5 * 1000 * 1000)
/* How long to wait for ENDOFSTREAM after the last buffer has gone in. */
#define DRAIN_TIMEOUT_NS (5 * 1000000000LL)

typedef struct options {
    char video_path[PATH_MAX];
    char audio_path[PATH_MAX];
    int fps;
    int width;
    int height;
} options;

/* The ipk stages the sample streams next to the binary, and a native webOS app is not
 * launched with its own directory as the working directory. */
static void resolve_default_paths(options *opts) {
    /* Leave headroom for "/sample.h264" so the join below cannot be truncated. */
    char dir[PATH_MAX - 16];
    ssize_t len = readlink("/proc/self/exe", dir, sizeof(dir) - 1);
    if (len <= 0) {
        snprintf(opts->video_path, sizeof(opts->video_path), "sample.h264");
        snprintf(opts->audio_path, sizeof(opts->audio_path), "sample.aac");
        return;
    }
    dir[len] = '\0';
    char *slash = strrchr(dir, '/');
    if (slash != NULL) {
        *slash = '\0';
    }
    snprintf(opts->video_path, sizeof(opts->video_path), "%s/sample.h264", dir);
    snprintf(opts->audio_path, sizeof(opts->audio_path), "%s/sample.aac", dir);
}

static bool parse_options(int argc, char *argv[], options *opts) {
    memset(opts, 0, sizeof(*opts));
    opts->fps = DEFAULT_FPS;
    opts->width = DEFAULT_WIDTH;
    opts->height = DEFAULT_HEIGHT;
    resolve_default_paths(opts);

    for (int i = 1; i < argc; i++) {
        /* The app manager hands native apps a JSON launch-parameters object as argv[1].
         * It is handled separately, in main(); rejecting it here would stop the app from
         * starting at all, with no console to say why. */
        if (argv[i][0] == '{') {
            continue;
        }
        if (strcmp(argv[i], "--video") == 0 && i + 1 < argc) {
            snprintf(opts->video_path, sizeof(opts->video_path), "%s", argv[++i]);
        } else if (strcmp(argv[i], "--audio") == 0 && i + 1 < argc) {
            snprintf(opts->audio_path, sizeof(opts->audio_path), "%s", argv[++i]);
        } else if (strcmp(argv[i], "--fps") == 0 && i + 1 < argc) {
            opts->fps = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--size") == 0 && i + 1 < argc) {
            if (sscanf(argv[++i], "%dx%d", &opts->width, &opts->height) != 2) {
                return false;
            }
        } else {
            fprintf(stderr,
                    "usage: %s [--video sample.h264] [--audio sample.aac]\n"
                    "          [--fps 30] [--size 1280x720]\n",
                    argv[0]);
            return false;
        }
    }
    return opts->fps > 0 && opts->width > 0 && opts->height > 0;
}

int main(int argc, char *argv[]) {
    /* Do this before anything else that might want to complain: launched from the TV's
     * home screen there is no console, so without a log file nothing below is visible. */
    const char *launch_params = app_launch_params(argc, argv);
    char log_path[PATH_MAX];
    if (app_launch_param_string(launch_params, "log", log_path, sizeof(log_path))) {
        app_log_to_file(log_path);
    }

    options opts;
    if (!parse_options(argc, argv, &opts)) {
        return 2;
    }
    if (launch_params != NULL) {
        fprintf(stderr, "[main] launch params: %s\n", launch_params);
    }

    /* Set by the webOS launcher for native apps. The pipeline uses it to account for the
     * decoder resources this app holds, and will not play without a plausible value. */
    const char *app_id = getenv("APPID");
    if (app_id == NULL) {
        app_id = "org.webosbrew.sample.media.smp.acb";
        fprintf(stderr, "[main] APPID unset, assuming %s\n", app_id);
    }

    es_file *video = es_file_open_h264(opts.video_path, opts.fps, 1);
    es_file *audio = es_file_open_adts(opts.audio_path);
    if (video == NULL || audio == NULL) {
        es_file_close(video);
        es_file_close(audio);
        return 1;
    }

    sdl_shell shell;
    if (!sdl_shell_preinit() || !sdl_shell_open_window(&shell, "SMP ACB")) {
        return 1;
    }
    /* Clear to transparent once, up front: from here on the screen belongs to the video
     * plane, and the sample has nothing of its own to draw. */
    sdl_shell_present_transparent(&shell);

    acb_plane acb;
    if (!acb_plane_init(&acb, &shell, app_id, opts.width, opts.height)) {
        sdl_shell_close(&shell);
        return 1;
    }

    smp_video_plane plane;
    acb_plane_bind(&acb, &plane);

    smp_player *player = smp_player_create(&plane);
    if (player == NULL) {
        acb_plane_destroy(&acb);
        sdl_shell_close(&shell);
        return 1;
    }

    smp_load_params params = {
            .app_id = app_id,
            .has_video = true,
            .video_codec = "H264",
            .video_width = opts.width,
            .video_height = opts.height,
            .video_fps_num = opts.fps,
            .video_fps_den = 1,
            .has_audio = true,
            .audio_codec = "AAC",
            .audio_channels = es_file_channels(audio),
            .audio_sample_rate = es_file_sample_rate(audio),
            .aac_object_type = es_file_aac_object_type(audio),
    };

    int exit_code = 0;
    if (!smp_player_load(player, &params)) {
        exit_code = 1;
        goto teardown;
    }

    /*
     * Feed loop.
     *
     * Two files, one time base. Whichever stream has the earlier next timestamp goes in
     * next, which keeps the pipeline's two queues advancing together without needing a
     * thread each.
     *
     * `clock_origin` is only established once the pipeline has actually accepted
     * something. Starting it earlier would count the Load-to-LOADCOMPLETED wait as
     * playback time, and the sample would then try to catch up by feeding a burst.
     */
    es_sample video_sample;
    es_sample audio_sample;
    bool have_video = es_file_next(video, &video_sample);
    bool have_audio = es_file_next(audio, &audio_sample);
    int64_t clock_origin = 0;
    long fed_video = 0;
    long fed_audio = 0;

    while ((have_video || have_audio) && sdl_shell_pump(&shell)) {
        bool feed_video = have_video && (!have_audio ||
                                         video_sample.pts_ns <= audio_sample.pts_ns);
        const es_sample *sample = feed_video ? &video_sample : &audio_sample;

        if (clock_origin != 0) {
            pacer_sleep_until(clock_origin, sample->pts_ns);
        }

        smp_feed_result result = smp_player_feed(player, sample->data, sample->size,
                                                 sample->pts_ns,
                                                 feed_video ? SMP_ES_VIDEO : SMP_ES_AUDIO);
        if (result == SMP_FEED_BUFFER_FULL || result == SMP_FEED_PENDING) {
            /* Normal back-pressure - the pipeline's queue is full, or it is not ready yet.
             * Offer the same buffer again shortly; do not advance. */
            pacer_sleep_ns(BUFFER_FULL_BACKOFF_NS);
            continue;
        }
        if (result != SMP_FEED_OK) {
            fprintf(stderr, "[main] feed failed, stopping\n");
            exit_code = 1;
            break;
        }

        if (clock_origin == 0) {
            clock_origin = pacer_now_ns() - sample->pts_ns;
        }
        if (feed_video) {
            fed_video++;
            if (fed_video % 100 == 0) {
                fprintf(stderr, "[main] fed %ld video / %ld audio, media time %.1f s\n",
                        fed_video, fed_audio, (double) sample->pts_ns / 1e9);
            }
            have_video = es_file_next(video, &video_sample);
        } else {
            fed_audio++;
            have_audio = es_file_next(audio, &audio_sample);
        }
    }

    fprintf(stderr, "[main] feed loop done: %ld video, %ld audio units\n", fed_video,
            fed_audio);

    /* Everything is in the pipeline's queues now, but not yet on screen. Say so, then wait
     * for it to drain, so the tail of the clip is not cut off. */
    if (exit_code == 0) {
        smp_player_push_eos(player);
        int64_t deadline = pacer_now_ns() + DRAIN_TIMEOUT_NS;
        while (!smp_player_ended(player) && !smp_player_errored(player) &&
               pacer_now_ns() < deadline && sdl_shell_pump(&shell)) {
            pacer_sleep_ns(20 * 1000 * 1000);
        }
    }

teardown:
    smp_player_unload(player);
    smp_player_destroy(player);
    acb_plane_destroy(&acb);
    sdl_shell_close(&shell);
    es_file_close(video);
    es_file_close(audio);
    return exit_code;
}
