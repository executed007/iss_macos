// SPDX-License-Identifier: GPL-2.0-or-later
// macOS: POSIX/Linux calls the port uses that Darwin lacks. Include after <pthread.h>/<time.h>.
#pragma once
#ifdef __APPLE__
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/types.h>
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
static inline int pthread_mutex_timedlock(pthread_mutex_t *m, const struct timespec *abs_realtime) {
    for (;;) {
        int e = pthread_mutex_trylock(m);
        if (e != EBUSY) return e;
        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        if (now.tv_sec > abs_realtime->tv_sec || (now.tv_sec == abs_realtime->tv_sec && now.tv_nsec >= abs_realtime->tv_nsec))
            return ETIMEDOUT;
        struct timespec nap = {0, 100000};
        nanosleep(&nap, NULL);
    }
}

// macOS names only the calling thread and takes no thread argument; other threads stay unnamed.
static inline int tlg_set_thread_name(pthread_t thread, const char *name) {
    return pthread_equal(thread, pthread_self()) ? pthread_setname_np(name) : 0;
}
#define pthread_setname_np(thread, name) tlg_set_thread_name(thread, name)
#endif
