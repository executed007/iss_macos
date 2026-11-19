// SPDX-License-Identifier: GPL-2.0-or-later
// Without nullDescriptor (MoltenVK) the renderer binds placeholders where it would write null
// handles: one zeroed buffer and one zeroed view per shader view type.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_null_resources.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "vulkan_test_driver.h"

int main() {
    using namespace Vulkan;
    if (SkipWithoutVulkanDriver()) {
        return 0;
    }
    Instance instance(0, std::getenv("TLG_VK_VALIDATION") != nullptr);
    if (instance.IsNullDescriptorSupported()) {
        std::puts("Null descriptors supported: placeholders not needed, skipped");
        return 0;
    }
    Scheduler scheduler(instance);
    NullResources null_resources(instance, scheduler);
    assert(null_resources.Buffer());
    for (const auto type : {AmdGpu::ImageType::Color1D, AmdGpu::ImageType::Color1DArray,
                            AmdGpu::ImageType::Color2D, AmdGpu::ImageType::Color2DArray,
                            AmdGpu::ImageType::Color2DMsaa, AmdGpu::ImageType::Color3D}) {
        assert(null_resources.View(type));
    }
    scheduler.Finish();
    std::puts("Null descriptor placeholders: buffer and 6 view types valid");
    return 0;
}
