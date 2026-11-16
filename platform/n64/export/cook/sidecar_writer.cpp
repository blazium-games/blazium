/**************************************************************************/
/*  sidecar_writer.cpp                                                    */
/**************************************************************************/

#include "sidecar_writer.h"

#include "cooker.h"

#include <cstring>

namespace n64_cook {

static void append_u16(Vector<uint8_t> &o, uint16_t v) {
	o.push_back(uint8_t(v & 0xff));
	o.push_back(uint8_t(v >> 8));
}

static void append_i16(Vector<uint8_t> &o, int16_t v) {
	append_u16(o, uint16_t(v));
}

static void append_f32(Vector<uint8_t> &o, float v) {
	uint32_t bits = 0;
	memcpy(&bits, &v, sizeof(bits));
	o.push_back(uint8_t(bits & 0xff));
	o.push_back(uint8_t((bits >> 8) & 0xff));
	o.push_back(uint8_t((bits >> 16) & 0xff));
	o.push_back(uint8_t((bits >> 24) & 0xff));
}

static void magic4(Vector<uint8_t> &o, char a, char b, char c, char d) {
	o.push_back(uint8_t(a));
	o.push_back(uint8_t(b));
	o.push_back(uint8_t(c));
	o.push_back(uint8_t(d));
}

Error write_camn_blob(const Vector<CamRow> &p_cams, int p_default_id, Vector<uint8_t> &r_out) {
	if (p_cams.size() > kMaxCams) {
		return ERR_OUT_OF_MEMORY;
	}
	r_out.clear();
	magic4(r_out, 'C', 'A', 'M', 'N');
	append_u16(r_out, uint16_t(kCookABI));
	append_u16(r_out, uint16_t(p_cams.size()));
	append_u16(r_out, uint16_t(p_default_id < 0 ? 0 : p_default_id));
	append_u16(r_out, 0);
	for (int i = 0; i < p_cams.size(); i++) {
		const CamRow &c = p_cams[i];
		append_u16(r_out, uint16_t(int16_t(c.node_id)));
		append_u16(r_out, c.flags);
		append_f32(r_out, c.pos.x);
		append_f32(r_out, c.pos.y);
		append_f32(r_out, c.pos.z);
		append_f32(r_out, c.euler.x);
		append_f32(r_out, c.euler.y);
		append_f32(r_out, c.euler.z);
		append_f32(r_out, c.fov);
	}
	return OK;
}

Error write_hitn_blob(const Vector<HitRow> &p_hits, Vector<uint8_t> &r_out) {
	if (p_hits.size() > kMaxHits) {
		return ERR_OUT_OF_MEMORY;
	}
	r_out.clear();
	magic4(r_out, 'H', 'I', 'T', 'N');
	append_u16(r_out, uint16_t(kCookABI));
	append_u16(r_out, uint16_t(p_hits.size()));
	for (int i = 0; i < p_hits.size(); i++) {
		const HitRow &h = p_hits[i];
		append_u16(r_out, uint16_t(int16_t(h.node_id)));
		r_out.push_back(h.kit);
		r_out.push_back(0);
		append_f32(r_out, h.mn.x);
		append_f32(r_out, h.mn.y);
		append_f32(r_out, h.mn.z);
		append_f32(r_out, h.mx.x);
		append_f32(r_out, h.mx.y);
		append_f32(r_out, h.mx.z);
		r_out.push_back(h.flags);
		r_out.push_back(h.kind);
		r_out.push_back(h.layer);
		append_u16(r_out, h.frame_lo);
		append_u16(r_out, h.frame_hi);
	}
	return OK;
}

Error write_anim_blob(const Vector<AnimRow> &p_anims, Vector<uint8_t> &r_out) {
	if (p_anims.size() > kMaxAnims) {
		return ERR_OUT_OF_MEMORY;
	}
	r_out.clear();
	magic4(r_out, 'A', 'N', 'I', 'M');
	append_u16(r_out, uint16_t(kCookABI));
	append_u16(r_out, uint16_t(p_anims.size()));
	for (int i = 0; i < p_anims.size(); i++) {
		const AnimRow &a = p_anims[i];
		const CharString utf = a.name.utf8();
		for (int c = 0; c < 32; c++) {
			r_out.push_back(c < utf.length() ? uint8_t(utf[c]) : 0);
		}
		append_f32(r_out, a.length);
		append_u16(r_out, uint16_t(a.keys.size()));
		append_u16(r_out, uint16_t(int16_t(a.node_id)));
		for (int k = 0; k < a.keys.size(); k++) {
			append_f32(r_out, a.keys[k].t);
			append_f32(r_out, a.keys[k].pos.x);
			append_f32(r_out, a.keys[k].pos.y);
			append_f32(r_out, a.keys[k].pos.z);
			append_f32(r_out, a.keys[k].rot.x);
			append_f32(r_out, a.keys[k].rot.y);
			append_f32(r_out, a.keys[k].rot.z);
		}
	}
	return OK;
}

Error write_sprn_blob(const Vector<SprtRow> &p_sprts, Vector<uint8_t> &r_out) {
	if (p_sprts.size() > kMaxSprites) {
		return ERR_OUT_OF_MEMORY;
	}
	r_out.clear();
	magic4(r_out, 'S', 'P', 'R', 'N');
	append_u16(r_out, uint16_t(kCookABI));
	append_u16(r_out, uint16_t(p_sprts.size()));
	for (int i = 0; i < p_sprts.size(); i++) {
		const SprtRow &s = p_sprts[i];
		append_u16(r_out, uint16_t(int16_t(s.node_id)));
		append_u16(r_out, 0);
		append_i16(r_out, s.x);
		append_i16(r_out, s.y);
		append_i16(r_out, s.w);
		append_i16(r_out, s.h);
		r_out.push_back(s.flip);
		r_out.push_back(s.frame);
		append_i16(r_out, s.z);
	}
	return OK;
}

Error write_tiln_blob(const Vector<TileRow> &p_tiles, Vector<uint8_t> &r_out) {
	if (p_tiles.size() > kMaxTiles) {
		return ERR_OUT_OF_MEMORY;
	}
	r_out.clear();
	magic4(r_out, 'T', 'I', 'L', 'N');
	append_u16(r_out, uint16_t(kCookABI));
	append_u16(r_out, uint16_t(p_tiles.size()));
	for (int i = 0; i < p_tiles.size(); i++) {
		append_i16(r_out, p_tiles[i].tx);
		append_i16(r_out, p_tiles[i].ty);
		r_out.push_back(p_tiles[i].solid);
		r_out.push_back(0);
	}
	return OK;
}

Error write_prtn_blob(const Vector<PartRow> &p_parts, Vector<uint8_t> &r_out) {
	if (p_parts.size() > kMaxParts) {
		return ERR_OUT_OF_MEMORY;
	}
	r_out.clear();
	magic4(r_out, 'P', 'R', 'T', 'N');
	append_u16(r_out, uint16_t(kCookABI));
	append_u16(r_out, uint16_t(p_parts.size()));
	for (int i = 0; i < p_parts.size(); i++) {
		const PartRow &p = p_parts[i];
		append_u16(r_out, uint16_t(int16_t(p.node_id)));
		r_out.push_back(p.mode);
		r_out.push_back(0);
		append_f32(r_out, p.rate);
		append_f32(r_out, p.life);
		append_f32(r_out, p.speed);
		r_out.push_back(uint8_t(p.color & 0xff));
		r_out.push_back(uint8_t((p.color >> 8) & 0xff));
		r_out.push_back(uint8_t((p.color >> 16) & 0xff));
		r_out.push_back(uint8_t((p.color >> 24) & 0xff));
		append_f32(r_out, p.u0);
		append_f32(r_out, p.v0);
		append_f32(r_out, p.u1);
		append_f32(r_out, p.v1);
	}
	return OK;
}

Error write_pthn_blob(const Vector<PathRow> &p_paths, Vector<uint8_t> &r_out) {
	if (p_paths.size() > kMaxPaths) {
		return ERR_OUT_OF_MEMORY;
	}
	r_out.clear();
	magic4(r_out, 'P', 'T', 'H', 'N');
	append_u16(r_out, uint16_t(kCookABI));
	append_u16(r_out, uint16_t(p_paths.size()));
	for (int i = 0; i < p_paths.size(); i++) {
		const PathRow &p = p_paths[i];
		if (p.pts.size() > kMaxPathPoints) {
			return ERR_OUT_OF_MEMORY;
		}
		append_u16(r_out, uint16_t(int16_t(p.node_id)));
		append_u16(r_out, uint16_t(p.pts.size()));
		for (int k = 0; k < p.pts.size(); k++) {
			append_f32(r_out, p.pts[k].x);
			append_f32(r_out, p.pts[k].y);
			append_f32(r_out, p.pts[k].z);
		}
	}
	return OK;
}

Error write_wayn_blob(const Vector<WayRow> &p_ways, Vector<uint8_t> &r_out) {
	if (p_ways.size() > kMaxWaypoints) {
		return ERR_OUT_OF_MEMORY;
	}
	r_out.clear();
	magic4(r_out, 'W', 'A', 'Y', 'N');
	append_u16(r_out, uint16_t(kCookABI));
	append_u16(r_out, uint16_t(p_ways.size()));
	for (int i = 0; i < p_ways.size(); i++) {
		append_u16(r_out, uint16_t(int16_t(p_ways[i].node_id)));
		append_u16(r_out, 0);
		append_f32(r_out, p_ways[i].pos.x);
		append_f32(r_out, p_ways[i].pos.y);
		append_f32(r_out, p_ways[i].pos.z);
	}
	return OK;
}

Error write_navn_blob(const Vector<NavEdge> &p_edges, Vector<uint8_t> &r_out) {
	r_out.clear();
	magic4(r_out, 'N', 'A', 'V', 'N');
	append_u16(r_out, uint16_t(kCookABI));
	append_u16(r_out, uint16_t(p_edges.size()));
	for (int i = 0; i < p_edges.size(); i++) {
		append_u16(r_out, p_edges[i].a);
		append_u16(r_out, p_edges[i].b);
	}
	return OK;
}

Error write_navm_blob(const Vector<NavmVert> &p_verts, const Vector<NavmTri> &p_tris, Vector<uint8_t> &r_out) {
	if (p_verts.size() > kMaxNavmVerts || p_tris.size() > kMaxNavmTris) {
		return ERR_OUT_OF_MEMORY;
	}
	r_out.clear();
	magic4(r_out, 'N', 'A', 'V', 'M');
	append_u16(r_out, uint16_t(kCookABI));
	append_u16(r_out, uint16_t(p_verts.size()));
	append_u16(r_out, uint16_t(p_tris.size()));
	append_u16(r_out, 0);
	for (int i = 0; i < p_verts.size(); i++) {
		append_f32(r_out, p_verts[i].pos.x);
		append_f32(r_out, p_verts[i].pos.y);
		append_f32(r_out, p_verts[i].pos.z);
	}
	for (int i = 0; i < p_tris.size(); i++) {
		append_u16(r_out, p_tris[i].a);
		append_u16(r_out, p_tris[i].b);
		append_u16(r_out, p_tris[i].c);
	}
	return OK;
}

Error write_vehn_blob(const Vector<VehlRow> &p_vehls, Vector<uint8_t> &r_out) {
	if (p_vehls.size() > kMaxVehicles) {
		return ERR_OUT_OF_MEMORY;
	}
	r_out.clear();
	magic4(r_out, 'V', 'E', 'H', 'N');
	append_u16(r_out, uint16_t(kCookABI));
	append_u16(r_out, uint16_t(p_vehls.size()));
	for (int i = 0; i < p_vehls.size(); i++) {
		const VehlRow &v = p_vehls[i];
		append_u16(r_out, uint16_t(int16_t(v.node_id)));
		r_out.push_back(v.mode);
		r_out.push_back(0);
		append_f32(r_out, v.pos.x);
		append_f32(r_out, v.pos.y);
		append_f32(r_out, v.pos.z);
		append_f32(r_out, v.yaw);
		append_f32(r_out, v.speed);
		append_f32(r_out, v.turn);
	}
	return OK;
}

} // namespace n64_cook
