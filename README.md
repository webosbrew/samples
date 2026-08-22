# webOS homebrew samples

Small, readable sample apps for LG webOS native homebrew. Each one does a single thing and
is meant to be read top to bottom, without a framework in the way.

Most of them are media samples: they feed raw elementary streams straight into the TV's
hardware decoder, with SDL2 owning the window and the remote control. There are three
entirely different ways to reach that decoder, and all of them get samples:
**starfish-media-pipeline** (`libplayerAPIs`), **NDL**, and **LGNC** (the LG NetCast Open
API).

The `web/` samples are the odd ones out. They link `libcbe.so` - the TV's own Chromium, the
engine every web app on the box already runs inside - and put a real web view in a *native*
app, with no WAM and no web app package. There is no SDK for that library; the headers were
reconstructed from firmware symbol tables and from the vtables of WAM's own subclasses, and
`web/cbe/README.md` writes down how. `web/hybrid` then runs SDL2 and a web view in one
process and switches between them, which is the awkward case: `WebOSMain()` never returns,
so SDL has to be pumped from Chromium's message loop rather than its own.

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
    directmedia/   libNDL_directmedia, NDL_Direct* - webOS 3.5+, built for API v1 and v2
  lgnc/            liblgncopenapi, LGNC_DIRECT* - webOS 1 to 4, one binary for all of them
web/
  libcbe/          reconstructed libcbe headers and the link stub, shared by both
  cbe/             the smallest thing that puts a web page on screen
  hybrid/          SDL2 and a web view in one process, swapping which one is shown
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
| timestamps | yes, nanoseconds | Esplayer 90 kHz ticks; DirectMedia v1 none, v2 microseconds | **none at all** |
| video plane | your problem - ACB, luna, or an exported window | owned by the API | owned by the API |
| end of stream | `pushEOS()` | in-band, an EOS-flagged empty buffer | no signal |
| build variants needed | 4 | 1 per API generation | none |
| in the SDK | yes | only the 3.5+ half | yes |

LGNC is the smallest API of the three and the only one that claims to reach webOS 1 - but
it hands back no timing information whatsoever, so keeping audio and video together is
entirely the caller's job.

Both ends of that range deserve a caveat, for the same reason. The upper one is known
wrong-if-trusted; the lower one is simply untested. Every symbol the sample uses is present
in a webOS 1.2 dump, but no webOS 1 hardware was available, and this repo has already been
bitten once by treating symbol presence as proof. Read "webOS 1" as *should work*, and
"webOS 2 to 4" as *seen working*.

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

### Icons

`assets/icons/<name>.png` carries white artwork on transparency - the API name, a mark (a
play triangle for the media samples, a globe for the web ones), and which variant it is -
and no colour at all. Each sample passes a Material 500 colour to `webos_add_ipk`, which
writes it to both `iconColor` and `bgColor` in `appinfo.json`, and webOS paints that behind
the glyph. Families are grouped by hue: blues for starfish, greens for NDL, orange for
LGNC, blue and purple for the web samples.

Both fields are set deliberately: `bgColor` is the tile background, while `iconColor` fills
behind the icon itself - without it the launcher's default shows through the glyph's
transparency. `assets/make-icon.py` regenerates the PNG, which is committed so the build
needs no Python.

**Changing an icon needs more than a reinstall.** The launcher caches the rendered tile
against the app id *and version*, and that cache survives both `ares-install` over the top
and a `restart sam`. An app will happily keep showing its old icon while the `icon.png`
inside its directory is provably the new one. Either bump the version in
`webos_add_ipk`, or uninstall before installing:

```sh
ares-install -d <device> -r <appid> && ares-install -d <device> <ipk>
```

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

The bundled dumps also stop at the 2022 models. Anything newer - `media/smp/webos5` was
tested on a 2025 OLED running webOS 10.3.1 - is outside what `-verify` can say anything
about, and rests on the device test alone.

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
| `media/smp/acb` (webos3) | 3.x | verified on a 43UH6100 (webOS 3.4.0), but that run predates the ordering fixes below - re-run pending |
| `media/ndl/esplayer` | 2.x - 3.4 | **verified on hardware** - 55LF6310 (webOS 2.2.0) and 43UH6100 (webOS 3.4.0): `FIRST_FRAME_PRESENTED` and the full 300 / 470 on both, confirmed smooth on screen |
| `media/lgnc` | 1 - 4 (1.x unverified) | **verified on hardware** - 55LF6310 (webOS 2.2.0) and 43UH6100 (webOS 3.4.0): both decoders opened and the full 300 / 470, confirmed smooth on screen. Capped at 4: webOS 5 links but does not play |
| `media/smp/acb` (webos2) | 2.x | **verified on hardware** - 55LF6310, webOS 2.2.0: the legacy `std::string` ABI and 3-argument `Load` both work, full 300 / 470, ACB reaching PLAYING before playback, and confirmed playing through on screen |
| `media/smp/acb` (webos4) | 4.x | **verified on hardware** - 49LK5900, webOS 4.4.0: `PLAYING`, `FRAMEREADY` and end of stream, full 300 / 470 |
| `media/smp/webos5` | 5+ | **verified on hardware** - 65UP7560 (webOS 6.5.2) and OLED77C5 (webOS 10.3.1): exported window accepted, full load / play / feed / EOS / unload, 300 video + 470 audio units on both. Those runs predate the `Play()` ordering fix, which all SMP samples share - re-run pending |
| `media/ndl/directmedia` (v2) | 5+ | **verified on hardware** - 65UP7560 (webOS 6.5.2) and OLED77C5 (webOS 10.3.1): 300 video + 469 PCM chunks on both |
| `media/ndl/directmedia` (v1) | 3.5 - 4.x | built and symbol-verified, needs a 2017-2019 set to test |
| `web/hybrid` | 4.0 - 4.9 | **verified on hardware** - 49LK5900, webOS 4.4.3: SDL2 and the web view both render full-screen from one process, switching works both ways, and the page's exit button reaches native code as `WebViewDelegate::Close()`. The transitions were driven by a test hook, not by the remote - key delivery is untested |
| `web/cbe` | 4.0 - 4.9 | **verified on hardware** - 49LK5900, webOS 4.4.3: the window registers with LSM and SAM as the foreground card, and a display capture shows the page rendered full-screen at 1920x1080. Input and lifecycle are not implemented |
| `media/smp/webos1` | 1.x | not written yet - and there is no webOS 1 hardware here to validate it against, so it would ship untestable |

## What a `Play()` that returns true does not tell you

`media-smp-acb-webos4` spent a long time showing nothing but a frozen first frame while its
own logs looked perfect, and the reason is worth keeping.

`Play()` issued before `LOADCOMPLETED` is **queued, not executed** - the pipeline logs
`prerolling state (play command pending)` - and it returns `true` anyway. So a player that
calls `Play()` when the first buffer is accepted (which webOS 2.2 requires, because its
`LOADCOMPLETED` does not arrive until feeding stops) gets a truthful-looking success and a
pipeline that never leaves `LoadingState`.

The fix is to re-issue `Play()` at `LOADCOMPLETED` unconditionally, even when an earlier
call was accepted. Playing an already-playing pipeline is harmless; skipping the one moment
the pipeline is definitely ready is not.

The only trustworthy confirmation that playback started is the
`STATE_UPDATE__PLAYING` event, and `FRAMEREADY` arriving. The samples log both. Several
things that looked diagnostic are not, each disproved with a control rather than an
argument:

- **`checkAppSrcBuffer: not Play State`** - YouTube prints it continuously while playing
  perfectly on the same TV.
- **`surface-manager LSM: client doesn't have enough permission`** - also present during a
  working NDL run.
- **`Play()` returning true** - as above.

## The TV tells you which Load options it understands

`libpf` (player-factory) contains the full list of option paths it parses, so there is no
need to guess whether a payload field does anything:

```sh
strings /usr/lib/libpf-1.0.so.1.0.0 | grep '^option\.' | sort
```

On a webOS 4.4 set that prints about a hundred paths - the whole `externalStreamingInfo`
tree, `adaptiveStreaming`, `transmission`, `drm`, and a `bufferControl` block that is not
in any of the reverse-engineered notes:

```
option.bufferControl.preBufferTime
option.bufferControl.bufferingMinTime
option.bufferControl.bufferingMaxTime
option.bufferControl.userBufferCtrl
```

It is equally useful for what is *absent*. `lowDelayMode` does not appear at all on
webOS 4.4, and neither does `WEBRTC` for `transmission.contentsType` to match, so both are
silently ignored there - measuring startup with and without them gave the same 950 ms
either way. Only the `srcBufferLevel*` limits are recognised across every generation.

The same library also explains why the startup wait has a floor:

```
preBufferTime < 3 secs use default preBufferTime
```

Asking for less than three seconds gets you the default, so that particular wait cannot be
tuned away from the payload.

### Where the startup time actually goes

The samples timestamp their milestones, because "how long until a picture appears" is not
the same question as "how late is a frame against its own timestamp" - and the second one
is easy to mistake for the first:

```
[main @68ms]  SDL window up
[main @102ms] calling Load
[smp @352ms]  Play() -> accepted
[smp @685ms]  load completed
[smp @950ms]  first picture on screen, its pts 0.13s
```

Most of it is pipeline creation and resource acquisition between `Load` and `load
completed` - roughly 580 ms on a 49LK5900 - which no payload field shortens.

## Debugging on a device

Two things make a native webOS app hard to debug, and both are handled by the samples:

**There is no console.** stdout and stderr of an app-manager-launched native app go
nowhere. Every sample takes a log path in its launch parameters:

```sh
ares-launch -d <device> org.webosbrew.sample.media.smp.acb.webos3 -p '{"log":"/tmp/smp.log"}'
ssh root@<device> cat /tmp/smp.log
```

**A killed run poisons the next one.** ACB registrations outlive the process. If an app is
killed rather than exiting cleanly, the next run is refused with
`ACB object was already registered ...` in `acb-client.error`, and shows a black screen or
a frozen frame while its own logs look perfect. Recover with `restart AcbService` on the TV
- a reboot works too but is not necessary. The samples install a SIGTERM/SIGINT handler to
avoid leaking in the first place.

**PmLog contexts are per-process.** Levels set with `PmLogCtl` apply to processes that
register them *afterwards*, so raising a level and then relaunching the app is required -
and a level raised for one run will not necessarily be in force for the next.

**The pipeline logs separately.** `libplayerAPIs` logs through PmLog, off by default. Turn
it up *while the app is running* - contexts are registered by the live process, so setting
them beforehand does not stick:

```sh
for c in smp.api smp.main smp.rm playerfactory.default playerfactory.feed \
         acb-client.error acb-client.info acb-svc.info NEWVSM; do PmLogCtl set $c debug; done
tail -f /var/log/messages | grep -v updatePeriodicalInfo
```

`acb-client.error` in particular is worth having on: it is the only place the ACB service
explains a refusal. That is how the registration leak above was found.

That is how the load deadlock described below was found: the pipeline sat in `LoadingState`
with `setupPlayback set paused done`, waiting for buffers that the app was withholding.

## Things that cost time, written down

- **Feed before LOADCOMPLETED, not after.** The pipeline prerolls on the first buffers and
  only *then* reports `LOADCOMPLETED`. Gating the first `Feed()` on that event deadlocks.
  Feeding is gated on `Load()` having returned true instead.
- **`pushEOS()` before waiting for `ENDOFSTREAM`.** The pipeline has no other way to know
  the stream ended, so the wait otherwise always times out and the tail is cut off.
- **Do not hang `Play()` off `LOADCOMPLETED` either.** Same root cause as the ACB note
  below, worse symptom: with `esInfo.pauseAtDecodeTime` set, the pipeline shows the first
  picture and holds it, so a player that waits for the event on webOS 2.2 displays exactly
  one frame for the whole clip and looks like it failed to decode. `Play()` now goes out on
  whichever comes first, the event or the first buffer the pipeline accepts.
- **Test streams must have no B-frames.** A raw elementary stream carries no timing, so
  these samples synthesise a timestamp from a frame counter - which is only a *presentation*
  timestamp when decode order and presentation order agree. With B-frames the counter is
  really a decode timestamp, and any decoder that displays in the order it is fed shows
  frames jumping backwards. LGNC has no timestamp parameter at all, so it is where this
  shows up first; `make-sample.sh` passes `bframes=0`.
- **Do not hang the ACB setup off `LOADCOMPLETED`.** On webOS 3.4 that event arrives
  promptly, but on webOS 2.2 it does not arrive *until feeding stops* - so a sample that
  waits for it attaches the video sink and places the window only after the clip has
  already played, and nothing is ever shown. `Load()` returning true is enough: the
  pipeline exists and has a media id, which is all ACB needs.
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
- **Direct audio cannot decode compressed formats on some SoCs, and no jail will help.**
  On an MStar `m3` set (49LK5900, webOS 4.4) feeding AAC or AC-3 to NDL direct audio kills
  the app inside LG's own HAL - `HAL_AUDIO_DIRECT_Write` dereferences a null `pES3BufInfo`,
  the elementary-stream buffer, because `/dev/adsp`, `/dev/audio` and `/dev/dsp` are not
  visible inside the jail. PCM works, because it never touches the DSP.

  Worth stating plainly, because the obvious next move is to hunt for a more privileged
  jail: **all eleven templates in `/etc/jail_*.conf` expose zero DSP nodes** - `native`,
  `native_builtin`, `native_game`, `native_mvpd`, `triton`, the lot. The dev-mode jail is
  actually the most permissive of them, being the only one with `/dev/ion` and
  `/dev/cmapool`; forcing `jailer -t native` drops those and breaks the *video* decoder as
  well. A store-shipped app would hit the same wall. Compressed audio on this SoC is the
  media pipeline's job, which is why the starfish samples play AAC on the very same TV.
- **NDL DirectMedia v2 has no AAC.** Its audio types are PCM, MP3 and Opus only, so the v2
  build plays `sample.pcm` where v1 plays `sample.aac`. Opus would be far smaller on disk
  but needs Ogg pages unpacked to recover packet boundaries - demuxing, which these samples
  deliberately avoid.
- **Neither DirectMedia version can be told the stream ended.** v2's callback does define
  an end-of-stream event, but with a buffer-stream source nothing triggers it: on webOS
  10.3.1 the callback reports load-completed and playing, then nothing until unload. Both
  builds finish on a drain timeout, which is only safe because the feed loop paced in real
  time.
- **Esplayer timestamps are 90 kHz ticks, not nanoseconds.** `NDL_EsplayerLoadEx` would let
  you ask for microseconds, but it is not exported by the library on these TVs - only the
  22 functions in webos-userland's linker script are - so the default `NDL_ESP_PTS_TICKS`
  is all there is.
- **A clean symbol check does not mean the API works.** LGNC exports its full surface on
  webOS 5 and is broken there anyway. The sample's upper bound comes from testing on real
  hardware; the symbol dumps would have happily let it claim webOS 5. (A webOS 6.5.2 set
  has no `liblgncopenapi` at all - nor `libAcbAPI` nor `libndl-directmedia2` - which is a
  neat independent check on where each of these APIs stops.)
- **LGNC places the video window in a fixed 1920x1080 space**, not in panel pixels.
  `_LGNC_DIRECTVIDEO_SetDisplayWindow` on a 4K set still wants 1920x1080 coordinates;
  passing real pixel sizes puts the picture in a corner.
- **The TV's own logging splices itself into yours**, mid-line and without a newline, so
  grepping a log for `^[tag]` quietly misses entries. LGNC is the worst offender but it is
  not alone - a webOS 6 set did it to the starfish sample too. Grep for the tag unanchored.
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
