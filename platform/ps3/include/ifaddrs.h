#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ifaddrs {
	struct ifaddrs *ifa_next;
	char *ifa_name;
	unsigned int ifa_flags;
	void *ifa_addr;
	void *ifa_netmask;
	void *ifa_ifu;
	void *ifa_data;
};

int getifaddrs(struct ifaddrs **ifap);
void freeifaddrs(struct ifaddrs *ifa);

#ifdef __cplusplus
}
#endif
