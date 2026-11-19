// The pool-mode startup check warns when the GPU working set is short (Task 8c). MiB throughout.
#include <cassert>
#include <cstdio>
#include "video_core/buffer_cache/memory_budget.h"

int main() {
    using VideoCore::MemoryBudgetWarning;
    // MacBook Air M5, 16 GB: working set 12124, direct memory 5056, textures 3500.
    assert(!MemoryBudgetWarning(16384, 12124, 5056, 3500));
    // 8 GB Mac: warned on RAM alone.
    assert(MemoryBudgetWarning(8192, 5461, 5056, 3500));
    // Enough RAM, but guest memory and textures leave less than 2 GB of the working set.
    assert(MemoryBudgetWarning(16384, 10000, 5056, 3500));
    // Unknown values (0) never warn by themselves.
    assert(!MemoryBudgetWarning(0, 0, 5056, 3500));
    std::puts("memory budget: warns below 16 GB or 2 GB of working set headroom");
}
