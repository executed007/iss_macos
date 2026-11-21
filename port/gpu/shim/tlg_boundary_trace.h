// SPDX-License-Identifier: GPL-2.0-or-later
// tlg: TLG_BOUNDARY_TRACE=<file> (macOS only) — timestamps of the serial path at frame boundaries
// (Task 8n): a label the GPU's work completes, the game's next submission, stage A, stage B's
// vkQueueSubmit. Events go to <file> as 32-byte records (uptime ns, kind, size, thread, a, b);
// a sync point is also emitted as an os_signpost every 200 ms, so a Metal System Trace recorded
// with the os_signpost instrument lines the GPU's timeline up with it. Off: one load per call.
#pragma once

#include <cstdint>

namespace TlgBoundary {
enum Kind : std::uint16_t {
    Sync = 1,         ///< a = the same time, as sent in the os_signpost
    LabelDeferred,    ///< stage B deferred a fence to GPU completion: a = tick
    LabelReady,       ///< priority runner: the tick is done (vkWaitSemaphores returned): a = tick
    OpDone,           ///< priority runner: the operation ran: a = tick
    EopWrite,         ///< a = address, b = value, size = bytes
    EosWrite,         ///< a = address, b = value, size = bytes
    CpWrite,          ///< WRITE_DATA: a = address, b = first 8 bytes, size = bytes
    Flip,             ///< the flip packet ran on stage B
    GameSubmit,       ///< sceGnmSubmit(AndFlip)CommandBuffers entry: a = count, size = 1 with flip
    GameSubmitQueued, ///< the same call queued its buffers for stage A
    GameSubmitDone,   ///< sceGnmSubmitDone entry
    GfxQueued,        ///< a graphics buffer queued for stage A: a = sequence number
    StageAStart,      ///< stage A starts that buffer: a = sequence number
    SubmitBegin,      ///< Scheduler::SubmitExecution: a = tick it signals, b = scheduler,
                      ///< size = draws and dispatches recorded
    QueueSubmit,      ///< just before vkQueueSubmit: a = tick, b = scheduler
    QueueSubmitDone,  ///< vkQueueSubmit returned: a = tick, b = scheduler
    Window,           ///< a frame-stats window ends (TLG_FRAME_STATS / ISS_FRAME_STATS)
    FeedIdle,         ///< draws submitted to an idle GPU (vk_feed_idle_gpu.h): a = tick, size = work
};
#ifdef __APPLE__
void Note(Kind kind, std::uint64_t a = 0, std::uint64_t b = 0, std::uint32_t size = 0) noexcept;
#else
inline void Note(Kind, std::uint64_t = 0, std::uint64_t = 0, std::uint32_t = 0) noexcept {}
#endif
} // namespace TlgBoundary
