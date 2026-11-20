#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Optional: builds MoltenVK 1.4.2 with six patches into port/out/moltenvk (dylib + ICD json); the
# launcher then prefers it over the SDK's MoltenVK. The patches are by satperm/iss_mac_port
# (https://github.com/satperm/iss_mac_port, branch macos), fetched at the pinned commit below into
# port/out (not vendored). Idempotent: patches already applied are skipped.
set -euo pipefail

TAG=v1.4.2
PATCH_SHA=d66dc47216a4e5e09cc3410de580bd480baad9a8
PATCHES=(compute-queue concurrent-dispatch drawable-race framebuffer-fetch pass-labels push-descriptor-storage)

port=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
src=$port/out/deps-src/MoltenVK
pdir=$port/out/deps-src/moltenvk-patches
dest=$port/out/moltenvk
export MACOSX_DEPLOYMENT_TARGET=13.0

[[ -d $src/.git ]] || git clone --depth 1 --branch "$TAG" https://github.com/KhronosGroup/MoltenVK "$src"
mkdir -p "$pdir" "$dest"
for name in "${PATCHES[@]}"; do
    f=$pdir/moltenvk-1.4.2-$name.patch
    [[ -s $f ]] || curl -fsSL -o "$f" \
        "https://raw.githubusercontent.com/satperm/iss_mac_port/$PATCH_SHA/port/scripts/macos/patches/${f##*/}"
    if git -C "$src" apply --check "$f" 2>/dev/null; then
        git -C "$src" apply "$f"; echo "applied $name"
    elif git -C "$src" apply --check -R "$f" 2>/dev/null; then
        echo "already applied: $name"
    else
        echo "patch $name neither applies nor is applied" >&2; exit 1
    fi
done

cd "$src"
[[ -d External/build ]] || ./fetchDependencies --macos
# `make macos` with a command-line variable puts it into MoltenVK's compiler defines (build fails), so
# this is its macos target by hand, with xcodebuild capped at 6 jobs.
xcodebuild build -jobs 6 -project MoltenVKPackaging.xcodeproj -scheme "MoltenVK Package (macOS only)" \
    -destination "generic/platform=macOS"

lib=Package/Release/MoltenVK/dynamic/dylib/macOS/libMoltenVK.dylib
archs=$(lipo -archs "$lib")
[[ $archs == *x86_64* ]] || { echo "$lib has no x86_64 slice ($archs)" >&2; exit 1; }
cp "$lib" "$dest/libMoltenVK.dylib"
# Absolute path: a bare "libMoltenVK.dylib" is found by dlopen's search path, i.e. the SDK's copy.
sed "s|\"library_path\": *\"[^\"]*\"|\"library_path\": \"$dest/libMoltenVK.dylib\"|" \
    Package/Release/MoltenVK/dynamic/dylib/macOS/MoltenVK_icd.json > "$dest/MoltenVK_icd.json"
echo "built $dest/libMoltenVK.dylib ($archs)"; cat "$dest/MoltenVK_icd.json"
