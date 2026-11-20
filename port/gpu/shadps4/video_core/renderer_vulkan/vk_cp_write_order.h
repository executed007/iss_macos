// SPDX-FileCopyrightText: Copyright 2026 inFAMOUS PC port
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// tlg: Task 8k. Unified memory (TLG_UNIFIED_MEMORY, always on with Apple's pool): when a command
// processor write (WRITE_DATA) must wait for the operations deferred to GPU completion before it
// (Rasterizer::OrderUnifiedCpWrite).
//
// What the guest can observe on the PS4 (GCN command processor, one queue):
// - an EOP/EOS label lands once the GPU has finished the work before its event, labels land in
//   order, and each lands after every WRITE_DATA before it;
// - a WRITE_DATA lands when the command processor reaches it. It does not wait for the draws,
//   dispatches or labels before it, so it may land before them. The command processor holds it
//   back only when told to: a WAIT_REG_MEM stops it until the memory holds the value (a label
//   waited for has landed, and so have the labels before it); a CS/VS/PS partial flush or an
//   ACQUIRE_MEM stops it until the work before is done.
// Here the labels land on the deferred-operation thread once the GPU is done (in order, after a
// host barrier) and a graphics queue WRITE_DATA lands on the draw recording thread at once, unless
// - a label not written yet covers any of its bytes: command order (the late label would
//   overwrite it);
// - another deferred write is pending (a copy back of GPU-written guest memory, an image
//   download): its range is not tracked;
// - the graphics queue waited before it, as above (CpWait), while a label deferred before that
//   wait is not written yet. For a WAIT_REG_MEM this is the case the GPU thread meets in stream
//   order on a label handed over and not written yet (Liverpool::PendingFenceValue).
// Then it waits for every deferred operation, as every CP write did before Task 8k (that covers
// the work up to the last label, not the work after it). Compute queue writes still always do.

#include <algorithm>
#include <vector>
#include "common/types.h"

namespace Vulkan {

class CpWriteOrder {
public:
    enum class Wait : u8 { No, Overlap, OtherWrite, CpWaited };
    /// What made the graphics queue's command processor wait (statistics only).
    enum class CpWait : u8 { PendingFence, PartialFlush, AcquireMem };

    /// A label deferred as operation `position` (Scheduler::DeferPriorityOperation), writing
    /// [address, address + size) (size 0: an interrupt only); `done`: the operations run so far
    /// (they run in order).
    void Deferred(u64 position, VAddr address, u32 size, u64 done) {
        std::erase_if(labels, [done](const Label& l) { return l.position <= done; });
        if (size != 0) {
            labels.push_back({position, address, size});
        }
        last = position;
    }

    /// The graphics queue's command processor waited here (handed over in order).
    void CpWaited() {
        cp_waited = last;
    }

    /// Whether a write of [address, address + size) waits, and why. `other_pending`: a deferred
    /// operation that is not a label has not run yet.
    Wait MustWait(VAddr address, u64 size, u64 done, bool other_pending) const {
        if (other_pending) {
            return Wait::OtherWrite;
        }
        if (done < cp_waited) {
            return Wait::CpWaited;
        }
        const bool overlap = std::ranges::any_of(labels, [&](const Label& l) {
            return l.position > done && l.address < address + size && address < l.address + l.size;
        });
        return overlap ? Wait::Overlap : Wait::No;
    }

private:
    struct Label {
        u64 position;
        VAddr address;
        u32 size;
    };
    std::vector<Label> labels; ///< deferred labels, pruned once written
    u64 last = 0;              ///< the latest label's position
    u64 cp_waited = 0;         ///< CP writes wait until the labels up to here are written
};

} // namespace Vulkan
