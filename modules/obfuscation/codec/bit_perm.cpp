/**************************************************************************/
/*  bit_perm.cpp                                                          */
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

#include "codec/bit_perm.h"

Vector<int> ObfuscationBitPerm::permutation(int p_count, uint32_t p_seed) {
	Vector<int> p;
	p.resize(p_count);
	for (int i = 0; i < p_count; i++) {
		p.write[i] = i;
	}
	uint32_t s = p_seed ? p_seed : 1;
	for (int i = p_count - 1; i > 0; i--) {
		s = s * 1664525u + 1013904223u;
		int j = (int)(s % (uint32_t)(i + 1));
		SWAP(p.write[i], p.write[j]);
	}
	return p;
}

void ObfuscationBitPerm::scramble(Vector<uint8_t> &p_bits, uint32_t p_seed) {
	Vector<int> perm = permutation(p_bits.size(), p_seed);
	Vector<uint8_t> out;
	out.resize(p_bits.size());
	for (int i = 0; i < p_bits.size(); i++) {
		out.write[perm[i]] = p_bits[i];
	}
	p_bits = out;
}

void ObfuscationBitPerm::unscramble(Vector<uint8_t> &p_bits, uint32_t p_seed) {
	Vector<int> perm = permutation(p_bits.size(), p_seed);
	Vector<uint8_t> out;
	out.resize(p_bits.size());
	for (int i = 0; i < p_bits.size(); i++) {
		out.write[i] = p_bits[perm[i]];
	}
	p_bits = out;
}
