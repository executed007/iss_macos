// A GPU hang report lists what the stuck tick contained (Task 8g): the ring keeps entries per
// (semaphore, tick), survives wrap-around and re-reads indirect args from memory.
#include <cassert>
#include <cstdio>
#include <memory>
#include <string>
#include "video_core/renderer_vulkan/vk_hang_trace.h"

using namespace Vulkan::HangTrace;

static std::string PrintToString(const Ring& ring, const void* owner, u64 tick, u32& matched) {
    std::FILE* out = std::tmpfile();
    assert(out);
    matched = Print(ring, owner, tick, out);
    std::rewind(out);
    std::string text;
    char buffer[512];
    for (size_t n; (n = std::fread(buffer, 1, sizeof(buffer), out)) > 0;) {
        text.append(buffer, n);
    }
    std::fclose(out);
    return text;
}

int main() {
    const int sem_a = 0, sem_b = 0;
    alignas(16) static u32 args[5] = {6, 1000, 0, 0, 0};
    {
        auto ring = std::make_unique<Ring>();
        ring->Add({&sem_a, 7, 0x11, 0x22, 0, {3, 1, 0, 0}, 0, Kind::Draw});
        ring->Add({&sem_a, 8, 0x33, 0x44, u64(reinterpret_cast<uintptr_t>(args)), {4, 1, 0, 0}, 1,
                   Kind::DrawIndirect, 19, Tessellated});
        ring->Add({&sem_b, 8, 0x55, 0, 0, {15, 15, 1, 0}, 0, Kind::Dispatch});
        ring->Add({&sem_a, 8, 0x66, 0, 0, {1, 1, 1, 0}, 0, Kind::Dispatch});
        args[0] = 4; // the GPU (or the guest) changed the args after recording
        u32 matched = 0;
        const std::string text = PrintToString(*ring, &sem_a, 8, matched);
        assert(matched == 2);
        assert(text.find("draw indirect cs/vs 0000000000000033") != std::string::npos);
        assert(text.find("prim 19 flags 1 a 4 1 0 0") != std::string::npos);
        assert(text.find("now 4 1000 0 0 0") != std::string::npos);
        assert(text.find("dispatch cs/vs 0000000000000066") != std::string::npos);
        assert(text.find("0000000000000055") == std::string::npos); // other scheduler
        assert(text.find("tick 8 held 2 commands (1 tessellated, 1 indirect)\n") !=
               std::string::npos);
        assert(text.find("lost its start") == std::string::npos);
    }
    {
        // Wrapped: only the newest RingSize entries remain, and the report says the tick's start
        // is gone. Unmapped args are reported, not read.
        auto ring = std::make_unique<Ring>();
        for (u32 i = 0; i < RingSize + 10; ++i) {
            ring->Add({&sem_a, 9, i, 0, i == RingSize + 9 ? 8ull : 0ull, {}, 0, Kind::Dispatch});
        }
        u32 matched = 0;
        const std::string text = PrintToString(*ring, &sem_a, 9, matched);
        assert(matched == RingSize);
        assert(text.find("lost its start") != std::string::npos);
        assert(text.find("args@0x8 unreadable") == std::string::npos); // past the 400 lines
        u32 none = 0;
        PrintToString(*ring, &sem_a, 10, none);
        assert(none == 0);
        ring->Add({&sem_a, 10, 1, 0, 8, {}, 0, Kind::DispatchIndirect});
        assert(PrintToString(*ring, &sem_a, 10, none).find("args@0x8 unreadable") !=
               std::string::npos);
    }
    {
        // Device lost: each registered timeline names its last completed tick and lists the
        // unfinished ones (the first in full).
        auto ring = std::make_unique<Ring>();
        struct Fake { u64 known, current; };
        static const Fake fake{5, 8}; // ticks 6 and 7 submitted, unfinished
        ring->Register({&fake, [](const void* f) { return static_cast<const Fake*>(f)->known; },
                        [](const void* f) { return static_cast<const Fake*>(f)->current; }});
        ring->Add({&fake, 5, 0x77, 0, 0, {}, 0, Kind::Dispatch});
        ring->Add({&fake, 6, 0x2bce4aee, 0, 0, {183, 1, 1, 11675}, 8, Kind::Dispatch, 64});
        ring->Add({&fake, 7, 0x11, 0x22, 0, {3, 1, 0, 0}, 0, Kind::Draw});
        std::FILE* out = std::tmpfile();
        PrintOnLoss(*ring, out); // once: CheckDeviceLost lets one thread report
        std::rewind(out);
        std::string text;
        char buffer[512];
        for (size_t n; (n = std::fread(buffer, 1, sizeof(buffer), out)) > 0;) {
            text.append(buffer, n);
        }
        std::fclose(out);
        assert(text.find("GPU lost: timeline 0 completed up to tick 5 (last read), 7 submitted\n") ==
               0);
        assert(text.find("dispatch cs/vs 000000002bce4aee") != std::string::npos);
        assert(text.find("tick 6 held 1 commands") != std::string::npos);
        assert(text.find("tick 7 held 1 commands") != std::string::npos);
        assert(text.find("0000000000000011") == std::string::npos); // tick 7: summary only
        assert(text.find("0000000000000077") == std::string::npos); // tick 5 completed
    }
    {
        // TLG_GPU_HANG_TRACE: the command a tick last finished and those after it, across ticks
        // (one crumb per tick: a later tick's crumb does not hide it); a dispatch shows buffer 0's
        // first dwords.
        auto ring = std::make_unique<Ring>();
        alignas(16) static u32 constants[4] = {7, 2, 3, 4};
        ring->Add({&sem_a, 3, 0x91, 0, 0, {}, 0, Kind::Draw});
        ring->Add({&sem_a, 3, 0x92, 0, 0, {}, 0, Kind::Draw}); // index 1: tick 3 finished it
        ring->Add({&sem_a, 3, 0x64ad2129, u64(reinterpret_cast<uintptr_t>(constants)), 0,
                   {32, 32, 1, 1}, 16, Kind::Dispatch, 16});
        ring->Add({&sem_a, 4, 0x94, 0, 0, {}, 0, Kind::Draw}); // index 3: tick 4 finished it
        static volatile u32 crumbs[CrumbSlots];
        for (auto& crumb : crumbs) {
            crumb = 0xffffffff;
        }
        crumbs[3] = 1;
        crumbs[4] = 3;
        ring->crumb = crumbs;
        auto crumb_text = [&](u64 tick) {
            std::FILE* out = std::tmpfile();
            PrintCrumb(*ring, &sem_a, tick, out);
            std::rewind(out);
            std::string text;
            char buffer[512];
            for (size_t n; (n = std::fread(buffer, 1, sizeof(buffer), out)) > 0;) {
                text.append(buffer, n);
            }
            std::fclose(out);
            return text;
        };
        std::string text = crumb_text(3);
        assert(text.find("tick 3 finished command 1 and all before it; it and the next:\n") !=
               std::string::npos);
        assert(text.find("0000000000000091") == std::string::npos); // before it
        assert(text.find("  done tick 3 draw cs/vs 0000000000000092") != std::string::npos);
        assert(text.find("  next tick 3 dispatch cs/vs 0000000064ad2129") != std::string::npos);
        assert(text.find("buf0 0x7 0x2 0x3 0x4") != std::string::npos);
        assert(text.find("  next tick 4 draw cs/vs 0000000000000094") != std::string::npos);
        u32 matched = 0; // the per-tick marker in the full listing
        assert(PrintToString(*ring, &sem_a, 3, matched)
                   .find("cs/vs 0000000000000092 ps 0000000000000000 prim 0 flags 0 a 0 0 0 0 "
                         "extra 0 <- the last command the GPU finished\n") != std::string::npos);
        crumbs[3] = 0xffffffff; // tick 3 finished nothing yet: its first commands
        text = crumb_text(3);
        assert(text.find("tick 3 finished none of its commands; the first:\n  next tick 3 draw "
                         "cs/vs 0000000000000091") != std::string::npos);
        assert(crumb_text(9).empty()); // the ring holds nothing of tick 9
    }
    std::puts("hang trace: per-tick entries, wrap-around, args re-read, device-loss report, "
              "last finished entry");
    return 0;
}
