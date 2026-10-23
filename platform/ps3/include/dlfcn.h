#pragma once

#define RTLD_LAZY 1
#define RTLD_NOW 2
#define RTLD_GLOBAL 0x100
#define RTLD_LOCAL 0
#define RTLD_DEEPBIND 0

#ifdef __cplusplus
extern "C" {
#endif

void *dlopen(const char *filename, int flag);
int dlclose(void *handle);
void *dlsym(void *handle, const char *symbol);
char *dlerror(void);

#ifdef __cplusplus
}
#endif
