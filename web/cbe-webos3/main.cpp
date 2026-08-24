// The webOS 3 build of web/cbe: the smallest thing that puts a page on screen,
// against the older libcbe.
//
// The shape is identical to web/cbe/main.cpp - hand the process to WebOSMain,
// get called back on the browser UI thread through the default GMainContext,
// build a window and a web view - and every difference is the API moving under
// it between webOS 3 and 4:
//
//   * WebViewBase takes its size in the constructor; there is no Initialize().
//   * The window has no InitWindow() either; Resize() gives it a size.
//   * The delegate has its own slot order, and LoadStarted carries a URL where
//     webOS 4 has a separate DidStartNavigation.
//   * All of it is the pre-C++11 std::string ABI.

#include <glib.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

#include "luna_register.h"
#include "webos/webapp_window_base.h"
#include "webos/webview_base.h"

extern "C" int WebOSMain(int argc, const char** argv);

namespace {

const char kAppId[] = "org.webosbrew.sample.web.cbe3";
const char kUrl[] = "https://example.com/";

class SampleWindow;
SampleWindow* g_window;

class SampleWebView : public webos::WebViewBase {
 public:
  SampleWebView(int w, int h) : webos::WebViewBase(w, h) {}

  void LoadProgressChanged(double progress, const std::string&) override {
    printf("[cbe] progress %3.0f%%\n", progress * 100);
  }
  void DidFirstFrameFocused() override {}
  void DidFirstNonBlankPaint() override {
    puts("[cbe] first non-blank paint");
    // Try asserting the window state once a frame actually exists - the
    // compositor may ignore it on a surface that has never committed a buffer.

  }
  void LoadVisuallyCommitted() override { puts("[cbe] visually committed"); }
  void TitleChanged(const std::string& title) override {
    printf("[cbe] title '%s'\n", title.c_str());
  }
  void NavigationHistoryChanged() override {}
  void Close() override { puts("[cbe] close requested"); }
  bool DecidePolicyForResponse(bool, int status, const std::string& url,
                               const std::string&) override {
    printf("[cbe] response %d %s\n", status, url.c_str());
    return false;
  }
  // Where a redirect would be caught on this generation.
  void LoadStarted(const std::string& url) override {
    printf("[cbe] load started %s\n", url.c_str());
  }
  void LoadFinished(const std::string& url) override {
    printf("[cbe] finished %s\n", url.c_str());
  }
  void LoadFailed(const std::string& url, int code, const std::string& desc) override {
    printf("[cbe] FAILED %s (%d %s)\n", url.c_str(), code, desc.c_str());
  }
  void LoadStopped(const std::string&) override { puts("[cbe] load stopped"); }
  void RenderProcessCreated(int pid) override { printf("[cbe] renderer pid %d\n", pid); }
  void RenderProcessGone() override { puts("[cbe] renderer gone"); }
  void DocumentLoadFinished() override { puts("[cbe] document loaded"); }
};

class SampleWindow : public webos::WebAppWindowBase {
 public:
  bool event(WebOSEvent*) override { return false; }
};

SampleWebView* g_webview;
std::string g_app_path;

gboolean CreateWebApp(gpointer) {
  // Before anything else: tell SAM this process is the app.
  luna_register_app(kAppId);



  g_window = new SampleWindow();
  // No InitWindow on webOS 3. WAM never calls Resize either, but dropping it
  // here stops the delegate firing at all and the Wayland connection starts
  // complaining "proxy already has listener", so it is doing something the
  // constructor alone does not.
  g_window->Resize(1920, 1080);
  g_window->SetScaleFactor(1.0f);
  g_window->SetOpacity(1.0f);
  g_window->SetHiddenState(false);
  // LSM ignores a surface with no window type. WAM sets this on every window it
  // creates, and it is the one property the earlier attempts were missing - see
  // its Wayland trace in README.md.
  g_window->SetWindowProperty("_WEBOS_WINDOW_TYPE", "_WEBOS_WINDOW_TYPE_CARD");
  g_window->SetWindowProperty("appId", kAppId);
  g_window->SetWindowProperty("title", "CBE WebView");
  g_window->SetWindowHostState(webos::NATIVE_WINDOW_FULLSCREEN);

  g_webview = new SampleWebView(1920, 1080);   // no Initialize either
  // Neither SetTrustLevel nor UpdatePreferences exists on webOS 3 - `-verify`
  // reports them undefined against a 3.4 dump, which is the cheapest way to find
  // out that a call you copied from the webOS 4 sample is not portable.
  g_webview->SetAppId(kAppId);
  g_webview->SetAllowLocalResourceLoad(true);
  g_webview->SetLocalStorageEnabled(true);
  g_webview->SetVisible(true);
  // Chromium does not paint a page it believes is hidden, and nothing sets this
  // for us - libcbe leaves the visibility state at its default.
  g_webview->SetVisibilityState(webos::WebViewBase::VISIBILITY_VISIBLE);

  // Show() is a no-op on this generation - see README - so the window state is
  // set directly. Neither maps the surface; libcbe reaches SetWidgetState(SHOW)
  // internally regardless, and that is the stub.
  void* contents = g_webview->GetWebContents();
  g_window->AttachWebContents(contents);

  g_window->Show();
  g_window->SetWindowHostState(webos::NATIVE_WINDOW_FULLSCREEN);
  // Everything here reads back valid except the host state, which stays 0
  // because the compositor never acknowledges a surface that has no buffer.
  printf("[cbe] window %dx%d native=%p handle=%u contents=%p host-state=%d\n",
         g_window->DisplayWidth(), g_window->DisplayHeight(),
         g_window->GetNativeWindow(), g_window->GetWindowHandle(), contents,
         (int)g_window->GetWindowHostState());

  printf("[cbe] loading %s\n", kUrl);
  g_webview->LoadUrl(kUrl);
  return G_SOURCE_REMOVE;
}

bool IsBrowserProcess(int argc, char** argv) {
  for (int i = 1; i < argc; ++i)
    if (strncmp(argv[i], "--type=", 7) == 0) return false;
  return true;
}

}  // namespace

int main(int argc, char** argv) {
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
    // "weboswayland", not "wayland" - webOS 3's Ozone registers it under that
    // name, and the wrong one gets as far as constructing a std::string from a
    // null and aborting with basic_string::_S_construct. Taken from WAM's own
    // WAM_SWITCHES on the device.
    // weboswayland, and it has to be: WebAppWindowBase only exists on that
    // backend. Constructing one under --ozone-platform=wayland segfaults inside
    // libcbe before the constructor returns. See README - webOS 3 ships two
    // ozone platforms and this API belongs to one of them.
    args.push_back("--ozone-platform=weboswayland");
    args.push_back("--no-sandbox");
    args.push_back("--no-zygote");
    args.push_back("--in-process-gpu");
    args.push_back(std::string("--browser-subprocess-path=") + argv[0]);
    args.push_back(std::string("--user-data-dir=/tmp/") + kAppId);
    // Required: without --webos-wam the process exits before writing a log line.
    args.push_back("--webos-wam");
    args.push_back("--noerrdialogs");
    // Page console.log lands in the app's log with this. --v=1 adds nothing:
    // verbose logging is compiled out of this build.
    args.push_back("--enable-logging=stderr");
    // webOS 3's GPU path needs more setup than webOS 4's. Without these the
    // command buffer fails to initialise - "Could not send
    // GpuCommandBufferMsg_Initialize". From WAM's own WAM_SWITCHES.
    args.push_back("--enable-gpu-rasterization");
    args.push_back("--enable-impl-side-painting");
    args.push_back("--ignore-gpu-blacklist");
    args.push_back("--enable-threaded-compositing");
    args.push_back("--num-raster-threads=2");
    args.push_back("--ui-use-prepare-shader-program");
    args.push_back("--ui-disable-opaque-shader-program");
    args.push_back("--disable-low-res-tiling");
  }
  // SAM hands a native app its launch parameters as a bare JSON argument. The
  // TV's own browser turns that into --webos-launch-json and does not forward
  // the raw form; libcbe would otherwise treat it as a URL.
  for (int i = 1; i < argc; ++i) {
    if (browser && argv[i][0] == '{') {
      args.push_back(std::string("--webos-launch-json=") + argv[i]);
    } else {
      args.push_back(argv[i]);
    }
  }

  std::vector<const char*> cargv;
  for (size_t i = 0; i < args.size(); ++i) cargv.push_back(args[i].c_str());

  if (!getenv("XDG_RUNTIME_DIR")) setenv("XDG_RUNTIME_DIR", "/tmp/xdg", 1);
  // The native browser has these; a plain native app does not.
  setenv("CHROMIUM_BROWSER", "yes", 1);
  setenv("BROWSER_NAME", "Chromium38", 1);
  setenv("FONTCONFIG_PATH", "/etc/fonts", 1);
  setenv("FONTCONFIG_FILE", "/etc/fonts/fonts.conf", 1);

  // webOS 3's WebOSMain does std::string(getenv("CDM_LIB_PATH")) with no null
  // check and appends "/libwidevinecdmadapter.so" to it, so an unset variable
  // aborts the process before anything of ours runs -
  // "basic_string::_S_construct null not valid", thrown from inside WebOSMain.
  // WAM gets it from its own environment; a plain native app does not.
  if (!getenv("CDM_LIB_PATH")) setenv("CDM_LIB_PATH", "/usr/lib", 1);
  if (browser) g_idle_add(CreateWebApp, NULL);

  return WebOSMain(static_cast<int>(cargv.size()), cargv.data());
}
