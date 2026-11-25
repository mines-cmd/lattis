// LD_PRELOAD shim for tests/ui/stats_test.py: the wall clock starts at CLOCKSHIM_START (epoch seconds)
// and jumps CLOCKSHIM_JUMP seconds forward once CLOCKSHIM_AFTER seconds of real time have passed.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

static int (*real_clock_gettime)(clockid_t, struct timespec *);

static long env_seconds(const char *name)
{
    const char *value = getenv(name);
    return value ? atol(value) : 0;
}

static long shifted(void)
{
    static long start = -1;
    struct timespec real, mono;
    if (!real_clock_gettime)
        real_clock_gettime = dlsym(RTLD_NEXT, "clock_gettime");
    real_clock_gettime(CLOCK_REALTIME, &real);
    real_clock_gettime(CLOCK_MONOTONIC, &mono);
    if (start < 0)
        start = mono.tv_sec;
    long offset = env_seconds("CLOCKSHIM_START") - real.tv_sec;
    if (mono.tv_sec - start >= env_seconds("CLOCKSHIM_AFTER"))
        offset += env_seconds("CLOCKSHIM_JUMP");
    return offset;
}

int clock_gettime(clockid_t id, struct timespec *ts)
{
    if (!real_clock_gettime)
        real_clock_gettime = dlsym(RTLD_NEXT, "clock_gettime");
    int result = real_clock_gettime(id, ts);
    if (result == 0 && (id == CLOCK_REALTIME || id == CLOCK_REALTIME_COARSE) && getenv("CLOCKSHIM_START"))
        ts->tv_sec += shifted();
    return result;
}

int gettimeofday(struct timeval *tv, void *tz)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    if (tv) {
        tv->tv_sec = ts.tv_sec;
        tv->tv_usec = ts.tv_nsec / 1000;
    }
    return 0;
}

time_t time(time_t *out)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    if (out)
        *out = ts.tv_sec;
    return ts.tv_sec;
}
