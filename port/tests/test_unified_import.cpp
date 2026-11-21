// SPDX-License-Identifier: GPL-2.0-or-later
// Without sparse binding (MoltenVK) guest memory reaches the GPU as imported host memory: the
// runtime's backing view of the pool, in chunks no larger than the driver's buffer limit
// (VideoCore::Unified::ImportedMemory, what BufferCache imports). The guest writes through its
// own views of the same pages, which page tracking may protect.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include "video_core/buffer_cache/unified_memory.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "vulkan_test_driver.h"

namespace {
constexpr u64 Chunk = 16_MB;
constexpr u64 PoolSize = 2 * Chunk + 8_MB; // three chunks, the last one shorter

// GPU copy of `size` bytes from pool offset `src` to `dst`, then the host barrier, and wait.
void GpuCopy(Vulkan::Scheduler& scheduler, const VideoCore::Unified::ImportedMemory& pool, u64 src,
             u64 dst, u64 size) {
    const auto [src_buffer, src_offset] = pool.Find(src, size);
    const auto [dst_buffer, dst_offset] = pool.Find(dst, size);
    assert(src_buffer && dst_buffer);
    scheduler.Record([=, s = src_buffer->Handle(), d = dst_buffer->Handle()](vk::CommandBuffer cmdbuf) {
        cmdbuf.copyBuffer(s, d, vk::BufferCopy{src_offset, dst_offset, size});
        const vk::MemoryBarrier2 barrier{
            .srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
            .srcAccessMask = vk::AccessFlagBits2::eMemoryWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eHost,
            .dstAccessMask = vk::AccessFlagBits2::eHostRead,
        };
        cmdbuf.pipelineBarrier2(vk::DependencyInfo{.memoryBarrierCount = 1, .pMemoryBarriers = &barrier});
    });
    scheduler.Finish();
}
} // namespace

int main() {
    using namespace VideoCore;
    if (SkipWithoutVulkanDriver()) {
        return 0;
    }
    Vulkan::Instance instance(0, std::getenv("TLG_VK_VALIDATION") != nullptr);
    if (instance.HasSparseBuffers() && !Unified::Enabled()) {
        std::puts("Sparse buffers supported, host memory import not enabled: skipped");
        return 0;
    }
    assert(instance.HasExternalMemoryHost());
    Vulkan::Scheduler scheduler(instance);

    // The pool as runtime_memory.c makes it: a shared memory object with two views.
    char name[32];
    std::snprintf(name, sizeof(name), "/tlg-import-test-%d", static_cast<int>(getpid()));
    const int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    assert(fd >= 0);
    shm_unlink(name);
    assert(ftruncate(fd, PoolSize) == 0);
    auto* backing = static_cast<u8*>(
        mmap(nullptr, PoolSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    auto* guest = static_cast<u8*>(
        mmap(nullptr, PoolSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    assert(backing != MAP_FAILED && guest != MAP_FAILED);

    const u64 usage_before = instance.GetDeviceMemoryUsage();
    Unified::ImportedMemory pool(instance, backing, 0, PoolSize, Chunk);
    if (pool.error) {
        std::printf("import failed: %s\n", pool.error);
    }
    assert(!pool.error && pool.Size() == PoolSize);
    // Guest memory, not the renderer's: drivers that count imports (MoltenVK) do not make the
    // texture cache think it is over budget. Counted as BufferCache::EnablePool counts it.
    instance.AddImportedBytes(static_cast<s64>(PoolSize));
    if (instance.CanReportMemoryUsage()) {
        assert(instance.GetDeviceMemoryUsage() < usage_before + PoolSize / 2);
    }

    // Chunks: offsets resolve into the chunk that holds them; ranges across a chunk end do not.
    const auto [first, first_offset] = pool.Find(100, 16);
    const auto [last, last_offset] = pool.Find(2 * Chunk + 4096, 4096);
    assert(first && first_offset == 100 && last && last != first && last_offset == 4096);
    assert(last->size_bytes == 8_MB);
    assert(!pool.Find(Chunk - 8, 16).first);
    assert(!pool.Find(PoolSize - 8, 16).first);
    assert(pool.Address(2 * Chunk + 4096) == last->BufferDeviceAddress() + 4096);

    // Guest CPU writes are what the GPU reads; GPU writes are what the guest reads.
    for (u32 i = 0; i < 1024; ++i) {
        reinterpret_cast<u32*>(guest + Chunk + 64_KB)[i] = 0x51ed0000 + i;
    }
    GpuCopy(scheduler, pool, Chunk + 64_KB, 2 * Chunk + 128_KB, 4096);
    for (u32 i = 0; i < 1024; ++i) {
        assert(reinterpret_cast<u32*>(guest + 2 * Chunk + 128_KB)[i] == 0x51ed0000 + i);
    }

    // Protection on the guest view (page tracking) does not reach the GPU or the backing view.
    u8* protected_page = guest + 1_MB;
    std::memset(backing + 1_MB, 0x3c, 16_KB);
    assert(mprotect(protected_page, 32_KB, PROT_NONE) == 0);
    GpuCopy(scheduler, pool, 1_MB, 1_MB + 16_KB, 16_KB);
    assert(backing[1_MB + 16_KB] == 0x3c && backing[1_MB + 32_KB - 1] == 0x3c);
    assert(mprotect(protected_page, 32_KB, PROT_READ | PROT_WRITE) == 0);
    assert(guest[1_MB + 16_KB + 77] == 0x3c);

    std::puts("Unified import: 3 chunks, guest and GPU writes coherent, protection ignored by the GPU");
    return 0;
}
