/**************************************************************************/
/*  adpcm_writer.cpp                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* SPDX-License-Identifier: MIT                                           */
/**************************************************************************/

#include "adpcm_writer.h"

#include "core/math/math_funcs.h"

#include <cstring>

namespace ps2_cook {

static const int16_t kImaStep[89] = {
	7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
	19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
	50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
	130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
	337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
	876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
	2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
	5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
	15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

static const int8_t kImaIndex[16] = {
	-1, -1, -1, -1, 2, 4, 6, 8,
	-1, -1, -1, -1, 2, 4, 6, 8
};

static void append_u16(Vector<uint8_t> &o, uint16_t v) {
	o.push_back(uint8_t(v & 0xff));
	o.push_back(uint8_t(v >> 8));
}

static void append_u32(Vector<uint8_t> &o, uint32_t v) {
	append_u16(o, uint16_t(v & 0xffff));
	append_u16(o, uint16_t(v >> 16));
}

static void encode_ima(const Vector<int16_t> &p_pcm, Vector<uint8_t> &r_nibbles) {
	int pred = 0;
	int idx = 0;
	uint8_t acc = 0;
	bool hi = false;
	for (int i = 0; i < p_pcm.size(); i++) {
		const int sample = p_pcm[i];
		const int step = kImaStep[idx];
		int diff = sample - pred;
		uint8_t nibble = 0;
		if (diff < 0) {
			nibble = 8;
			diff = -diff;
		}
		if (diff >= step) {
			nibble |= 4;
			diff -= step;
		}
		if (diff >= (step >> 1)) {
			nibble |= 2;
			diff -= step >> 1;
		}
		if (diff >= (step >> 2)) {
			nibble |= 1;
		}
		int delta = step >> 3;
		if (nibble & 4) {
			delta += step;
		}
		if (nibble & 2) {
			delta += step >> 1;
		}
		if (nibble & 1) {
			delta += step >> 2;
		}
		if (nibble & 8) {
			pred -= delta;
		} else {
			pred += delta;
		}
		if (pred > 32767) {
			pred = 32767;
		}
		if (pred < -32768) {
			pred = -32768;
		}
		idx += kImaIndex[nibble];
		if (idx < 0) {
			idx = 0;
		}
		if (idx > 88) {
			idx = 88;
		}
		if (hi) {
			acc |= uint8_t(nibble << 4);
			r_nibbles.push_back(acc);
			acc = 0;
			hi = false;
		} else {
			acc = uint8_t(nibble & 0x0f);
			hi = true;
		}
	}
	if (hi) {
		r_nibbles.push_back(acc);
	}
}

static void finish_blob(const Vector<uint8_t> &p_nibbles, int p_rate, SfxBlob &r_out) {
	Vector<uint8_t> out;
	out.push_back('S');
	out.push_back('F');
	out.push_back('X');
	out.push_back(' ');
	append_u16(out, 1);
	append_u16(out, uint16_t(CLAMP(p_rate, 1, 65535)));
	append_u32(out, uint32_t(p_nibbles.size()));
	for (int i = 0; i < p_nibbles.size(); i++) {
		out.push_back(p_nibbles[i]);
	}
	r_out.bytes = out;
	r_out.sample_rate = p_rate > 0 ? p_rate : 22050;
}

Error write_sfx_beep(SfxBlob &r_out) {
	const int rate = 22050;
	const int samples = rate / 5; // 200 ms
	Vector<int16_t> pcm;
	pcm.resize(samples);
	for (int i = 0; i < samples; i++) {
		pcm.write[i] = ((i / 25) & 1) ? 12000 : -12000;
	}
	return write_sfx_pcm(pcm, rate, r_out);
}

Error write_sfx_pcm(const Vector<int16_t> &p_pcm, int p_sample_rate, SfxBlob &r_out) {
	if (p_pcm.is_empty()) {
		return write_sfx_beep(r_out);
	}
	Vector<uint8_t> nibbles;
	encode_ima(p_pcm, nibbles);
	if (nibbles.is_empty()) {
		return write_sfx_beep(r_out);
	}
	finish_blob(nibbles, p_sample_rate > 0 ? p_sample_rate : 22050, r_out);
	return OK;
}

} // namespace ps2_cook
