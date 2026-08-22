// webos::WebAppWindowBase - the LSM/Wayland window libcbe paints a WebViewBase
// into. Same reconstruction caveats as webview_base.h.
//
// Layout that must hold: WebAppWindowDelegate DOES have a virtual destructor
// (slots 0 and 1), event() is slot 2, and the object is 8 bytes total.
#pragma once

#include <string>

class WebOSEvent;

namespace webos {

enum NativeWindowState {
  NATIVE_WINDOW_DEFAULT = 0,
  NATIVE_WINDOW_MINIMIZED,
  NATIVE_WINDOW_MAXIMIZED,
  NATIVE_WINDOW_FULLSCREEN,
};

enum CustomCursorType { CUSTOM_CURSOR_NOT_USE, CUSTOM_CURSOR_BLANK, CUSTOM_CURSOR_PATH };

typedef unsigned WebOSKeyMask;

// libcbe does not export this class's own members (webOS 4 and older do not
// export it at all), so the defaults live here. They only ever fill vtable slots
// in *our* subclass; libcbe's WebAppWindowBase keeps its internal ones.
class WebAppWindowDelegate {
 public:
  virtual ~WebAppWindowDelegate() {}
  virtual bool event(WebOSEvent*) { return false; }
  // unsigned, not bool - the SDK's webos/webapp_window_delegate.h says so.
  virtual unsigned CheckKeyFilterTable(unsigned, unsigned*) { return 0; }
};

class WebAppWindowBase : public WebAppWindowDelegate {
 public:
  WebAppWindowBase();
  ~WebAppWindowBase() override;

  void InitWindow(int width, int height);

  virtual void Show();
  virtual void Hide();
  virtual void SetCustomCursor(CustomCursorType type, const std::string& path,
                               int hotspot_x, int hotspot_y);
  virtual void AttachWebContents(void* web_contents);
  virtual void DetachWebContents();
  virtual void RecreatedWebContents();

  void Activate();
  void Deactivate();
  void Resize(int width, int height);
  void SetOpacity(float opacity);
  void SetWindowHostState(NativeWindowState state);
  NativeWindowState GetWindowHostState() const;
  void SetWindowProperty(const std::string& name, const std::string& value);
  void SetUseVirtualKeyboard(bool enable);
  void SetKeyMask(WebOSKeyMask key_mask, bool set);
  int DisplayWidth();
  int DisplayHeight();

 private:
  void* webapp_window_;  // WebAppWindow*, owned by libcbe
};

}  // namespace webos
