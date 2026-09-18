/**************************************************************************/
/*  anticheat_ed25519.cpp                                                 */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            BLAZIUM ENGINE                              */
/*                          https://blazium.app                           */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
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

#include "anticheat_ed25519.h"

#include "tweetnacl.h"

#include "core/os/memory.h"

#include <cstring>

extern "C" void randombytes(unsigned char *x, unsigned long long n) {
	memset(x, 0, (size_t)n);
}

namespace {

int json_string(const String &body, const char *key, String &out) {
	const String needle = String("\"") + key + "\"";
	int p = body.find(needle);
	if (p < 0) {
		return 1;
	}
	p = body.find(":", p);
	if (p < 0) {
		return 1;
	}
	p = body.find("\"", p);
	if (p < 0) {
		return 1;
	}
	int e = body.find("\"", p + 1);
	if (e < 0) {
		return 1;
	}
	out = body.substr(p + 1, e - p - 1);
	return 0;
}

int b64_val(char c) {
	if (c >= 'A' && c <= 'Z') {
		return c - 'A';
	}
	if (c >= 'a' && c <= 'z') {
		return c - 'a' + 26;
	}
	if (c >= '0' && c <= '9') {
		return c - '0' + 52;
	}
	if (c == '+') {
		return 62;
	}
	if (c == '/') {
		return 63;
	}
	return -1;
}

} // namespace

bool anticheat_parse_runtime_sig(const String &p_body, String &r_sha256_hex, String &r_sig_b64) {
	r_sha256_hex = String();
	r_sig_b64 = String();
	if (json_string(p_body, "sha256", r_sha256_hex) != 0) {
		return false;
	}
	if (json_string(p_body, "sig", r_sig_b64) != 0) {
		return false;
	}
	return true;
}

int anticheat_b64_decode(const String &p_s, uint8_t *p_out, int p_cap) {
	CharString cs = p_s.utf8();
	const char *p = cs.get_data();
	int n = 0;
	int val = 0, bits = 0;
	for (int i = 0; p[i]; i++) {
		if (p[i] == '=' || p[i] == '\n' || p[i] == '\r' || p[i] == ' ') {
			continue;
		}
		int d = b64_val(p[i]);
		if (d < 0) {
			return -1;
		}
		val = (val << 6) | d;
		bits += 6;
		if (bits >= 8) {
			bits -= 8;
			if (n < p_cap) {
				p_out[n++] = (uint8_t)((val >> bits) & 0xff);
			}
		}
	}
	return n;
}

int anticheat_hex_decode(const String &p_s, uint8_t *p_out, int p_want) {
	String t = p_s.strip_edges();
	if (t.length() != p_want * 2) {
		return 1;
	}
	CharString cs = t.utf8();
	const char *p = cs.get_data();
	auto nib = [](char c) -> int {
		if (c >= '0' && c <= '9') {
			return c - '0';
		}
		if (c >= 'a' && c <= 'f') {
			return c - 'a' + 10;
		}
		if (c >= 'A' && c <= 'F') {
			return c - 'A' + 10;
		}
		return -1;
	};
	for (int i = 0; i < p_want; i++) {
		int hi = nib(p[i * 2]);
		int lo = nib(p[i * 2 + 1]);
		if (hi < 0 || lo < 0) {
			return 1;
		}
		p_out[i] = (uint8_t)((hi << 4) | lo);
	}
	return 0;
}

bool anticheat_ed25519_verify(const uint8_t p_pk[32], const uint8_t *p_msg, size_t p_msg_len, const uint8_t p_sig[64]) {
	if (!p_pk || !p_msg || !p_sig) {
		return false;
	}
	const unsigned long long n = 64ull + (unsigned long long)p_msg_len;
	unsigned char *sm = (unsigned char *)memalloc((int)n);
	unsigned char *m = (unsigned char *)memalloc((int)n);
	if (!sm || !m) {
		if (sm) {
			memfree(sm);
		}
		if (m) {
			memfree(m);
		}
		return false;
	}
	memcpy(sm, p_sig, 64);
	memcpy(sm + 64, p_msg, p_msg_len);
	unsigned long long mlen = 0;
	const int rc = crypto_sign_open(m, &mlen, sm, n, p_pk);
	memfree(sm);
	memfree(m);
	return rc == 0;
}
