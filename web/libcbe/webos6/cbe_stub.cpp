// Link-time stand-in for webOS 6+'s /usr/lib/libcbe.so. Same idea as the webOS 4
// and webOS 3 stubs: right SONAME, right mangled names, never installed.
#include "webos/webapp_window_base.h"
#include "webos/webview_base.h"

#define STUB(...) \
  { return __VA_ARGS__; }

namespace webos {

WebOSMain::WebOSMain(WebOSMainDelegate*) : main_(0) STUB()
int WebOSMain::Run(int, const char**) STUB(0)

WebViewBase::WebViewBase(bool, int, int) : webview_(0) STUB()
WebViewBase::~WebViewBase() STUB()
void WebViewBase::Initialize(const std::string&, const std::string&, const std::string&,
                             const std::string&, const std::string&, int, int, bool,
                             WebViewMode, bool) STUB()
void* WebViewBase::GetWebContents() STUB(0)
void WebViewBase::LoadUrl(const std::string&) STUB()
void WebViewBase::StopLoading() STUB()
void WebViewBase::Reload() STUB()
std::string WebViewBase::GetUrl() STUB(std::string())
void WebViewBase::RunJavaScript(const std::string&) STUB()
void WebViewBase::UpdatePreferences() STUB()
void WebViewBase::SetAppId(const std::string&) STUB()
void WebViewBase::SetTrustLevel(const std::string&) STUB()
void WebViewBase::SetUserAgent(const std::string&) STUB()
void WebViewBase::SetVisible(bool) STUB()
void WebViewBase::SetAllowLocalResourceLoad(bool) STUB()
void WebViewBase::SetAllowUniversalAccessFromFileUrls(bool) STUB()
void WebViewBase::SetFileAccessBlocked(bool) STUB()
void WebViewBase::SetLocalStorageEnabled(bool) STUB()
void WebViewBase::SetVisibilityState(WebPageVisibilityState) STUB()

WebAppWindowBase::WebAppWindowBase() : webapp_window_(0) STUB()
WebAppWindowBase::~WebAppWindowBase() STUB()
void WebAppWindowBase::InitWindow(int, int) STUB()
void WebAppWindowBase::Show() STUB()
void WebAppWindowBase::Activate() STUB()
void WebAppWindowBase::AttachWebContents(void*) STUB()
void WebAppWindowBase::SetWindowHostState(NativeWindowState) STUB()
void WebAppWindowBase::SetWindowProperty(const std::string&, const std::string&) STUB()

}  // namespace webos
