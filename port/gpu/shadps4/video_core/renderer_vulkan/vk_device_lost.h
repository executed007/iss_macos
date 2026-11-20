// SPDX-FileCopyrightText: Copyright 2026 inFAMOUS PC port
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdio>
#include <cstdlib>
#include "common/types.h"
#include "video_core/renderer_vulkan/vk_common.h"
#include "video_core/renderer_vulkan/vk_hang_trace.h"

namespace Vulkan {

// tlg: a lost device never signals its semaphores again, so every GPU wait and the guest's EOP
// label polls would hang until the watchdog. Stop at once with the reason (MoltenVK loses the
// device when Metal runs out of memory: kIOGPUCommandBufferCallbackErrorOutOfMemory). _Exit: the
// other GPU threads are blocked inside the driver, static destructors would wait on them.
inline void CheckDeviceLost(vk::Result result, const char* where) {
    if (result != vk::Result::eErrorDeviceLost) {
        return;
    }
#ifdef __APPLE__
    HangTrace::PrintOnLoss(HangTrace::Global(), stderr); // what the unfinished ticks held
#endif
    std::fprintf(stderr, "STOP: GPU device lost (%s: %s)\n", where, vk::to_string(result).c_str());
    std::fflush(stderr);
    std::_Exit(21);
}

// tlg: Task 8f. Metal can run one command buffer for minutes without an error or a device loss,
// so the guest's EOP poll freezes silently. A submitted tick (below current_tick) still not done
// after the limit is a GPU hang. TLG_GPU_HANG_SECONDS overrides the 5 s; 0 never stops.
inline double GpuHangLimitSeconds() {
    static const double limit = [] {
        const char* env = std::getenv("TLG_GPU_HANG_SECONDS");
        return env && *env ? std::atof(env) : 5.0;
    }();
    return limit;
}

inline bool IsGpuHang(u64 tick, u64 current_tick, double waited_s, double limit_s) {
    return limit_s > 0 && tick < current_tick && waited_s > limit_s;
}

[[noreturn]] inline void StopGpuHang(u64 tick, u64 gpu_tick, u64 current_tick, double waited_s) {
    std::fprintf(stderr, "GPU hang: tick %llu submitted, the GPU completed up to tick %llu, %llu "
                         "submissions so far\n",
                 static_cast<unsigned long long>(tick), static_cast<unsigned long long>(gpu_tick),
                 static_cast<unsigned long long>(current_tick - 1));
    std::fprintf(stderr, "STOP: GPU hang (tick %llu, waited %.1f s)\n",
                 static_cast<unsigned long long>(tick), waited_s);
    std::fflush(stderr);
    std::_Exit(21);
}

} // namespace Vulkan
