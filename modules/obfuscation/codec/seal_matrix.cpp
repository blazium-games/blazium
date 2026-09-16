/**************************************************************************/
/*  seal_matrix.cpp                                                       */
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

#include "codec/seal_matrix.h"

#include "codec/bit_perm.h"
#include "codec/reed_solomon.h"

#include "core/math/math_funcs.h"

static void bytes_to_bits(const Vector<uint8_t> &p_bytes, Vector<uint8_t> &r_bits) {
	r_bits.resize(p_bytes.size() * 8);
	for (int i = 0; i < p_bytes.size(); i++) {
		for (int b = 0; b < 8; b++) {
			r_bits.write[i * 8 + b] = (uint8_t)((p_bytes[i] >> (7 - b)) & 1);
		}
	}
}

static void bits_to_bytes(const Vector<uint8_t> &p_bits, Vector<uint8_t> &r_bytes) {
	int nbytes = p_bits.size() / 8;
	r_bytes.resize(nbytes);
	for (int i = 0; i < nbytes; i++) {
		uint8_t v = 0;
		for (int b = 0; b < 8; b++) {
			v = (uint8_t)((v << 1) | (p_bits[i * 8 + b] & 1));
		}
		r_bytes.write[i] = v;
	}
}

static void draw_chevron(Ref<Image> p_image, int cx, int cy, bool p_dark) {
	Color c = p_dark ? Color(0.15, 0.16, 0.18, 1) : Color(0.85, 0.86, 0.82, 1);
	for (int y = -6; y <= 6; y++) {
		for (int x = -6; x <= 6; x++) {
			int ax = Math::abs(x);
			int ay = Math::abs(y);
			if ((ay <= 2 && ax <= 5) || (ax <= 2 && y >= 0 && y <= 5)) {
				int px = cx + x;
				int py = cy + y;
				if (px >= 0 && py >= 0 && px < ObfuscationSealMatrix::SIZE && py < ObfuscationSealMatrix::SIZE) {
					p_image->set_pixel(px, py, c);
				}
			}
		}
	}
}

Ref<Image> ObfuscationSealMatrix::encode(const Vector<uint8_t> &p_payload) {
	Vector<uint8_t> encoded;
	ObfuscationReedSolomon::encode(p_payload, encoded);

	Ref<Image> image = Image::create_empty(SIZE, SIZE, false, Image::FORMAT_RGBA8);
	for (int y = 0; y < SIZE; y++) {
		for (int x = 0; x < SIZE; x++) {
			float n = ((x * 13 + y * 7) & 15) / 64.0f;
			image->set_pixel(x, y, Color(0.22f + n, 0.23f + n * 0.5f, 0.25f, 1));
		}
	}
	draw_chevron(image, 20, 20, true);
	draw_chevron(image, SIZE - 21, 20, false);
	draw_chevron(image, 20, SIZE - 21, false);
	draw_chevron(image, SIZE - 21, SIZE - 21, true);

	const int bpp = 3;
	const int tile_bits = TILE * TILE * bpp;
	Vector<uint8_t> tile_bytes = encoded;
	while (tile_bytes.size() * 8 < tile_bits) {
		tile_bytes.push_back(0);
	}
	Vector<uint8_t> bits;
	bytes_to_bits(tile_bytes, bits);
	if (bits.size() > tile_bits) {
		bits.resize(tile_bits);
	}
	ObfuscationBitPerm::scramble(bits);

	for (int ty = 0; ty < SIZE / TILE; ty++) {
		for (int tx = 0; tx < SIZE / TILE; tx++) {
			for (int pix = 0; pix < TILE * TILE; pix++) {
				int lx = pix % TILE;
				int ly = pix / TILE;
				int x = tx * TILE + lx;
				int y = ty * TILE + ly;
				Color c = image->get_pixel(x, y);
				int r = CLAMP((int)Math::round(c.r * 255.0f), 0, 255);
				int g = CLAMP((int)Math::round(c.g * 255.0f), 0, 255);
				int b = CLAMP((int)Math::round(c.b * 255.0f), 0, 255);
				int bi = pix * bpp;
				r = (r & ~1) | ((bi < bits.size()) ? (bits[bi] & 1) : 0);
				g = (g & ~1) | ((bi + 1 < bits.size()) ? (bits[bi + 1] & 1) : 0);
				b = (b & ~1) | ((bi + 2 < bits.size()) ? (bits[bi + 2] & 1) : 0);
				c.r = r / 255.0f;
				c.g = g / 255.0f;
				c.b = b / 255.0f;
				image->set_pixel(x, y, c);
			}
		}
	}
	return image;
}

bool ObfuscationSealMatrix::decode_tile(const Ref<Image> &p_image, int p_tile_x, int p_tile_y, Vector<uint8_t> &r_payload) {
	if (p_image.is_null() || p_image->get_width() < SIZE || p_image->get_height() < SIZE) {
		if (p_image.is_null() || p_image->get_width() < TILE || p_image->get_height() < TILE) {
			return false;
		}
	}
	Ref<Image> img = p_image->duplicate();
	if (img->get_format() != Image::FORMAT_RGBA8) {
		img->convert(Image::FORMAT_RGBA8);
	}
	const int bpp = 3;
	const int tile_bits = TILE * TILE * bpp;
	Vector<uint8_t> bits;
	bits.resize(tile_bits);
	int ox = p_tile_x * TILE;
	int oy = p_tile_y * TILE;
	if (img->get_width() == TILE && img->get_height() == TILE) {
		ox = 0;
		oy = 0;
	}
	for (int pix = 0; pix < TILE * TILE; pix++) {
		int lx = pix % TILE;
		int ly = pix / TILE;
		int x = ox + lx;
		int y = oy + ly;
		int r = 0;
		int g = 0;
		int b = 0;
		if (x < img->get_width() && y < img->get_height()) {
			Color c = img->get_pixel(x, y);
			r = CLAMP((int)Math::round(c.r * 255.0f), 0, 255);
			g = CLAMP((int)Math::round(c.g * 255.0f), 0, 255);
			b = CLAMP((int)Math::round(c.b * 255.0f), 0, 255);
		}
		int bi = pix * bpp;
		bits.write[bi] = (uint8_t)(r & 1);
		bits.write[bi + 1] = (uint8_t)(g & 1);
		bits.write[bi + 2] = (uint8_t)(b & 1);
	}
	ObfuscationBitPerm::unscramble(bits);
	Vector<uint8_t> bytes;
	bits_to_bytes(bits, bytes);
	if (bytes.size() < ObfuscationReedSolomon::N) {
		return false;
	}
	if (bytes.size() > ObfuscationReedSolomon::N * 4) {
		bytes.resize(ObfuscationReedSolomon::N * 4);
	}
	int usable = (bytes.size() / ObfuscationReedSolomon::N) * ObfuscationReedSolomon::N;
	bytes.resize(usable);
	return ObfuscationReedSolomon::decode(bytes, r_payload);
}

bool ObfuscationSealMatrix::decode(const Ref<Image> &p_image, Vector<uint8_t> &r_payload) {
	if (p_image.is_null()) {
		return false;
	}
	Ref<Image> img = p_image->duplicate();
	if (img->get_format() != Image::FORMAT_RGBA8) {
		img->convert(Image::FORMAT_RGBA8);
	}
	int tiles_x = MAX(1, img->get_width() / TILE);
	int tiles_y = MAX(1, img->get_height() / TILE);
	if (img->get_width() == TILE && img->get_height() == TILE) {
		return decode_tile(img, 0, 0, r_payload);
	}
	for (int ty = 0; ty < tiles_y; ty++) {
		for (int tx = 0; tx < tiles_x; tx++) {
			Vector<uint8_t> payload;
			if (decode_tile(img, tx, ty, payload) && payload.size() >= 4) {
				if (payload[0] == MAGIC[0] && payload[1] == MAGIC[1] && payload[2] == MAGIC[2] && payload[3] == MAGIC[3]) {
					r_payload = payload;
					return true;
				}
			}
		}
	}
	return decode_tile(img, 0, 0, r_payload);
}
