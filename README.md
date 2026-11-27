# inFAMOUS Second Son for macOS (Apple silicon)

A macOS fork of [HayatoG/iss_pc](https://github.com/HayatoG/iss_pc), the native Linux/Windows port of
inFAMOUS Second Son (PS4, CUSA00223 / CUSA00004, update 1.07). It runs **your own** copy of the game on
an Apple silicon Mac: Rosetta 2 runs the game's x86-64 code, and the graphics go through Vulkan on
Metal via MoltenVK.

> **No game files are included.** You need your own copy of the game, dumped from your own PS4.

> **Status: work in progress, tested up to the area after the graffiti tutorial.** On a MacBook Air M5
> (16 GB, macOS 26) the game boots, shows the title screen and reaches gameplay with keyboard and
> mouse. At the [recommended settings](#graphics-presets-macbook-air-m5) (35% internal resolution,
> FSR 1, quality level 8, 30 FPS cap) it held **30 FPS** in every window of the last 5 minutes of a
> 7-minute test, with dips to 22–27 FPS in the heaviest scene and while shaders compile. At the full
> 1080p it runs at 12–20 FPS. The GPU hang after the graffiti autosave is fixed. Speed (the GPU is the
> limit) is the main open problem. See [Status](#status) and [Known issues](#known-issues).

The upstream project's original README (Linux/Windows) is kept as [README-UPSTREAM.md](README-UPSTREAM.md).

## Contents

- [Status](#status)
- [Requirements](#requirements)
- [Build](#build)
- [Run](#run)
- [Controls (keyboard and mouse)](#controls-keyboard-and-mouse)
- [Graphics presets (MacBook Air M5)](#graphics-presets-macbook-air-m5)
- [Internal resolution](#internal-resolution)
- [Patched MoltenVK (optional)](#patched-moltenvk-optional)
- [Game data](#game-data)
- [Mods / Cheats](#mods--cheats)
- [How it differs from the Linux/Windows port](#how-it-differs-from-the-linuxwindows-port)
- [Known issues](#known-issues)
- [Changes that affect all platforms (for upstream)](#changes-that-affect-all-platforms-for-upstream)
- [Credits and license](#credits-and-license)

## Status

Tested on one Mac: MacBook Air M5, 16 GB, macOS 26, EU dump CUSA00004 with update 1.07.

| Area | State |
|---|---|
| Build (x86_64, run by Rosetta 2) | ✅ `port/build.sh` builds; `--test` runs the unit tests, on macOS also the native-resolution tests and the GPU library's tests (all pass; `orb-multiplier-test` is left out: it does not link) |
| Boot, title screen | ✅ |
| Gameplay (keyboard and mouse) | ✅ reached, through the graffiti tutorial |
| After the graffiti autosave | ✅ plays on (GPU hang fixed; tested 120 s with a post-graffiti save) |
| Performance | 🔧 30 FPS at the [recommended settings](#graphics-presets-macbook-air-m5) in the last 5 minutes of a 7-minute test (dips to 22–27 in the heaviest scene and while shaders compile); 12–20 FPS in gameplay at the full 1080p (was 8–12); the game's pre-rendered movies play at 60 |
| Graphics | 🔧 a flickering black square next to Delsin's face; 42 of ~25,000 shaders fail to compile |
| Controller | ⏳ not tested yet |
| Native arm64 build | ⏳ not possible without an x86-64 translator (the game's own code is x86-64) |

## Requirements

- An Apple silicon Mac with Rosetta 2: `softwareupdate --install-rosetta`.
- 16 GB of memory. The game peaked at about 7.5 GB at the recommended settings (7-minute test) and used
  about 11 GB at the full 1080p (title scene, an earlier build): quit other apps before playing.
- Xcode Command Line Tools (full Xcode only for the optional patched MoltenVK).
- Homebrew: `brew install cmake ninja pkgconf boost glslang`.
- The LunarG Vulkan SDK **1.4.363.1** for macOS (it ships MoltenVK), installed at
  `~/VulkanSDK/1.4.363.1`. `VULKAN_SDK` must point to its `macOS` folder.
- Python 3 for the launcher scripts, plus `iced-x86` (see [Build](#build)).
- Your own dump of the game with update 1.07 merged (see [Game data](#game-data)).

## Build

```bash
cd port                                     # the commands of this block run in port/
scripts/macos/deps.sh                       # once: x86_64 SDL3, FreeType, FFmpeg and GPU libraries
VULKAN_SDK=$HOME/VulkanSDK/1.4.363.1/macOS ./build.sh          # tlg-probe in port/out
VULKAN_SDK=$HOME/VulkanSDK/1.4.363.1/macOS ./build.sh --test   # also the unit tests
```

Apple silicon CPUs, like Intel ones, have no SSE4A, and Rosetta does not emulate it, so the launcher
writes a rewrite table with `scripts/isa_rewrite.py`, which needs `iced-x86`. Without it the game stops
with `STOP: this CPU lacks SSE4A and no rewrite table matches the image`. A venv inside the git-ignored
`port/out` keeps it apart from the system Python. From the repository root (after the build block
above, `cd ..` first):

```bash
python3 -m venv port/out/pyenv
port/out/pyenv/bin/python -m pip install iced-x86==1.21.0
```

## Run

The Qt launcher needs PySide6 (once, into the port's own Python; 116 MB), then runs on macOS:

```bash
port/out/pyenv/bin/pip install PySide6-Essentials==6.12.0
./infamous_launcher_qt.sh                  # also: --selftest (off screen), --play [seconds]
```

On macOS the game folder defaults to `Game/Extracted Game/CUSA00004` (or `ISS_GAME_DIR`). The
Updates screen only points at this repository: it never checks for or installs anything.

Without PySide6, use the command line (it prepares the eboot, builds the shader list and starts
the game). From the repository root:

```bash
export VULKAN_SDK=$HOME/VulkanSDK/1.4.363.1/macOS
export ISS_GAME_DIR="/path/to/CUSA00004"      # or CUSA00223
port/out/pyenv/bin/python port/launcher/infamous_run.py
```

An optional number after `infamous_run.py` stops the game after that many seconds (the probe's
`--timeout`); without it, or with 0, there is no limit.

The game's output goes to `port/out/logs/run-<date>.log` (`port/out/last_run.log` points to the latest
one); `ISS_FRAME_STATS=1` adds frame statistics to it. The launcher sets these for the game:

- `ROSETTA_ADVERTISE_AVX=1`: without it Rosetta hides AVX, and the game stops with exit code 2
  (`Run through the launcher or set ROSETTA_ADVERTISE_AVX=1`).
- `VK_ICD_FILENAMES` points to MoltenVK: the patched build in `port/out/moltenvk` if you built it,
  otherwise the SDK's (the system-wide `icd.d` can also hold KosmicKrisp, which is never picked). If you
  set `VK_ICD_FILENAMES` or `VK_DRIVER_FILES` yourself, the launcher keeps it.
- `VK_LAYER_PATH` points to the SDK's layers; `TLG_VK_VALIDATION=1` turns Vulkan validation on.

The first start compiles the game's shader pipelines behind a progress bar; later starts skip it.

### The app

`port/scripts/macos/make_app.sh` builds `port/out/inFAMOUS Second Son.app`, which opens the Qt launcher
(PySide6 required, see above) from Finder or the Dock without a Terminal window:

```bash
export VULKAN_SDK=$HOME/VulkanSDK/1.4.363.1/macOS
port/scripts/macos/make_app.sh "/path/to/CUSA00004"      # or CUSA00223
open "port/out/inFAMOUS Second Son.app"
```

- The app is a small script, not a copy of the game: it runs this checkout's `port/out/pyenv` and
  Qt launcher with `ISS_GAME_DIR` set to the folder you gave (the launcher's game folder, until you change it there). It stores that path and the checkout's
  path, never game files, so **if you move the checkout (or the game folder), build the app again**.
- `START=/path/to/script` at build time makes the app run that executable instead of the launcher
  (for your own start script, for instance one that keeps your progress apart from the test save).
- If the game stops with an error, the app shows the last lines of its output in a dialog.
- The bundle is signed ad hoc (`codesign --force --deep -s -`). `OUT=/folder` puts it somewhere else
  than `port/out`.
- The app records the checkout's path and `VULKAN_SDK`: build it again after changing either.

Measured on the M5 after `infamous_run.py --clear-shader-cache`: `Precompile: 25175 records in 112.6 s:
11328 graphics and 8052 compute pipelines built, 5753 already there, 42 failed`.

## Controls (keyboard and mouse)

The upstream controls apply (full tables in [README-UPSTREAM.md](README-UPSTREAM.md#mouse)). The ones
you need first:

| Action | Key / mouse |
|---|---|
| Confirm (Cross) | Space |
| Pause (Options) | Enter |
| Move | W A S D |
| Camera | mouse (Esc releases it, a click captures it again) |
| Use a power / shoot (R2) | left mouse button, or F |
| FPS counter | F10 |
| Port menu | Insert |
| Graffiti: hold the controller sideways | hold T (or G) |
| Graffiti: shake the can | Y while holding T, or wiggle the mouse |
| Graffiti: spray and aim | F while holding T, aim with the arrow keys |

The mouse camera works on CUSA00223 1.07 and on CUSA00004 dumps (APP_VER 01.00 or 01.07, which hold the
verified 1.07 executable).

On a non-US keyboard layout (for example French AZERTY), tell us if a key does not act where the table
says: it may be read by letter instead of by position.

## Graphics presets (MacBook Air M5)

Three sets of settings for a fanless M5 Air, from the measurements below. **Balanced is the recommended
pick.** The command-line launcher and the app have no Graphics screen: put the lines in
`port/user/tlg.ini` (the next start applies them; replace a line that sets the same key). Balanced:

```ini
iss_native_output=1
iss_native_percent=35
fsr1=1
rcas=1
quality_floor=8
fps_limit=30
```

Performance is `iss_native_output=1`, `iss_native_percent=50`, `fsr1=1`, `rcas=1`, `quality_floor=8`
and `fps_limit=0` (no cap). Battery is Balanced with `quality_floor=12`. All three keep vsync on (the
default, `vsync=1`).

| Set | FPS cap | `quality_floor` | Internal resolution | Measured |
|---|---|---|---|---|
| Balanced | 30 | 8 | 35% | 30.0 in every window of the last 5 minutes of a 7-minute run; dips to 22-27 in the heaviest scene and while shaders compile (see below); every object kept, softer |
| Performance | none | 8 | 50% | 27.4 FPS, sharper |
| Battery | 30 | 12 | 35% | level 12 kept every object at 50%; at 35% and its power use are **unverified** |

The Qt launcher's Graphics screen has the same three as presets (`M5 Air · Balanced`, `M5 Air ·
Performance`, `M5 Air · Battery`, on macOS only), but that screen was never opened on this Mac
(**unverified**). It needs PySide6 6.7 or later: `python3 -m venv .venv-qt`,
`.venv-qt/bin/pip install -r port/launcher/qt/requirements.txt`, then `./infamous_launcher_qt.sh` from
the repository root. A preset does not set the internal resolution (that lives in `tlg.ini`), so add the
two `iss_native_*` lines yourself, or pick it in the launcher's resolution control.

**Does 30 hold on a hot Mac?** One 7-minute run at the Balanced settings (35%, `fsr1=1`, `rcas=1`,
`quality_floor=8`, `fps_limit=30` in `tlg.ini`, the post-graffiti save, plugged in, window 1396×785,
no shader precompile so the shaders compiled during the run), started with
`ISS_FRAME_STATS=1 TLG_PRECOMPILE_JOBS=/nonexistent port/out/pyenv/bin/python port/launcher/infamous_run.py 420`
(83 statistics windows of about 5 s):

| Windows | FPS | Worst frame |
|---|---|---|
| 1-31 (first 2.5 minutes) | average 29.0; windows 3, 27, 28, 29, 31 at 25.6, 26.9, **22.4**, 25.1, 27.4 | up to 810 ms (window 3, 238 shader compiles); 43-106 ms in the 4,200-draw scene |
| 32-83 (last 5 minutes) | **30.0 in all 52 windows** | at most 66.6 ms |

The Mac's thermal state went from nominal to fair at window 13 (about 65 s) and never reached serious.
The heaviest scene measured (about 4,200 draws per frame, windows 25-29) stays below 30 even with the
cap. Peak `footprint_mb` was 7723 (2536 at the start), so expect about 7.5 GB of memory use.
Not measured: on battery, longer than 7 minutes, other areas.

The measurements are in [Best settings measured](#best-settings-measured-macbook-air-m5). The presets leave
`lod_scale` and `far_clip` at their defaults: they were not measured on their own, and `lod_scale=0.45`
deleted walls.
`ISS_FRAME_STATS=1` adds `thermal=` (the Mac's thermal state) and `footprint_mb=` (the game's memory)
to the frame statistics, and the log warns when the thermal state reaches serious or critical.

## Internal resolution

The game can render its 3D scene at a lower resolution and stretch it to the window. On the M5 Air,
where the GPU is the limit, this is the biggest speed setting. Add two lines to `port/user/tlg.ini`
(the next start applies them; remove both lines for the default):

```ini
iss_native_output=1
iss_native_percent=67
```

`iss_native_output=1` is a 1920×1080 output, and `iss_native_percent` is the scene's size as a percent
of it (25–200). Measured on the M5 Air in the busiest scenes after the graffiti (1396×785 window,
same scenes for every setting):

| Setting | 3D scene | FPS |
|---|---|---|
| default, or 100% | 1920×1080 | 12.5 |
| 83% | 1594×896 | 15.8 |
| 67% | 1286×724 | 19.8 |
| 50% | 960×540 | 25.5 |

- The picture gets softer as the percent drops. It is stretched with plain bilinear filtering; FSR 1
  was not tried with it (**unverified**).
- The subtitle prompt keeps its place and stays readable. It was the only HUD element on screen in
  this test, so the rest of the HUD (minimap, health, powers) is **unverified**, as are fullscreen
  and other areas.
- It works on CUSA00223 1.07 and on CUSA00004 dumps whose `param.sfo` still says 01.00: those hold
  the same verified 1.07 executable. The game's code is checked byte for byte before anything is
  changed; the log says `Native rendering: output selection …` when it applies, or
  `Native rendering: refused …` when it does not.
- On CUSA00004 the log also says `Native live observer: incompatible code; no hooks installed`. That
  is the in-game live-resize menu of the other renderer, which the Mac build does not use; it is
  harmless.

### Best settings measured (MacBook Air M5)

Measured in the busiest scenes after the graffiti (1396×785 window, vsync on), with these lines in
`port/user/tlg.ini`:

| Settings | FPS | Picture |
|---|---|---|
| `iss_native_percent=50`, `fsr1=1`, `rcas=1` | 25.3 | intact; FSR 1 costs nothing and is visibly sharper than plain stretching |
| 50%, FSR 1, `quality_floor=8` | **27.4** | nothing missing in the compared shots (fewer shadow cascades: distant shadows not checked) |
| 35%, FSR 1, `quality_floor=8`, `fps_limit=30` | **steady 30** (lowest scene 29.8, worst frame 62 ms) | every object kept; softer, most visible on thin edges such as railings |
| 40%, FSR 1, `quality_floor=8`, `fps_limit=30` | 30, except 28.4 in the busiest scene | every object kept; a little softer |
| `iss_native_percent=67`, `fsr1=1`, `rcas=1` | about 20 (from two measurements, not one run) | intact, sharper |
| 50%, FSR 1, `quality_floor=14`, `fps_limit=30` | steady 30.0 (worst frame 41 ms) | the game's lowest detail: small objects vanish indoors, and in outdoor views from above the railing, roof and ground vanish |

- `quality_floor` is the game's own quality level, 0 (best) to 14 (lowest); the port's default is 1.
  Levels 7 to 12 removed nothing in the compared rooftop and hall shots. Level 8 gains 8% at 50%
  (27.4 FPS), level 7 only 2%, and levels 10 to 13 are no faster than 8. Level 13 already removes the
  rooftop's railing, roof and ground; level 14 gains 24% at 50% and breaks outdoor views the same way.
  At 50% a 30 FPS cap does not hold at any intact level (level 8: 26–29 FPS in the busy scenes); at 35%
  it holds.
- `lod_scale=0.45` on top of level 14 deletes walls and doors: avoid it.
- 60 FPS was out of reach at every setting tried (best average 34 FPS, with walls deleted).
- Not verified: outdoor gameplay, combat, fullscreen, and the HUD at 35–40% (no prompt was on screen in
  those runs).

## Patched MoltenVK (optional)

`port/scripts/macos/build_moltenvk.sh` builds MoltenVK 1.4.2 with six patches into `port/out/moltenvk`
(needs Xcode; the launcher then uses it instead of the SDK's MoltenVK). The patches are by
[satperm/iss_mac_port](https://github.com/satperm/iss_mac_port) (branch `macos`, commit
`d66dc47216a4e5e09cc3410de580bd480baad9a8`); the script downloads them, they are not in this repository.

- `drawable-race`: fixes MoltenVK issue #2791, a crash in Metal's present callbacks.
- `push-descriptor-storage`, `framebuffer-fetch`: fixes inside MoltenVK.
- `pass-labels`: lets `TLG_PASS_LABELS=1` name Metal render passes after the game's passes (off by
  default).
- `concurrent-dispatch`, `compute-queue`: only act through satperm's game-side variables
  `MVK_TLG_CONCURRENT_DISPATCH` and `MVK_TLG_CHAIN_QUEUE_FAMILY`, which this port does not set, so they
  do nothing here.

Build: `VULKAN_SDK=... port/scripts/macos/build_moltenvk.sh` (before the first build, Xcode needs
`sudo xcode-select -s /Applications/Xcode.app`, `sudo xcodebuild -license accept` and
`sudo xcodebuild -runFirstLaunch`). Switch back: delete `port/out/moltenvk`.

## Game data

The game files are never part of this repository. Keep them in `Game/`, which is git-ignored, or
anywhere outside the repository.

- Tested with the EU release **CUSA00004** with update 1.07 merged. Its `sce_sys/param.sfo` still says
  `APP_VER 01.00`. Because its `eboot.bin` matches the verified 1.07 executable, mods made for 1.07 are
  offered for it anyway. The game starts because its `eboot.bin` and `sce_module` match 1.07.
- `sce_discmap_patch.plt` can be missing.

## Mods / Cheats

Cheat files go in the mods folder (`ISS_MODS_DIR`), next to the PATCHES XML files.

- GoldHEN cheat JSON files (`json/<TITLE>_<VERSION>.json` in `GoldHEN/GoldHEN_Cheat_Repository`, for
  example `CUSA00004_01.07.json`) are read as well. They are not part of this repository.
- Only checkbox cheats are offered, and only when the file's original bytes match your `eboot.bin`.
  Button cheats write runtime data and are not supported.

## How it differs from the Linux/Windows port

All of these apply only on macOS (compiled under `__APPLE__`, or checked at run time for MoltenVK):

- **Rosetta 2 instead of native x86-64.** The build is x86_64; Rosetta runs the port and the game's own
  code in one process. Rendering still runs natively on Metal.
- **Memory layout.** Rosetta keeps an invisible reservation at 0xF_C000_0000–0x70_0000_0000, so the
  game's memory starts at 0x70_0000_0000; a fixed mapping the game asks for inside the reservation fails
  with a log line that says `inside Rosetta's reserved range`.
- **40-bit pointers.** The host objects the game can see live below 0xF_C000_0000, because the game packs
  pointers into 40 bits. If that low heap runs out the game stops with `STOP: low heap exhausted`.
- **Thread storage.** The game's thread block lives in a pthread key slot, and its `mov reg, gs:[0]`
  loads are rewritten to read it (the Windows port's approach).
- **Main thread.** AppKit only works on the main thread, so the window and its events run there and the
  game runs on a second thread.
- **MoltenVK limits.** No robustBufferAccess2 or nullDescriptor (the port falls back to robustBufferAccess
  and binds placeholder buffers and images); no fragment-shader barycentrics (SPIRV-Cross cannot turn
  `PerVertexKHR` into MSL); no sparse binding, so the game's GPU memory is imported host memory
  (VK_EXT_external_memory_host) instead of sparse arenas.
- **GPU memory on 16 GB.** Metal keeps every buffer resident, so the port imports only the game's direct
  memory (5056 MiB; flexible memory gets copies), replaces the unused 512 MiB GPU address table with a
  64 KiB stand-in unless `TLG_DIRECT_MEMORY_ACCESS` is on, binds index buffers to the range a draw reads,
  and limits how many particle draws (tessellation on MoltenVK, ~150 MiB of temporary Metal buffers each)
  are in flight. The log prints `GPU memory budget: ...` at start. Importing only direct memory and
  dropping the address table: approach learned from satperm/iss_mac_port.
- **GPU waits and device loss.** GPU waits wake up every second to ask the device, a submission the GPU
  has not finished after 5 s stops the game with `STOP: GPU hang (...)` (`TLG_GPU_HANG_SECONDS`), and
  the device-loss stop (see [Changes that affect all platforms](#changes-that-affect-all-platforms-for-upstream))
  also lists what the unfinished submissions held.
- **GPU caches.** Without `TLG_GPU_USER_DIR` (GPU tests and tools run by hand) the GPU caches go to
  `port/user`, not `<current folder>/user`.

## Known issues

- **GPU hang after the graffiti (fixed).** On macOS the GPU reads guest memory in place, and Metal
  has no buffer bounds checks. A compute shader in the area after the graffiti ran past the end of its
  buffer and hung the GPU. Compute shaders now apply the PS4 GPU's own range check to the buffers the
  game describes: reads past a buffer's records return 0, and writes there are dropped. Graphics
  shaders are not checked yet. Updating rebuilds the shader cache once: "Compiling shaders" shows at
  the next start. When this fix changed the cache, the rebuild took about 4 minutes of precompile,
  spread over three starts of 120-second test runs (13980, then 23951 of the 25175 jobs after the first
  two). The 112.6 s under [The app](#the-app) is a rebuild after `--clear-shader-cache` that found 5753
  pipelines already there. If the GPU still hangs, the stop message lists what the stuck submission
  held. `TLG_GPU_HANG_TRACE=1` also names the last compute dispatch or render pass the GPU
  finished and the commands after it; it is slower, so use it for bug reports.
- **GPU hang at the cutscene after "The Gauntlet" autosave (fixed by a workaround).** Continuing
  from that autosave, the in-engine cutscene on the rooftop lost the GPU about 10–15 seconds in
  (`STOP: GPU device lost`, Metal's `GPU Hang Error`), every time. In that frame some of the game's
  mesh-walk decals have NaN positions. Their vertex shaders (`3ab1ca58`, `ab140dd8`, `abea73dd`,
  `5e6e49c1`) walk a 31,269-triangle mesh per vertex to find the triangle under the decal; with NaN
  the walk never finds it and runs its full length, and that render pass never finished. On the Mac
  these walks now stop after 256 steps: those draws then take under 0.4 ms (measured) and the
  cutscene plays through. What it changes: a decal on such a mesh whose walk would need more than
  256 steps may sit slightly off. Why the positions are NaN is not known yet.
- **Speed.** About 12–20 FPS in gameplay on the M5 Air at the game's native 1920×1080. In the
  busier part of the area after the graffiti it runs at 12–14 FPS, where it used to run at 8–9. The
  Mac's GPU is the limit, not Rosetta: it is busy 93–98% of the time, almost all of it on pixel
  work. Making the thread that prepares draws 3× less busy did not change the frame rate (measured).
  A GPU trace showed 16 passes per frame that draw into no image, each sized at the largest area the
  GPU allows (32768×32768). The Mac's tile-based GPU walked that whole area every time, about 43 ms
  per frame. These passes are now sized to the area they can draw in. Most of the remaining time is
  ordinary pixel shading in about ten full-screen passes, plus one compute pass (about 9 ms), so a
  lower [internal resolution](#internal-resolution) helps the most. Removing the integer divisions
  of that compute pass's range checks saved under 1% (measured, not kept). If a
  scene draws wrong where it did not before, `TLG_AREA_CLAMP=0` restores the old size (slower).
  `TLG_PASS_LABELS=1` names each Metal render pass after the game's pass in an Xcode or Instruments
  trace; it needs the patched MoltenVK. The cutscenes that run at 60 FPS are the game's pre-rendered
  movies, which draw almost nothing.
- **What lower resolution cannot fix.** At 50% internal resolution the GPU takes about 41 ms per
  frame (about 25 FPS). About 30 ms of that does not shrink with the resolution (measured):
  - about 6 ms of GPU idle time. The game sends each frame only after the GPU has finished the one
    before, so every frame starts with the GPU idle. Getting back to new GPU work takes about 1.3 ms;
    the rest is the emulator's draw thread recording the frame's first big batch (about 1000 draws,
    5–7 ms), with only about 2 ms of other GPU work meanwhile;
  - about 8 ms of geometry (vertex) work;
  - about 4 ms of compute;
  - about 5 ms of fixed-size passes (shadow maps and quarter-size effects);
  - a fixed share of the full-screen passes (about 6 ms).

  So 60 FPS (16.7 ms per frame) is out of reach by resolution alone. The per-frame wait in the
  draw-recording thread is required by the PS4's ordering rules for this game's commands; removing it
  as a test raised FPS by only 2%, so it stays. MoltenVK's "immediate encoding"
  (`MVK_CONFIG_PREFILL_METAL_COMMAND_BUFFERS=2` or `3`) crashes this game in its occlusion-query
  encoding: do not use it. Sending a frame's first draws to the idle GPU in smaller pieces, and
  MoltenVK's asynchronous submits (`MVK_CONFIG_SYNCHRONOUS_QUEUE_SUBMITS=0`), did not change the frame
  rate either (27.4 and 27.2 against 27.35 FPS), so neither is kept. For developers,
  `TLG_BOUNDARY_TRACE=<file>` records this frame-start path; its os_signpost marks line it up with a
  Metal System Trace.
- **Slow first start after an update.** After every rebuild, the first start can sit on a black
  window for several minutes. Stack samples show MoltenVK compiling the Metal shaders of the saved
  pipeline cache (`vkCreatePipelineCache`). Why a rebuild makes it do that again is **unverified**
  (likely Metal's shader cache is kept per executable). Later starts are quick.
- **Flickering black square** next to Delsin's face (his hood or hair), not investigated yet.
- **42 of the 25175 precompiled pipelines** fail with
  `resource_discover_pass.cpp:257 MarkReadConstBufferSharpSources: Assertion Failed!`. Not investigated
  yet.
- **Memory.** On a 16 GB Mac the game peaked at about 7.5 GB at the recommended settings (`footprint_mb`
  7723 in the frame statistics of a 7-minute run) and used about 11 GB at the full 1080p (10841 at the
  title scene, an earlier build); other open apps push macOS into swap.
- **Build headers.** `/usr/local/include` holds the Vulkan SDK's headers, and Apple clang searches it
  before `-isystem`, so those are used instead of `port/gpu/third_party/vulkan-headers`.
- `tests/test_probe_env.py` has three tests that check Linux library paths; they fail on macOS, as they
  did before this port.

## Changes that affect all platforms (for upstream)

- Mods (`mod_patches.py`): GoldHEN cheat JSON files are read next to the PATCHES XML (checkbox cheats
  only), and any entry with an `Original` is offered and applied only when those bytes match the eboot.
- Mods (`infamous_patches.py` `mods_app_version`): a verified CUSA00004 dump (`verified_107`) whose
  `param.sfo` says 01.00 is matched against 01.07 mods, and `CUSA00004` joins `TITLE_IDS`.
- `probe.c` `install_iss_mouse_aim`: direct mouse aiming uses `iss_camera_title_supported`, so CUSA00004
  dumps (APP_VER 01.00 or 01.07) get the mouse camera like the camera hooks. The executable, writable
  and `iss_mouse_signature` byte guards are unchanged.
- `iss_native_resolution.h` (commit c2537aa): native rendering also accepts CUSA00004 dumps (APP_VER
  01.00 or 01.07), which hold the verified 1.07 executable, as `camera_build_supported` in
  `infamous_run.py` already does for the camera mod. Every byte check of the game code is unchanged.
- `runtime_memory.c` `split_at` for flexible mappings (commit b0118f7): a split flexible mapping kept the
  left half's physical offset in the right half, so writes to and frees of its backing hit the wrong
  pages.
- `vk_device_lost.h` `CheckDeviceLost`: a lost Vulkan device (queue submit, semaphore counter,
  semaphore wait) prints `STOP: GPU device lost (...)` and exits with code 21. Before, the submit and the
  counter asserted, and `Semaphore::Wait` spun forever on the lost device.
- Qt launcher (`qt/logic.py` `news_readme`, `qt/backend.py` `openReadme`): this fork's `README.md` is the
  macOS one, so the news panel reads `README-UPSTREAM.md` when it exists, and "Open README" opens
  `README.md` on macOS and `README-UPSTREAM.md` elsewhere. Without `README-UPSTREAM.md` (upstream) both
  use `README.md` as before.
- Latent on devices with nullDescriptor (Linux/Windows): a written null image is written as
  `eSampledImage` into a storage binding (VUID-00319). The fix is gated off there; the comment is at
  `NullImageDesc` in `vk_rasterizer.cpp`.
- This fork was not compiled on Linux or Windows. From reading the code (**unverified**): it needs Clang 16
  or GCC 10 or later (structured bindings captured in lambdas, in the SPIR-V emitter) and a C++20 library
  with `std::atomic::wait` (`vk_scheduler.h`); `signal_regs.h` needs `_GNU_SOURCE` first on Linux (every
  current includer defines it).

## Credits and license

- [HayatoG/iss_pc](https://github.com/HayatoG/iss_pc) and its contributors: the Linux/Windows port this
  fork is based on (see [README-UPSTREAM.md](README-UPSTREAM.md) for its own credits:
  [tlg_pc](https://github.com/HayatoG/tlg_pc), [gr2_pc](https://github.com/HayatoG/gr2_pc),
  [gow3_pc](https://github.com/FelipePrado0/gow3_pc), [bloodborne_pc](https://github.com/deadinside28/bloodborne_pc)).
- [shadPS4](https://github.com/shadps4-emu/shadPS4): the video core in `port/gpu/shadps4/`.
- [satperm/iss_mac_port](https://github.com/satperm/iss_mac_port): an independent macOS fork whose notes
  and MoltenVK patches this fork learned from (credited where used).
- [MoltenVK](https://github.com/KhronosGroup/MoltenVK) and the LunarG Vulkan SDK.

License: GNU GPL v2 or later ([port/LICENSE](port/LICENSE)); third-party components keep their own
licenses. inFAMOUS Second Son is a trademark of its owners; this project is not affiliated with them.
