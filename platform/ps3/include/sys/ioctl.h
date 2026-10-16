#pragma once

#define _IOR(type, nr, size) (((type) << 8) | ((nr) << 4) | ((sizeof(size)) << 2))
#define FIONREAD _IOR('f', 127, int)

#ifdef __cplusplus
extern "C" {
#endif

int ioctl(int fd, unsigned long request, ...);

#ifdef __cplusplus
}
#endif
