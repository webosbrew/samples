// Link-time stand-in for the neva_app_runtime half of libcbe.
#include "neva_app_runtime/webapp_window_base.h"
#include "neva_app_runtime/webview_base.h"

#define STUB(...) \
  { return __VA_ARGS__; }

int AppRuntimeMain(int, const char**) STUB(0)

namespace neva_app_runtime {

WebViewBase::WebViewBase(int, int, WebViewProfile*) : webview_(0) STUB()
WebViewBase::~WebViewBase() STUB()
void* WebViewBase::GetWebContents() STUB(0)
void WebViewBase::LoadUrl(const std::string&) STUB()
void WebViewBase::SetAppId(const std::string&) STUB()
void WebViewBase::SetVisible(bool) STUB()

WebAppWindowBase::WebAppWindowBase() : webapp_window_(0) STUB()
WebAppWindowBase::~WebAppWindowBase() STUB()
void WebAppWindowBase::SetBounds(int, int, int, int) STUB()
void WebAppWindowBase::Show() STUB()
void WebAppWindowBase::Hide() STUB()
void WebAppWindowBase::Activate() STUB()
void WebAppWindowBase::Resize(int, int) STUB()
void WebAppWindowBase::AttachWebContents(void*) STUB()
void WebAppWindowBase::SetWindowHostState(WidgetState) STUB()
void WebAppWindowBase::SetWindowProperty(const std::string&, const std::string&) STUB()

}  // namespace neva_app_runtime
