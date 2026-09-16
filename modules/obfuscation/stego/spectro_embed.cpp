/**************************************************************************/
/*  spectro_embed.cpp                                                     */
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

#include "stego/spectro_embed.h"

#include "stego/fft_radix2.h"

#include "core/math/math_defs.h"
#include "core/math/math_funcs.h"

static constexpr int FRAME = 1024;

PackedByteArray ObfuscationSpectroEmbed::embed(const PackedByteArray &p_pcm_f32, int p_mix_rate, const Ref<Image> &p_image) {
	if (p_pcm_f32.is_empty() || p_image.is_null() || p_mix_rate < 44100) {
		return p_pcm_f32;
	}
	Ref<Image> img = p_image->duplicate();
	img->resize(32, 32, Image::INTERPOLATE_NEAREST);
	img->convert(Image::FORMAT_L8);
	PackedByteArray out = p_pcm_f32;
	int samples = out.size() / (int)sizeof(float);
	float *pcm = (float *)out.ptrw();
	int bin0 = CLAMP((int)(12000.0f * FRAME / (float)p_mix_rate), 8, FRAME / 2 - 40);
	int col_w = MAX(1, samples / 32);
	for (int x = 0; x < 32; x++) {
		for (int y = 0; y < 32; y++) {
			float lum = img->get_pixel(x, y).r;
			if (lum < 0.5f) {
				continue;
			}
			int start = x * col_w;
			float freq = (float)(bin0 + y);
			float amp = 0.004f;
			for (int i = 0; i < col_w && start + i < samples; i++) {
				pcm[start + i] += amp * Math::sin(2.0f * (float)Math::PI * freq * (float)i / (float)FRAME);
			}
		}
	}
	return out;
}

Ref<Image> ObfuscationSpectroEmbed::preview(const PackedByteArray &p_pcm_f32, int p_mix_rate, int p_height) {
	int h = MAX(64, p_height);
	int samples = p_pcm_f32.size() / (int)sizeof(float);
	int frames = MAX(1, samples / FRAME);
	Ref<Image> img = Image::create_empty(frames, h, false, Image::FORMAT_RGBA8);
	if (samples < FRAME) {
		return img;
	}
	const float *pcm = (const float *)p_pcm_f32.ptr();
	for (int f = 0; f < frames; f++) {
		Vector<ObfuscationComplex> spec;
		spec.resize(FRAME);
		for (int i = 0; i < FRAME; i++) {
			int idx = f * FRAME + i;
			spec.write[i].re = idx < samples ? pcm[idx] : 0;
			spec.write[i].im = 0;
		}
		ObfuscationFFT::forward(spec);
		for (int y = 0; y < h; y++) {
			int bin = CLAMP(y * (FRAME / 2) / h, 0, FRAME / 2 - 1);
			float mag = Math::sqrt(spec[bin].re * spec[bin].re + spec[bin].im * spec[bin].im);
			float v = CLAMP(Math::log(1.0f + mag * 40.0f) / 4.0f, 0.0f, 1.0f);
			img->set_pixel(f, h - 1 - y, Color(v, v * 0.8f, 0.2f, 1));
		}
	}
	return img;
}
