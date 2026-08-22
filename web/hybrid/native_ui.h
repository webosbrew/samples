// The native half of the hybrid app's UI, drawn with Nuklear on GLES2.
//
// Kept behind this four-function C interface on purpose. main.cpp is about one
// process owning two window systems; the immediate-mode toolkit behind here is
// an implementation detail of "the native view", and inlining it would bury the
// part worth reading. It is also plain C, which is what Nuklear is.
#ifndef WEB_HYBRID_NATIVE_UI_H_
#define WEB_HYBRID_NATIVE_UI_H_

#include <SDL.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Creates the GLES2 context on the window and bakes the font atlas.
bool native_ui_init(SDL_Window *window);

// Feed every SDL event here, whether or not the native view is on screen.
void native_ui_handle_event(const SDL_Event *event);

// Draws one frame. Returns true if the user asked to open the web view.
bool native_ui_frame(SDL_Window *window);

// The value this side owns, and hands to the page on the way in.
int native_ui_counter(void);

// Whatever the page last sent back, shown in the panel.
void native_ui_set_web_message(const char *text);

void native_ui_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif  // WEB_HYBRID_NATIVE_UI_H_
