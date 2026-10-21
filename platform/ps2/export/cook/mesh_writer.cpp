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

#include "core/math/math_funcs.h"
#include "core/templates/hash_map.h"

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

// The guest culls runs of consecutive triangles by one bounding sphere, and
// sends a run that reaches past the camera through its slower clipper. A
// node's triangles arrive in whatever order its mesh was built (a road
// strip, a whole lap long), so runs could cover hundreds of meters. Each
// node's triangles are put in Z order of 12 m ground cells instead; nodes
// keep their order and stay contiguous, and triangles are unindexed, so
// nothing else depends on the order.
static uint32_t morton_cell(const Vector3 &p_c) {
	const float cell = 12.0f;
	const uint32_t x = uint32_t(CLAMP(int(Math::floor(p_c.x / cell)) + 32768, 0, 65535));
	const uint32_t z = uint32_t(CLAMP(int(Math::floor(p_c.z / cell)) + 32768, 0, 65535));
	uint32_t m = 0;
	for (int b = 0; b < 16; b++) {
		m |= ((x >> b) & 1u) << (2 * b);
		m |= ((z >> b) & 1u) << (2 * b + 1);
	}
	return m;
}

struct TriOrder {
	uint32_t node_rank = 0;
	uint32_t cell = 0;
	uint16_t tex = 0;
	int index = 0;
	bool operator<(const TriOrder &p_o) const {
		if (node_rank != p_o.node_rank) {
			return node_rank < p_o.node_rank;
		}
		if (cell != p_o.cell) {
			return cell < p_o.cell;
		}
		if (tex != p_o.tex) {
			return tex < p_o.tex;
		}
		return index < p_o.index;
	}
};

Error write_mesh_ee(const Vector<CookTri> &p_src_tris, int p_max_tris, MeshBlob &r_out) {
	const int src_n = p_src_tris.size();
	const int out_n = MIN(src_n, MAX(0, p_max_tris));
	HashMap<uint16_t, uint32_t> rank;
	Vector<TriOrder> order;
	order.resize(out_n);
	for (int i = 0; i < out_n; i++) {
		const CookTri &t = p_src_tris[i];
		if (!rank.has(t.node_id)) {
			rank.insert(t.node_id, rank.size());
		}
		TriOrder &o = order.write[i];
		o.node_rank = rank[t.node_id];
		o.cell = morton_cell((t.v[0] + t.v[1] + t.v[2]) / 3.0f);
		o.tex = t.tex_slot;
		o.index = i;
	}
	order.sort();
	Vector<CookTri> sorted;
	sorted.resize(out_n);
	for (int i = 0; i < out_n; i++) {
		sorted.write[i] = p_src_tris[order[i].index];
	}
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
		const CookTri &t = sorted[i];
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
