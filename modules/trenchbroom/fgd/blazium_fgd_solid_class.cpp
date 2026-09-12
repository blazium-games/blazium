/**************************************************************************/
/*  blazium_fgd_solid_class.cpp                                           */
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

#include "blazium_fgd_solid_class.h"

#include "core/object/class_db.h"

void BlaziumFGDSolidClass::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_spawn_type", "spawn_type"), &BlaziumFGDSolidClass::set_spawn_type);
	ClassDB::bind_method(D_METHOD("get_spawn_type"), &BlaziumFGDSolidClass::get_spawn_type);
	ClassDB::bind_method(D_METHOD("set_origin_type", "origin_type"), &BlaziumFGDSolidClass::set_origin_type);
	ClassDB::bind_method(D_METHOD("get_origin_type"), &BlaziumFGDSolidClass::get_origin_type);
	ClassDB::bind_method(D_METHOD("set_build_visuals", "build_visuals"), &BlaziumFGDSolidClass::set_build_visuals);
	ClassDB::bind_method(D_METHOD("get_build_visuals"), &BlaziumFGDSolidClass::get_build_visuals);
	ClassDB::bind_method(D_METHOD("set_global_illumination_mode", "global_illumination_mode"), &BlaziumFGDSolidClass::set_global_illumination_mode);
	ClassDB::bind_method(D_METHOD("get_global_illumination_mode"), &BlaziumFGDSolidClass::get_global_illumination_mode);
	ClassDB::bind_method(D_METHOD("set_shadow_casting_setting", "shadow_casting_setting"), &BlaziumFGDSolidClass::set_shadow_casting_setting);
	ClassDB::bind_method(D_METHOD("get_shadow_casting_setting"), &BlaziumFGDSolidClass::get_shadow_casting_setting);
	ClassDB::bind_method(D_METHOD("set_build_occlusion", "build_occlusion"), &BlaziumFGDSolidClass::set_build_occlusion);
	ClassDB::bind_method(D_METHOD("get_build_occlusion"), &BlaziumFGDSolidClass::get_build_occlusion);
	ClassDB::bind_method(D_METHOD("set_render_layers", "render_layers"), &BlaziumFGDSolidClass::set_render_layers);
	ClassDB::bind_method(D_METHOD("get_render_layers"), &BlaziumFGDSolidClass::get_render_layers);
	ClassDB::bind_method(D_METHOD("set_collision_shape_type", "collision_shape_type"), &BlaziumFGDSolidClass::set_collision_shape_type);
	ClassDB::bind_method(D_METHOD("get_collision_shape_type"), &BlaziumFGDSolidClass::get_collision_shape_type);
	ClassDB::bind_method(D_METHOD("set_collision_layer", "collision_layer"), &BlaziumFGDSolidClass::set_collision_layer);
	ClassDB::bind_method(D_METHOD("get_collision_layer"), &BlaziumFGDSolidClass::get_collision_layer);
	ClassDB::bind_method(D_METHOD("set_collision_mask", "collision_mask"), &BlaziumFGDSolidClass::set_collision_mask);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &BlaziumFGDSolidClass::get_collision_mask);
	ClassDB::bind_method(D_METHOD("set_collision_priority", "collision_priority"), &BlaziumFGDSolidClass::set_collision_priority);
	ClassDB::bind_method(D_METHOD("get_collision_priority"), &BlaziumFGDSolidClass::get_collision_priority);
	ClassDB::bind_method(D_METHOD("set_collision_shape_margin", "collision_shape_margin"), &BlaziumFGDSolidClass::set_collision_shape_margin);
	ClassDB::bind_method(D_METHOD("get_collision_shape_margin"), &BlaziumFGDSolidClass::get_collision_shape_margin);
	ClassDB::bind_method(D_METHOD("set_add_textures_metadata", "add_textures_metadata"), &BlaziumFGDSolidClass::set_add_textures_metadata);
	ClassDB::bind_method(D_METHOD("get_add_textures_metadata"), &BlaziumFGDSolidClass::get_add_textures_metadata);
	ClassDB::bind_method(D_METHOD("set_add_vertex_metadata", "add_vertex_metadata"), &BlaziumFGDSolidClass::set_add_vertex_metadata);
	ClassDB::bind_method(D_METHOD("get_add_vertex_metadata"), &BlaziumFGDSolidClass::get_add_vertex_metadata);
	ClassDB::bind_method(D_METHOD("set_add_face_position_metadata", "add_face_position_metadata"), &BlaziumFGDSolidClass::set_add_face_position_metadata);
	ClassDB::bind_method(D_METHOD("get_add_face_position_metadata"), &BlaziumFGDSolidClass::get_add_face_position_metadata);
	ClassDB::bind_method(D_METHOD("set_add_face_normal_metadata", "add_face_normal_metadata"), &BlaziumFGDSolidClass::set_add_face_normal_metadata);
	ClassDB::bind_method(D_METHOD("get_add_face_normal_metadata"), &BlaziumFGDSolidClass::get_add_face_normal_metadata);
	ClassDB::bind_method(D_METHOD("set_add_collision_shape_to_face_indices_metadata", "add_collision_shape_to_face_indices_metadata"), &BlaziumFGDSolidClass::set_add_collision_shape_to_face_indices_metadata);
	ClassDB::bind_method(D_METHOD("get_add_collision_shape_to_face_indices_metadata"), &BlaziumFGDSolidClass::get_add_collision_shape_to_face_indices_metadata);
	ClassDB::bind_method(D_METHOD("set_script_class", "script_class"), &BlaziumFGDSolidClass::set_script_class);
	ClassDB::bind_method(D_METHOD("get_script_class"), &BlaziumFGDSolidClass::get_script_class);

	BIND_ENUM_CONSTANT(SPAWN_WORLDSPAWN);
	BIND_ENUM_CONSTANT(SPAWN_MERGE_WORLDSPAWN);
	BIND_ENUM_CONSTANT(SPAWN_ENTITY);
	BIND_ENUM_CONSTANT(ORIGIN_AVERAGED);
	BIND_ENUM_CONSTANT(ORIGIN_ABSOLUTE);
	BIND_ENUM_CONSTANT(ORIGIN_RELATIVE);
	BIND_ENUM_CONSTANT(ORIGIN_BRUSH);
	BIND_ENUM_CONSTANT(ORIGIN_BOUNDS_CENTER);
	BIND_ENUM_CONSTANT(ORIGIN_BOUNDS_MINS);
	BIND_ENUM_CONSTANT(ORIGIN_BOUNDS_MAXS);
	BIND_ENUM_CONSTANT(COLLISION_NONE);
	BIND_ENUM_CONSTANT(COLLISION_CONVEX);
	BIND_ENUM_CONSTANT(COLLISION_CONCAVE);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "spawn_type", PROPERTY_HINT_ENUM, "Worldspawn,Merge Worldspawn,Entity"), "set_spawn_type", "get_spawn_type");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "origin_type", PROPERTY_HINT_ENUM, "Averaged,Absolute,Relative,Brush,Bounds Center,Bounds Mins,Bounds Maxs"), "set_origin_type", "get_origin_type");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "build_visuals"), "set_build_visuals", "get_build_visuals");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "global_illumination_mode", PROPERTY_HINT_ENUM, "Disabled,Static,Dynamic"), "set_global_illumination_mode", "get_global_illumination_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "shadow_casting_setting", PROPERTY_HINT_ENUM, "Off,On,Double-Sided,Shadows Only"), "set_shadow_casting_setting", "get_shadow_casting_setting");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "build_occlusion"), "set_build_occlusion", "get_build_occlusion");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "render_layers", PROPERTY_HINT_LAYERS_3D_RENDER), "set_render_layers", "get_render_layers");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_shape_type", PROPERTY_HINT_ENUM, "None,Convex,Concave"), "set_collision_shape_type", "get_collision_shape_type");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_layer", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_layer", "get_collision_layer");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_mask", "get_collision_mask");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collision_priority"), "set_collision_priority", "get_collision_priority");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collision_shape_margin"), "set_collision_shape_margin", "get_collision_shape_margin");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "add_textures_metadata"), "set_add_textures_metadata", "get_add_textures_metadata");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "add_vertex_metadata"), "set_add_vertex_metadata", "get_add_vertex_metadata");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "add_face_position_metadata"), "set_add_face_position_metadata", "get_add_face_position_metadata");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "add_face_normal_metadata"), "set_add_face_normal_metadata", "get_add_face_normal_metadata");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "add_collision_shape_to_face_indices_metadata"), "set_add_collision_shape_to_face_indices_metadata", "get_add_collision_shape_to_face_indices_metadata");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "script_class", PROPERTY_HINT_RESOURCE_TYPE, "Script"), "set_script_class", "get_script_class");
}

BlaziumFGDSolidClass::BlaziumFGDSolidClass() {
	prefix = "@SolidClass";
}
