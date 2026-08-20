# webOS homebrew samples

Small, readable sample apps for LG webOS native homebrew. Each one does a single thing and
is meant to be read top to bottom, without a framework in the way.

The media samples feed raw elementary streams straight into the TV's hardware decoder, with
SDL2 owning the window and the remote control. There are two entirely different ways to
reach that decoder, and all of them get samples: **starfish-media-pipeline**
(`libplayerAPIs`), **NDL**, and **LGNC** (the LG NetCast Open API).

## What is here

```
media/
  common/          elementary-stream readers, pacing, JSON helpers, the SDL shell
  smp/             starfish-media-pipeline samples
    common/        the pipeline lifecycle, built once per webOS ABI generation
    acb/           libAcbAPI video plane - webOS 2 to 4.x
    webos5/        SDL exported window - webOS 5 and newer
  ndl/             NDL samples
    esplayer/      libndl-directmedia2, NDL_Esplayer* - webOS 2.x to 3.4
  lgnc/            liblgncopenapi, LGNC_DIRECT* - webOS 1 to 4, one binary for all of them
```

The same two files play through all three stacks. Comparing the three `main.c` files is the
quickest way to see the difference: the feed loop is identical in each, everything around
it is not.

### Which API?

| | starfish-media-pipeline | NDL | LGNC |
|---|---|---|---|
| library | `libplayerAPIs` | `libndl-directmedia2` (2.x-3.4), `libNDL_directmedia` (3.5+) | `liblgncopenapi` |
| covers | all generations | 2.x onwards | webOS 1 - 4 (see below) |
| entry point | C++ class, JSON payloads | plain C, structs | plain C, structs |
| configuration | one big `Load()` JSON document | a metadata struct | one struct per decoder |
| buffers | pointer formatted into a JSON string | a struct with a real pointer | pointer + length |
| timestamps | yes, nanoseconds | yes, 90 kHz ticks | **none at all** |
| video plane | your problem - ACB, luna, or an exported window | owned by the API | owned by the API |
| end of stream | `pushEOS()` | in-band, an EOS-flagged empty buffer | no signal |
| build variants needed | 4 | 1 per API generation | none |
| in the SDK | yes | only the 3.5+ half | yes |

LGNC is the smallest API of the three and the only one that reaches webOS 1 - but it hands
back no timing information whatsoever, so keeping audio and video together is entirely the
caller's job.

Its upper bound is a trap. webOS 5 still exports the entire LGNC API, so a build targeting
it links and passes `-verify` - but playback is broken there in practice. The library is
only actually *gone* in webOS 6. This is the one case in the repo where the firmware symbol
tables are not the final word, so the sample is capped at webOS 4 on the strength of
testing rather than of symbols.

`media/common` never touches a webOS media API, so it builds and runs on a normal Linux
host. That is what makes the elementary-stream parsers testable without a TV.

## Why there is more than one build of the same code

`StarfishMediaAPIs` is a C++ class, and its ABI is not stable across webOS generations.
Diffing the exported symbols of `libplayerAPIs.so.1.0.0` across retail firmware gives:

| webOS | `std::string` ABI | constructor | `Load()` with user pointer | `notifyForeground` | video plane |
|---|---|---|---|---|---|
| 1 (2014) | pre-C++11 | `StarfishMediaAPIs()` | no | no | luna `videosinkmanager` + `tv.display` |
| 2 (2015) | pre-C++11 | `(const char *uid)` | no | yes | ACB |
| 3 (2016-17) | pre-C++11 | `(const char *uid)` | yes | yes | ACB |
| 4 (2018-19) | `__cxx11` | `(const char *uid)` | yes | yes | ACB |
| 5+ (2020-) | `__cxx11` | `(const char *uid)` | yes | yes | SDL exported window |

Note that the two axes do not line up: the string ABI changes between webOS 3 and 4, the
video plane changes between 4 and 5. So the *video plane* gets a directory each, and the
*ABI* differences are build variants of `media/smp/common`:

| variant | `std::string` ABI | `Load()` | constructor | for |
|---|---|---|---|---|
| `media-smp-common-modern` | `__cxx11` | 4-arg | `(const char *)` | webOS 4, 5+ |
| `media-smp-common-legacy` | pre-C++11 | 4-arg | `(const char *)` | webOS 3 |
| `media-smp-common-legacy-nouserdata` | pre-C++11 | 3-arg | `(const char *)` | webOS 2 |
| `media-smp-common-webos1` | pre-C++11 | 3-arg | `()` | webOS 1 |

Getting this wrong does not fail to build - the SDK's `libplayerAPIs.so` is a link-time
stub that exports every symbol at a single address. It fails on the TV. That is what the
`-verify` targets below are for.

## Building

Needs the [openlgtv buildroot NDK](https://github.com/openlgtv/buildroot-nc4):

```sh
cmake -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=/opt/arm-webos-linux-gnueabi_sdk-buildroot/share/buildroot/toolchainfile.cmake
cmake --build build
```

Configuring without a webOS toolchain builds `media/common` and its test tool for the host,
and skips everything else.

### Sample streams

No media is committed. Generate the two elementary streams from any video file:

```sh
./assets/make-sample.sh /path/to/anything.mp4
```

That writes `assets/sample.h264` (Annex-B) and `assets/sample.aac` (ADTS), which the ipk
targets stage next to the binary. Point the build somewhere else with
`-DSAMPLE_MEDIA_DIR=/path/to/streams`.

### Per-sample targets

```sh
cmake --build build --target media-smp-webos5-ipk      # -> build/dist/*.ipk
cmake --build build --target media-smp-webos5-install  # ares-install, honours $ARES_DEVICE
cmake --build build --target media-smp-webos5-verify   # symbol check against firmware dumps
```

`-verify` runs `webosbrew-ipk-verify` against the bundled firmware symbol dumps and reports
anything the ipk needs that the TV does not export, scoped to the webOS releases the
variant actually targets. It is the only way to catch an ABI mistake before installing.

It proves the binary will *load*, and nothing more. An API can export every symbol and
still not work - LGNC on webOS 5 is exactly that - so a clean `-verify` is a precondition
for testing on a device, not a substitute for it.

### Checking the stream parsers without a TV

```sh
cmake -B build/host -G Ninja && cmake --build build/host
./build/host/media/common/es-dump h264 assets/sample.h264 30
./build/host/media/common/es-dump adts assets/sample.aac
```

The unit counts should match `ffprobe -count_frames`. If they do not, the access-unit
grouping is wrong and the pipeline will receive malformed buffers - a failure that is very
hard to diagnose from the TV side.

## Status

| sample | webOS | state |
|---|---|---|
| `media/smp/acb` (webos3) | 3.x | **verified on hardware** - 43UH6100, webOS 3.4.0: full load / play / feed / EOS / unload, 300 video + 470 audio units at real-time pace |
| `media/ndl/esplayer` | 2.x - 3.4 | **verified on hardware** - same TV, same clip, `FIRST_FRAME_PRESENTED` and the full 300 / 470 |
| `media/lgnc` | 1 - 4 | **verified on hardware** - same TV, same clip, both decoders opened and the full 300 / 470. Capped at 4: webOS 5 links but does not play |
| `media/smp/acb` (webos2, webos4) | 2.x, 4.x | built and symbol-verified, not yet run on a device |
| `media/smp/webos5` | 5+ | built and symbol-verified, not yet run on a device |
| `media/ndl/directmedia` | 3.5+ | not written yet |
| `media/smp/webos1` | 1.x | not written yet |

## Debugging on a device

Two things make a native webOS app hard to debug, and both are handled by the samples:

**There is no console.** stdout and stderr of an app-manager-launched native app go
nowhere. Every sample takes a log path in its launch parameters:

```sh
ares-launch -d <device> org.webosbrew.sample.media.smp.acb.webos3 -p '{"log":"/tmp/smp.log"}'
ssh root@<device> cat /tmp/smp.log
```

**The pipeline logs separately.** `libplayerAPIs` logs through PmLog, off by default. Turn
it up *while the app is running* - contexts are registered by the live process, so setting
them beforehand does not stick:

```sh
for c in smp.api smp.main smp.rm playerfactory.default; do PmLogCtl set $c debug; done
tail -f /var/log/messages | grep -v updatePeriodicalInfo
```

That is how the load deadlock described below was found: the pipeline sat in `LoadingState`
with `setupPlayback set paused done`, waiting for buffers that the app was withholding.

## Things that cost time, written down

- **Feed before LOADCOMPLETED, not after.** The pipeline prerolls on the first buffers and
  only *then* reports `LOADCOMPLETED`. Gating the first `Feed()` on that event deadlocks.
  Feeding is gated on `Load()` having returned true instead.
- **`pushEOS()` before waiting for `ENDOFSTREAM`.** The pipeline has no other way to know
  the stream ended, so the wait otherwise always times out and the tail is cut off.
- **ACB wants `LOADED` before `PLAYING`.** The triggers arrive out of order and on
  different threads - the first buffer is accepted before `LOADCOMPLETED` - so the ACB
  plane holds `PLAYING` back. Get it wrong and ACB answers
  `{"errorCode":10,"errorText":"Invalid State Request"}`.
- **Do not bundle SDL2.** Every TV ships one. The NDK's much newer SDL expects a newer
  libwayland than older firmware has, so a vendored copy links fine and then fails at
  `SDL_INIT_VIDEO` with "wayland not available". Link the system SDL and let
  `webosbrew-ipk-verify` prove the symbols exist - that is how the one missing symbol
  (`SDL_webOSCursorVisibility`, absent before webOS 4) was found and weak-linked.
- **argv[1] is a JSON object.** The app manager passes launch parameters positionally. An
  option parser that rejects unknown arguments will exit before it can say why.
- **Esplayer never reports `NDL_ESP_END_OF_STREAM` on webOS 3.4.** The EOS buffers are
  accepted (`NDL_EsplayerFeedData` returns 0) and the decoder goes quiet after
  `STREAM_DRAINED_VIDEO`, but the documented event does not arrive. Since the feed loop
  paces in real time, the drain wait is insurance and timing out is the normal exit.
- **Esplayer timestamps are 90 kHz ticks, not nanoseconds.** `NDL_EsplayerLoadEx` would let
  you ask for microseconds, but it is not exported by the library on these TVs - only the
  22 functions in webos-userland's linker script are - so the default `NDL_ESP_PTS_TICKS`
  is all there is.
- **A clean symbol check does not mean the API works.** LGNC exports its full surface on
  webOS 5 and is broken there anyway. The sample's upper bound comes from testing on real
  hardware; the symbol dumps would have happily let it claim webOS 5.
- **LGNC places the video window in a fixed 1920x1080 space**, not in panel pixels.
  `_LGNC_DIRECTVIDEO_SetDisplayWindow` on a 4K set still wants 1920x1080 coordinates;
  passing real pixel sizes puts the picture in a corner.
- **LGNC writes to stdout/stderr without line discipline.** Its own logging will splice
  itself into the middle of yours, so grepping the log for `^[lgnc]` quietly misses lines.
- **The SDK's `AcbAPI.h` is wrong.** It declares `AcbAPI_setMediaVideoData(long, const char *)`.
  The shipped library takes a third `long *taskId`, which the exported C++ symbol spells
  out: `_ZN3ACB7AcbCore17setMediaVideoDataESsPl`.

## References

- [ss4s](https://github.com/mariotaku/ss4s) - the production version of this, with a
  plugin architecture these samples deliberately do not have
- [webos-userland](https://github.com/webosbrew/webos-userland) - headers and link stubs
  for the closed webOS libraries
- [SDL-webOS](https://github.com/webosbrew/SDL-webOS) - the SDL2 fork with the
  exported-window API

`libndl-directmedia2` is not in the NDK, so its headers and link stub are fetched from
webos-userland at configure time (`cmake/WebOSUserland.cmake`). Nothing is vendored into
this repo, and nothing is bundled into the ipk - the real library is already on the TV.
