#ifndef COMPAT_MMAN_H_
#define COMPAT_MMAN_H_

#include <sys/mman.h>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

#endif  // COMPAT_MMAN_H_
