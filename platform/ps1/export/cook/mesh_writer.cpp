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

namespace ps1_cook {

static const int kFit = 180;

static void append_u16(Vector<uint8_t> &o, uint16_t v) {
	o.push_back(uint8_t(v & 0xff));
	o.push_back(uint8_t(v >> 8));
}

static void append_i16(Vector<uint8_t> &o, int16_t v) {
	append_u16(o, uint16_t(v));
}

Error write_mesh_svector(const Vector<CookTri> &p_tris, int p_max_tris, MeshBlob &r_out) {
	ERR_FAIL_COND_V(p_tris.is_empty(), ERR_INVALID_PARAMETER);
	const int src_n = p_tris.size();
	const int max_tris = MAX(1, p_max_tris);
	const int stride = MAX(1, src_n / max_tris);
	int out_n = 0;
	for (int i = 0; i < src_n && out_n < max_tris; i += stride) {
		out_n++;
	}

	Vector3 mn(1e9f, 1e9f, 1e9f);
	Vector3 mx(-1e9f, -1e9f, -1e9f);
	for (int i = 0; i < src_n; i += stride) {
		if ((i / stride) >= out_n) {
			break;
		}
		for (int k = 0; k < 3; k++) {
			const Vector3 &p = p_tris[i].v[k];
			mn.x = MIN(mn.x, p.x);
			mn.y = MIN(mn.y, p.y);
			mn.z = MIN(mn.z, p.z);
			mx.x = MAX(mx.x, p.x);
			mx.y = MAX(mx.y, p.y);
			mx.z = MAX(mx.z, p.z);
		}
	}
	const Vector3 center = (mn + mx) * 0.5f;
	const Vector3 ext = mx - mn;
	const float biggest = MAX(0.001f, MAX(ext.x, MAX(ext.y, ext.z)));
	const float scale = float(kFit) / biggest;

	Vector<uint8_t> out;
	append_u16(out, uint16_t(out_n));
	append_u16(out, 1); // has UV
	int written = 0;
	for (int i = 0; i < src_n && written < out_n; i += stride) {
		const CookTri &t = p_tris[i];
		for (int k = 0; k < 3; k++) {
			const Vector3 p = (t.v[k] - center) * scale;
			append_i16(out, int16_t(CLAMP(int(p.x), -32767, 32767)));
			append_i16(out, int16_t(CLAMP(int(-p.y), -32767, 32767)));
			append_i16(out, int16_t(CLAMP(int(p.z), -32767, 32767)));
			append_i16(out, 0);
		}
		for (int k = 0; k < 3; k++) {
			const int u = CLAMP(int(t.uv[k].x * 64.0f), 0, 63);
			const int v = CLAMP(int(t.uv[k].y * 64.0f), 0, 63);
			out.push_back(uint8_t(u));
			out.push_back(uint8_t(v));
		}
		append_u16(out, 0);
		written++;
	}

	r_out.bytes = out;
	r_out.tri_count = written;
	r_out.center = center;
	r_out.scale = scale;
	return OK;
}

} // namespace ps1_cook
