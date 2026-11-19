// SPDX-License-Identifier: GPL-2.0-or-later
// tlg: without nullDescriptor (MoltenVK) descriptors and vertex binds cannot hold VK_NULL_HANDLE.
// These zeroed placeholders take their place: one buffer, and one view per shader view type.
#pragma once

#include <array>
#include "video_core/amdgpu/resource.h"
#include "video_core/buffer_cache/buffer.h"
#include "video_core/renderer_vulkan/vk_common.h"
#include "video_core/texture_cache/image.h"

namespace Vulkan {

class Instance;
class Scheduler;

class NullResources {
public:
    explicit NullResources(const Instance& instance, Scheduler& scheduler);

    /// Bound with offset 0 and range/size VK_WHOLE_SIZE.
    vk::Buffer Buffer() const noexcept {
        return buffer.Handle();
    }

    /// The view for a shader's image type (ImageResource GetHostViewType), in eGeneral.
    vk::ImageView View(AmdGpu::ImageType type) const noexcept;

private:
    // Color1D, Color1DArray, Color2D, Color2DArray, Color2DMsaa, Color3D. Cube images reach
    // shaders as 2D arrays (SharpFetchPostOp::ConvertCubeTo2DArray), so there is no cube view.
    static constexpr u32 NumViews = 6;
    VideoCore::Buffer buffer;
    std::array<VideoCore::UniqueImage, NumViews> images;
    std::array<vk::UniqueImageView, NumViews> views;
};

} // namespace Vulkan
