#pragma once

#include <stdarg.h>

#define LOG_EMERG 0
#define LOG_ALERT 1
#define LOG_CRIT 2
#define LOG_ERR 3
#define LOG_WARNING 4
#define LOG_NOTICE 5
#define LOG_INFO 6
#define LOG_DEBUG 7

#ifdef __cplusplus
extern "C" {
#endif

void openlog(const char *ident, int option, int facility);
void closelog(void);
void syslog(int priority, const char *format, ...);
void vsyslog(int priority, const char *format, va_list ap);

#ifdef __cplusplus
}
#endif
