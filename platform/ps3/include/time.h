#pragma once

#include_next <time.h>

#include <sys/time.h>
#include <errno.h>

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

#ifndef CLOCK_MONOTONIC_RAW
#define CLOCK_MONOTONIC_RAW 4
#endif

#ifdef __cplusplus
extern "C" {
#endif

static inline int clock_gettime(int clk_id, struct timespec *tp) {
	if (!tp) {
		return -1;
	}

	(void)clk_id;
	struct timeval tv;
	gettimeofday(&tv, NULL);
	tp->tv_sec = tv.tv_sec;
	tp->tv_nsec = (long)(tv.tv_usec * 1000L);
	return 0;
}

int nanosleep(const struct timespec *rqtp, struct timespec *rmtp);

#ifdef __cplusplus
}
#endif
