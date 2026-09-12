/**************************************************************************/
/*  geometry_displacement.cpp                                             */
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

#include "modules/trenchbroom/trenchbroom_map_settings.h"

#include "core/math/math_defs.h"

static void _disp_face_plane_basis(const FaceData *p_face, Vector3 &r_origin, Vector3 &r_u, Vector3 &r_v) {
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

static Vector2 _disp_project_to_face_plane(const Vector3 &p_vertex, const Vector3 &p_origin, const Vector3 &p_u, const Vector3 &p_v) {
	const Vector3 rel = p_vertex - p_origin;
	return Vector2(p_u.dot(rel), p_v.dot(rel));
}

static int _find_closest_vertex_index(const PackedVector3Array &p_vertices, const Vector3 &p_position) {
	int closest_index = 0;
	real_t closest_distance = Math::INF;
	for (int i = 0; i < p_vertices.size(); i++) {
		const real_t distance = p_vertices[i].distance_squared_to(p_position);
		if (distance < closest_distance) {
			closest_distance = distance;
			closest_index = i;
		}
	}
	return closest_index;
}

static Vector3 _project_on_plane(const Vector3 &p_vector, const Vector3 &p_normal) {
	return p_vector - p_normal * p_normal.dot(p_vector);
}

static void _build_disp_quad_corners(const FaceData &p_face, Vector3 &r_c0, Vector3 &r_c1, Vector3 &r_c2, Vector3 &r_c3) {
	const Vector3 normal = p_face.plane.normal;
	PackedVector3Array corners;
	if (p_face.vertices.size() == 4) {
		corners = p_face.vertices;
	} else {
		Vector3 origin;
		Vector3 u_axis;
		Vector3 v_axis;
		_disp_face_plane_basis(&p_face, origin, u_axis, v_axis);

		real_t min_u = Math::INF;
		real_t max_u = -Math::INF;
		real_t min_v = Math::INF;
		real_t max_v = -Math::INF;
		for (int i = 0; i < p_face.vertices.size(); i++) {
			const Vector2 projected = _disp_project_to_face_plane(p_face.vertices[i], origin, u_axis, v_axis);
			min_u = MIN(min_u, projected.x);
			max_u = MAX(max_u, projected.x);
			min_v = MIN(min_v, projected.y);
			max_v = MAX(max_v, projected.y);
		}
		corners.push_back(origin + u_axis * min_u + v_axis * min_v);
		corners.push_back(origin + u_axis * max_u + v_axis * min_v);
		corners.push_back(origin + u_axis * max_u + v_axis * max_v);
		corners.push_back(origin + u_axis * min_u + v_axis * max_v);
	}

	const int start_index = _find_closest_vertex_index(corners, p_face.disp.startposition);
	const int next_index = (start_index + 1) % 4;
	const int prev_index = (start_index + 3) % 4;
	const int opposite_index = (start_index + 2) % 4;

	Vector3 row_axis = _project_on_plane(corners[next_index] - corners[start_index], normal);
	if (row_axis.length_squared() < 0.0001) {
		row_axis = _project_on_plane(corners[prev_index] - corners[start_index], normal);
	}
	if (p_face.uv_axes.size() > 0) {
		const Vector3 u_axis = _project_on_plane(p_face.uv_axes[0], normal);
		if (u_axis.length_squared() > 0.0001) {
			row_axis = u_axis.normalized();
		}
	}
	if (row_axis.length_squared() > 0.0001) {
		row_axis.normalize();
	} else {
		row_axis = _project_on_plane(Vector3(1, 0, 0), normal).normalized();
	}

	const Vector3 next_edge = corners[next_index] - corners[start_index];
	const Vector3 prev_edge = corners[prev_index] - corners[start_index];
	const bool next_is_row = next_edge.dot(row_axis) >= prev_edge.dot(row_axis);

	r_c0 = corners[start_index];
	if (next_is_row) {
		r_c1 = corners[next_index];
		r_c3 = corners[prev_index];
	} else {
		r_c1 = corners[prev_index];
		r_c3 = corners[next_index];
	}
	r_c2 = corners[opposite_index];
}

static Vector3 _bilinear_quad(const Vector3 &p_c0, const Vector3 &p_c1, const Vector3 &p_c2, const Vector3 &p_c3, real_t p_u, real_t p_v) {
	return p_c0 * ((1.0 - p_u) * (1.0 - p_v)) +
			p_c1 * (p_u * (1.0 - p_v)) +
			p_c2 * (p_u * p_v) +
			p_c3 * ((1.0 - p_u) * p_v);
}

static void _accumulate_grid_normals(const PackedVector3Array &p_vertices, const PackedInt32Array &p_indices, PackedVector3Array &r_normals) {
	r_normals.resize(p_vertices.size());
	for (int i = 0; i < r_normals.size(); i++) {
		r_normals.write[i] = Vector3();
	}
	for (int i = 0; i + 2 < p_indices.size(); i += 3) {
		const int i0 = p_indices[i];
		const int i1 = p_indices[i + 1];
		const int i2 = p_indices[i + 2];
		const Vector3 face_normal = (p_vertices[i1] - p_vertices[i0]).cross(p_vertices[i2] - p_vertices[i0]).normalized();
		r_normals.write[i0] += face_normal;
		r_normals.write[i1] += face_normal;
		r_normals.write[i2] += face_normal;
	}
	for (int i = 0; i < r_normals.size(); i++) {
		if (r_normals[i].length_squared() > 0.0001) {
			r_normals.write[i] = r_normals[i].normalized();
		}
	}
}

void TrenchbroomGeometryGenerator::apply_face_displacement(FaceData &p_face) const {
	if (!p_face.disp.valid || p_face.vertices.size() < 3) {
		return;
	}

	const int subdivisions = 1 << CLAMP(p_face.disp.power, 2, 4);
	const int grid_size = subdivisions + 1;
	const Vector3 face_normal = p_face.plane.normal;

	Vector3 c0;
	Vector3 c1;
	Vector3 c2;
	Vector3 c3;
	_build_disp_quad_corners(p_face, c0, c1, c2, c3);

	const bool use_distance_grid = p_face.disp.has_distance_grid();
	const bool has_disp_normals = p_face.disp.disp_normals.size() == grid_size * grid_size;
	const bool has_offsets = p_face.disp.offsets.size() == grid_size * grid_size;
	const bool has_offset_normals = p_face.disp.offset_normals.size() == grid_size * grid_size;
	const bool has_alphas = p_face.disp.alphas.size() == grid_size * grid_size;

	PackedVector3Array grid_vertices;
	grid_vertices.resize(grid_size * grid_size);
	for (int y = 0; y < grid_size; y++) {
		for (int x = 0; x < grid_size; x++) {
			const real_t fu = (subdivisions == 0) ? 0.0 : (real_t)x / (real_t)subdivisions;
			const real_t fv = (subdivisions == 0) ? 0.0 : (real_t)y / (real_t)subdivisions;
			const int grid_index = y * grid_size + x;
			Vector3 pos = _bilinear_quad(c0, c1, c2, c3, fu, fv);
			const Vector3 flat_pos = pos;

			if (!p_face.disp.is_vertex_allowed(grid_index)) {
				grid_vertices.write[grid_index] = pos;
				continue;
			}

			Vector3 vertex_normal = face_normal;
			real_t displacement = p_face.disp.elevation;

			if (use_distance_grid) {
				displacement += p_face.disp.distances[grid_index];
				if (has_disp_normals) {
					vertex_normal = p_face.disp.disp_normals[grid_index];
				}
			} else if (p_face.disp.elevation != 0.0) {
				const real_t weight = Math::sin(fu * Math::PI) * Math::sin(fv * Math::PI);
				displacement = p_face.disp.elevation * weight;
			} else {
				displacement = 0.0;
			}

			pos += vertex_normal * displacement;
			if (has_offsets) {
				if (has_offset_normals) {
					const real_t offset_magnitude = p_face.disp.offsets[grid_index].length();
					pos += p_face.disp.offset_normals[grid_index] * offset_magnitude;
				} else {
					pos += p_face.disp.offsets[grid_index];
				}
			}
			if (has_alphas) {
				const real_t alpha = CLAMP(p_face.disp.alphas[grid_index], (real_t)0.0, (real_t)1.0);
				pos = flat_pos.lerp(pos, alpha);
			}
			grid_vertices.write[grid_index] = pos;
		}
	}

	PackedInt32Array grid_indices;
	for (int y = 0; y < subdivisions; y++) {
		for (int x = 0; x < subdivisions; x++) {
			const int i0 = y * grid_size + x;
			const int i1 = i0 + 1;
			const int i2 = i0 + grid_size;
			const int i3 = i2 + 1;
			grid_indices.push_back(i0);
			grid_indices.push_back(i2);
			grid_indices.push_back(i1);
			grid_indices.push_back(i1);
			grid_indices.push_back(i2);
			grid_indices.push_back(i3);
		}
	}

	p_face.vertices = grid_vertices;
	p_face.indices = grid_indices;

	if (use_distance_grid) {
		PackedVector3Array averaged_normals;
		_accumulate_grid_normals(grid_vertices, grid_indices, averaged_normals);
		p_face.normals = averaged_normals;
	}
}
