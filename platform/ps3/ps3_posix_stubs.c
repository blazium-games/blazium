#include "sys/ioctl.h"
#include "dlfcn.h"
#include "syslog.h"
#include "ifaddrs.h"
#include "poll.h"
#include "sys/statvfs.h"
#include "sys/stat.h"
#include "sys/resource.h"
#include "signal.h"

#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <netdb.h>

int ioctl(int fd, unsigned long request, ...) {
	(void)fd;
	(void)request;
	return -1;
}

void *dlopen(const char *filename, int flag) {
	(void)filename;
	(void)flag;
	return NULL;
}

int dlclose(void *handle) {
	(void)handle;
	return 0;
}

void *dlsym(void *handle, const char *symbol) {
	(void)handle;
	(void)symbol;
	return NULL;
}

char *dlerror(void) {
	return (char *)"Dynamic loading is unavailable on PS3";
}

void openlog(const char *ident, int option, int facility) {
	(void)ident;
	(void)option;
	(void)facility;
}

void closelog(void) {}

void syslog(int priority, const char *format, ...) {
	(void)priority;
	(void)format;
}

void vsyslog(int priority, const char *format, va_list ap) {
	(void)priority;
	(void)format;
	(void)ap;
}

int getifaddrs(struct ifaddrs **ifap) {
	if (ifap) {
		*ifap = NULL;
	}
	return -1;
}

void freeifaddrs(struct ifaddrs *ifa) {
	(void)ifa;
}

int statvfs(const char *path, struct statvfs *buf) {
	(void)path;
	if (buf) {
		memset(buf, 0, sizeof(*buf));
	}
	return -1;
}

int poll(struct pollfd *fds, unsigned long nfds, int timeout) {
	(void)fds;
	(void)nfds;
	(void)timeout;
	return -1;
}

int lstat(const char *path, struct stat *buf) {
	return stat(path, buf);
}

int nanosleep(const struct timespec *rqtp, struct timespec *rmtp) {
	(void)rqtp;
	if (rmtp) {
		memset(rmtp, 0, sizeof(*rmtp));
	}
	return 0;
}

int getrlimit(int resource, struct rlimit *rlp) {
	(void)resource;
	if (rlp) {
		memset(rlp, 0, sizeof(*rlp));
	}
	return 0;
}

int kill(pid_t pid, int sig) {
	(void)pid;
	(void)sig;
	return -1;
}

int sigaction(int signo, const struct sigaction *act, struct sigaction *oldact) {
	(void)signo;
	(void)act;
	(void)oldact;
	return 0;
}

pid_t waitpid(pid_t pid, int *status, int options) {
	(void)pid;
	(void)status;
	(void)options;
	errno = ECHILD;
	return -1;
}

pid_t setsid(void) {
	errno = ENOSYS;
	return -1;
}

int execvp(const char *file, char *const argv[]) {
	(void)file;
	(void)argv;
	errno = ENOSYS;
	return -1;
}

FILE *popen(const char *command, const char *type) {
	(void)command;
	(void)type;
	errno = ENOSYS;
	return NULL;
}

int pclose(FILE *stream) {
	(void)stream;
	errno = ENOSYS;
	return -1;
}

int pipe(int pipefd[2]) {
	(void)pipefd;
	errno = ENOSYS;
	return -1;
}

int dup2(int oldfd, int newfd) {
	(void)oldfd;
	(void)newfd;
	errno = ENOSYS;
	return -1;
}

int symlink(const char *target, const char *linkpath) {
	(void)target;
	(void)linkpath;
	errno = ENOSYS;
	return -1;
}

ssize_t readlink(const char *path, char *buf, size_t bufsiz) {
	(void)path;
	(void)buf;
	(void)bufsiz;
	errno = ENOSYS;
	return -1;
}

int fchmod(int fd, mode_t mode) {
	(void)fd;
	(void)mode;
	errno = ENOSYS;
	return -1;
}

int mkfifo(const char *pathname, mode_t mode) {
	(void)pathname;
	(void)mode;
	errno = ENOSYS;
	return -1;
}

long pathconf(const char *path, int name) {
	(void)path;
	(void)name;
	errno = ENOSYS;
	return -1;
}

/* M10: incremental network module stubs (builtin_enet uses LV2 sockets when linked). */
#ifndef EAI_FAIL
#define EAI_FAIL (-1)
#endif

int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res) {
	(void)node;
	(void)service;
	(void)hints;
	if (res) {
		*res = NULL;
	}
	return EAI_FAIL;
}

void freeaddrinfo(struct addrinfo *res) {
	(void)res;
}

const char *gai_strerror(int errcode) {
	(void)errcode;
	return "getaddrinfo unavailable on PS3";
}
