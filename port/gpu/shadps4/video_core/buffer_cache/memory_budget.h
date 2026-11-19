// SPDX-FileCopyrightText: Copyright 2026 inFAMOUS PC port
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/types.h"

namespace VideoCore {

// tlg (pool mode, Task 8c): Metal keeps the imported guest memory wired in full, and what the GPU
// may use beyond it (textures, render targets, MoltenVK's temporary buffers) has to fit the
// device's working set. Why the budget is short, or null when it leaves enough. All in MiB.
inline const char* MemoryBudgetWarning(u64 ram, u64 working_set, u64 imported, u64 textures) {
    constexpr u64 min_ram = 16 * 1024;
    constexpr u64 min_left = 2 * 1024; // MoltenVK's temporary buffers, render targets, buffers
    if (ram != 0 && ram < min_ram) {
        return "less than 16 GB of memory";
    }
    if (working_set != 0 && working_set < imported + textures + min_left) {
        return "the GPU working set leaves less than 2 GB beyond guest memory and textures";
    }
    return nullptr;
}

} // namespace VideoCore
