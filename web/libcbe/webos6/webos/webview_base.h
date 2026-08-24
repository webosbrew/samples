// webos::WebViewBase as it exists on webOS 6 and newer.
//
// A third shape of the same API - see ../../webos/webview_base.h for webOS 4 and
// ../../webos3/webos/webview_base.h for webOS 3. What changed here:
//
//   * the entry point is a class, webos::WebOSMain(delegate)->Run(argc, argv),
//     not the free WebOSMain() of webOS 3 and 4;
//   * WebViewBase takes (bool, int, int) and Initialize takes five strings and a
//     bool, where webOS 4 took five strings, two ints and a bool;
//   * the delegate is 61 slots rather than 24, and its order is different again.
//
// Measured on a 65UP7560 running starfish 6.5.2 (Chromium 79). The slot order
// below is BlinkWebView's vtable inside that set's libWebAppMgr.so - WAM still
// subclasses the webos:: API on this generation, so it remains the reference
// implementation. Slots it fills with local functions are named UnknownNN: they
// must exist and keep their position, but their signatures are not recoverable.
// They are declared void and argument-less deliberately - the callee ignores
// arguments, and on AAPCS that is safe.
//
// Names come from the vtable itself; upstream's neva_app_runtime headers
// (webosose/chromium87, src/neva/app_runtime/public) describe a related but
// *different* interface - 32 virtuals against these 61 - so they are a naming
// reference, not the layout.
#pragma once

#include <string>
#include <vector>

class WebOSEvent;

namespace webos {

class WebViewProfile;

class WebViewDelegate {
 public:
  // slot 0
  virtual void OnLoadProgressChanged(double progress) { (void)progress; }
  // slot 1
  virtual void DidFirstFrameFocused() {}
  // slot 2
  virtual void TitleChanged(const std::string& title) { (void)title; }
  // slot 3
  virtual void NavigationHistoryChanged() {}
  // slot 4
  virtual void Close() {}
  // slot 5
  virtual bool DecidePolicyForResponse(bool is_main_frame, int status_code,
                                       const std::string& url,
                                       const std::string& status_text) { (void)is_main_frame; (void)status_code; (void)url; (void)status_text; return false; }
  // slot 6
  virtual bool AcceptsVideoCapture() { return false; }
  // slot 7
  virtual bool AcceptsAudioCapture() { return false; }
  // slot 8
  virtual void LoadStarted() {}
  // slot 9
  virtual void LoadFinished(const std::string& url) { (void)url; }
  // slot 10
  virtual void LoadFailed(const std::string& url, int err_code,
                          const std::string& err_desc) { (void)url; (void)err_code; (void)err_desc; }
  // slot 11
  virtual void LoadAborted(const std::string& url) { (void)url; }
  // slot 12 - name not recoverable; WAM implements it with a local function.
  virtual void Unknown12() {}
  // slot 13
  virtual void RenderProcessCreated(int pid) { (void)pid; }
  // slot 14
  virtual void RenderProcessGone(bool crashed) { (void)crashed; }
  // slot 15
  virtual void DocumentLoadFinished() {}
  // slot 16
  virtual void DidStartNavigation(const std::string& url, bool is_main_frame) { (void)url; (void)is_main_frame; }
  // slot 17
  virtual void DidFinishNavigation(const std::string& url, bool is_main_frame) { (void)url; (void)is_main_frame; }
  // slot 18 - name not recoverable; WAM implements it with a local function.
  virtual void Unknown18() {}
  // slot 19
  virtual void DidClearWindowObject() {}
  // slot 20
  virtual void DidSwapCompositorFrame() {}
  // slot 21
  virtual bool AllowMouseOnOffEvent() const { return false; }
  // slot 22 - name not recoverable; WAM implements it with a local function.
  virtual void Unknown22() {}
  // slot 23
  virtual void DidLoadingEnd() {}
  // slot 24 - name not recoverable; WAM implements it with a local function.
  virtual void Unknown24() {}
  // slot 25 - name not recoverable; WAM implements it with a local function.
  virtual void Unknown25() {}
  // slot 26 - name not recoverable; WAM implements it with a local function.
  virtual void Unknown26() {}
  // slot 27
  virtual void DidFirstMeaningfulPaint() {}
  // slot 28
  virtual void DidNonFirstMeaningfulPaint() {}
  // slot 29
  virtual void ErrorPageStateChanged(bool enable) { (void)enable; }
  // slot 30 - name not recoverable; WAM implements it with a local function.
  virtual void Unknown30() {}
  // slot 31
  virtual void SkipBeginMainFrameAck() {}
  // slot 32
  virtual void CreatePlugin() {}
  // slot 33 - name not recoverable; WAM implements it with a local function.
  virtual void Unknown33() {}
  // slot 34
  virtual void MediaAboutToPlayNotify(const std::string& id, bool audio) { (void)id; (void)audio; }
  // slot 35 - name not recoverable; WAM implements it with a local function.
  virtual void Unknown35() {}
  // slot 36 - name not recoverable; WAM implements it with a local function.
  virtual void Unknown36() {}
  // slot 37
  virtual void RequestLaunchFullBrowser(const std::string& url) { (void)url; }
  // slot 38
  virtual bool CanDownload(std::string& url) { (void)url; return false; }
  // slot 39 - name not recoverable; WAM implements it with a local function.
  virtual void Unknown39() {}
  // slot 40
  virtual void LoadStopped(const std::string& url) { (void)url; }
  // slot 41 - name not recoverable; WAM implements it with a local function.
  virtual void Unknown41() {}
  // slot 42 - name not recoverable; WAM implements it with a local function.
  virtual void Unknown42() {}
  // slot 43 - name not recoverable; WAM implements it with a local function.
  virtual void Unknown43() {}
  // slot 44
  virtual void NotifyFault(const std::string& a, int b, const std::string& c) { (void)a; (void)b; (void)c; }
  // slot 45
  virtual void HandleKeyboardEvent(int key) { (void)key; }
  // slot 46
  virtual void RequestMediaLayer(const std::string& id, unsigned type) { (void)id; (void)type; }
  // slot 47 - name not recoverable; WAM implements it with a local function.
  virtual void Unknown47() {}
  // slot 48 - name not recoverable; WAM implements it with a local function.
  virtual void Unknown48() {}
  // slot 49
  virtual void SetMediaProperty(const std::string& a, const std::string& b,
                                const std::string& c) { (void)a; (void)b; (void)c; }
  // slot 50
  virtual void DestroyMediaLayer(const std::string& id) { (void)id; }
  // slot 51 - name not recoverable; WAM implements it with a local function.
  virtual void Unknown51() {}
  // slot 52 - name not recoverable; WAM implements it with a local function.
  virtual void Unknown52() {}
  // slot 53 is GetWebContents(), and it is virtual - libcbe calls it through the
  // vtable during Initialize(). Declaring it void, as a placeholder, hands
  // libcbe whatever was in r0 as a WebContents pointer and the process dies
  // inside Initialize. It is pure here and overridden in WebViewBase below with
  // no body, so the slot resolves to libcbe's own implementation at link time.
  virtual void* GetWebContents() = 0;
  // slot 54
  virtual void HandleBrowserControlCommand(const std::string& command,
                                          const std::vector<std::string>& args) { (void)command; (void)args; }
  // slot 55
  virtual void HandleBrowserControlFunction(const std::string& command,
                                           const std::vector<std::string>& args,
                                           std::string* result) { (void)command; (void)args; (void)result; }
  // slot 56
  virtual void LoadVisuallyCommitted() {}
  // slot 57 - name not recoverable; WAM implements it with a local function.
  virtual void Unknown57() {}
  // slot 58 - name not recoverable; WAM implements it with a local function.
  virtual void Unknown58() {}
  // slot 59 - name not recoverable; WAM implements it with a local function.
  virtual void Unknown59() {}
  // slot 60
  virtual void DidDropAllPeerConnections(int reason) { (void)reason; }
};

class WebViewBase : public WebViewDelegate {
 public:
  enum WebViewMode { WEBVIEW_MODE_NORMAL };
  enum WebPageVisibilityState { VISIBILITY_VISIBLE, VISIBILITY_HIDDEN, VISIBILITY_LAUNCHING };

  WebViewBase(bool alt_storage, int width, int height);
  ~WebViewBase();

  // Two overloads exist on this generation. WAM calls this one - the long form,
  // carrying width/height, a WebViewMode and two bools - at every one of its
  // three call sites, and the short (five strings + bool) form segfaults inside
  // libcbe when called from here. Use this one.
  void Initialize(const std::string& app_id,
                  const std::string& app_path,
                  const std::string& trust_level,
                  const std::string& v8_snapshot_path,
                  const std::string& v8_extra_flags,
                  int width,
                  int height,
                  bool use_native_scroll,
                  WebViewMode mode,
                  bool inspectable);

  void* GetWebContents() override;  // implemented by libcbe, not by us
  void LoadUrl(const std::string& url);
  void StopLoading();
  void Reload();
  std::string GetUrl();
  void RunJavaScript(const std::string& js);
  void UpdatePreferences();

  void SetAppId(const std::string& app_id);
  void SetTrustLevel(const std::string& trust_level);
  void SetUserAgent(const std::string& user_agent);
  void SetVisible(bool visible);
  void SetAllowLocalResourceLoad(bool allow);
  void SetAllowUniversalAccessFromFileUrls(bool allow);
  void SetFileAccessBlocked(bool blocked);
  void SetLocalStorageEnabled(bool enabled);
  void SetVisibilityState(WebPageVisibilityState state);

 private:
  // 92 bytes total, and the size is load-bearing: libcbe's constructor writes as
  // far as offset 90, so a subclass declared any smaller corrupts the heap and
  // the process dies later inside malloc, nowhere near the cause. The layout is
  // opaque; only the size matters here. (webOS 4's WebViewBase was 8 bytes,
  // which is why this needed measuring rather than assuming.)
  void* webview_;      // WebView*, owned by libcbe
  char reserved_[84];  // to sizeof == 92
};

// The entry point on this generation. Run() is Chromium's content main: it takes
// the process over and does not return.
class WebOSMainDelegate {
 public:
  // One slot, per WAM's WebOSMainDelegateWAM vtable.
  virtual void AboutToCreateContentBrowserClient() = 0;
};

class WebOSMain {
 public:
  explicit WebOSMain(WebOSMainDelegate* delegate);
  int Run(int argc, const char** argv);

 private:
  void* main_;
};

}  // namespace webos
