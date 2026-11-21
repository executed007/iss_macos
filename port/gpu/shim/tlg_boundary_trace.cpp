// SPDX-License-Identifier: GPL-2.0-or-later
// tlg: TLG_BOUNDARY_TRACE (tlg_boundary_trace.h). macOS only.
#ifdef __APPLE__
#include "tlg_boundary_trace.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <os/signpost.h>
#include <pthread.h>
#include <sys/mman.h>
#include <thread>
#include <time.h>
#include <unistd.h>

namespace TlgBoundary {
namespace {
struct Event {
    std::uint64_t ns;
    std::uint16_t kind; ///< written last: 0 while the event is being filled in
    std::uint16_t size;
    std::uint32_t thread;
    std::uint64_t a, b;
};
static_assert(sizeof(Event) == 32);

// 64 MB of address space, touched as it fills: ~150 events per frame fill it in ~4 minutes.
constexpr std::size_t Capacity = std::size_t{1} << 21;

struct Trace {
    Event* events;
    std::atomic<std::size_t> next{0};
};

std::uint64_t Now() noexcept {
    return clock_gettime_nsec_np(CLOCK_UPTIME_RAW); // the clock Instruments' timeline uses
}

void Write(Trace& trace, Kind kind, std::uint64_t a, std::uint64_t b, std::uint32_t size) noexcept {
    const std::size_t i = trace.next.fetch_add(1, std::memory_order_relaxed);
    if (i >= Capacity) {
        return; // full: the file ends here
    }
    thread_local const std::uint32_t thread = [] {
        std::uint64_t id = 0;
        pthread_threadid_np(nullptr, &id);
        return static_cast<std::uint32_t>(id);
    }();
    Event& e = trace.events[i];
    e.ns = Now();
    e.size = static_cast<std::uint16_t>(std::min<std::uint32_t>(size, 0xffff));
    e.thread = thread;
    e.a = a;
    e.b = b;
    __atomic_store_n(&e.kind, static_cast<std::uint16_t>(kind), __ATOMIC_RELEASE);
}

void Writer(Trace& trace, std::FILE* file) {
    const os_log_t log = os_log_create("tlg.boundary", "sync");
    std::size_t written = 0;
    for (;;) {
        const std::uint64_t now = Now();
        Write(trace, Sync, now, 0, 0);
        os_signpost_event_emit(log, OS_SIGNPOST_ID_EXCLUSIVE, "tlg-sync", "%llu",
                               static_cast<unsigned long long>(now));
        const std::size_t end = std::min(trace.next.load(std::memory_order_relaxed), Capacity);
        std::size_t done = written;
        while (done < end && __atomic_load_n(&trace.events[done].kind, __ATOMIC_ACQUIRE) != 0) {
            ++done;
        }
        if (done > written) {
            std::fwrite(trace.events + written, sizeof(Event), done - written, file);
            std::fflush(file);
            written = done;
        }
        usleep(200'000);
    }
}

Trace* Open() {
    const char* path = std::getenv("TLG_BOUNDARY_TRACE");
    if (!path || !*path) {
        return nullptr;
    }
    std::FILE* file = std::fopen(path, "wb");
    void* memory = file ? mmap(nullptr, Capacity * sizeof(Event), PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)
                        : MAP_FAILED;
    if (memory == MAP_FAILED) {
        std::printf("Boundary trace: cannot open %s\n", path);
        if (file) {
            std::fclose(file);
        }
        return nullptr;
    }
    auto* trace = new Trace{static_cast<Event*>(memory)};
    std::thread([trace, file] { Writer(*trace, file); }).detach();
    std::printf("Boundary trace: %s\n", path);
    return trace;
}
} // namespace

void Note(Kind kind, std::uint64_t a, std::uint64_t b, std::uint32_t size) noexcept {
    static Trace* const trace = Open();
    if (trace) {
        Write(*trace, kind, a, b, size);
    }
}
} // namespace TlgBoundary
#endif
