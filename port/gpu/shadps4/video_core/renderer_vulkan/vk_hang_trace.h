// SPDX-FileCopyrightText: Copyright 2026 inFAMOUS PC port
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// tlg: Task 8g. What each submitted tick contained, so a GPU hang report (vk_device_lost.h) says
// which work never finished. A fixed ring of 64-byte entries, one per draw, dispatch or
// transfer, written with one atomic add and no allocation. Apple only: elsewhere HANG_TRACE(...)
// is nothing.

#ifdef __APPLE__

#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include "common/types.h"

namespace Vulkan::HangTrace {

enum class Kind : u8 {
    Draw,
    DrawIndexed,
    DrawIndirect,        ///< a[] = first slot's args (read on the host when recorded)
    DrawIndexedIndirect, ///< a[] = first slot's args
    Dispatch, ///< a[0..2] = groups, a[3] = buffer 0 records; hash1 = its address, extra = stride,
              ///< prim = threads per group in x
    DispatchIndirect,    ///< a[0..2] = groups (read on the host when recorded)
    CopyImage,           ///< a[] = src w, h, layers, dst layers; extra = dst w << 16 | h
    ClearImage,
    FillBuffer, ///< a[0] = bytes
    CopyBuffer, ///< a[0] = regions, a[1] = first region's bytes
};

enum Flags : u8 {
    Tessellated = 1, ///< TCS stage or QuadList/RectList (MoltenVK runs it as compute)
    CountBuffer = 2, ///< indirect count from a buffer (addr2)
    Square = 4,      ///< recorded twice (SquarePass)
};

struct Entry {
    const void* owner; ///< the scheduler's timeline semaphore
    u64 tick;
    u64 hash0; ///< vs or cs program hash
    u64 hash1; ///< ps program hash
    u64 addr;  ///< indirect args (guest), 0 when none
    std::array<u32, 4> a;
    u32 extra; ///< draws: render area width << 16 | height; indirect: max_count
    Kind kind;
    u8 prim = 0;
    u8 flags = 0;
    u8 pad = 0;
};
static_assert(sizeof(Entry) == 64);

constexpr u32 RingSize = 16384; ///< ~20 submissions of TLG_SUBMIT_DRAWS (1024) work; 1 MiB

struct Ring {
    std::array<Entry, RingSize> entries{};
    std::atomic<u64> next{0};
    /// TLG_GPU_HANG_TRACE=1: the GPU writes here the low 32 bits of the index of each dispatch
    /// it finished (a fill after it, behind a barrier), so the report shows which one hangs.
    std::atomic<const volatile u32*> crumb{nullptr};

    u64 Add(const Entry& entry) {
        const u64 index = next.fetch_add(1, std::memory_order_relaxed);
        entries[index % RingSize] = entry;
        return index;
    }
};

inline Ring& Global() {
    static Ring ring;
    return ring;
}

inline const char* KindName(Kind kind) {
    constexpr const char* names[] = {"draw",         "draw indexed", "draw indirect",
                                     "draw indexed indirect", "dispatch", "dispatch indirect",
                                     "copy image",   "clear image",  "fill buffer",
                                     "copy buffer"};
    return u32(kind) < std::size(names) ? names[u32(kind)] : "?";
}

/// Guest memory read that fails instead of faulting (the range may be unmapped by now).
inline bool ReadNow(u64 addr, void* dst, u64 size) {
    mach_vm_size_t got = 0;
    return addr && mach_vm_read_overwrite(mach_task_self(), addr, size,
                                          reinterpret_cast<mach_vm_address_t>(dst),
                                          &got) == KERN_SUCCESS &&
           got == size;
}

/// Prints the entries `ring` still holds for (owner, tick); returns how many matched.
/// Indirect args are read again: in pool mode they are what the GPU reads.
inline u32 Print(const Ring& ring, const void* owner, u64 tick, std::FILE* out,
                 u32 max_lines = 400) {
    const u64 end = ring.next.load(std::memory_order_relaxed);
    const u64 begin = end > RingSize ? end - RingSize : 0;
    u32 matched = 0, tessellated = 0, indirect = 0;
    const volatile u32* crumb = ring.crumb.load(std::memory_order_relaxed);
    const u32 finished = crumb ? *crumb : 0;
    for (u64 i = begin; i < end; ++i) {
        const Entry& e = ring.entries[i % RingSize];
        if (e.owner != owner || e.tick != tick) {
            continue;
        }
        tessellated += (e.flags & Tessellated) != 0;
        indirect += e.addr != 0;
        if (matched++ >= max_lines) {
            continue;
        }
        std::fprintf(out, "  #%u %s cs/vs %016llx ps %016llx prim %u flags %u a %u %u %u %u "
                          "extra %#x",
                     matched - 1, KindName(e.kind), (unsigned long long)e.hash0,
                     (unsigned long long)e.hash1, e.prim, e.flags, e.a[0], e.a[1], e.a[2], e.a[3],
                     e.extra);
        if (e.addr) {
            std::array<u32, 5> now{};
            if (ReadNow(e.addr, now.data(), sizeof(now))) {
                std::fprintf(out, " args@%#llx now %u %u %u %u %u", (unsigned long long)e.addr,
                             now[0], now[1], now[2], now[3], now[4]);
            } else {
                std::fprintf(out, " args@%#llx unreadable", (unsigned long long)e.addr);
            }
        }
        if (e.kind == Kind::Dispatch && e.hash1 != 0 && e.extra != 0) {
            // The first thread past buffer 0's records, if the dispatch has one: what it reads.
            const u64 threads = u64(e.a[0]) * e.prim;
            if (threads > e.a[3]) {
                std::array<u32, 2> past{};
                if (ReadNow(e.hash1 + u64(e.a[3]) * e.extra, past.data(), sizeof(past))) {
                    std::fprintf(out, " (%llu threads, past buffer 0's end: %#x %#x)",
                                 (unsigned long long)threads, past[0], past[1]);
                }
            }
        }
        if (crumb && u32(i) == finished) {
            std::fputs(" <- the last dispatch the GPU finished", out);
        }
        std::fputc('\n', out);
    }
    const bool complete = begin == 0 || [&] {
        const Entry& oldest = ring.entries[begin % RingSize];
        return oldest.owner != owner || oldest.tick < tick;
    }();
    std::fprintf(out, "GPU hang trace: tick %llu held %u commands (%u tessellated, %u indirect)%s\n",
                 (unsigned long long)tick, matched, tessellated, indirect,
                 complete ? "" : ", the ring lost its start");
    return matched;
}

} // namespace Vulkan::HangTrace

#define HANG_TRACE(...) ::Vulkan::HangTrace::Global().Add(::Vulkan::HangTrace::Entry{__VA_ARGS__})

#else
#define HANG_TRACE(...) ((void)0)
#endif
