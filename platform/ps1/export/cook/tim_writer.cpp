/**************************************************************************/
/*  tim_writer.cpp                                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* SPDX-License-Identifier: MIT                                           */
/**************************************************************************/

#include "tim_writer.h"

namespace ps1_cook {

static uint16_t rgb555(const Color &c) {
	const int r = CLAMP(int(c.r * 31.0f + 0.5f), 0, 31);
	const int g = CLAMP(int(c.g * 31.0f + 0.5f), 0, 31);
	const int b = CLAMP(int(c.b * 31.0f + 0.5f), 0, 31);
	return uint16_t(r | (g << 5) | (b << 10));
}

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

Error write_tim4(const Ref<Image> &p_image, TimBlob &r_out) {
	ERR_FAIL_COND_V(p_image.is_null() || p_image->is_empty(), ERR_INVALID_PARAMETER);

	Ref<Image> img = p_image->duplicate();
	img->convert(Image::FORMAT_RGBA8);
	int w = img->get_width();
	int h = img->get_height();
	const int max_dim = 64;
	if (w > max_dim || h > max_dim) {
		img->resize(MIN(w, max_dim), MIN(h, max_dim), Image::INTERPOLATE_LANCZOS);
		w = img->get_width();
		h = img->get_height();
	}
	w = MAX(32, (w + 31) / 32 * 32);
	h = MAX(32, (h + 31) / 32 * 32);
	if (img->get_width() != w || img->get_height() != h) {
		img->resize(w, h, Image::INTERPOLATE_NEAREST);
	}

	uint16_t pal[16] = {};
	int pal_used = 0;
	Vector<uint8_t> idx;
	idx.resize(w * h);

	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			const uint16_t c = rgb555(img->get_pixel(x, y));
			int found = -1;
			for (int i = 0; i < pal_used; i++) {
				if (pal[i] == c) {
					found = i;
					break;
				}
			}
			if (found < 0) {
				if (pal_used < 16) {
					found = pal_used;
					pal[pal_used++] = c;
				} else {
					int best = 0;
					int best_d = 1 << 30;
					for (int i = 0; i < 16; i++) {
						const int dr = int(c & 31) - int(pal[i] & 31);
						const int dg = int((c >> 5) & 31) - int((pal[i] >> 5) & 31);
						const int db = int((c >> 10) & 31) - int((pal[i] >> 10) & 31);
						const int d = dr * dr + dg * dg + db * db;
						if (d < best_d) {
							best_d = d;
							best = i;
						}
					}
					found = best;
				}
			}
			idx.write[y * w + x] = uint8_t(found);
		}
	}

	const int width_units = w / 4;
	const uint32_t clut_size = 12 + 16 * 2;
	const uint32_t img_size = 12 + uint32_t(width_units * h * 2);

	Vector<uint8_t> out;
	append_u32(out, 0x00000010);
	append_u32(out, 0x00000008); // 4-bit + CLUT

	append_u32(out, clut_size);
	append_u16(out, 0); // clut x
	append_u16(out, 480); // clut y
	append_u16(out, 16);
	append_u16(out, 1);
	for (int i = 0; i < 16; i++) {
		append_u16(out, pal[i]);
	}

	append_u32(out, img_size);
	append_u16(out, 640); // image x (tpage)
	append_u16(out, 0);
	append_u16(out, uint16_t(width_units));
	append_u16(out, uint16_t(h));
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x += 2) {
			const uint8_t lo = idx[y * w + x] & 0x0f;
			const uint8_t hi = idx[y * w + x + 1] & 0x0f;
			out.push_back(uint8_t(lo | (hi << 4)));
		}
	}

	r_out.bytes = out;
	r_out.pixel_w = w;
	r_out.pixel_h = h;
	r_out.vram_bytes = int(clut_size + img_size);
	return OK;
}

} // namespace ps1_cook
