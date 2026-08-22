# SDL2 and a web view in one process

Native code draws one screen, Chromium draws another, and pressing a button swaps them:

```
  [SDL view] --OK/Enter--> [web view] --exit button or Back--> [SDL view]
```

One process, two Wayland surfaces, no second app and no IPC.

## The problem this solves

The two toolkits disagree about who owns the process. `WebOSMain()` is Chromium's content
main: it never returns and it owns the message loop. SDL normally wants a
`while (SDL_PollEvent(...))` loop in `main()`. Only one of them can have it.

Chromium wins, and SDL is driven from *its* loop instead. libcbe pumps the default
`GMainContext` on its browser UI thread, so a `g_timeout_add(16, Pump, ...)` there polls SDL
events and repaints at ~60 Hz. Both toolkits then live on one thread with one loop, and
switching views is a plain function call rather than a lifecycle event:

```cpp
gboolean Pump(gpointer) {
  SDL_Event e;
  while (SDL_PollEvent(&e)) { ... }
  if (g_native_visible) DrawNativeView();
  return G_SOURCE_CONTINUE;
}
```

SDL initialises fine from there - `SDL_InitSubSystem(SDL_INIT_VIDEO)` on the browser UI
thread reports the `wayland` driver and gives back a window and an accelerated renderer,
with libcbe already running in the same process.

## Switching

Hiding is not just `SDL_HideWindow()` on one side and `Hide()` on the other; the web view
also gets suspended, which is what stops a backgrounded page burning CPU on timers and
animation:

```cpp
webview->SuspendPaintingAndSetVisibilityHidden();
webview->SuspendWebPageDOM();
webview->SetVisible(false);
window->SetWindowHostState(webos::NATIVE_WINDOW_MINIMIZED);
window->Hide();
```

Coming back resumes rather than reloads. That is the whole reason for suspending instead of
destroying: the second visit keeps whatever state the page had, and the log shows no
navigation at all.

## How the page asks to leave

`document.title`. The page sets it, the app sees `WebViewDelegate::TitleChanged()`, and
that is the entire channel:

```js
function exitToNative() { document.title = 'hybrid:exit'; }
```

It needs no injection, no permissions and no extra IPC, which makes it the right size for
one signal. `web/cbe/README.md` describes the two heavier channels - the `palmsystem`
injection with `HandleBrowserControlFunction()`, and `PalmServiceBridge` over Luna - for
when you need arguments and return values.

Two details make it work reliably. The app ignores the exit title unless the web view is
actually on screen, or a title left over from a previous visit bounces the user straight
back out. And the page restores its title on `visibilitychange`, so there is a fresh edge
for native code to see next time.

## Loading the app's own page

Three settings are needed to load `page.html` over `file://`, and two are not enough:

```cpp
webview->SetAllowLocalResourceLoad(true);
webview->SetFileAccessBlocked(false);
webview->SetAllowUniversalAccessFromFileUrls(true);
```

Without the third, the renderer is killed mid-load - `bad IPC message, reason 114` - rather
than being told no, which looks like a crash rather than a permissions problem.
`SetWebSecurityEnabled(false)`, the obvious sledgehammer, turns out not to be needed.

## Logging

SAM points a launched app's stdout at `/dev/null`, so both samples redirect it:

```sh
ares-shell -d <device> -r 'cat /tmp/web-hybrid.log'
```

libcbe's own logging goes through PmLog and reaches `/var/log/messages` either way, and
page `console.log` shows up in the redirected stream because the app passes
`--enable-logging=stderr`.

## State

Verified on a 49LK5900 (webOS 4.4.3): both views render full-screen, the switch works in
both directions, the page's exit button reaches native code through `TitleChanged`, and
re-entering the web view resumes the existing page without reloading. The SDL view keeps
animating after coming back.

**Not verified: the remote itself.** Every transition above was driven from a test hook
calling the same functions a key press would. Synthetic key injection through `/dev/uinput`
does not work here - the device registers, but LSM does not route its events to the app -
so the `SDLK_RETURN` and Back paths have not been exercised with real hardware. The rest of
the repo's SDL samples do handle the remote on this TV, so the mechanism is sound; this
particular wiring is simply untested.
