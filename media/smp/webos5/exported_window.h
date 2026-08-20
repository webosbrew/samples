/*
 * The webOS 5+ video plane: an SDL "exported window".
 *
 * From webOS 5 the app asks SDL for a punch-through surface, gets back an opaque window
 * id, and hands that id to the pipeline inside the Load payload as option.windowId. The
 * compositor then puts the decoded video on a hardware plane behind the app's own surface,
 * wherever that surface is transparent.
 *
 * Two consequences worth noting, because they are easy to get wrong:
 *
 *   - The id has to exist before Load(), since it travels in the Load payload. Older
 *     firmware had a setWindowId() method, but it does not exist before webOS 5 either, so
 *     the payload is the only route.
 *   - The API lives in the webOS SDL fork, not in stock SDL2. The buildroot NDK ships
 *     2.30.12 with it; the SDL2 already on the TV does not have it, which is why the ipk
 *     bundles its own copy.
 */
#pragma once

#include <stdbool.h>

#include "sdl_shell.h"
#include "smp_player.h"

typedef struct exported_window {
    sdl_shell *shell;
    char window_id[64];
    int video_width;
    int video_height;
} exported_window;

void exported_window_init(exported_window *window, sdl_shell *shell, int video_width,
                          int video_height);

/* Fills in the smp_video_plane hooks. The player copies it, so this may be a stack value. */
void exported_window_plane(exported_window *window, smp_video_plane *out);

void exported_window_destroy(exported_window *window);
