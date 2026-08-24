// Embedding the TV's Chromium on webOS 6 and newer.
//
// Same shape as web/cbe: hand the process to Chromium, get called back on its
// browser UI thread through the default GMainContext, build a window and a web
// view. Everything that differs is the API moving between generations - see
// ../libcbe/webos6/webos/webview_base.h for the full list, but the three that
// change the code are:
//
//   * the entry point is a class - webos::WebOSMain(delegate).Run(argc, argv) -
//     where webOS 3 and 4 had a free WebOSMain() function;
//   * WebViewBase takes (bool, int, int) and Initialize takes five strings and
//     a bool, dropping the width/height pair webOS 4 passed there;
//   * the delegate is 61 slots instead of 24.
//
// One binary covers webOS 6.4 through 11.2: the API stops moving after 6.4.

#include <glib.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

#include "webos/webapp_window_base.h"
#include "webos/webview_base.h"

namespace {

const char kAppId[] = "org.webosbrew.sample.web.cbe6";
const char kUrl[] = "https://example.com/";

class SampleWebView : public webos::WebViewBase {
 public:
  SampleWebView(int w, int h) : webos::WebViewBase(false, w, h) {}

  void OnLoadProgressChanged(double progress) override {
    printf("[cbe] progress %3.0f%%\n", progress * 100);
  }
  void TitleChanged(const std::string& title) override {
    printf("[cbe] title '%s'\n", title.c_str());
  }
  void LoadStarted() override { puts("[cbe] load started"); }
  void LoadFinished(const std::string& url) override {
    printf("[cbe] finished %s\n", url.c_str());
  }
  void LoadFailed(const std::string& url, int code, const std::string& desc) override {
    printf("[cbe] FAILED %s (%d %s)\n", url.c_str(), code, desc.c_str());
  }
  void DocumentLoadFinished() override { puts("[cbe] document loaded"); }
  void LoadVisuallyCommitted() override { puts("[cbe] visually committed"); }
  void DidFirstMeaningfulPaint() override { puts("[cbe] first meaningful paint"); }
  void RenderProcessCreated(int pid) override { printf("[cbe] renderer pid %d\n", pid); }
  void RenderProcessGone(bool crashed) override {
    printf("[cbe] renderer gone (crashed=%d)\n", (int)crashed);
  }
  void DidStartNavigation(const std::string& url, bool) override {
    printf("[cbe] navigate %s\n", url.c_str());
  }
};

class SampleWindow : public webos::WebAppWindowBase {
 public:
  bool event(WebOSEvent*) override { return false; }
};

SampleWindow* g_window;
SampleWebView* g_webview;
std::string g_app_path;

gboolean CreateWebApp(gpointer) {
  g_window = new SampleWindow();
  g_window->InitWindow(1920, 1080);
  g_window->SetWindowProperty("appId", kAppId);
  g_window->SetWindowHostState(webos::NATIVE_WINDOW_FULLSCREEN);

  g_webview = new SampleWebView(1920, 1080);
  // WAM passes the same register for all five trailing arguments, so they are
  // zeros there - width and height included. The size comes from the
  // constructor on this generation, not from here.
  g_webview->Initialize(kAppId, g_app_path, "default", "", "", 0, 0, false,
                        webos::WebViewBase::WEBVIEW_MODE_NORMAL, false);
  g_webview->SetAppId(kAppId);
  g_webview->SetAllowLocalResourceLoad(true);
  g_webview->SetLocalStorageEnabled(true);
  g_webview->SetVisible(true);
  g_webview->UpdatePreferences();

  g_window->AttachWebContents(g_webview->GetWebContents());
  g_window->Show();
  g_window->Activate();

  printf("[cbe] loading %s\n", kUrl);
  g_webview->LoadUrl(kUrl);
  return G_SOURCE_REMOVE;
}

// WebOSMain wants a delegate. WAM's has a single slot; nothing here needs it.
class SampleMainDelegate : public webos::WebOSMainDelegate {
 public:
  void AboutToCreateContentBrowserClient() override {}
};

bool IsBrowserProcess(int argc, char** argv) {
  for (int i = 1; i < argc; ++i)
    if (strncmp(argv[i], "--type=", 7) == 0) return false;
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  const bool browser = IsBrowserProcess(argc, argv);
  // SAM points a launched app's stdout at /dev/null.
  if (browser) {
    freopen("/tmp/" APP_LOG_NAME ".log", "w", stdout);
    dup2(1, 2);
  }
  setvbuf(stdout, NULL, _IOLBF, 0);

  const char* slash = strrchr(argv[0], '/');
  g_app_path = slash ? std::string(argv[0], slash - argv[0]) : std::string(".");

  std::vector<std::string> args;
  args.push_back(argv[0]);
  if (browser) {
    args.push_back("--ozone-platform=wayland");
    args.push_back("--no-sandbox");
    args.push_back("--no-zygote");
    args.push_back("--in-process-gpu");
    args.push_back(std::string("--browser-subprocess-path=") + argv[0]);
    args.push_back(std::string("--user-data-dir=/tmp/") + kAppId);
    args.push_back("--enable-logging=stderr");
  }
  for (int i = 1; i < argc; ++i) args.push_back(argv[i]);

  std::vector<const char*> cargv;
  for (size_t i = 0; i < args.size(); ++i) cargv.push_back(args[i].c_str());

  if (!getenv("XDG_RUNTIME_DIR")) setenv("XDG_RUNTIME_DIR", "/tmp/xdg", 1);
  if (browser) g_idle_add(CreateWebApp, NULL);

  static SampleMainDelegate delegate;
  webos::WebOSMain main_runner(&delegate);
  return main_runner.Run(static_cast<int>(cargv.size()), cargv.data());
}
