/**************************************************************************/
/*  image_mark.cpp                                                        */
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

#include "stego/image_mark.h"

#include "stego/haar_dwt.h"
#include "stego/qim.h"

#include "core/crypto/crypto_core.h"
#include "core/math/math_funcs.h"

static Vector<int> pn_indices(int p_count, int p_need, const PackedByteArray &p_key, int p_offset) {
	Vector<int> idx;
	if (p_count <= 0 || p_need <= 0) {
		return idx;
	}
	uint8_t hash[32];
	PackedByteArray seed = p_key;
	if (seed.is_empty()) {
		seed.resize(4);
	}
	CryptoCore::sha256(seed.ptr(), seed.size(), hash);
	uint32_t s = ((uint32_t)hash[0] << 24) | ((uint32_t)hash[1] << 16) | ((uint32_t)hash[2] << 8) | hash[3];
	s ^= (uint32_t)p_offset * 0x9E3779B9u;
	if (s == 0) {
		s = 1;
	}
	idx.resize(p_need);
	for (int i = 0; i < p_need; i++) {
		s = s * 1664525u + 1013904223u;
		idx.write[i] = p_offset + (int)(s % (uint32_t)MAX(1, p_count));
	}
	return idx;
}

bool ObfuscationImageMark::can_mark(const Ref<Image> &p_image, int p_min_size) {
	if (p_image.is_null() || p_image->is_empty()) {
		return false;
	}
	return p_image->get_width() >= p_min_size && p_image->get_height() >= p_min_size;
}

Ref<Image> ObfuscationImageMark::embed(const Ref<Image> &p_image, const PackedByteArray &p_payload, const PackedByteArray &p_key, bool p_gutter) {
	if (p_image.is_null()) {
		return p_image;
	}
	Ref<Image> img = p_image->duplicate();
	if (p_gutter) {
		int nw = img->get_width() + 16;
		int nh = img->get_height() + 16;
		Ref<Image> padded = Image::create_empty(nw, nh, false, Image::FORMAT_RGBA8);
		img->convert(Image::FORMAT_RGBA8);
		padded->blit_rect(img, Rect2i(0, 0, img->get_width(), img->get_height()), Point2i(8, 8));
		img = padded;
	}
	img->convert(Image::FORMAT_RGBA8);
	int w = img->get_width();
	int h = img->get_height();
	int dw = w & ~3;
	int dh = h & ~3;
	if (dw < 8 || dh < 8) {
		return img;
	}
	Vector<float> luma;
	luma.resize(dw * dh);
	for (int y = 0; y < dh; y++) {
		for (int x = 0; x < dw; x++) {
			Color c = img->get_pixel(x, y);
			luma.write[y * dw + x] = (0.299f * c.r + 0.587f * c.g + 0.114f * c.b) * 255.0f;
		}
	}
	ObfuscationHaarDWT::forward_2d(luma, dw, dh, 2);
	Vector<uint8_t> bits;
	int bit_count = p_payload.size() * 8;
	bits.resize(bit_count);
	for (int i = 0; i < p_payload.size(); i++) {
		for (int b = 0; b < 8; b++) {
			bits.write[i * 8 + b] = (uint8_t)((p_payload[i] >> (7 - b)) & 1);
		}
	}
	int band_w = dw / 4;
	int band_h = dh / 4;
	int band_off = band_h * dw + band_w;
	int band_count = MAX(1, band_w * band_h);
	Vector<int> indices = pn_indices(band_count, bit_count, p_key, band_off);
	ObfuscationQIM::embed(luma, bits, indices, 4.0f);
	ObfuscationHaarDWT::inverse_2d(luma, dw, dh, 2);
	for (int y = 0; y < dh; y++) {
		for (int x = 0; x < dw; x++) {
			Color c = img->get_pixel(x, y);
			float yv = luma[y * dw + x] / 255.0f;
			float old = 0.299f * c.r + 0.587f * c.g + 0.114f * c.b;
			float d = yv - old;
			c.r = CLAMP(c.r + d, 0.0f, 1.0f);
			c.g = CLAMP(c.g + d, 0.0f, 1.0f);
			c.b = CLAMP(c.b + d, 0.0f, 1.0f);
			img->set_pixel(x, y, c);
		}
	}
	if (p_gutter) {
		PackedByteArray gbits;
		gbits.resize(MIN(128, p_payload.size()));
		for (int i = 0; i < gbits.size(); i++) {
			gbits.write[i] = p_payload[i];
		}
		for (int i = 0; i < gbits.size() && i < img->get_width(); i++) {
			Color c = img->get_pixel(i, 0);
			int r = (int)Math::round(c.r * 255.0f);
			r = (r & ~1) | ((gbits[i] >> 7) & 1);
			c.r = r / 255.0f;
			img->set_pixel(i, 0, c);
		}
	}
	return img;
}

PackedByteArray ObfuscationImageMark::extract(const Ref<Image> &p_image, const PackedByteArray &p_key, int p_bit_count) {
	PackedByteArray empty;
	if (p_image.is_null() || p_bit_count <= 0) {
		return empty;
	}
	Ref<Image> img = p_image->duplicate();
	img->convert(Image::FORMAT_RGBA8);
	int w = img->get_width();
	int h = img->get_height();
	int dw = w & ~3;
	int dh = h & ~3;
	if (dw < 8 || dh < 8) {
		return empty;
	}
	Vector<float> luma;
	luma.resize(dw * dh);
	for (int y = 0; y < dh; y++) {
		for (int x = 0; x < dw; x++) {
			Color c = img->get_pixel(x, y);
			luma.write[y * dw + x] = (0.299f * c.r + 0.587f * c.g + 0.114f * c.b) * 255.0f;
		}
	}
	ObfuscationHaarDWT::forward_2d(luma, dw, dh, 2);
	int band_w = dw / 4;
	int band_h = dh / 4;
	int band_off = band_h * dw + band_w;
	int band_count = MAX(1, band_w * band_h);
	Vector<int> indices = pn_indices(band_count, p_bit_count, p_key, band_off);
	Vector<uint8_t> bits;
	ObfuscationQIM::extract(luma, bits, indices, 4.0f);
	int nbytes = p_bit_count / 8;
	PackedByteArray out;
	out.resize(nbytes);
	for (int i = 0; i < nbytes; i++) {
		uint8_t v = 0;
		for (int b = 0; b < 8; b++) {
			int bi = i * 8 + b;
			v = (uint8_t)((v << 1) | ((bi < bits.size()) ? (bits[bi] & 1) : 0));
		}
		out.write[i] = v;
	}
	return out;
}
