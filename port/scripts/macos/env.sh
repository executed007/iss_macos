# Source from bash (port/build.sh): sets PKG_CONFIG_PATH/CMAKE_PREFIX_PATH for the x86_64 deps and the LunarG Vulkan SDK.
_mac_port=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
_mac_prefix=$_mac_port/out/deps-x86_64
if [[ ! -f ${VULKAN_SDK:-/nonexistent}/lib/libvulkan.dylib ]]; then
    echo 'Install the LunarG Vulkan SDK for macOS and set VULKAN_SDK (README.md).' >&2
    (return 0 2>/dev/null) && return 1 || exit 1
fi
# Some SDK versions ship no vulkan.pc; generate one in the deps prefix.
if [[ ! -f $VULKAN_SDK/lib/pkgconfig/vulkan.pc ]]; then
    mkdir -p "$_mac_prefix/lib/pkgconfig"
    cat > "$_mac_prefix/lib/pkgconfig/vulkan.pc" <<PC
Name: vulkan
Description: LunarG Vulkan SDK loader
Version: 1
Libs: -L$VULKAN_SDK/lib -lvulkan
Cflags: -I$VULKAN_SDK/include
PC
fi
# zlib is the SDK's (no .pc there); FreeType's freetype2.pc requires it.
if [[ ! -f $_mac_prefix/lib/pkgconfig/zlib.pc ]]; then
    mkdir -p "$_mac_prefix/lib/pkgconfig"
    printf 'Name: zlib\nDescription: macOS SDK zlib\nVersion: 1.2\nLibs: -lz\nCflags:\n' > "$_mac_prefix/lib/pkgconfig/zlib.pc"
fi
export PKG_CONFIG_PATH=$_mac_prefix/lib/pkgconfig:$VULKAN_SDK/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}
# Only the x86_64 prefix: Homebrew's arm64 .pc files and CMake packages (fmt, SDL3, FFmpeg) must not be found.
# Header-only Boost comes from its own keg: Homebrew's shared prefix would put /opt/homebrew/include on the path.
export PKG_CONFIG_LIBDIR=$_mac_prefix/lib/pkgconfig
export CMAKE_PREFIX_PATH=$_mac_prefix:/opt/homebrew/opt/boost${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}
unset _mac_port _mac_prefix
