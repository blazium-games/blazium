#pragma once

#include_next <fcntl.h>

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif
