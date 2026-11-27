// The GPU stops with a readable reason on a GPU hang: a submitted tick not done in time (Task 8f).
// The GPU stops with a readable reason on device loss instead of waiting forever (Task 8c).
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include "video_core/renderer_vulkan/vk_device_lost.h"

static std::string RunChild(void (*body)(), int& exit_code) {
    int fds[2];
    assert(pipe(fds) == 0);
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        dup2(fds[1], 2);
        body();
        _exit(0); // not reached
    }
    close(fds[1]);
    std::string text;
    char buffer[256];
    for (ssize_t n; (n = read(fds[0], buffer, sizeof(buffer))) > 0;) {
        text.append(buffer, size_t(n));
    }
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status));
    exit_code = WEXITSTATUS(status);
    return text;
}

static void TestGpuHang() {
    // Submitted (tick below current) and waited past the limit: a hang.
    assert(Vulkan::IsGpuHang(143183, 143204, 6.0, 5.0));
    // Within the limit, or exactly at it: not yet.
    assert(!Vulkan::IsGpuHang(143183, 143204, 5.0, 5.0));
    assert(!Vulkan::IsGpuHang(143183, 143204, 1.0, 5.0));
    // Not submitted yet (the tick being recorded): never, however long.
    assert(!Vulkan::IsGpuHang(143204, 143204, 600.0, 5.0));
    // Limit 0 (TLG_GPU_HANG_SECONDS=0): never.
    assert(!Vulkan::IsGpuHang(143183, 143204, 600.0, 0.0));
    int code = 0;
    const std::string text = RunChild([] { Vulkan::StopGpuHang(143183, 143182, 143204, 6.0); }, code);
    assert(code == 21);
    assert(text == "GPU hang: tick 143183 submitted, the GPU completed up to tick 143182, 143203 "
                   "submissions so far\nSTOP: GPU hang (tick 143183, waited 6.0 s)\n");
}

static void TestMeshWalkBound() {
    // The four walk shaders with s10 as their only user SGPR: the walk is capped.
    for (const u64 hash : {0x3ab1ca58ull, 0xab140dd8ull, 0xabea73ddull, 0x5e6e49c1ull}) {
        assert(Vulkan::MeshWalkBound(hash, 1u << 10, 31269) == Vulkan::MeshWalkStepCap);
    }
    // Bounds under the cap, other shaders and other user-data layouts are unchanged.
    assert(Vulkan::MeshWalkBound(0x3ab1ca58, 1u << 10, 100) == 100);
    assert(Vulkan::MeshWalkBound(0x12345678, 1u << 10, 31269) == 31269);
    assert(Vulkan::MeshWalkBound(0x3ab1ca58, (1u << 10) | 1u, 31269) == 31269);
}

int main() {
    TestGpuHang();
    TestMeshWalkBound();
    // Results other than device loss return.
    Vulkan::CheckDeviceLost(vk::Result::eSuccess, "test");
    Vulkan::CheckDeviceLost(vk::Result::eTimeout, "test");
    Vulkan::CheckDeviceLost(vk::Result::eErrorOutOfDeviceMemory, "test");

    int fds[2];
    assert(pipe(fds) == 0);
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        dup2(fds[1], 2);
        Vulkan::CheckDeviceLost(vk::Result::eErrorDeviceLost, "semaphore wait");
        _exit(0); // not reached
    }
    close(fds[1]);
    char text[256] = {};
    size_t got = 0;
    for (ssize_t n; (n = read(fds[0], text + got, sizeof(text) - 1 - got)) > 0;) {
        got += size_t(n);
    }
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 21);
    assert(std::strcmp(text, "STOP: GPU device lost (semaphore wait: ErrorDeviceLost)\n") == 0);
    std::puts("device lost: STOP with exit 21, other results return; GPU hang: STOP after the limit; mesh walks capped");
}
