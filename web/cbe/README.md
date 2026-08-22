# libcbe - the TV's Chromium in a native app

`/usr/lib/libcbe.so` is the "Chromium Browser Engine": a ~70 MB build of Chromium with
LG's webOS patches, and the thing every web app on the TV actually runs inside. It is a
plain shared library with a C++ ABI, and a native app can link against it and get a real
web view - no WAM, no web app package, no `type: "web"`.

There is no SDK for this. No headers ship on the device or in the NDK, and the library is
stripped. What this sample links against was reconstructed, and the reconstruction is the
interesting part, so it is written down below.

## What it does

Hands the process to Chromium, waits to be called back on the browser UI thread, and puts
a page on screen as a normal foreground card:

```
main()
  |
  +- g_idle_add(CreateWebApp)      queue work on the default GMainContext
  |
  +- WebOSMain(argc, argv)         never returns
        |
        +- ... Chromium starts, begins pumping the default GMainContext ...
             |
             +- CreateWebApp()     on the browser UI thread
                  WebAppWindowBase::InitWindow + SetWindowProperty("appId", ...)
                  WebViewBase::Initialize + LoadUrl
                  window->AttachWebContents(webview->GetWebContents())
```

That inversion is the whole shape of the sample. `WebOSMain()` is Chromium's content main:
it takes the process over, re-execs this same binary for the renderer, owns the message
loop, and does not return. So there is no "initialise the web view, then carry on" - the
app has to give the process away and arrange to be called back.

The seam is the one WAM uses. libcbe drives its browser UI thread from the **default**
`GMainContext`, which is how WAM's Luna service ends up running on that thread: WAM's
`main()` starts its LS2 service, then calls `WebOSMain()`, and the callbacks arrive on the
browser thread afterwards. Anything queued onto that context before `WebOSMain()` therefore
runs exactly once, on the browser UI thread, as soon as Chromium is up - which is the first
moment a window or a web view may legally be created.

## webOS 4 only, and why

| release | libcbe | entry point | verdict |
|---|---|---|---|
| 1.x, 2.x | absent | - | a different engine entirely - Qt5WebKit, see below |
| 3.4 - 3.9 | yes | `WebOSMain` | reachable, but a legacy-ABI variant: see below |
| **4.x** | **yes** | **`WebOSMain`** | **this sample** |
| 5.x | yes | `webos::WebOSMain::Run` | different entry point and `Initialize` |
| 6.x - 11.x | yes | `webos::WebOSMain::Run`, plus `neva_app_runtime` | a second, parallel API |

Both bounds are ABI, not caution, and neither is a wall - each is a build variant, the same
way `media/smp/common` is built four times for four generations of `StarfishMediaAPIs`.

**Below.** webOS 3's libcbe is the pre-C++11 `std::string` ABI throughout - there is not one
`__cxx11` symbol in the whole library - and it predates the split constructor:

| | webOS 3.4 | webOS 4.4 |
|---|---|---|
| `std::string` | `std::basic_string` | `std::__cxx11::basic_string` |
| construction | `WebViewBase(int w, int h)` | `WebViewBase()` then `Initialize(...)` |
| window setup | `Resize()` + `Show()` | `InitWindow(w, h)` + `Activate()` |
| absent on 3.4 | | `SetAppPath`, `LoadExtension`, `UpdatePreferences`, `SetWindowProperty`* |

\* `SetWindowProperty` and `SetCustomCursor` do exist on 3.4, but with old-ABI strings.

Of the 30 libcbe symbols this sample needs, 11 are absent from a webOS 3.4 dump - all of
them either old-ABI spellings or the newer split-init calls. So webOS 3 wants a variant
built `-D_GLIBCXX_USE_CXX11_ABI=0` against an older header, not a newer runtime.

**Above.** `webos::WebViewBase` exists all the way to 11.2, but it is not one API - how you
construct and initialise it moves five times:

| release | constructor | `Initialize(...)` | entry point |
|---|---|---|---|
| 3.4 - 3.9 | `WebViewBase(int, int)` | *(none)* | `WebOSMain()`, pre-C++11 strings |
| **4.0 - 4.9** | **`WebViewBase()`** | **`(5 str, int, int, bool)`** | **`WebOSMain()`** - what this sample builds |
| 4.10 | `WebViewBase()` | `(5 str, int, int, bool, bool)` | `WebOSMain()` |
| 5.x | `WebViewBase(int, int)` | `(5 str, int, int, bool, WebViewMode, bool)` | `webos::WebOSMain::Run()` |
| 6.4 - 11.2 | `WebViewBase(bool, int, int)` | `(5 str, bool)` | `webos::WebOSMain::Run()` |

Everything else is stable: of the 30 libcbe symbols this sample uses, the *same three* are
the only ones missing on every release from 5.3.1 to 11.2 - the free `WebOSMain`, the
no-argument constructor, and that `Initialize`. The other 27 are untouched across six
generations.

So the interesting variant is not webOS 5, it is **6.4 through 11.2**: one build covers
every set from 2021 to 2025, because the constructor and `Initialize` do not move again
after 6.4. (Those releases also carry a second, parallel API under `neva_app_runtime`,
which is the one with public upstream headers.)

The upper bound of *this* sample is therefore 4.9, not 5 - the break is `Initialize`
gaining one `bool` at 4.10, not the webOS 5 rewrite.

**The hard floor is 3.4**, and it is the library, not the ABI: webOS 1 and 2 have no
`libcbe.so` at all. Chromium arrived with webOS 3.

### webOS 1 and 2: Qt5WebKit, and why it is not a variant

Those releases do have a web engine, just not this one:

| | webOS 1.2 / 1.4 | webOS 2.2.3 | webOS 3.4+ |
|---|---|---|---|
| Qt | 5.0.0 | 5.2.1 | - |
| engine | `libQt5WebKit.so.5.0.0` | `libQt5WebKit.so.5.2.0` | `libcbe.so` |

`libQt5WebKitWidgets` is there too, with the whole classic WebKit1 API - `QWebView`,
`QWebPage`, `QWebFrame`, `QWebSettings` - and a JavaScript bridge that is frankly nicer
than anything libcbe offers: `QWebFrame::evaluateJavaScript()` returns a value
synchronously, `addToJavaScriptWindowObject()` hands page JavaScript a real `QObject` with
slots and properties, and `QWebPage::javaScriptConsoleMessage()` is a console hook. No
injection to load, no command names to overload.

Two things make it a separate project rather than a variant of this one:

* **Nothing in the firmware uses the widgets API.** Scanning every shared object in the
  webOS 2.2.3 dump, `libQt5WebKitWidgets.so.5` has zero consumers; WAM reaches WebKit
  through QML instead (it needs `libQt5Qml`, `libQt5Quick` and `libQt5WebKit`). The dumps
  cover libraries and not executables, so this is not quite proof - but a shipped,
  unexercised library is exactly the situation this repo already has a scar from, so the
  first job would be proving it loads and paints at all.
* **The NDK cannot build for it.** The buildroot SDK ships Qt **5.15.14** against TVs
  running 5.0.0 and 5.2.1, and Qt's binary compatibility runs forwards only. It also has no
  QtWebKit headers at all, QtWebKit having been dropped from Qt after 5.5. That means
  period-correct headers from upstream plus link stubs - the same trick `web/libcbe` uses -
  plus `moc` for any object exposed to JavaScript.

A 55LF6310 (webOS 2.2.0) is the set that could settle the first point.

## What "webOS 4" is actually backed by

The `-verify` firmware set has exactly two webOS 4 dumps, 4.4.2 and 4.9.7, and both are
clean. There is no dump anywhere between 4.0 and 4.3, so that part of the declared range
rests on the API being unchanged across the generation rather than on evidence. Hardware
testing was on a 49LK5900 at 4.4.3.

Read it as: **4.4.2 and 4.9.7 verified, 4.0 to 4.3 assumed, 4.10 known broken.**

That last one is worth dwelling on, because `-verify` cannot see it: the verifier ships no
4.10 dump, so `>=4, <5` and `>=4, <4.10` check exactly the same two firmwares and both pass.
The break was only visible in the larger symbol set under
`dev-toolbox-cli/common/data`. A clean `-verify` means "nothing missing in the dumps we
have", which is a narrower claim than it looks.

## Where the headers came from

Two independent sources that agree with each other:

* **Firmware symbol tables** (`dev-toolbox-cli/common/data/*/libcbe.so.json`) give every
  exported name, and therefore every signature, across every release.
* **WAM's own vtables.** `libWebAppMgr.so` contains `BlinkWebView` and
  `WebAppWaylandWindow`, the only in-firmware subclasses of these classes. Their vtables
  pin down slot order, and their constructors pin down object size - `operator new(32)`
  followed by a `BlinkWebView` that writes its first field at offset 8 says
  `webos::WebViewBase` is exactly 8 bytes: a vptr and one pimpl pointer.

Three facts in `web/libcbe/webos/webview_base.h` are load-bearing:

* `WebViewDelegate` has **no virtual destructor**. Adding one shifts every slot by two.
* The delegate is **24 slots** long. Only the first 17 have recoverable names; libcbe still
  indexes past them, and a short vtable reads whatever follows it in memory. Leaving them
  out segfaults a few hundred milliseconds into the first page load - which is exactly how
  they were found.
* Chromium is built **without RTTI** and exports no typeinfo for these classes, so anything
  deriving from them must be compiled `-fno-rtti` too.

`WebAppWindowDelegate`, by contrast, *does* have a virtual destructor, so its `event()`
lands at slot 2. The two classes are not symmetric.

## Linking

The NDK has no libcbe, so the build makes a stand-in: `web/libcbe/cbe_stub.cpp` is compiled
into a shared object with SONAME `libcbe.so` and nothing else in it. The loader picks up the TV's
real library at runtime because the SONAME matches. It is never installed - see
`BUNDLE_LIBS` being absent from the `webos_add_ipk` call.

The stub is written as ordinary C++ against the same headers the sample uses, rather than
as a list of mangled names, so the two cannot drift apart.

## Resources

None need shipping. The system libcbe has `/usr/lib/cbe/webos_resources.pak` compiled in,
and finds `icudtl.dat`, the V8 snapshots and the locale paks next to it. (LG's own browser
app bundles a private copy of all of that, plus its own libcbe - that is a different, much
heavier arrangement, and not one homebrew wants.)

## Running it

```
cmake --build build --target web-cbe-install
ares-launch -d <device> org.webosbrew.sample.web.cbe
```

Launch it, do not run the binary from a shell: SAM sets `APPID` and `XDG_RUNTIME_DIR` and
gives the process the session it needs. Direct execution mostly works but is not the thing
being demonstrated.

SAM points a launched app's stdout at `/dev/null`, so the sample redirects it - its own
delegate output is in `/tmp/web-cbe.log`. libcbe's logging goes through PmLog and reaches
`/var/log/messages` under the `web-cbe` tag either way (`journalctl` is not on the PATH
these shells get).

To see what actually reached the screen - LSM will happily report a foreground surface that
drew nothing:

```sh
ares-shell -d <device> -r "luna-send -n 1 -w 10000 -f \
    luna://com.webos.service.tv.capture/executeOneShot \
    '{\"path\":\"/tmp/shot.png\",\"method\":\"DISPLAY\",\"format\":\"PNG\"}'"
ares-pull -d <device> /tmp/shot.png .
```

## Hybrid apps: talking to the page, and being talked to

All of this was checked on the 49LK5900, with a throwaway probe build rather than with the
sample as committed.

**Native to JS.** `RunJavaScript()` and `RunJavaScriptInAllFrames()` work and take effect
immediately. Neither returns a value - there is no `...AndReturnResult` in this API.

**JS to native.** libcbe carries Chromium V8 *injections*, and one of them is the
`PalmSystem` object every webOS web app already uses. Load it before navigating:

```cpp
webview->Initialize(app_id, app_path, "trusted", "", "", 1920, 1080, false);
webview->LoadExtension("palmsystem");   // "palmsystem", NOT "v8/palmsystem"
```

The injection's native functions do not stay inside libcbe - they turn into
`BrowserControlMsg_Command` / `BrowserControlMsg_Function` IPC, which surfaces in the
browser process as delegate slots 19 and 20, `HandleBrowserControlCommand()` and
`HandleBrowserControlFunction()`. Those are *your* overrides. `Function` is synchronous and
hands you a `std::string*` to fill in, and the value lands back in JavaScript as the return
value of the call.

So this round trip works today:

```js
var reply = PalmSystem.getResource('probe-cmd', 'probe-arg');   // -> "native-said-hello"
```
```
[bridge] FUNCTION 'getResource' (1 args)
[bridge]   arg[0] = 'probe-cmd'
```

Two things to know. The command names are the injection's, not yours - you are overloading
`getResource`, `serviceCall`, `activate` and the rest, so a real app JSON-encodes its own
protocol into one of them. And only the first argument came through on `getResource`, so
pack everything into that one string. The injection also calls `initialize` and
`identifier` on startup expecting WAM-shaped answers.

Some of the injection's calls do not need overloading at all, because they map onto
delegate slots that exist for them:

| JavaScript | native |
|---|---|
| `PalmSystem.close()` | `WebViewDelegate::Close()` |
| `PalmSystem.platformBack()` | `HandleBrowserControlCommand("platformBack")` |

Prefer those where they fit - `web/hybrid` leaves its web view through `close()`.

`PalmServiceBridge` is injected too (`new PalmServiceBridge()` yields an object with a
`call` function), which is the standard path for page JavaScript to reach `luna://`
services - including one your own native process registers. That is the right answer when
the bridge needs a real protocol rather than a signal, but it was not tested here.

**What not to use.** `document.title` also reaches native code, as `TitleChanged()`, and it
is tempting because it needs no injection. It is not an IPC channel: the title is a UI
property with one global slot, so it collides with any page that manages its own, carries
no arguments, and forces edge-detection hacks to tell a fresh signal from a leftover one.
Use it for what it is - a page title.

**Capturing console output.** Page `console.log` goes nowhere by default. Add
`--enable-logging=stderr` to the switch list and it appears on stderr, tagged with the app
id:

```
[org.webosbrew.sample.web.cbe] "PROBE console.log works", source: data:text/html,...
```

## Showing and hiding the web window

The web window can be taken off the screen and brought back, and this was measured with
display captures rather than guessed at:

```cpp
webview->SuspendPaintingAndSetVisibilityHidden();
webview->SuspendWebPageDOM();
webview->SetVisible(false);
window->SetWindowHostState(webos::NATIVE_WINDOW_MINIMIZED);
window->Hide();
// ... later ...
window->Show();
window->SetWindowHostState(webos::NATIVE_WINDOW_FULLSCREEN);
webview->SetVisible(true);
webview->ResumeWebPageDOM();
webview->ResumePaintingAndSetVisibilityVisible();
window->Activate();
```

Hidden, the surface really is gone - the capture showed the TV falling through to the HDMI
input behind it. Restored, the page came back **without reloading**: same document, no
`LoadStarted`, no navigation. So "hand the screen to something else and come back" works
inside one process, and the page keeps its state across the round trip.

That is the cheap half. The expensive half is what fills the screen while the web window is
hidden, and there the process layout decides everything.

## Two processes, or one?

**Two apps.** Tested with `media/lgnc` (an ordinary SDL2 sample) as the native app and this
one as the web app:

* launching the web app over the running native app works, and the native app keeps
  running in the background;
* `luna://com.webos.applicationManager/closeByAppId` closes the web app cleanly;
* **but the screen does not go back.** Closing the web app left the TV showing
  `com.webos.app.externalinput.av1`, not the native app that was still alive behind it. LSM
  does not restore a caller;
* and relaunching the native app to get back gave it **new pids** - a cold restart, not a
  resume, because the sample implements none of the SAM native lifecycle
  (`nativeLifeCycleInterfaceVersion`, `handlesRelaunch`, `registerApp`).

So the two-app split is workable but not free: the web app has to explicitly launch the
native app on its way out, and the native app has to implement the SAM lifecycle or it will
lose its state every time you come back. That lifecycle work is the real cost of this
option, not the launching.

**One process.** The constraint is that `WebOSMain()` owns `main()` and the default
`GMainContext` and never returns, so a second toolkit cannot run a `while (SDL_PollEvent)`
loop in the usual place. The way around it is the same seam the sample already uses for
startup: SDL is driven *from* the glib loop - create the window and pump events from a
`g_timeout_add` on the browser UI thread, so both toolkits live on one thread with one
loop. Two Wayland surfaces in one process is not itself a problem.

This is reasoning, not a result: it has not been built. If you go that way, the thing to
check first is whether SDL2 tolerates having its video subsystem initialised somewhere
other than the real `main()` thread.

## Combining with native rendering

There is no offscreen path. Nothing in the exported API hands back a GL texture or an
exported surface - `AttachWebContents()` gives the contents to a libcbe-owned Wayland
window and that is where they are drawn. So a hybrid UI has to be composed at the window
level, and libcbe exports the pieces for it:

* `WebViewBase::SetTransparentBackground(true)` punches the page through to whatever is
  behind it. This is how web apps on the TV show hardware video: the decoder owns a plane,
  the page draws the UI over a transparent hole. Pairing it with `media/smp/acb` is the
  most likely shape of a native-plus-web app here.
* `WebAppWindowBase::CreateWindowGroup()` / `AttachToWindowGroup()` with
  `WindowGroupConfiguration::AddLayer()` put several surfaces into one LSM group with named,
  ordered layers - WAM's mechanism for overlays.
* `SetOpacity()` and `WebOSPlatform::SetInputRegion()` control blending and which rectangles
  take input, so a native surface can stay clickable under a full-screen page.

Neither of those two combinations was tested. What *is* established is the constraint that
shapes them: `WebOSMain()` owns the process and the default `GMainContext`, so a second
toolkit (SDL2, say) cannot run its own main loop in the usual way - it would need its own
thread and its own Wayland surface, and the two would then compete for LSM focus.

## A worked hybrid app

`web/hybrid` is this sample plus an SDL2 view, in one process, swapping which one is on
screen. It is where the show/hide and JavaScript-bridge machinery below is actually used.

## What is not here

* **Input.** The sample never calls `ForwardWebOSEvent()`, and whether remote-control keys
  reach the page on their own has not been tested.
* **The bridge.** The sample loads no injection and overrides the browser-control slots
  with empty bodies. The section above says what it takes to turn them on.
* **Lifecycle.** No SAM relaunch/close handling and no suspend on background. The calls
  exist and are wired into the header - see the show/hide section - but nothing drives them,
  and without the SAM side a relaunch is a cold restart.
* **webOS 5+.** See the table above.
