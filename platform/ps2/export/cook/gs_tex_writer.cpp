/**************************************************************************/
/*  gs_tex_writer.cpp                                                     */
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

#include "gs_tex_writer.h"

#include "cooker.h"

#include "core/math/math_funcs.h"

namespace ps2_cook {

static int next_pow2(int v) {
	int p = 1;
	while (p < v) {
		p <<= 1;
	}
	return CLAMP(p, 8, 256);
}

static uint16_t rgb555(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	uint16_t out = uint16_t((r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10));
	if (a > 127) {
		out |= 0x8000;
	}
	return out;
}

static void append_u16(Vector<uint8_t> &o, uint16_t v) {
	o.push_back(uint8_t(v & 0xff));
	o.push_back(uint8_t(v >> 8));
}

static void append_u32(Vector<uint8_t> &o, uint32_t v) {
	append_u16(o, uint16_t(v & 0xffff));
	append_u16(o, uint16_t(v >> 16));
}

static int page_align(int bytes) {
	const int page = 8192;
	return (bytes + page - 1) / page * page;
}

Error write_gs_tex(const Ref<Image> &p_image, GsTex &r_out) {
	Ref<Image> img = p_image;
	if (img.is_null() || img->is_empty()) {
		img = Image::create_empty(16, 16, false, Image::FORMAT_RGBA8);
		img->fill(Color(0.4f, 0.4f, 0.45f));
	}
	if (img->is_compressed()) {
		img = img->duplicate();
		img->decompress();
	}
	if (img->get_format() != Image::FORMAT_RGBA8) {
		img = img->duplicate();
		img->convert(Image::FORMAT_RGBA8);
	}
	const int w = next_pow2(img->get_width());
	const int h = next_pow2(img->get_height());
	if (w != img->get_width() || h != img->get_height()) {
		// Growing a tiny image (a 2x2 checker) must keep its texels sharp;
		// shrinking a large one averages.
		const bool grow = w >= img->get_width() && h >= img->get_height();
		img = img->duplicate();
		img->resize(w, h, grow ? Image::INTERPOLATE_NEAREST : Image::INTERPOLATE_BILINEAR);
	}

	Vector<Color> palette;
	Vector<uint8_t> index;
	index.resize(w * h);
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			const Color c = img->get_pixel(x, y);
			int found = -1;
			for (int p = 0; p < palette.size(); p++) {
				if (palette[p].is_equal_approx(c)) {
					found = p;
					break;
				}
			}
			if (found < 0) {
				if (palette.size() < 256) {
					found = palette.size();
					palette.push_back(c);
				} else {
					found = 0;
				}
			}
			index.write[y * w + x] = uint8_t(found);
		}
	}

	r_out.width = w;
	r_out.height = h;
	r_out.clut.clear();
	r_out.data.clear();
	if (palette.size() <= 16) {
		r_out.psm = GS_PSM_T4;
		for (int i = 0; i < 16; i++) {
			const Color c = i < palette.size() ? palette[i] : Color();
			r_out.clut.push_back(rgb555(uint8_t(c.r * 255.0f), uint8_t(c.g * 255.0f), uint8_t(c.b * 255.0f), uint8_t(c.a * 255.0f)));
		}
		const int packed = (w * h + 1) / 2;
		r_out.data.resize(packed);
		for (int i = 0; i < w * h; i += 2) {
			const uint8_t a = index[i] & 0xf;
			const uint8_t b = (i + 1 < w * h) ? (index[i + 1] & 0xf) : 0;
			r_out.data.write[i / 2] = uint8_t(a | (b << 4));
		}
	} else if (palette.size() <= 256) {
		r_out.psm = GS_PSM_T8;
		for (int i = 0; i < 256; i++) {
			const Color c = i < palette.size() ? palette[i] : Color();
			r_out.clut.push_back(rgb555(uint8_t(c.r * 255.0f), uint8_t(c.g * 255.0f), uint8_t(c.b * 255.0f), uint8_t(c.a * 255.0f)));
		}
		r_out.data = index;
	} else {
		r_out.psm = GS_PSM_CT16;
		r_out.data.resize(w * h * 2);
		for (int i = 0; i < w * h; i++) {
			const int x = i % w;
			const int y = i / w;
			const Color c = img->get_pixel(x, y);
			const uint16_t px = rgb555(uint8_t(c.r * 255.0f), uint8_t(c.g * 255.0f), uint8_t(c.b * 255.0f), uint8_t(c.a * 255.0f));
			r_out.data.write[i * 2] = uint8_t(px & 0xff);
			r_out.data.write[i * 2 + 1] = uint8_t(px >> 8);
		}
	}
	r_out.vram_bytes = page_align(r_out.data.size() + int(r_out.clut.size()) * 2);
	return OK;
}

Error write_gtex_pack(const Vector<GsTex> &p_texs, GtexBlob &r_out) {
	Vector<uint8_t> out;
	out.push_back('G');
	out.push_back('T');
	out.push_back('E');
	out.push_back('X');
	append_u16(out, uint16_t(kCookABI));
	append_u16(out, uint16_t(p_texs.size()));
	int vram = 0;
	for (int i = 0; i < p_texs.size(); i++) {
		const GsTex &t = p_texs[i];
		append_u16(out, uint16_t(t.width));
		append_u16(out, uint16_t(t.height));
		out.push_back(t.psm);
		out.push_back(0);
		append_u16(out, uint16_t(t.clut.size()));
		append_u32(out, uint32_t(t.data.size()));
		for (int b = 0; b < t.data.size(); b++) {
			out.push_back(t.data[b]);
		}
		for (int c = 0; c < t.clut.size(); c++) {
			append_u16(out, t.clut[c]);
		}
		vram += t.vram_bytes;
	}
	r_out.bytes = out;
	r_out.tex_count = p_texs.size();
	r_out.vram_bytes = vram;
	return OK;
}

} // namespace ps2_cook
