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
| 1.x, 2.x | absent | - | no engine to link |
| 3.4 - 3.9 | yes | `WebOSMain` | libstdc++ 6.0.19: no C++11 `std::string` ABI |
| **4.x** | **yes** | **`WebOSMain`** | **this sample** |
| 5.x | yes | `webos::WebOSMain::Run` | different entry point and `Initialize` |
| 6.x - 11.x | yes | `webos::WebOSMain::Run`, plus `neva_app_runtime` | a second, parallel API |

Both bounds are ABI, not caution. Below: libcbe's own exports are mangled with
`std::__cxx11::basic_string`, but webOS 3's system libstdc++ does not export
`GLIBCXX_3.4.21`, so nothing can call this API there without statically linking a newer
runtime. Above: webOS 5 replaced the free `WebOSMain()` with a `webos::WebOSMain` class and
changed `WebViewBase`'s constructor, and webOS 6 added a whole second API under
`neva_app_runtime`. Those want their own variant rather than a wider range on this one.

The `webos::` API itself is present unchanged from 3.4 all the way to 11.2, so a webOS 5+
variant is a small delta, not a rewrite.

## Where the headers came from

Two independent sources that agree with each other:

* **Firmware symbol tables** (`dev-toolbox-cli/common/data/*/libcbe.so.json`) give every
  exported name, and therefore every signature, across every release.
* **WAM's own vtables.** `libWebAppMgr.so` contains `BlinkWebView` and
  `WebAppWaylandWindow`, the only in-firmware subclasses of these classes. Their vtables
  pin down slot order, and their constructors pin down object size - `operator new(32)`
  followed by a `BlinkWebView` that writes its first field at offset 8 says
  `webos::WebViewBase` is exactly 8 bytes: a vptr and one pimpl pointer.

Three facts in `webos/webview_base.h` are load-bearing:

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

The NDK has no libcbe, so the build makes a stand-in: `cbe_stub.cpp` is compiled into a
shared object with SONAME `libcbe.so` and nothing else in it. The loader picks up the TV's
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

Progress shows up in `/var/log/messages` under the `web-cbe` tag (`journalctl` is not on
the PATH these shells get), and the sample's own delegate output goes to stdout.

To see what actually reached the screen - LSM will happily report a foreground surface that
drew nothing:

```sh
ares-shell -d <device> -r "luna-send -n 1 -w 10000 -f \
    luna://com.webos.service.tv.capture/executeOneShot \
    '{\"path\":\"/tmp/shot.png\",\"method\":\"DISPLAY\",\"format\":\"PNG\"}'"
ares-pull -d <device> /tmp/shot.png .
```

## What is not here

* **Input.** The sample never calls `ForwardWebOSEvent()`, and whether remote-control keys
  reach the page on their own has not been tested.
* **Lifecycle.** No SAM relaunch/close handling, no `PalmSystem` bridge, no suspend on
  background - all of which is most of what WAM actually does.
* **webOS 5+.** See the table above.
