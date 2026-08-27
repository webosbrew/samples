# web/cbe, on webOS 3

> **Status: unfinished, and parked.** The reconstructed ABI is correct and demonstrated -
> the page loads and every delegate callback fires - but the window never reaches the
> screen. The failure is localised (see "Where the gap actually is") and the dead ends are
> written down so nobody repeats them. Picking this up again means interactive Ghidra work
> on `weboswayland::WaylandDisplay`, not more black-box probing.
>
> If you want a *working* embedded web view, use `web/cbe` on webOS 4, or the
> `neva_app_runtime` API on webOS 6 and newer - the latter has public upstream headers and
> needs no reverse engineering at all.

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

There is no `--webos-wam` at all. `weboswayland` is WAM's backend, and a non-WAM app is
evidently expected to use plain `wayland` and identify itself through
`--webos-launch-json`, whose `nid` is the app id.

Adopting it does not work yet, and bisecting says exactly which part is fatal: with
everything else from the browser adopted - the launch-json handling below,
`CHROMIUM_BROWSER=yes`, `BROWSER_NAME=Chromium38`, its GPU switches - the sample still runs
fine on `weboswayland`, and switching that one flag to `--ozone-platform=wayland` kills it
before Chromium writes a single log line. The log is zero bytes and the crash lands under
`__vsnprintf_chk` inside libcbe.

So `--ozone-platform=wayland` is the blocker, on its own, and the jail is not it: this
sample is jailed too, under `/var/palm/jail/org.webosbrew.sample.web.cbe3`.

#### Why `wayland` cannot work here: two platforms, two window APIs

Stepping through the startup one `puts()` at a time puts the crash in
`new SampleWindow()` - the `webos::WebAppWindowBase` constructor - and it is not timing:
delaying window creation by three seconds crashes identically.

That is the whole answer, and the rest of the evidence lines up behind it:

* webOS 3's libcbe contains **two** ozone platforms, `ozonewayland` (registered as
  `wayland`) and `weboswayland`. webOS 4's contains only `ozonewayland`.
* On the TV, `weboswayland` is passed by exactly one process, `/usr/bin/WebAppMgr`. The
  native browser passes `wayland`.
* The browser's binary does not reference `WebAppWindowBase` **at all** - it builds its UI
  from `Browser::Init(content::BrowserContext*, aura::Window*)` and the Views stack.

So the two are a matched pair. `WebAppWindowBase` is WAM's windowing API and belongs to
`weboswayland`; `wayland` is for Views-based apps like the browser, which use a different
and much larger API. Constructing a `WebAppWindowBase` under `wayland` segfaults inside
libcbe before the constructor returns, because that object has no backend there.

That also explains why `web/cbe` works on webOS 4 with the identical code: LG collapsed the
two platforms into one by then, so `wayland` and `WebAppWindowBase` are the same world. On
webOS 3 they are not, and a standalone embedder has to pick a side:

* **WAM's side** (`weboswayland` + `WebAppWindowBase`) - what this sample does. Everything
  works except the browser-side compositor ever producing a frame.
* **The browser's side** (`wayland` + Views) - a working standalone embedder exists, but it
  is a different API surface entirely, and none of the reconstructed headers here apply
  to it.

#### How far the `wayland` backend gets before that

Two of the browser's remaining differences turn out to be required, and finding the first
one needed the crash report rather than the log - because the log was the casualty.

**`FONTCONFIG_PATH` and `FONTCONFIG_FILE` must be set.** Without them the process dies with
a zero-byte log, and the backtrace lands in `__vsnprintf_chk` called from libcbe. Reading
the disassembly at that address shows a varargs logging helper: an `__snprintf_chk` for the
prefix, then `__vsnprintf_chk` for the message. **libcbe crashes inside its own logger**,
which is why nothing is ever written - whatever it was trying to report is lost with it.
Pointing the two variables at the system `/etc/fonts` is enough; the browser points them at
its own bundled copy.

With those set, the `wayland` backend gets much further - through Ozone init, SAM
registration and the Luna lifecycle subscription - before dying again:

```
[0824/183136:INFO:desktop_factory_wayland.cc(17)] Ozone: DesktopFactoryWayland
...
[luna] registerNativeApp(org.webosbrew.sample.web.cbe3) -> 0
[0100/000000:ERROR:zygote_linux.cc(622)] write: Broken pipe
```

That last line is a forked child noticing its parent has gone. The parent's own crash is a
virtual call through a garbage vtable pointer, in what the disassembly shows to be a
set-delegate helper - store the pointer at `this+64`, then immediately call slot 10 on it:

```asm
str  r1, [r0, #64]     ; this->delegate = arg
cbz  r1, done
ldr  r2, [r1]          ; r2 = arg->vptr
ldr  r2, [r2, #40]     ; <- SIGSEGV, vtable slot 10
blx  r2
```

So the `wayland` path wants a delegate that a WAM-shaped embedder never has to supply. It
is not this sample's reconstructed vtables at fault: on `weboswayland` every delegate
callback arrives correctly.

`--no-zygote` is also required - removing it, as the browser does, regresses to a zero-byte
log again.

Two pieces of the browser's setup were adopted and kept, because they are right regardless:

* **SAM hands a native app its launch parameters as a bare JSON argument**, and the browser
  turns that into `--webos-launch-json=` rather than forwarding it - libcbe would otherwise
  see `{"nid":...}` where it expects a URL. The sample now does the same.
* `CHROMIUM_BROWSER=yes` and `BROWSER_NAME=Chromium38`, which the browser has in its
  environment and a plain native app does not.

### Where the gap actually is

Everything observable on both sides of the comparison now matches, and the list is worth
having so nobody re-checks it:

| | this sample | WAM |
|---|---|---|
| Wayland requests sent | identical but for `xinput_extension.register_input` | |
| threads | `Chrome_InProcGp`, `WaylandDisplayP`, and the rest | same set |
| environment | identical after adding `CHROMIUM_BROWSER`, `BROWSER_NAME`, fontconfig | |
| SAM registration | `registered`, listed in `/running` | |
| window object | 1920x1080, native pointer, handle 1 | |
| web contents | non-null, attached | |
| delegate callbacks | all fire, including `DidFirstNonBlankPaint` | |

That last row is the one that localises it. **`DidFirstNonBlankPaint` fires, so the renderer
is painting.** What never happens is the browser-side compositor turning that into a frame:
`CreateAcceleratedSurface` is never reached, so there is no EGL surface, so no buffer, so no
`wl_surface.attach`.

So the gap is between "renderer has painted" and "browser compositor asks the GPU for an
output surface for widget 1". Two libcbe stubs sit near that path and are logged every run:

```
ERROR:webos_view.cc(102)]  Not implemented ... content::WebContents* WebOSView::GetActiveWebContents() const
ERROR:display.cc(305)]     Not implemented ... weboswayland::WaylandDisplay::SetWidgetState(... SHOW ...)
```

`GetActiveWebContents()` returning nothing from the view that is supposed to host the
contents looked like the answer, and it is worth writing down why it is not - along with
the technique, because that is the reusable part.

#### Finding a stub's callers in a stripped 65 MB binary

libcbe has no `.symtab`, and these functions are local, so the only handle is the
`NOTIMPLEMENTED` string. ARM Thumb reaches it PC-relatively, as a literal `V` plus an
`add rX, pc` at address `P`, where `V = target - (P + 4)`. So scan `.text` for words whose
implied `P` lands within a few KB *and* decodes as `add rX, pc` (`0x4478`-`0x447f`):

```python
V = struct.unpack_from('<i', data, toff + i)[0]
P = string_va - 4 - V
ins = struct.unpack_from('<H', data, off_of(P))[0]
if 0x4478 <= ins <= 0x447f: ...        # a real reference
```

That gives three sites for this string, and disassembling the first shows the stub body -
`logging::GetMinLogLevel`, a `LogMessage` built with line 102, matching
`webos_view.cc(102)`. Its single caller is four instructions long:

```asm
bl   GetActiveWebContents()   ; the stub - returns NULL
cbz  r0, done                 ; NULL, so give up silently
ldr  r3, [r0]                 ; contents->vptr
ldr  r3, [r3, #356]           ; slot 89
blx  r3
```

So libcbe wants to call one method on the active `WebContents` and skips it because the
accessor is a stub. **We hold that pointer** - `WebViewBase::GetWebContents()` returns it -
so the call libcbe skipped can be made by hand through the same vtable slot. Doing that
runs cleanly and changes nothing: no window, host state still 0. Whatever slot 89 is, it is
not what starts compositing, and the stub is a red herring.

Chromium's own verbose logging is no help: `--v=1` adds nothing, because `VLOG` is compiled
out of this build.

**So can the WAM side ever show a window?** On the evidence, yes: WAM produces buffers on
exactly this backend, through exactly this API, in the browser process, on the same TV. What
has not been found is what triggers the first frame. LSM's `state_changed` arrives only
*after* a buffer - the working SDL sample shows `attach`, `damage`, `commit`, and only then
`state_changed(3)` and `exposed` - so the host state reading back 0 is a consequence of
having no buffer, not a cause.

**Where that leaves it.** The sample stays on `weboswayland`, which is the only backend its
API exists on. Getting a window there means finding what makes the browser-side compositor
ask the GPU for an output surface; getting one the browser's way means reconstructing
`Browser`, `content::BrowserContext` and the Views classes instead, which is a much larger
job than the embedding API this directory is about.

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
