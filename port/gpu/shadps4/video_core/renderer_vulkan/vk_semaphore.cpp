// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <chrono>
#include <limits>
#include "video_core/renderer_vulkan/vk_device_lost.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_semaphore.h"

#include "common/assert.h"

namespace Vulkan {

#ifdef __APPLE__
// tlg: MoltenVK can leave a waiter blocked after it loses the device: wake up every second and
// ask the device (vkGetSemaphoreCounterValue returns VK_ERROR_DEVICE_LOST once it is lost).
constexpr u64 WAIT_TIMEOUT = 1'000'000'000;
#else
constexpr u64 WAIT_TIMEOUT = std::numeric_limits<u64>::max();
#endif

Semaphore::Semaphore(const Instance& instance_) : instance{instance_} {
    const vk::StructureChain semaphore_chain = {
        vk::SemaphoreCreateInfo{},
        vk::SemaphoreTypeCreateInfo{
            .semaphoreType = vk::SemaphoreType::eTimeline,
            .initialValue = 0,
        },
    };
    auto [semaphore_result, sem] =
        instance.GetDevice().createSemaphoreUnique(semaphore_chain.get());
    ASSERT_MSG(semaphore_result == vk::Result::eSuccess, "Failed to create master semaphore: {}",
               vk::to_string(semaphore_result));
    semaphore = std::move(sem);
}

Semaphore::~Semaphore() = default;

void Semaphore::Refresh() {
    u64 this_tick{};
    u64 counter{};
    do {
        this_tick = gpu_tick.load(std::memory_order_acquire);
        auto [counter_result, cntr] = instance.GetDevice().getSemaphoreCounterValue(*semaphore);
        CheckDeviceLost(counter_result, "semaphore counter");
        ASSERT_MSG(counter_result == vk::Result::eSuccess,
                   "Failed to get master semaphore value: {}", vk::to_string(counter_result));
        counter = cntr;
        if (counter < this_tick) {
            return;
        }
    } while (!gpu_tick.compare_exchange_weak(this_tick, counter, std::memory_order_release,
                                             std::memory_order_relaxed));
}

void Semaphore::Wait(u64 tick) {
    // No need to wait if the GPU is ahead of the tick
    if (IsFree(tick)) {
        return;
    }
    // Update the GPU tick and try again
    Refresh();
    if (IsFree(tick)) {
        return;
    }

    // If none of the above is hit, fallback to a regular wait
    const vk::SemaphoreWaitInfo wait_info = {
        .semaphoreCount = 1,
        .pSemaphores = &semaphore.get(),
        .pValues = &tick,
    };

    vk::Result result;
#ifdef __APPLE__
    std::chrono::steady_clock::time_point submitted_seen{};
#endif
    while ((result = instance.GetDevice().waitSemaphores(&wait_info, WAIT_TIMEOUT)) !=
           vk::Result::eSuccess) {
        CheckDeviceLost(result, "semaphore wait");
        if (result == vk::Result::eTimeout) {
            const auto [counter_result, counter] =
                instance.GetDevice().getSemaphoreCounterValue(*semaphore);
            CheckDeviceLost(counter_result, "semaphore wait");
            if (counter >= tick) {
                break; // reached, the wake-up was missed
            }
#ifdef __APPLE__
            // The clock starts once the tick is submitted: an unsubmitted tick may wait on the guest.
            const u64 current = CurrentTick();
            const auto now = std::chrono::steady_clock::now();
            if (tick >= current) {
                submitted_seen = {};
                continue;
            }
            if (submitted_seen == std::chrono::steady_clock::time_point{}) {
                submitted_seen = now;
            }
            // + the timed-out wait that came before the tick was seen submitted.
            const double waited =
                std::chrono::duration<double>(now - submitted_seen).count() +
                static_cast<double>(WAIT_TIMEOUT) / 1e9;
            if (IsGpuHang(tick, current, waited, GpuHangLimitSeconds())) {
                StopGpuHang(tick, counter, current, waited);
            }
#endif
        }
    }
    Refresh();
}

} // namespace Vulkan
