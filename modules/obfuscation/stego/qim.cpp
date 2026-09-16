/**************************************************************************/
/*  qim.cpp                                                               */
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

#include "stego/qim.h"

#include "core/math/math_funcs.h"

void ObfuscationQIM::embed(Vector<float> &p_coeffs, const Vector<uint8_t> &p_bits, const Vector<int> &p_indices, float p_delta) {
	if (p_delta <= 0.0001f) {
		p_delta = 2.0f;
	}
	for (int i = 0; i < p_bits.size() && i < p_indices.size(); i++) {
		int idx = p_indices[i];
		if (idx < 0 || idx >= p_coeffs.size()) {
			continue;
		}
		float x = p_coeffs[idx];
		int q = (int)Math::floor(x / p_delta);
		int bit = p_bits[i] & 1;
		if ((q & 1) != bit) {
			q += (x >= (q + 0.5f) * p_delta) ? 1 : -1;
		}
		p_coeffs.write[idx] = (q + 0.5f) * p_delta;
	}
}

void ObfuscationQIM::extract(const Vector<float> &p_coeffs, Vector<uint8_t> &r_bits, const Vector<int> &p_indices, float p_delta) {
	if (p_delta <= 0.0001f) {
		p_delta = 2.0f;
	}
	r_bits.resize(p_indices.size());
	for (int i = 0; i < p_indices.size(); i++) {
		int idx = p_indices[i];
		if (idx < 0 || idx >= p_coeffs.size()) {
			r_bits.write[i] = 0;
			continue;
		}
		int q = (int)Math::floor(p_coeffs[idx] / p_delta);
		r_bits.write[i] = (uint8_t)(q & 1);
	}
}
