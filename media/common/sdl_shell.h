/*
 * The SDL side of a webOS media sample: a fullscreen window that stays out of the way.
 *
 * On webOS the decoded video does not travel through SDL at all. It goes to a hardware
 * plane underneath the app's surface, and the app's job is to be transparent where the
 * video should show through. So this shell exists to do three things:
 *
 *   - set up the environment and hints in the order webOS needs them,
 *   - own a fullscreen window and renderer that clear to fully transparent black,
 *   - turn remote-control key presses into "the user wants to quit".
 *
 * Initialisation is split in two on purpose. Some webOS media backends (NDL in
 * particular) must be initialised before SDL brings up the video subsystem, so the
 * sequence is: sdl_shell_preinit() -> initialise the media API -> sdl_shell_open_window().
 * The Starfish samples do not strictly need the split, but they follow it so the two
 * families stay comparable.
 */
#pragma once

#include <stdbool.h>

#include <SDL.h>

typedef struct sdl_shell {
    SDL_Window *window;
    SDL_Renderer *renderer;
    /* Panel size in pixels - what the video plane should be stretched to. */
    int display_width;
    int display_height;
    bool quit_requested;
} sdl_shell;

/* Environment, SDL_Init(0) and the webOS hints. Call before touching anything else SDL,
 * and before initialising the media pipeline. */
bool sdl_shell_preinit(void);

/* Brings up SDL_INIT_VIDEO, a fullscreen window and an accelerated renderer. */
bool sdl_shell_open_window(sdl_shell *shell, const char *title);

/* Clears to transparent black and presents, letting the video plane below show through.
 * This is the whole of "punch-through" on webOS - there is no special surface type. */
void sdl_shell_present_transparent(sdl_shell *shell);

/* Drains the event queue. Returns false once the user has asked to quit (window close,
 * Escape, or the remote's Back / Exit keys). */
bool sdl_shell_pump(sdl_shell *shell);

void sdl_shell_close(sdl_shell *shell);
