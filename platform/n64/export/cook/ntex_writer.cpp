/**************************************************************************/
/*  ntex_writer.cpp                                                       */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* SPDX-License-Identifier: MIT                                           */
/**************************************************************************/

#include "ntex_writer.h"

#include "cooker.h"

namespace n64_cook {

static void append_u16(Vector<uint8_t> &o, uint16_t v) {
	o.push_back(uint8_t(v & 0xff));
	o.push_back(uint8_t(v >> 8));
}

static uint16_t pack_rgba16(const Color &p_c) {
	const int r = CLAMP(int(p_c.r * 31.0f + 0.5f), 0, 31);
	const int g = CLAMP(int(p_c.g * 31.0f + 0.5f), 0, 31);
	const int b = CLAMP(int(p_c.b * 31.0f + 0.5f), 0, 31);
	const int a = p_c.a >= 0.5f ? 1 : 0;
	return uint16_t((r << 11) | (g << 6) | (b << 1) | a);
}

Error write_ntex_blob(const Ref<Image> &p_image, Vector<uint8_t> &r_out) {
	if (p_image.is_null() || p_image->is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	const int w = p_image->get_width();
	const int h = p_image->get_height();
	if (w < 1 || h < 1) {
		return ERR_INVALID_PARAMETER;
	}
	if (w * h * 2 > kTmemBytes) {
		return ERR_OUT_OF_MEMORY;
	}
	r_out.clear();
	r_out.push_back('N');
	r_out.push_back('T');
	r_out.push_back('E');
	r_out.push_back('X');
	append_u16(r_out, uint16_t(kCookABI));
	append_u16(r_out, uint16_t(w));
	append_u16(r_out, uint16_t(h));
	append_u16(r_out, uint16_t(kNtexFmtRgba16));
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			append_u16(r_out, pack_rgba16(p_image->get_pixel(x, y)));
		}
	}
	return OK;
}

} // namespace n64_cook
