// SPDX-License-Identifier: GPL-2.0-or-later
// tlg (Task 8n, macOS): when the draw recording thread submits draws to a GPU that has run out of
// work (Rasterizer::SubmitIfWorkPiledUp). Labels are untouched: each still has its own tick and
// lands after the work before it; this only ends a command buffer earlier.
#pragma once

#include <cstdint>

namespace Vulkan::FeedIdleGpu {

/// Fewer draws than this are not worth a command buffer of their own.
constexpr std::uint32_t MinDraws = 32;

/// Submit the draws recorded so far (at a render-pass boundary): the GPU has completed every
/// submission (gpu_tick: the last tick it is known to have reached; current_tick: the one being
/// recorded), enough is recorded, and no tessellated indirect draw is in the command buffer
/// (Task 8c's cap counts the submissions those are in, to bound MoltenVK's memory for them).
constexpr bool ShouldSubmit(std::uint64_t gpu_tick, std::uint64_t current_tick, std::uint32_t work,
                            bool tessellated_indirect) {
    return work >= MinDraws && gpu_tick + 1 >= current_tick && !tessellated_indirect;
}

} // namespace Vulkan::FeedIdleGpu
