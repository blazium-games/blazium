/**************************************************************************/
/*  data.cpp                                                              */
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

#include "data.h"

#include "modules/trenchbroom/fgd/blazium_fgd_solid_class.h"
#include "modules/trenchbroom/util/trenchbroom_util.h"

int DispInfoData::get_grid_size() const {
	const int clamped_power = CLAMP(power, 2, 4);
	return (1 << clamped_power) + 1;
}

bool DispInfoData::has_distance_grid() const {
	const int grid_size = get_grid_size();
	return distances.size() == grid_size * grid_size;
}

bool DispInfoData::is_vertex_allowed(int p_index) const {
	const int grid_cells = get_grid_size() * get_grid_size();
	if (allowed_verts.is_empty()) {
		return true;
	}
	if (allowed_verts.size() == grid_cells && p_index >= 0 && p_index < allowed_verts.size()) {
		return allowed_verts[p_index] >= 0;
	}
	return true;
}

Vector3 FaceData::get_centroid() const {
	return TrenchbroomUtil::op_vec3_avg(vertices);
}

Vector3 FaceData::get_basis() const {
	if (vertices.size() < 2) {
		return Vector3();
	}
	return (vertices[1] - vertices[0]).normalized();
}

void FaceData::wind() {
	if (vertices.size() < 3) {
		return;
	}
	const Vector3 centroid = get_centroid();
	const Vector3 u_axis = get_basis();
	const Vector3 v_axis = u_axis.cross(plane.normal).normalized();

	Vector<Vector3> sorted;
	sorted.resize(vertices.size());
	for (int i = 0; i < vertices.size(); i++) {
		sorted.write[i] = vertices[i];
	}

	struct FaceVertexSort {
		Vector3 centroid;
		Vector3 u_axis;
		Vector3 v_axis;

		bool operator()(const Vector3 &a, const Vector3 &b) const {
			const Vector3 dir_a = a - centroid;
			const Vector3 dir_b = b - centroid;
			const real_t angle_a = Math::atan2(dir_a.dot(v_axis), dir_a.dot(u_axis));
			const real_t angle_b = Math::atan2(dir_b.dot(v_axis), dir_b.dot(u_axis));
			return angle_a < angle_b;
		}
	};

	sorted.sort_custom<FaceVertexSort>(FaceVertexSort{ centroid, u_axis, v_axis });

	vertices = PackedVector3Array();
	for (int i = 0; i < sorted.size(); i++) {
		vertices.push_back(sorted[i]);
	}
}

void FaceData::index_vertices() {
	if (vertices.size() < 3) {
		indices.clear();
		return;
	}
	const int tri_count = vertices.size() - 2;
	indices.resize(tri_count * 3);
	int index = 0;
	for (int i = 0; i < tri_count; i++) {
		indices.set(index, 0);
		indices.set(index + 1, i + 1);
		indices.set(index + 2, i + 2);
		index += 3;
	}
}

bool EntityData::is_visual() const {
	const BlaziumFGDSolidClass *solid = Object::cast_to<BlaziumFGDSolidClass>(definition.ptr());
	return solid && solid->get_build_visuals();
}

bool EntityData::is_gi_enabled() const {
	const BlaziumFGDSolidClass *solid = Object::cast_to<BlaziumFGDSolidClass>(definition.ptr());
	return solid && solid->get_global_illumination_mode() != GeometryInstance3D::GI_MODE_DISABLED;
}

bool EntityData::is_collision_convex() const {
	const BlaziumFGDSolidClass *solid = Object::cast_to<BlaziumFGDSolidClass>(definition.ptr());
	return solid && solid->get_collision_shape_type() == BlaziumFGDSolidClass::COLLISION_CONVEX;
}

bool EntityData::is_collision_concave() const {
	const BlaziumFGDSolidClass *solid = Object::cast_to<BlaziumFGDSolidClass>(definition.ptr());
	return solid && solid->get_collision_shape_type() == BlaziumFGDSolidClass::COLLISION_CONCAVE;
}

bool EntityData::is_smooth_shaded(const String &p_smoothing_property) const {
	if (!properties.has(p_smoothing_property)) {
		return false;
	}
	return (int)properties[p_smoothing_property] != 0;
}

real_t EntityData::get_smoothing_angle(const String &p_smoothing_angle_property) const {
	if (properties.has(p_smoothing_angle_property)) {
		return properties[p_smoothing_angle_property];
	}
	return 89.0;
}
