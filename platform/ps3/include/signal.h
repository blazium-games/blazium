#pragma once

#include_next <signal.h>

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

int kill(pid_t pid, int sig);
int sigaction(int signo, const struct sigaction *act, struct sigaction *oldact);

#ifdef __cplusplus
}
#endif
