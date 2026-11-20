# inFAMOUS Second Son for macOS (Apple silicon)

A macOS fork of [HayatoG/iss_pc](https://github.com/HayatoG/iss_pc), the native Linux/Windows port of
inFAMOUS Second Son (PS4, CUSA00223 / CUSA00004, update 1.07). It runs **your own** copy of the game on
an Apple silicon Mac: Rosetta 2 runs the game's x86-64 code, and the graphics go through Vulkan on
Metal via MoltenVK.

> **No game files are included.** You need your own copy of the game, dumped from your own PS4.

> **Status: work in progress, not playable through yet.** On a MacBook Air M5 (16 GB, macOS 26) the
> game boots, shows the title screen and reaches gameplay with keyboard and mouse, through the
> graffiti tutorial and into the area after it, at **7–14 FPS**. The GPU hang after the graffiti
> autosave is fixed. Speed is the main open problem. See [Status](#status) and
> [Known issues](#known-issues).

The upstream project's original README (Linux/Windows) is kept as [README-UPSTREAM.md](README-UPSTREAM.md).

## Contents

- [Status](#status)
- [Requirements](#requirements)
- [Build](#build)
- [Run](#run)
- [Controls (keyboard and mouse)](#controls-keyboard-and-mouse)
- [Graphics presets (MacBook Air M5)](#graphics-presets-macbook-air-m5)
- [Patched MoltenVK (optional)](#patched-moltenvk-optional)
- [Game data](#game-data)
- [How it differs from the Linux/Windows port](#how-it-differs-from-the-linuxwindows-port)
- [Known issues](#known-issues)
- [Changes that affect all platforms (for upstream)](#changes-that-affect-all-platforms-for-upstream)
- [Credits and license](#credits-and-license)

## Status

Tested on one Mac: MacBook Air M5, 16 GB, macOS 26, EU dump CUSA00004 with update 1.07.

| Area | State |
|---|---|
| Build (x86_64, run by Rosetta 2) | ✅ `port/build.sh` builds and its unit tests pass |
| Boot, title screen | ✅ |
| Gameplay (keyboard and mouse) | ✅ reached, through the graffiti tutorial |
| After the graffiti autosave | ✅ plays on (GPU hang fixed; tested 120 s with a post-graffiti save) |
| Performance | 🔧 7–14 FPS in gameplay on the M5 Air; the game's pre-rendered movies play at 60 |
| Graphics | 🔧 a flickering black square next to Delsin's face; 42 of ~25,000 shaders fail to compile |
| Controller | ⏳ not tested yet |
| Native arm64 build | ⏳ not possible without an x86-64 translator (the game's own code is x86-64) |

## Requirements

- An Apple silicon Mac with Rosetta 2: `softwareupdate --install-rosetta`.
- 16 GB of memory. On 16 GB the game uses about 11 GB: quit other apps before playing.
- Xcode Command Line Tools (full Xcode only for the optional patched MoltenVK).
- Homebrew: `brew install cmake ninja boost glslang`.
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

The Qt launcher needs PySide6. Without it, use the command line (it prepares the eboot, builds the
shader list and starts the game). From the repository root:

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

On a non-US keyboard layout (for example French AZERTY), tell us if a key does not act where the table
says: it may be read by letter instead of by position.

## Graphics presets (MacBook Air M5)

The launcher has three presets for a fanless M5 Air: `M5 Air · Battery`, `M5 Air · Balanced` and
`M5 Air · Performance`. **Balanced is the recommended pick**; it is not applied automatically, so choose
it once in the Graphics screen. The values are provisional and will be tuned from measurements.
`ISS_FRAME_STATS=1` adds `thermal=` (the Mac's thermal state) and `footprint_mb=` (the game's memory)
to the frame statistics, and the log warns when the thermal state reaches serious or critical.

## Patched MoltenVK (optional)

`port/scripts/macos/build_moltenvk.sh` builds MoltenVK 1.4.2 with six patches into `port/out/moltenvk`
(needs Xcode; the launcher then uses it instead of the SDK's MoltenVK). The patches are by
[satperm/iss_mac_port](https://github.com/satperm/iss_mac_port) (branch `macos`, commit
`d66dc47216a4e5e09cc3410de580bd480baad9a8`); the script downloads them, they are not in this repository.

- `drawable-race`: fixes MoltenVK issue #2791, a crash in Metal's present callbacks.
- `push-descriptor-storage`, `framebuffer-fetch`: fixes inside MoltenVK.
- `concurrent-dispatch`, `compute-queue`, `pass-labels`: only act through satperm's game-side variables
  `MVK_TLG_CONCURRENT_DISPATCH`, `MVK_TLG_CHAIN_QUEUE_FAMILY` and `TLG_PASS_LABELS`, which this port does
  not set, so they do nothing here.

Build: `VULKAN_SDK=... port/scripts/macos/build_moltenvk.sh` (before the first build, Xcode needs
`sudo xcode-select -s /Applications/Xcode.app`, `sudo xcodebuild -license accept` and
`sudo xcodebuild -runFirstLaunch`). Switch back: delete `port/out/moltenvk`.

## Game data

The game files are never part of this repository. Keep them in `Game/`, which is git-ignored, or
anywhere outside the repository.

- Tested with the EU release **CUSA00004** with update 1.07 merged. Its `sce_sys/param.sfo` still says
  `APP_VER 01.00`, so the launcher warns that cheat mods made for 1.07 are hidden. The game starts
  anyway, because its `eboot.bin` and `sce_module` match 1.07.
- `sce_discmap_patch.plt` can be missing.

## How it differs from the Linux/Windows port

All of these are compiled only for macOS (`__APPLE__`):

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
  are in flight. The log prints `GPU memory budget: ...` at start. A lost GPU device stops the game with
  `STOP: GPU device lost (...)`. Importing only direct memory and dropping the address table: approach
  learned from satperm/iss_mac_port.
- **GPU caches.** Without `TLG_GPU_USER_DIR` (GPU tests and tools run by hand) the GPU caches go to
  `port/user`, not `<current folder>/user`.

## Known issues

- **GPU hang after the graffiti (fixed).** On macOS the GPU reads guest memory in place, and Metal
  has no buffer bounds checks. A compute shader in the area after the graffiti ran past the end of its
  buffer and hung the GPU. Compute shaders now apply the PS4 GPU's own range check to the buffers the
  game describes: reads past a buffer's records return 0, and writes there are dropped. Graphics
  shaders are not checked yet. Updating rebuilds the shader cache once: "Compiling shaders" shows at
  the next start, for about 4 minutes. If the GPU still hangs, the stop message lists what the stuck
  submission held. `TLG_GPU_HANG_TRACE=1` also names the last compute shader the GPU finished; it is
  slower, so use it for bug reports.
- **Speed.** 7–14 FPS in gameplay on the M5 Air. The frame rate drops as the number of draws per
  frame rises (about 850–2,000 in gameplay): the port prepares each draw on the CPU, in x86 code
  translated by Rosetta, and that tops out at about 12,000–21,000 draws/s. The cutscenes that run at
  60 FPS are the game's pre-rendered movies, which take about 2 draws per frame. Being worked on.
- **Flickering black square** next to Delsin's face (his hood or hair), not investigated yet.
- **42 of the 25175 precompiled pipelines** fail with
  `resource_discover_pass.cpp:257 MarkReadConstBufferSharpSources: Assertion Failed!`. Not investigated
  yet.
- **Memory.** On a 16 GB Mac the game uses about 11 GB (`footprint_mb` in the frame statistics); other
  open apps push macOS into swap.
- **Build headers.** `/usr/local/include` holds the Vulkan SDK's headers, and Apple clang searches it
  before `-isystem`, so those are used instead of `port/gpu/third_party/vulkan-headers`.
- `tests/test_probe_env.py` has three tests that check Linux library paths; they fail on macOS, as they
  did before this port.

## Changes that affect all platforms (for upstream)

- `runtime_memory.c` `split_at` for flexible mappings (commit b0118f7): a split flexible mapping kept the
  left half's physical offset in the right half, so writes to and frees of its backing hit the wrong
  pages.
- Latent on devices with nullDescriptor (Linux/Windows): a written null image is written as
  `eSampledImage` into a storage binding (VUID-00319). The fix is gated off there; the comment is at
  `NullImageDesc` in `vk_rasterizer.cpp`.

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
