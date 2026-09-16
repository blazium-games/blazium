/**************************************************************************/
/*  haar_dwt.cpp                                                          */
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

#include "stego/haar_dwt.h"

static void haar_1d_fwd(float *v, int n) {
	Vector<float> tmp;
	tmp.resize(n);
	while (n >= 2) {
		int half = n / 2;
		for (int i = 0; i < half; i++) {
			float a = v[i * 2];
			float b = v[i * 2 + 1];
			tmp.write[i] = (a + b) * 0.5f;
			tmp.write[half + i] = (a - b) * 0.5f;
		}
		for (int i = 0; i < n; i++) {
			v[i] = tmp[i];
		}
		n = half;
	}
}

static void haar_1d_inv(float *v, int n) {
	Vector<float> tmp;
	tmp.resize(n);
	int len = 2;
	while (len <= n) {
		int half = len / 2;
		for (int i = 0; i < half; i++) {
			float s = v[i];
			float d = v[half + i];
			tmp.write[i * 2] = s + d;
			tmp.write[i * 2 + 1] = s - d;
		}
		for (int i = 0; i < len; i++) {
			v[i] = tmp[i];
		}
		len *= 2;
	}
}

void ObfuscationHaarDWT::forward_2d(Vector<float> &p_data, int p_width, int p_height, int p_levels) {
	int w = p_width;
	int h = p_height;
	Vector<float> row;
	for (int level = 0; level < p_levels; level++) {
		row.resize(w);
		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				row.write[x] = p_data[y * p_width + x];
			}
			haar_1d_fwd(row.ptrw(), w);
			for (int x = 0; x < w; x++) {
				p_data.write[y * p_width + x] = row[x];
			}
		}
		Vector<float> col;
		col.resize(h);
		for (int x = 0; x < w; x++) {
			for (int y = 0; y < h; y++) {
				col.write[y] = p_data[y * p_width + x];
			}
			haar_1d_fwd(col.ptrw(), h);
			for (int y = 0; y < h; y++) {
				p_data.write[y * p_width + x] = col[y];
			}
		}
		w /= 2;
		h /= 2;
		if (w < 2 || h < 2) {
			break;
		}
	}
}

void ObfuscationHaarDWT::inverse_2d(Vector<float> &p_data, int p_width, int p_height, int p_levels) {
	int dims_w[8];
	int dims_h[8];
	int w = p_width;
	int h = p_height;
	int levels = 0;
	for (int level = 0; level < p_levels && w >= 2 && h >= 2; level++) {
		dims_w[level] = w;
		dims_h[level] = h;
		w /= 2;
		h /= 2;
		levels++;
	}
	for (int level = levels - 1; level >= 0; level--) {
		w = dims_w[level];
		h = dims_h[level];
		Vector<float> col;
		col.resize(h);
		for (int x = 0; x < w; x++) {
			for (int y = 0; y < h; y++) {
				col.write[y] = p_data[y * p_width + x];
			}
			haar_1d_inv(col.ptrw(), h);
			for (int y = 0; y < h; y++) {
				p_data.write[y * p_width + x] = col[y];
			}
		}
		Vector<float> row;
		row.resize(w);
		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				row.write[x] = p_data[y * p_width + x];
			}
			haar_1d_inv(row.ptrw(), w);
			for (int x = 0; x < w; x++) {
				p_data.write[y * p_width + x] = row[x];
			}
		}
	}
}
