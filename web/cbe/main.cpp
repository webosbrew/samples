// Embedding the TV's own Chromium in a native webOS app.
//
// Every other sample in this repo calls a media API and keeps control of its own
// main loop. This one is the opposite, and that inversion is the point: libcbe
// exports WebOSMain(), which is Chromium's content main. It takes the process
// over - dispatching the renderer and GPU subprocesses, owning the message loop -
// and does not return until the browser shuts down. There is no "initialise the
// web view, then carry on".
//
// So the app hands the process over and arranges to be called back. The seam is
// the one WAM itself uses: libcbe drives its browser UI thread from the *default*
// GMainContext, which is how WAM's Luna service ends up running on that thread.
// Anything queued onto that context before WebOSMain() therefore runs exactly
// once, on the browser UI thread, as soon as Chromium is up - which is the first
// moment a window or a web view may legally be created.

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

const char kAppId[] = "org.webosbrew.sample.web.cbe";
const char kUrl[] = "https://example.com/";

// None of this is needed to get a page on screen - the sample implements the
// whole delegate so it doubles as documentation of what libcbe calls back.
class SampleWebView : public webos::WebViewBase {
 public:
  void LoadProgressChanged(double progress) override {
    printf("[cbe] progress %3.0f%%\n", progress * 100);
  }
  void DidFirstFrameFocused() override { puts("[cbe] first frame focused"); }
  void LoadVisuallyCommitted() override { puts("[cbe] visually committed"); }
  void TitleChanged(const std::string& title) override {
    printf("[cbe] title '%s'\n", title.c_str());
  }
  void NavigationHistoryChanged() override {}
  void Close() override { puts("[cbe] close requested"); }
  bool DecidePolicyForResponse(bool, int status, const std::string& url,
                               const std::string&) override {
    printf("[cbe] response %d %s\n", status, url.c_str());
    return false;  // false: let Chromium handle it
  }
  void LoadStarted() override { puts("[cbe] load started"); }
  void LoadStopped() override { puts("[cbe] load stopped"); }
  void DidStartNavigation(const std::string& url, bool) override {
    printf("[cbe] navigate %s\n", url.c_str());
  }
  void DidFinishNavigation(const std::string&, bool) override {}
  void LoadFinished(const std::string& url) override {
    printf("[cbe] finished %s\n", url.c_str());
  }
  void LoadFailed(const std::string& url, int code, const std::string& desc) override {
    printf("[cbe] FAILED %s (%d %s)\n", url.c_str(), code, desc.c_str());
  }
  void LoadAborted(const std::string& url) override {
    printf("[cbe] aborted %s\n", url.c_str());
  }
  void DocumentLoadFinished() override { puts("[cbe] document loaded"); }
  void RenderProcessCreated(int pid) override { printf("[cbe] renderer pid %d\n", pid); }
  void RenderProcessGone() override { puts("[cbe] renderer gone"); }
};

class SampleWindow : public webos::WebAppWindowBase {
 public:
  bool event(WebOSEvent*) override { return false; }
};

SampleWindow* g_window;
SampleWebView* g_webview;
std::string g_app_path;

// Runs on Chromium's browser UI thread, once.
gboolean CreateWebApp(gpointer) {
  g_window = new SampleWindow();
  g_window->InitWindow(1920, 1080);
  // Without this the surface still reaches the screen, but LSM reports it as a
  // card with an empty appId: no lifecycle, no place in the recents list, and
  // nothing for the Home key to come back to.
  g_window->SetWindowProperty("appId", kAppId);
  g_window->SetWindowProperty("title", "CBE WebView");
  g_window->SetWindowHostState(webos::NATIVE_WINDOW_FULLSCREEN);

  g_webview = new SampleWebView();
  g_webview->Initialize(kAppId, g_app_path, "default", "", "", 1920, 1080, false);
  g_webview->SetAppId(kAppId);
  g_webview->SetAppPath(g_app_path);
  g_webview->SetAllowLocalResourceLoad(true);
  g_webview->SetLocalStorageEnabled(true);
  g_webview->SetVisible(true);
  g_webview->UpdatePreferences();

  // The window does not own the web contents, it only composites them.
  g_window->AttachWebContents(g_webview->GetWebContents());
  g_window->Show();
  g_window->Activate();

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
  // SAM points a launched app's stdout at /dev/null, so everything printed below
  // is invisible unless it is redirected somewhere. (libcbe's own logging goes
  // through PmLog and reaches /var/log/messages regardless.)
  if (IsBrowserProcess(argc, argv)) {
    freopen("/tmp/" APP_LOG_NAME ".log", "w", stdout);
    dup2(1, 2);
  }
  setvbuf(stdout, nullptr, _IOLBF, 0);

  const char* slash = strrchr(argv[0], '/');
  g_app_path = slash ? std::string(argv[0], slash - argv[0]) : std::string(".");
  const bool browser = IsBrowserProcess(argc, argv);

  // The launcher starts a native app with a JSON parameter, not with Chromium
  // switches, so the app supplies its own. WAM gets the equivalent list from its
  // systemd unit; these are the few that are not optional.
  std::vector<std::string> args;
  args.push_back(argv[0]);
  if (browser) {
    args.push_back("--ozone-platform=wayland");
    args.push_back("--no-sandbox");
    args.push_back("--no-zygote");       // no forked helper; Chromium re-execs
    args.push_back("--in-process-gpu");  // one less process to get right
    args.push_back(std::string("--browser-subprocess-path=") + argv[0]);
    args.push_back(std::string("--user-data-dir=/tmp/") + kAppId);
  }
  for (int i = 1; i < argc; ++i) args.push_back(argv[i]);

  std::vector<const char*> cargv;
  for (size_t i = 0; i < args.size(); ++i) cargv.push_back(args[i].c_str());

  // Wayland clients need this and the app launcher does not always set it.
  if (!getenv("XDG_RUNTIME_DIR")) setenv("XDG_RUNTIME_DIR", "/tmp/xdg", 1);

  // Chromium re-execs this same binary for the renderer; only the browser
  // process gets a window.
  if (browser) g_idle_add(CreateWebApp, NULL);

  return WebOSMain(static_cast<int>(cargv.size()), cargv.data());
}
