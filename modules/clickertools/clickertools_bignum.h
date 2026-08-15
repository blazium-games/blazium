/**************************************************************************/
/*  clickertools_bignum.h                                                 */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "thirdparty/bignumpp/BigNum.hpp"

class BlaziumBigNum : public RefCounted {
	GDCLASS(BlaziumBigNum, RefCounted);

protected:
	BigNumber::BigNum bignum;

	static void _bind_methods();

public:
	BlaziumBigNum();
	BlaziumBigNum(BigNumber::BigNum p_bignum);

	Ref<BlaziumBigNum> add(Ref<BlaziumBigNum> p_other) const;
	Ref<BlaziumBigNum> sub(Ref<BlaziumBigNum> p_other) const;
	Ref<BlaziumBigNum> mul(Ref<BlaziumBigNum> p_other) const;
	Ref<BlaziumBigNum> div(Ref<BlaziumBigNum> p_other) const;
	Ref<BlaziumBigNum> abs_num() const;
	Ref<BlaziumBigNum> negate() const;

	bool is_greater_than(Ref<BlaziumBigNum> p_other) const;
	bool is_less_than(Ref<BlaziumBigNum> p_other) const;
	bool is_equal_to(Ref<BlaziumBigNum> p_other) const;
	int compare_to(Ref<BlaziumBigNum> p_other) const;

	static void set_default_max_digits(int p_max_digits);
	static int get_default_max_digits();
	static void set_default_print_precision(int p_precision);
	static int get_default_print_precision();

	static Ref<BlaziumBigNum> max_of(Ref<BlaziumBigNum> a, Ref<BlaziumBigNum> b);
	static Ref<BlaziumBigNum> min_of(Ref<BlaziumBigNum> a, Ref<BlaziumBigNum> b);

	void parse_mantissa_exponent(double p_mantissa, int64_t p_exponent);
	static Ref<BlaziumBigNum> from_mantissa_exponent(double p_mantissa, int64_t p_exponent);

	Ref<BlaziumBigNum> pow_float(double p_power) const;
	Ref<BlaziumBigNum> pow_int(int64_t p_power) const;
	Ref<BlaziumBigNum> root(int64_t p_n) const;
	Ref<BlaziumBigNum> sqroot() const;
	double log10() const;
	int64_t to_int() const;

	static Ref<BlaziumBigNum> exp(int64_t p_n);

	static Ref<BlaziumBigNum> get_max();
	static Ref<BlaziumBigNum> get_min();
	static Ref<BlaziumBigNum> get_inf();
	static Ref<BlaziumBigNum> get_nan();

	bool is_greater_than_or_equal_to(Ref<BlaziumBigNum> p_other) const;
	bool is_less_than_or_equal_to(Ref<BlaziumBigNum> p_other) const;
	bool is_approximately_equal(Ref<BlaziumBigNum> p_other, double p_tolerance = 1e-9) const;

	bool is_inf() const;
	bool is_nan() const;
	bool is_positive() const;
	bool is_negative() const;

	double get_mantissa() const;
	int64_t get_exponent() const;

	String as_string(int p_precision = -1) const;
	String to_pretty_string(int p_precision = -1) const;

	void parse_string(const String &p_string);
	void parse_float(double p_val);

	static Ref<BlaziumBigNum> from_string(const String &p_string);
	static Ref<BlaziumBigNum> from_float(double p_val);
	static Ref<BlaziumBigNum> from_bignum(const Ref<BlaziumBigNum> &p_other);

	BigNumber::BigNum get_bignum() const { return bignum; }
};
