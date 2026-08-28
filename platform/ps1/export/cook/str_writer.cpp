/**************************************************************************/
/*  str_writer.cpp                                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* SPDX-License-Identifier: MIT                                           */
/**************************************************************************/

#include "str_writer.h"

#include "core/math/math_funcs.h"

namespace ps1_cook {

static void append_u16(Vector<uint8_t> &o, uint16_t v) {
	o.push_back(uint8_t(v & 0xff));
	o.push_back(uint8_t(v >> 8));
}

static void append_u32(Vector<uint8_t> &o, uint32_t v) {
	append_u16(o, uint16_t(v & 0xffff));
	append_u16(o, uint16_t(v >> 16));
}

static uint16_t rgb555(const Color &c) {
	const int r = CLAMP(int(c.r * 31.0f), 0, 31);
	const int g = CLAMP(int(c.g * 31.0f), 0, 31);
	const int b = CLAMP(int(c.b * 31.0f), 0, 31);
	return uint16_t(r | (g << 5) | (b << 10));
}

Error write_str_frames(const Vector<Ref<Image>> &p_frames, StrBlob &r_out) {
	if (p_frames.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	const int fw = 160;
	const int fh = 128;
	Vector<uint8_t> out;
	out.push_back('F');
	out.push_back('S');
	out.push_back('T');
	out.push_back('R');
	append_u16(out, uint16_t(p_frames.size()));
	append_u16(out, uint16_t(fw));
	append_u16(out, uint16_t(fh));
	append_u16(out, 0);
	for (int f = 0; f < p_frames.size(); f++) {
		Ref<Image> img = p_frames[f];
		if (img.is_null() || img->is_empty()) {
			return ERR_INVALID_PARAMETER;
		}
		img = img->duplicate();
		if (img->get_width() != fw || img->get_height() != fh) {
			img->resize(fw, fh, Image::INTERPOLATE_NEAREST);
		}
		if (img->get_format() != Image::FORMAT_RGBA8) {
			img->convert(Image::FORMAT_RGBA8);
		}
		append_u32(out, uint32_t(fw * fh * 2));
		for (int y = 0; y < fh; y++) {
			for (int x = 0; x < fw; x++) {
				append_u16(out, rgb555(img->get_pixel(x, y)));
			}
		}
	}
	r_out.bytes = out;
	r_out.frames = p_frames.size();
	r_out.width = fw;
	r_out.height = fh;
	return OK;
}

} // namespace ps1_cook
