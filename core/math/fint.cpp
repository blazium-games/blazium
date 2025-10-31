#include "core/math/fint.h"

#include <cstdint>

const int32_t FInt::SHIFT_AMOUNT = 12;
const int64_t FInt::ONE_RAW = 1 << SHIFT_AMOUNT; //12 is 4096
const FInt FInt::ZERO = FInt{0};
const FInt FInt::ONE = FInt{FInt::ONE_RAW};
const FInt FInt::HALF = FInt{FInt::ONE_RAW >> 1};