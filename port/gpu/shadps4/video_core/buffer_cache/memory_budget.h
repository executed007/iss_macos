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

// tlg (pool mode): the driver's heap usage less the imported guest memory, which MoltenVK counts
// in it. A driver whose usage is not above the import does not count it and keeps its figure: 0
// would stop the texture collector and wrap its count of the images it frees. Bytes.
inline u64 UsageWithoutImport(u64 heap_usage, u64 imported) {
    return heap_usage > imported ? heap_usage - imported : heap_usage;
}

} // namespace VideoCore
