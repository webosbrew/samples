#include "native_ui.h"

#include <SDL_opengles2.h>

/* Nuklear is a single header that compiles its own implementation into exactly
 * one translation unit - this one. The SDL/GLES2 backend is a second such
 * header from the same repository. Neither is happy being compiled as C++,
 * which is the other reason this file is C. */
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_SDL_GLES2_IMPLEMENTATION
#include "nuklear.h"

/* The backend calls SDL_GetTicks64, which arrived in SDL 2.0.18. The TV ships
 * 2.0.4, and the buildroot NDK ships 2.30.12 - so this links cleanly on the host
 * and dies on the device the moment Nuklear times a double click. It was caught
 * by `-verify`, not by the compiler.
 *
 * A function-like macro substitutes at both call sites without patching the
 * fetched header, and without defining a competing SDL_GetTicks64 in this binary
 * that would then shadow the real one on firmware that has it. The 32-bit
 * counter wraps after ~49 days, which matters to nothing here. */
#define SDL_GetTicks64() ((Uint64)SDL_GetTicks())
#include "nuklear_sdl_gles2.h"
#undef SDL_GetTicks64

/* Nuklear's vertex and element scratch buffers. The UI here is a handful of
 * rectangles and some text, so these are generous. */
#define MAX_VERTEX_MEMORY (256 * 1024)
#define MAX_ELEMENT_MEMORY (128 * 1024)

/* The panel, centred on a 1920x1080 panel. */
#define PANEL_W 900
#define PANEL_H 470

static SDL_GLContext g_gl;
static struct nk_context *g_nk;
static bool g_open_requested;

/* The sign-in nonce, and whatever the flow produced. */
static char g_state[32] = "";
static char g_result[200] = "(not signed in)";

const char *native_ui_new_state(void) {
    /* Good enough to prove the redirect came from the attempt we started. A real
     * client wants something unguessable. */
    static unsigned n;
    SDL_snprintf(g_state, sizeof(g_state), "s%u-%u", ++n, (unsigned)SDL_GetTicks());
    return g_state;
}

const char *native_ui_state(void) { return g_state; }

void native_ui_set_result(const char *text) {
    SDL_strlcpy(g_result, text != NULL ? text : "", sizeof(g_result));
}

bool native_ui_init(SDL_Window *window) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    g_gl = SDL_GL_CreateContext(window);
    if (g_gl == NULL) {
        SDL_Log("native_ui: SDL_GL_CreateContext failed: %s", SDL_GetError());
        return false;
    }
    SDL_Log("native_ui: GL %s on %s", glGetString(GL_VERSION), glGetString(GL_RENDERER));

    g_nk = nk_sdl_init(window);
    if (g_nk == NULL) {
        SDL_Log("native_ui: nk_sdl_init failed");
        return false;
    }

    /* Nuklear's built-in font is baked at 13px, which is unreadable across a
     * living room. Rebaking it larger is the whole of "font handling" here -
     * there is no font file to ship. */
    struct nk_font_atlas *atlas;
    nk_sdl_font_stash_begin(&atlas);
    struct nk_font *font = nk_font_atlas_add_default(atlas, 30.0f, NULL);
    nk_sdl_font_stash_end();
    if (font != NULL) {
        nk_style_set_font(g_nk, &font->handle);
    }
    return true;
}

void native_ui_handle_event(const SDL_Event *event) {
    if (g_nk == NULL) {
        return;
    }
    /* nk_sdl_handle_event wants a mutable pointer but does not modify it. */
    SDL_Event copy = *event;
    nk_input_begin(g_nk);
    nk_sdl_handle_event(&copy);
    nk_input_end(g_nk);
}

bool native_ui_frame(SDL_Window *window) {
    if (g_nk == NULL) {
        return false;
    }
    int w = 0, h = 0;
    SDL_GetWindowSize(window, &w, &h);

    const struct nk_rect panel =
        nk_rect((float)(w - PANEL_W) / 2, (float)(h - PANEL_H) / 2, PANEL_W, PANEL_H);

    if (nk_begin(g_nk, "native", panel, NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER)) {
        nk_layout_row_dynamic(g_nk, 56, 1);
        nk_label(g_nk, "sign in", NK_TEXT_CENTERED);

        nk_layout_row_dynamic(g_nk, 34, 1);
        nk_label(g_nk, "The provider's login page runs in a web view;", NK_TEXT_CENTERED);
        nk_label(g_nk, "the code comes back in the redirect URL.", NK_TEXT_CENTERED);

        nk_layout_row_dynamic(g_nk, 16, 1);
        nk_spacing(g_nk, 1);

        nk_layout_row_dynamic(g_nk, 40, 1);
        nk_label(g_nk, g_result, NK_TEXT_LEFT);

        nk_layout_row_dynamic(g_nk, 72, 1);
        if (nk_button_label(g_nk, "Sign in")) {
            g_open_requested = true;
        }

        nk_layout_row_dynamic(g_nk, 32, 1);
        nk_label(g_nk, "or press OK on the remote", NK_TEXT_CENTERED);
    }
    nk_end(g_nk);

    glViewport(0, 0, w, h);
    glClearColor(0.05f, 0.07f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    nk_sdl_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_MEMORY, MAX_ELEMENT_MEMORY);
    SDL_GL_SwapWindow(window);

    const bool requested = g_open_requested;
    g_open_requested = false;
    return requested;
}

void native_ui_shutdown(void) {
    if (g_nk != NULL) {
        nk_sdl_shutdown();
        g_nk = NULL;
    }
    if (g_gl != NULL) {
        SDL_GL_DeleteContext(g_gl);
        g_gl = NULL;
    }
}
