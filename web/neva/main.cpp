// The same web view again, through neva_app_runtime instead of webos::.
//
// Worth having beside web/cbe-webos6 because the two APIs live in the same
// library on the same TV, and this is the one whose headers LG published. The
// differences are all simplifications:
//
//   * AppRuntimeMain(argc, argv) is a free function, where webos:: has a
//     WebOSMain class taking a delegate;
//   * there is no Initialize() - WebViewBase(width, height, profile) does it;
//   * the delegate is 40 slots against webos::'s 61, and 0-28 of them are
//     upstream's own, in upstream's order.
//
// Verified on a 65UP7560 running starfish 6.5.2.

#include <glib.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

#include "neva_app_runtime/webapp_window_base.h"
#include "neva_app_runtime/webview_base.h"

namespace {

const char kAppId[] = "org.webosbrew.sample.web.neva";
const char kUrl[] = "https://example.com/";

class SampleWebView : public neva_app_runtime::WebViewBase {
 public:
  SampleWebView(int w, int h) : neva_app_runtime::WebViewBase(w, h, NULL) {}

  void OnLoadProgressChanged(double progress) override {
    printf("[neva] progress %3.0f%%\n", progress * 100);
  }
  void TitleChanged(const std::string& title) override {
    printf("[neva] title '%s'\n", title.c_str());
  }
  void LoadStarted() override { puts("[neva] load started"); }
  void LoadFinished(const std::string& url) override {
    printf("[neva] finished %s\n", url.c_str());
  }
  void LoadFailed(const std::string& url, int code, const std::string& desc) override {
    printf("[neva] FAILED %s (%d %s)\n", url.c_str(), code, desc.c_str());
  }
  void DocumentLoadFinished() override { puts("[neva] document loaded"); }
  void DidFirstMeaningfulPaint() override { puts("[neva] first meaningful paint"); }
  void RenderProcessCreated(int pid) override { printf("[neva] renderer pid %d\n", pid); }
};

class SampleWindow : public neva_app_runtime::WebAppWindowBase {};

SampleWindow* g_window;
SampleWebView* g_webview;

gboolean CreateWebApp(gpointer) {
  g_window = new SampleWindow();
  g_window->Resize(1920, 1080);
  g_window->SetWindowProperty("appId", kAppId);
  g_window->SetWindowHostState(neva_app_runtime::FULLSCREEN);

  g_webview = new SampleWebView(1920, 1080);
  g_webview->SetAppId(kAppId);
  g_webview->SetVisible(true);

  g_window->AttachWebContents(g_webview->GetWebContents());
  g_window->Show();
  g_window->Activate();

  printf("[neva] loading %s\n", kUrl);
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
  const bool browser = IsBrowserProcess(argc, argv);
  if (browser) {
    freopen("/tmp/" APP_LOG_NAME ".log", "w", stdout);
    dup2(1, 2);
  }
  setvbuf(stdout, NULL, _IOLBF, 0);

  std::vector<std::string> args;
  args.push_back(argv[0]);
  if (browser) {
    args.push_back("--ozone-platform=wayland");
    args.push_back("--no-sandbox");
    args.push_back("--no-zygote");
    args.push_back("--in-process-gpu");
    args.push_back(std::string("--browser-subprocess-path=") + argv[0]);
    args.push_back(std::string("--user-data-dir=/tmp/") + kAppId);
  }
  for (int i = 1; i < argc; ++i) args.push_back(argv[i]);

  std::vector<const char*> cargv;
  for (size_t i = 0; i < args.size(); ++i) cargv.push_back(args[i].c_str());

  if (!getenv("XDG_RUNTIME_DIR")) setenv("XDG_RUNTIME_DIR", "/tmp/xdg", 1);
  if (browser) g_idle_add(CreateWebApp, NULL);

  return AppRuntimeMain(static_cast<int>(cargv.size()), cargv.data());
}
