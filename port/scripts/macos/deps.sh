#!/usr/bin/env bash
# Builds x86_64 SDL3, FreeType, FFmpeg and the GPU library's libraries (Zydis, fmt, xxHash, robin-map,
# VMA; versions as packaging/linux/build_deps.sh) from upstream source into port/out/deps-x86_64.
# Header-only Boost and the glslang shader compiler come from Homebrew (brew install boost glslang).
# The game binary is x86-64 (runs under Rosetta 2), so Homebrew's arm64 libs cannot be linked.
# Host tools (cmake, ninja, pkgconf) stay native arm64. Idempotent: skips a lib whose .pc exists.
set -euo pipefail

SDL_TAG=release-3.4.20
FT_TAG=VER-2-13-3
FFMPEG_TAG=n7.1.5

port=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
prefix=$port/out/deps-x86_64
src=$port/out/deps-src
export MACOSX_DEPLOYMENT_TARGET=13.0
jobs=${DEPS_JOBS:-4}
mkdir -p "$prefix" "$src"

fetch() { # url tag dir
    [[ -d $3/.git ]] || git clone --depth 1 --branch "$2" "$1" "$3"
}
cmake_build() { # srcdir builddir extra-args...
    local s=$1 b=$2; shift 2
    cmake -S "$s" -B "$b" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" \
        -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
        -DCMAKE_INSTALL_NAME_DIR=@rpath -DCMAKE_INSTALL_LIBDIR=lib -DBUILD_SHARED_LIBS=ON "$@"
    cmake --build "$b" -j"$jobs"
    cmake --install "$b"
}

if [[ ! -f $prefix/lib/pkgconfig/sdl3.pc ]]; then
    fetch https://github.com/libsdl-org/SDL "$SDL_TAG" "$src/SDL"
    cmake_build "$src/SDL" "$src/SDL/build-x86_64" -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_INSTALL_TESTS=OFF
fi

if [[ ! -f $prefix/lib/pkgconfig/freetype2.pc ]]; then
    fetch https://gitlab.freedesktop.org/freetype/freetype.git "$FT_TAG" "$src/freetype"
    cmake_build "$src/freetype" "$src/freetype/build-x86_64" \
        -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_PNG=ON -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_BROTLI=ON
fi

# Components as packaging/linux/build_deps.sh: the GPU library's AvPlayer and PNG loader need libavformat.
if [[ ! -f $prefix/lib/pkgconfig/libavformat.pc ]]; then
    fetch https://git.ffmpeg.org/ffmpeg.git "$FFMPEG_TAG" "$src/ffmpeg"
    mkdir -p "$src/ffmpeg/build-x86_64"
    (cd "$src/ffmpeg/build-x86_64" && ../configure --prefix="$prefix" --install-name-dir=@rpath \
        --enable-cross-compile --target-os=darwin --arch=x86_64 \
        --cc="clang -arch x86_64 -mmacosx-version-min=13.0" --disable-x86asm \
        --disable-network --disable-autodetect --enable-zlib --disable-everything \
        --enable-decoder=h264,mp3,mp3float,png --enable-parser=h264,mpegaudio,png \
        --enable-demuxer=h264,mp3,image2 --enable-protocol=file \
        --enable-swscale --enable-avformat --disable-avdevice --disable-avfilter \
        --disable-programs --disable-doc --enable-shared --disable-static
     make -j"$jobs" && make install)
fi

# Zydis 5 has no release tag yet: the pinned master commit of packaging/linux/build_deps.sh.
# Zycore is installed on its own (static, PIC) so zydis-config.cmake's find_dependency(Zycore) resolves.
if [[ ! -f $prefix/lib/cmake/zydis/zydis-config.cmake ]]; then
    [[ -d $src/zydis/.git ]] || git clone -q https://github.com/zyantific/zydis.git "$src/zydis"
    git -C "$src/zydis" checkout -q a95bb71
    git -C "$src/zydis" submodule update -q --init --recursive
    cmake_build "$src/zydis/dependencies/zycore" "$src/zycore-build-x86_64" -DBUILD_SHARED_LIBS=OFF \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DZYCORE_BUILD_SHARED_LIB=OFF -DZYCORE_BUILD_EXAMPLES=OFF -DZYCORE_BUILD_TESTS=OFF
    cmake_build "$src/zydis" "$src/zydis/build-x86_64" -DCMAKE_PREFIX_PATH="$prefix" -DZYAN_SYSTEM_ZYCORE=ON \
        -DZYDIS_BUILD_SHARED_LIB=ON -DZYDIS_BUILD_EXAMPLES=OFF -DZYDIS_BUILD_TOOLS=OFF -DZYDIS_BUILD_TESTS=OFF \
        -DZYDIS_BUILD_DOXYGEN=OFF
fi

if [[ ! -f $prefix/lib/cmake/fmt/fmt-config.cmake ]]; then
    fetch https://github.com/fmtlib/fmt 11.2.0 "$src/fmt"
    cmake_build "$src/fmt" "$src/fmt/build-x86_64" -DFMT_TEST=OFF -DFMT_DOC=OFF
fi

if [[ ! -f $prefix/lib/pkgconfig/libxxhash.pc ]]; then
    fetch https://github.com/Cyan4973/xxHash v0.8.3 "$src/xxhash"
    cmake_build "$src/xxhash/cmake_unofficial" "$src/xxhash/build-x86_64" -DXXHASH_BUILD_XXHSUM=OFF
fi

# Header-only.
if [[ ! -d $prefix/share/cmake/tsl-robin-map ]]; then
    fetch https://github.com/Tessil/robin-map v1.4.0 "$src/robin-map"
    cmake_build "$src/robin-map" "$src/robin-map/build-x86_64"
fi
if [[ ! -f $prefix/include/vk_mem_alloc.h ]]; then
    fetch https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator v3.3.0 "$src/vma"
    cmake_build "$src/vma" "$src/vma/build-x86_64"
fi

# Every shared lib must be x86_64 only
bad=$(for f in "$prefix"/lib/*.dylib; do [[ -L $f ]] || echo "$f $(lipo -archs "$f")"; done | grep -v ' x86_64$' || true)
[[ -z $bad ]] || { echo "Non-x86_64 libraries: $bad" >&2; exit 1; }
echo "deps ready: $prefix"
