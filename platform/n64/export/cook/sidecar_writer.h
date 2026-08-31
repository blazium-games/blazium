/**************************************************************************/
/*  sidecar_writer.h                                                      */
/**************************************************************************/

#pragma once

#include "core/error/error_list.h"
#include "core/math/vector3.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/typedefs.h"

namespace n64_cook {

struct CamRow {
	int node_id = 0;
	uint16_t flags = 0;
	Vector3 pos;
	Vector3 euler;
	float fov = 55.0f;
};

struct HitRow {
	int node_id = 0;
	uint8_t kit = 0;
	Vector3 mn;
	Vector3 mx;
	uint8_t flags = 0;
	uint8_t kind = 0;
	uint8_t layer = 0;
	uint16_t frame_lo = 0;
	uint16_t frame_hi = 0xffff;
};

struct AnimKey {
	float t = 0;
	Vector3 pos;
	Vector3 rot;
};

struct AnimRow {
	String name;
	float length = 0;
	int node_id = 0;
	Vector<AnimKey> keys;
};

struct SprtRow {
	int node_id = 0;
	int16_t x = 0;
	int16_t y = 0;
	int16_t w = 16;
	int16_t h = 16;
	int16_t z = 0;
	uint8_t flip = 0;
	uint8_t frame = 0;
};

struct TileRow {
	int16_t tx = 0;
	int16_t ty = 0;
	uint8_t solid = 1;
};

struct PartRow {
	int node_id = 0;
	uint8_t mode = 0;
	float rate = 8.0f;
	float life = 1.0f;
	float speed = 2.0f;
	uint32_t color = 0xffffffff;
	float u0 = 0;
	float v0 = 0;
	float u1 = 1;
	float v1 = 1;
};

struct PathRow {
	int node_id = 0;
	Vector<Vector3> pts;
};

struct WayRow {
	int node_id = 0;
	Vector3 pos;
};

struct NavEdge {
	uint16_t a = 0;
	uint16_t b = 0;
};

struct NavmVert {
	Vector3 pos;
};

struct NavmTri {
	uint16_t a = 0;
	uint16_t b = 0;
	uint16_t c = 0;
};

struct VehlRow {
	int node_id = 0;
	uint8_t mode = 0;
	Vector3 pos;
	float yaw = 0;
	float speed = 8.0f;
	float turn = 2.0f;
};

Error write_camn_blob(const Vector<CamRow> &p_cams, int p_default_id, Vector<uint8_t> &r_out);
Error write_hitn_blob(const Vector<HitRow> &p_hits, Vector<uint8_t> &r_out);
Error write_anim_blob(const Vector<AnimRow> &p_anims, Vector<uint8_t> &r_out);
Error write_sprn_blob(const Vector<SprtRow> &p_sprts, Vector<uint8_t> &r_out);
Error write_tiln_blob(const Vector<TileRow> &p_tiles, Vector<uint8_t> &r_out);
Error write_prtn_blob(const Vector<PartRow> &p_parts, Vector<uint8_t> &r_out);
Error write_pthn_blob(const Vector<PathRow> &p_paths, Vector<uint8_t> &r_out);
Error write_wayn_blob(const Vector<WayRow> &p_ways, Vector<uint8_t> &r_out);
Error write_navn_blob(const Vector<NavEdge> &p_edges, Vector<uint8_t> &r_out);
Error write_navm_blob(const Vector<NavmVert> &p_verts, const Vector<NavmTri> &p_tris, Vector<uint8_t> &r_out);
Error write_vehn_blob(const Vector<VehlRow> &p_vehls, Vector<uint8_t> &r_out);

} // namespace n64_cook
