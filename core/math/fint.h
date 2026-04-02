#pragma once

#include "core/error/error_macros.h"
#include "core/math/math_funcs.h"
#include <cstdint>

class String;

//https://stackoverflow.com/questions/605124/fixed-point-math-in-c
//Q12 deterministic number.
//Search 'Q notation' for more info.
struct [[nodiscard]] FInt {
	static const int32_t SHIFT_AMOUNT;
	static const int64_t ONE_RAW;
	static const FInt ZERO;
	static const FInt ONE;
	static const FInt HALF;
	static const FInt MAX_VALUE;
	static const FInt MIN_VALUE;

	int64_t raw_value;

	constexpr FInt(const FInt &other) :
			raw_value(other.raw_value) {}

	constexpr FInt() :
			raw_value(0) {}
	//Initializes with a whole number.
	constexpr FInt(int64_t interger) :
			raw_value(interger << FInt::SHIFT_AMOUNT) {}
	//Initializes number with interger being the whole part and decimals_x10000 being the decimals * 10000.
	// FInt(4, 1000) = 4.1 = 4.09985...
	//Optimized into unreadability.

	constexpr FInt(int64_t interger, short decimals_x10000) :
			raw_value(
					(interger << FInt::SHIFT_AMOUNT) + (((uint64_t)decimals_x10000 << FInt::SHIFT_AMOUNT) / 10000 + (decimals_x10000 < 3)) * (((interger < 0) * -1) | 1)) {}

	constexpr static FInt from(const int64_t raw_value) {
		FInt fodder;
		fodder.raw_value = raw_value;
		return fodder;
	}

	constexpr FInt operator+(const FInt p_d) const;
	constexpr FInt &operator+=(const FInt p_d);
	constexpr FInt operator-(const FInt p_d) const;
	constexpr FInt &operator-=(const FInt p_d);
	constexpr FInt &operator*=(const FInt p_d);
	constexpr FInt operator*(const FInt p_v1) const;
	constexpr FInt &operator/=(const FInt p_d);
	constexpr FInt operator/(const FInt p_d) const;

	constexpr FInt operator+(int64_t p_rvalue) const;
	constexpr FInt &operator+=(int64_t p_rvalue);
	constexpr FInt operator+(int32_t p_rvalue) const;
	constexpr FInt &operator+=(int32_t p_rvalue);

	constexpr FInt operator-(int64_t p_rvalue) const;
	constexpr FInt &operator-=(int64_t p_rvalue);
	constexpr FInt operator-(int32_t p_rvalue) const;
	constexpr FInt &operator-=(int32_t p_rvalue);

	constexpr FInt operator*(int64_t p_rvalue) const;
	constexpr FInt &operator*=(int64_t p_rvalue);
	constexpr FInt operator*(int32_t p_rvalue) const;
	constexpr FInt &operator*=(int32_t p_rvalue);

	constexpr FInt operator/(int64_t p_rvalue) const;
	constexpr FInt &operator/=(int64_t p_rvalue);
	constexpr FInt operator/(int32_t p_rvalue) const;
	constexpr FInt &operator/=(int32_t p_rvalue);

	constexpr FInt operator%(const FInt &p_v1) const;
	constexpr FInt &operator%=(const FInt p_v1);
	constexpr FInt operator%(int64_t p_rvalue) const;
	constexpr FInt &operator%=(int64_t p_rvalue);

	constexpr FInt operator>>(int32_t shift) const;
	constexpr FInt operator>>(int64_t shift) const;
	constexpr FInt &operator>>=(int32_t shift);
	constexpr FInt &operator>>=(int64_t shift);

	constexpr FInt operator<<(int32_t shift) const;
	constexpr FInt operator<<(int64_t shift) const;
	constexpr FInt &operator<<=(int32_t shift);
	constexpr FInt &operator<<=(int64_t shift);

	constexpr FInt operator-() const;

	constexpr bool operator==(const FInt &p_d) const;
	constexpr bool operator!=(const FInt &p_d) const;
	constexpr bool operator<(const FInt &p_d) const;
	constexpr bool operator<=(const FInt &p_d) const;
	constexpr bool operator>(const FInt &p_d) const;
	constexpr bool operator>=(const FInt &p_d) const;

	constexpr explicit operator int32_t() const { return (int32_t)(raw_value >> FInt::SHIFT_AMOUNT); }
	constexpr explicit operator int64_t() const { return raw_value >> FInt::SHIFT_AMOUNT; }

	constexpr explicit FInt(const int32_t sbj) :
			raw_value{ ((int64_t)sbj) << FInt::SHIFT_AMOUNT } {}
	constexpr explicit FInt(const int64_t sbj) :
			raw_value{ sbj << FInt::SHIFT_AMOUNT } {}
	operator String() const;
};

_FORCE_INLINE_ constexpr FInt &FInt::operator+=(const FInt p_d) {
	raw_value += p_d.raw_value;
	return *this;
}

_FORCE_INLINE_ constexpr FInt FInt::operator+(const FInt p_d) const {
	return { raw_value + p_d.raw_value };
}

_FORCE_INLINE_ constexpr FInt &FInt::operator-=(const FInt p_d) {
	raw_value -= p_d.raw_value;
	return *this;
}

_FORCE_INLINE_ constexpr FInt FInt::operator-(const FInt p_d) const {
	return { raw_value - p_d.raw_value };
}

_FORCE_INLINE_ constexpr FInt &FInt::operator*=(const FInt p_d) {
	raw_value = (raw_value * p_d.raw_value) >> SHIFT_AMOUNT;
	return *this;
}

_FORCE_INLINE_ constexpr FInt FInt::operator*(const FInt p_d) const {
	return { (raw_value * p_d.raw_value >> SHIFT_AMOUNT) };
}

_FORCE_INLINE_ constexpr FInt &FInt::operator/=(const FInt p_d) {
	raw_value = (raw_value << SHIFT_AMOUNT) / p_d.raw_value;
	return *this;
}

_FORCE_INLINE_ constexpr FInt FInt::operator/(const FInt p_d) const {
	return FInt::from((raw_value << SHIFT_AMOUNT) / p_d.raw_value);
}

//Foreign addition.

_FORCE_INLINE_ constexpr FInt &FInt::operator+=(int64_t p_ot) {
	raw_value += p_ot << FInt::SHIFT_AMOUNT;
	return *this;
}

_FORCE_INLINE_ constexpr FInt &FInt::operator+=(int32_t p_ot) {
	raw_value += ((int64_t)p_ot) << FInt::SHIFT_AMOUNT;
	return *this;
}

_FORCE_INLINE_ constexpr FInt FInt::operator+(int64_t p_ot) const {
	return FInt::from(raw_value + (p_ot << FInt::SHIFT_AMOUNT));
}

_FORCE_INLINE_ constexpr FInt FInt::operator+(int32_t p_ot) const {
	return FInt::from(raw_value + (p_ot << FInt::SHIFT_AMOUNT));
}

_FORCE_INLINE_ constexpr FInt operator+(int32_t p_ot, const FInt &p_det) {
	return p_det + (((int64_t)p_ot) << FInt::SHIFT_AMOUNT);
}

_FORCE_INLINE_ constexpr FInt operator+(int64_t p_ot, const FInt &p_det) {
	return p_det + (p_ot << FInt::SHIFT_AMOUNT);
}

//Foreign substraction.

_FORCE_INLINE_ constexpr FInt &FInt::operator-=(int64_t p_ot) {
	raw_value -= p_ot << FInt::SHIFT_AMOUNT;
	return *this;
}

_FORCE_INLINE_ constexpr FInt &FInt::operator-=(int32_t p_ot) {
	raw_value -= ((int64_t)p_ot) << FInt::SHIFT_AMOUNT;
	return *this;
}

_FORCE_INLINE_ constexpr FInt FInt::operator-(int64_t p_ot) const {
	return FInt::from(raw_value - (p_ot << FInt::SHIFT_AMOUNT));
}

_FORCE_INLINE_ constexpr FInt FInt::operator-(int32_t p_ot) const {
	return FInt::from(raw_value - (p_ot << FInt::SHIFT_AMOUNT));
}

_FORCE_INLINE_ constexpr FInt operator-(int32_t p_ot, const FInt &p_det) {
	return p_det - (((int64_t)p_ot) << FInt::SHIFT_AMOUNT);
}

_FORCE_INLINE_ constexpr FInt operator-(int64_t p_ot, const FInt &p_det) {
	return p_det - (p_ot << FInt::SHIFT_AMOUNT);
}

//Foreign multiplication.

_FORCE_INLINE_ constexpr FInt &FInt::operator*=(int64_t p_ot) {
	raw_value *= p_ot;
	return *this;
}

_FORCE_INLINE_ constexpr FInt &FInt::operator*=(int32_t p_ot) {
	raw_value *= p_ot;
	return *this;
}

_FORCE_INLINE_ constexpr FInt FInt::operator*(int64_t p_ot) const {
	return FInt::from(raw_value * p_ot);
}

_FORCE_INLINE_ constexpr FInt FInt::operator*(int32_t p_ot) const {
	return FInt::from(raw_value * p_ot);
}

_FORCE_INLINE_ constexpr FInt operator*(int32_t p_ot, const FInt &p_det) {
	return p_det * p_ot;
}

_FORCE_INLINE_ constexpr FInt operator*(int64_t p_ot, const FInt &p_det) {
	return p_det * p_ot;
}

//Foreign division

_FORCE_INLINE_ constexpr FInt &FInt::operator/=(int64_t p_ot) {
	raw_value *= p_ot;
	return *this;
}
_FORCE_INLINE_ constexpr FInt FInt::operator/(int64_t p_ot) const {
	return FInt::from(raw_value / p_ot);
}

_FORCE_INLINE_ constexpr FInt &FInt::operator/=(int32_t p_ot) {
	raw_value *= p_ot;
	return *this;
}

_FORCE_INLINE_ constexpr FInt FInt::operator/(int32_t p_ot) const {
	return FInt::from(raw_value / p_ot);
}

_FORCE_INLINE_ constexpr FInt FInt::operator%(const FInt &p_v1) const {
	return { raw_value % p_v1.raw_value };
}

_FORCE_INLINE_ constexpr FInt &FInt::operator%=(const FInt p_v1) {
	raw_value %= p_v1.raw_value;
	return *this;
}

_FORCE_INLINE_ constexpr FInt FInt::operator%(int64_t p_rvalue) const {
	return { raw_value % (p_rvalue << FInt::SHIFT_AMOUNT) };
}

_FORCE_INLINE_ constexpr FInt &FInt::operator%=(int64_t p_rvalue) {
	raw_value %= p_rvalue << FInt::SHIFT_AMOUNT;
	return *this;
}

_FORCE_INLINE_ constexpr FInt FInt::operator>>(int32_t shift) const {
	return { this->raw_value >> shift };
}

_FORCE_INLINE_ constexpr FInt FInt::operator>>(int64_t shift) const {
	return { this->raw_value >> shift };
}

constexpr FInt &FInt::operator>>=(int32_t shift) {
	raw_value >>= shift;
	return *this;
}

_FORCE_INLINE_ constexpr FInt &FInt::operator>>=(int64_t shift) {
	raw_value >>= shift;
	return *this;
}

_FORCE_INLINE_ constexpr FInt FInt::operator<<(int32_t shift) const {
	return { this->raw_value << shift };
}

_FORCE_INLINE_ constexpr FInt FInt::operator<<(int64_t shift) const {
	return { this->raw_value << shift };
}

_FORCE_INLINE_ constexpr FInt &FInt::operator<<=(int32_t shift) {
	raw_value <<= shift;
	return *this;
}

_FORCE_INLINE_ constexpr FInt &FInt::operator<<=(int64_t shift) {
	raw_value <<= shift;
	return *this;
}

_FORCE_INLINE_ constexpr FInt FInt::operator-() const {
	return { -raw_value };
}

_FORCE_INLINE_ constexpr bool FInt::operator==(const FInt &p_d) const {
	return raw_value == p_d.raw_value;
}

_FORCE_INLINE_ constexpr bool FInt::operator!=(const FInt &p_d) const {
	return raw_value != p_d.raw_value;
}

_FORCE_INLINE_ constexpr bool FInt::operator<(const FInt &p_d) const {
	return raw_value < p_d.raw_value;
}

_FORCE_INLINE_ constexpr bool FInt::operator<=(const FInt &p_d) const {
	return raw_value <= p_d.raw_value;
}

_FORCE_INLINE_ constexpr bool FInt::operator>(const FInt &p_d) const {
	return raw_value > p_d.raw_value;
}

_FORCE_INLINE_ constexpr bool FInt::operator>=(const FInt &p_d) const {
	return raw_value >= p_d.raw_value;
}

_FORCE_INLINE_ constexpr bool FInt::operator==(const FInt &p_d) const {
	return raw_value == p_d.raw_value;
}
