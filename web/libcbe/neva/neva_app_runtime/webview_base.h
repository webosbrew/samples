// neva_app_runtime - the API LG introduced alongside webos:: on webOS 6.
//
// Unlike every other header under web/libcbe, this one starts from *published*
// source: webosose/chromium87, src/neva/app_runtime/public/webview_delegate.h.
// It is still not a drop-in, because the TV is Chromium 79 - between the public
// chromium68 and chromium87 trees - and its vtable has 40 slots where
// chromium87 declares 32 virtuals and chromium68 declares 26.
//
// What makes the upstream order trustworthy anyway is a cross-check that does
// not depend on names at all. libcbe's own vtable says which slots are pure
// virtual (they relocate to __cxa_pure_virtual) and which carry a default
// implementation, and that pattern is:
//
//   firmware  pure at: 0-15, 18, 19, 20, 21, 29
//   upstream  pure at: 0-15, 18, 19, 20, 21
//
// Identical through slot 28. So slots 0-28 are upstream's, in upstream's order,
// and 29-39 are LG additions this header leaves as placeholders.
//
// Measured against a 65UP7560 running starfish 6.5.2.
#pragma once

#include <string>
#include <vector>

namespace neva_app_runtime {

class WebViewProfile;

enum DropPeerConnectionReason {
  DROP_PEER_CONNECTION_REASON_UNKNOWN,
  DROP_PEER_CONNECTION_REASON_PAGE_HIDDEN,
  DROP_PEER_CONNECTION_REASON_MULTIMEDIA_PLAYING,
};

class WebViewDelegate {
 public:
  // slot 0
  virtual void OnLoadProgressChanged(double progress) {}
  // slot 1
  virtual void DidFirstFrameFocused() {}
  // slot 2
  virtual void TitleChanged(const std::string& title) {}
  // slot 3
  virtual void NavigationHistoryChanged() {}
  // slot 4
  virtual void Close() {}
  // slot 5
  virtual bool DecidePolicyForResponse(bool is_main_frame, int status_code, const std::string& url, const std::string& status_text) { return false; }
  // slot 6
  virtual bool AcceptsVideoCapture() { return false; }
  // slot 7
  virtual bool AcceptsAudioCapture() { return false; }
  // slot 8
  virtual void LoadStarted() {}
  // slot 9
  virtual void LoadFinished(const std::string& url) {}
  // slot 10
  virtual void LoadFailed(const std::string& url, int error_code, const std::string& error_description) {}
  // slot 11
  virtual void LoadAborted(const std::string& url) {}
  // slot 12
  virtual void LoadStopped() {}
  // slot 13
  virtual void RenderProcessCreated(int pid) {}
  // slot 14
  virtual void RenderProcessGone() {}
  // slot 15
  virtual void DocumentLoadFinished() {}
  // slot 16
  virtual void DidStartNavigation(const std::string& url, bool is_main_frame) {}
  // slot 17
  virtual void DidFinishNavigation(const std::string& url, bool is_main_frame) {}
  // slot 18
  virtual void DidHistoryBackOnTopPage() {}
  // slot 19
  virtual void DidClearWindowObject() {}
  // slot 20
  virtual void DidSwapCompositorFrame() {}
  // slot 21
  virtual void DidErrorPageLoadedFromNetErrorHelper() {}
  // slot 22
  virtual void DidLoadingEnd() {}
  // slot 23
  virtual void DidFirstPaint() {}
  // slot 24
  virtual void DidFirstContentfulPaint() {}
  // slot 25
  virtual void DidFirstImagePaint() {}
  // slot 26
  virtual void DidFirstMeaningfulPaint() {}
  // slot 27
  virtual void DidNonFirstMeaningfulPaint() {}
  // slot 28
  virtual void DidLargestContentfulPaint() {}
  // slot 29
  virtual void DidDropAllPeerConnections(neva_app_runtime::DropPeerConnectionReason reason) {}
  // slot 30
  virtual void DidResumeDOM() {}
  // slot 31
  virtual void SendCookiesForHostname(const std::string& cookies) {}
  // slot 32 - beyond what upstream declares; LG additions.
  virtual void Unknown32() {}
  // slot 33 - beyond what upstream declares; LG additions.
  virtual void Unknown33() {}
  // slot 34 - beyond what upstream declares; LG additions.
  virtual void Unknown34() {}
  // slot 35 - beyond what upstream declares; LG additions.
  virtual void Unknown35() {}
  // slot 36 - beyond what upstream declares; LG additions.
  virtual void Unknown36() {}
  // slot 37 - beyond what upstream declares; LG additions.
  virtual void Unknown37() {}
  // slot 38 - beyond what upstream declares; LG additions.
  virtual void Unknown38() {}
  // slot 39 - beyond what upstream declares; LG additions.
  virtual void Unknown39() {}
};

class WebViewBase : public WebViewDelegate {
 public:
  // No Initialize() on this API - the constructor takes the size, and a null
  // profile means the default one.
  WebViewBase(int width, int height, WebViewProfile* profile);
  ~WebViewBase();

  // Virtual, and libcbe implements it - declared with no body here so the slot
  // resolves to libcbe's own symbol rather than being shadowed. Stubbing a slot
  // that returns a pointer is what breaks these bindings; see
  // ../../../cbe-webos6/README.md.
  void* GetWebContents();
  void LoadUrl(const std::string& url);
  void SetAppId(const std::string& app_id);
  void SetVisible(bool visible);

 private:
  // libcbe's constructor writes as far as offset 12, so 16 bytes.
  void* webview_;
  char reserved_[8];
};

}  // namespace neva_app_runtime

// The entry point. A free function on this API, where webos:: has a WebOSMain
// class - and Chromium's content main either way: it does not return.
//
// Deliberately NOT extern "C": libcbe exports it C++-mangled, as
// _Z14AppRuntimeMainiPPKc, so an extern "C" declaration looks for a plain
// "AppRuntimeMain" that does not exist.
int AppRuntimeMain(int argc, const char** argv);
