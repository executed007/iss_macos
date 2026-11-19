# Source from bash (port/build.sh): sets PKG_CONFIG_PATH for the x86_64 deps and the LunarG Vulkan SDK.
_mac_port=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
_mac_prefix=$_mac_port/out/deps-x86_64
if [[ ! -f ${VULKAN_SDK:-/nonexistent}/lib/libvulkan.dylib ]]; then
    echo 'Install the LunarG Vulkan SDK for macOS and set VULKAN_SDK (README-MACOS.md).' >&2
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
export PKG_CONFIG_PATH=$_mac_prefix/lib/pkgconfig:$VULKAN_SDK/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}
unset _mac_port _mac_prefix
