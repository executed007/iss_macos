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
    // The renderer's usage: the import comes off a usage that counts it, never below 0.
    using VideoCore::UsageWithoutImport;
    assert(UsageWithoutImport(8500, 6000) == 2500);
    assert(UsageWithoutImport(2300, 6000) == 2300); // the driver did not count the import
    assert(UsageWithoutImport(6000, 6000) == 6000);
    assert(UsageWithoutImport(4000, 0) == 4000);    // nothing imported (not pool mode)
    std::puts("memory budget: warns below 16 GB or 2 GB of working set headroom; import comes off "
              "the usage that counts it");
}
