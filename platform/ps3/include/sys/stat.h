#pragma once

#include_next <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

int lstat(const char *path, struct stat *buf);

#ifdef __cplusplus
}
#endif
