/**************************************************************************/
/*  xa_writer.cpp                                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* SPDX-License-Identifier: MIT                                           */
/**************************************************************************/

#include "xa_writer.h"

#include "core/typedefs.h"

namespace ps1_cook {

Error write_xa_pcm(const Vector<int16_t> &p_pcm, int p_sample_rate, XaBlob &r_out) {
	if (p_pcm.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	const int rate = p_sample_rate > 0 ? p_sample_rate : 37800;
	const int src_blocks = (p_pcm.size() + 27) / 28;
	const int blocks = MAX(1, src_blocks);
	Vector<uint8_t> out;
	out.push_back('F');
	out.push_back('X');
	out.push_back('A');
	out.push_back(' ');
	auto u32 = [&](uint32_t v) {
		out.push_back(uint8_t(v & 0xff));
		out.push_back(uint8_t((v >> 8) & 0xff));
		out.push_back(uint8_t((v >> 16) & 0xff));
		out.push_back(uint8_t((v >> 24) & 0xff));
	};
	u32(uint32_t(rate));
	u32(uint32_t(blocks * 16));
	for (int b = 0; b < blocks; b++) {
		int16_t smp[28];
		int max_abs = 1;
		for (int i = 0; i < 28; i++) {
			const int idx = b * 28 + i;
			smp[i] = idx < p_pcm.size() ? p_pcm[idx] : 0;
			const int a = smp[i] < 0 ? -int(smp[i]) : int(smp[i]);
			if (a > max_abs) {
				max_abs = a;
			}
		}
		int shift = 12;
		while (shift > 0 && (max_abs >> (12 - shift)) > 7) {
			shift--;
		}
		out.push_back(uint8_t(shift & 0x0f));
		out.push_back(b == blocks - 1 ? 0x01 : 0x00);
		for (int i = 0; i < 14; i++) {
			const int a = CLAMP(int(smp[i * 2]) >> shift, -8, 7) & 0x0f;
			const int c = CLAMP(int(smp[i * 2 + 1]) >> shift, -8, 7) & 0x0f;
			out.push_back(uint8_t(a | (c << 4)));
		}
	}
	r_out.bytes = out;
	r_out.sample_rate = rate;
	return OK;
}

} // namespace ps1_cook
