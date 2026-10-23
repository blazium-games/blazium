#pragma once

#include <sys/types.h>

typedef unsigned long rlim_t;

struct rlimit {
	rlim_t rlim_cur;
	rlim_t rlim_max;
};

#ifndef RLIMIT_STACK
#define RLIMIT_STACK 3
#endif

#ifdef __cplusplus
extern "C" {
#endif

int getrlimit(int resource, struct rlimit *rlp);

#ifdef __cplusplus
}
#endif
