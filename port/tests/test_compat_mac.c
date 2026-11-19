// macOS POSIX compat shims: gettid, clock_nanosleep, pthread_mutex_timedlock.
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <time.h>
#include "compat_mac.h"

static int64_t ns(const struct timespec *t) { return (int64_t)t->tv_sec * 1000000000 + t->tv_nsec; }
static int64_t now(clockid_t c) { struct timespec t; clock_gettime(c, &t); return ns(&t); }
static struct timespec at(int64_t v) { struct timespec t = {(time_t)(v / 1000000000), (long)(v % 1000000000)}; return t; }

static pid_t other_tid;
static void *tid_thread(void *a) { (void)a; other_tid = gettid(); return NULL; }
static pthread_mutex_t held = PTHREAD_MUTEX_INITIALIZER;
static int locked, release;
static void *holder(void *a) {
    (void)a;
    pthread_mutex_lock(&held);
    __atomic_store_n(&locked, 1, __ATOMIC_RELEASE);
    while (!__atomic_load_n(&release, __ATOMIC_ACQUIRE)) { struct timespec t = {0, 1000000}; nanosleep(&t, NULL); }
    pthread_mutex_unlock(&held);
    return NULL;
}

int main(void) {
    pthread_t th;
    assert(gettid() != 0);
    pthread_create(&th, NULL, tid_thread, NULL); pthread_join(th, NULL);
    assert(other_tid != 0 && other_tid != gettid());

    int64_t t0 = now(CLOCK_MONOTONIC);
    struct timespec d = at(t0 + 20000000);
    assert(clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &d, NULL) == 0);
    int64_t el = now(CLOCK_MONOTONIC) - t0;
    assert(el >= 20000000 && el < 200000000);

    t0 = now(CLOCK_REALTIME);
    d = at(t0 + 20000000);
    assert(clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &d, NULL) == 0);
    assert(now(CLOCK_REALTIME) - t0 >= 20000000);

    t0 = now(CLOCK_MONOTONIC);
    d = at(t0 - 1000000000);
    assert(clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &d, NULL) == 0);
    assert(now(CLOCK_MONOTONIC) - t0 < 5000000);

    d = at(20000000);
    t0 = now(CLOCK_MONOTONIC);
    assert(clock_nanosleep(CLOCK_MONOTONIC, 0, &d, NULL) == 0);
    assert(now(CLOCK_MONOTONIC) - t0 >= 20000000);

    pthread_mutex_t free_m = PTHREAD_MUTEX_INITIALIZER;
    d = at(now(CLOCK_REALTIME) + 30000000);
    assert(pthread_mutex_timedlock(&free_m, &d) == 0);

    pthread_create(&th, NULL, holder, NULL);
    while (!__atomic_load_n(&locked, __ATOMIC_ACQUIRE)) {}
    t0 = now(CLOCK_REALTIME);
    d = at(t0 + 30000000);
    assert(pthread_mutex_timedlock(&held, &d) == ETIMEDOUT);
    assert(now(CLOCK_REALTIME) - t0 >= 30000000);
    __atomic_store_n(&release, 1, __ATOMIC_RELEASE);
    pthread_join(th, NULL);
    return 0;
}
