// SPDX-License-Identifier: GPL-2.0-or-later
// macOS: POSIX/Linux calls the port uses that Darwin lacks. Include after <pthread.h>/<time.h>.
#pragma once
#ifdef __APPLE__
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <time.h>

#ifndef TIMER_ABSTIME
#define TIMER_ABSTIME 1
#endif

static inline pid_t gettid(void) {
    uint64_t id = 0;
    pthread_threadid_np(NULL, &id);
    return (pid_t)(uint32_t)id;
}

static inline int clock_nanosleep(clockid_t clock, int flags, const struct timespec *request, struct timespec *remain) {
    if (clock != CLOCK_MONOTONIC && clock != CLOCK_REALTIME) return EINVAL;
    struct timespec rel = *request;
    if (flags & TIMER_ABSTIME) {
        struct timespec now;
        clock_gettime(clock, &now);
        rel.tv_sec -= now.tv_sec;
        rel.tv_nsec -= now.tv_nsec;
        if (rel.tv_nsec < 0) { rel.tv_nsec += 1000000000L; rel.tv_sec--; }
        if (rel.tv_sec < 0) return 0;
        remain = NULL; // absolute sleeps restart from the deadline, not the remainder
    }
    return nanosleep(&rel, remain) ? errno : 0;
}

// shortcut: polls at 100 us, upgrade to a condvar if it shows up in profiles
static inline int tlg_poll_until(int (*attempt)(void *), void *lock, const struct timespec *abs_realtime) {
    for (;;) {
        int e = attempt(lock);
        if (e != EBUSY) return e;
        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        if (now.tv_sec > abs_realtime->tv_sec || (now.tv_sec == abs_realtime->tv_sec && now.tv_nsec >= abs_realtime->tv_nsec))
            return ETIMEDOUT;
        struct timespec nap = {0, 100000};
        nanosleep(&nap, NULL);
    }
}
static inline int tlg_mutex_try(void *lock) { return pthread_mutex_trylock((pthread_mutex_t *)lock); }
static inline int tlg_rwlock_try_write(void *lock) { return pthread_rwlock_trywrlock((pthread_rwlock_t *)lock); }
static inline int tlg_rwlock_try_read(void *lock) { return pthread_rwlock_tryrdlock((pthread_rwlock_t *)lock); }
static inline int pthread_mutex_timedlock(pthread_mutex_t *m, const struct timespec *abs_realtime) {
    return tlg_poll_until(tlg_mutex_try, m, abs_realtime);
}
static inline int pthread_rwlock_timedwrlock(pthread_rwlock_t *r, const struct timespec *abs_realtime) {
    return tlg_poll_until(tlg_rwlock_try_write, r, abs_realtime);
}
static inline int pthread_rwlock_timedrdlock(pthread_rwlock_t *r, const struct timespec *abs_realtime) {
    return tlg_poll_until(tlg_rwlock_try_read, r, abs_realtime);
}

// macOS names only the calling thread and takes no thread argument; other threads stay unnamed.
static inline int tlg_set_thread_name(pthread_t thread, const char *name) {
    return pthread_equal(thread, pthread_self()) ? pthread_setname_np(name) : 0;
}
#define pthread_setname_np(thread, name) tlg_set_thread_name(thread, name)

// Linux names for what Darwin has under other names.
#define CLOCK_MONOTONIC_COARSE CLOCK_MONOTONIC
#define CLOCK_REALTIME_COARSE CLOCK_REALTIME
#define PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP PTHREAD_RECURSIVE_MUTEX_INITIALIZER
#define st_atim st_atimespec
#define st_mtim st_mtimespec
#define st_ctim st_ctimespec
// shortcut: process times, not the thread's; use thread_info(THREAD_BASIC_INFO) if a game relies on it
#define RUSAGE_THREAD RUSAGE_SELF

static inline ssize_t getrandom(void *buffer, size_t size, unsigned flags) {
    (void)flags;
    arc4random_buf(buffer, size);
    return (ssize_t)size;
}

// Reads of this process's memory that fail instead of faulting (compat_mac.c; Linux: own pid, one iovec each side).
ssize_t process_vm_readv(pid_t pid, const struct iovec *local, unsigned long local_count, const struct iovec *remote,
                         unsigned long remote_count, unsigned long flags);

// shortcut: no public CPU-number call on macOS, always 0; fine while only GR2's sceKernelGetCurrentCpu asks
static inline int sched_getcpu(void) { return 0; }

static inline int pthread_getattr_np(pthread_t thread, pthread_attr_t *attr) {
    pthread_attr_init(attr);
    const size_t size = pthread_get_stacksize_np(thread);
    return pthread_attr_setstack(attr, (char *)pthread_get_stackaddr_np(thread) - size, size);
}
#endif
