/**************************************************************************/
/*  wav_writer.cpp                                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* SPDX-License-Identifier: MIT                                           */
/**************************************************************************/

#include "wav_writer.h"

#include "core/math/math_funcs.h"

namespace n64_cook {

static const int kMaxWavBytes = 524288;

static void append_u16(Vector<uint8_t> &o, uint16_t v) {
	o.push_back(uint8_t(v & 0xff));
	o.push_back(uint8_t(v >> 8));
}

static void append_u32(Vector<uint8_t> &o, uint32_t v) {
	o.push_back(uint8_t(v & 0xff));
	o.push_back(uint8_t((v >> 8) & 0xff));
	o.push_back(uint8_t((v >> 16) & 0xff));
	o.push_back(uint8_t((v >> 24) & 0xff));
}

Error write_wav_pcm(const Vector<int16_t> &p_pcm, int p_rate, Vector<uint8_t> &r_out) {
	if (p_pcm.is_empty() || p_rate < 4000 || p_rate > 48000) {
		return ERR_INVALID_PARAMETER;
	}
	const int data_bytes = p_pcm.size() * 2;
	const int total = 44 + data_bytes;
	if (total > kMaxWavBytes) {
		return ERR_OUT_OF_MEMORY;
	}
	r_out.clear();
	r_out.push_back('R');
	r_out.push_back('I');
	r_out.push_back('F');
	r_out.push_back('F');
	append_u32(r_out, uint32_t(36 + data_bytes));
	r_out.push_back('W');
	r_out.push_back('A');
	r_out.push_back('V');
	r_out.push_back('E');
	r_out.push_back('f');
	r_out.push_back('m');
	r_out.push_back('t');
	r_out.push_back(' ');
	append_u32(r_out, 16);
	append_u16(r_out, 1);
	append_u16(r_out, 1);
	append_u32(r_out, uint32_t(p_rate));
	append_u32(r_out, uint32_t(p_rate * 2));
	append_u16(r_out, 2);
	append_u16(r_out, 16);
	r_out.push_back('d');
	r_out.push_back('a');
	r_out.push_back('t');
	r_out.push_back('a');
	append_u32(r_out, uint32_t(data_bytes));
	for (int i = 0; i < p_pcm.size(); i++) {
		append_u16(r_out, uint16_t(p_pcm[i]));
	}
	if (r_out.size() >= 2 && r_out[0] == 0x4D && r_out[1] == 0x5A) {
		return FAILED;
	}
	return OK;
}

Error write_wav_beep(int p_hz, int p_ms, int p_rate, Vector<uint8_t> &r_out) {
	const int hz = p_hz < 80 ? 80 : (p_hz > 4000 ? 4000 : p_hz);
	const int ms = p_ms < 20 ? 20 : (p_ms > 800 ? 800 : p_ms);
	const int rate = p_rate < 8000 ? 22050 : p_rate;
	const int n = rate * ms / 1000;
	Vector<int16_t> pcm;
	pcm.resize(n);
	for (int i = 0; i < n; i++) {
		const float t = float(i) / float(rate);
		const float env = (i < n / 10) ? float(i) / float(n / 10) : ((i > n * 8 / 10) ? float(n - i) / float(n / 5) : 1.0f);
		pcm.write[i] = int16_t(Math::sin(t * 6.28318530718f * float(hz)) * 8000.0f * env);
	}
	return write_wav_pcm(pcm, rate, r_out);
}

} // namespace n64_cook
