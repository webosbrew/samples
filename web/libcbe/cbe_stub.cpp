// Link-time stand-in for the TV's /usr/lib/libcbe.so.
//
// The NDK has no libcbe and no stub for it, so the build makes one: a shared
// object with the right SONAME and the right symbol names, and nothing else.
// The loader picks up the TV's real 70 MB library at runtime because the SONAME
// matches; this file is never installed.
//
// It is written as ordinary C++ against the same headers the sample uses, rather
// than as a hand-written list of mangled names, so the two cannot drift apart.
// If a declaration changes, the stub changes with it.

#include "webos/webapp_window_base.h"
#include "webos/webview_base.h"

#define STUB(...) \
  { return __VA_ARGS__; }

extern "C" int WebOSMain(int, const char**) STUB(0)

namespace webos {

WebViewBase::WebViewBase() : webview_(0) STUB()
WebViewBase::~WebViewBase() STUB()

void WebViewBase::Initialize(const std::string&, const std::string&, const std::string&,
                             const std::string&, const std::string&, int, int, bool) STUB()
void* WebViewBase::GetWebContents() STUB(0)
void WebViewBase::LoadUrl(const std::string&) STUB()
void WebViewBase::StopLoading() STUB()
void WebViewBase::Reload() STUB()
bool WebViewBase::CanGoBack() const STUB(false)
std::string WebViewBase::GetUrl() STUB(std::string())
std::string WebViewBase::DocumentTitle() const STUB(std::string())
std::string WebViewBase::DefaultUserAgent() const STUB(std::string())
void WebViewBase::RunJavaScript(const std::string&) STUB()
void WebViewBase::RunJavaScriptInAllFrames(const std::string&) STUB()
void WebViewBase::LoadExtension(const std::string&) STUB()
void WebViewBase::ClearExtensions() STUB()
void WebViewBase::AddUserStyleSheet(const std::string&) STUB()
void WebViewBase::ForwardWebOSEvent(WebOSEvent*) STUB()
void WebViewBase::EnableInspectablePage() STUB()
void WebViewBase::UpdatePreferences() STUB()
void WebViewBase::SuspendPaintingAndSetVisibilityHidden() STUB()
void WebViewBase::ResumePaintingAndSetVisibilityVisible() STUB()
void WebViewBase::SuspendWebPageDOM() STUB()
void WebViewBase::ResumeWebPageDOM() STUB()
void WebViewBase::SuspendWebPageMedia() STUB()
void WebViewBase::ResumeWebPageMedia() STUB()
void WebViewBase::SetAppId(const std::string&) STUB()
void WebViewBase::SetAppPath(const std::string&) STUB()
void WebViewBase::SetTrustLevel(const std::string&) STUB()
void WebViewBase::SetUserAgent(const std::string&) STUB()
void WebViewBase::SetVisible(bool) STUB()
void WebViewBase::SetFocus(bool) STUB()
void WebViewBase::SetViewportSize(int, int) STUB()
void WebViewBase::SetHardwareResolution(int, int) STUB()
void WebViewBase::SetTransparentBackground(bool) STUB()
void WebViewBase::SetBackgroundColor(int, int, int, int) STUB()
void WebViewBase::SetAllowLocalResourceLoad(bool) STUB()
void WebViewBase::SetAllowUniversalAccessFromFileUrls(bool) STUB()
void WebViewBase::SetFileAccessBlocked(bool) STUB()
void WebViewBase::SetWebSecurityEnabled(bool) STUB()
void WebViewBase::SetLocalStorageEnabled(bool) STUB()
void WebViewBase::SetJavascriptCanOpenWindows(bool) STUB()
void WebViewBase::SetSupportsMultipleWindows(bool) STUB()
void WebViewBase::SetShouldSuppressDialogs(bool) STUB()
void WebViewBase::SetDisallowScrollingInMainFrame(bool) STUB()
void WebViewBase::SetVisibilityState(WebPageVisibilityState) STUB()
void WebViewBase::SetFontHinting(FontRenderParams) STUB()

WebAppWindowBase::WebAppWindowBase() : webapp_window_(0) STUB()
WebAppWindowBase::~WebAppWindowBase() STUB()
void WebAppWindowBase::InitWindow(int, int) STUB()
void WebAppWindowBase::Show() STUB()
void WebAppWindowBase::Hide() STUB()
void WebAppWindowBase::SetCustomCursor(CustomCursorType, const std::string&, int, int) STUB()
void WebAppWindowBase::AttachWebContents(void*) STUB()
void WebAppWindowBase::DetachWebContents() STUB()
void WebAppWindowBase::RecreatedWebContents() STUB()
void WebAppWindowBase::Activate() STUB()
void WebAppWindowBase::Deactivate() STUB()
void WebAppWindowBase::Resize(int, int) STUB()
void WebAppWindowBase::SetOpacity(float) STUB()
void WebAppWindowBase::SetWindowHostState(NativeWindowState) STUB()
NativeWindowState WebAppWindowBase::GetWindowHostState() const STUB(NATIVE_WINDOW_DEFAULT)
void WebAppWindowBase::SetWindowProperty(const std::string&, const std::string&) STUB()
void WebAppWindowBase::SetUseVirtualKeyboard(bool) STUB()
void WebAppWindowBase::SetKeyMask(WebOSKeyMask, bool) STUB()
int WebAppWindowBase::DisplayWidth() STUB(0)
int WebAppWindowBase::DisplayHeight() STUB(0)

}  // namespace webos
