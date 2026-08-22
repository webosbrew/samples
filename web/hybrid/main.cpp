// One process, two windows: an SDL2 view and a libcbe web view, swapping places.
//
// This is the shape a hybrid app wants - native code drawing one screen, a real
// browser drawing another - and the awkward part is that the two disagree about
// who owns the process. WebOSMain() is Chromium's content main: it never returns
// and it owns the message loop. SDL normally wants a `while (SDL_PollEvent)` loop
// in main(). Only one of them can have it.
//
// Chromium wins, and SDL is driven from its loop instead. libcbe pumps the
// default GMainContext on its browser UI thread, so a g_timeout_add() there polls
// SDL events and repaints at a fixed rate. Both toolkits then live on one thread
// with one loop, each with its own Wayland surface, and swapping views is a
// matter of hiding one and showing the other.
//
//   [SDL view] --OK/Enter--> [web view] --exit button or Back--> [SDL view]

#include <SDL.h>
#include <glib.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <string>
#include <vector>

#include "webos/webapp_window_base.h"
#include "webos/webview_base.h"

extern "C" int WebOSMain(int argc, const char** argv);

namespace {

const char kAppId[] = "org.webosbrew.sample.web.hybrid";

std::string g_app_path;

// ---------------------------------------------------------------- web view

class HybridWebView;
void ShowNativeView();

// The page asks to leave through libcbe's own callbacks, not through a side
// effect of some UI property. Loading the "palmsystem" injection gives page
// JavaScript real entry points, and two of them land here:
//
//   PalmSystem.close()         -> WebViewDelegate::Close()
//   PalmSystem.platformBack()  -> HandleBrowserControlCommand("platformBack")
//
// Close() is the delegate's own dedicated slot, so nothing is overloaded and
// nothing has to be parsed out of a shared channel.

bool g_web_visible;

class HybridWebView : public webos::WebViewBase {
 public:
  void TitleChanged(const std::string& title) override {
    printf("[web] title '%s'\n", title.c_str());
  }
  void LoadFinished(const std::string&) override { puts("[web] load finished"); }
  void LoadFailed(const std::string& url, int code, const std::string& desc) override {
    printf("[web] FAILED %s (%d %s)\n", url.c_str(), code, desc.c_str());
  }
  void RenderProcessCreated(int pid) override { printf("[web] renderer pid %d\n", pid); }

  void LoadProgressChanged(double) override {}
  void DidFirstFrameFocused() override {}
  void LoadVisuallyCommitted() override {}
  void NavigationHistoryChanged() override {}
  // PalmSystem.close(): the page is done and wants to be dismissed.
  void Close() override {
    puts("[web] page called PalmSystem.close()");
    ShowNativeView();
  }

  // PalmSystem.platformBack(): a Back gesture the page chose not to consume.
  void HandleBrowserControlCommand(const std::string& command,
                                   const std::vector<std::string>&) override {
    printf("[web] browser control '%s'\n", command.c_str());
    if (command == "platformBack") ShowNativeView();
  }
  bool DecidePolicyForResponse(bool, int, const std::string&, const std::string&) override {
    return false;
  }
  void LoadStarted() override { puts("[web] load started"); }
  void LoadStopped() override { puts("[web] load stopped"); }
  void DidStartNavigation(const std::string& u, bool) override {
    printf("[web] navigate %s\n", u.c_str());
  }
  void DidFinishNavigation(const std::string&, bool) override {}
  void LoadAborted(const std::string&) override {}
  void DocumentLoadFinished() override {}
  void RenderProcessGone() override { puts("[web] renderer gone"); }
};

class HybridWindow : public webos::WebAppWindowBase {
 public:
  // The remote's Back key, if the TV lets the app have it.
  bool event(WebOSEvent*) override { return false; }
};

HybridWindow* g_window;
HybridWebView* g_webview;

// ---------------------------------------------------------------- SDL view

SDL_Window* g_sdl_window;
SDL_Renderer* g_sdl_renderer;
bool g_native_visible;
int g_frame;

void DrawNativeView() {
  // Something obviously native and obviously alive, so it is clear at a glance
  // which of the two views is on screen.
  const double phase = g_frame / 60.0;
  const Uint8 pulse = static_cast<Uint8>(40 + 30 * (1 + SDL_sin(phase)));
  SDL_SetRenderDrawColor(g_sdl_renderer, pulse, pulse / 2, 120, 255);
  SDL_RenderClear(g_sdl_renderer);

  SDL_SetRenderDrawColor(g_sdl_renderer, 255, 255, 255, 255);
  const int bar = 40 + (g_frame % 120) * 8;
  SDL_Rect r = {200, 500, bar, 80};
  SDL_RenderFillRect(g_sdl_renderer, &r);
  SDL_RenderPresent(g_sdl_renderer);
  ++g_frame;
}

// --------------------------------------------------------------- switching

void ShowWebView() {
  if (g_web_visible) return;
  puts("[switch] native -> web");
  g_native_visible = false;
  SDL_HideWindow(g_sdl_window);

  const bool first_time = (g_webview == NULL);
  if (first_time) {
    g_window = new HybridWindow();
    g_window->InitWindow(1920, 1080);
    g_window->SetWindowProperty("appId", kAppId);
    g_window->SetWindowHostState(webos::NATIVE_WINDOW_FULLSCREEN);

    g_webview = new HybridWebView();
    g_webview->Initialize(kAppId, g_app_path, "trusted", "", "", 1920, 1080, false);
    g_webview->SetAppId(kAppId);
    g_webview->SetAppPath(g_app_path);
    // All three are needed to load the app's own page.html over file://. Two are
    // not enough: without SetAllowUniversalAccessFromFileUrls the renderer is
    // killed mid-load with "bad IPC message, reason 114" rather than being told
    // no. SetWebSecurityEnabled(false), which is the obvious sledgehammer, turns
    // out not to be needed at all.
    g_webview->SetAllowLocalResourceLoad(true);
    g_webview->SetFileAccessBlocked(false);
    g_webview->SetAllowUniversalAccessFromFileUrls(true);
    g_webview->SetLocalStorageEnabled(true);
    // Without this the page has no PalmSystem object and therefore no way to
    // reach native code at all. It needs the "trusted" trust level passed to
    // Initialize() above, and the name is "palmsystem", not "v8/palmsystem".
    g_webview->LoadExtension("palmsystem");
    g_webview->UpdatePreferences();
    g_window->AttachWebContents(g_webview->GetWebContents());
  } else {
    // Second time round the page is still loaded - only woken up.
    g_webview->ResumeWebPageDOM();
    g_webview->ResumePaintingAndSetVisibilityVisible();
  }

  g_webview->SetVisible(true);
  g_window->Show();
  g_window->Activate();
  g_web_visible = true;

  // Only the first time. Coming back to a suspended page is the point of
  // Suspend/Resume - reloading would throw away whatever state it had.
  if (first_time) {
    const std::string url = "file://" + g_app_path + "/page.html";
    printf("[web] loading %s\n", url.c_str());
    g_webview->LoadUrl(url);
  }
}

void ShowNativeView() {
  if (!g_native_visible) {
    puts("[switch] web -> native");
    if (g_web_visible) {
      // Suspending the page as well as hiding the window is what stops a
      // backgrounded web view from burning CPU on timers and animation.
      g_webview->SuspendPaintingAndSetVisibilityHidden();
      g_webview->SuspendWebPageDOM();
      g_webview->SetVisible(false);
      g_window->SetWindowHostState(webos::NATIVE_WINDOW_MINIMIZED);
      g_window->Hide();
      g_web_visible = false;
    }
    SDL_ShowWindow(g_sdl_window);
    SDL_RaiseWindow(g_sdl_window);
    g_native_visible = true;
  }
}

// ------------------------------------------------------------- the one loop

gboolean Pump(gpointer) {
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT) return G_SOURCE_REMOVE;
    if (e.type != SDL_KEYDOWN) continue;
    printf("[sdl] key %d\n", static_cast<int>(e.key.keysym.sym));
    switch (e.key.keysym.sym) {
      case SDLK_RETURN:
      case SDLK_KP_ENTER:
      case SDLK_SPACE:
        if (g_native_visible) ShowWebView();
        break;
      case SDLK_AC_BACK:
      case SDLK_ESCAPE:
      case SDLK_BACKSPACE:
        if (!g_native_visible) ShowNativeView();
        break;
      default:
        break;
    }
  }
  if (g_native_visible && g_sdl_renderer != NULL) DrawNativeView();
  return G_SOURCE_CONTINUE;
}

// Runs on Chromium's browser UI thread, once, as soon as the browser is up.
gboolean StartApp(gpointer) {
  // SDL reads its EGL platform from the environment at video-init time.
  setenv("EGL_PLATFORM", "wayland", 0);

  if (SDL_Init(0) != 0) {
    printf("[sdl] SDL_Init failed: %s\n", SDL_GetError());
    return G_SOURCE_REMOVE;
  }
  // Without these the TV keeps Back and Exit for itself and the app never sees
  // them. They are plain strings, so setting them costs nothing where they are
  // not understood.
  SDL_SetHint("SDL_WEBOS_ACCESS_POLICY_KEYS_BACK", "true");
  SDL_SetHint("SDL_WEBOS_ACCESS_POLICY_KEYS_EXIT", "true");

  if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
    printf("[sdl] SDL_INIT_VIDEO failed: %s\n", SDL_GetError());
    return G_SOURCE_REMOVE;
  }
  printf("[sdl] video driver: %s\n", SDL_GetCurrentVideoDriver());

  g_sdl_window = SDL_CreateWindow("hybrid", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                  1920, 1080, SDL_WINDOW_FULLSCREEN);
  if (g_sdl_window == NULL) {
    printf("[sdl] SDL_CreateWindow failed: %s\n", SDL_GetError());
    return G_SOURCE_REMOVE;
  }
  g_sdl_renderer = SDL_CreateRenderer(g_sdl_window, -1, SDL_RENDERER_ACCELERATED);
  if (g_sdl_renderer == NULL) {
    printf("[sdl] SDL_CreateRenderer failed: %s\n", SDL_GetError());
    return G_SOURCE_REMOVE;
  }
  g_native_visible = true;
  puts("[sdl] native view up - OK/Enter opens the web view");

  g_timeout_add(16, Pump, NULL);
  return G_SOURCE_REMOVE;
}

bool IsBrowserProcess(int argc, char** argv) {
  for (int i = 1; i < argc; ++i)
    if (strncmp(argv[i], "--type=", 7) == 0) return false;
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  // SAM points a launched app's stdout at /dev/null, so everything printed below
  // is invisible unless it is redirected somewhere. (libcbe's own logging goes
  // through PmLog and reaches /var/log/messages regardless.)
  if (IsBrowserProcess(argc, argv)) {
    freopen("/tmp/" APP_LOG_NAME ".log", "w", stdout);
    dup2(1, 2);
  }
  setvbuf(stdout, NULL, _IOLBF, 0);

  const char* slash = strrchr(argv[0], '/');
  g_app_path = slash ? std::string(argv[0], slash - argv[0]) : std::string(".");
  const bool browser = IsBrowserProcess(argc, argv);

  std::vector<std::string> args;
  args.push_back(argv[0]);
  if (browser) {
    args.push_back("--ozone-platform=wayland");
    args.push_back("--no-sandbox");
    args.push_back("--no-zygote");
    args.push_back("--in-process-gpu");
    args.push_back(std::string("--browser-subprocess-path=") + argv[0]);
    args.push_back(std::string("--user-data-dir=/tmp/") + kAppId);
    // Page console.log is invisible without this.
    args.push_back("--enable-logging=stderr");
  }
  for (int i = 1; i < argc; ++i) args.push_back(argv[i]);

  std::vector<const char*> cargv;
  for (size_t i = 0; i < args.size(); ++i) cargv.push_back(args[i].c_str());

  if (!getenv("XDG_RUNTIME_DIR")) setenv("XDG_RUNTIME_DIR", "/tmp/xdg", 1);

  // Chromium re-execs this same binary for the renderer; only the browser
  // process gets windows.
  if (browser) g_idle_add(StartApp, NULL);

  return WebOSMain(static_cast<int>(cargv.size()), cargv.data());
}
