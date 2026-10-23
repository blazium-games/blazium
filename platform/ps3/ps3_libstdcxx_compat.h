#pragma once

#include <errno.h>
#include <malloc.h>
#include <math.h>
#include <stdlib.h>

extern "C" {

inline int posix_memalign(void **memptr, size_t alignment, size_t size) {
	if (!memptr) {
		return EINVAL;
	}
	if (alignment < sizeof(void *)) {
		alignment = sizeof(void *);
	}
	if ((alignment & (alignment - 1)) != 0) {
		return EINVAL;
	}

	void *ptr = memalign(alignment, size);
	if (!ptr) {
		return ENOMEM;
	}
	*memptr = ptr;
	return 0;
}

} // extern "C"

namespace std {
using ::asinh;
using ::acosh;
using ::atanh;
using ::log1p;
using ::log2;
using ::round;
using ::rint;
using ::fmax;
using ::fmin;
using ::fabs;
using ::sqrt;
using ::sin;
using ::cos;
using ::tan;
using ::pow;
using ::exp;
using ::log;
using ::floor;
using ::ceil;
using ::trunc;
using ::hypot;
using ::copysign;
using ::nearbyint;
using ::ilogb;
using ::nextafter;
using ::strtoull;
using ::llabs;
using ::labs;
using ::fma;
using ::fmaf;
using ::remainder;
using ::remainderf;
using ::fmod;
using ::fmodf;
} // namespace std
