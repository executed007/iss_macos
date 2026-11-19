#!/usr/bin/env bash
# Builds x86_64 SDL3, FreeType and FFmpeg from upstream source into port/out/deps-x86_64.
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

if [[ ! -f $prefix/lib/pkgconfig/libavcodec.pc ]]; then
    fetch https://git.ffmpeg.org/ffmpeg.git "$FFMPEG_TAG" "$src/ffmpeg"
    mkdir -p "$src/ffmpeg/build-x86_64"
    (cd "$src/ffmpeg/build-x86_64" && ../configure --prefix="$prefix" --install-name-dir=@rpath \
        --enable-cross-compile --target-os=darwin --arch=x86_64 \
        --cc="clang -arch x86_64 -mmacosx-version-min=13.0" --disable-x86asm \
        --disable-everything --enable-decoder=h264,mp3 --enable-parser=h264,mpegaudio \
        --enable-swscale --disable-avformat --disable-avdevice --disable-avfilter \
        --disable-programs --disable-doc --enable-shared --disable-static
     make -j"$jobs" && make install)
fi

# Every shared lib must be x86_64 only
bad=$(for f in "$prefix"/lib/*.dylib; do [[ -L $f ]] || echo "$f $(lipo -archs "$f")"; done | grep -v ' x86_64$' || true)
[[ -z $bad ]] || { echo "Non-x86_64 libraries: $bad" >&2; exit 1; }
echo "deps ready: $prefix"
