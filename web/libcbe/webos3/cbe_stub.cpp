// Link-time stand-in for webOS 3's /usr/lib/libcbe.so. Same idea as the webOS 4
// stub next door, compiled against the webOS 3 headers and with the pre-C++11
// std::string ABI, so the mangled names come out matching that generation.
#include "webos/webapp_window_base.h"
#include "webos/webview_base.h"

#define STUB(...) \
  { return __VA_ARGS__; }

extern "C" int WebOSMain(int, const char**) STUB(0)

namespace base {
FilePath::FilePath(const std::string& p) : path_(p) STUB()
FilePath::~FilePath() STUB()
}  // namespace base

namespace webos {

Runtime* Runtime::Get() STUB(0)
void Runtime::InitializePlatform(const base::FilePath&) STUB()
void Runtime::Initialize(PlatformDelegate*) STUB()
void Runtime::SetWindowSize(int, int) STUB()

WebViewBase::WebViewBase(int, int) : webview_(0) STUB()
WebViewBase::~WebViewBase() STUB()

void* WebViewBase::GetWebContents() STUB(0)
void WebViewBase::LoadUrl(const std::string&) STUB()
void WebViewBase::StopLoading() STUB()
void WebViewBase::Reload() STUB()
std::string WebViewBase::GetUrl() STUB(std::string())
std::string WebViewBase::DocumentTitle() const STUB(std::string())
void WebViewBase::RunJavaScript(const std::string&) STUB()
void WebViewBase::LoadExtension(const std::string&) STUB()
void WebViewBase::ClearExtensions() STUB()
void WebViewBase::SetAppId(const std::string&) STUB()
void WebViewBase::SetUserAgent(const std::string&) STUB()
void WebViewBase::SetVisible(bool) STUB()
void WebViewBase::SetViewportSize(int, int) STUB()
void WebViewBase::SetTransparentBackground(bool) STUB()
void WebViewBase::SetAllowLocalResourceLoad(bool) STUB()
void WebViewBase::SetAllowUniversalAccessFromFileUrls(bool) STUB()
void WebViewBase::SetFileAccessBlocked(bool) STUB()
void WebViewBase::SetLocalStorageEnabled(bool) STUB()
void WebViewBase::SetShouldSuppressDialogs(bool) STUB()
void WebViewBase::SetVisibilityState(WebPageVisibilityState) STUB()

WebAppWindowBase::WebAppWindowBase() : webapp_window_(0) STUB()
WebAppWindowBase::~WebAppWindowBase() STUB()
void WebAppWindowBase::Show() STUB()
void WebAppWindowBase::Hide() STUB()
void WebAppWindowBase::SetCustomCursor(CustomCursorType, const std::string&, int, int) STUB()
void WebAppWindowBase::SetHiddenState(bool) STUB()
void WebAppWindowBase::FirstFrameVisuallyCommitted() STUB()
void* WebAppWindowBase::GetNativeWindow() STUB(0)
void WebAppWindowBase::AttachWebContents(void*) STUB()
void WebAppWindowBase::DetachWebContents() STUB()
void WebAppWindowBase::RecreatedWebContents() STUB()
void WebAppWindowBase::Resize(int, int) STUB()
void WebAppWindowBase::SetOpacity(float) STUB()
void WebAppWindowBase::SetWindowHostState(NativeWindowState) STUB()
NativeWindowState WebAppWindowBase::GetWindowHostState() const STUB(NATIVE_WINDOW_DEFAULT)
void WebAppWindowBase::SetWindowProperty(const std::string&, const std::string&) STUB()
void WebAppWindowBase::SetUseVirtualKeyboard(bool) STUB()
void WebAppWindowBase::SetKeyMask(WebOSKeyMask, bool) STUB()
unsigned WebAppWindowBase::GetWindowHandle() STUB(0)
int WebAppWindowBase::DisplayWidth() STUB(0)
int WebAppWindowBase::DisplayHeight() STUB(0)

}  // namespace webos
