#include "sdl_shell.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_SDL_WEBOS
#include <SDL_webOS.h>

/* Re-declared weak. The webOS extensions did not all arrive at once: the SDL that ships on
 * webOS 3 and older has the access-policy hints but not this call, and a hard reference
 * would stop the app from loading there. Hints cost nothing to set on firmware that
 * ignores them, because they are plain strings rather than symbols.
 *
 * Which symbols a given firmware actually exports is not a guess - check a built ipk with
 * `webosbrew-ipk-verify`, which is what turned this one up. */
extern SDL_bool SDL_webOSCursorVisibility(SDL_bool visible) __attribute__((weak));
#endif

bool sdl_shell_preinit(void) {
    /* SDL picks its EGL platform from the environment at video-init time, and a native
     * webOS app is not launched with these set. They have to be in place before
     * SDL_INIT_VIDEO, which is one of the reasons init is split in two here. */
    setenv("EGL_PLATFORM", "wayland", 0);
    setenv("XDG_RUNTIME_DIR", "/tmp/xdg", 0);

    if (SDL_Init(0) != 0) {
        fprintf(stderr, "sdl_shell: SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

#ifdef HAVE_SDL_WEBOS
    /* Without these the TV keeps Back and Exit for itself and the app never sees them,
     * so there would be no way to leave a fullscreen sample. */
    SDL_SetHint(SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_BACK, "true");
    SDL_SetHint(SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_EXIT, "true");
#endif
    return true;
}

bool sdl_shell_open_window(sdl_shell *shell, const char *title) {
    SDL_memset(shell, 0, sizeof(*shell));

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "sdl_shell: SDL_INIT_VIDEO failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_DisplayMode mode;
    if (SDL_GetCurrentDisplayMode(0, &mode) != 0) {
        fprintf(stderr, "sdl_shell: SDL_GetCurrentDisplayMode failed: %s\n", SDL_GetError());
        return false;
    }
    shell->display_width = mode.w;
    shell->display_height = mode.h;

    /* SDL_WINDOW_FULLSCREEN, not FULLSCREEN_DESKTOP: on webOS the desktop-fullscreen path
     * does not give the app the whole panel. */
    shell->window = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                     mode.w, mode.h, SDL_WINDOW_FULLSCREEN);
    if (shell->window == NULL) {
        fprintf(stderr, "sdl_shell: SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    shell->renderer = SDL_CreateRenderer(shell->window, -1, SDL_RENDERER_ACCELERATED);
    if (shell->renderer == NULL) {
        fprintf(stderr, "sdl_shell: SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }

#ifdef HAVE_SDL_WEBOS
    if (SDL_webOSCursorVisibility != NULL) {
        SDL_webOSCursorVisibility(SDL_FALSE);
    }
#endif
    return true;
}

void sdl_shell_present_transparent(sdl_shell *shell) {
    SDL_SetRenderDrawBlendMode(shell->renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(shell->renderer, 0, 0, 0, 0);
    SDL_RenderClear(shell->renderer);
    SDL_RenderPresent(shell->renderer);
}

bool sdl_shell_pump(sdl_shell *shell) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                shell->quit_requested = true;
                break;
            case SDL_KEYDOWN:
                switch ((int) event.key.keysym.scancode) {
                    case SDL_SCANCODE_ESCAPE:
                    case SDL_SCANCODE_AC_BACK:
#ifdef HAVE_SDL_WEBOS
                    /* The remote's Back and Exit keys arrive as ordinary key events with
                     * webOS-specific scancodes. */
                    case SDL_WEBOS_SCANCODE_BACK:
                    case SDL_WEBOS_SCANCODE_EXIT:
#endif
                        shell->quit_requested = true;
                        break;
                    default:
                        break;
                }
                break;
            default:
                break;
        }
    }
    return !shell->quit_requested;
}

void sdl_shell_close(sdl_shell *shell) {
    if (shell->renderer != NULL) {
        SDL_DestroyRenderer(shell->renderer);
        shell->renderer = NULL;
    }
    if (shell->window != NULL) {
        SDL_DestroyWindow(shell->window);
        shell->window = NULL;
    }
    SDL_Quit();
}
