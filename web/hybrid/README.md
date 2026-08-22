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

Two things about coming back are not obvious, and both showed up as a white screen on the
second visit with the page alive and running script behind it.

**`Hide()` destroys the window, it does not unmap it.** The log is explicit - `Wayland
Window(id:1 widget:0xc1f28) will be destroyed` - and the next `Show()` builds a new one,
`id:2`. Web contents attached to the old window composite nowhere. So
`AttachWebContents()` has to run on *every* show, not just the first.

**It has to run before `Show()`, not after.** Attaching afterwards leaves the page loading
normally, reporting `load finished`, and never appearing.

There is no matching `DetachWebContents()` on the way out. Calling it there segfaults on a
null pointer: by then the contents it would detach are already gone.

## How the page asks to leave

Through libcbe's own callbacks. The app loads the `palmsystem` injection, which gives page
JavaScript real entry points, and two of them arrive in the delegate:

| JavaScript | native | what it means |
|---|---|---|
| `PalmSystem.platformBack()` | `HandleBrowserControlCommand("platformBack")` | a notification - nothing is torn down |
| `PalmSystem.close()` | `WebViewDelegate::Close()` | a real close - the render view is going away |

```js
function exitToNative() { PalmSystem.platformBack(); }
```

**Not `close()`.** It reads like the right call and it is not. The callback arrives from
`RenderViewHostImpl::OnClose()` with the render view already being destroyed, which is
correct for a page that is quitting and wrong for one stepping aside for a moment - the
next visit gets a dead view. `platformBack()` is a plain notification, so the page survives
to be shown again. `Close()` is still handled, so a page that really does close itself
hands the screen back rather than leaving a dead window up.

Turning the injection on is two lines, and both matter:

```cpp
webview->Initialize(app_id, app_path, "trusted", "", "", 1920, 1080, false);
webview->LoadExtension("palmsystem");   // "palmsystem", NOT "v8/palmsystem"
```

Get either wrong and there is no `PalmSystem` object at all, with no error - the page just
finds it undefined, which is why `page.html` checks for it and says so on screen.

`web/cbe/README.md` covers the rest of the bridge, including
`HandleBrowserControlFunction()`, which is synchronous and returns a string to JavaScript,
and `PalmServiceBridge` for reaching `luna://` services.

### Not the title

An earlier version of this sample signalled the exit by setting `document.title` and
watching `TitleChanged()`. It worked, and it was wrong: the title is a UI property with one
global slot, so the channel collides with any page that manages its own title, cannot carry
arguments, and needs edge-detection hacks - a leftover title from the previous visit
bounced the user straight back out, and the page had to reset it on `visibilitychange` to
manufacture a fresh edge. None of that exists now. `TitleChanged()` is only logged.

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
both directions, the page's exit button reaches native code as `WebViewDelegate::Close()`,
and re-entering the web view resumes the existing page without reloading. The SDL view keeps
animating after coming back.

Anything that changes the window or the web view must also be **deferred out of a delegate
callback**. Those callbacks run inside libcbe's own call stack, and switching views from
one lands in the middle of a teardown it has not finished - a null-pointer segfault, with
`ShowNativeView` sitting directly under `RenderViewHostImpl::OnClose()` in the backtrace.
A one-shot `g_idle_add` is enough: the switch then happens once libcbe is back at idle.

**Not verified: the remote itself.** Every transition above was driven from a test hook
calling the same functions a key press would. Synthetic key injection through `/dev/uinput`
does not work here - the device registers, but LSM does not route its events to the app -
so the `SDLK_RETURN` and Back paths have not been exercised with real hardware. The rest of
the repo's SDL samples do handle the remote on this TV, so the mechanism is sound; this
particular wiring is simply untested.
