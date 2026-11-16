/**************************************************************************/
/*  vag_writer.cpp                                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* SPDX-License-Identifier: MIT                                           */
/**************************************************************************/

#include "vag_writer.h"

#include "core/typedefs.h"

namespace ps1_cook {

static void be32(Vector<uint8_t> &o, uint32_t v) {
	o.push_back(uint8_t((v >> 24) & 0xff));
	o.push_back(uint8_t((v >> 16) & 0xff));
	o.push_back(uint8_t((v >> 8) & 0xff));
	o.push_back(uint8_t(v & 0xff));
}

Error write_vag_beep(VagBlob &r_out) {
	const int blocks = 16; // 16 * 28 samples @ 22050 ≈ 20 ms
	const uint32_t data_size = uint32_t(blocks * 16);
	Vector<uint8_t> out;
	out.push_back('V');
	out.push_back('A');
	out.push_back('G');
	out.push_back('p');
	be32(out, 0x20);
	be32(out, 0);
	be32(out, data_size);
	be32(out, 22050);
	for (int i = 0; i < 12; i++) {
		out.push_back(0);
	}
	const char name[] = "BLAZIUMBEEP";
	for (int i = 0; i < 16; i++) {
		out.push_back(i < 11 ? uint8_t(name[i]) : 0);
	}
	for (int b = 0; b < blocks; b++) {
		out.push_back(0x0c); // filter 0, shift 12
		out.push_back(b == blocks - 1 ? 0x01 : 0x00);
		for (int i = 0; i < 14; i++) {
			out.push_back((b & 1) ? 0x77 : 0x88);
		}
	}
	r_out.bytes = out;
	r_out.sample_rate = 22050;
	return OK;
}

Error write_vag_pcm(const Vector<int16_t> &p_pcm, int p_sample_rate, int p_max_bytes, VagBlob &r_out, bool p_loop) {
	if (p_pcm.is_empty()) {
		return write_vag_beep(r_out);
	}
	const int rate = p_sample_rate > 0 ? p_sample_rate : 22050;
	const int max_bytes = MAX(48 + 16, p_max_bytes);
	int max_blocks = (max_bytes - 48) / 16;
	if (max_blocks < 1) {
		max_blocks = 1;
	}
	const int src_blocks = (p_pcm.size() + 27) / 28;
	const int blocks = MIN(max_blocks, MAX(1, src_blocks));
	const uint32_t data_size = uint32_t(blocks * 16);
	Vector<uint8_t> out;
	out.push_back('V');
	out.push_back('A');
	out.push_back('G');
	out.push_back('p');
	be32(out, 0x20);
	be32(out, 0);
	be32(out, data_size);
	be32(out, uint32_t(rate));
	for (int i = 0; i < 12; i++) {
		out.push_back(0);
	}
	const char name[] = "BLAZIUMWAV";
	for (int i = 0; i < 16; i++) {
		out.push_back(i < 10 ? uint8_t(name[i]) : 0);
	}
	for (int b = 0; b < blocks; b++) {
		int16_t smp[28];
		int max_abs = 0;
		for (int i = 0; i < 28; i++) {
			const int idx = b * 28 + i;
			const int16_t s = idx < p_pcm.size() ? p_pcm[idx] : 0;
			smp[i] = s;
			const int a = s < 0 ? -int(s) : int(s);
			if (a > max_abs) {
				max_abs = a;
			}
		}
		int shift = 0;
		while (shift < 12 && ((7 << 12) >> shift) < max_abs) {
			shift++;
		}
		out.push_back(uint8_t(shift));
		uint8_t flag = 0x00;
		if (b == blocks - 1) {
			flag = p_loop ? 0x03 : 0x01;
		} else if (b == 0 && p_loop) {
			flag = 0x02;
		}
		out.push_back(flag);
		for (int i = 0; i < 28; i += 2) {
			const int sh = 12 - shift;
			int n0 = sh >= 0 ? (int(smp[i]) >> sh) : (int(smp[i]) << (-sh));
			int n1 = sh >= 0 ? (int(smp[i + 1]) >> sh) : (int(smp[i + 1]) << (-sh));
			n0 = CLAMP(n0, -8, 7);
			n1 = CLAMP(n1, -8, 7);
			out.push_back(uint8_t((n0 & 0x0f) | ((n1 & 0x0f) << 4)));
		}
	}
	r_out.bytes = out;
	r_out.sample_rate = rate;
	return OK;
}

} // namespace ps1_cook
