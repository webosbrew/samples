# neva_app_runtime

The same web view as `../cbe-webos6`, through the other API in the same library.

**Verified on a 65UP7560 (starfish 6.5.2):** the page loads and renders full-screen and the
app takes the foreground. `-verify` is clean from webOS 6.4 to 11.2.

## Why this one is the better starting point

webOS 6 ships two embedding APIs in one `libcbe.so`: the `webos::` one WAM still uses, and
`neva_app_runtime`, whose headers LG **published** - `webosose/chromium87`,
`src/neva/app_runtime/public`. That makes this the only sample under `web/` that starts
from source rather than from a vtable.

It is smaller and simpler, too:

| | `webos::` (webOS 6) | `neva_app_runtime` |
|---|---|---|
| entry point | `webos::WebOSMain(delegate).Run(...)` | `AppRuntimeMain(argc, argv)` |
| view setup | constructor **and** a 10-argument `Initialize` | constructor only |
| delegate slots | 61 | 40 |
| `sizeof(WebViewBase)` | 92 | 16 |
| window sizing | `InitWindow(w, h)` | `Resize(w, h)` |

## The published headers are a starting point, not the answer

The TV is Chromium **79** - between the public `chromium68` and `chromium87` trees - and its
vtable has **40** slots where chromium87 declares 32 virtuals and chromium68 declares 26. So
the upstream order still has to be checked.

What checks it, without relying on names at all, is which slots are **pure virtual**. libcbe's
own vtable says: a pure slot relocates to `__cxa_pure_virtual`, an implemented one to real
code. Lining that up against the header's `= 0` versus `{}`:

```
firmware  pure at: 0-15, 18, 19, 20, 21, 29
upstream  pure at: 0-15, 18, 19, 20, 21
```

Identical through slot 28. That is a strong enough signal to take slots 0-28 as upstream's,
in upstream's order; 29-39 are LG additions and stay placeholders.

## Two traps, both worth knowing

**`AppRuntimeMain` is not `extern "C"`.** libcbe exports it C++-mangled, as
`_Z14AppRuntimeMainiPPKc`. Declaring it `extern "C"` looks for a plain `AppRuntimeMain` that
does not exist, and `-verify` catches it.

**Object sizes are load-bearing.** libcbe's constructors write to offset 12
(`WebViewBase`) and 4 (`WebAppWindowBase`), so those classes are 16 and 8 bytes. A subclass
declared smaller lets libcbe write past the allocation, and the process dies later inside
`malloc` with a backtrace nowhere near the cause. `../cbe-webos6/README.md` has the longer
version of that story, along with the slot-tracer trick for finding a stubbed slot that was
supposed to return a pointer.
