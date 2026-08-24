// webos::WebAppWindowBase on webOS 3. Thirteen virtual slots against webOS 4's
// ten, and no InitWindow() - the window exists once the object does, and Resize
// gives it a size.
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

class WebAppWindowDelegate {
 public:
  virtual ~WebAppWindowDelegate() {}
  virtual bool event(WebOSEvent*) { return false; }
  virtual unsigned CheckKeyFilterTable(unsigned, unsigned*) { return 0; }
};

class WebAppWindowBase : public WebAppWindowDelegate {
 public:
  WebAppWindowBase();
  ~WebAppWindowBase() override;

  // Slots 4 to 12, in this order. SetHiddenState, FirstFrameVisuallyCommitted
  // and GetNativeWindow sit in the middle of them on this generation, where
  // webOS 4 has nothing.
  virtual void Show();
  virtual void Hide();
  virtual void SetCustomCursor(CustomCursorType type, const std::string& path,
                               int hotspot_x, int hotspot_y);
  virtual void SetHiddenState(bool hidden);
  virtual void FirstFrameVisuallyCommitted();
  virtual void* GetNativeWindow();
  virtual void AttachWebContents(void* web_contents);
  virtual void DetachWebContents();
  virtual void RecreatedWebContents();

  void Resize(int width, int height);
  void SetOpacity(float opacity);
  void SetWindowHostState(NativeWindowState state);
  NativeWindowState GetWindowHostState() const;
  void SetWindowProperty(const std::string& name, const std::string& value);
  void SetUseVirtualKeyboard(bool enable);
  void SetKeyMask(WebOSKeyMask key_mask, bool set);
  // The handle the GPU side uses to make an accelerated surface for this window.
  unsigned GetWindowHandle();
  int DisplayWidth();
  int DisplayHeight();

 private:
  void* webapp_window_;  // WebAppWindow*, owned by libcbe
};

}  // namespace webos
