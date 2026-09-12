/**************************************************************************/
/*  blazium_fgd_solid_class.h                                             */
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

#include "blazium_fgd_entity_class.h"

#include "scene/3d/visual_instance_3d.h"

class BlaziumFGDSolidClass : public BlaziumFGDEntityClass {
	GDCLASS(BlaziumFGDSolidClass, BlaziumFGDEntityClass);

public:
	enum SpawnType {
		SPAWN_WORLDSPAWN,
		SPAWN_MERGE_WORLDSPAWN,
		SPAWN_ENTITY,
	};

	enum OriginType {
		ORIGIN_AVERAGED,
		ORIGIN_ABSOLUTE,
		ORIGIN_RELATIVE,
		ORIGIN_BRUSH,
		ORIGIN_BOUNDS_CENTER,
		ORIGIN_BOUNDS_MINS,
		ORIGIN_BOUNDS_MAXS,
	};

	enum CollisionShapeType {
		COLLISION_NONE,
		COLLISION_CONVEX,
		COLLISION_CONCAVE,
	};

protected:
	static void _bind_methods();

	SpawnType spawn_type = SPAWN_ENTITY;
	OriginType origin_type = ORIGIN_AVERAGED;
	bool build_visuals = true;
	GeometryInstance3D::GIMode global_illumination_mode = GeometryInstance3D::GI_MODE_STATIC;
	GeometryInstance3D::ShadowCastingSetting shadow_casting_setting = GeometryInstance3D::SHADOW_CASTING_SETTING_ON;
	bool build_occlusion = false;
	uint32_t render_layers = 1;
	CollisionShapeType collision_shape_type = COLLISION_CONVEX;
	uint32_t collision_layer = 1;
	uint32_t collision_mask = 1;
	real_t collision_priority = 1.0;
	real_t collision_shape_margin = 0.04;
	bool add_textures_metadata = false;
	bool add_vertex_metadata = false;
	bool add_face_position_metadata = false;
	bool add_face_normal_metadata = false;
	bool add_collision_shape_to_face_indices_metadata = false;
	Ref<Script> script_class;

public:
	BlaziumFGDSolidClass();

	SpawnType get_spawn_type() const { return spawn_type; }
	void set_spawn_type(SpawnType p_type) { spawn_type = p_type; }

	OriginType get_origin_type() const { return origin_type; }
	void set_origin_type(OriginType p_type) { origin_type = p_type; }

	bool get_build_visuals() const { return build_visuals; }
	void set_build_visuals(bool p_build) { build_visuals = p_build; }

	GeometryInstance3D::GIMode get_global_illumination_mode() const { return global_illumination_mode; }
	void set_global_illumination_mode(GeometryInstance3D::GIMode p_mode) { global_illumination_mode = p_mode; }

	GeometryInstance3D::ShadowCastingSetting get_shadow_casting_setting() const { return shadow_casting_setting; }
	void set_shadow_casting_setting(GeometryInstance3D::ShadowCastingSetting p_setting) { shadow_casting_setting = p_setting; }

	bool get_build_occlusion() const { return build_occlusion; }
	void set_build_occlusion(bool p_build) { build_occlusion = p_build; }

	uint32_t get_render_layers() const { return render_layers; }
	void set_render_layers(uint32_t p_layers) { render_layers = p_layers; }

	CollisionShapeType get_collision_shape_type() const { return collision_shape_type; }
	void set_collision_shape_type(CollisionShapeType p_type) { collision_shape_type = p_type; }

	uint32_t get_collision_layer() const { return collision_layer; }
	void set_collision_layer(uint32_t p_layer) { collision_layer = p_layer; }

	uint32_t get_collision_mask() const { return collision_mask; }
	void set_collision_mask(uint32_t p_mask) { collision_mask = p_mask; }

	real_t get_collision_priority() const { return collision_priority; }
	void set_collision_priority(real_t p_priority) { collision_priority = p_priority; }

	real_t get_collision_shape_margin() const { return collision_shape_margin; }
	void set_collision_shape_margin(real_t p_margin) { collision_shape_margin = p_margin; }

	bool get_add_textures_metadata() const { return add_textures_metadata; }
	void set_add_textures_metadata(bool p_add) { add_textures_metadata = p_add; }

	bool get_add_vertex_metadata() const { return add_vertex_metadata; }
	void set_add_vertex_metadata(bool p_add) { add_vertex_metadata = p_add; }

	bool get_add_face_position_metadata() const { return add_face_position_metadata; }
	void set_add_face_position_metadata(bool p_add) { add_face_position_metadata = p_add; }

	bool get_add_face_normal_metadata() const { return add_face_normal_metadata; }
	void set_add_face_normal_metadata(bool p_add) { add_face_normal_metadata = p_add; }

	bool get_add_collision_shape_to_face_indices_metadata() const { return add_collision_shape_to_face_indices_metadata; }
	void set_add_collision_shape_to_face_indices_metadata(bool p_add) { add_collision_shape_to_face_indices_metadata = p_add; }

	Ref<Script> get_script_class() const { return script_class; }
	void set_script_class(const Ref<Script> &p_script) { script_class = p_script; }
};

VARIANT_ENUM_CAST(BlaziumFGDSolidClass::SpawnType);
VARIANT_ENUM_CAST(BlaziumFGDSolidClass::OriginType);
VARIANT_ENUM_CAST(BlaziumFGDSolidClass::CollisionShapeType);
