#include "core/math/fint.h"

#include <cstdint>
#include "fint.h"
#include "core/string/ustring.h"

const int32_t FInt::SHIFT_AMOUNT = 12;
const int64_t FInt::ONE_RAW = 1 << SHIFT_AMOUNT; //12 is 4096
const FInt FInt::ZERO = FInt{0};
const FInt FInt::ONE = FInt{FInt::ONE_RAW};
const FInt FInt::HALF = FInt{FInt::ONE_RAW >> 1};

FInt::operator String() const {
    FInt here = *this;
    int64_t floored_whole = (int64_t) here;
    FInt decimals_x100000 = (here - floored_whole) * 100000;
    int64_t floored_decimals = (int64_t)decimals_x100000;
    String whole = String::num_int64(floored_decimals);
    String decimals_unwashed = String::num_int64(floored_decimals);
    int dec_end = decimals_unwashed.find_char('0', decimals_unwashed.length() - 1);
    String decimals;
    if (decimals_unwashed.length() > 1 & dec_end >= 0)
    {
        decimals = decimals_unwashed.substr(0, dec_end + 1);
    }
    else
    {
        decimals = decimals_unwashed;
    }

    return whole + '.' + decimals;
}