// neva_app_runtime::WebAppWindowBase. Five virtual slots, none of them pure -
// libcbe implements every one, so a subclass that overrides nothing is valid.
#pragma once

#include <string>

namespace neva_app_runtime {

enum WidgetState {
  UNINITIALIZED = 0,
  CREATED,
  SHOW,
  HIDE,
  FULLSCREEN,
  MAXIMIZED,
  MINIMIZED,
  RESTORE,
  ACTIVE,
  INACTIVE,
  RESIZE,
  DESTROYED,
};

class AppRuntimeEvent;

class WebAppWindowBase {
 public:
  WebAppWindowBase();
  virtual ~WebAppWindowBase();

  virtual void OnWindowClosing() {}
  virtual void CursorVisibilityChanged(bool visible) { (void)visible; }
  virtual bool event(AppRuntimeEvent* e) { (void)e; return false; }

  // No InitWindow() on this API - Resize gives the window its size.
  void Resize(int width, int height);
  void SetBounds(int x, int y, int width, int height);
  void Show();
  void Hide();
  void Activate();
  void AttachWebContents(void* web_contents);
  void SetWindowHostState(WidgetState state);
  void SetWindowProperty(const std::string& name, const std::string& value);

 private:
  // libcbe's constructor writes as far as offset 4.
  void* webapp_window_;
};

}  // namespace neva_app_runtime
