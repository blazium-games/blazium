/**************************************************************************/
/*  mesh_writer.cpp                                                       */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* SPDX-License-Identifier: MIT                                           */
/**************************************************************************/

#include "mesh_writer.h"

#include "cooker.h"

#include <cstring>

namespace ps2_cook {

static void append_u16(Vector<uint8_t> &o, uint16_t v) {
	o.push_back(uint8_t(v & 0xff));
	o.push_back(uint8_t(v >> 8));
}

static void append_u32(Vector<uint8_t> &o, uint32_t v) {
	append_u16(o, uint16_t(v & 0xffff));
	append_u16(o, uint16_t(v >> 16));
}

static void append_f32(Vector<uint8_t> &o, float v) {
	uint32_t bits = 0;
	memcpy(&bits, &v, sizeof(bits));
	append_u32(o, bits);
}

Error write_mesh_ee(const Vector<CookTri> &p_tris, int p_max_tris, MeshBlob &r_out) {
	const int src_n = p_tris.size();
	const int out_n = MIN(src_n, MAX(0, p_max_tris));
	Vector<uint8_t> out;
	out.push_back('M');
	out.push_back('E');
	out.push_back('S');
	out.push_back('H');
	append_u16(out, uint16_t(kCookABI));
	append_u16(out, 0);
	append_u32(out, uint32_t(out_n * 3));
	append_u32(out, uint32_t(out_n * 3));
	for (int i = 0; i < out_n; i++) {
		const CookTri &t = p_tris[i];
		for (int k = 0; k < 3; k++) {
			append_f32(out, t.v[k].x);
			append_f32(out, t.v[k].y);
			append_f32(out, t.v[k].z);
			out.push_back(t.rgb[k][0]);
			out.push_back(t.rgb[k][1]);
			out.push_back(t.rgb[k][2]);
			out.push_back(255);
			append_f32(out, t.uv[k].x);
			append_f32(out, t.uv[k].y);
			append_u16(out, t.node_id);
			append_u16(out, t.tex_slot);
		}
	}
	for (int i = 0; i < out_n * 3; i++) {
		append_u32(out, uint32_t(i));
	}
	r_out.bytes = out;
	r_out.tri_count = out_n;
	r_out.vert_count = out_n * 3;
	return OK;
}

} // namespace ps2_cook
