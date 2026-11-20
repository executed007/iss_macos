// A CP write (WRITE_DATA) lands ahead of the deferred labels before it, except where the guest
// could tell (vk_cp_write_order.h, Task 8k).
#include <cassert>
#include <cstdio>
#include "video_core/renderer_vulkan/vk_cp_write_order.h"

using Vulkan::CpWriteOrder;
using Wait = CpWriteOrder::Wait;

int main() {
    CpWriteOrder order;
    // Nothing deferred: lands at once.
    assert(order.MustWait(0x1000, 4, 0, false) == Wait::No);

    // Labels 1 (EOP, 8 bytes at 0x2000) and 2 (EOS, 4 bytes at 0x3000), none written yet.
    order.Deferred(1, 0x2000, 8, 0);
    order.Deferred(2, 0x3000, 4, 0);
    assert(order.MustWait(0x1000, 4, 0, false) == Wait::No);      // elsewhere: ahead of them
    assert(order.MustWait(0x2004, 4, 0, false) == Wait::Overlap); // inside label 1
    assert(order.MustWait(0x1ffc, 8, 0, false) == Wait::Overlap); // across its start
    assert(order.MustWait(0x1ffc, 4, 0, false) == Wait::No);      // just before it
    assert(order.MustWait(0x2008, 4, 0, false) == Wait::No);      // just after it
    assert(order.MustWait(0x3000, 4, 1, false) == Wait::Overlap); // label 2 still pending
    assert(order.MustWait(0x3000, 4, 2, false) == Wait::No);      // label 2 written
    assert(order.MustWait(0x2000, 8, 1, false) == Wait::No);      // label 1 written

    // A copy back or download pending (range unknown): waits, wherever it writes.
    assert(order.MustWait(0x9000, 4, 2, true) == Wait::OtherWrite);

    // An interrupt-only EOP writes nothing: no overlap, but it counts as the latest label.
    order.Deferred(3, 0x5000, 0, 2);
    assert(order.MustWait(0x4ffc, 16, 2, false) == Wait::No);

    // The GPU thread met a WAIT_REG_MEM on a label not written yet: later writes wait until
    // every label deferred so far (up to 3) is written, wherever they write.
    order.CpWaited();
    assert(order.MustWait(0x9000, 4, 2, false) == Wait::CpWaited);
    assert(order.MustWait(0x9000, 4, 3, false) == Wait::No);
    // Labels deferred after the wait do not extend it.
    order.Deferred(4, 0x6000, 4, 3);
    assert(order.MustWait(0x9000, 4, 3, false) == Wait::No);
    assert(order.MustWait(0x6000, 4, 3, false) == Wait::Overlap);

    // Once written, a label no longer holds writes back.
    order.Deferred(5, 0x7000, 4, 4);
    assert(order.MustWait(0x6000, 4, 4, false) == Wait::No);
    assert(order.MustWait(0x7000, 4, 4, false) == Wait::Overlap);
    std::puts("cp-write-order-test: ok");
}
