/* A ruler for the pointer.
 *
 * webOS injects pointer events through com.webos.service.networkinput, and the
 * Magic Remote cursor it draws is on an overlay plane that
 * com.webos.service.capture does not include - so a screenshot cannot show
 * where the pointer is. That makes it impossible to tell, from the host, what
 * a given wire encoding actually did.
 *
 * This draws the answer instead. A crosshair marks the cursor, so a screenshot
 * carries the position, and every event is also appended to a log file with
 * exact numbers, so no pixel measuring is needed at all:
 *
 *     ares-do -d tv exec 'tail -5 /tmp/input-pointer.log'
 *
 * Drawn with glScissor + glClear only. No shaders, no font, nothing that could
 * fail differently between the NDK's SDL and the one on the TV - the point is
 * to measure the input path, so the drawing path must not be in question.
 */
#include <SDL.h>
#include <SDL_opengles2.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

/* Requested size, which is not necessarily what you get.
 *
 * A 1080p request comes back as a 1280x720 window on a 49LK5900 - and the
 * panel is 1080p, so this is not a panel limit. The graphics UI runs at 720p
 * system-wide on this model: configd selects the `uires/hd720` layer via
 * /usr/bin/starfish-selector, giving system.sysAsset = "1280x720", and SDL's
 * wayland backend duly reports the display as 1280x720. The compositor scales
 * that to the 1080p output, which is why a screenshot is 1920x1080 while
 * pointer coordinates are in a 1280x720 space - exactly 1.5x apart.
 *
 * So the real window size is queried at startup rather than assumed. Getting
 * it wrong silently mis-places the crosshair, which is the one thing this
 * sample exists to get right.
 *
 *     ares-do -d tv luna luna://com.webos.service.config/getConfigs \
 *       '{"configNames":["system.sysAsset"]}'   -> {"system.sysAsset":"1280x720"} */
#define REQ_W 1920
#define REQ_H 1080

static int W = REQ_W;
static int H = REQ_H;

static FILE *g_log;

static void logf_(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    if (g_log) {
        vfprintf(g_log, fmt, ap);
        fputc('\n', g_log);
        fflush(g_log);
    }
    va_end(ap);
}

/* glScissor's origin is bottom-left; everything here thinks in top-left. */
static void fill(int x, int y, int w, int h, float r, float g, float b) {
    if (w <= 0 || h <= 0) return;
    /* Scissor works in drawable pixels; the caller thinks in window
     * coordinates, which are the same here only because no HiDPI scaling is
     * in play on this device. */
    glScissor(x, H - y - h, w, h);
    glClearColor(r, g, b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    g_log = fopen("/tmp/input-pointer.log", "w");
    setvbuf(stdout, NULL, _IOLBF, 0);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("[pointer] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    SDL_Window *win = SDL_CreateWindow("pointer", SDL_WINDOWPOS_UNDEFINED,
                                       SDL_WINDOWPOS_UNDEFINED, REQ_W, REQ_H,
                                       SDL_WINDOW_FULLSCREEN | SDL_WINDOW_OPENGL);
    if (!win) {
        printf("[pointer] SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GLContext gl = SDL_GL_CreateContext(win);
    if (!gl) {
        printf("[pointer] SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return 1;
    }
    printf("[pointer] GL %s on %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));

    int dw = REQ_W, dh = REQ_H;
    SDL_GL_GetDrawableSize(win, &dw, &dh);
    int ww = REQ_W, wh = REQ_H;
    SDL_GetWindowSize(win, &ww, &wh);
    /* Pointer events arrive in window coordinates, so that is what the
     * crosshair has to be drawn in; the viewport maps it to the drawable. */
    W = ww; H = wh;
    glViewport(0, 0, dw, dh);
    logf_("start window=%dx%d drawable=%dx%d requested=%dx%d", ww, wh, dw, dh, REQ_W, REQ_H);
    logf_("driver=%s displays=%d", SDL_GetCurrentVideoDriver(), SDL_GetNumVideoDisplays());
    for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
        SDL_Rect b = {0, 0, 0, 0};
        SDL_DisplayMode dm;
        SDL_GetDisplayBounds(i, &b);
        if (SDL_GetCurrentDisplayMode(i, &dm) == 0)
            logf_("display %d bounds=%dx%d+%d+%d mode=%dx%d@%d", i, b.w, b.h, b.x, b.y,
                  dm.w, dm.h, dm.refresh_rate);
        if (SDL_GetDesktopDisplayMode(i, &dm) == 0)
            logf_("display %d desktop=%dx%d@%d modes=%d", i, dm.w, dm.h, dm.refresh_rate,
                  SDL_GetNumDisplayModes(i));
        for (int m = 0; m < SDL_GetNumDisplayModes(i); m++)
            if (SDL_GetDisplayMode(i, m, &dm) == 0)
                logf_("  mode %d: %dx%d@%d", m, dm.w, dm.h, dm.refresh_rate);
    }
    printf("[pointer] window %dx%d drawable %dx%d\n", ww, wh, dw, dh);

    int mx = W / 2, my = H / 2;      /* last reported position, window coords */
    int buttons = 0;                 /* bitmask of held buttons */
    int motions = 0, clicks = 0, wheels = 0;
    int flash = 0;                   /* frames left of the click flash */
    int running = 1;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT:
                running = 0;
                break;
            case SDL_MOUSEMOTION:
                mx = e.motion.x; my = e.motion.y; motions++;
                logf_("motion x=%d y=%d xrel=%d yrel=%d state=%u",
                      e.motion.x, e.motion.y, e.motion.xrel, e.motion.yrel, e.motion.state);
                break;
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP: {
                int down = (e.type == SDL_MOUSEBUTTONDOWN);
                mx = e.button.x; my = e.button.y;
                if (down) { buttons |= 1 << e.button.button; clicks++; flash = 20; }
                else       buttons &= ~(1 << e.button.button);
                logf_("button %s which=%u btn=%u clicks=%u x=%d y=%d",
                      down ? "down" : "up", e.button.which, e.button.button,
                      e.button.clicks, e.button.x, e.button.y);
                break;
            }
            case SDL_MOUSEWHEEL:
                wheels++;
                logf_("wheel x=%d y=%d direction=%u", e.wheel.x, e.wheel.y, e.wheel.direction);
                break;
            case SDL_FINGERDOWN:
            case SDL_FINGERUP:
            case SDL_FINGERMOTION:
                logf_("finger type=%u x=%.4f y=%.4f", e.type, e.tfinger.x, e.tfinger.y);
                break;
            case SDL_KEYDOWN:
                logf_("key scancode=%d sym=%d", e.key.keysym.scancode, e.key.keysym.sym);
                if (e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) running = 0;
                break;
            default:
                break;
            }
        }

        /* Background: dark, or a bright flash for a moment after a click, so a
         * click is visible in a screenshot taken shortly afterwards. */
        glScissor(0, 0, W, H);
        if (flash > 0) { glClearColor(0.35f, 0.10f, 0.10f, 1.0f); flash--; }
        else             glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        /* Ruler: a tick every 100px, taller every 500, so a screenshot can be
         * read off by eye as well as by machine. */
        for (int x = 0; x < W; x += 100) fill(x, 0, 2, (x % 500) ? 18 : 40, .3f, .3f, .35f);
        for (int y = 0; y < H; y += 100) fill(0, y, (y % 500) ? 18 : 40, 2, .3f, .3f, .35f);

        /* Crosshair through the pointer: the bar positions *are* the coordinates. */
        fill(mx, 0, 3, H, 0.15f, 0.55f, 0.95f);
        fill(0, my, W, 3, 0.15f, 0.55f, 0.95f);

        /* The pointer itself, green while a button is held. */
        int held = buttons != 0;
        fill(mx - 18, my - 18, 37, 37, held ? 0.2f : 1.0f, held ? 0.9f : 0.75f, 0.2f);

        /* Counters as bars along the bottom, one pixel per event, so a
         * screenshot shows whether anything arrived at all. */
        fill(0, H - 8, motions % W, 8, 0.2f, 0.8f, 0.4f);
        fill(0, H - 18, (clicks * 40) % W, 8, 0.9f, 0.4f, 0.2f);
        fill(0, H - 28, (wheels * 40) % W, 8, 0.6f, 0.4f, 0.9f);

        SDL_GL_SwapWindow(win);
        SDL_Delay(16);
    }

    SDL_GL_DeleteContext(gl);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
