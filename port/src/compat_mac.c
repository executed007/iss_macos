// SPDX-License-Identifier: GPL-2.0-or-later
// macOS implementations of compat_mac.h that need Mach headers (kept out of every runtime file).
#include "compat_mac.h"
#include <mach/mach_init.h>
#include <mach/mach_vm.h>

// Unlike Linux, a range with an unmapped page reads nothing rather than the part before it.
ssize_t process_vm_readv(pid_t pid, const struct iovec *local, unsigned long local_count, const struct iovec *remote,
                         unsigned long remote_count, unsigned long flags) {
    (void)pid; (void)flags;
    if (local_count != 1 || remote_count != 1) { errno = EINVAL; return -1; }
    mach_vm_size_t got = 0;
    const size_t size = local->iov_len < remote->iov_len ? local->iov_len : remote->iov_len;
    if (mach_vm_read_overwrite(mach_task_self(), (mach_vm_address_t)(uintptr_t)remote->iov_base, size,
                               (mach_vm_address_t)(uintptr_t)local->iov_base, &got) != KERN_SUCCESS) {
        errno = EFAULT;
        return -1;
    }
    return (ssize_t)got;
}
