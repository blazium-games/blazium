#include "fint.h"
#include "math_funcs_deterministic.h"

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

    constexpr Vector2FI (int32_t ix, int32_t iy) : x(FInt(ix)), y(FInt(iy)) {}
    constexpr Vector2FI (int64_t ix, int64_t iy) : x(FInt(ix)), y(FInt(iy)) {}
    constexpr Vector2FI (FInt ix, FInt iy) : x(ix), y(iy) {}

    Vector2FI min(const Vector2FI &p_vector2i) const {
		return Vector2FI(MIN(x, p_vector2i.x), MIN(y, p_vector2i.y));
	}

	Vector2FI max(const Vector2FI &p_vector2i) const {
		return Vector2FI(MAX(x, p_vector2i.x), MAX(y, p_vector2i.y));
	}

    FInt distance_squared_to(const Vector2FI &p_to) const {
        FInt dx = x - p_to.x;
        FInt dy = y - p_to.y;

        return dx*dx + dy*dy;
    }

    FInt distance_to(const Vector2FI &p_to) const {
        return MathFI::sqrt(distance_squared_to(p_to));
    }

    constexpr Vector2FI operator+(const Vector2FI& other) const;
    constexpr Vector2FI& operator+=(const Vector2FI& other);

    constexpr Vector2FI operator-(const Vector2FI& other) const;
    constexpr Vector2FI& operator-=(const Vector2FI& other);

    constexpr Vector2FI operator*(const Vector2FI& other) const;
    constexpr Vector2FI& operator*=(const Vector2FI& other);

    constexpr Vector2FI operator/(const Vector2FI& other) const;
    constexpr Vector2FI& operator/=(const Vector2FI& other);

    Vector2FI operator+(const FInt& scalar) const;
    Vector2FI& operator+=(const FInt& scalar);

    Vector2FI operator-(const FInt& scalar) const;
    Vector2FI& operator-=(const FInt& scalar);

    Vector2FI operator*(const FInt& scalar) const;
    Vector2FI& operator*=(const FInt& scalar);

    Vector2FI operator/(const FInt& scalar) const;
    Vector2FI& operator/=(const FInt& scalar);

    constexpr Vector2FI operator%(const Vector2FI &p_v1) const;
	constexpr Vector2FI operator%(int64_t p_rvalue) const;
	constexpr void operator%=(int64_t p_rvalue);
    FInt angle_r() const;
    Vector2FI from_angle_r(real_t p_angle);
    Vector2FI from_angle_d(real_t p_angle);
};

constexpr Vector2FI Vector2FI::operator+(const Vector2FI& other) const {
    return Vector2FI{x + other.x, y + other.y};
}

constexpr Vector2FI& Vector2FI::operator+=(const Vector2FI& other) {
    x += other.x;
    y += other.y;
    return *this;
}

constexpr Vector2FI Vector2FI::operator-(const Vector2FI& other) const {
    return Vector2FI{x - other.x, y - other.y};
}

constexpr Vector2FI& Vector2FI::operator-=(const Vector2FI& other) {
    x -= other.x;
    y -= other.y;
    return *this;
}

constexpr Vector2FI Vector2FI::operator*(const Vector2FI& other) const {
    return Vector2FI{x * other.x, y * other.y};
}

constexpr Vector2FI& Vector2FI::operator*=(const Vector2FI& other) {
    x *= other.x;
    y *= other.y;
    return *this;
}

constexpr Vector2FI Vector2FI::operator/(const Vector2FI& other) const {
    return Vector2FI{x / other.x, y / other.y};
}

constexpr Vector2FI& Vector2FI::operator/=(const Vector2FI& other) {
    x /= other.x;
    y /= other.y;
    return *this;
}

Vector2FI Vector2FI::operator+(const FInt& scalar) const {
    return Vector2FI{x + scalar, y + scalar};
}

Vector2FI& Vector2FI::operator+=(const FInt& scalar) {
    x += scalar;
    y += scalar;
    return *this;
}

// Vector scalar subtraction
Vector2FI Vector2FI::operator-(const FInt& scalar) const {
    return Vector2FI{x - scalar, y - scalar};
}

Vector2FI& Vector2FI::operator-=(const FInt& scalar) {
    x -= scalar;
    y -= scalar;
    return *this;
}
Vector2FI Vector2FI::operator*(const FInt& scalar) const {
    return Vector2FI{x * scalar, y * scalar};
}

Vector2FI& Vector2FI::operator*=(const FInt& scalar) {
    x *= scalar;
    y *= scalar;
    return *this;
}

// Vector scalar division
_FORCE_INLINE_ Vector2FI Vector2FI::operator/(const FInt& scalar) const {
    return Vector2FI{x / scalar, y / scalar};
}

_FORCE_INLINE_ Vector2FI& Vector2FI::operator/=(const FInt& scalar) {
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