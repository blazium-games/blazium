/**************************************************************************/
/*  geometry_generator.h                                                  */
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

#pragma once

#include "core/object/ref_counted.h"
#include "core/templates/hash_set.h"
#include "modules/trenchbroom/core/data.h"

class TrenchbroomMapSettings;
class BlaziumFGDSolidClass;

class TrenchbroomGeometryGenerator : public RefCounted {
	GDCLASS(TrenchbroomGeometryGenerator, RefCounted);

protected:
	static void _bind_methods();

	const TrenchbroomMapSettings *map_settings = nullptr;
	real_t hyperplane_size = 512.0;
	LocalVector<EntityData> *entity_data = nullptr;
	HashMap<String, Ref<Material>> texture_materials;
	HashMap<String, Vector2> texture_sizes;
	int active_build_flags = 0;

	bool is_skip(const FaceData &p_face) const;
	bool is_clip(const FaceData &p_face) const;
	bool is_origin(const FaceData &p_face) const;

	PackedVector3Array generate_base_winding(const Plane &p_plane) const;
	PackedVector3Array generate_face_vertices(const BrushData &p_brush, int p_face_index, real_t p_vertex_merge_distance = 0.0) const;
	void generate_brush_vertices(int p_entity_index, int p_brush_index);
	void generate_entity_vertices(int p_entity_index);
	void determine_entity_origins(int p_entity_index);
	void wind_entity_faces(int p_entity_index);
	Vector4i get_plane_lookup_key(const Plane &p_plane) const;
	void generate_entity_surfaces(int p_entity_index);
	void generate_entity_patches(int p_entity_index);
	void generate_entity_overlays(int p_entity_index);
	void apply_face_displacement(FaceData &p_face) const;
	void create_patch_mesh(const LocalVector<PatchData> &p_patches, Ref<ArrayMesh> p_mesh, EntityData &p_entity, const BlaziumFGDSolidClass *p_def, PackedVector3Array &r_concave_vertices, Vector<Vector3> &r_convex_points);
	void unwrap_uv2s(int p_entity_index, real_t p_texel_size);

public:
	HashSet<FaceData *> compute_interior_faces_to_cull(
			const LocalVector<FaceData *> &p_faces,
			const HashMap<FaceData *, uint32_t> &p_face_brush_index,
			real_t p_merge_epsilon) const;

	TrenchbroomGeometryGenerator() = default;
	TrenchbroomGeometryGenerator(const TrenchbroomMapSettings *p_settings, real_t p_hyperplane_size = 512.0);

	Error build(int p_build_flags, LocalVector<EntityData> &p_entities);
};
