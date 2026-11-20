# inFAMOUS Second Son on macOS (Apple silicon, Rosetta 2)

Work in progress. The port is built as an x86_64 program: Rosetta 2 runs the game's own x86-64 code
in the same process. Rendering goes through the shadPS4 video core, Vulkan and MoltenVK (Metal).
Tested on a MacBook Air M5 (16 GB) with macOS 26.

## Requirements

- An Apple silicon Mac with Rosetta 2 (`softwareupdate --install-rosetta`).
- Xcode Command Line Tools.
- Homebrew: `brew install cmake ninja boost glslang`.
- The LunarG Vulkan SDK **1.4.363.1** for macOS (it ships MoltenVK), installed at
  `~/VulkanSDK/1.4.363.1`. `VULKAN_SDK` must point to its `macOS` folder.
- Python 3 for the launcher scripts, plus `iced-x86` (see below).
- Your own dump of the game with update 1.07 merged (see "Game data").

## Build

```bash
cd port                                     # the commands of this block run in port/
scripts/macos/deps.sh                       # once: x86_64 SDL3, FreeType, FFmpeg and GPU libraries
VULKAN_SDK=$HOME/VulkanSDK/1.4.363.1/macOS ./build.sh          # tlg-probe in port/out
VULKAN_SDK=$HOME/VulkanSDK/1.4.363.1/macOS ./build.sh --test   # also the unit tests
```

Apple silicon CPUs, like Intel ones, have no SSE4A. Rosetta does not emulate it, so the launcher
writes a rewrite table with `scripts/isa_rewrite.py`, and that script needs `iced-x86`. Without it the
game stops with `STOP: this CPU lacks SSE4A and no rewrite table matches the image`. A venv inside
the git-ignored `port/out` keeps it apart from the system Python. From the repository root (after the
build block above, `cd ..` first):

```bash
python3 -m venv port/out/pyenv
port/out/pyenv/bin/python -m pip install iced-x86==1.21.0
```

## Run

The Qt launcher needs PySide6. Without it, use the command-line path (it prepares the eboot, builds
the shader list and starts the game). From the repository root:

```bash
export VULKAN_SDK=$HOME/VulkanSDK/1.4.363.1/macOS
export ISS_GAME_DIR="/path/to/CUSA00004"      # or CUSA00223
port/out/pyenv/bin/python port/launcher/infamous_run.py 600
```

The optional number is a limit in seconds: the game is stopped after that long (the probe's
`--timeout`). Leave it out (or pass 0) to play without a limit.

The game's output goes to `port/out/logs/run-<date>.log` (`port/out/last_run.log` points to the
latest one). The launcher sets these for the game:

- `ROSETTA_ADVERTISE_AVX=1`: without it Rosetta hides AVX, and the probe stops with exit code 2
  (`Run through the launcher or set ROSETTA_ADVERTISE_AVX=1`).
- `VK_ICD_FILENAMES` points to the SDK's MoltenVK (the system-wide `icd.d` can also hold
  KosmicKrisp). If you set `VK_ICD_FILENAMES` or `VK_DRIVER_FILES` yourself, the launcher keeps it.
- `VK_LAYER_PATH` points to the SDK's layers; `TLG_VK_VALIDATION=1` turns validation on.

The first start compiles the game's shader pipelines behind a progress bar; later starts skip it.
Measured on the M5 with the command above after `infamous_run.py --clear-shader-cache`:
`Precompile: 25175 records in 112.6 s: 11328 graphics and 8052 compute pipelines built, 5753 already
there, 42 failed` (`port/out/logs/run-20261119-162206.log`).

## Patched MoltenVK (optional)

`port/scripts/macos/build_moltenvk.sh` builds MoltenVK 1.4.2 with six patches into `port/out/moltenvk`
(needs Xcode, about 15 minutes); the launcher then uses it instead of the SDK's MoltenVK. The patches are
by [satperm/iss_mac_port](https://github.com/satperm/iss_mac_port) (branch `macos`, commit
`d66dc47216a4e5e09cc3410de580bd480baad9a8`); the script downloads them, they are not in this repository.

- `drawable-race`: fixes MoltenVK issue #2791, a crash in Metal's present callbacks.
- `push-descriptor-storage`, `framebuffer-fetch`: fixes inside MoltenVK.
- `concurrent-dispatch`, `compute-queue`, `pass-labels`: only act through satperm's game-side variables
  `MVK_TLG_CONCURRENT_DISPATCH`, `MVK_TLG_CHAIN_QUEUE_FAMILY` and `TLG_PASS_LABELS`, which this port does
  not set, so they do nothing here.

Build: `VULKAN_SDK=... port/scripts/macos/build_moltenvk.sh`. Switch back: delete `port/out/moltenvk`.
A `VK_ICD_FILENAMES` or `VK_DRIVER_FILES` you set yourself still wins.

## Game data

The game files are never part of this repository. Keep them in `Game/`, which is git-ignored, or
anywhere outside the repository.

- Tested with the EU release **CUSA00004** with update 1.07 merged. Its `sce_sys/param.sfo` still
  says `APP_VER 01.00`, so the launcher prints a warning about cheat mods made for 1.07. The game
  starts anyway, because the eboot and `sce_module` match 1.07.
- `sce_discmap_patch.plt` can be missing.

## How it differs from Linux

All of these are compiled only for `__APPLE__`:

- Rosetta keeps an invisible reservation at 0xF_C000_0000–0x70_0000_0000. The guest user range
  starts at 0x70_0000_0000, and a guest fixed map into the hole fails with a log line that says
  `inside Rosetta's reserved range`.
- The host objects the game can see live below 0xF_C000_0000, because the game packs pointers into
  40 bits. When that low heap runs out the game stops with `STOP: low heap exhausted` (exit 21).
- The guest TCB lives in a pthread key slot, and `mov reg, gs:[0]` loads are rewritten to read it.
- AppKit only works on the main thread. The window and its event loop run there, and the game
  runs on a second thread.
- MoltenVK has no robustBufferAccess2 or nullDescriptor. The port falls back to robustBufferAccess
  and binds placeholder buffers and views.
- MoltenVK has no sparse binding, which the buffer cache's guest-memory arenas use elsewhere. Guest
  GPU memory is imported host memory instead (VK_EXT_external_memory_host).
- GPU memory on a 16 GB Mac (Task 8c). Metal keeps every buffer wired, so the port imports only
  direct memory (5056 MiB; flexible memory gets copies), uses a 64 KiB stand-in for the 512 MiB BDA
  page table unless `TLG_DIRECT_MEMORY_ACCESS` is on, binds index buffers to the range a draw reads,
  and submits every 4 indirect tessellated draws (QuadList/RectList are tessellation on MoltenVK;
  each takes ~150 MiB of temporary Metal buffers), with at most two such submissions in flight.
  The log prints `GPU memory budget: ...` at start and warns below 16 GB or when the GPU working set
  leaves less than 2 GB. A lost device stops the game with `STOP: GPU device lost (...)` (exit 21).
  Importing only direct memory and dropping the page table: approach learned from
  satperm/iss_mac_port.
- Without `TLG_GPU_USER_DIR` (GPU tests and tools run by hand), the GPU caches go to the `user`
  folder next to the `out` folder that holds the executable (`port/user`), not `<cwd>/user`.
- MoltenVK offers VK_KHR_fragment_shader_barycentric, but SPIRV-Cross cannot turn PerVertexKHR
  inputs into MSL, so the port does not use it on MoltenVK.

## Changes that affect all platforms (for upstream)

- `runtime_memory.c` `split_at` for flexible mappings (commit b0118f7): a split flexible mapping kept
  the left half's physical offset in the right half, so writes to and frees of its backing hit the
  wrong pages.
- Latent on devices with nullDescriptor (Linux/Windows): a written null image is written as
  `eSampledImage` into a storage binding (VUID-00319). The fix is gated off there; the comment is at
  `NullImageDesc` in `vk_rasterizer.cpp`.

## Known issues

- The title-scene freeze was Metal running out of memory (Task 8b/8c; see above). With the Task 8c
  cuts the scene change passes and the game keeps rendering, at about 9-10 FPS on an M5 Air with
  6 GB of swap in use (measured in Task 8c; see its report).
- 42 of the 25175 precompile records (same log as above) fail with
  `resource_discover_pass.cpp:257 MarkReadConstBufferSharpSources: Assertion Failed!`. This has not
  been looked at yet.
- `/usr/local/include` holds the Vulkan SDK headers, and Apple clang searches it before `-isystem`.
  Those headers are used instead of `port/gpu/third_party/vulkan-headers`. The build works, but the
  headers depend on what is installed system-wide.
- `tests/test_probe_env.py` has three tests that check Linux library paths. They fail when run on
  macOS, and they failed before this port too.

## Graphics presets (MacBook Air M5)

The launcher has three presets for a fanless M5 Air: `M5 Air · Battery`, `M5 Air · Balanced` and
`M5 Air · Performance`. **Balanced is the recommended pick**; it is not applied automatically, so
choose it once in the Graphics screen. The values are provisional starting points and will be tuned
from measurements. `ISS_FRAME_STATS=1` adds `thermal=` and `footprint_mb=` to the frame stats lines.
