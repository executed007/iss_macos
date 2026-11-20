# macOS (Apple Silicon, Rosetta 2) Port Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the port as an x86_64 macOS binary that Rosetta 2 runs on the MacBook Air M5, rendering through MoltenVK, and boot the user's own dump to gameplay.

**Architecture:** All macOS changes sit behind `__APPLE__` (C/C++), `Darwin)` (build.sh) and `sys.platform == 'darwin'` (Python), mirroring how the Windows port is isolated today. The Windows scheme for the guest TCB (a per-thread slot read through `gs`) is reused; guest memory moves above Rosetta's reserved hole. Phase 3 tuning is data (a preset file) plus two log lines.

**Tech Stack:** C11/C++20, Apple clang 21 with `-arch x86_64`, CMake + Ninja (arm64 Homebrew), SDL3 / FreeType / FFmpeg built x86_64 from source, LunarG Vulkan SDK (universal MoltenVK, loader, validation layers), Python 3 launcher.

**Spec:** `docs/superpowers/specs/2026-11-19-macos-rosetta-design.md`

## Global Constraints

- Branch `macos-arm64-m5` on `executed007/iss_pc`; small commits; push after each task.
- Linux and Windows builds must not change behaviour: every edit is inside `__APPLE__` / `Darwin)` / `darwin` branches, or is a pure refactor with identical output there.
- No game or firmware file is downloaded, bundled or committed. Tests that need the dump are run by the user, or by the agent only on a path the user supplies.
- Rosetta's reserved hole is `0xF_C000_0000`–`0x70_0000_0000`; nothing may be mapped there with `MAP_FIXED`.
- macOS guest user range: `0x70_0000_0000`–`0xFC_0000_0000`. macOS low (host-owned, guest-visible) range: `0x8_0000_0000`–`0xF_C000_0000`.
- The game process always runs with `ROSETTA_ADVERTISE_AVX=1`.
- Every performance or thermal number written anywhere is measured on this M5, with the command that produced it; otherwise it is marked **unverified**.
- Out of scope: `gpu_v2`, DLSS, FSR4 tools, freezing the Qt launcher, upstreaming.

## Review Focus

1. **Guest asks for a fixed address inside Rosetta's hole** (`sceKernelMapDirectMemory`/`sceKernelReserveVirtualRange` with a fixed hint in 63–448 GB) — expect a clear error naming the address, never a silent `MAP_FIXED` that kills Rosetta. Test in Task 3.
2. **Rosetta ignores edits to XMM registers made in a signal handler** — SSE4a emulation (`isa_rewrite_trap`) would then compute garbage silently. Test in Task 4 must fail loudly if so.
3. **`ROSETTA_ADVERTISE_AVX` missing** (user runs `out/tlg-probe` directly instead of through the launcher) — expect the probe to print one line saying how to fix it and exit, not crash on the first AVX instruction. Test in Task 6.
4. **Low-range exhaustion**: `runtime_low_map` walking past `0xF_C000_0000` — must return `NULL`, never map into the hole. Test in Task 3.
5. **Vulkan SDK not installed or not found** — `build.sh` must stop with one line naming the SDK and the env var, not a CMake trace. Check in Task 1.

---

### Task 1: x86_64 dependencies and toolchain check

**Files:**
- Create: `port/scripts/macos/deps.sh`
- Create: `port/scripts/macos/env.sh` (sourced by build.sh; sets `PKG_CONFIG_PATH`, `VULKAN_SDK` checks)
- Modify: `.gitignore` (add `port/out/deps-x86_64/`, `port/out/deps-src/`)

**Interfaces:**
- Produces: prefix `port/out/deps-x86_64` with `lib/pkgconfig/{sdl3,freetype2,libavcodec,libavutil,libswscale}.pc`, all dylibs `x86_64` only. `env.sh` exports `PKG_CONFIG_PATH` (prefix + `$VULKAN_SDK/lib/pkgconfig`) and fails with `Install the LunarG Vulkan SDK for macOS and set VULKAN_SDK (README-MACOS.md).` when `$VULKAN_SDK/lib/libvulkan.dylib` is missing.

- [ ] **Step 1: Install native host tools**

Run: `brew install cmake ninja nasm` (nasm is unused by the minimal FFmpeg build; skip it if `--disable-x86asm` is used — prefer that).
Expected: `cmake --version` and `ninja --version` print versions.

- [ ] **Step 2: Ask the user to install the LunarG Vulkan SDK for macOS** (stop and wait; the agent does not download installers). Record the installed version in the task notes.

- [ ] **Step 3: Write `deps.sh`**

Pinned versions: SDL3 `3.4.x` matching Homebrew's `3.4.20` tag, FreeType `2.13.x`, FFmpeg `7.1.x`. Sources are git clones/tarballs from upstream into `port/out/deps-src`. Build each with `CFLAGS=-arch x86_64`, `CMAKE_OSX_ARCHITECTURES=x86_64`, `MACOSX_DEPLOYMENT_TARGET=13.0`, install to the prefix, shared libs with `install_name` `@rpath/<lib>`. FFmpeg: `--arch=x86_64 --enable-cross-compile --disable-x86asm --disable-everything --enable-decoder=h264,mp3 --enable-parser=h264,mpegaudio --enable-swscale --disable-programs --disable-doc --enable-shared --disable-static` (add decoders only if `tests/test_videodec.c`/`test_mp3.c` need more). Script is idempotent (skips a lib whose `.pc` exists).

- [ ] **Step 4: Run it and verify**

Run: `port/scripts/macos/deps.sh && for f in port/out/deps-x86_64/lib/*.dylib; do lipo -archs "$f"; done | sort -u`
Expected: single line `x86_64`.
Run: `source port/scripts/macos/env.sh && pkg-config --exists vulkan sdl3 libavcodec libavutil libswscale freetype2 && echo ok`
Expected: `ok`. If the SDK has no `vulkan.pc`, `env.sh` writes one into the prefix pointing at `$VULKAN_SDK/lib` and `$VULKAN_SDK/include`.
Run with `VULKAN_SDK=/nonexistent`: expect the one-line error from Interfaces (Review Focus 5).

- [ ] **Step 5: Commit** — `build(macos): x86_64 dependency script and environment check`

---

### Task 2: POSIX compatibility header

**Files:**
- Create: `port/src/compat_mac.h`
- Modify: `port/src/runtime_thread.c`, `runtime_freeze.c`, `runtime_audio.c`, `runtime_mutex.c`, `probe.c` (include it under `#ifdef __APPLE__`); `port/gpu/shim/tlg_threads.h`, `tlg_write_log.cpp` (same header via `#include "../../src/compat_mac.h"` under `__APPLE__`, as they include `tlg_win_compat.h` under `_WIN32`)
- Test: `port/tests/test_compat_mac.c`

**Interfaces:**
- Produces (all `static inline`, only under `__APPLE__`):
  - `pid_t gettid(void)` — low 32 bits of `pthread_threadid_np(NULL, &id)`.
  - `int clock_nanosleep(clockid_t clock, int flags, const struct timespec *request, struct timespec *remain)` — supports `CLOCK_MONOTONIC`/`CLOCK_REALTIME` and `TIMER_ABSTIME` (define `TIMER_ABSTIME 1` if absent) by computing the remaining time and calling `nanosleep`; returns 0 or an errno value (POSIX contract, not -1).
  - `int pthread_mutex_timedlock(pthread_mutex_t *m, const struct timespec *abs_realtime)` — `pthread_mutex_trylock` loop with 100 µs `nanosleep` until the deadline; returns `ETIMEDOUT`. `// shortcut: polls at 100 µs, upgrade to a condvar if it shows up in profiles`.
  - `pthread_setname_np(thread, name)` call sites: macOS takes only the name and only for the calling thread; wrap as `tlg_set_thread_name(pthread_t, const char *)` that is a no-op for other threads.
  - `prctl` and `<asm/prctl.h>` includes are compiled out on macOS.

- [ ] **Step 1: Write the failing test** `tests/test_compat_mac.c` with `assert`s:
  - `gettid()` is nonzero and differs between the main thread and a `pthread_create`d thread.
  - absolute `clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, now+20 ms)` returns 0 and elapsed is ≥ 20 ms and < 200 ms.
  - a deadline already in the past returns 0 immediately (< 5 ms).
  - `pthread_mutex_timedlock` on a mutex held by another thread with a 30 ms deadline returns `ETIMEDOUT` after ≥ 30 ms; on a free mutex returns 0.

- [ ] **Step 2: Run it to verify it fails**

Run: `cd port && clang -arch x86_64 -std=c11 -Wall -Werror -pthread -Isrc tests/test_compat_mac.c -o out/compat-mac-test`
Expected: compile errors (`gettid`/`clock_nanosleep`/`TIMER_ABSTIME` undeclared).

- [ ] **Step 3: Implement `src/compat_mac.h`** per the Interfaces block and include it at the listed call sites.

- [ ] **Step 4: Run the test**

Run: same compile, then `out/compat-mac-test && echo pass`
Expected: `pass`.

- [ ] **Step 5: Commit** — `macos: POSIX compatibility header (gettid, clock_nanosleep, timedlock)`

---

### Task 3: Guest memory layout on macOS

**Files:**
- Modify: `port/src/runtime_memory.c:41-47` (constants), `:118` and `:288-310` (pool, host_map, host_discard), `:716-740` (`runtime_low_map`)
- Test: `port/tests/test_macos_memory.c`

**Interfaces:**
- Consumes: Task 2 header.
- Produces (behaviour, no new public functions):
  - `__APPLE__`: `USER_MIN 0x7000000000`, `USER_MAX 0xfc00000000`, new `LOW_MAX` = `0xfc0000000` on Apple and `USER_MIN` elsewhere; `runtime_low_map` loops while `low_next+size<=LOW_MAX`.
  - Pool: `shm_open("/tlg-<pid>", O_RDWR|O_CREAT|O_EXCL, 0600)` + immediate `shm_unlink`, then `ftruncate` once to `POOL_SIZE+FLEX_SPAN`.
  - Non-replacing low maps: `mmap` with the address as a hint (no `MAP_FIXED`); if the result differs from the hint, `munmap` it and treat as busy.
  - `host_discard`: no-op on macOS. `// shortcut: guest frees do not return RAM to macOS until exit; upgrade with a mach memory entry if memory pressure shows up`.
  - `MAP_NORESERVE` defined to 0 on macOS.
  - A guest fixed-address request overlapping `[0xfc0000000, 0x7000000000)` returns `NO_MEMORY` and logs `Runtime: guest asked for 0x... inside Rosetta's reserved range` once per call site (Review Focus 1).

- [ ] **Step 1: Write the failing test** `tests/test_macos_memory.c`, linked against `src/runtime*.c` like `tests/test_runtime.c` is in `build.sh`. Assertions:
  - 200 calls of `runtime_low_map(256<<20, PROT_READ|PROT_WRITE)`: every non-NULL result is in `[0x800000000, 0xfc0000000)`; once one returns NULL all later ones do (Review Focus 4).
  - the shm pool aliases: allocate one 64 KiB direct-memory block (`rTXw65xmLIA#libkernel`, looked up the way `test_runtime.c` looks up exports), map it twice at two guest addresses (`L-Q3LEjIbgA#libkernel`), write a byte through the first view, read it through the second.
  - `L-Q3LEjIbgA#libkernel` (`direct_map`) with `MAP_FIXED_FLAG` at `0x1000000000` returns `NO_MEMORY` (`0x8002000c`) and the process is still alive.

- [ ] **Step 2: Run it to verify it fails** — build with the Task 6 link line once that exists; until then: `clang -arch x86_64 ... tests/test_macos_memory.c src/runtime*.c` and expect compile failures on `memfd_create`/`MAP_FIXED_NOREPLACE`/`fallocate`.

- [ ] **Step 3: Implement** per Interfaces.

- [ ] **Step 4: Run the test** — expected exit 0. Also re-run `out/runtime-test` (from `build.sh --test`) once Task 6 lands.

- [ ] **Step 5: Commit** — `macos: guest memory above Rosetta's reserved range, shm pool`

---

### Task 4: Guest TCB slot and signal-context registers

**Files:**
- Modify: `port/src/runtime_thread.c:79-97` (`runtime_thread_tcb_offset`, `set_gs` for `__APPLE__`), `port/src/probe.c:263-280` and `:1339-1344`, `:1690-1692` (enable `patch_tcb_loads` and the early `runtime_thread_tcb_offset()` for `_WIN32 || __APPLE__`)
- Create: `port/src/signal_regs.h`
- Modify: `port/src/probe.c:281-460` (all `uc_mcontext.gregs[...]` / `fpregs` uses go through `signal_regs.h`)
- Test: `port/tests/test_tcb_slot.c`, `port/tests/test_signal_regs.c`

**Interfaces:**
- Produces:
  - `uint32_t runtime_thread_tcb_offset(void)` on macOS: creates one `pthread_key_t` on first call and returns `key*8`. `set_gs(base)` = `pthread_setspecific(key, base)`.
  - `signal_regs.h`: `uint64_t *sig_gpr(ucontext_t *uc, int x86_index)` (x86 numbering rax=0…r15=15), `uint64_t *sig_rip(ucontext_t *uc)`, `uint64_t *sig_rflags(ucontext_t *uc)`, `unsigned char *sig_xmm0(ucontext_t *uc)` (pointer to XMM0; Linux `fpregs+160`, macOS `&uc->uc_mcontext->__fs.__fpu_xmm0`), `void sig_xmm_commit(ucontext_t *uc)` (Linux: the existing XSTATE_BV fix at `fpu+464`/`fpu[512]`; macOS: no-op).

- [ ] **Step 1: Write the failing tests**
  - `test_tcb_slot.c`: `off = runtime_thread_tcb_offset()`; write bytes `65 48 8b 04 25 <off as le32> c3` into an `mmap`ed page, `mprotect` it RX; `pthread_setspecific((pthread_key_t)(off/8), (void*)0x1234abcd)`; calling the page returns `0x1234abcd`. Repeat in a second thread with a different value.
  - `test_signal_regs.c`: SIGTRAP handler does `*sig_gpr(uc,0)=0x55; memset(sig_xmm0(uc),0xAB,16); sig_xmm_commit(uc);`. Main runs inline asm `int3` then reads `rax` and `xmm0`. Assert `rax==0x55` and all 16 bytes of xmm0 `==0xAB` (Review Focus 2: if the XMM assert fails under Rosetta, stop and report — SSE4a emulation needs another route).

- [ ] **Step 2: Run them to verify they fail** — `clang -arch x86_64 -std=c11 -Isrc tests/test_signal_regs.c -o out/signal-regs-test` fails: `signal_regs.h` not found. `test_tcb_slot` fails to link `runtime_thread_tcb_offset` or crashes in `arch_prctl`.

- [ ] **Step 3: Implement** per Interfaces; convert `probe.c`'s `isa_gregs`, `fault`, `clobber_below`, `dump_frames`, `thread_dump` to the helpers (on Linux the helpers expand to the current expressions).

- [ ] **Step 4: Run both tests** (with `ROSETTA_ADVERTISE_AVX=1`) — expected exit 0.

- [ ] **Step 5: Commit** — `macos: guest TCB in a pthread slot; portable signal-context register access`

---

### Task 5: GPU library builds for x86_64 macOS

**Files:**
- Modify: `port/gpu/CMakeLists.txt` (an `APPLE` branch next to the `WIN32` ones at `:77`, `:190`, `:223`, `:235`, `:247`: static `tlggpu`, Foundation/Metal/QuartzCore/IOKit frameworks, no `-no-pie`)
- Modify: whatever files the compiler rejects, each fix behind `__APPLE__`; known: `gpu/shim/tlg_perf_stats.cpp:142` (`dlopen` of NVML: skip on Apple), `gpu/shim/tlg_threads.h:117` (`setpriority` per thread: skip on Apple)
- Modify: `port/gpu/shadps4/video_core/renderer_vulkan/vk_platform.cpp` (keep shadPS4's `__APPLE__` path: `VK_EXT_metal_surface`, `VK_KHR_portability_enumeration` + `VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR`; enable `VK_KHR_portability_subset` on the device when offered)

**Interfaces:**
- Consumes: Task 1 `env.sh`, Task 2 header.
- Produces: `port/out/gpu/libtlggpu.a` (x86_64) and the CMake unit tests already defined in `gpu/CMakeLists.txt` (`pipeline-binary-cache-test`, `pipeline-binary-initialization-test`, `bit-array-test`, …).

- [ ] **Step 1: Configure** — `source scripts/macos/env.sh && cmake -S gpu -B out/gpu -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_OSX_ARCHITECTURES=x86_64 -DTLG_PGO=off -DTLG_LTO=OFF`. Expected: configure succeeds.

- [ ] **Step 2: Build until it compiles** — `ninja -C out/gpu tlggpu`; fix each error behind `__APPLE__`, one commit per logical group (e.g. `macos(gpu): threads`, `macos(gpu): perf stats`). Expected end state: `lipo -archs out/gpu/libtlggpu.a` → `x86_64`.

- [ ] **Step 3: Build and run the GPU unit tests** — `ninja -C out/gpu && ctest --test-dir out/gpu --output-on-failure` (or run each test binary listed in CMake if no ctest). Expected: all pass; any test that needs a GPU feature MoltenVK lacks is listed in the commit message, not skipped silently.

- [ ] **Step 4: Commit** — `macos(gpu): build the GPU library for x86_64 macOS`

---

### Task 6: build.sh Darwin case, probe binary, AVX guard

**Files:**
- Modify: `port/build.sh` (a `Darwin)` arm in the `case $(uname -s)` at `:17`, before the Linux path)
- Modify: `port/src/probe.c` near `main` (AVX guard), `port/src/cpu_check.c` if the check belongs there
- Test: `port/tests/test_cpu_check.c` (extend)

**Interfaces:**
- Consumes: Tasks 1–5.
- Produces: `port/out/tlg-probe` (x86_64 Mach-O, rpath `@executable_path/gpu` and the deps prefix), `./build.sh --test` runs every existing unit test plus `compat-mac-test`, `macos-memory-test`, `tcb-slot-test`, `signal-regs-test` under `ROSETTA_ADVERTISE_AVX=1`.
- Probe guard: under `__APPLE__`, if CPUID lacks AVX and `ROSETTA_ADVERTISE_AVX` is unset, print `Run through the launcher or set ROSETTA_ADVERTISE_AVX=1 (Rosetta hides AVX otherwise).` and exit 2.

- [ ] **Step 1: Write the failing test** — in `tests/test_cpu_check.c` add a case for the guard's decision function `int cpu_check_rosetta_avx_missing(int has_avx, const char *env)` (returns 1 only for `has_avx==0 && env==NULL`).

- [ ] **Step 2: Run it** — fails to link `cpu_check_rosetta_avx_missing`.

- [ ] **Step 3: Implement** the function in `cpu_check.c`, the guard in `probe.c`, and the `Darwin)` arm: `source scripts/macos/env.sh`, `CC="clang -arch x86_64"`, submodule/patch steps as on Linux, CMake as in Task 5, then the loader link line from `:117` with `-Wl,-export_dynamic` instead of `-rdynamic`, `-Wl,-rpath,@executable_path/gpu`, no `-no-pie` (x86_64 macOS defaults to PIE; check the image loads in Task 7 before reaching for `-Wl,-no_pie`), `-framework Foundation -framework Metal -framework QuartzCore -framework IOKit`. `--test` reuses the Linux test list, with `-mf16c`/`-msse4a` tests kept (the F16C/SSE4a tests already fall back to recorded results).

- [ ] **Step 4: Run** — `cd port && ./build.sh --test` → `Built .../out/tlg-probe`, every test prints its pass line, exit 0. `env -u ROSETTA_ADVERTISE_AVX out/tlg-probe --cpu-features; echo $?` → the guard line and `2` (Review Focus 3).

- [ ] **Step 5: Commit** — `build(macos): Darwin arm in build.sh, probe AVX guard`

---

### Task 7: Vulkan on MoltenVK and the launcher environment

**Files:**
- Modify: `port/launcher/infamous_run.py:627` (`probe_env()`: on `darwin` set `ROSETTA_ADVERTISE_AVX=1`; set `VK_ICD_FILENAMES` to `$VULKAN_SDK/share/vulkan/icd.d/MoltenVK_icd.json` and `VK_LAYER_PATH` to `$VULKAN_SDK/share/vulkan/explicit_layer.d` only when they are unset and the files exist; skip the `LD_LIBRARY_PATH` logic)
- Test: `port/tests/test_launcher_macos_env.py`

**Interfaces:**
- Consumes: Task 6 binary.
- Produces: `probe_env()` returns the darwin variables above; `TLG_VK_VALIDATION=1` (existing, `gpu/shim/core/emulator_settings.h:68`) turns on `VK_LAYER_KHRONOS_validation`.

- [ ] **Step 1: Write the failing test** — patch `sys.platform` to `'darwin'` and `os.environ` to `{'VULKAN_SDK': <tmpdir with share/vulkan/icd.d/MoltenVK_icd.json>}`; assert `probe_env()['ROSETTA_ADVERTISE_AVX'] == '1'`, `VK_ICD_FILENAMES` ends with `MoltenVK_icd.json`, and `'LD_LIBRARY_PATH' not in env`. Second case: an explicit `VK_ICD_FILENAMES` in the environment is kept.

- [ ] **Step 2: Run** — `cd port && python3 -m unittest tests.test_launcher_macos_env` → FAIL.

- [ ] **Step 3: Implement** in `probe_env()`.

- [ ] **Step 4: Run the test** → PASS. Then the device check on hardware:
Run: `ROSETTA_ADVERTISE_AVX=1 TLG_VK_VALIDATION=1 out/tlg-probe --vulkan-only`
Expected: it reports the Apple M5 device via MoltenVK and exits 0. If `TargetVulkanApiVersion` (1.3) is rejected or a required feature is missing, record the exact message in `README-MACOS.md` Known issues and fix the feature query behind `__APPLE__` (geometry shaders, `shaderFloat64` are optional on MoltenVK).

- [ ] **Step 5: Commit** — `macos: launcher environment for Rosetta and MoltenVK; Vulkan device on Metal`

---

### Task 7b: Low heap for guest-visible host objects (added during execution, Ruling J)

**Why:** the guest packs pointers into 40-bit fields. Linux keeps host objects handed to the guest (GuestThread handles, TLS blocks, mutex/cond/sema/rwlock objects, file handles...) below 1 TiB via `-no-pie` + the brk heap + `mallopt` (probe.c ~:1322); Windows via `--disable-high-entropy-va`. Darwin malloc zones sit near 0x6000_0000_0000, so on macOS any such pointer is truncated.

**Files:**
- Create: `port/src/runtime_low_heap.c` (Apple only body; on other platforms the file compiles to nothing)
- Modify: `port/src/runtime.h` (under `__APPLE__`: `#define malloc/calloc/realloc/free/aligned_alloc/posix_memalign/strdup` to the low-heap functions for every `runtime*.c` translation unit; not for probe.c, the GPU library, or tests unless they include runtime.h on purpose)
- Modify: `port/build.sh` (Darwin arm compiles the new file; it is already covered by the `src/runtime*.c` glob — verify)
- Test: `port/tests/test_low_heap.c` (Apple only, wired into `build.sh --test`)

**Interfaces:**
- Produces: `void *runtime_low_malloc(size_t)`, `void *runtime_low_calloc(size_t, size_t)`, `void *runtime_low_realloc(void *, size_t)`, `void runtime_low_free(void *)`, `void *runtime_low_aligned_alloc(size_t align, size_t size)`, `int runtime_low_posix_memalign(void **, size_t, size_t)`, `char *runtime_low_strdup(const char *)` — malloc contracts (NULL/0 handling, `calloc` overflow → NULL + ENOMEM, `free(NULL)` no-op, 16-byte alignment minimum).
- Backing: chunks from `runtime_low_map()` (Task 3; range `0x8_0000_0000`–`0xF_C000_0000`). Small sizes: power-of-two size classes 16 B…32 KiB with per-class free lists carved from 1 MiB chunks; larger: own `runtime_low_map` mapping, returned with `munmap` on free. One `pthread_mutex_t`. Header stores the class/size. `// shortcut: one global lock, no chunk return to the OS; upgrade if profiles show contention`.
- A pointer not from the low heap passed to `runtime_low_free`/`realloc` (e.g. memory from a library's malloc) must be detected (address outside the low range) and forwarded to the system `free`/`realloc`, never corrupt the heap.

- [ ] **Step 1: Write the failing test** `tests/test_low_heap.c`: 10 000 random-size allocations (1 B–256 KiB) all return non-NULL addresses `< 0xfc0000000` and 16-byte aligned; contents survive `realloc` growth and shrink; freed blocks are reused (a malloc after a free of the same class returns an address seen before); `calloc` memory reads zero, including after reuse; `calloc(SIZE_MAX/2, 4)` returns NULL; `runtime_low_free(malloc_from_system)` does not crash; 8 threads × 50 000 alloc/free pairs finish with every live block's contents intact.
- [ ] **Step 2: Run** — fails to link the functions.
- [ ] **Step 3: Implement**, plus the `runtime.h` macros. Then grep `port/src` for host objects handed to the guest that are not allocated in a `runtime*.c` file (probe.c, gpu shim) and route each through `runtime_low_*` explicitly; list them in the report.
- [ ] **Step 4: Run** the test and the full `./build.sh --test` → pass.
- [ ] **Step 5: Commit** — `macos: low heap for guest-visible host objects (40-bit pointers)`

---

### Task 7c: Placeholder descriptors when nullDescriptor is unavailable (added during execution, Ruling L)

**Why:** MoltenVK has no `nullDescriptor`. The renderer writes `VK_NULL_HANDLE` buffers/views into descriptors and vertex-buffer binds (`vk_rasterizer.cpp` ~:2489 vertex buffers, ~:2741 and ~:2799 buffer infos, ~:3057 and ~:3335 image infos — verify, and grep the renderer for any other `VK_NULL_HANDLE`/`nullptr` descriptor writes). Invalid usage; a null vertex buffer may crash inside MoltenVK.

**Files:**
- Modify: `port/gpu/shadps4/video_core/renderer_vulkan/vk_instance.{h,cpp}` (store `null_descriptor` support; accessor `bool IsNullDescriptorSupported() const`)
- Modify: `port/gpu/shadps4/video_core/renderer_vulkan/vk_rasterizer.cpp` (and any other site found)
- Reuse the renderer's existing dummy/null resources if shadPS4 already has them (look for a null buffer / null image in buffer_cache / texture_cache — shadPS4 upstream keeps a `null_buffer` and null images for exactly this); create one only if none exists.
- Test: one GPU test in `port/tests/` (CMake target in `gpu/CMakeLists.txt`, like `async-pipeline-cache-test`) that creates the device on this machine and asserts the placeholder buffer and one placeholder view per used view type (1D, 2D, 2D array, 3D, cube) are valid handles when `IsNullDescriptorSupported()` is false.

**Interfaces:**
- On devices with `nullDescriptor` (Linux/Windows today) behaviour is unchanged: still write null handles.
- Without it: vertex buffers bind the placeholder buffer with offset 0; buffer descriptors use the placeholder buffer with range `VK_WHOLE_SIZE`; image descriptors use the placeholder view matching the binding's view type, in `VK_IMAGE_LAYOUT_GENERAL` (or the layout the descriptor expects), contents zero.

- [ ] **Step 1: Write the failing test** (asserts above).
- [ ] **Step 2: Run** — fails (accessor/placeholders missing).
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** the test, the four device GPU tests, and `./build.sh --test` → pass; `TLG_VK_VALIDATION=1` run of the test shows no null-descriptor VUID.
- [ ] **Step 5: Commit** — `macos(gpu): placeholder descriptors when nullDescriptor is unavailable`

---

### Task 8: Boot ladder with the user's dump

**Files:**
- Modify: as blockers are found, each behind `__APPLE__`
- Create: `README-MACOS.md` (repo root) — started here, completed in Task 10

**Interfaces:**
- Consumes: everything above. Needs the user's dump path (ask once; never copy or commit it).

- [ ] **Step 1: Ask the user** for the merged CUSA00223 1.07 folder path on this Mac (or for them to run the commands themselves).
- [ ] **Step 2: Run through the launcher's script mode** — `python3 port/launcher/infamous_run.py --play` (or the documented `run_infamous.sh` path) with the game folder set. Record the furthest rung reached: eboot loaded → imports resolved → first guest thread → window → title → gameplay → input.
- [ ] **Step 3: For each failure**, capture the log tail and fault report, fix behind `__APPLE__` with a regression test where the cause is unit-testable, commit (`macos: <cause>`), rerun. Unfixed blockers go to `README-MACOS.md` Known issues with the log line.
- [ ] **Step 4: Verify input** — keyboard, mouse capture/release (Esc), one SDL3 controller if the user has one.
- [ ] **Step 5: Commit** the README skeleton and any fixes.

---

### Task 8a: GPU memory without sparse binding on macOS (added during execution, Ruling P)

**Why:** Task 8 reached the window and presented frames, then stopped at `buffer.cpp:161 ... Failed to get buffer device address`: the buffer cache backs guest memory with 4 GiB sparse-residency arenas (`buffer_cache.cpp` ~:1051, `vk::SparseMemoryBind`), and MoltenVK 1.4.2 on the M5 has `sparseBinding = false`. Measured: MoltenVK offers `VK_EXT_external_memory_host` and `bufferDeviceAddress`; KosmicKrisp's ICD is arm64-only and cannot load in this x86_64 process.

**Approach (Ruling P):** generalise the existing `TLG_UNIFIED_MEMORY` design (`buffer_cache/unified_memory.h`, `BufferCache::EnableUnified`, `Core::Memory` host view) from one 512 MiB range to all guest direct + flexible memory on Apple. Import the runtime's **backing view** of the guest memory pool (`backing_base` in `runtime_memory.c`: `[0,POOL_SIZE)` direct, `[POOL_SIZE,+FLEX_SPAN)` flexible; never remapped, never protected) as host-pointer buffers, in chunks no larger than the device's `maxBufferSize`/Metal `maxBufferLength` (query it). Bindings translate guest VA → physical offset through the runtime's VMA table and address the imported buffer at that offset; a binding whose VA range is not physically contiguous falls back to the existing copy/staging path. On Apple, no sparse arenas are created. Linux/Windows keep sparse arenas unchanged.

**Files:** `port/gpu/shadps4/video_core/buffer_cache/{buffer_cache.cpp,buffer_cache.h,unified_memory.h,buffer.cpp}`, `port/gpu/shim/core/memory.{h,cpp}` (host-view lookup for any guest address), `port/src/runtime_memory.c` (export a VA→phys/backing lookup for the GPU hooks if none exists), `port/gpu/CMakeLists.txt` (test target). Test: `port/tests/test_unified_import.cpp`.

**Interfaces (decide exact names in the code, keep them in this shape):**
- Runtime → GPU: `int runtime_memory_backing(uintptr_t guest_addr, uint64_t size, uint64_t *phys_out)` returns 1 when `[guest_addr, +size)` is one physically contiguous direct/flexible mapping (phys offset into `backing_base`), else 0; plus the existing `backing_base`/pool size exposure the unified path already uses.
- GPU: `BufferCache` on Apple resolves a binding to `(imported chunk buffer, offset)` when contiguous; otherwise the existing non-unified path minus sparse (staging copies into a regular device buffer sized to the binding).

- [ ] **Step 1: Spike (throwaway, report the numbers):** under Rosetta, import a 16 KiB-aligned `mmap(MAP_SHARED)` view of an `shm_open` fd of 6 GiB via `VK_EXT_external_memory_host` on MoltenVK in chunks ≤ maxBufferLength; get each chunk's device address; CPU writes through a *second* `MAP_SHARED` view of the same fd are visible to a GPU compute read, and GPU writes are visible to the CPU after a fence. Record maxBufferLength, import alignment, and whether `mprotect` on the second view affects GPU access. If import or coherence fails, stop and report NEEDS_CONTEXT with the numbers.
- [ ] **Step 2: Write the failing test** `test_unified_import.cpp` from the spike's assertions, against the real `BufferCache` entry point on Apple.
- [ ] **Step 3: Implement** per Approach. Keep end-of-pipe fence / WAIT_REG_MEM / readback rules from `unified_memory.h` correct for the whole range.
- [ ] **Step 4: Run** the test, the GPU unit tests, `./build.sh --test`, then the game (Task 8 command) — it must pass the point where Task 8 stopped (no `Failed to get buffer device address`); report the next rung reached with log lines.
- [ ] **Step 5: Commit** — `macos(gpu): guest GPU memory through imported host memory (no sparse binding)`

---

### Task 8b: Title-scene freeze (added during execution, Ruling R)

**Why:** with Task 8a the 3D title/intro scene renders, then the game freezes after the next scene transition: `FREEZE: no frame for 10 s`, guest `main` running at `guest+0x53b340` with the same stack in every run, until `STOP: watchdog timeout`. Task 8a's ordering fixes (deferred fence labels vs CP writes) did not change it. Upstream reaches gameplay on Linux/Windows, so the cause is most likely macOS-specific (unified/pool GPU memory, Rosetta, Darwin runtime paths).

**Approach:** diagnose before fixing. Find what the loop at `guest+0x53b340` waits for: disassemble that loop in the locally prepared eboot (the user's own dump; local analysis only, nothing copied or committed), read the polled address/registers from the port's freeze report (`runtime_freeze.c`) or an lldb attach (`process attach`, `register read`, `memory read`), then classify the waited-on value: a GPU-written label/fence (pool mode), a value another guest thread should write (look at the other threads' freeze stacks and wait reasons), a runtime/kernel call result (time, events, semaphores, file I/O, video decode, audio), or a timer. Re-run with `TLG_WRITE_LOG=2` if it is a GPU/CP-written address.

**Files:** whatever the root cause needs, behind `__APPLE__` or gated on pool mode; a regression test where the cause is unit-testable.

- [ ] **Step 1: Diagnose** — report the loop's disassembly (a few instructions), the polled address and its current value, who is supposed to write it, and evidence.
- [ ] **Step 2: Write the failing test** for the root cause (if unit-testable).
- [ ] **Step 3: Fix the root cause.**
- [ ] **Step 4: Run** the test, `./build.sh --test`, the GPU tests, and the game; report the next rung with log lines (title menu interactive? gameplay?).
- [ ] **Step 5: Commit** — `macos: <cause>`

---

### Task 8c: GPU memory budget on a 16 GB Mac (added during execution, Ruling S)

**Why (Task 8b diagnosis):** the title-scene freeze is a lost Metal device: `kIOGPUCommandBufferCallbackErrorOutOfMemory` in every run since Task 8a. A fence label is never written and the guest waits on it forever (`guest+0x53b340` is a 50 µs rdtsc delay in a loop polling an EOP label at 0x7048000000; the deferred-label thread is stuck in `vkWaitSemaphores`). MoltenVK keeps every allocation resident, including the whole 6080 MiB imported pool (wired memory ~3 → 10 GB at import). At the scene change, GPU memory charged to the process climbs 1.9 → 16 GB in ~25 s while Vulkan's own count stays ~8.5 GB — unexplained. A 1200 MB texture budget did not change it. At startup MoltenVK recompiles every shader of the 377 MB driver pipeline cache (>4 min under memory pressure).

**Approach:** measure, then cut the largest real consumers; never paper over device loss.
- [ ] **Step 1: Fail loudly on device loss** — on `VK_ERROR_DEVICE_LOST` / Metal command-buffer errors, log the error and stop with `STOP: GPU device lost (<reason>)` within seconds instead of hanging until the watchdog. Test: unit-testable piece (the check/exit path) + one game run showing the message.
- [ ] **Step 2: Measure** the composition at the scene change with memory-heavy apps closed (ask the controller to ask the user if needed): log `MTLDevice.currentAllocatedSize`, MoltenVK allocation counts, Vulkan heap usage (raw), `footprint --by-category`/`vmmap -summary` of the game process, wired/compressed/swap totals every few seconds. Identify what grows by ~8 GB beyond Vulkan's count (candidates: textures/render targets created via MoltenVK, MTLHeaps, shader/pipeline conversions, staging/readback buffers, duplicate residency of imported memory). Report a table.
- [ ] **Step 3: Cut** the consumers the table names, cheapest first, each behind `__APPLE__`/pool mode: texture-cache budget derived from physical RAM on Apple; BDA page table sized to the guest range actually used; import only GPU-mapped regions instead of the whole pool if residency is the cost; disable or bound the driver pipeline-cache preload on Apple if it costs memory/time. Add a startup memory-budget check that logs the budget and warns when physical RAM leaves too little headroom (the user's brief asks for 16 GB budget checks).
- [ ] **Step 4: Run** `./build.sh --test`, the GPU tests and the game; the scene transition must pass without device loss; report the next rung with log lines and the peak memory numbers.
- [ ] **Step 5: Commit** in logical pieces (`macos(gpu): stop on device loss`, `macos(gpu): <cut>`).

---

### Task 8d: Patched MoltenVK 1.4.2 (added during execution, user request, Ruling U)

**Why:** the user asked to build MoltenVK with the same patches as https://github.com/satperm/iss_mac_port (branch `macos`, `port/scripts/macos/patches/moltenvk-1.4.2-*.patch`, six patches; their `build_moltenvk.sh` carries `SPDX-License-Identifier: GPL-2.0-or-later`). Expected value: drawable-race fix (MoltenVK issue #2791, a crash in Metal's present callbacks on ~1 start in 6–8 in their port), push-descriptor-storage and framebuffer-fetch fixes inside MoltenVK. Concurrent-dispatch, compute-queue chaining and pass labels activate only through their game-side env hooks (`MVK_TLG_CONCURRENT_DISPATCH`, `MVK_TLG_CHAIN_QUEUE_FAMILY`, `TLG_PASS_LABELS`) — inert for us unless ported later. None addresses memory. Xcode 27.2 is installed (user).

**Files:**
- Create: `port/scripts/macos/build_moltenvk.sh` — clones `KhronosGroup/MoltenVK` tag `v1.4.2` into `port/out/deps-src/MoltenVK`, downloads the six patches from satperm/iss_mac_port **pinned to a commit SHA** (not vendored into our repo), verifies each with `git apply --check`, applies in name order, runs `./fetchDependencies --macos && make macos`, installs the dylib to `port/out/moltenvk/libMoltenVK.dylib` and writes `port/out/moltenvk/MoltenVK_icd.json` pointing at it. Idempotent. Checks the result contains x86_64 (`lipo -archs`).
- Modify: `port/launcher/infamous_run.py` `mac_probe_env()` — prefer `port/out/moltenvk/MoltenVK_icd.json` when it exists (and the user set no VK_ICD_FILENAMES/VK_DRIVER_FILES), else the SDK MoltenVK as today. The SDK's system-wide MoltenVK is never overwritten.
- Modify: `port/tests/test_launcher_macos_env.py` (preference order test), `README-MACOS.md` (optional step, credit, what each patch does, how to switch back).

- [ ] **Step 1: Write the failing test** — with a temp `port/out/moltenvk/MoltenVK_icd.json`, `mac_probe_env()` returns it in `VK_ICD_FILENAMES`; without it, the SDK path as before; a user-set VK_ICD_FILENAMES/VK_DRIVER_FILES still wins.
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement** the script and the launcher change.
- [ ] **Step 4: Run** the script (≈15 min first time; `make` jobs ≤ 6), `lipo -archs` → includes x86_64; the test → PASS; `./build.sh --test`; `ROSETTA_ADVERTISE_AVX=1 out/tlg-probe --vulkan-only` with the patched ICD → Apple M5, MoltenVK, PASS; the four device GPU tests; one game run (log the MoltenVK path it loaded) — compare with the previous run's rung.
- [ ] **Step 5: Commit** — `macos: optional patched MoltenVK 1.4.2 build (patches from satperm/iss_mac_port)`

---

### Task 8e: pad-test intermittent failure on macOS (added during execution, Ruling V)

**Why:** `out/pad-test` aborts on macOS in most runs (3/3 in Task 8d's runs, 2/3 in the controller's): `Assertion failed: ((data.buttons & BTN_TOUCHPAD) && touch_at(&data,1440)), function main, file test_pad.c, line 127.` Same build passes on other runs, so it is timing-dependent. It must be classified: a test race (reads pad state before the runtime processed the injected SDL event) or a real race in `runtime_pad.c` that would drop touchpad clicks in the game.

**Files:** `port/tests/test_pad.c`, `port/src/runtime_pad.c` (only if the race is real), behind `__APPLE__` unless the race is platform-independent (then a shared fix is allowed with a note — it would also be a latent Linux/Windows bug).

- [ ] **Step 1: Diagnose** — reproduce with a loop (e.g. 20 runs), read test_pad.c around :100-130 and the code path from the injected SDL touch/button event to `scePadReadState` data; say who writes `buttons`/touch data, on which thread, and what orders it with the test's read.
- [ ] **Step 2:** if it's a real race, write a failing test that shows it deterministically (or a stress loop that fails reliably); if it's a test race, make the test wait for the event to be processed using the runtime's existing mechanism, not a sleep.
- [ ] **Step 3: Fix** the root cause.
- [ ] **Step 4: Run** pad-test 50 times in a loop → 50/50 pass; `./build.sh --test` passes.
- [ ] **Step 5: Commit** — `macos: <cause>` (or `pad: <cause>` if shared).

---

### Task 8f: Freeze after the graffiti save (added during execution, Ruling X)

**Why:** the user's first human play test (2026-11-20, keyboard+mouse, run `port/out/logs/run-20261120-100448.log`, live process pid 54276 still frozen at dispatch time) got through the title, into gameplay and the graffiti scene, then the screen went black with "game saved" and frames stopped: `FREEZE: no frame for 10 s`, guest `main` at `guest+0x53b340` (same EOP-label poll loop as Task 8b), no `STOP: GPU device lost`. Host threads: `shadPS4:GpuSchedPriorityPendingOpsRunner` in `vkWaitSemaphores` (MoltenVK) waiting on a GPU tick; `shadPS4:GpuCommandProcessor` in `__ulock_wait` (tlg-probe+0x1c35d0 < +0x190211 < +0x18fff0 < +0x1a64eb ...). Suspect a deadlock between Task 8a's ordering rules (`OrderUnifiedCpWrite` / `WaitPriorityOperations` on the CP thread vs deferred fence labels waiting for a tick that is never flushed/submitted), or a GPU hang Metal hasn't reported.

- [ ] **Step 1: Diagnose on the live process** (attach lldb to the frozen pid if it is still alive: `thread backtrace all`, symbolize with `atos -o port/out/tlg-probe -l <load addr>` or lldb's own symbols; identify which tick the priority thread waits for, whether that tick was ever submitted, and what the CP thread waits on). Detach without killing unless the user agrees. If the process is gone, reproduce: the save comes right after the graffiti; reaching it needs a human, so reason from the log and code first.
- [ ] **Step 2: Failing test** for the deadlock if it can be reproduced in a unit test (scheduler/priority-queue ordering).
- [ ] **Step 3: Fix the root cause** (pool mode / `__APPLE__` gated), keeping Task 8a's ordering guarantees.
- [ ] **Step 4: Run** tests and `./build.sh --test`; ask the controller for a human re-test of the graffiti→save transition.
- [ ] **Step 5: Commit** — `macos(gpu): <cause>`

---

### Task 9: Diagnostics and M5 Air presets

**Files:**
- Modify: `port/gpu/shim/tlg_perf_stats.cpp` (under `__APPLE__`, append to the existing `ISS_FRAME_STATS` line: `thermal=<nominal|fair|serious|critical>` from `NSProcessInfo.processInfo.thermalState` via `objc_msgSend`, and `footprint_mb=<task_vm_info.phys_footprint>`; log `WARNING: thermal state serious` once per transition upward)
- Create: `port/launcher/presets_m5air.py`
- Modify: `port/launcher/qt/gfxlogic.py:18,29,41` (register `M5AIR_PRESETS` like `APOLAR_PRESETS`)
- Modify: `port/tests/test_qt_screens_a.py:263` (the preset-order tuple) and add `test_m5air_profiles`

**Interfaces:**
- Produces: `M5AIR_PRESETS = (M5AIR_BATTERY, M5AIR_BALANCED, M5AIR_PERFORMANCE)` with ids `m5air_battery`, `m5air_balanced`, `m5air_performance`, built with the same `_profile`-style dict as `presets_apolar.py` (`credit`: `macOS port`). Only keys that `gfxlogic.PRESET_ITEMS`/`PRESET_SETTINGS` accept.
- Starting values (to be adjusted in Task 10 from measurements):
  - Battery: `menu:fps_limit '30'`, `menu:fsr1 '1'`, `menu:rcas '1'`, `menu:lod_scale '0.45'`, `menu:far_clip '400000'`, vsync `True`, `gpu_readbacks '2'`.
  - Balanced (the macOS default selection): `menu:fps_limit '60'` if measured stable, else `'30'`; `menu:fsr1 '1'`, `menu:lod_scale '0.65'`, vsync `True`.
  - Performance: no fps cap, `menu:fsr1 '0'`, native LOD, vsync `False`; launcher shows "Fanless Mac: expect throttling after a few minutes" in `what`.
  - `vram_budget_mb` for all three: `gfxlogic.suggested_budget_mb(8)` (half of 16 GB unified; adjust from Task 10 footprint logs).

- [ ] **Step 1: Write the failing test** `test_m5air_profiles`: the three ids exist in `gfxlogic.FULL_PRESETS`; `preset_targets(id)` gives fps `30`/`60 or 30`/absent and vsync `True`/`True`/`False`; `matching_preset(settings, items) == id`; every item value is accepted by its table item (same loop as the existing test at `:255-262`).
- [ ] **Step 2: Run** — `cd port && QT_QPA_PLATFORM=offscreen python3 -m unittest tests.test_qt_screens_a` → FAIL (needs PySide6 in a venv per README; if unavailable, run only the gfxlogic part, which imports no Qt).
- [ ] **Step 3: Implement** the preset file, registration, the macOS default selection (`balanced` when `sys.platform == 'darwin'` and no saved preset), and the two log fields.
- [ ] **Step 4: Run** the tests → PASS; run the game 2 minutes with `ISS_FRAME_STATS=1` and confirm the log shows `thermal=` and `footprint_mb=`.
- [ ] **Step 5: Commit** — `macos: thermal and memory footprint in frame stats; M5 Air presets`

---

### Task 10: Measurement, .app bundle, README-MACOS

**Files:**
- Create: `port/scripts/macos/make_app.sh`
- Modify: `README-MACOS.md`
- Create: `docs/macos/m5-air-measurements.md`

**Interfaces:**
- Produces: `port/out/inFAMOUS Second Son.app` — `Contents/MacOS/` holds `tlg-probe` and a launcher entry; `Contents/Frameworks/` holds the deps dylibs (rpath `@executable_path/../Frameworks`), MoltenVK from the SDK and its ICD json in `Contents/Resources/vulkan/icd.d/` (path set by `probe_env()` when bundled); `codesign --force --deep -s -` ad-hoc signed.

- [ ] **Step 1: Measure** each preset with `ISS_FRAME_STATS=1` for 7 minutes on the same route, plugged in and on battery; `scripts/session_chart.py` for charts; note `thermal=` transitions and peak `footprint_mb`. Write results with the exact commands to `docs/macos/m5-air-measurements.md`. Adjust preset values, rerun Task 9 tests, commit.
- [ ] **Step 2: Write `make_app.sh`**, run it, verify: `codesign -v --deep "port/out/inFAMOUS Second Son.app"` exits 0; `otool -L` on the binary shows only `@rpath/` and system paths; `open "port/out/inFAMOUS Second Son.app"` reaches the title with the user's dump.
- [ ] **Step 3: Complete `README-MACOS.md`**: requirements (macOS 13+, Rosetta, Vulkan SDK version), exact setup/build/run commands, the Rosetta and memory-range explanation in two lines, presets, validation layers (`TLG_VK_VALIDATION=1`), known issues, link to measurements. Every command in it is copy-pasted from a run that worked.
- [ ] **Step 4: Run `./build.sh --test` and the Python unit tests one last time** → all pass.
- [ ] **Step 5: Commit and push** — `macos: app bundle, README-MACOS, M5 Air measurements`
