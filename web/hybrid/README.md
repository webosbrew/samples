# SDL2 and a web view in one process

A native app that needs the user to sign in on somebody else's web page, and needs whatever
that page hands back. Native code draws one screen, Chromium draws the provider's, and the
result comes home in the redirect URL:

```
  [native: "Sign in"]  ->  [web: provider's login form]  ->  [native: "signed in as demo"]
                                        |
                            navigates to redirect_uri?user=...
                            which the app intercepts and the browser never loads
```

One process, two Wayland surfaces, no second app and no browser to ship.

This is the shape [chiaki-ng](https://github.com/streetpea/chiaki-ng) needs for PSN login,
and shipping a whole browser for it is absurd. The provider here is a local `page.html`
rather than Sony, so the sample is self-contained, but the mechanism is identical.

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
thread reports the `wayland` driver and gives back a window, with libcbe already running in
the same process. So does a second GL context: `Mali-470 MP, OpenGL ES 2.0`, alongside the
EGL that Chromium is already using.

## The native view

Drawn with [Nuklear](https://github.com/Immediate-Mode-UI/Nuklear) on GLES2, behind the
four functions in `native_ui.h`. It is in its own file, and in C rather than C++, for two
reasons: Nuklear compiles its implementation into exactly one translation unit and is not
happy as C++, and `main.cpp` is about one process owning two window systems - burying that
under an immediate-mode toolkit would hide the part worth reading.

The toolkit earns its place by drawing text at all. SDL2 alone cannot, so the alternative
was coloured rectangles and a shipped font; Nuklear bakes its own atlas, rebaked here at
30px because the built-in 13px is unreadable across a living room.

### GLES2 is not a preference

Nuklear's `sdl_renderer` backend - and Dear ImGui's `imgui_impl_sdlrenderer2` - are built on
`SDL_RenderGeometry`, which arrived in **SDL 2.0.18**. The TV ships **2.0.4**:

```
SDL_RenderGeometry:     MISSING
SDL_RenderGeometryRaw:  MISSING
SDL_GetTicks64:         MISSING
```

The buildroot NDK ships 2.30.12, so a `sdl_renderer` build compiles perfectly and fails on
the device. `demo/sdl_opengles2/nuklear_sdl_gles2.h` avoids all of it by using its own GL
context.

That backend still calls `SDL_GetTicks64` twice, which `-verify` caught and the compiler
could not:

```
* Symbol SDL_GetTicks64 is undefined (bound lazily)
```

`native_ui.c` substitutes a function-like macro over the two call sites before including
the header - no patching a fetched dependency, and no competing `SDL_GetTicks64` in this
binary that would shadow the real one on firmware that has it.

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

## Getting the answer out: intercept the navigation

The important channel is not JavaScript at all. Every navigation the web view starts arrives
in the delegate first, with the full URL and its query string:

```cpp
void DidStartNavigation(const std::string& url, bool is_main_frame) override {
  if (!is_main_frame) return;
  if (url.compare(0, sizeof(kRedirectPrefix) - 1, kRedirectPrefix) != 0) return;
  OnRedirect(url);          // pull "user" and "state" straight out of the URL
}
```

**It fires before the request is made**, which is the property the whole flow rests on: the
redirect target never has to exist. This sample points it at `webosbrew.invalid`, and the
log shows the interception landing first and the failure arriving after:

```
[web] navigate https://webosbrew.invalid/callback?state=s1-9431&user=demo
[auth] redirect: user='demo' state='s1-9431'
```

`StopLoading()` in the handler keeps the dead host from costing a DNS lookup and an error
page. A real provider's `redirect_uri` behaves the same way - chiaki sends PSN to
`https://remoteplay.dl.playstation.net/remoteplay/redirect`, which serves nothing useful,
and reads `?code=` off it exactly like this.

Data goes the *other* way in the URL too, the way any OAuth client does it - no JavaScript
involved:

```cpp
"file://" + app_path + "/page.html?state=" + nonce + "&redirect_uri=" + kRedirectPrefix
```

The page reads those with `URLSearchParams` and sends the nonce back; the app throws the
result away if it does not match. That check is not decoration - it is what stops an
unrelated navigation being mistaken for your redirect.

The form needs somewhere to type from a remote, so the window sets
`SetUseVirtualKeyboard(true)`.

What this sample skips and a real client must not: proper percent-decoding of the query
(the parser here is about ten lines), a nonce that is actually unguessable, and probably
`SetUserAgent()` - some providers refuse to serve a login page to an unrecognised browser.
Cookies persist in the `WebViewProfile`, so a second sign-in may not need the form at all.

## The other channel: JavaScript to native

The login flow does not need it, but it is the only channel that carries a value in *both*
directions, so it is worth knowing:

```js
var reply = PalmSystem.getResource('note::hello', '');   // -> whatever native writes
```
```cpp
void HandleBrowserControlFunction(const std::string& command,
                                  const std::vector<std::string>& args,
                                  std::string* result) override {
  // args[0] == "note::hello";  *result becomes the JS return value, synchronously
}
```

Two constraints. The command names belong to the injection rather than to you - the page
arrives by calling `getResource` - so a real protocol lives inside the argument. And only
the **first** argument survives the trip.

Going the other way, `RunJavaScript()` returns nothing, so native can push but never ask.
A native-initiated request needs two hops and a request id.

### This is what webOSTV.js is built on

Not a homebrew-only seam. LG's own [webOSTV.js](https://webostv.developer.lge.com/develop/tools/webostvjs-introduction)
reaches the platform exactly this way - unminified, its service call is:

```js
new PalmServiceBridge, this.bridge.onservicecallback = ..., this.bridge.call(uri, params)
```

and it uses `PalmSystem.platformBack`, `PalmSystem.deviceInfo`, `PalmSystem.identifier` and
`PalmSystem.stageReady` besides - `platformBack` being the same call this sample leaves the
web view with. So a page inside this app has the same foundation a normal webOS web app
does, and stock `webOSTV.js` should load in it.

Whether `webOS.service.request()` then *succeeds* is a separate question and untested here:
`PalmServiceBridge.call()` ends up at ls-hubd, which already logs
`Can not find service "" permissions` for this executable. That is the LS2 role side, and
this sample makes no Luna calls.

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

Verified on a 49LK5900 (webOS 4.4.3): the whole sign-in flow runs end to end - native panel,
provider form, and back to the panel reading `signed in as demo`, with the nonce checked and
the redirect never loaded. Both views render full-screen, and the switch works in both
directions. The SDL view keeps
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
