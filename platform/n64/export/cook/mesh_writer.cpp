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

namespace n64_cook {

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

Error write_mesh_blob(const Vector<Vector3> &p_verts, Vector<uint8_t> &r_out, int &r_tris) {
	if (p_verts.size() % 3 != 0) {
		return ERR_INVALID_DATA;
	}
	const int tris = p_verts.size() / 3;
	if (tris > kMaxTris) {
		return ERR_OUT_OF_MEMORY;
	}
	if (tris < 1) {
		return ERR_INVALID_DATA;
	}
	r_out.clear();
	r_out.push_back('M');
	r_out.push_back('E');
	r_out.push_back('S');
	r_out.push_back('H');
	append_u16(r_out, uint16_t(kCookABI));
	append_u16(r_out, 0);
	append_u32(r_out, uint32_t(tris));
	for (int i = 0; i < p_verts.size(); i++) {
		append_f32(r_out, p_verts[i].x);
		append_f32(r_out, p_verts[i].y);
		append_f32(r_out, p_verts[i].z);
	}
	r_tris = tris;
	return OK;
}

} // namespace n64_cook
