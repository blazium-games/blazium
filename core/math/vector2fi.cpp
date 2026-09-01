#include "vector2fi.h"

#include "core/math/vector2i.h"
#include "core/string/ustring.h"

const Vector2FI Vector2FI::ZERO = Vector2FI{ FInt(0), FInt(0) };

Vector2FI Vector2FI::get_normal_clockwise(Vector2FI p_other) {
	return Vector2FI::from_angle_r((p_other - *this).angle_r()).inverted_xy();
}

void Vector2FI::invert_xy() {
	FInt f_x = y;
	FInt f_y = x;
	x = f_x;
	y = f_y;
}

Vector2FI Vector2FI::inverted_xy() {
	Vector2FI f = *this;
	f.invert_xy();

	return f;
}

// Returns angle of the vector in RADIANS.
FInt Vector2FI::angle_r() const {
	return MathFI::atan2_r(y, x);
}

// Returns angle of the vector in DEGREES.
FInt Vector2FI::angle_d() const {
	return MathFI::atan2_d(y, x);
}

// Makes a normalized vector from RADIANS.
Vector2FI Vector2FI::from_angle_r(FInt p_angle) {
	return Vector2FI(MathFI::cos_r(p_angle), MathFI::sin_r(p_angle));
}

// Makes a normalized vector from DEGREES.
Vector2FI Vector2FI::from_angle_d(FInt p_angle) {
	return Vector2FI(MathFI::cos_d(p_angle), MathFI::sin_d(p_angle));
}

//Vector length.
FInt Vector2FI::length() const {
	return MathFI::sqrt(x * x + y * y);
}

//Squared Vector length.
FInt Vector2FI::length_squared() const {
	return x * x + y * y;
}

//Mutates the vector into a normalized version.
void Vector2FI::normalize() {
	FInt l = x * x + y * y;
	if (l != FInt::ZERO) {
		l = MathFI::sqrt(l);
		x /= l;
		y /= l;
	}
}

//Creates a normalized version of the vector.
Vector2FI Vector2FI::normalized() const {
	Vector2FI v = *this;
	v.normalize();
	return v;
}

bool Vector2FI::is_normalized() const {
	// use length_squared() instead of length() to avoid sqrt(), makes it more stringent.
	return length_squared() == FInt::ONE;
}

//Returns the distance between 2 points elevated to the power of 2.
FInt Vector2FI::distance_squared_to(const Vector2FI &p_to) const {
	FInt x_diff = x - p_to.x;
	FInt y_diff = y - p_to.y;
	return x_diff * x_diff + y_diff * y_diff;
}

//Returns the distance between 2 points.
FInt Vector2FI::distance_to(const Vector2FI &p_to) const {
	FInt x_diff = x - p_to.x;
	FInt y_diff = y - p_to.y;
	return MathFI::sqrt(x_diff * x_diff + y_diff * y_diff);
}

//Returns the angle to the second point in RADIANS.
FInt Vector2FI::angle_r_to(const Vector2FI &p_vector2) const {
	return MathFI::atan2_r(cross(p_vector2), dot(p_vector2));
}

//Returns the angle to the second point in DEGREES.
FInt Vector2FI::angle_d_to(const Vector2FI &p_vector2) const {
	return MathFI::atan2_d(cross(p_vector2), dot(p_vector2));
}

//Returns the angle of the difference between 2 points in RADIANS.
FInt Vector2FI::angle_r_to_point(const Vector2FI &p_vector2) const {
	return (p_vector2 - *this).angle_r();
}

//Returns the angle of the difference between 2 points in DEGREES.
FInt Vector2FI::angle_d_to_point(const Vector2FI &p_vector2) const {
	return (p_vector2 - *this).angle_d();
}

//Returns the dot product.
//(the vector projected on the other vector, presumably a normal)
FInt Vector2FI::dot(const Vector2FI &p_other) const {
	return x * p_other.x + y * p_other.y;
}

//Returns the cross product.
FInt Vector2FI::cross(const Vector2FI &p_other) const {
	return x * p_other.y - y * p_other.x;
}

Vector2FI Vector2FI::sign() const {
	return Vector2FI(SIGN(x.raw_value), SIGN(y.raw_value));
}

//Rounds x and y to the lowest whole number.
Vector2FI Vector2FI::floor() const {
	return Vector2FI(MathFI::floor(x), MathFI::floor(y));
}

//Rounds x and y to the highest whole number.
Vector2FI Vector2FI::ceil() const {
	return Vector2FI(MathFI::ceil(x), MathFI::ceil(y));
}

//Rounds x and y to the nearest whole number.
Vector2FI Vector2FI::round() const {
	return Vector2FI(MathFI::round(x), MathFI::round(y));
}

//Rotates vector around 0,0 by the RADIANS provided.
Vector2FI Vector2FI::rotated_r(FInt p_by_r) const {
	FInt sine = MathFI::sin_r(p_by_r);
	FInt cosi = MathFI::cos_r(p_by_r);
	return Vector2FI(
			x * cosi - y * sine,
			x * sine + y * cosi);
}

//Rotates vector around 0,0 by the DEGREES provided.
Vector2FI Vector2FI::rotated_d(FInt p_by_d) const {
	FInt sine = MathFI::sin_d(p_by_d);
	FInt cosi = MathFI::cos_d(p_by_d);
	return Vector2FI(
			x * cosi - y * sine,
			x * sine + y * cosi);
}

//Returns a 2d projection
Vector2FI Vector2FI::project(const Vector2FI &p_to) const {
	return p_to * (dot(p_to) / p_to.length_squared());
}

//Clamps coordinates with minimum being
//p_min.coordinate and maximun being p_max.coordinate
Vector2FI Vector2FI::clamp(const Vector2FI &p_min, const Vector2FI &p_max) const {
	return Vector2FI(
			MathFI::clamp(x, p_min.x, p_max.x),
			MathFI::clamp(y, p_min.y, p_max.y));
}

//Clamps coordinate with a general minimum and maximum
//for both coordinates.
Vector2FI Vector2FI::clampf(FInt p_min, FInt p_max) const {
	return Vector2FI(
			MathFI::clamp(x, p_min, p_max),
			MathFI::clamp(y, p_min, p_max));
}

//Rounds coordinate to the nearest number divisable by p_step.coordinate.
Vector2FI Vector2FI::snapped(const Vector2FI &p_step) const {
	return Vector2FI(
			MathFI::snapped(x, p_step.x),
			MathFI::snapped(y, p_step.y));
}

//Rounds all coordinates to the nearest number divisable by p_step.
Vector2FI Vector2FI::snappedf(FInt p_step) const {
	return Vector2FI(
			MathFI::snapped(x, p_step),
			MathFI::snapped(y, p_step));
}

//Snaps the length of the vector back to p_len if
//the vector's length is higher than p_len.
Vector2FI Vector2FI::limit_length(FInt p_len) const {
	const FInt ls = length_squared();
	Vector2FI v = *this;
	if (ls > 0 && p_len * p_len < ls) {
		const FInt l = MathFI::sqrt(ls);
		//Had to use q16 here for better precision.
		int64_t q16_x = v.x.raw_value << 4;
		int64_t q16_y = v.y.raw_value << 4;
		const int64_t q16_l = l.raw_value << 4;
		const int64_t q16_p_len = p_len.raw_value << 4;
		q16_x = MathFI::Q16Div(q16_x, q16_l);
		q16_y = MathFI::Q16Div(q16_y, q16_l);
		q16_x = MathFI::Q16Mul(q16_x, q16_p_len);
		q16_y = MathFI::Q16Mul(q16_y, q16_p_len);
		v = Vector2FI(FInt{ q16_x >> 4 }, FInt{ q16_y >> 4 });
	}

	return v;
}

//Move this vector towards p_to by p_delta units. (p_delta functions as a length here)
Vector2FI Vector2FI::move_toward(const Vector2FI &p_to, FInt p_delta) const {
	Vector2FI v = *this;
	Vector2FI vd = p_to - v;
	FInt len = vd.length();
	return len <= p_delta ? p_to : v + vd / len * p_delta;
}

// slide returns the component of the vector along the given plane, specified by its normal vector.
Vector2FI Vector2FI::slide(const Vector2FI &p_normal) const {
#ifdef MATH_CHECKS
	ERR_FAIL_COND_V_MSG(!p_normal.is_normalized(), Vector2FI::ZERO, "The normal Vector2FI " + p_normal.operator String() + "must be normalized.");
#endif

	return *this - p_normal * dot(p_normal);
}

//Bounces velocity vector according to the normal.
Vector2FI Vector2FI::bounce(const Vector2FI &p_normal) const {
	return -reflect(p_normal);
}

Vector2FI Vector2FI::reflect(const Vector2FI &p_normal) const {
#ifdef MATH_CHECKS
	ERR_FAIL_COND_V_MSG(!p_normal.is_normalized(), Vector2FI::ZERO, "The normal Vector2 " + p_normal.operator String() + "must be normalized.");
#endif
	return p_normal * 2 * dot(p_normal) - *this;
}

//Determines if the vector is close enough to p_v with the tolerance max_approx.
bool Vector2FI::is_equal_approx(const Vector2FI &p_v, FInt max_approx) const {
	return MathFI::is_equal_approx(x, p_v.x, max_approx) && MathFI::is_equal_approx(y, p_v.y, max_approx);
}

//Returns wether or not 2 vectors are equal.
bool Vector2FI::is_same(const Vector2FI &p_v) const {
	return x == p_v.x & y == p_v.y;
}

Vector2FI::operator String() const {
	return "(" + (String)x + ", " + (String)y + ")";
}

Vector2FI::operator Vector2i() const {
	return Vector2i((int32_t)x, (int32_t)y);
}