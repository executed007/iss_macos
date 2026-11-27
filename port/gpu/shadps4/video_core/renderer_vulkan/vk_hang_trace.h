// SPDX-FileCopyrightText: Copyright 2026 inFAMOUS PC port
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// tlg: Task 8g. What each submitted tick contained, so a GPU hang report (vk_device_lost.h) says
// which work never finished. A fixed ring of 64-byte entries, one per draw, dispatch or
// transfer, written with one atomic add and no allocation. Apple only: elsewhere HANG_TRACE(...)
// is nothing.

#ifdef __APPLE__

#include <algorithm>
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
/// One crumb per tick (slot tick % CrumbSlots): Metal runs later command buffers after it
/// stops a hung one, and their crumbs must not hide how far the hung one got.
constexpr u32 CrumbSlots = 1024; ///< 4 KiB

struct Ring {
    std::array<Entry, RingSize> entries{};
    std::atomic<u64> next{0};
    /// TLG_GPU_HANG_TRACE: CrumbSlots u32s; for each tick the GPU writes the low 32 bits of the
    /// index of the last command it finished (a fill after it, behind a barrier), so the report
    /// shows which one hangs.
    std::atomic<const volatile u32*> crumb{nullptr};

    /// The index (low 32 bits) of the last command `tick` finished, 0xffffffff when none.
    u32 Crumb(u64 tick) const {
        const volatile u32* crumbs = crumb.load(std::memory_order_relaxed);
        return crumbs ? crumbs[tick % CrumbSlots] : 0xffffffffu;
    }

    /// The timeline semaphores, for the device-loss report: their last known GPU tick and the
    /// next tick to submit. shortcut: never unregistered, schedulers live until exit.
    struct Timeline {
        const void* owner;
        u64 (*known)(const void* owner);
        u64 (*current)(const void* owner);
    };
    std::array<Timeline, 4> timelines{};
    std::atomic<u32> num_timelines{0};

    u64 Add(const Entry& entry) {
        const u64 index = next.fetch_add(1, std::memory_order_relaxed);
        entries[index % RingSize] = entry;
        return index;
    }

    void Register(const Timeline& timeline) {
        const u32 slot = num_timelines.load(std::memory_order_relaxed);
        if (slot < timelines.size()) {
            timelines[slot] = timeline;
            num_timelines.store(slot + 1, std::memory_order_release);
        }
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

/// One entry, from its kind to the end of its line (no newline). Indirect args and buffer 0 are
/// read again: in pool mode they are what the GPU reads.
inline void PrintEntry(const Entry& e, std::FILE* out) {
    std::fprintf(out, "%s cs/vs %016llx ps %016llx prim %u flags %u a %u %u %u %u extra %#x",
                 KindName(e.kind), (unsigned long long)e.hash0, (unsigned long long)e.hash1,
                 e.prim, e.flags, e.a[0], e.a[1], e.a[2], e.a[3], e.extra);
    if (e.addr) {
        std::array<u32, 5> now{};
        if (ReadNow(e.addr, now.data(), sizeof(now))) {
            std::fprintf(out, " args@%#llx now %u %u %u %u %u", (unsigned long long)e.addr,
                         now[0], now[1], now[2], now[3], now[4]);
        } else {
            std::fprintf(out, " args@%#llx unreadable", (unsigned long long)e.addr);
        }
    }
    if (e.kind == Kind::Dispatch && e.hash1 != 0) {
        // Buffer 0's first dwords (often the constants that bound the shader's loops).
        std::array<u32, 4> head{};
        if (ReadNow(e.hash1, head.data(), sizeof(head))) {
            std::fprintf(out, " buf0 %#x %#x %#x %#x", head[0], head[1], head[2], head[3]);
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
}

/// Prints the entries `ring` still holds for (owner, tick); returns how many matched.
inline u32 Print(const Ring& ring, const void* owner, u64 tick, std::FILE* out,
                 u32 max_lines = 400) {
    const u64 end = ring.next.load(std::memory_order_relaxed);
    const u64 begin = end > RingSize ? end - RingSize : 0;
    u32 matched = 0, tessellated = 0, indirect = 0;
    const bool crumbs = ring.crumb.load(std::memory_order_relaxed) != nullptr;
    const u32 finished = ring.Crumb(tick);
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
        std::fprintf(out, "  #%u ", matched - 1);
        PrintEntry(e, out);
        if (crumbs && u32(i) == finished) {
            std::fputs(" <- the last command the GPU finished", out);
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

/// TLG_GPU_HANG_TRACE: the last command of (owner, tick) the GPU marked finished (a dispatch, a
/// render pass's last draw, or with TLG_GPU_HANG_TRACE=2 every draw) and the `after` commands
/// recorded after it, whatever their tick: the stuck command is the first of those.
inline void PrintCrumb(const Ring& ring, const void* owner, u64 tick, std::FILE* out,
                       u32 after = 48) {
    if (!ring.crumb.load(std::memory_order_relaxed)) {
        return;
    }
    const u32 finished = ring.Crumb(tick);
    const u64 end = ring.next.load(std::memory_order_relaxed);
    const u64 begin = end > RingSize ? end - RingSize : 0;
    u64 first = end, done = end;
    for (u64 i = begin; i < end; ++i) {
        const Entry& e = ring.entries[i % RingSize];
        if (e.owner == owner && e.tick == tick) {
            first = std::min(first, i);
            done = u32(i) == finished ? i : done;
        }
    }
    if (first == end) {
        return; // the ring holds nothing of this tick
    }
    if (done == end) {
        std::fprintf(out, "GPU hang trace: tick %llu finished none of its commands; the first:\n",
                     (unsigned long long)tick);
    } else {
        std::fprintf(out, "GPU hang trace: tick %llu finished command %llu and all before it; it "
                          "and the next:\n",
                     (unsigned long long)tick, (unsigned long long)done);
    }
    const u64 from = done == end ? first : done;
    for (u64 i = from; i < end && i <= from + after; ++i) {
        const Entry& e = ring.entries[i % RingSize];
        std::fprintf(out, "  %s tick %llu ", i == done ? "done" : "next",
                     (unsigned long long)e.tick);
        PrintEntry(e, out);
        std::fputc('\n', out);
    }
}

/// Device lost (whichever stop fires first): for each timeline, the tick the GPU was last seen to
/// complete and every submitted tick after it the ring still holds (the first one in full, the
/// last known tick may be stale). MoltenVK signals the awaited timeline values when it loses the
/// device (MVKDevice::markLost), so the stuck tick can read as completed: the stop on a 5 s wait
/// (Semaphore::Wait) names it reliably, this report does not. CheckDeviceLost lets only one
/// thread call it.
inline void PrintOnLoss(Ring& ring, std::FILE* out) {
    const u32 count = ring.num_timelines.load(std::memory_order_acquire);
    for (u32 t = 0; t < count; ++t) {
        const Ring::Timeline& timeline = ring.timelines[t];
        const u64 known = timeline.known(timeline.owner);
        const u64 current = timeline.current(timeline.owner);
        if (current <= known + 1) {
            continue; // nothing submitted is unfinished
        }
        std::fprintf(out, "GPU lost: timeline %u completed up to tick %llu (last read), %llu "
                          "submitted\n",
                     t, (unsigned long long)known, (unsigned long long)(current - 1));
        // The ring holds ~20 submissions: older ticks would print nothing.
        for (u64 tick = std::max(known + 1, current - std::min<u64>(current, 64)); tick < current;
             ++tick) {
            Print(ring, timeline.owner, tick, out, tick == known + 1 ? 400 : 0);
        }
        PrintCrumb(ring, timeline.owner, known + 1, out);
    }
    std::fflush(out);
}

} // namespace Vulkan::HangTrace

#define HANG_TRACE(...) ::Vulkan::HangTrace::Global().Add(::Vulkan::HangTrace::Entry{__VA_ARGS__})

#else
#define HANG_TRACE(...) ((void)0)
#endif
