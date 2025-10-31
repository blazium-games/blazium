#pragma once

#include <cstdint>
#include "core/error/error_macros.h"
#include "core/math/math_funcs.h"

//https://stackoverflow.com/questions/605124/fixed-point-math-in-c
//Q12 deterministic number.
//Search 'Q notation' for more info.
struct [[nodiscard]] FInt {
    static const int32_t SHIFT_AMOUNT;
    static const int64_t ONE_RAW;
	static const FInt ZERO;
	static const FInt ONE;
	static const FInt HALF;

    int64_t raw_value;

    constexpr FInt() :
			raw_value(0) {}
	constexpr FInt(int64_t interger) :
			raw_value(interger << FInt::SHIFT_AMOUNT) {}
    constexpr FInt(int64_t interger, int decimalCount) :
            raw_value(raw_value = (interger << FInt::SHIFT_AMOUNT) / decimalCount) {}
    
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

	constexpr explicit operator int32_t () const { return (int32_t)(raw_value >> FInt::SHIFT_AMOUNT); }
	constexpr explicit operator int64_t () const { return raw_value >> FInt::SHIFT_AMOUNT; }

	constexpr explicit FInt(const int32_t sbj) : raw_value{((int64_t) sbj) << FInt::SHIFT_AMOUNT}  {}
	constexpr explicit FInt(const int64_t sbj) : raw_value{sbj << FInt::SHIFT_AMOUNT}  {}
};

constexpr FInt &FInt::operator+=(const FInt p_d) {
    
	raw_value += p_d.raw_value;
	return *this;
}

constexpr FInt FInt::operator+(const FInt p_d) const {
	return { raw_value + p_d.raw_value };
}

constexpr FInt &FInt::operator-=(const FInt p_d) {
    raw_value -= p_d.raw_value;
	return *this;
}

constexpr FInt FInt::operator-(const FInt p_d) const {
	return { raw_value - p_d.raw_value };
}

constexpr FInt &FInt::operator*=(const FInt p_d) {
	raw_value = (raw_value * p_d.raw_value) >> SHIFT_AMOUNT;
	return *this;
}

constexpr FInt FInt::operator*(const FInt p_d) const {
	return { (raw_value * p_d.raw_value >> SHIFT_AMOUNT) };
}

constexpr FInt &FInt::operator/=(const FInt p_d) {
	raw_value = (raw_value << SHIFT_AMOUNT) / p_d.raw_value;
	return *this;
}

constexpr FInt FInt::operator/(const FInt p_d) const {
	return FInt { (raw_value << SHIFT_AMOUNT) / p_d.raw_value };
}

//Foreign addition.

constexpr FInt &FInt::operator+=(int64_t p_ot) {
	raw_value += p_ot << FInt::SHIFT_AMOUNT;
	return *this;
}

constexpr FInt &FInt::operator+=(int32_t p_ot) {
	raw_value += ((int64_t)p_ot) << FInt::SHIFT_AMOUNT;
	return *this;
}

constexpr FInt FInt::operator+(int64_t p_ot) const {
	return FInt { raw_value + (p_ot << FInt::SHIFT_AMOUNT) };
}

constexpr FInt FInt::operator+(int32_t p_ot) const {
	return FInt { raw_value + (p_ot << FInt::SHIFT_AMOUNT) };
}

constexpr FInt operator+(int32_t p_ot, const FInt &p_det) {
	return p_det + (((int64_t)p_ot) << FInt::SHIFT_AMOUNT);
}

constexpr FInt operator+(int64_t p_ot, const FInt &p_det) {
	return p_det + (p_ot << FInt::SHIFT_AMOUNT);
}

//Foreign substraction.

constexpr FInt &FInt::operator-=(int64_t p_ot) {
	raw_value -= p_ot << FInt::SHIFT_AMOUNT;
	return *this;
}

constexpr FInt &FInt::operator-=(int32_t p_ot) {
	raw_value -= ((int64_t)p_ot) << FInt::SHIFT_AMOUNT;
	return *this;
}

constexpr FInt FInt::operator-(int64_t p_ot) const {
	return FInt { raw_value - (p_ot << FInt::SHIFT_AMOUNT) };
}

constexpr FInt FInt::operator-(int32_t p_ot) const {
	return FInt { raw_value - (p_ot << FInt::SHIFT_AMOUNT) };
}

constexpr FInt operator-(int32_t p_ot, const FInt &p_det) {
	return p_det - (((int64_t)p_ot) << FInt::SHIFT_AMOUNT);
}

constexpr FInt operator-(int64_t p_ot, const FInt &p_det) {
	return p_det - (p_ot << FInt::SHIFT_AMOUNT);
}

//Foreign multiplication.

constexpr FInt &FInt::operator*=(int64_t p_ot) {
	raw_value *= p_ot;
	return *this;
}

constexpr FInt &FInt::operator*=(int32_t p_ot) {
	raw_value *= p_ot;
	return *this;
}

constexpr FInt FInt::operator*(int64_t p_ot) const {
	return FInt { raw_value * p_ot };
}

constexpr FInt FInt::operator*(int32_t p_ot) const {
	return FInt { raw_value * p_ot };
}

constexpr FInt operator*(int32_t p_ot, const FInt &p_det) {
	return p_det * p_ot;
}

constexpr FInt operator*(int64_t p_ot, const FInt &p_det) {
	return p_det * p_ot;
}

//Foreign division

constexpr FInt &FInt::operator/=(int64_t p_ot) {
	raw_value *= p_ot;
	return *this;
}

constexpr FInt FInt::operator/(int64_t p_ot) const {
	return FInt { raw_value / p_ot };
}

constexpr FInt &FInt::operator/=(int32_t p_ot) {
	raw_value *= p_ot;
	return *this;
}

constexpr FInt FInt::operator/(int32_t p_ot) const {
	return FInt { raw_value / p_ot };
}

constexpr FInt FInt::operator%(const FInt &p_v1) const {
	return {raw_value % p_v1.raw_value};
}

constexpr FInt FInt::operator%(int64_t p_rvalue) const {
	return {raw_value % (p_rvalue << FInt::SHIFT_AMOUNT)};
}

constexpr FInt &FInt::operator%=(int64_t p_rvalue) {
	raw_value %= p_rvalue;
	return *this;
}

constexpr FInt FInt::operator>>(int32_t shift) const {
	return { this->raw_value >> shift };
}

constexpr FInt FInt::operator>>(int64_t shift) const {
	return { this->raw_value >> shift };
}

constexpr FInt &FInt::operator>>=(int32_t shift) {
	raw_value >>= shift;
	return *this;
}

constexpr FInt &FInt::operator>>=(int64_t shift) {
	raw_value >>= shift;
	return *this;
}

constexpr FInt FInt::operator<<(int32_t shift) const {
	return { this->raw_value << shift };
}

constexpr FInt FInt::operator<<(int64_t shift) const {
	return { this->raw_value << shift };
}

constexpr FInt &FInt::operator<<=(int32_t shift) {
	raw_value <<= shift;
	return *this;
}

constexpr FInt &FInt::operator<<=(int64_t shift) {
	raw_value <<= shift;
	return *this;
}

constexpr FInt FInt::operator-() const {
	return { -raw_value };
}

constexpr bool FInt::operator==(const FInt &p_d) const {
	return raw_value == p_d.raw_value;
}

constexpr bool FInt::operator!=(const FInt &p_d) const {
	return raw_value != p_d.raw_value;
}

constexpr bool FInt::operator<(const FInt &p_d) const {
	return raw_value < p_d.raw_value;
}

constexpr bool FInt::operator<=(const FInt &p_d) const {
	return raw_value <= p_d.raw_value;
}

constexpr bool FInt::operator>(const FInt &p_d) const {
	return raw_value > p_d.raw_value;
}

constexpr bool FInt::operator>=(const FInt &p_d) const {
	return raw_value >= p_d.raw_value;
}

constexpr bool FInt::operator==(const FInt &p_d) const {
	return raw_value == p_d.raw_value;
}
