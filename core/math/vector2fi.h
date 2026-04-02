#include "core/error/error_macros.h"
#include "core/math/math_funcs.h"
#include "fint.h"
#include "math_funcs_deterministic.h"

struct Vector2i;

struct [[nodiscard]] Vector2FI {
	static const int AXIS_COUNT = 2;
	static const Vector2FI ZERO;

	enum Axis {
		AXIS_X,
		AXIS_Y,
	};

	union {
		// NOLINTBEGIN(modernize-use-default-member-init)
		struct {
			FInt x;
			FInt y;
		};

		struct {
			FInt width;
			FInt height;
		};

		FInt coord[2] = { 0 };
		// NOLINTEND(modernize-use-default-member-init)
	};

	constexpr Vector2FI() :
			x(FInt{ 0 }), y(FInt{ 0 }) {}
	constexpr Vector2FI(int32_t ix, int32_t iy) :
			x(FInt(ix)), y(FInt(iy)) {}
	constexpr Vector2FI(int64_t ix, int64_t iy) :
			x(FInt(ix)), y(FInt(iy)) {}
	constexpr Vector2FI(FInt ix, FInt iy) :
			x(ix), y(iy) {}

	Vector2FI min(const Vector2FI &p_vector2i) const {
		return Vector2FI(MIN(x, p_vector2i.x), MIN(y, p_vector2i.y));
	}

	Vector2FI max(const Vector2FI &p_vector2i) const {
		return Vector2FI(MAX(x, p_vector2i.x), MAX(y, p_vector2i.y));
	}

	FInt distance_squared_to(const Vector2FI &p_to) const;

	FInt distance_to(const Vector2FI &p_to) const;

	FInt angle_r_to(const Vector2FI &p_vector2) const;

	FInt angle_d_to(const Vector2FI &p_vector2) const;

	FInt angle_r_to_point(const Vector2FI &p_vector2) const;

	FInt angle_d_to_point(const Vector2FI &p_vector2) const;

	FInt dot(const Vector2FI &p_other) const;

	FInt cross(const Vector2FI &p_other) const;

	Vector2FI sign() const;

	Vector2FI floor() const;

	Vector2FI ceil() const;

	Vector2FI round() const;

	Vector2FI rotated_r(FInt p_by_r) const;

	Vector2FI rotated_d(FInt p_by_d) const;

	Vector2FI project(const Vector2FI &p_to) const;

	Vector2FI clamp(const Vector2FI &p_min, const Vector2FI &p_max) const;

	Vector2FI clampf(FInt p_min, FInt p_max) const;

	Vector2FI snapped(const Vector2FI &p_step) const;

	Vector2FI snappedf(FInt p_step) const;

	Vector2FI limit_length(FInt p_len) const;

	Vector2FI move_toward(const Vector2FI &p_to, FInt p_delta) const;

	Vector2FI slide(const Vector2FI &p_normal) const;

	Vector2FI bounce(const Vector2FI &p_normal) const;

	Vector2FI reflect(const Vector2FI &p_normal) const;

	bool is_equal_approx(const Vector2FI &p_v, FInt max_approx) const;

	bool is_same(const Vector2FI &p_v) const;

	operator String() const;
	operator Vector2i() const;

	constexpr Vector2FI operator+(const Vector2FI &other) const;
	constexpr Vector2FI &operator+=(const Vector2FI &other);

	constexpr Vector2FI operator-(const Vector2FI &other) const;
	constexpr Vector2FI &operator-=(const Vector2FI &other);

	constexpr _FORCE_INLINE_ Vector2FI operator*(const Vector2FI &other) const;
	constexpr _FORCE_INLINE_ Vector2FI &operator*=(const Vector2FI &other);

	constexpr _FORCE_INLINE_ Vector2FI operator/(const Vector2FI &other) const;
	constexpr _FORCE_INLINE_ Vector2FI &operator/=(const Vector2FI &other);

	constexpr Vector2FI operator+(const FInt &scalar) const;
	constexpr Vector2FI &operator+=(const FInt &scalar);

	constexpr Vector2FI operator-(const FInt &scalar) const;
	constexpr Vector2FI &operator-=(const FInt &scalar);

	constexpr _FORCE_INLINE_ Vector2FI operator*(const FInt &scalar) const;
	constexpr _FORCE_INLINE_ Vector2FI &operator*=(const FInt &scalar);

	constexpr _FORCE_INLINE_ Vector2FI operator/(const FInt &scalar) const;
	constexpr _FORCE_INLINE_ Vector2FI &operator/=(const FInt &scalar);

	constexpr _FORCE_INLINE_ Vector2FI operator%(const Vector2FI &p_v1) const;
	constexpr _FORCE_INLINE_ Vector2FI operator%(int64_t p_rvalue) const;
	constexpr _FORCE_INLINE_ void operator%=(int64_t p_rvalue);

	constexpr _FORCE_INLINE_ Vector2FI operator>>(int32_t shift) const;

	constexpr _FORCE_INLINE_ Vector2FI operator>>(int64_t shift) const;

	constexpr _FORCE_INLINE_ Vector2FI &operator>>=(int32_t shift);

	constexpr _FORCE_INLINE_ Vector2FI &operator>>=(int64_t shift);

	constexpr _FORCE_INLINE_ Vector2FI operator<<(int32_t shift) const;

	constexpr _FORCE_INLINE_ Vector2FI operator<<(int64_t shift) const;

	constexpr _FORCE_INLINE_ Vector2FI &operator<<=(int32_t shift);

	constexpr _FORCE_INLINE_ Vector2FI &operator<<=(int64_t shift);

	constexpr _FORCE_INLINE_ Vector2FI operator-() const;

	_FORCE_INLINE_ bool operator==(const Vector2FI &other) const;
	_FORCE_INLINE_ bool operator!=(const Vector2FI &other) const;

	[[nodiscard]] Vector2FI get_normal_clockwise(Vector2FI p_other);

	void invert_xy();

	[[nodiscard]] Vector2FI inverted_xy();

	FInt angle_r() const;
	FInt angle_d() const;
	Vector2FI from_angle_r(FInt p_angle);
	Vector2FI from_angle_d(FInt p_angle);
	FInt length() const;
	FInt length_squared() const;
	void normalize();
	[[nodiscard]] Vector2FI normalized() const;
	bool is_normalized() const;
};

constexpr Vector2FI Vector2FI::operator+(const Vector2FI &other) const {
	return Vector2FI{ x + other.x, y + other.y };
}

constexpr Vector2FI &Vector2FI::operator+=(const Vector2FI &other) {
	x += other.x;
	y += other.y;
	return *this;
}

constexpr Vector2FI Vector2FI::operator-(const Vector2FI &other) const {
	return Vector2FI{ x - other.x, y - other.y };
}

constexpr Vector2FI &Vector2FI::operator-=(const Vector2FI &other) {
	x -= other.x;
	y -= other.y;
	return *this;
}

_FORCE_INLINE_ constexpr Vector2FI Vector2FI::operator*(const Vector2FI &other) const {
	return Vector2FI{ x * other.x, y * other.y };
}

_FORCE_INLINE_ constexpr Vector2FI &Vector2FI::operator*=(const Vector2FI &other) {
	x *= other.x;
	y *= other.y;
	return *this;
}

_FORCE_INLINE_ constexpr Vector2FI Vector2FI::operator/(const Vector2FI &other) const {
	return Vector2FI{ x / other.x, y / other.y };
}

_FORCE_INLINE_ constexpr Vector2FI &Vector2FI::operator/=(const Vector2FI &other) {
	x /= other.x;
	y /= other.y;
	return *this;
}

constexpr Vector2FI Vector2FI::operator+(const FInt &scalar) const {
	return Vector2FI{ x + scalar, y + scalar };
}

constexpr Vector2FI &Vector2FI::operator+=(const FInt &scalar) {
	x += scalar;
	y += scalar;
	return *this;
}

// Vector scalar subtraction
constexpr Vector2FI Vector2FI::operator-(const FInt &scalar) const {
	return Vector2FI{ x - scalar, y - scalar };
}

constexpr Vector2FI &Vector2FI::operator-=(const FInt &scalar) {
	x -= scalar;
	y -= scalar;
	return *this;
}

_FORCE_INLINE_ constexpr Vector2FI Vector2FI::operator*(const FInt &scalar) const {
	return Vector2FI{ x * scalar, y * scalar };
}

_FORCE_INLINE_ constexpr Vector2FI &Vector2FI::operator*=(const FInt &scalar) {
	x *= scalar;
	y *= scalar;
	return *this;
}

// Vector scalar division
_FORCE_INLINE_ constexpr Vector2FI Vector2FI::operator/(const FInt &scalar) const {
	return Vector2FI{ x / scalar, y / scalar };
}

_FORCE_INLINE_ constexpr Vector2FI &Vector2FI::operator/=(const FInt &scalar) {
	x /= scalar;
	y /= scalar;
	return *this;
}

_FORCE_INLINE_ constexpr Vector2FI Vector2FI::operator%(const Vector2FI &p_v1) const {
	return Vector2FI(x % p_v1.x, y % p_v1.y);
}

_FORCE_INLINE_ constexpr Vector2FI Vector2FI::operator%(int64_t p_rvalue) const {
	return Vector2FI(x % FInt(p_rvalue), y % FInt(p_rvalue));
}

_FORCE_INLINE_ constexpr void Vector2FI::operator%=(int64_t p_rvalue) {
	x %= FInt(p_rvalue);
	y %= FInt(p_rvalue);
}

_FORCE_INLINE_ constexpr Vector2FI Vector2FI::operator>>(int32_t shift) const {
	return { this->x >> shift, this->y >> shift };
}

_FORCE_INLINE_ constexpr Vector2FI Vector2FI::operator>>(int64_t shift) const {
	return { this->x >> shift, this->y >> shift };
}

_FORCE_INLINE_ constexpr Vector2FI &Vector2FI::operator>>=(int32_t shift) {
	x >>= shift;
	y >>= shift;
	return *this;
}

_FORCE_INLINE_ constexpr Vector2FI &Vector2FI::operator>>=(int64_t shift) {
	x >>= shift;
	y >>= shift;
	return *this;
}

_FORCE_INLINE_ constexpr Vector2FI Vector2FI::operator<<(int32_t shift) const {
	return { this->x << shift, this->y << shift };
}

_FORCE_INLINE_ constexpr Vector2FI Vector2FI::operator<<(int64_t shift) const {
	return { this->x << shift, this->y << shift };
}

_FORCE_INLINE_ constexpr Vector2FI &Vector2FI::operator<<=(int32_t shift) {
	x <<= shift;
	y <<= shift;
	return *this;
}

_FORCE_INLINE_ constexpr Vector2FI &Vector2FI::operator<<=(int64_t shift) {
	x <<= shift;
	y <<= shift;
	return *this;
}

_FORCE_INLINE_ constexpr Vector2FI Vector2FI::operator-() const {
	return { -x, -y };
}

_FORCE_INLINE_ constexpr bool Vector2FI::operator==(const Vector2FI &other) const {
	return x == other.x & y == other.y;
}

_FORCE_INLINE_ constexpr bool Vector2FI::operator!=(const Vector2FI &other) const {
	return x != other.x & y != other.y;
}