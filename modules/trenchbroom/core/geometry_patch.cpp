/**************************************************************************/
/*  geometry_patch.cpp                                                    */
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
#include "modules/trenchbroom/trenchbroom_map_settings.h"
#include "modules/trenchbroom/util/trenchbroom_util.h"

#include "core/math/convex_hull.h"
#include "core/math/geometry_3d.h"
#include "scene/resources/3d/concave_polygon_shape_3d.h"
#include "scene/resources/3d/convex_polygon_shape_3d.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"

void TrenchbroomGeometryGenerator::create_patch_mesh(const LocalVector<PatchData> &p_patches, Ref<ArrayMesh> p_mesh, EntityData &p_entity, const BlaziumFGDSolidClass *p_def, PackedVector3Array &r_concave_vertices, Vector<Vector3> &r_convex_points) {
	ERR_FAIL_NULL(map_settings);

	const bool build_visuals = p_def && p_def->get_build_visuals();
	const auto op_entity_ogl_xf = [&](const Vector3 &p_vertex) -> Vector3 {
		return TrenchbroomUtil::id_to_opengl(p_vertex - p_entity.origin);
	};

	Array texture_names_metadata;
	Array textures_metadata;
	PackedVector3Array vertices_metadata;
	PackedVector3Array normals_metadata;
	PackedVector3Array positions_metadata;

	for (uint32_t patch_index = 0; patch_index < p_patches.size(); patch_index++) {
		const PatchData &patch = p_patches[patch_index];
		if (patch.size.size() < 2 || patch.points.size() < 4) {
			continue;
		}

		const int control_width = patch.size[0];
		const int control_height = patch.size[1];
		if (control_width < 2 || control_height < 2) {
			continue;
		}

		const int subdivisions = 3;
		const int sample_width = (control_width - 1) * subdivisions + 1;
		const int sample_height = (control_height - 1) * subdivisions + 1;

		PackedVector3Array vertices;
		PackedVector3Array normals;
		PackedFloat32Array tangents;
		PackedVector2Array uvs;
		vertices.resize(sample_width * sample_height);
		normals.resize(sample_width * sample_height);
		uvs.resize(sample_width * sample_height);
		tangents.resize(sample_width * sample_height * 4);

		for (int row = 0; row < sample_height; row++) {
			const real_t v = sample_height > 1 ? (real_t)row / (real_t)(sample_height - 1) : 0.0;
			for (int col = 0; col < sample_width; col++) {
				const real_t u = sample_width > 1 ? (real_t)col / (real_t)(sample_width - 1) : 0.0;
				const int idx = col + row * sample_width;
				const Vector3 vertex = TrenchbroomUtil::sample_bezier_surface(patch.points, control_width, control_height, u, v);
				vertices.set(idx, op_entity_ogl_xf(vertex));

				Vector2 uv = Vector2(u, v);
				if (patch.uvs.size() == patch.points.size()) {
					PackedVector3Array uv_controls;
					uv_controls.resize(patch.uvs.size());
					for (int uv_i = 0; uv_i < patch.uvs.size(); uv_i++) {
						uv_controls.write[uv_i] = Vector3(patch.uvs[uv_i].x, patch.uvs[uv_i].y, 0.0);
					}
					const Vector3 sampled_uv = TrenchbroomUtil::sample_bezier_surface(uv_controls, control_width, control_height, u, v);
					uv = Vector2(sampled_uv.x, sampled_uv.y);
				}
				uvs.set(idx, uv);
			}
		}

		for (int row = 0; row < sample_height; row++) {
			for (int col = 0; col < sample_width; col++) {
				const int idx = col + row * sample_width;
				Vector3 normal = Vector3(0, 0, 1);
				if (col > 0 && col < sample_width - 1 && row > 0 && row < sample_height - 1) {
					const Vector3 left = vertices[(col - 1) + row * sample_width];
					const Vector3 right = vertices[(col + 1) + row * sample_width];
					const Vector3 up = vertices[col + (row - 1) * sample_width];
					const Vector3 down = vertices[col + (row + 1) * sample_width];
					normal = (right - left).cross(down - up).normalized();
				}
				normals.set(idx, normal);
				const PackedFloat32Array tangent = TrenchbroomUtil::get_quake_tangent(normal, 1.0, 0.0);
				for (int t = 0; t < 4; t++) {
					tangents.set((idx * 4) + t, tangent[t]);
				}
			}
		}

		PackedInt32Array indices = TrenchbroomUtil::get_triangle_indices(sample_width, sample_height);
		if (indices.is_empty()) {
			continue;
		}

		for (int index_i = 0; index_i < indices.size(); index_i++) {
			const Vector3 transformed = vertices[indices[index_i]];
			r_concave_vertices.push_back(transformed);
			r_convex_points.push_back(transformed);
		}

		if (TrenchbroomUtil::filter_face(patch.texture, map_settings)) {
			continue;
		}

		if (p_def && p_def->get_add_textures_metadata()) {
			const int tex_index = texture_names_metadata.size();
			texture_names_metadata.push_back(patch.texture);
			const int tri_count = indices.size() / 3;
			for (int tri_i = 0; tri_i < tri_count; tri_i++) {
				textures_metadata.push_back(tex_index);
			}
		}
		if (p_def && p_def->get_add_face_normal_metadata()) {
			const int tri_count = indices.size() / 3;
			for (int tri_i = 0; tri_i < tri_count; tri_i++) {
				const Vector3 tri_normal = normals[indices[tri_i * 3]];
				normals_metadata.push_back(tri_normal);
			}
		}
		if (p_def && p_def->get_add_face_position_metadata()) {
			const int tri_count = indices.size() / 3;
			for (int tri_i = 0; tri_i < tri_count; tri_i++) {
				PackedVector3Array triangle_vertices;
				for (int corner = 0; corner < 3; corner++) {
					triangle_vertices.push_back(vertices[indices[tri_i * 3 + corner]]);
				}
				positions_metadata.push_back(TrenchbroomUtil::op_vec3_avg(triangle_vertices));
			}
		}
		if (p_def && p_def->get_add_vertex_metadata()) {
			for (int index_i = 0; index_i < indices.size(); index_i++) {
				vertices_metadata.push_back(vertices[indices[index_i]]);
			}
		}

		if (!build_visuals || p_mesh.is_null()) {
			continue;
		}

		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = vertices;
		arrays[Mesh::ARRAY_NORMAL] = normals;
		arrays[Mesh::ARRAY_TANGENT] = tangents;
		arrays[Mesh::ARRAY_TEX_UV] = uvs;
		arrays[Mesh::ARRAY_INDEX] = indices;

		const int surface_index = p_mesh->get_surface_count();
		p_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
		p_mesh->surface_set_name(surface_index, patch.texture);

		Ref<Material> material;
		if (texture_materials.has(patch.texture)) {
			material = texture_materials[patch.texture];
		}
		p_mesh->surface_set_material(surface_index, material);
	}

	if (p_def && build_visuals && p_mesh.is_valid()) {
		if (p_def->get_add_textures_metadata()) {
			if (p_entity.mesh_metadata.has("texture_names")) {
				Array existing = p_entity.mesh_metadata["texture_names"];
				for (int i = 0; i < texture_names_metadata.size(); i++) {
					existing.push_back(texture_names_metadata[i]);
				}
				p_entity.mesh_metadata["texture_names"] = existing;
			} else {
				p_entity.mesh_metadata["texture_names"] = texture_names_metadata;
			}
			if (p_entity.mesh_metadata.has("textures")) {
				Array existing = p_entity.mesh_metadata["textures"];
				for (int i = 0; i < textures_metadata.size(); i++) {
					existing.push_back(textures_metadata[i]);
				}
				p_entity.mesh_metadata["textures"] = existing;
			} else {
				p_entity.mesh_metadata["textures"] = textures_metadata;
			}
		}
		if (p_def->get_add_vertex_metadata()) {
			if (p_entity.mesh_metadata.has("vertices")) {
				PackedVector3Array existing = p_entity.mesh_metadata["vertices"];
				existing.append_array(vertices_metadata);
				p_entity.mesh_metadata["vertices"] = existing;
			} else {
				p_entity.mesh_metadata["vertices"] = vertices_metadata;
			}
		}
		if (p_def->get_add_face_normal_metadata()) {
			if (p_entity.mesh_metadata.has("normals")) {
				PackedVector3Array existing = p_entity.mesh_metadata["normals"];
				existing.append_array(normals_metadata);
				p_entity.mesh_metadata["normals"] = existing;
			} else {
				p_entity.mesh_metadata["normals"] = normals_metadata;
			}
		}
		if (p_def->get_add_face_position_metadata()) {
			if (p_entity.mesh_metadata.has("positions")) {
				PackedVector3Array existing = p_entity.mesh_metadata["positions"];
				existing.append_array(positions_metadata);
				p_entity.mesh_metadata["positions"] = existing;
			} else {
				p_entity.mesh_metadata["positions"] = positions_metadata;
			}
		}
	}
}

void TrenchbroomGeometryGenerator::generate_entity_patches(int p_entity_index) {
	ERR_FAIL_NULL(entity_data);
	EntityData &entity = (*entity_data)[p_entity_index];
	if (entity.patches.is_empty()) {
		return;
	}

	const BlaziumFGDSolidClass *def = Object::cast_to<BlaziumFGDSolidClass>(entity.definition.ptr());
	const bool build_visuals = def && def->get_build_visuals();

	Ref<ArrayMesh> mesh = entity.mesh;
	if (build_visuals) {
		if (mesh.is_null()) {
			mesh.instantiate();
			entity.mesh = mesh;
		}
	}

	PackedVector3Array concave_vertices;
	Vector<Vector3> convex_points;
	create_patch_mesh(entity.patches, mesh, entity, def, concave_vertices, convex_points);

	if (build_visuals && mesh.is_valid() && mesh->get_surface_count() > 0) {
		entity.mesh = mesh;
	}

	const bool has_brush_collision = !entity.brushes.is_empty();
	if (has_brush_collision) {
		return;
	}

	if (entity.is_collision_concave() && !concave_vertices.is_empty()) {
		Ref<ConcavePolygonShape3D> shape;
		shape.instantiate();
		Vector<Vector3> faces;
		for (int i = 0; i < concave_vertices.size(); i++) {
			faces.push_back(concave_vertices[i]);
		}
		shape->set_faces(faces);
		entity.shapes.push_back(shape);
	} else if (entity.is_collision_convex() && !convex_points.is_empty()) {
		Geometry3D::MeshData hull_mesh;
		if (ConvexHullComputer::convex_hull(convex_points, hull_mesh) == OK && !hull_mesh.vertices.is_empty()) {
			Vector<Vector3> shape_points;
			for (uint32_t i = 0; i < hull_mesh.vertices.size(); i++) {
				shape_points.push_back(hull_mesh.vertices[i]);
			}
			Ref<ConvexPolygonShape3D> shape;
			shape.instantiate();
			shape->set_points(shape_points);
			entity.shapes.push_back(shape);
		}
	}
}
