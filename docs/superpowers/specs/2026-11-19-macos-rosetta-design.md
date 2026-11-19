# macOS port (Apple Silicon, via Rosetta 2) — design

Branch `macos-arm64-m5` on `executed007/iss_pc`. Target: MacBook Air M5, 16 GB, macOS 26.

## Intent

Run inFAMOUS Second Son (user's own CUSA00223 1.07 dump) on the user's MacBook Air M5,
as well as that fanless 16 GB machine allows. A true arm64 build needs an x86-64→arm64
CPU translator (the port runs the game's x86-64 code directly); the user chose to defer
that to a later project. This project ships an **x86_64 build run by Rosetta 2**, with
rendering on Metal through MoltenVK (GPU work is native; only CPU code is translated).

## Goals

1. `port/build.sh` builds an x86_64 macOS binary on Apple Silicon.
2. The game boots to gameplay on the M5 with the user's dump.
3. `README-MACOS.md`: exact setup, build, run commands.
4. M5 Air defaults (Battery / Balanced / Performance) on top of the existing launcher knobs.
5. Small commits on the branch; every claim measured on this machine or marked unverified.

## Non-goals

- arm64-native CPU execution (later, separate project).
- `gpu_v2` (experimental renderer v2), DLSS, FSR4 tooling, Qt-launcher freezing/packaging.
- Upstreaming to `hayatog/iss_pc` (possible later; keep diffs isolated with `__APPLE__` to make it easy).
- New graphics features. Phase 3 reuses existing settings (FPS limit, FSR 1, VRAM budget,
  presets, `ISS_FRAME_STATS`).

## Anti-goals

- Bundling or downloading any game/firmware file. Weakening ownership checks.
- Breaking the Linux or Windows builds.
- Reporting FPS/thermal numbers that were not measured.

## Approaches considered

| Approach | Trade-off | Decision |
|---|---|---|
| x86_64 process under Rosetta 2 | Small diff; GPU native via MoltenVK; CPU side pays Rosetta's cost; Rosetta limited after macOS 27 | **Chosen by the user** |
| arm64 host + own x86-64 translator | Truly native; months of translator work before a frame | Deferred to a later project |
| Static recompilation of eboot to arm64 | Research-grade, uncertain | Rejected |

## Facts measured on this M5 (x86_64 probe under Rosetta, 2026-11-19)

- CPUID shows **no AVX/F16C/BMI/MOVBE/LZCNT by default**. With `ROSETTA_ADVERTISE_AVX=1`
  it reports AVX, AVX2, F16C, BMI1/2, MOVBE, LZCNT; **SSE4a is missing** (already emulated
  by `isa_rewrite.c`). The env var must be set by the launcher/run script before exec.
- Rosetta reserves **0xF_C000_0000–0x70_0000_0000** (63–448 GB), invisible to `vmmap`.
  `MAP_FIXED` there crashes the process. The port's `USER_MIN..USER_MAX`
  (0x10_0000_0000–0xFC_0000_0000) overlaps it. Free and usable: 0x7_0000_0000–0xF_C000_0000
  (system-reserved + low image range) and **0x70_0000_0000–0xFC_0000_0000** (560 GB).
- Works under Rosetta: shared-memory aliasing (`shm_open` + two `MAP_SHARED|MAP_FIXED`
  views), `gs:[key*8]` reads the pthread TSD slot, executing freshly written x86 code,
  `int3` → `SIGTRAP` with a readable/resumable `ucontext`, `VCVTPH2PS`.
- Homebrew on macOS 26 ships **arm64 bottles only**; x86_64 libraries are not available from it.

## Design

### Build (Phase 1)

- Host tools stay native arm64 (Homebrew `cmake`, `ninja`, `pkgconf`). Apple clang
  cross-compiles with `-arch x86_64`; no Rosetta needed to build.
- `port/scripts/macos/deps.sh` builds pinned x86_64 dependencies from source into
  `port/out/deps-x86_64`: SDL3, FreeType, FFmpeg (only libavcodec/libavutil/libswscale with
  the decoders the port uses), Vulkan-Loader. `PKG_CONFIG_PATH` points `build.sh` at it.
- MoltenVK + validation layers: **LunarG Vulkan SDK for macOS** (universal binaries), installed
  by the user (MoltenVK from source needs full Xcode, which this machine lacks).
- `build.sh` gets a `Darwin)` case mirroring the MSYS2 one: CMake `-DCMAKE_OSX_ARCHITECTURES=x86_64`,
  static GPU library like Windows, then the loader.
- Output: `port/out/` runnable binary + `run_infamous.sh` support first; `.app` bundle
  (dylibs copied, `@executable_path/../Frameworks` rpath, ad-hoc `codesign -s -`) after the
  game boots.

### Runtime (Phase 2) — the `__APPLE__` changes

1. **TCB/TLS:** reuse the Windows scheme — guest TCB in a pthread key slot; the loader rewrites
   `mov reg, gs:[0]` displacement to `key*8` (macOS x86_64 GS = pthread TSD base).
2. **Memory:** on macOS move the guest user range above Rosetta's hole (start ≥ 0x70_0000_0000);
   `memfd_create` → `shm_open`+`shm_unlink`; `MAP_FIXED_NOREPLACE` → `mach_vm_allocate(VM_FLAGS_FIXED)`
   probe or hint+verify. Guest requests with fixed addresses inside the hole fail loudly.
3. **Signals:** macOS `ucontext` layout (`uc_mcontext->__ss.__rip`, `__fs` FP state) behind a small
   register-access macro set in `probe.c`.
4. **Small POSIX gaps:** `gettid` → `pthread_threadid_np`; `clock_nanosleep` → `nanosleep` on
   deadline; `pthread_mutex_timedlock` → trylock + timed wait; the one futex → `os_sync_wait_on_address`
   or a condvar; `pthread_setname_np` single-arg form; drop `prctl`.
5. **Vulkan:** enable `VK_KHR_portability_enumeration`/`portability_subset` (shadPS4's existing
   `__APPLE__` code in `vk_platform.cpp`); metal surface via SDL3. Validation via `VK_LAYER_KHRONOS_validation`
   behind an env var (existing debug switches where present). Known MoltenVK gaps to handle as found:
   no geometry shaders, Vulkan 1.3 feature subset.
6. **Run script:** sets `ROSETTA_ADVERTISE_AVX=1`, `VK_ICD_FILENAMES`/loader paths, game folder.
   Crash diagnostics: existing fault handler + macOS crash reports.

Bring-up order: compile → Vulkan device/swapchain (`vulkan_smoke.c`) → load eboot → title → gameplay → input.

### M5 tuning (Phase 3)

Three presets as defaults files over existing settings:
- **M5 Air Battery:** 30 FPS cap, lowest internal resolution the port supports, FSR 1 upscale, fast GPU accuracy.
- **M5 Air Balanced** (default): 60 FPS cap with frame pacing, FSR 1 when below target, no MSAA.
- **M5 Air Performance:** uncapped/60, native resolution, thermal warning in the log.
Plus: VRAM budget sized for 16 GB unified (unified memory is shared with the OS and the
game's own ~8 GB guest pool), FPS/frame-time from `ISS_FRAME_STATS`, thermal state via
`NSProcessInfo.thermalState` (or `IOKit` fallback) logged every N seconds. Only settings that
already exist or are one-line plumbing; numbers come from measurement.

## Risks / open items for the builder

- The game may pass fixed addresses inside Rosetta's hole → needs a remap or guest-address shift; unknown until boot.
- Rosetta speed on the game's hot loops; Rosetta may not translate some instruction the game uses (fault handler will show).
- MoltenVK feature gaps (geometry shaders, specific formats, barycentrics) may break effects or pipelines.
- 16 GB unified memory: guest pool + GPU textures + macOS. Watch for memory pressure.
- Rosetta's future (Apple: limited after macOS 27).

## Left to the builder

Exact dependency versions, FFmpeg configure flags, how the presets are stored (whatever the
launcher already uses), and the order of small POSIX fixes.
