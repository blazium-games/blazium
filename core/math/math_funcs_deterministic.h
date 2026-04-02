#include "core/error/error_macros.h"
#include "core/math/fint.cpp"
#include "core/math/fint.h"
#include "core/math/math_funcs.h"
#include <cstdint>

namespace MathFI {
constexpr static const FInt NUM_360 = FInt::from(1474560);

constexpr static const FInt PI = FInt::from(12868);
constexpr static const FInt PI_X2 = FInt::from(25736);

constexpr static const FInt PI_DIV_2 = FInt::from(6434);

int64_t binary_sign(int64_t input);

bool is_equal_approx(const FInt v1, const FInt v2, FInt max_approx);

FInt min(const FInt v1, const FInt v2);

FInt max(const FInt v1, const FInt v2);

FInt lerp(const FInt start, const FInt end, const FInt progress);

FInt clamp(const FInt m_a, const FInt m_min, const FInt m_max);

FInt abs(const FInt flip, int64_t condition);
FInt abs(const FInt subj);
FInt abs(const FInt flip, bool condition);

FInt flip_sign(const FInt flip, int64_t condition);
FInt flip_sign(const FInt flip);
FInt flip_sign(const FInt flip, bool condition);

FInt floor(const FInt subj);

FInt ceil(const FInt subj);

FInt round(const FInt subj);

FInt snapped(FInt p_value, FInt p_step);

int64_t Q13Mul(const int64_t v1, const int64_t v2);

int64_t Q16Mul(const int64_t v1, const int64_t v2);

int64_t Q16Div(const int64_t v1, const int64_t v2);

FInt radians_to_degrees(FInt radians);

FInt degrees_to_radians(FInt degrees);

FInt sqrt(const FInt f);

FInt opt_sqrt(const FInt f);

uint64_t sqrtfx12(const uint64_t v);

FInt sin_d(const FInt degrees_arg);

FInt sin_r(const FInt degrees_arg);

FInt cos_d(const FInt degrees);

FInt cos_r(const FInt degrees);

FInt tan_d(const FInt degrees);

FInt tan_r(const FInt radians);

FInt atan_r(const FInt p);

FInt atan_d(const FInt p);

FInt atan2_r(const FInt in, const FInt inX);

FInt atan2_d(const FInt in, const FInt inX);

FInt atan_div(const FInt p_y, const FInt p_x);

int64_t MathFI::atan_sanitized(const int64_t p_x);

FInt MathFI::fp_sin_d(const FInt degrees);

FInt MathFI::fp_sin_r(const FInt radians);

int64_t MathFI::fp_sin(const int16_t value);
} //namespace MathFI