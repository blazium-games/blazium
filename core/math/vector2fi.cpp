#include "vector2fi.h"

const Vector2FI Vector2FI::ZERO = Vector2FI{FInt(0), FInt(0)};

FInt Vector2FI::angle_r() const {
	return MathFI::atan2_r(y, x);
}

Vector2FI Vector2FI::from_angle_r(real_t p_angle) {
	return Vector2FI(MathFI::cos_r(p_angle), MathFI::sin_r(p_angle));
}

Vector2FI Vector2FI::from_angle_d(real_t p_angle) {
	return Vector2FI(MathFI::cos_d(p_angle), MathFI::sin_d(p_angle));
}