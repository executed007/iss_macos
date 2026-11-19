// The GPU stops with a readable reason on device loss instead of waiting forever (Task 8c).
#include <cassert>
#include <cstdio>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>
#include "video_core/renderer_vulkan/vk_device_lost.h"

int main() {
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
    std::puts("device lost: STOP with exit 21, other results return");
}
