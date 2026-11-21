// SPDX-License-Identifier: GPL-2.0-or-later
// Task 8n: when stage B gives the draws recorded so far to a GPU that has run out of work.
#include <cassert>
#include <cstdio>

#include "video_core/renderer_vulkan/vk_feed_idle_gpu.h"

using Vulkan::FeedIdleGpu::MinDraws;
using Vulkan::FeedIdleGpu::ShouldSubmit;

int main() {
    // Recording tick 10; the GPU has completed tick 9, the last one submitted: it has nothing to do.
    assert(ShouldSubmit(9, 10, MinDraws, false));
    assert(ShouldSubmit(9, 10, 5000, false));
    // The GPU still works on the last submission: the draws wait, as before.
    assert(!ShouldSubmit(8, 10, 5000, false));
    assert(!ShouldSubmit(0, 10, 5000, false));
    // A handful of draws is not worth a command buffer.
    assert(!ShouldSubmit(9, 10, MinDraws - 1, false));
    assert(!ShouldSubmit(9, 10, 0, false));
    // A tessellated indirect draw recorded: Task 8c's cap decides when that command buffer goes.
    assert(!ShouldSubmit(9, 10, 5000, true));
    std::puts("feed-idle-gpu-test: ok");
    return 0;
}
