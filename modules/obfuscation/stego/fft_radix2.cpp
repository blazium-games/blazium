/**************************************************************************/
/*  fft_radix2.cpp                                                        */
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

#include "stego/fft_radix2.h"

#include "core/math/math_defs.h"
#include "core/math/math_funcs.h"

static void fft_inplace(Vector<ObfuscationComplex> &p_data, bool p_inverse) {
	int n = p_data.size();
	if (n < 2 || (n & (n - 1)) != 0) {
		return;
	}
	int j = 0;
	for (int i = 1; i < n; i++) {
		int bit = n >> 1;
		for (; j & bit; bit >>= 1) {
			j ^= bit;
		}
		j ^= bit;
		if (i < j) {
			SWAP(p_data.write[i], p_data.write[j]);
		}
	}
	for (int len = 2; len <= n; len <<= 1) {
		float ang = 2.0f * (float)Math::PI / (float)len * (p_inverse ? 1.0f : -1.0f);
		float wlen_re = Math::cos(ang);
		float wlen_im = Math::sin(ang);
		for (int i = 0; i < n; i += len) {
			float wre = 1.0f;
			float wim = 0.0f;
			for (int k = 0; k < len / 2; k++) {
				ObfuscationComplex u = p_data[i + k];
				ObfuscationComplex v = p_data[i + k + len / 2];
				float tmp_re = wre * v.re - wim * v.im;
				float tmp_im = wre * v.im + wim * v.re;
				p_data.write[i + k].re = u.re + tmp_re;
				p_data.write[i + k].im = u.im + tmp_im;
				p_data.write[i + k + len / 2].re = u.re - tmp_re;
				p_data.write[i + k + len / 2].im = u.im - tmp_im;
				float nwre = wre * wlen_re - wim * wlen_im;
				wim = wre * wlen_im + wim * wlen_re;
				wre = nwre;
			}
		}
	}
	if (p_inverse) {
		float inv = 1.0f / (float)n;
		for (int i = 0; i < n; i++) {
			p_data.write[i].re *= inv;
			p_data.write[i].im *= inv;
		}
	}
}

void ObfuscationFFT::forward(Vector<ObfuscationComplex> &p_data) {
	fft_inplace(p_data, false);
}

void ObfuscationFFT::inverse(Vector<ObfuscationComplex> &p_data) {
	fft_inplace(p_data, true);
}
