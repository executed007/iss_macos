// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_vulkan/vk_null_resources.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_platform.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"

namespace Vulkan {

namespace {
constexpr u64 BufferSize = 64 * 1024;

u32 ViewIndex(AmdGpu::ImageType type) {
    switch (type) {
    case AmdGpu::ImageType::Color1D:
        return 0;
    case AmdGpu::ImageType::Color1DArray:
        return 1;
    case AmdGpu::ImageType::Color2D:
        return 2;
    case AmdGpu::ImageType::Color2DArray:
        return 3;
    case AmdGpu::ImageType::Color2DMsaa:
        return 4;
    case AmdGpu::ImageType::Color3D:
        return 5;
    default:
        UNREACHABLE_MSG("No placeholder view for image type {}", u32(type));
    }
}
} // namespace

NullResources::NullResources(const Instance& instance, Scheduler& scheduler)
    : buffer{instance, 0, BufferSize, VideoCore::MemoryType::DeviceLocal, "Null Buffer"} {
    struct Kind {
        vk::ImageType image;
        vk::ImageViewType view;
        vk::SampleCountFlagBits samples;
    };
    static constexpr std::array<Kind, NumViews> kinds{{
        {vk::ImageType::e1D, vk::ImageViewType::e1D, vk::SampleCountFlagBits::e1},
        {vk::ImageType::e1D, vk::ImageViewType::e1DArray, vk::SampleCountFlagBits::e1},
        {vk::ImageType::e2D, vk::ImageViewType::e2D, vk::SampleCountFlagBits::e1},
        {vk::ImageType::e2D, vk::ImageViewType::e2DArray, vk::SampleCountFlagBits::e1},
        {vk::ImageType::e2D, vk::ImageViewType::e2D, vk::SampleCountFlagBits::e4},
        {vk::ImageType::e3D, vk::ImageViewType::e3D, vk::SampleCountFlagBits::e1},
    }};
    constexpr vk::Format format = vk::Format::eR8G8B8A8Unorm;
    const vk::Device device = instance.GetDevice();
    std::array<vk::ImageMemoryBarrier2, NumViews> to_clear{}, to_general{};
    for (u32 i = 0; i < NumViews; ++i) {
        const auto& kind = kinds[i];
        const bool msaa = kind.samples != vk::SampleCountFlagBits::e1;
        images[i] = VideoCore::UniqueImage(device, instance.GetAllocator());
        images[i].Create(vk::ImageCreateInfo{
            .imageType = kind.image,
            .format = format,
            .extent = {1, 1, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = kind.samples,
            // shortcut: multisampled storage needs shaderStorageImageMultisample, so the MSAA
            // placeholder is sampled only; a written MSAA binding would need its own.
            .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst |
                     (msaa ? vk::ImageUsageFlags{} : vk::ImageUsageFlagBits::eStorage),
            .initialLayout = vk::ImageLayout::eUndefined,
        });
        const vk::ImageSubresourceRange range{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
        views[i] = Check(device.createImageViewUnique({
            .image = vk::Image(images[i]),
            .viewType = kind.view,
            .format = format,
            .subresourceRange = range,
        }));
        to_clear[i] = {
            .dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
            .dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eTransferDstOptimal,
            .image = vk::Image(images[i]),
            .subresourceRange = range,
        };
        to_general[i] = {
            .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
            .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
            .dstAccessMask = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
            .oldLayout = vk::ImageLayout::eTransferDstOptimal,
            .newLayout = vk::ImageLayout::eGeneral,
            .image = vk::Image(images[i]),
            .subresourceRange = range,
        };
    }
    scheduler.EndRendering();
    const auto cmd = scheduler.CommandBuffer();
    cmd.pipelineBarrier2(
        {.imageMemoryBarrierCount = NumViews, .pImageMemoryBarriers = to_clear.data()});
    cmd.fillBuffer(buffer.Handle(), 0, VK_WHOLE_SIZE, 0);
    for (const auto& barrier : to_clear) {
        cmd.clearColorImage(barrier.image, vk::ImageLayout::eTransferDstOptimal,
                            vk::ClearColorValue{}, barrier.subresourceRange);
    }
    const vk::MemoryBarrier2 buffer_barrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
        .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .dstAccessMask = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
    };
    cmd.pipelineBarrier2({.memoryBarrierCount = 1,
                          .pMemoryBarriers = &buffer_barrier,
                          .imageMemoryBarrierCount = NumViews,
                          .pImageMemoryBarriers = to_general.data()});
}

vk::ImageView NullResources::View(AmdGpu::ImageType type) const noexcept {
    return *views[ViewIndex(type)];
}

} // namespace Vulkan
