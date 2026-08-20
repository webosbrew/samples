#include "exported_window.h"

#include <stdio.h>
#include <string.h>

#include <SDL_webOS.h>

#include "json_scan.h"

/* Stretches the plane so `video_width x video_height` fills the panel. */
static void apply_geometry(exported_window *window) {
    if (window->window_id[0] == '\0') {
        return;
    }
    SDL_Rect src = { 0, 0, window->video_width, window->video_height };
    SDL_Rect dst = { 0, 0, window->shell->display_width, window->shell->display_height };
    if (!SDL_webOSSetExportedWindow(window->window_id, &src, &dst)) {
        fprintf(stderr, "[plane] SDL_webOSSetExportedWindow failed: %s\n", SDL_GetError());
        return;
    }
    fprintf(stderr, "[plane] window %s: %dx%d -> %dx%d\n", window->window_id, src.w, src.h,
            dst.w, dst.h);
}

static bool prepare_load(void *self, smp_load_params *params) {
    exported_window *window = self;

    if (window->window_id[0] == '\0') {
        /* Type 0 is the video plane. The returned string is owned by SDL and is only
         * guaranteed until the next call, so it gets copied. */
        const char *created = SDL_webOSCreateExportedWindow(
                SDL_WEBOS_EXPORED_WINDOW_TYPE_VIDEO);
        if (created == NULL) {
            fprintf(stderr, "[plane] SDL_webOSCreateExportedWindow failed: %s\n", SDL_GetError());
            return false;
        }
        snprintf(window->window_id, sizeof(window->window_id), "%s", created);
        fprintf(stderr, "[plane] created exported window %s\n", window->window_id);
    }

    params->window_id = window->window_id;
    return true;
}

static void post_load(void *self, const smp_load_params *params) {
    exported_window *window = self;
    if (params->has_video) {
        window->video_width = params->video_width;
        window->video_height = params->video_height;
    }
    apply_geometry(window);
}

static void video_info(void *self, const char *info_json) {
    exported_window *window = self;

    /* The decoder knows the real picture size; whatever the sample was told on the command
     * line was only a guess. Re-apply the geometry once the truth arrives, otherwise a
     * mismatch shows up as a stretched or cropped picture. */
    int width = 0;
    int height = 0;
    if (!json_scan_int(info_json, "width", &width) ||
        !json_scan_int(info_json, "height", &height)) {
        return;
    }
    if (width <= 0 || height <= 0) {
        return;
    }
    if (width == window->video_width && height == window->video_height) {
        return;
    }
    fprintf(stderr, "[plane] decoder reports %dx%d, was %dx%d\n", width, height,
            window->video_width, window->video_height);
    window->video_width = width;
    window->video_height = height;
    apply_geometry(window);
}

void exported_window_init(exported_window *window, sdl_shell *shell, int video_width,
                          int video_height) {
    memset(window, 0, sizeof(*window));
    window->shell = shell;
    window->video_width = video_width;
    window->video_height = video_height;
}

void exported_window_plane(exported_window *window, smp_video_plane *out) {
    memset(out, 0, sizeof(*out));
    out->self = window;
    out->prepare_load = prepare_load;
    out->post_load = post_load;
    out->video_info = video_info;
    /* load_completed, start_playing and post_unload have nothing to do here: unlike ACB,
     * the exported window needs no notification of pipeline state. */
}

void exported_window_destroy(exported_window *window) {
    if (window->window_id[0] != '\0') {
        SDL_webOSDestroyExportedWindow(window->window_id);
        window->window_id[0] = '\0';
    }
}
