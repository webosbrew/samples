// webos::WebViewBase - the Chromium embedding surface exported by the TV's
// /usr/lib/libcbe.so ("Chromium Browser Engine").
//
// This is not an SDK header. There is none: libcbe ships as a stripped 70 MB
// blob with a C++ ABI and no headers anywhere on the device or in the NDK. What
// is here was reconstructed from two sources that agree with each other - the
// firmware symbol tables, which give every name and signature, and the vtables
// of WAM's own BlinkWebView, which is the only in-firmware subclass of this
// class and therefore pins down the slot order.
//
// Three things are load-bearing and must not be tidied up:
//
//   * WebViewDelegate has NO virtual destructor. Adding one shifts every slot
//     by two and libcbe will call the wrong function.
//   * The delegate is 24 slots long. Only the first 17 have recoverable names;
//     the rest are no-ops in WAM too, but they must exist, because libcbe
//     indexes past slot 17 and a short vtable reads whatever follows it in
//     memory. Leaving them out is a segfault a few hundred milliseconds into
//     the first page load, which is exactly how they were found.
//   * WebViewBase adds no virtuals and exactly one pointer member, so the whole
//     object is 8 bytes - the size libcbe's own constructor assumes.
#pragma once

#include <string>
#include <vector>

class WebOSEvent;

namespace webos {

class WebViewProfile;

class WebViewDelegate {
 public:
  virtual void LoadProgressChanged(double progress) = 0;
  virtual void DidFirstFrameFocused() = 0;
  virtual void LoadVisuallyCommitted() = 0;
  virtual void TitleChanged(const std::string& title) = 0;
  virtual void NavigationHistoryChanged() = 0;
  virtual void Close() = 0;
  virtual bool DecidePolicyForResponse(bool is_main_frame,
                                       int status_code,
                                       const std::string& url,
                                       const std::string& status_text) = 0;
  virtual void LoadStarted() = 0;
  virtual void LoadStopped() = 0;
  virtual void DidStartNavigation(const std::string& url, bool is_main_frame) = 0;
  virtual void DidFinishNavigation(const std::string& url, bool is_main_frame) = 0;
  virtual void LoadFinished(const std::string& url) = 0;
  virtual void LoadFailed(const std::string& url,
                          int err_code,
                          const std::string& err_desc) = 0;
  virtual void LoadAborted(const std::string& url) = 0;
  virtual void DocumentLoadFinished() = 0;
  virtual void RenderProcessCreated(int pid) = 0;
  virtual void RenderProcessGone() = 0;

  // Slots 17 to 23. Two of them are named in WAM's binary - the browser-control
  // bridge behind window.PalmSystem - and the rest WAM overrides with empty
  // bodies, which is all the sample needs them to be. They are declared void and
  // argument-less on purpose: the callee never touches the arguments, and on
  // AAPCS ignoring them is safe. Do not add or remove entries.
  virtual void HandleUnknown17() {}
  virtual void HandleUnknown18() {}
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
  virtual void HandleUnknown21() {}
  virtual void HandleUnknown22() {}
  virtual void HandleUnknown23() {}
};

class WebViewBase : public WebViewDelegate {
 public:
  enum FontRenderParams { HINTING_NONE, HINTING_SLIGHT, HINTING_MEDIUM, HINTING_FULL };
  enum MemoryPressureLevel {
    MEMORY_PRESSURE_NONE,
    MEMORY_PRESSURE_LOW,
    MEMORY_PRESSURE_CRITICAL
  };
  enum WebPageVisibilityState { VISIBILITY_VISIBLE, VISIBILITY_HIDDEN, VISIBILITY_LAUNCHING };

  WebViewBase();
  ~WebViewBase();

  // The last three arguments are the appinfo.json "width", "height" and
  // "useNativeScroll" keys - that is literally where WAM reads them from.
  void Initialize(const std::string& app_id,
                  const std::string& app_path,
                  const std::string& trust_level,
                  const std::string& v8_snapshot_path,
                  const std::string& v8_extra_flags,
                  int width,
                  int height,
                  bool use_native_scroll);

  void* GetWebContents();
  void LoadUrl(const std::string& url);
  void StopLoading();
  void Reload();
  bool CanGoBack() const;
  std::string GetUrl();
  std::string DocumentTitle() const;
  std::string DefaultUserAgent() const;
  void RunJavaScript(const std::string& js);
  void RunJavaScriptInAllFrames(const std::string& js);
  // Loads one of libcbe's built-in V8 injections by name ("v8/palmsystem",
  // "v8/netcast", ...). An injection is what gives page JavaScript something to
  // call that reaches native code: its native functions land in the browser
  // process as HandleBrowserControlCommand / HandleBrowserControlFunction.
  void LoadExtension(const std::string& name);
  void ClearExtensions();
  void AddUserStyleSheet(const std::string& css);
  void ForwardWebOSEvent(WebOSEvent* event);
  void EnableInspectablePage();
  void UpdatePreferences();
  void SuspendPaintingAndSetVisibilityHidden();
  void ResumePaintingAndSetVisibilityVisible();
  void SuspendWebPageDOM();
  void ResumeWebPageDOM();
  void SuspendWebPageMedia();
  void ResumeWebPageMedia();

  void SetAppId(const std::string& app_id);
  void SetAppPath(const std::string& app_path);
  void SetTrustLevel(const std::string& trust_level);
  void SetUserAgent(const std::string& user_agent);
  void SetVisible(bool visible);
  void SetFocus(bool focus);
  void SetViewportSize(int width, int height);
  void SetHardwareResolution(int width, int height);
  void SetTransparentBackground(bool transparent);
  void SetBackgroundColor(int r, int g, int b, int a);
  void SetAllowLocalResourceLoad(bool allow);
  void SetAllowUniversalAccessFromFileUrls(bool allow);
  void SetFileAccessBlocked(bool blocked);
  void SetWebSecurityEnabled(bool enabled);
  void SetLocalStorageEnabled(bool enabled);
  void SetJavascriptCanOpenWindows(bool allow);
  void SetSupportsMultipleWindows(bool support);
  void SetShouldSuppressDialogs(bool suppress);
  void SetDisallowScrollingInMainFrame(bool disallow);
  void SetVisibilityState(WebPageVisibilityState state);
  void SetFontHinting(FontRenderParams hinting);

 private:
  void* webview_;  // WebView*, owned by libcbe
};

}  // namespace webos
