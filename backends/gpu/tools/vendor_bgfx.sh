#!/usr/bin/env bash
# Copy the subset of bgfx, bimg and bx that gmpi_ui's GPU renderer compiles
# into backends/gpu/3rdparty/bgfx/, then apply our local change (gmpi.patch).
#
#   tools/vendor_bgfx.sh                       # clone bgfx.cmake at $TAG and vendor it
#   tools/vendor_bgfx.sh /path/to/bgfx.cmake   # vendor from an existing, PRISTINE checkout
#
# The file lists below are what the renderer actually needs, measured from
# the compiler's dependency records (GPU_SINGLE_DEPENDENCY_PLAN.md, milestone 0):
# one renderer per platform - Vulkan on Linux, Direct3D 12 on Windows, Metal
# on Apple - and bimg's core without its codecs. The renderers bgfx.cpp still
# names but we compile out (GL, D3D11, consoles) get stubs from
# gmpi/renderer_stubs.cpp instead of their sources.
#
# Bumping bgfx: change TAG, run this, rebuild, then regenerate every compiled
# shader blob with the matching shaderc (the binary format is versioned):
# gmpi_ui's own (shaders/compile_shaders.cmake) and every module's.
set -euo pipefail

TAG=v1.129.8958-499   # bgfx.cmake tag. Keep in step with tools/shaderc.cmake.

HERE=$(cd "$(dirname "$0")" && pwd)
DEST=$(cd "$HERE/../3rdparty/bgfx" && pwd)

if [ $# -ge 1 ]; then
    SRC=$(cd "$1" && pwd)
else
    SRC=$(mktemp -d)
    trap 'rm -rf "$SRC"' EXIT
    git clone --quiet --depth 1 --branch "$TAG" --recurse-submodules --shallow-submodules \
        https://github.com/bkaradzic/bgfx.cmake.git "$SRC"
fi

for sub in bgfx bimg bx; do
    [ -d "$SRC/$sub/.git" ] || [ -f "$SRC/$sub/.git" ] || { echo "$SRC/$sub is not a git checkout"; exit 1; }
    if [ -n "$(git -C "$SRC/$sub" status --porcelain)" ]; then
        echo "$SRC/$sub has local changes; vendor from a pristine checkout"; exit 1
    fi
done

COMMON=(
    # bx: everything except tests and unused compat shims
    bx/LICENSE
    bx/include/bx
    bx/include/tinystl
    bx/include/compat/linux bx/include/compat/msvc bx/include/compat/osx
    bx/src
    bx/3rdparty/ini/ini.h
    # bimg: the format table and conversions, no codecs
    bimg/LICENSE
    bimg/include/bimg
    bimg/src/bimg_p.h bimg/src/config.h bimg/src/image.cpp bimg/src/image_gnf.cpp
    bimg/3rdparty/astc-encoder/include/astcenc.h   # included unconditionally; BIMG_DECODE_ASTC=0 keeps it unused
    bimg/3rdparty/astc-encoder/LICENSE.txt
    # bgfx: core
    bgfx/LICENSE
    bgfx/include/bgfx
    bgfx/src/bgfx.cpp bgfx/src/bgfx_p.h bgfx/src/bgfx.idl.inl bgfx/src/config.h bgfx/src/charset.h
    bgfx/src/debug_renderdoc.cpp bgfx/src/debug_renderdoc.h bgfx/3rdparty/renderdoc/renderdoc_app.h
    bgfx/src/renderer.h bgfx/src/renderer_noop.cpp
    bgfx/src/shader.cpp bgfx/src/shader.h
    bgfx/src/shader_dxbc.cpp bgfx/src/shader_dxbc.h
    bgfx/src/shader_spirv.cpp bgfx/src/shader_spirv.h
    bgfx/src/topology.cpp bgfx/src/topology.h
    bgfx/src/vertexlayout.cpp bgfx/src/vertexlayout.h bgfx/src/version.h
    bgfx/src/vs_clear.bin.h bgfx/src/fs_clear0.bin.h bgfx/src/fs_clear1.bin.h bgfx/src/fs_clear2.bin.h
    bgfx/src/fs_clear3.bin.h bgfx/src/fs_clear4.bin.h bgfx/src/fs_clear5.bin.h bgfx/src/fs_clear6.bin.h
    bgfx/src/fs_clear7.bin.h bgfx/src/vs_debugfont.bin.h bgfx/src/fs_debugfont.bin.h
    # used by gmpi_ui's renderer itself (graphics_utils.cpp: atlas packing)
    bgfx/3rdparty/stb/stb_rect_pack.h
)
LINUX=(  # Vulkan
    bgfx/src/renderer_vk.cpp bgfx/src/renderer_vk.h
    bgfx/3rdparty/khronos/vulkan-local/vk_platform.h
    bgfx/3rdparty/khronos/vulkan-local/vk_video
    bgfx/3rdparty/khronos/vulkan-local/vulkan.h
    bgfx/3rdparty/khronos/vulkan-local/vulkan_core.h
    bgfx/3rdparty/khronos/vulkan-local/vulkan_beta.h
    bgfx/3rdparty/khronos/vulkan-local/vulkan_wayland.h
    bgfx/3rdparty/khronos/vulkan-local/vulkan_xcb.h
    bgfx/3rdparty/khronos/vulkan-local/vulkan_xlib.h
)
WINDOWS=(  # Direct3D 12
    bgfx/src/renderer_d3d12.cpp bgfx/src/renderer_d3d12.h bgfx/src/renderer_d3d.h
    bgfx/src/dxgi.cpp bgfx/src/dxgi.h bgfx/src/nvapi.cpp bgfx/src/nvapi.h
    bgfx/3rdparty/directx-headers/LICENSE
)
for h in d3d12.h d3d12sdklayers.h d3d12shader.h d3dcommon.h dxgi.h dxgi1_2.h dxgi1_3.h dxgi1_4.h \
         dxgi1_5.h dxgi1_6.h dxgicommon.h dxgiformat.h dxgitype.h winapifamily.h; do
    WINDOWS+=(bgfx/3rdparty/directx-headers/include/directx/$h)
done
APPLE=(  # Metal
    bgfx/src/renderer_mtl.mm bgfx/src/renderer_mtl.h
)

# Wipe what a previous run vendored. Our own files (gmpi/, gmpi.patch,
# VENDORED.md) are outside the three upstream directories and survive.
find "$DEST" -mindepth 1 -maxdepth 1 \( -name bx -o -name bimg -o -name bgfx \) -exec rm -rf {} +

for p in "${COMMON[@]}" "${LINUX[@]}" "${WINDOWS[@]}" "${APPLE[@]}"; do
    [ -e "$SRC/$p" ] || { echo "missing in source: $p"; exit 1; }
    mkdir -p "$DEST/$(dirname "$p")"
    cp -R "$SRC/$p" "$DEST/$(dirname "$p")/"
done

# git apply works outside a repository too; inside gmpi_ui it needs the
# patch's paths re-rooted at this directory.
(cd "$DEST" && git apply -p1 --directory="$(git rev-parse --show-prefix 2>/dev/null)" gmpi.patch)

{
    echo "bgfx.cmake $TAG"
    for sub in bgfx bimg bx; do echo "$sub $(git -C "$SRC/$sub" rev-parse HEAD)"; done
} > "$DEST/VERSIONS"

echo "vendored $(du -sh "$DEST" | cut -f1) into $DEST"
