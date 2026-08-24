# web/cbe, on webOS 6 and newer

> **Status: unfinished.** The ABI is mapped and the sample builds, verifies clean and
> starts - it reaches libcbe's delegate - but the delegate arrives with mis-decoded
> arguments and the process then dies. The slot layout below is right in outline and wrong
> in some detail. Everything established so far is written down here; `web/cbe` on webOS 4
> is the one that works.

Measured on a **65UP7560 running starfish 6.5.2** (Chromium 79).

## A third shape of the same API

`webos::WebViewBase` exists from webOS 3 to 11.2, but it is not one API. This generation
differs from webOS 4 in every part that the calling code touches:

| | webOS 4 | webOS 6+ |
|---|---|---|
| entry point | `WebOSMain(argc, argv)`, a free function | `webos::WebOSMain(delegate).Run(argc, argv)` |
| view constructor | `WebViewBase()` | `WebViewBase(bool, int, int)` |
| `Initialize` | 5 strings + `int, int, bool` | see below |
| delegate slots | 24 | **61** |
| `sizeof(WebViewBase)` | 8 | **92** |
| `sizeof(WebAppWindowBase)` | 8 | 8 |

**The object size is load-bearing.** libcbe's `WebViewBase` constructor writes as far as
offset 90, and WAM's `BlinkWebView` allocates 116 bytes with its own fields starting at 92 -
so the base is 92. Declare a subclass any smaller and libcbe writes past the allocation; the
process then dies inside `malloc` much later, nowhere near the cause. That is what the
`reserved_` member is for, and why it is not cosmetic.

## `Initialize` has two overloads, and they split the range

| release | 10-arg `Initialize` | 6-arg | constructor |
|---|---|---|---|
| 5.3.1 | yes | no | `WebViewBase(int, int)` |
| **6.4** | **yes** | **yes** | `WebViewBase(bool, int, int)` |
| 7.4 - 11.2 | no | yes | `WebViewBase(bool, int, int)` |

webOS 6 is a transition release carrying both. This sample uses the 10-argument form,
because that is the one WAM calls at all three of its call sites on 6.5 - which caps it at
webOS 6. A 7-and-up variant is the same code with the 6-argument form, and that form does
verify clean all the way to 11.2.

## Where the layout came from

WAM still subclasses the `webos::` API on this generation - `libWebAppMgr.so` makes 122
calls into it and carries both `BlinkWebView` and `WebAppWaylandWindow` - so the same
technique as webOS 4 applies, and `BlinkWebView`'s vtable gives all 61 slots with names for
most. Slots WAM fills with local functions are `UnknownNN` placeholders: they must exist and
hold their position, but their signatures are not recoverable from the vtable alone.

`webos::WebAppWindowBase`'s vtable is more interesting - it mixes `webos::` methods with
`neva_app_runtime::WebAppWindowDelegate` ones, so by webOS 6 the old API sits *on top of*
the new runtime rather than beside it.

### `neva_app_runtime` is not the shortcut it looks like

It is the API with public upstream headers (`webosose/chromium87`,
`src/neva/app_runtime/public`), and webOS 6.5 exports 218 of its symbols. But:

* **nothing in the firmware uses it** - WAM is on `webos::`, so there is no reference
  implementation and no vtable to recover names from;
* **the upstream headers do not match.** LG's build is Chromium 79, between the public
  chromium68 and chromium87 trees, and its `WebViewBase` vtable has **40** slots where
  chromium87 declares 32 virtuals and chromium68 declares 26. So the headers are a naming
  reference, not the layout - the same position as `webos::`, but without WAM to check
  against.

That is why this sample stays on `webos::`.

## What is still wrong

The delegate is reached - `DidStartNavigation` fires - but its `const std::string&` arrives
as garbage, so a slot is misaligned or a signature is wrong somewhere in the first 17. The
base `webos::WebViewBase` vtable counts 51 relocated slots against `BlinkWebView`'s 61,
which is the obvious place to start: confirm the 51 really are a prefix of the 61, and that
no thunk entries shift the numbering.
