/**************************************************************************/
/*  audio_mark.cpp                                                        */
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

#include "stego/audio_mark.h"

#include "stego/fft_radix2.h"

#include "core/math/math_funcs.h"

static constexpr int FRAME = 1024;

static int bin_for_hz(float hz, int mix_rate) {
	return CLAMP((int)Math::round(hz * FRAME / (float)MAX(1, mix_rate)), 1, FRAME / 2 - 2);
}

static uint8_t crc8(const PackedByteArray &p_bytes) {
	uint8_t crc = 0;
	for (int i = 0; i < p_bytes.size(); i++) {
		crc ^= p_bytes[i];
		for (int b = 0; b < 8; b++) {
			crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
		}
	}
	return crc;
}

static void set_bin_mag(Vector<ObfuscationComplex> &p_spec, int p_bin, float p_mag) {
	float re = p_spec[p_bin].re;
	float im = p_spec[p_bin].im;
	float mag = Math::sqrt(re * re + im * im);
	if (mag < 1e-8f) {
		p_spec.write[p_bin].re = p_mag;
		p_spec.write[p_bin].im = 0;
	} else {
		float s = p_mag / mag;
		p_spec.write[p_bin].re = re * s;
		p_spec.write[p_bin].im = im * s;
	}
	int mir = FRAME - p_bin;
	if (mir > 0 && mir < FRAME && mir != p_bin) {
		p_spec.write[mir].re = p_spec[p_bin].re;
		p_spec.write[mir].im = -p_spec[p_bin].im;
	}
}

static void force_pair(Vector<ObfuscationComplex> &p_spec, int p_a, int p_b, int p_bit) {
	float ea = p_spec[p_a].re * p_spec[p_a].re + p_spec[p_a].im * p_spec[p_a].im;
	float eb = p_spec[p_b].re * p_spec[p_b].re + p_spec[p_b].im * p_spec[p_b].im;
	float mag_a = Math::sqrt(ea);
	float mag_b = Math::sqrt(eb);
	float mean = 0.5f * (mag_a + mag_b);
	if (mean < 1e-4f) {
		mean = 1e-3f;
	}
	float delta = MAX(0.35f * mean, 2e-3f);
	if (p_bit == 0) {
		set_bin_mag(p_spec, p_a, mean + delta);
		set_bin_mag(p_spec, p_b, MAX(mean - delta, 0.0f));
	} else {
		set_bin_mag(p_spec, p_a, MAX(mean - delta, 0.0f));
		set_bin_mag(p_spec, p_b, mean + delta);
	}
}

static int bit_at(const PackedByteArray &p_payload, uint8_t p_crc, int p_bit, int p_payload_bits) {
	if (p_bit < p_payload_bits) {
		return (p_payload[p_bit / 8] >> (7 - (p_bit % 8))) & 1;
	}
	int cb = p_bit - p_payload_bits;
	return (p_crc >> (7 - (cb % 8))) & 1;
}

PackedByteArray ObfuscationAudioMark::embed(const PackedByteArray &p_pcm_f32, int p_mix_rate, int p_channels, const PackedByteArray &p_payload) {
	if (p_pcm_f32.size() < (int)sizeof(float) * FRAME || p_channels < 1 || p_payload.is_empty()) {
		return p_pcm_f32;
	}
	PackedByteArray out = p_pcm_f32;
	int samples = out.size() / (int)sizeof(float);
	int frames = samples / (p_channels * FRAME);
	if (frames < 1) {
		return out;
	}
	int b0 = bin_for_hz(800.0f, p_mix_rate);
	int b1 = bin_for_hz(5000.0f, p_mix_rate);
	if (b1 <= b0 + 4) {
		b1 = b0 + 8;
	}
	int pairs = MAX(1, (b1 - b0) / 2);
	int payload_bits = p_payload.size() * 8;
	int bits = payload_bits + 8;
	uint8_t crc = crc8(p_payload);
	float *pcm = (float *)out.ptrw();
	for (int f = 0; f < frames; f++) {
		Vector<ObfuscationComplex> spec;
		spec.resize(FRAME);
		int base = f * FRAME * p_channels;
		for (int i = 0; i < FRAME; i++) {
			spec.write[i].re = pcm[base + i * p_channels];
			spec.write[i].im = 0;
		}
		ObfuscationFFT::forward(spec);
		int group = (pairs > 0) ? (f % ((bits + pairs - 1) / pairs)) : 0;
		for (int local = 0; local < pairs; local++) {
			int bit = group * pairs + local;
			if (bit >= bits) {
				break;
			}
			int a = b0 + local * 2;
			int b = a + 1;
			if (b >= FRAME / 2 || b > b1) {
				continue;
			}
			force_pair(spec, a, b, bit_at(p_payload, crc, bit, payload_bits));
		}
		spec.write[0].im = 0;
		spec.write[FRAME / 2].im = 0;
		ObfuscationFFT::inverse(spec);
		for (int i = 0; i < FRAME; i++) {
			pcm[base + i * p_channels] = spec[i].re;
		}
	}
	return out;
}

PackedByteArray ObfuscationAudioMark::extract(const PackedByteArray &p_pcm_f32, int p_mix_rate, int p_channels, int p_payload_bytes) {
	PackedByteArray out;
	if (p_pcm_f32.size() < (int)sizeof(float) * FRAME || p_payload_bytes <= 0) {
		return out;
	}
	int samples = p_pcm_f32.size() / (int)sizeof(float);
	int frames = samples / (MAX(1, p_channels) * FRAME);
	if (frames < 1) {
		return out;
	}
	int payload_bits = p_payload_bytes * 8;
	int bits = payload_bits + 8;
	Vector<int> votes;
	votes.resize(bits);
	for (int i = 0; i < bits; i++) {
		votes.write[i] = 0;
	}
	int b0 = bin_for_hz(800.0f, p_mix_rate);
	int b1 = bin_for_hz(5000.0f, p_mix_rate);
	if (b1 <= b0 + 4) {
		b1 = b0 + 8;
	}
	int pairs = MAX(1, (b1 - b0) / 2);
	const float *pcm = (const float *)p_pcm_f32.ptr();
	for (int f = 0; f < frames; f++) {
		Vector<ObfuscationComplex> spec;
		spec.resize(FRAME);
		int base = f * FRAME * p_channels;
		for (int i = 0; i < FRAME; i++) {
			spec.write[i].re = pcm[base + i * p_channels];
			spec.write[i].im = 0;
		}
		ObfuscationFFT::forward(spec);
		int group = (pairs > 0) ? (f % ((bits + pairs - 1) / pairs)) : 0;
		for (int local = 0; local < pairs; local++) {
			int bit = group * pairs + local;
			if (bit >= bits) {
				break;
			}
			int a = b0 + local * 2;
			int b = a + 1;
			if (b >= FRAME / 2 || b > b1) {
				continue;
			}
			float ea = spec[a].re * spec[a].re + spec[a].im * spec[a].im;
			float eb = spec[b].re * spec[b].re + spec[b].im * spec[b].im;
			votes.write[bit] += (eb > ea) ? 1 : -1;
		}
	}
	out.resize(p_payload_bytes);
	for (int i = 0; i < p_payload_bytes; i++) {
		uint8_t v = 0;
		for (int b = 0; b < 8; b++) {
			v = (uint8_t)((v << 1) | (votes[i * 8 + b] >= 0 ? 1 : 0));
		}
		out.write[i] = v;
	}
	return out;
}
