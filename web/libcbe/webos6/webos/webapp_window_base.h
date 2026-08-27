// webos::WebAppWindowBase on webOS 6 and newer.
//
// Thirteen virtual slots, and a hierarchy that gives away where LG was heading:
// the vtable mixes webos:: methods with neva_app_runtime::WebAppWindowDelegate
// ones, so on this generation the old API sits on top of the new runtime rather
// than beside it.
//
// Slot order from WebAppWaylandWindow's vtable in a 65UP7560's libWebAppMgr.so:
//
//   0,1  destructor
//   2    OnWindowClosing            (webos::WebAppWindowBase)
//   3    CursorVisibilityChanged    (neva_app_runtime::WebAppWindowDelegate)
//   4    event(AppRuntimeEvent*)    (neva_app_runtime::WebAppWindowDelegate)
//   5    WebAppWindowDestroyed      (webos::WebAppWindowBase)
//   6    event(WebOSEvent*)
//   7    CheckKeyFilterTable
//   8,9  not recoverable
//   10   OnCreatedMediaLayer
//   11   WillDestroyAllMediaLayers
//   12   ResizedSwapBuffer
#pragma once

#include <string>

class WebOSEvent;

namespace neva_app_runtime {
class AppRuntimeEvent;
}

namespace webos {

enum NativeWindowState {
  NATIVE_WINDOW_DEFAULT = 0,
  NATIVE_WINDOW_MINIMIZED,
  NATIVE_WINDOW_MAXIMIZED,
  NATIVE_WINDOW_FULLSCREEN,
};

enum CustomCursorType { CUSTOM_CURSOR_NOT_USE, CUSTOM_CURSOR_BLANK, CUSTOM_CURSOR_PATH };

typedef unsigned WebOSKeyMask;

class WebAppWindowBase {
 public:
  WebAppWindowBase();
  virtual ~WebAppWindowBase();

  virtual void OnWindowClosing() {}
  virtual void CursorVisibilityChanged(bool visible) { (void)visible; }
  virtual void event(neva_app_runtime::AppRuntimeEvent* e) { (void)e; }
  virtual void WebAppWindowDestroyed() {}
  virtual bool event(WebOSEvent* e) { (void)e; return false; }
  virtual unsigned CheckKeyFilterTable(unsigned keycode, unsigned* modifier) {
    (void)keycode;
    (void)modifier;
    return 0;
  }
  virtual void Unknown8() {}
  virtual void Unknown9() {}
  virtual void OnCreatedMediaLayer(const std::string& a, const std::string& b, unsigned c) {
    (void)a;
    (void)b;
    (void)c;
  }
  virtual void WillDestroyAllMediaLayers() {}
  virtual void ResizedSwapBuffer() {}

  // Non-virtual, and InitWindow is back after webOS 3 did without it.
  void InitWindow(int width, int height);
  void Show();
  void Activate();
  void AttachWebContents(void* web_contents);
  void SetWindowHostState(NativeWindowState state);
  void SetWindowProperty(const std::string& name, const std::string& value);

 private:
  // 8 bytes, unlike WebViewBase next door: libcbe's constructor touches only
  // offsets 0 and 4.
  void* webapp_window_;  // WebAppWindow*, owned by libcbe
};

}  // namespace webos
