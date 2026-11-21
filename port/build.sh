#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
# Players' names work too (the README says ISS_LTO=OFF): ISS_LTO/ISS_PGO set TLG_LTO/TLG_PGO.
[[ -n ${ISS_LTO:-} && -z ${TLG_LTO:-} ]] && export TLG_LTO=$ISS_LTO
[[ -n ${ISS_PGO:-} && -z ${TLG_PGO:-} ]] && export TLG_PGO=$ISS_PGO
# The commit the probe logs on its first line (CI: GITHUB_SHA; local: git, plus "+dirty").
if [[ -z ${TLG_BUILD_COMMIT:-} ]]; then
    TLG_BUILD_COMMIT=${GITHUB_SHA:-$(git rev-parse HEAD 2>/dev/null || echo unknown)}
    TLG_BUILD_COMMIT=${TLG_BUILD_COMMIT:0:12}
    [[ -z ${GITHUB_SHA:-} ]] && ! git diff --quiet HEAD -- . 2>/dev/null && TLG_BUILD_COMMIT+="+dirty"
fi
export TLG_BUILD_COMMIT
mkdir -p out
# Windows: MSYS2 CLANG64 (packages in packaging/windows/README.md). The GPU library is static
# there and CMake links tlg-probe.exe as well; no PGO (GCC-only flags), LTO off by default.
case $(uname -s) in
MINGW*|MSYS*)
    # shellcheck source=msys2-env.sh
    source ./msys2-env.sh
    for tool in clang cmake ninja pkg-config; do
        command -v "$tool" >/dev/null || { echo "Need $tool from MSYS2 CLANG64 (packaging/windows/README.md)." >&2; exit 1; }
    done
    if [[ ! -f gpu/third_party/fsr-vulkan/CMakeLists.txt || ! -f gpu/third_party/imgui/imgui.h ||
          ! -f gpu/third_party/xbyak/xbyak/xbyak.h ]]; then
        git submodule update --init --recursive
    fi
    for patch in gpu/patches/fsr-vulkan/*.patch; do
        if ! git -C gpu/third_party/fsr-vulkan apply --reverse --check "$PWD/$patch" 2>/dev/null; then
            git -C gpu/third_party/fsr-vulkan apply "$PWD/$patch"
        fi
    done
    cmake -S gpu -B out/gpu -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTLG_PGO=off \
        -DTLG_LTO="${TLG_LTO:-OFF}" -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ >/dev/null
    echo "GPU library and loader: LTO ${TLG_LTO:-OFF}"
    if ! ninja -C out/gpu tlg-probe tlg-gpu-capabilities tlg-play > out/gpu-build.log 2>&1; then
        grep -v '^\[' out/gpu-build.log | tail -40 >&2
        echo 'Build failed (full log: out/gpu-build.log)' >&2; exit 1
    fi
    cp out/gpu/tlg-probe.exe out/gpu/tlg-gpu-capabilities.exe out/gpu/tlg-play.exe out/
    echo "Built $PWD/out/tlg-probe.exe"
    # TLG_RENDERER_V2=1: renderer v2 (gpu_v2/README.md), linked into out/tlg-probe-v2.exe.
    if [[ ${TLG_RENDERER_V2:-${ISS_RENDERER_V2:-}} == 1 ]]; then
        cmake -S gpu_v2 -B out/gpu_v2 -G Ninja -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ >/dev/null
        if ! ninja -C out/gpu_v2 tlg-probe-v2 > out/gpu_v2-build.log 2>&1; then
            grep -v '^\[' out/gpu_v2-build.log | tail -60 >&2
            echo 'Renderer v2 build failed (full log: out/gpu_v2-build.log)' >&2; exit 1
        fi
        cp out/gpu_v2/tlg-probe-v2.exe out/
        echo "Built $PWD/out/tlg-probe-v2.exe (renderer v2)"
    fi
    exit 0
    ;;
Darwin)
    # macOS (Apple Silicon): the game's x86-64 code runs directly, so the process is x86_64 and
    # Rosetta 2 runs it. Deps from scripts/macos/deps.sh; the shared steps below do the rest.
    # shellcheck source=scripts/macos/env.sh
    source scripts/macos/env.sh
    # 13.3: libc++ formats floating point (std::format/to_chars) from there; the deps target 13.0.
    export MACOSX_DEPLOYMENT_TARGET=13.3
    # Every compile below quotes "$CC" as one word.
    printf '#!/bin/sh\nexec clang -arch x86_64 "$@"\n' > out/clang-x86_64 && chmod +x out/clang-x86_64
    CC=$PWD/out/clang-x86_64
    # Rosetta hides AVX without it; the probe and the tests need it.
    export ROSETTA_ADVERTISE_AVX=1
    ;;
esac
darwin=$([[ $(uname -s) == Darwin ]] && echo 1 || true)
if [[ -z ${CC:-} ]]; then
    CC=$(command -v cc || command -v gcc || true)
    if [[ -z $CC ]]; then
        for candidate in /nix/store/*-gcc-wrapper-*/bin/gcc; do
            if [[ -x $candidate ]]; then CC=$candidate; break; fi
        done
    fi
fi
if [[ -z ${CC:-} ]]; then echo 'Install GCC/Clang or set CC.' >&2; exit 1; fi
# Dependencies come from pkg-config (Vulkan loader/headers, SDL3). On NixOS the
# environment is provided by shell.nix; re-enter it automatically if needed.
if ! { command -v pkg-config >/dev/null && pkg-config --exists vulkan sdl3 && command -v cmake >/dev/null && command -v ninja >/dev/null; }; then
    if [[ -z ${TLG_IN_NIX_SHELL:-} ]] && command -v nix-shell >/dev/null; then
        exec env TLG_IN_NIX_SHELL=1 nix-shell shell.nix --run "bash build.sh $*"
    fi
    [[ -n $darwin ]] && { echo 'Need pkg-config, cmake and ninja (brew install pkgconf cmake ninja) and the deps from scripts/macos/deps.sh (README.md).' >&2; exit 1; }
    echo 'Need pkg-config with vulkan and sdl3, cmake and ninja (see shell.nix).' >&2; exit 1
fi
# FreeType headers: the system package, or a local copy in the workspace tools (no root needed).
if ! pkg-config --exists freetype2 && [[ -d ../../tools/freetype-devel/pkgconfig ]]; then
    export PKG_CONFIG_PATH="$(cd ../../tools/freetype-devel/pkgconfig && pwd)${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
fi
read -r -a includes <<< "$(pkg-config --cflags vulkan sdl3 libavcodec libavutil libswscale freetype2)"
read -r -a libraries <<< "$(pkg-config --libs vulkan sdl3 libavcodec libavutil libswscale freetype2)"
# macOS: the SDK's loader has an @rpath install name.
[[ -n $darwin ]] && libraries+=(-Wl,-rpath,"$VULKAN_SDK/lib")
# GPU library (shadPS4 video core + drivers), built by CMake into out/gpu/libtlggpu.so.
# TLG_PGO: generate (instrumented build that writes pgo/ while the game runs), use, off.
# Default: use the profile in pgo/ when there is one. TLG_LTO=OFF disables link-time optimization.
pgo=${TLG_PGO:-}
[[ -n $darwin ]] && pgo=off
if [[ -z $pgo ]]; then
    if [[ -n $(find pgo -name '*.gcda' -print -quit 2>/dev/null) ]]; then pgo=use; else pgo=off; fi
fi
mkdir -p pgo
# Submodules (git clone --recursive, or: git submodule update --init) and this port's changes
# to FSR-Vulkan (gpu/patches/fsr-vulkan), applied to its working tree once.
if [[ ! -f gpu/third_party/fsr-vulkan/CMakeLists.txt || ! -f gpu/third_party/imgui/imgui.h ]]; then
    git submodule update --init --recursive
fi
for patch in gpu/patches/fsr-vulkan/*.patch; do
    if ! git -C gpu/third_party/fsr-vulkan apply --reverse --check "$PWD/$patch" 2>/dev/null; then
        git -C gpu/third_party/fsr-vulkan apply "$PWD/$patch"
    fi
done
lto=${TLG_LTO:-ON} ninja=(ninja)
# macOS: LTO off as on Windows; -j6 keeps a fanless laptop cool.
[[ -n $darwin ]] && lto=${TLG_LTO:-OFF} ninja=(ninja -j6)
cmake -S gpu -B out/gpu -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTLG_PGO="$pgo" \
    -DTLG_LTO="$lto" -DTLG_PGO_DIR="$PWD/pgo" ${darwin:+-DCMAKE_OSX_ARCHITECTURES=x86_64} >/dev/null
echo "GPU library: PGO $pgo, LTO $lto"
# A failed GPU build must stop here: an older libtlggpu.so would otherwise be used silently.
if ! "${ninja[@]}" -C out/gpu tlggpu > out/gpu-build.log 2>&1; then
    grep -v '^\[' out/gpu-build.log | tail -40 >&2
    echo 'GPU library build failed (full log: out/gpu-build.log)' >&2; exit 1
fi
# $ORIGIN/gpu: packaged copies keep the library next to the binary without patching it.
gpu=(-Lout/gpu -ltlggpu -Wl,-rpath,'$ORIGIN/gpu' -Wl,-rpath,"$PWD/out/gpu" -rdynamic)
pie=-no-pie
if [[ -n $darwin ]]; then
    # Static there: its own static deps (the LINK_LIBRARIES of CMake's GPU test executables) come along.
    # -export_dynamic keeps the symbols dladdr names in crash reports, as -rdynamic does on Linux.
    gpu=(out/gpu/libtlggpu.a out/gpu/third_party/fsr-vulkan/libffx_*.a out/gpu/third_party/miniz/libminiz.a
        out/gpu/third_party/sirit/src/libsirit.a out/deps-x86_64/lib/libZycore.a -Lout/deps-x86_64/lib
        -lfmt -lxxhash -lZydis -lavformat -lswresample -lc++ -framework Foundation -framework Metal
        -framework QuartzCore -framework IOKit -Wl,-rpath,@executable_path/gpu -Wl,-export_dynamic)
    # The deps' own rpath (out/deps-x86_64/lib) comes from sdl3.pc in "${libraries[@]}".
    pie=-Wl,-pie   # the default for x86_64 Mach-O; whether the image needs -no_pie is checked at run time
fi
runtime=(src/runtime*.c)
[[ -n $darwin ]] && runtime+=(src/compat_mac.c)
# Third-party decoders: compiled once, without this project's -Werror policy.
atrac9=(third_party/LibAtrac9/C/src/*.c)
if [[ ! -f out/libatrac9.a || -n $(find third_party/LibAtrac9/C/src -newer out/libatrac9.a -name '*.c') ]]; then
    rm -rf out/atrac9 && mkdir -p out/atrac9
    for source in "${atrac9[@]}"; do "$CC" -std=c99 -O2 -g -w -c "$source" -o "out/atrac9/$(basename "${source%.c}").o"; done
    ar rcs out/libatrac9.a out/atrac9/*.o
fi
# isa_emulate runs inside guest code: no XMM/YMM.
"$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -mgeneral-regs-only -c src/isa_emulate.c -o out/isa_emulate.o
# --test builds everything, then the unit tests; --test-only reuses the GPU library in out/ and
# builds and runs only the tests (CI, after the Build step with the same TLG_LTO/TLG_PGO).
if [[ ${1:-} != --test-only ]]; then
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread "$pie" -DTLG_BUILD_COMMIT="\"$TLG_BUILD_COMMIT\"" "${includes[@]}" -I. -Isrc src/probe.c "${runtime[@]}" src/cpu_check.c src/vulkan_smoke.c out/isa_emulate.o src/isa_rewrite.c out/libatrac9.a -lm "${gpu[@]}" "${libraries[@]}" -o out/tlg-probe
    echo "Built $PWD/out/tlg-probe"
    # TLG_RENDERER_V2=1 (ISS_RENDERER_V2): also renderer v2 (gpu_v2/README.md) and
    # the probe that uses it, out/tlg-probe-v2 (TLG_PROBE=tlg-probe-v2 runs it). Needs clang.
    if [[ ${TLG_RENDERER_V2:-${ISS_RENDERER_V2:-}} == 1 ]]; then
        cmake -S gpu_v2 -B out/gpu_v2 -G Ninja -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang >/dev/null
        if ! ninja -C out/gpu_v2 issgpu > out/gpu_v2-build.log 2>&1; then
            grep -v '^\[' out/gpu_v2-build.log | tail -40 >&2
            echo 'Renderer v2 build failed (full log: out/gpu_v2-build.log)' >&2; exit 1
        fi
        cs=(-Lout/gpu_v2 -lissgpu -Wl,-rpath,'$ORIGIN/gpu_v2' -Wl,-rpath,"$PWD/out/gpu_v2" -rdynamic)
        "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread "$pie" -DTLG_BUILD_COMMIT="\"$TLG_BUILD_COMMIT\"" "${includes[@]}" -I. -Isrc src/probe.c "${runtime[@]}" src/cpu_check.c src/vulkan_smoke.c out/isa_emulate.o src/isa_rewrite.c out/libatrac9.a -lm "${cs[@]}" "${libraries[@]}" -o out/tlg-probe-v2
        echo "Built $PWD/out/tlg-probe-v2 (renderer v2)"
    fi
    # GPU check for run.sh (live_resolution=auto): links only the Vulkan loader.
    "$CC" -std=c11 -O2 -Wall -Wextra -Werror -Igpu/shadps4 tools/gpu_capabilities.c "${libraries[@]}" -o out/tlg-gpu-capabilities
fi
if [[ ${1:-} == --test || ${1:-} == --test-only ]]; then
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread "${includes[@]}" -I. -Isrc tests/test_pad.c "${libraries[@]}" -lm -o out/pad-test
    out/pad-test
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread "${includes[@]}" -I. -Isrc tests/test_pad_map.c "${libraries[@]}" -lm -o out/pad-map-test
    out/pad-map-test
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -UNDEBUG tests/test_mouse_shake.c -lm -o out/mouse-shake-test
    out/mouse-shake-test
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -UNDEBUG tests/test_mouse_touch.c -Isrc -lm -o out/mouse-touch-test
    out/mouse-touch-test
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -UNDEBUG tests/test_rumble_gate.c -Isrc -o out/rumble-gate-test
    out/rumble-gate-test
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -UNDEBUG -pthread "${includes[@]}" -I. -Isrc tests/test_mouse_pad.c "${libraries[@]}" -lm -o out/mouse-pad-test
    out/mouse-pad-test
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread "${includes[@]}" -I. -Isrc tests/test_runtime.c "${runtime[@]}" out/libatrac9.a -lm "${gpu[@]}" "${libraries[@]}" -o out/runtime-test
    out/runtime-test
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -I. -Isrc tests/test_file_mods.c -o out/file-mods-test
    out/file-mods-test
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread "${includes[@]}" -I. -Isrc tests/test_sema.c "${runtime[@]}" out/libatrac9.a -lm "${gpu[@]}" "${libraries[@]}" -o out/sema-test
    out/sema-test
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -Isrc tests/test_rwlock_rp.c -o out/rwlock-rp-test
    out/rwlock-rp-test
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread "${includes[@]}" -I. -Isrc tests/test_freeze.c "${runtime[@]}" out/libatrac9.a -lm "${gpu[@]}" "${libraries[@]}" -o out/freeze-test
    out/freeze-test
    "$CC" -std=c11 -D_GNU_SOURCE -DTLG_SYSTEM_MALLOC -O2 -g -Wall -Wextra -Werror -I. -Isrc tests/test_content.c src/runtime_content.c -o out/content-test
    out/content-test
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -DTLG_SYSTEM_MALLOC "${includes[@]}" -I. -Isrc tests/test_mp3.c src/runtime_mp3.c out/libatrac9.a -lm "${libraries[@]}" -o out/mp3-test
    out/mp3-test tests/data/sine.mp3
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread "${includes[@]}" -I. -Isrc tests/test_videodec.c "${libraries[@]}" -o out/videodec-test
    out/videodec-test tests/data/tiny.h264
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -I. -Isrc tests/test_services.c -o out/services-test
    out/services-test
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -mbmi -mlzcnt -mmovbe -I. -Isrc tests/test_isa_emulate.c out/isa_emulate.o -o out/isa-emulate-test
    out/isa-emulate-test
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -c src/isa_rewrite.c -I. -Isrc -o out/isa_rewrite.o
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -mbmi -mlzcnt -I. -Isrc tests/test_isa_stub.c out/isa_rewrite.o out/isa_emulate.o -o out/isa-stub-test
    out/isa-stub-test tests/data/isa_fixture.bin
    # Windows red-zone protection: patched fixture functions survive a Windows-like clobber below rsp.
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -I. -Isrc tests/test_red_zone.c out/isa_rewrite.o out/isa_emulate.o -o out/red-zone-test
    out/red-zone-test tests/data/red_zone_image.bin tests/data/red_zone_table.bin
    # SSE4A (EXTRQ/INSERTQ): the CPU is the oracle on AMD; elsewhere recorded AMD results.
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -msse4a -I. -Isrc tests/test_isa_sse4a.c out/isa_rewrite.o out/isa_emulate.o -o out/isa-sse4a-test
    out/isa-sse4a-test
    # F16C (VCVTPH2PS/VCVTPS2PH, #51): the CPU is the oracle where it has F16C; elsewhere recorded results.
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -mavx -mf16c -I. -Isrc tests/test_isa_f16c.c out/isa_rewrite.o out/isa_emulate.o -o out/isa-f16c-test
    out/isa-f16c-test
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -UNDEBUG -I. -Isrc tests/test_cpu_check.c -o out/cpu-check-test
    out/cpu-check-test
    if [[ -n $darwin ]]; then
        "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -UNDEBUG -pthread -Isrc tests/test_compat_mac.c -o out/compat-mac-test
        out/compat-mac-test
        "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -UNDEBUG -Isrc tests/test_signal_regs.c -o out/signal-regs-test
        out/signal-regs-test
        for test in macos_memory tcb_slot low_heap; do
            "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -UNDEBUG -pthread "${includes[@]}" -I. -Isrc "tests/test_$test.c" "${runtime[@]}" out/libatrac9.a -lm "${gpu[@]}" "${libraries[@]}" -o "out/${test/_/-}-test"
            "out/${test/_/-}-test"
        done
        # Native resolution (iss_native_resolution.h; gpu_v2's CMake builds these elsewhere).
        "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -c src/runtime_native_resolution.c -o out/native_resolution.o
        for test in native_resolution native_exposure; do
            clang++ -arch x86_64 -std=c++20 -O2 -g -Wall -Wextra -Werror -UNDEBUG "tests/test_$test.cpp" out/native_resolution.o -o "out/${test/_/-}-test"
            "out/${test/_/-}-test"
        done
        # The GPU library's tests (CMake targets, as CI runs them on Linux), each in an empty folder with
        # its own GPU user folder: without one they write into port/user (path_util.cpp). Not
        # orb-multiplier-test, which does not link (tlg_view.cpp needs tlggpu.cpp).
        gpu_tests=(hang-trace-test device-lost-test thermal-warn-test memory-budget-test null-descriptors-test
            unified-import-test recompiler-fixes-test)
        if ! "${ninja[@]}" -C out/gpu "${gpu_tests[@]}" > out/gpu-tests-build.log 2>&1; then
            grep -v '^\[' out/gpu-tests-build.log | tail -40 >&2
            echo 'GPU test build failed (full log: out/gpu-tests-build.log)' >&2; exit 1
        fi
        export VK_ICD_FILENAMES=${VK_ICD_FILENAMES:-$VULKAN_SDK/share/vulkan/icd.d/MoltenVK_icd.json}
        bin=$PWD/out/gpu
        for test in "${gpu_tests[@]}"; do
            dir=$(mktemp -d)
            (cd "$dir" && TLG_GPU_USER_DIR=$dir/user "$bin/$test")
            rm -rf "$dir"
        done
    fi
fi
