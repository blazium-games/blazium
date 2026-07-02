/**************************************************************************/
/*  geometry_generator.cpp                                                */
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

#include "geometry_generator.h"

#include "modules/trenchbroom/fgd/blazium_fgd_solid_class.h"
#include "modules/trenchbroom/trenchbroom_map.h"
#include "modules/trenchbroom/trenchbroom_map_settings.h"
#include "modules/trenchbroom/util/trenchbroom_util.h"

#include "core/math/aabb.h"
#include "core/math/convex_hull.h"
#include "core/math/geometry_3d.h"
#include "core/object/callable_method_pointer.h"
#include "core/object/class_db.h"
#include "core/object/worker_thread_pool.h"
#include "core/templates/hash_set.h"
#include "scene/resources/3d/concave_polygon_shape_3d.h"
#include "scene/resources/3d/convex_polygon_shape_3d.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"

static Vector3i _vertex_snap_key(const Vector3 &p_vertex, real_t p_epsilon) {
	const real_t inv = p_epsilon > 0.0 ? (1.0 / p_epsilon) : 1.0;
	return Vector3i(
			Math::round(p_vertex.x * inv),
			Math::round(p_vertex.y * inv),
			Math::round(p_vertex.z * inv));
}

static HashSet<Vector3i> _face_vertex_keys(const PackedVector3Array &p_vertices, real_t p_epsilon) {
	HashSet<Vector3i> keys;
	for (int i = 0; i < p_vertices.size(); i++) {
		keys.insert(_vertex_snap_key(p_vertices[i], p_epsilon));
	}
	return keys;
}

static bool _face_vertices_subset(const FaceData *p_inner, const FaceData *p_outer, real_t p_epsilon) {
	const HashSet<Vector3i> outer_keys = _face_vertex_keys(p_outer->vertices, p_epsilon);
	for (int i = 0; i < p_inner->vertices.size(); i++) {
		if (!outer_keys.has(_vertex_snap_key(p_inner->vertices[i], p_epsilon))) {
			return false;
		}
	}
	return true;
}

static real_t _face_area(const FaceData *p_face) {
	if (p_face->indices.size() < 3) {
		return 0.0;
	}
	real_t area = 0.0;
	const int tri_count = p_face->indices.size() / 3;
	for (int tri_i = 0; tri_i < tri_count; tri_i++) {
		const Vector3 a = p_face->vertices[p_face->indices[tri_i * 3]];
		const Vector3 b = p_face->vertices[p_face->indices[tri_i * 3 + 1]];
		const Vector3 c = p_face->vertices[p_face->indices[tri_i * 3 + 2]];
		area += (b - a).cross(c - a).length() * 0.5;
	}
	return area;
}

static AABB _face_aabb(const FaceData *p_face) {
	AABB bounds;
	for (int i = 0; i < p_face->vertices.size(); i++) {
		bounds.expand_to(p_face->vertices[i]);
	}
	return bounds;
}

static bool _aabb_contains(const AABB &p_outer, const AABB &p_inner, real_t p_epsilon) {
	return p_outer.position.x - p_epsilon <= p_inner.position.x &&
			p_outer.position.y - p_epsilon <= p_inner.position.y &&
			p_outer.position.z - p_epsilon <= p_inner.position.z &&
			p_outer.position.x + p_outer.size.x + p_epsilon >= p_inner.position.x + p_inner.size.x &&
			p_outer.position.y + p_outer.size.y + p_epsilon >= p_inner.position.y + p_inner.size.y &&
			p_outer.position.z + p_outer.size.z + p_epsilon >= p_inner.position.z + p_inner.size.z;
}

static void _face_plane_basis(const FaceData *p_face, Vector3 &r_origin, Vector3 &r_u, Vector3 &r_v) {
	r_origin = p_face->get_centroid();
	r_u = p_face->get_basis();
	if (r_u.length_squared() < 0.0001) {
		Vector3 up = Vector3(0, 1, 0);
		if (Math::abs(p_face->plane.normal.dot(up)) > 0.9) {
			up = Vector3(1, 0, 0);
		}
		r_u = p_face->plane.normal.cross(up).normalized();
	}
	r_v = p_face->plane.normal.cross(r_u).normalized();
}

static Vector2 _project_to_face_plane(const Vector3 &p_vertex, const Vector3 &p_origin, const Vector3 &p_u, const Vector3 &p_v) {
	const Vector3 rel = p_vertex - p_origin;
	return Vector2(p_u.dot(rel), p_v.dot(rel));
}

static bool _point_in_polygon_2d(const Vector2 &p_point, const LocalVector<Vector2> &p_polygon) {
	if (p_polygon.size() < 3) {
		return false;
	}
	bool inside = false;
	const int count = p_polygon.size();
	for (int i = 0, j = count - 1; i < count; j = i++) {
		const Vector2 a = p_polygon[i];
		const Vector2 b = p_polygon[j];
		if ((a.y > p_point.y) != (b.y > p_point.y)) {
			const real_t x_intersect = (b.x - a.x) * (p_point.y - a.y) / (b.y - a.y) + a.x;
			if (p_point.x < x_intersect) {
				inside = !inside;
			}
		}
	}
	return inside;
}

static bool _face_vertices_inside_face_2d(const FaceData *p_inner, const FaceData *p_outer) {
	Vector3 origin;
	Vector3 u_axis;
	Vector3 v_axis;
	_face_plane_basis(p_outer, origin, u_axis, v_axis);

	LocalVector<Vector2> outer_poly;
	for (int i = 0; i < p_outer->vertices.size(); i++) {
		outer_poly.push_back(_project_to_face_plane(p_outer->vertices[i], origin, u_axis, v_axis));
	}

	for (int i = 0; i < p_inner->vertices.size(); i++) {
		const Vector2 projected = _project_to_face_plane(p_inner->vertices[i], origin, u_axis, v_axis);
		if (!_point_in_polygon_2d(projected, outer_poly)) {
			return false;
		}
	}
	return true;
}

void TrenchbroomGeometryGenerator::_bind_methods() {
}

TrenchbroomGeometryGenerator::TrenchbroomGeometryGenerator(const TrenchbroomMapSettings *p_settings, real_t p_hyperplane_size) {
	map_settings = p_settings;
	hyperplane_size = p_hyperplane_size;
}

bool TrenchbroomGeometryGenerator::is_skip(const FaceData &p_face) const {
	return TrenchbroomUtil::is_skip(p_face.texture, map_settings);
}

bool TrenchbroomGeometryGenerator::is_clip(const FaceData &p_face) const {
	return TrenchbroomUtil::is_clip(p_face.texture, map_settings);
}

bool TrenchbroomGeometryGenerator::is_origin(const FaceData &p_face) const {
	return TrenchbroomUtil::is_origin(p_face.texture, map_settings);
}

PackedVector3Array TrenchbroomGeometryGenerator::generate_base_winding(const Plane &p_plane) const {
	Vector3 up = Vector3(0, 1, 0);
	if (Math::abs(p_plane.normal.dot(up)) > 0.9) {
		up = Vector3(1, 0, 0);
	}

	const Vector3 right = p_plane.normal.cross(up).normalized();
	const Vector3 forward = right.cross(p_plane.normal).normalized();
	const Vector3 centroid = p_plane.get_center();

	const real_t h = hyperplane_size;
	PackedVector3Array winding;
	winding.push_back(centroid + (right * h) + (forward * h));
	winding.push_back(centroid + (right * -h) + (forward * h));
	winding.push_back(centroid + (right * -h) + (forward * -h));
	winding.push_back(centroid + (right * h) + (forward * -h));
	return winding;
}

PackedVector3Array TrenchbroomGeometryGenerator::generate_face_vertices(const BrushData &p_brush, int p_face_index, real_t p_vertex_merge_distance) const {
	const Plane plane = p_brush.faces[p_face_index].plane;

	Vector<Vector3> winding = generate_base_winding(plane);

	for (uint32_t other_face_index = 0; other_face_index < p_brush.faces.size(); other_face_index++) {
		if ((int)other_face_index == p_face_index) {
			continue;
		}

		winding = Geometry3D::clip_polygon(winding, -p_brush.faces[other_face_index].plane);
		if (winding.is_empty()) {
			break;
		}
	}

	if (p_vertex_merge_distance > 0.0 && !winding.is_empty()) {
		PackedVector3Array merged_winding;
		const Vector3 snap_step(p_vertex_merge_distance, p_vertex_merge_distance, p_vertex_merge_distance);
		Vector3 prev_vtx = winding[0].snapped(snap_step);
		merged_winding.push_back(prev_vtx);
		for (int i = 1; i < winding.size(); i++) {
			const Vector3 cur_vtx = winding[i].snapped(snap_step);
			if (prev_vtx != cur_vtx) {
				merged_winding.push_back(cur_vtx);
			}
			prev_vtx = cur_vtx;
		}
		return merged_winding;
	}

	PackedVector3Array result;
	for (int i = 0; i < winding.size(); i++) {
		result.push_back(winding[i]);
	}
	return result;
}

void TrenchbroomGeometryGenerator::generate_brush_vertices(int p_entity_index, int p_brush_index) {
	ERR_FAIL_NULL(entity_data);
	EntityData &entity = (*entity_data)[p_entity_index];
	BrushData &brush = entity.brushes[p_brush_index];

	real_t vertex_merge_distance = 0.0;
	if (map_settings && entity.properties.has(map_settings->get_vertex_merge_distance_property())) {
		vertex_merge_distance = entity.properties[map_settings->get_vertex_merge_distance_property()];
	}

	for (uint32_t face_index = 0; face_index < brush.faces.size(); face_index++) {
		FaceData &face = brush.faces[face_index];
		face.vertices = generate_face_vertices(brush, face_index, vertex_merge_distance);
		if (face.disp.valid) {
			apply_face_displacement(face);
		}

		if (!face.disp.valid || !face.disp.has_distance_grid()) {
			face.normals.resize(face.vertices.size());
			for (int i = 0; i < face.vertices.size(); i++) {
				face.normals.set(i, face.plane.normal);
			}
		} else if (face.normals.size() != face.vertices.size()) {
			face.normals.resize(face.vertices.size());
			for (int i = 0; i < face.vertices.size(); i++) {
				face.normals.set(i, face.plane.normal);
			}
		}

		const PackedFloat32Array tangent = TrenchbroomUtil::get_face_tangent(face);
		face.tangents.clear();
		for (int i = 0; i < face.vertices.size(); i++) {
			face.tangents.push_back(tangent[1]);
			face.tangents.push_back(tangent[2]);
			face.tangents.push_back(tangent[0]);
			face.tangents.push_back(tangent[3]);
		}
	}
}

void TrenchbroomGeometryGenerator::generate_entity_vertices(int p_entity_index) {
	ERR_FAIL_NULL(entity_data);
	EntityData &entity = (*entity_data)[p_entity_index];
	for (uint32_t brush_index = 0; brush_index < entity.brushes.size(); brush_index++) {
		const BrushData &brush = entity.brushes[brush_index];
		if (brush.cordon && !(active_build_flags & TrenchbroomMap::INCLUDE_CORDON_VOLUMES)) {
			continue;
		}
		generate_brush_vertices(p_entity_index, brush_index);
	}
}

void TrenchbroomGeometryGenerator::determine_entity_origins(int p_entity_index) {
	ERR_FAIL_NULL(entity_data);
	EntityData &entity = (*entity_data)[p_entity_index];

	const BlaziumFGDSolidClass *solid_def = Object::cast_to<BlaziumFGDSolidClass>(entity.definition.ptr());
	BlaziumFGDSolidClass::OriginType origin_type = BlaziumFGDSolidClass::ORIGIN_BRUSH;

	if (!solid_def) {
		if (entity.brushes.is_empty()) {
			return;
		}
	} else {
		origin_type = solid_def->get_origin_type();
	}

	if (p_entity_index == 0) {
		entity.origin = Vector3();
		return;
	}

	Vector3 entity_mins(Math::INF, Math::INF, Math::INF);
	Vector3 entity_maxs(Math::INF, Math::INF, Math::INF);
	Vector3 origin_mins(Math::INF, Math::INF, Math::INF);
	Vector3 origin_maxs(-Math::INF, -Math::INF, -Math::INF);

	for (uint32_t brush_index = 0; brush_index < entity.brushes.size(); brush_index++) {
		const BrushData &brush = entity.brushes[brush_index];
		for (uint32_t face_index = 0; face_index < brush.faces.size(); face_index++) {
			const FaceData &face = brush.faces[face_index];
			for (int vertex_index = 0; vertex_index < face.vertices.size(); vertex_index++) {
				const Vector3 vertex = face.vertices[vertex_index];
				if (entity_mins.is_finite()) {
					entity_mins = entity_mins.min(vertex);
				} else {
					entity_mins = vertex;
				}
				if (entity_maxs.is_finite()) {
					entity_maxs = entity_maxs.max(vertex);
				} else {
					entity_maxs = vertex;
				}

				if (brush.origin) {
					if (origin_mins.is_finite()) {
						origin_mins = origin_mins.min(vertex);
					} else {
						origin_mins = vertex;
					}
					if (origin_maxs.is_finite()) {
						origin_maxs = origin_maxs.max(vertex);
					} else {
						origin_maxs = vertex;
					}
				}
			}
		}
	}

	if (entity_maxs.is_finite() && entity_mins.is_finite()) {
		entity.origin = entity_maxs - ((entity_maxs - entity_mins) * 0.5);
	}

	if (origin_type != BlaziumFGDSolidClass::ORIGIN_BOUNDS_CENTER && !entity.brushes.is_empty()) {
		switch (origin_type) {
			case BlaziumFGDSolidClass::ORIGIN_ABSOLUTE:
			case BlaziumFGDSolidClass::ORIGIN_RELATIVE: {
				if (entity.properties.has("origin")) {
					const Vector<double> origin_comps = String(entity.properties["origin"]).split_floats(" ", false);
					if (origin_comps.size() > 2 && map_settings) {
						const Vector3 origin_vec(origin_comps[0], origin_comps[1], origin_comps[2]);
						if (origin_type == BlaziumFGDSolidClass::ORIGIN_ABSOLUTE) {
							entity.origin = origin_vec * map_settings->get_scale_factor();
						} else {
							entity.origin += origin_vec * map_settings->get_scale_factor();
						}
					}
				}
			} break;
			case BlaziumFGDSolidClass::ORIGIN_BRUSH: {
				if (origin_mins.is_finite()) {
					entity.origin = origin_maxs - ((origin_maxs - origin_mins) * 0.5);
				}
			} break;
			case BlaziumFGDSolidClass::ORIGIN_BOUNDS_MINS: {
				entity.origin = entity_mins;
			} break;
			case BlaziumFGDSolidClass::ORIGIN_BOUNDS_MAXS: {
				entity.origin = entity_maxs;
			} break;
			case BlaziumFGDSolidClass::ORIGIN_AVERAGED: {
				PackedVector3Array vertices;
				for (uint32_t brush_index = 0; brush_index < entity.brushes.size(); brush_index++) {
					const BrushData &brush = entity.brushes[brush_index];
					for (uint32_t face_index = 0; face_index < brush.faces.size(); face_index++) {
						const FaceData &face = brush.faces[face_index];
						for (int vertex_index = 0; vertex_index < face.vertices.size(); vertex_index++) {
							vertices.push_back(face.vertices[vertex_index]);
						}
					}
				}
				entity.origin = TrenchbroomUtil::op_vec3_avg(vertices);
			} break;
			default:
				break;
		}
	}
}

void TrenchbroomGeometryGenerator::wind_entity_faces(int p_entity_index) {
	ERR_FAIL_NULL(entity_data);
	EntityData &entity = (*entity_data)[p_entity_index];
	for (uint32_t brush_index = 0; brush_index < entity.brushes.size(); brush_index++) {
		BrushData &brush = entity.brushes[brush_index];
		for (uint32_t face_index = 0; face_index < brush.faces.size(); face_index++) {
			FaceData &face = brush.faces[face_index];
			face.wind();
			face.index_vertices();
		}
	}
}

Vector4i TrenchbroomGeometryGenerator::get_plane_lookup_key(const Plane &p_plane) const {
	constexpr real_t OCCLUSION_PRECISION = 100.0;
	return Vector4i(
			Math::round(p_plane.normal.x * OCCLUSION_PRECISION),
			Math::round(p_plane.normal.y * OCCLUSION_PRECISION),
			Math::round(p_plane.normal.z * OCCLUSION_PRECISION),
			Math::round(p_plane.d * OCCLUSION_PRECISION));
}

HashSet<FaceData *> TrenchbroomGeometryGenerator::compute_interior_faces_to_cull(
		const LocalVector<FaceData *> &p_faces,
		const HashMap<FaceData *, uint32_t> &p_face_brush_index,
		real_t p_merge_epsilon) const {
	HashSet<FaceData *> faces_to_remove;

	HashMap<Vector4i, LocalVector<FaceData *>> interior_faces_lookup_dict;
	for (uint32_t face_i = 0; face_i < p_faces.size(); face_i++) {
		FaceData *face = p_faces[face_i];
		if (face->vertices.size() < 3 || is_skip(*face) || is_origin(*face)) {
			continue;
		}
		const Vector4i key = get_plane_lookup_key(face->plane);
		interior_faces_lookup_dict[key].push_back(face);
	}

	HashMap<FaceData *, real_t> face_areas;
	HashMap<FaceData *, AABB> face_aabbs;
	for (uint32_t face_i = 0; face_i < p_faces.size(); face_i++) {
		FaceData *face = p_faces[face_i];
		if (face->vertices.size() < 3) {
			continue;
		}
		face_areas[face] = _face_area(face);
		face_aabbs[face] = _face_aabb(face);
	}

	for (uint32_t face_i = 0; face_i < p_faces.size(); face_i++) {
		FaceData *face = p_faces[face_i];
		if (face->vertices.size() < 3 || is_skip(*face) || is_origin(*face)) {
			continue;
		}
		if (faces_to_remove.has(face)) {
			continue;
		}

		Plane opposite_plane = face->plane;
		opposite_plane.d *= -1.0;
		opposite_plane.normal *= -1.0;
		const Vector4i opposite_key = get_plane_lookup_key(opposite_plane);
		const LocalVector<FaceData *> *lookup_faces = interior_faces_lookup_dict.getptr(opposite_key);
		if (!lookup_faces) {
			continue;
		}

		const real_t face_area = face_areas[face];
		const AABB face_bounds = face_aabbs[face];
		const uint32_t face_brush = p_face_brush_index.has(face) ? p_face_brush_index[face] : UINT32_MAX;

		for (uint32_t face2_i = 0; face2_i < lookup_faces->size(); face2_i++) {
			FaceData *face2 = (*lookup_faces)[face2_i];
			if (face == face2 || faces_to_remove.has(face)) {
				continue;
			}
			if (p_face_brush_index.has(face2) && p_face_brush_index[face2] == face_brush) {
				continue;
			}
			if (face_areas[face2] < face_area) {
				continue;
			}
			if (!_aabb_contains(face_aabbs[face2], face_bounds, p_merge_epsilon)) {
				continue;
			}
			if (!face2->plane.has_point(face->plane.get_center(), 0.0001)) {
				continue;
			}
			if (!(face->plane.normal * -1.0).is_equal_approx(face2->plane.normal)) {
				continue;
			}

			if (_face_vertices_subset(face, face2, p_merge_epsilon)) {
				faces_to_remove.insert(face);
				break;
			}
			if (_face_vertices_inside_face_2d(face, face2)) {
				faces_to_remove.insert(face);
				break;
			}
		}
	}

	return faces_to_remove;
}

void TrenchbroomGeometryGenerator::generate_entity_surfaces(int p_entity_index) {
	ERR_FAIL_NULL(entity_data);
	ERR_FAIL_NULL(map_settings);

	EntityData &entity = (*entity_data)[p_entity_index];
	if (entity.brushes.is_empty()) {
		return;
	}

	const BlaziumFGDSolidClass *solid_def = Object::cast_to<BlaziumFGDSolidClass>(entity.definition.ptr());
	Ref<BlaziumFGDSolidClass> def;
	if (solid_def) {
		def = Ref<BlaziumFGDSolidClass>(entity.definition);
	} else {
		def.instantiate();
	}

	const auto op_entity_ogl_xf = [&](const Vector3 &p_vertex) -> Vector3 {
		return TrenchbroomUtil::id_to_opengl(p_vertex - entity.origin);
	};

	HashMap<String, LocalVector<FaceData *>> surfaces;
	HashMap<FaceData *, uint32_t> face_brush_index;
	int current_metadata_index = 0;
	Array texture_names_metadata;
	PackedInt32Array textures_metadata;
	PackedVector3Array vertices_metadata;
	PackedVector3Array normals_metadata;
	PackedVector3Array positions_metadata;
	Vector<PackedInt32Array> shape_to_face_metadata;
	HashMap<FaceData *, PackedInt32Array> face_index_metadata_map;

	for (uint32_t brush_index = 0; brush_index < entity.brushes.size(); brush_index++) {
		BrushData &brush = entity.brushes[brush_index];
		for (uint32_t face_index = 0; face_index < brush.faces.size(); face_index++) {
			FaceData &face = brush.faces[face_index];
			if (is_skip(face) || is_origin(face)) {
				continue;
			}
			surfaces[face.texture].push_back(&face);
			face_brush_index[&face] = brush_index;
		}
	}

	Vector<String> textures;
	for (const KeyValue<String, LocalVector<FaceData *>> &surface_entry : surfaces) {
		textures.push_back(surface_entry.key);
	}

	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	Vector<Array> mesh_arrays;
	const bool build_concave = entity.is_collision_concave();
	PackedVector3Array concave_vertices;

	const bool cull_interior_faces = entity.properties.has(map_settings->get_cull_interior_faces_property()) &&
			(bool)entity.properties[map_settings->get_cull_interior_faces_property()];
	real_t vertex_merge_epsilon = TrenchbroomUtil::VERTEX_EPSILON;
	if (entity.properties.has(map_settings->get_vertex_merge_distance_property())) {
		const real_t merge_distance = entity.properties[map_settings->get_vertex_merge_distance_property()];
		if (merge_distance > 0.0) {
			vertex_merge_epsilon = merge_distance;
		}
	}

	for (int texture_index = 0; texture_index < textures.size(); texture_index++) {
		const String texture_name = textures[texture_index];
		LocalVector<FaceData *> &faces = surfaces[texture_name];

		HashSet<FaceData *> culled_faces;
		if (cull_interior_faces) {
			culled_faces = compute_interior_faces_to_cull(faces, face_brush_index, vertex_merge_epsilon);
		}

		const int tex_index = texture_names_metadata.size();
		if (def->get_add_textures_metadata()) {
			texture_names_metadata.push_back(texture_name);
		}

		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = PackedVector3Array();
		arrays[Mesh::ARRAY_NORMAL] = PackedVector3Array();
		arrays[Mesh::ARRAY_TANGENT] = PackedFloat32Array();
		arrays[Mesh::ARRAY_TEX_UV] = PackedVector2Array();
		arrays[Mesh::ARRAY_INDEX] = PackedInt32Array();

		int index_offset = 0;

		PackedVector3Array surface_vertices = arrays[Mesh::ARRAY_VERTEX];
		PackedVector3Array surface_normals = arrays[Mesh::ARRAY_NORMAL];
		PackedFloat32Array surface_tangents = arrays[Mesh::ARRAY_TANGENT];
		PackedVector2Array surface_uvs = arrays[Mesh::ARRAY_TEX_UV];
		PackedInt32Array surface_indices = arrays[Mesh::ARRAY_INDEX];

		for (uint32_t face_i = 0; face_i < faces.size(); face_i++) {
			FaceData *face = faces[face_i];

			if (face->vertices.size() < 3 || is_skip(*face) || is_origin(*face)) {
				continue;
			}

			if (cull_interior_faces && culled_faces.has(face)) {
				continue;
			}

			if (build_concave) {
				for (int index_i = 0; index_i < face->indices.size(); index_i++) {
					concave_vertices.push_back(op_entity_ogl_xf(face->vertices[face->indices[index_i]]));
				}
			}

			if (is_clip(*face)) {
				continue;
			}

			const int num_tris = face->indices.size() / 3;
			if (def->get_add_textures_metadata()) {
				for (int tri_i = 0; tri_i < num_tris; tri_i++) {
					textures_metadata.push_back(tex_index);
				}
			}
			if (def->get_add_face_normal_metadata()) {
				const Vector3 opengl_normal = TrenchbroomUtil::id_to_opengl(face->plane.normal);
				for (int tri_i = 0; tri_i < num_tris; tri_i++) {
					normals_metadata.push_back(opengl_normal);
				}
			}
			if (def->get_add_face_position_metadata()) {
				for (int tri_i = 0; tri_i < num_tris; tri_i++) {
					PackedVector3Array triangle_vertices;
					for (int corner = 0; corner < 3; corner++) {
						triangle_vertices.push_back(face->vertices[face->indices[tri_i * 3 + corner]]);
					}
					const Vector3 position = TrenchbroomUtil::op_vec3_avg(triangle_vertices);
					positions_metadata.push_back(op_entity_ogl_xf(position));
				}
			}
			if (def->get_add_vertex_metadata()) {
				for (int index_i = 0; index_i < face->indices.size(); index_i++) {
					vertices_metadata.push_back(op_entity_ogl_xf(face->vertices[face->indices[index_i]]));
				}
			}
			if (def->get_add_collision_shape_to_face_indices_metadata()) {
				PackedInt32Array face_indices;
				for (int tri_i = 0; tri_i < num_tris; tri_i++) {
					face_indices.push_back(current_metadata_index + tri_i);
				}
				face_index_metadata_map[face] = face_indices;
			}
			current_metadata_index += num_tris;

			Vector2 texture_size = Vector2(map_settings->get_inverse_scale_factor(), map_settings->get_inverse_scale_factor());
			if (texture_sizes.has(face->texture)) {
				texture_size = texture_sizes[face->texture];
			}

			for (int vertex_i = 0; vertex_i < face->vertices.size(); vertex_i++) {
				const Vector3 vertex = face->vertices[vertex_i];
				surface_vertices.push_back(op_entity_ogl_xf(vertex));
				surface_normals.push_back(TrenchbroomUtil::id_to_opengl(face->normals[vertex_i]));
				surface_uvs.push_back(TrenchbroomUtil::get_face_vertex_uv(vertex, *face, texture_size));
				for (int tangent_i = 0; tangent_i < 4; tangent_i++) {
					surface_tangents.push_back(face->tangents[(vertex_i * 4) + tangent_i]);
				}
			}

			for (int index_i = 0; index_i < face->indices.size(); index_i++) {
				surface_indices.push_back(face->indices[index_i] + index_offset);
			}

			index_offset += face->vertices.size();
		}

		arrays[Mesh::ARRAY_VERTEX] = surface_vertices;
		arrays[Mesh::ARRAY_NORMAL] = surface_normals;
		arrays[Mesh::ARRAY_TANGENT] = surface_tangents;
		arrays[Mesh::ARRAY_TEX_UV] = surface_uvs;
		arrays[Mesh::ARRAY_INDEX] = surface_indices;

		if (TrenchbroomUtil::filter_face(texture_name, map_settings)) {
			continue;
		}

		if (surface_vertices.size() < 3 || surface_indices.size() < 3) {
			continue;
		}

		mesh_arrays.push_back(arrays);
	}

	textures.erase(map_settings->get_clip_texture());

	if (def->get_build_visuals()) {
		for (int array_index = 0; array_index < mesh_arrays.size(); array_index++) {
			mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, mesh_arrays[array_index]);
			mesh->surface_set_name(array_index, textures[array_index]);
			Ref<Material> material;
			if (texture_materials.has(textures[array_index])) {
				material = texture_materials[textures[array_index]];
			}
			mesh->surface_set_material(array_index, material);
		}

		if (def->get_add_textures_metadata()) {
			entity.mesh_metadata["texture_names"] = texture_names_metadata;
			entity.mesh_metadata["textures"] = textures_metadata;
		}
		if (def->get_add_vertex_metadata()) {
			entity.mesh_metadata["vertices"] = vertices_metadata;
		}
		if (def->get_add_face_normal_metadata()) {
			entity.mesh_metadata["normals"] = normals_metadata;
		}
		if (def->get_add_face_position_metadata()) {
			entity.mesh_metadata["positions"] = positions_metadata;
		}

		entity.mesh = mesh;
	}

	if (entity.is_collision_convex()) {
		for (uint32_t brush_index = 0; brush_index < entity.brushes.size(); brush_index++) {
			const BrushData &brush = entity.brushes[brush_index];
			if (brush.planes.is_empty() || brush.origin) {
				continue;
			}

			const Vector<Vector3> points = Geometry3D::compute_convex_mesh_points(brush.planes.ptr(), brush.planes.size());
			if (points.is_empty()) {
				continue;
			}

			PackedVector3Array transformed_points;
			for (int point_i = 0; point_i < points.size(); point_i++) {
				transformed_points.push_back(op_entity_ogl_xf(points[point_i]));
			}

			Vector<Vector3> shape_points;
			for (int point_i = 0; point_i < transformed_points.size(); point_i++) {
				shape_points.push_back(transformed_points[point_i]);
			}

			Ref<ConvexPolygonShape3D> shape;
			shape.instantiate();
			shape->set_points(shape_points);
			entity.shapes.push_back(shape);

			if (def->get_add_collision_shape_to_face_indices_metadata()) {
				PackedInt32Array face_indices_array;
				for (uint32_t face_index = 0; face_index < brush.faces.size(); face_index++) {
					FaceData *face = const_cast<FaceData *>(&brush.faces[face_index]);
					const PackedInt32Array *face_indices_metadata = face_index_metadata_map.getptr(face);
					if (face_indices_metadata) {
						for (int meta_i = 0; meta_i < face_indices_metadata->size(); meta_i++) {
							face_indices_array.push_back((*face_indices_metadata)[meta_i]);
						}
					}
				}
				shape_to_face_metadata.push_back(face_indices_array);
			}
		}
	} else if (build_concave && !concave_vertices.is_empty()) {
		Ref<ConcavePolygonShape3D> shape;
		shape.instantiate();
		Vector<Vector3> faces;
		for (int i = 0; i < concave_vertices.size(); i++) {
			faces.push_back(concave_vertices[i]);
		}
		shape->set_faces(faces);
		entity.shapes.push_back(shape);

		if (def->get_add_collision_shape_to_face_indices_metadata()) {
			PackedInt32Array face_indices_array;
			for (const KeyValue<FaceData *, PackedInt32Array> &metadata_entry : face_index_metadata_map) {
				const PackedInt32Array &face_indices_metadata = metadata_entry.value;
				for (int meta_i = 0; meta_i < face_indices_metadata.size(); meta_i++) {
					face_indices_array.push_back(face_indices_metadata[meta_i]);
				}
			}
			shape_to_face_metadata.push_back(face_indices_array);
		}
	}

	if (def->get_add_collision_shape_to_face_indices_metadata()) {
		Array shape_metadata;
		for (int i = 0; i < shape_to_face_metadata.size(); i++) {
			shape_metadata.push_back(shape_to_face_metadata[i]);
		}
		entity.mesh_metadata["shape_to_face_array"] = shape_metadata;
	}
}

void TrenchbroomGeometryGenerator::unwrap_uv2s(int p_entity_index, real_t p_texel_size) {
	ERR_FAIL_NULL(entity_data);
	ERR_FAIL_NULL(map_settings);

	EntityData &entity = (*entity_data)[p_entity_index];
	if (entity.mesh.is_valid() && entity.is_gi_enabled() &&
			!entity.is_smooth_shaded(map_settings->get_entity_smoothing_property())) {
		entity.mesh->lightmap_unwrap(Transform3D(), p_texel_size);
	}
}

Error TrenchbroomGeometryGenerator::build(int p_build_flags, LocalVector<EntityData> &p_entities) {
	ERR_FAIL_NULL_V(map_settings, ERR_INVALID_PARAMETER);

	entity_data = &p_entities;
	active_build_flags = p_build_flags;
	const int entity_count = p_entities.size();
	const bool show_profile = p_build_flags & TrenchbroomMap::SHOW_PROFILE_INFO;

	if (show_profile) {
		TrenchbroomUtil::print_profile_info(vformat("Preparing %d entities", entity_count), "[GEO]");
	}

	Array texture_map = TrenchbroomUtil::build_texture_map_parallel(p_entities, map_settings);
	const Dictionary materials_dict = texture_map[0];
	const Dictionary sizes_dict = texture_map[1];

	texture_materials.clear();
	texture_sizes.clear();
	const Array material_keys = materials_dict.keys();
	for (int i = 0; i < material_keys.size(); i++) {
		const String key = material_keys[i];
		texture_materials[key] = materials_dict[key];
	}
	const Array size_keys = sizes_dict.keys();
	for (int i = 0; i < size_keys.size(); i++) {
		const String key = size_keys[i];
		texture_sizes[key] = sizes_dict[key];
	}

	WorkerThreadPool *pool = WorkerThreadPool::get_singleton();
	if (show_profile) {
		TrenchbroomUtil::print_profile_info("Generating brush vertices", "[GEO]");
	}
	WorkerThreadPool::GroupID task_id = pool->add_group_task(
			callable_mp(this, &TrenchbroomGeometryGenerator::generate_entity_vertices),
			entity_count, -1, false, "Generate Brush Vertices");
	pool->wait_for_group_task_completion(task_id);

	if (show_profile) {
		TrenchbroomUtil::print_profile_info("Determining entity origins", "[GEO]");
	}
	task_id = pool->add_group_task(
			callable_mp(this, &TrenchbroomGeometryGenerator::determine_entity_origins),
			entity_count, -1, false, "Determine Entity Origins");
	pool->wait_for_group_task_completion(task_id);

	if (show_profile) {
		TrenchbroomUtil::print_profile_info("Winding brush faces", "[GEO]");
	}
	task_id = pool->add_group_task(
			callable_mp(this, &TrenchbroomGeometryGenerator::wind_entity_faces),
			entity_count, -1, false, "Wind Brush Faces");
	pool->wait_for_group_task_completion(task_id);

	if (show_profile) {
		TrenchbroomUtil::print_profile_info("Generating surfaces", "[GEO]");
	}
	task_id = pool->add_group_task(
			callable_mp(this, &TrenchbroomGeometryGenerator::generate_entity_surfaces),
			entity_count, -1, false, "Generate Surfaces");
	pool->wait_for_group_task_completion(task_id);

	if (show_profile) {
		TrenchbroomUtil::print_profile_info("Generating patches", "[GEO]");
	}
	task_id = pool->add_group_task(
			callable_mp(this, &TrenchbroomGeometryGenerator::generate_entity_patches),
			entity_count, -1, false, "Generate Patches");
	pool->wait_for_group_task_completion(task_id);

	if (show_profile) {
		TrenchbroomUtil::print_profile_info("Generating overlays", "[GEO]");
	}
	task_id = pool->add_group_task(
			callable_mp(this, &TrenchbroomGeometryGenerator::generate_entity_overlays),
			entity_count, -1, false, "Generate Overlays");
	pool->wait_for_group_task_completion(task_id);

	if (p_build_flags & TrenchbroomMap::UNWRAP_UV2) {
		if (show_profile) {
			TrenchbroomUtil::print_profile_info("UV2 unwrap", "[GEO]");
		}
		const real_t texel_size = map_settings->get_uv_unwrap_texel_size() * map_settings->get_scale_factor();
		for (int entity_index = 0; entity_index < entity_count; entity_index++) {
			unwrap_uv2s(entity_index, texel_size);
		}
	}

	if (show_profile) {
		TrenchbroomUtil::print_profile_info("Geometry generation complete", "[GEO]");
	}

	return OK;
}
