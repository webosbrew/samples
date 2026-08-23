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

The live lead is registration. libcbe contains
`palm://com.webos.applicationManager/registerNativeApp` and a `webos::LunaServices` class
whose `Initialize(const base::FilePath&)` is an instance method needing a
`webos::LunaServices(webos::Platform*)` - and `Platform` is the browser application's layer,
built by `ChromeMain` rather than by `WebOSMain`. So on this generation the Luna
registration that a native app needs may simply live on the browser's side of the library
and not the embedder's. That is a hypothesis, not a finding.

So this is honest work-in-progress. The hard half - the ABI - is done and demonstrated. The
window handover is not, and the next person should start at `LunaServices`.
