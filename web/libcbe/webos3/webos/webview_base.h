// webos::WebViewBase as it exists on webOS 3 - a different API from the one in
// ../../webos/webview_base.h, not just a different string ABI.
//
// Recovered the same way: the firmware symbol tables for the names, and the
// vtable of BlinkWebView inside webOS 3's own libWebAppMgr.so for the slot
// order. Measured against a 43UH6100 running starfish 3.4.0.
//
// What moved between webOS 3 and 4:
//
//   * the whole library is the pre-C++11 std::string ABI, so everything here
//     must be compiled -D_GLIBCXX_USE_CXX11_ABI=0;
//   * there is no Initialize() at all - the constructor takes the dimensions;
//   * DidFirstNonBlankPaint exists and DidStartNavigation does not, so a
//     redirect has to be caught in LoadStarted, which carries the URL here;
//   * LoadStarted and LoadStopped take a URL, and LoadProgressChanged takes one
//     besides the progress;
//   * the browser-control pair sits at 18/19 rather than 19/20.
#pragma once

#include <string>
#include <vector>

class WebOSEvent;

namespace webos {

class WebViewProfile;

}  // namespace webos

// Chromium's own path type, exported by libcbe. Layout is a single std::string,
// which is what makes it safe to declare here.
namespace base {
class FilePath {
 public:
  explicit FilePath(const std::string& path);
  ~FilePath();

 private:
  std::string path_;
};
}  // namespace base

namespace webos {

// Recovered from the exported vtable of webos::PlatformDelegate: two destructor
// slots followed by nine pure virtuals, all of them __cxa_pure_virtual in the
// base. The names and signatures are unknown - nothing in the firmware
// implements this class - so these are placeholders with the right *shape*.
// Enough to hand Runtime::Initialize something it will accept.
class PlatformDelegate {
 public:
  virtual ~PlatformDelegate() {}
  virtual void Unknown2() {}
  virtual void Unknown3() {}
  virtual void Unknown4() {}
  virtual void Unknown5() {}
  virtual void Unknown6() {}
  virtual void Unknown7() {}
  virtual void Unknown8() {}
  virtual void Unknown9() {}
  virtual void Unknown10() {}
};

// A singleton libcbe keeps for platform-wide state. Unlike webos::Platform,
// which belongs to the browser application, this one may exist in a plain
// embedder - and the window size lives here too.
class Runtime {
 public:
  static Runtime* Get();
  void SetWindowSize(int width, int height);
  // Suspected to be what builds webos::Platform - the layer that owns the Luna
  // side, and which is null in a plain embedder.
  void InitializePlatform(const base::FilePath& path);
  void Initialize(PlatformDelegate* delegate);
};


// 24 slots. Names for 0-14 and 18-19 come from BlinkWebView's vtable; the rest
// are stubs it fills with empty bodies, and must be present or libcbe indexes
// past the end of ours.
class WebViewDelegate {
 public:
  virtual void LoadProgressChanged(double progress, const std::string& url) = 0;
  virtual void DidFirstFrameFocused() = 0;
  virtual void DidFirstNonBlankPaint() = 0;
  virtual void LoadVisuallyCommitted() = 0;
  virtual void TitleChanged(const std::string& title) = 0;
  virtual void NavigationHistoryChanged() = 0;
  virtual void Close() = 0;
  virtual bool DecidePolicyForResponse(bool is_main_frame,
                                       int status_code,
                                       const std::string& url,
                                       const std::string& status_text) = 0;
  // Slot 8. The nearest thing webOS 3 has to DidStartNavigation, and the hook a
  // redirect-catching login flow has to use here.
  virtual void LoadStarted(const std::string& url) = 0;
  virtual void LoadFinished(const std::string& url) = 0;
  virtual void LoadFailed(const std::string& url,
                          int err_code,
                          const std::string& err_desc) = 0;
  virtual void LoadStopped(const std::string& url) = 0;
  virtual void RenderProcessCreated(int pid) = 0;
  virtual void RenderProcessGone() = 0;
  virtual void DocumentLoadFinished() = 0;
  virtual void Unknown15() {}
  virtual void Unknown16() {}
  virtual void Unknown17() {}
  virtual void HandleBrowserControlCommand(
      const std::string& command, const std::vector<std::string>& arguments) {
    (void)command;
    (void)arguments;
  }
  virtual void HandleBrowserControlFunction(
      const std::string& command,
      const std::vector<std::string>& arguments,
      std::string* result) {
    (void)command;
    (void)arguments;
    (void)result;
  }
  virtual void Unknown20() {}
  virtual void Unknown21() {}
  virtual void Unknown22() {}
  virtual void Unknown23() {}
};

class WebViewBase : public WebViewDelegate {
 public:
  enum FontRenderParams { HINTING_NONE, HINTING_SLIGHT, HINTING_MEDIUM, HINTING_FULL };
  enum WebPageVisibilityState { VISIBILITY_VISIBLE, VISIBILITY_HIDDEN, VISIBILITY_LAUNCHING };

  // No Initialize() on this generation: the size goes in here.
  WebViewBase(int width, int height);
  ~WebViewBase();

  void* GetWebContents();
  void LoadUrl(const std::string& url);
  void StopLoading();
  void Reload();
  std::string GetUrl();
  std::string DocumentTitle() const;
  void RunJavaScript(const std::string& js);
  void LoadExtension(const std::string& name);
  void ClearExtensions();

  void SetAppId(const std::string& app_id);
  void SetUserAgent(const std::string& user_agent);
  void SetVisible(bool visible);
  void SetViewportSize(int width, int height);
  void SetTransparentBackground(bool transparent);
  void SetAllowLocalResourceLoad(bool allow);
  void SetAllowUniversalAccessFromFileUrls(bool allow);
  void SetFileAccessBlocked(bool blocked);
  void SetLocalStorageEnabled(bool enabled);
  void SetShouldSuppressDialogs(bool suppress);
  void SetVisibilityState(WebPageVisibilityState state);

 private:
  void* webview_;  // WebView*, owned by libcbe
};

}  // namespace webos
