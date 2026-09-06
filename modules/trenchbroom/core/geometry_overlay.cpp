/**************************************************************************/
/*  geometry_overlay.cpp                                                  */
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

void TrenchbroomGeometryGenerator::generate_entity_overlays(int p_entity_index) {
	ERR_FAIL_NULL(entity_data);
	ERR_FAIL_NULL(map_settings);

	EntityData &entity = (*entity_data)[p_entity_index];
	if (entity.overlays.is_empty()) {
		return;
	}

	Ref<ArrayMesh> mesh = entity.mesh;
	if (mesh.is_null()) {
		mesh.instantiate();
		entity.mesh = mesh;
	}

	for (uint32_t overlay_index = 0; overlay_index < entity.overlays.size(); overlay_index++) {
		const OverlayData &overlay = entity.overlays[overlay_index];
		if (overlay.material.is_empty()) {
			continue;
		}

		Vector3 u = overlay.basis_u;
		Vector3 v = overlay.basis_v;
		if (u.length_squared() < 0.0001 || v.length_squared() < 0.0001) {
			u = Vector3(32, 0, 0) * map_settings->get_scale_factor();
			v = Vector3(0, 32, 0) * map_settings->get_scale_factor();
		}
		const Vector3 origin = overlay.origin - entity.origin;

		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = PackedVector3Array{
			origin,
			origin + v,
			origin + u + v,
			origin + u,
		};
		arrays[Mesh::ARRAY_NORMAL] = PackedVector3Array{
			overlay.basis_normal,
			overlay.basis_normal,
			overlay.basis_normal,
			overlay.basis_normal,
		};
		arrays[Mesh::ARRAY_TEX_UV] = PackedVector2Array{
			overlay.uv0,
			overlay.uv1,
			overlay.uv2,
			overlay.uv3,
		};
		arrays[Mesh::ARRAY_INDEX] = PackedInt32Array{ 0, 1, 2, 0, 2, 3 };

		const int surface_index = mesh->get_surface_count();
		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
		mesh->surface_set_name(surface_index, overlay.material);
		if (texture_materials.has(overlay.material)) {
			mesh->surface_set_material(surface_index, texture_materials[overlay.material]);
		}
	}
}
