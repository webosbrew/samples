# web/cbe, on webOS 3

The same twenty lines of "make a window, make a web view, load a URL" as `web/cbe`, against
the libcbe that shipped on webOS 3. Everything that differs is the library moving underneath
it, and there is more of that than the version numbers suggest.

Measured on a 43UH6100 running **starfish 3.4.0**.

## The API is different, not just the ABI

| | webOS 3 | webOS 4 |
|---|---|---|
| `std::string` | pre-C++11 (`RKSs`) | `__cxx11` |
| construction | `WebViewBase(int w, int h)` | `WebViewBase()` then `Initialize(...)` |
| window size | no `InitWindow()`; `Resize()` | `InitWindow(w, h)` |
| delegate slots | 24, own order | 24, different order |
| navigation start | `LoadStarted(url)` | `DidStartNavigation(url, bool)` |
| browser control | slots 18/19 | slots 19/20 |
| absent here | `Initialize`, `UpdatePreferences`, `SetTrustLevel`, `Activate` | - |

The delegate came out of webOS 3's own `libWebAppMgr.so`, the same way the webOS 4 one did:
`BlinkWebView`'s vtable is the only authority for slot order. `DidFirstNonBlankPaint` at slot
2 exists only on this generation, and `LoadProgressChanged` takes a URL besides the progress.

`-verify` earns its keep here: `SetTrustLevel` and `UpdatePreferences` were copied over from
the webOS 4 sample, build fine, and are simply absent on webOS 3.

## Two things that stop it starting

**`--ozone-platform=weboswayland`**, not `wayland`. The wrong name gets as far as
`DesktopFactoryWayland` and then aborts; the right one logs `Ozone: WebOSFactoryWayland`.

**`CDM_LIB_PATH` must be set.** webOS 3's `WebOSMain` does

```cpp
std::string cdm(getenv("CDM_LIB_PATH"));   // no null check
cdm += "/libwidevinecdmadapter.so";
```

so an unset variable aborts the process before any of your code runs, with

```
terminate called after throwing an instance of 'std::logic_error'
  what():  basic_string::_S_construct null not valid
```

and nothing else to go on. WAM inherits the variable from its own environment; a plain
native app does not, so the sample sets it to `/usr/lib`. That was found by pulling the
64 MB library, resolving the crash address to `WebOSMain+0x21f8`, and reading the literal the
`getenv` call loads - there was no other way to see it.

## State: loads and paints, but does not reach the screen

Verified working: the process starts, the page loads, and every delegate callback fires with
the recovered signatures -

```
[cbe] title 'Example Domain'
[cbe] document loaded
[cbe] first non-blank paint
[cbe] progress 100%
[cbe] finished example.com/
```

so the reconstructed ABI is right: the vtable slots line up, the old-ABI strings arrive
intact, and the web view is rendering.

**What does not work is compositing.** LSM keeps the previous app foreground, and with the
splash disabled the screen shows the TV's own no-signal wallpaper - so the surface is not
merely behind something, it is not there.

The search so far, all of it negative, and worth writing down so it is not repeated:

| tried | result |
|---|---|
| `SetWindowProperty("appId", ...)` | no effect - and it is the only property WAM sets besides the key-access ones |
| `SetWindowHostState(FULLSCREEN)`, before and after `Show()` | no effect |
| `SetHiddenState(false)`, `SetOpacity(1.0f)` after `Show()` | no effect |
| `--app-id=<appid>` (the switch exists in libcbe) | no effect |
| `webos::Platform::Get()` then `SetFullscreen(true)` | returns **nil** - that singleton is not constructed in a plain embedder |
| dropping `Resize()`, which WAM never calls here | **worse**: the delegate stops firing and Wayland reports `proxy already has listener` |

Two things were learned rather than guessed. `noSplashOnLaunch` in `appinfo.json` matters:
without it SAM's launch splash covers the screen indefinitely and hides what is really
happening, which is what made this look like a compositing bug with a picture on top of it.
And `WebAppWaylandWindow::show()` in webOS 3's WAM turns out to be nothing but
`onStageActivated()` - pure WAM bookkeeping, no libcbe calls - followed by
`WebAppWindowBase::Show()`, so WAM is not doing anything special that the sample omits.

### What the window itself reports

The clearest symptom, from the diagnostics the sample prints:

```
[diag] before Resize: display=1920x1080 native=0x690b0 state=0
[diag] after Resize:  display=1920x1080 native=0x690b0 state=0
[diag] after Show:    native=0x690b0 state=0
```

The window object is real - a non-null native handle and the right panel size - but
`GetWindowHostState()` stays `0` (`NATIVE_WINDOW_DEFAULT`) through
`SetWindowHostState(NATIVE_WINDOW_FULLSCREEN)` and through `Show()`. The compositor never
acknowledges the state, which is a better description of the failure than "the window does
not appear": the surface exists and is being drawn into, and LSM is simply not treating it
as an app window.

The enum is not the problem - WAM passes literal `3` for fullscreen, matching
`NATIVE_WINDOW_FULLSCREEN` here - and neither is the call sequence. Disassembling
`WebAppWayland::raise()`, webOS 3's equivalent of webOS 4's `Activate()`, shows it makes
exactly one libcbe call, `SetWindowHostState(3)`, which the sample already does.

Two more switches turn out to be load-bearing rather than decorative: **`--webos-wam` is
required** - without it the process exits before writing a line of log - while `--app-id`,
which also exists in the library, changes nothing either way.

The app does reach the Luna bus: `ls-monitor -l` shows two client-only connections owned by
the executable, without a service name. So libcbe's own LS2 client is running.

### There *is* a standalone embedder on webOS 3

An earlier version of this file said there was not, on the strength of scanning `/usr/bin`
and `/usr/palm/applications`. That was wrong, and the way to find it is to ask SAM what is
running rather than to search the filesystem:

```sh
luna-send -n 1 -f luna://com.webos.applicationManager/running '{}'
```

`com.webos.app.browser` is there, `appType: native_builtin`, with a live pid - and
`/proc/<pid>/exe` points at
`/mnt/otncabi/usr/palm/applications/com.webos.app.browser/chrome`, which does link libcbe.
It is under `/mnt/otncabi`, which is why the earlier search missed it.

Its command line is the reference this sample has been missing, and two switches in it
contradict what was being used here:

```
--ozone-platform=wayland          (not weboswayland)
--webos-launch-json={"@system_native_app":true,"preload":"partial",
                     "nid":"com.webos.app.browser","launchHidden":true}
--in-process-gpu --ignore-gpu-blacklist --gpu-no-context-lost
--disable-gpu-watchdog --enable-accelerated-compositing
--set-maximized --window-size=1920,1080
```

There is no `--webos-wam` at all. `weboswayland` is WAM's backend - the one whose
`SetWidgetState` leaves `SHOW` unimplemented - and a non-WAM app is evidently expected to
use plain `wayland` and identify itself through `--webos-launch-json`, whose `nid` is the
app id.

Adopting it does not work yet, and bisecting says exactly which part is fatal: with
everything else from the browser adopted - the launch-json handling below,
`CHROMIUM_BROWSER=yes`, `BROWSER_NAME=Chromium38`, its GPU switches - the sample still runs
fine on `weboswayland`, and switching that one flag to `--ozone-platform=wayland` kills it
before Chromium writes a single log line. The log is zero bytes and the crash lands under
`__vsnprintf_chk` inside libcbe.

So `--ozone-platform=wayland` is the blocker, on its own, and the jail is not it: this
sample is jailed too, under `/var/palm/jail/org.webosbrew.sample.web.cbe3`.

Two pieces of the browser's setup were adopted and kept, because they are right regardless:

* **SAM hands a native app its launch parameters as a bare JSON argument**, and the browser
  turns that into `--webos-launch-json=` rather than forwarding it - libcbe would otherwise
  see `{"nid":...}` where it expects a URL. The sample now does the same.
* `CHROMIUM_BROWSER=yes` and `BROWSER_NAME=Chromium38`, which the browser has in its
  environment and a plain native app does not.

**This is the thread to pull.** A working standalone embedder exists on the same TV, its
configuration is known, and the difference is down to one switch that the sample cannot yet
survive.

### Registering with SAM: necessary, and still not sufficient

A native webOS app has to tell SAM it is running. libcbe does not do it - it opens its own
Luna connections but never registers the app - and on webOS 3 nothing else will either: WAM's
binary registers *itself* as `com.palm.webappmanager` before handing over to `WebOSMain`.

SDL-webOS does it in `SDL_webOSRegisterApp()`, and `luna_register.c` here is the same call
with the same library, minus SDL:

```c
HLunaServiceCall("luna://com.webos.applicationManager/registerNativeApp",
                 "{\"id\":\"org.webosbrew.sample.web.cbe3\"}", &ctx);
```

`libhelpers.so.2` is already on the TV, so it is `dlopen`ed rather than linked - one function
is not worth a NEEDED entry and something for `-verify` to check. `ctx.multiple = 1` keeps
the subscription open, which is how relaunch and close events arrive later. Version 1 of the
native lifecycle interface is `registerNativeApp`; version 2 would be `registerApp`, and the
sample's appinfo declares neither, which means 1.

It works:

```
[luna] lifecycle: {"message":"registered","returnValue":true}
[luna] registerNativeApp(org.webosbrew.sample.web.cbe3) -> 0
```

and the window still does not appear. So registration is a thing this sample was missing and
should have been doing, and it is not what the compositor is waiting for.

### The live lead is registration. libcbe contains
`palm://com.webos.applicationManager/registerNativeApp` and a `webos::LunaServices` class
whose `Initialize(const base::FilePath&)` is an instance method needing a
`webos::LunaServices(webos::Platform*)` - and `Platform` is the browser application's layer,
built by `ChromeMain` rather than by `WebOSMain`. So on this generation the Luna
registration that a native app needs may simply live on the browser's side of the library
and not the embedder's. That is a hypothesis, not a finding.

### A conclusion worth considering

Both in-firmware users of libcbe on webOS 3 bring their own window management: WAM wraps
`WebOSMain` in `WebAppWayland`, and the browser does not use `WebOSMain` at all - it uses
`ChromeMain`, which is what constructs `webos::Platform` and its Luna side. There may
therefore be no supported standalone-embedder path on this generation, and `WebOSMain` alone
may be expected to yield a rendering web view whose *window* somebody else owns. webOS 4,
where the same sample works unchanged, would then be the generation that fixed it.

That is a hypothesis with four pieces of evidence behind it - `Platform::Get()` returning
nil, `ChromeMain` existing beside `WebOSMain`, `--webos-wam` being mandatory, and WAM being
the only binary on the TV that links the library at all - and it should be tested rather
than believed.

### Chasing `PlatformDelegate`, and what it cost

`webos::PlatformDelegate`'s vtable is exported, so its *shape* is recoverable even though
nothing in the firmware implements it: two destructor slots, then nine slots that are all
`__cxa_pure_virtual` in the base. Eleven virtuals, no names, no signatures.

A stub delegate with that shape is enough for `Runtime::Initialize(PlatformDelegate*)` to
accept it, and both it and `InitializePlatform(base::FilePath)` then return cleanly. Neither
changes anything: the host state still reads back 0 and LSM still shows the previous app.
The delegate is never called during startup, so the unknown signatures never come up - which
also means initialising it is not what the window is waiting for.

One more idea, also dead: asserting `Show()` and `SetWindowHostState(FULLSCREEN)` from
`DidFirstNonBlankPaint()`, on the theory that the compositor might ignore a state set on a
surface that has never committed a buffer. It does not - `host-state=0` after the first
frame too.

### `webos::Runtime` is worth knowing about either way. Unlike `Platform`, **its singleton is
alive in a plain embedder** - `Runtime::Get()` returns a real pointer - and it carries
`SetWindowSize()`, `InitializePlatform(const base::FilePath&)` and
`Initialize(webos::PlatformDelegate*)`. `SetWindowSize` and `InitializePlatform` were both
called successfully and changed nothing, so the remaining candidate on that path is
`Initialize(PlatformDelegate*)`, which needs a delegate whose interface has not been
reconstructed. `base::FilePath` is declarable, for what it is worth: libcbe exports its
`std::string` constructor and destructor, and its layout is that one member.

So this is honest work-in-progress. The hard half - the ABI - is done and demonstrated, and
the failure is now located precisely: no frame is ever committed to the window's Wayland
surface. The next person should start there - comparing this log against the same capture
from `web/cbe` on a webOS 4 set, where the identical code does present - rather than at the
windowing API, which the protocol trace shows is being driven correctly.
