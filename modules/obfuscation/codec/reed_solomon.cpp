/**************************************************************************/
/*  reed_solomon.cpp                                                      */
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

#include "codec/reed_solomon.h"

#include <cstring>

namespace {
uint8_t gf_exp[512];
uint8_t gf_log[256];
bool gf_ready = false;
Vector<uint8_t> gen_cache;

void gf_init() {
	if (gf_ready) {
		return;
	}
	uint8_t x = 1;
	for (int i = 0; i < 255; i++) {
		gf_exp[i] = x;
		gf_log[x] = (uint8_t)i;
		x = (uint8_t)((x << 1) ^ ((x & 0x80) ? 0x1d : 0));
	}
	for (int i = 255; i < 512; i++) {
		gf_exp[i] = gf_exp[i - 255];
	}
	gf_log[0] = 0;
	gf_ready = true;
}

uint8_t gf_mul(uint8_t a, uint8_t b) {
	if (a == 0 || b == 0) {
		return 0;
	}
	return gf_exp[(int)gf_log[a] + (int)gf_log[b]];
}

uint8_t gf_pow(uint8_t a, int n) {
	if (a == 0) {
		return 0;
	}
	int e = ((int)gf_log[a] * n) % 255;
	if (e < 0) {
		e += 255;
	}
	return gf_exp[e];
}

uint8_t poly_eval(const uint8_t *p, int len, uint8_t x) {
	uint8_t y = p[0];
	for (int i = 1; i < len; i++) {
		y = gf_mul(y, x) ^ p[i];
	}
	return y;
}

void build_gen() {
	if (gen_cache.size() == ObfuscationReedSolomon::NSYM + 1) {
		return;
	}
	gf_init();
	gen_cache.resize(1);
	gen_cache.write[0] = 1;
	for (int i = 0; i < ObfuscationReedSolomon::NSYM; i++) {
		Vector<uint8_t> ng;
		ng.resize(gen_cache.size() + 1);
		for (int j = 0; j < ng.size(); j++) {
			ng.write[j] = 0;
		}
		uint8_t root = gf_pow(2, i);
		for (int j = 0; j < gen_cache.size(); j++) {
			ng.write[j] ^= gen_cache[j];
			ng.write[j + 1] ^= gf_mul(gen_cache[j], root);
		}
		gen_cache = ng;
	}
}

void encode_block(const uint8_t *msg, int k, uint8_t *out_n) {
	build_gen();
	const int nsym = ObfuscationReedSolomon::NSYM;
	for (int i = 0; i < k; i++) {
		out_n[i] = msg[i];
	}
	for (int i = 0; i < nsym; i++) {
		out_n[k + i] = 0;
	}
	for (int i = 0; i < k; i++) {
		uint8_t coef = out_n[i];
		if (coef == 0) {
			continue;
		}
		for (int j = 1; j < gen_cache.size(); j++) {
			if (i + j < k + nsym) {
				out_n[i + j] ^= gf_mul(gen_cache[j], coef);
			}
		}
	}
	for (int i = 0; i < k; i++) {
		out_n[i] = msg[i];
	}
}
} // namespace

void ObfuscationReedSolomon::encode(const Vector<uint8_t> &p_data, Vector<uint8_t> &r_out) {
	gf_init();
	const int k = N - NSYM;
	r_out.clear();
	int offset = 0;
	do {
		uint8_t msg[N - NSYM];
		memset(msg, 0, sizeof(msg));
		int take = 0;
		if (offset < p_data.size()) {
			take = MIN(k - 1, p_data.size() - offset);
		}
		msg[0] = (uint8_t)take;
		for (int i = 0; i < take; i++) {
			msg[1 + i] = p_data[offset + i];
		}
		uint8_t block[N];
		encode_block(msg, k, block);
		for (int i = 0; i < N; i++) {
			r_out.push_back(block[i]);
		}
		offset += take;
	} while (offset < p_data.size());
}

bool ObfuscationReedSolomon::decode(const Vector<uint8_t> &p_data, Vector<uint8_t> &r_out) {
	gf_init();
	r_out.clear();
	if (p_data.size() < N) {
		return false;
	}
	const int k = N - NSYM;
	for (int block_i = 0; block_i + N <= p_data.size(); block_i += N) {
		uint8_t block[N];
		for (int i = 0; i < N; i++) {
			block[i] = p_data[block_i + i];
		}
		bool clean = true;
		for (int i = 0; i < NSYM; i++) {
			if (poly_eval(block, N, gf_pow(2, i)) != 0) {
				clean = false;
				break;
			}
		}
		(void)clean;
		int take = block[0];
		if (take < 0 || take > k - 1) {
			return false;
		}
		for (int i = 0; i < take; i++) {
			r_out.push_back(block[1 + i]);
		}
	}
	return true;
}
