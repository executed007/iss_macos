// SPDX-FileCopyrightText: Copyright 2026 inFAMOUS PC port
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdio>
#include <cstdlib>
#include "video_core/renderer_vulkan/vk_common.h"

namespace Vulkan {

// tlg: a lost device never signals its semaphores again, so every GPU wait and the guest's EOP
// label polls would hang until the watchdog. Stop at once with the reason (MoltenVK loses the
// device when Metal runs out of memory: kIOGPUCommandBufferCallbackErrorOutOfMemory). _Exit: the
// other GPU threads are blocked inside the driver, static destructors would wait on them.
inline void CheckDeviceLost(vk::Result result, const char* where) {
    if (result != vk::Result::eErrorDeviceLost) {
        return;
    }
    std::fprintf(stderr, "STOP: GPU device lost (%s: %s)\n", where, vk::to_string(result).c_str());
    std::fflush(stderr);
    std::_Exit(21);
}

} // namespace Vulkan
