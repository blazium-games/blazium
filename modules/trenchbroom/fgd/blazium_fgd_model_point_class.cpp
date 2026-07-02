/**************************************************************************/
/*  blazium_fgd_model_point_class.cpp                                     */
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

#include "blazium_fgd_model_point_class.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/error/error_macros.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/class_db.h"
#include "core/string/print_string.h"
#include "modules/gltf/gltf_document.h"
#include "modules/gltf/gltf_state.h"
#include "modules/gltf/structures/gltf_mesh.h"
#include "modules/gltf/structures/gltf_node.h"
#include "modules/trenchbroom/trenchbroom_local_config.h"
#include "scene/3d/node_3d.h"
#include "scene/resources/packed_scene.h"

void BlaziumFGDModelPointClass::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_target_map_editor", "target_map_editor"), &BlaziumFGDModelPointClass::set_target_map_editor);
	ClassDB::bind_method(D_METHOD("get_target_map_editor"), &BlaziumFGDModelPointClass::get_target_map_editor);
	ClassDB::bind_method(D_METHOD("set_models_sub_folder", "models_sub_folder"), &BlaziumFGDModelPointClass::set_models_sub_folder);
	ClassDB::bind_method(D_METHOD("get_models_sub_folder"), &BlaziumFGDModelPointClass::get_models_sub_folder);
	ClassDB::bind_method(D_METHOD("set_scale_expression", "scale_expression"), &BlaziumFGDModelPointClass::set_scale_expression);
	ClassDB::bind_method(D_METHOD("get_scale_expression"), &BlaziumFGDModelPointClass::get_scale_expression);
	ClassDB::bind_method(D_METHOD("set_generate_size_property", "generate_size_property"), &BlaziumFGDModelPointClass::set_generate_size_property);
	ClassDB::bind_method(D_METHOD("get_generate_size_property"), &BlaziumFGDModelPointClass::get_generate_size_property);
	ClassDB::bind_method(D_METHOD("set_rotation_offset", "rotation_offset"), &BlaziumFGDModelPointClass::set_rotation_offset);
	ClassDB::bind_method(D_METHOD("get_rotation_offset"), &BlaziumFGDModelPointClass::get_rotation_offset);
	ClassDB::bind_method(D_METHOD("set_model_generation_enabled", "model_generation_enabled"), &BlaziumFGDModelPointClass::set_model_generation_enabled);
	ClassDB::bind_method(D_METHOD("get_model_generation_enabled"), &BlaziumFGDModelPointClass::get_model_generation_enabled);
	ClassDB::bind_method(D_METHOD("generate_gd_ignore_file"), &BlaziumFGDModelPointClass::generate_gd_ignore_file);
	ClassDB::bind_method(D_METHOD("_get_generate_gd_ignore_file_func"), &BlaziumFGDModelPointClass::_get_generate_gd_ignore_file_func);
	ClassDB::bind_method(D_METHOD("_save_gltf_deferred", "gltf_document", "gltf_state", "path"), &BlaziumFGDModelPointClass::_save_gltf_deferred);

	BIND_ENUM_CONSTANT(EDITOR_GENERIC);
	BIND_ENUM_CONSTANT(EDITOR_TRENCHBROOM);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "target_map_editor", PROPERTY_HINT_ENUM, "Generic,Trenchbroom"), "set_target_map_editor", "get_target_map_editor");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "models_sub_folder", PROPERTY_HINT_DIR), "set_models_sub_folder", "get_models_sub_folder");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "scale_expression"), "set_scale_expression", "get_scale_expression");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "generate_size_property"), "set_generate_size_property", "get_generate_size_property");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "rotation_offset"), "set_rotation_offset", "get_rotation_offset");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "model_generation_enabled"), "set_model_generation_enabled", "get_model_generation_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::CALLABLE, "_generate_gd_ignore_file_func", PROPERTY_HINT_TOOL_BUTTON, "Generate GD Ignore File,FileAccess", PROPERTY_USAGE_EDITOR), "", "_get_generate_gd_ignore_file_func");
}

Callable BlaziumFGDModelPointClass::_get_generate_gd_ignore_file_func() const {
	return callable_mp(const_cast<BlaziumFGDModelPointClass *>(this), &BlaziumFGDModelPointClass::generate_gd_ignore_file);
}

static Vector3 _parse_scale_expression_vector(const String &p_scale_expression, real_t p_default_inverse_scale) {
	Vector3 scale_factor = Vector3(1, 1, 1);
	if (p_scale_expression.is_empty()) {
		scale_factor *= p_default_inverse_scale;
		return scale_factor;
	}
	if (p_scale_expression.begins_with("'")) {
		PackedFloat64Array scale_arr = p_scale_expression.split_floats(" ", false);
		if (scale_arr.size() == 3) {
			scale_factor *= Vector3(scale_arr[0], scale_arr[1], scale_arr[2]);
		}
	} else {
		const real_t scalar = p_scale_expression.to_float();
		if (scalar > 0.0) {
			scale_factor *= scalar;
		}
	}
	if (scale_factor.length_squared() == 0.0) {
		scale_factor = Vector3(1, 1, 1);
	}
	return scale_factor;
}

void BlaziumFGDModelPointClass::generate_gd_ignore_file() {
	if (!Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	const String path = _get_game_path().path_join(_get_model_folder());
	if (DirAccess::make_dir_recursive_absolute(path) != OK) {
		ERR_PRINT("Failed creating dir for GDIgnore file");
		return;
	}

	const String ignore_path = path.path_join(".gdignore");
	if (FileAccess::exists(ignore_path)) {
		return;
	}

	Ref<FileAccess> file = FileAccess::open(ignore_path, FileAccess::WRITE);
	if (file.is_null()) {
		ERR_PRINT("Failed creating .gdignore file");
		return;
	}
	file->store_string("");
}

String BlaziumFGDModelPointClass::build_def_text(BlaziumFGDFile::TargetMapEditor p_target_editor) const {
	BlaziumFGDModelPointClass *mutable_this = const_cast<BlaziumFGDModelPointClass *>(this);
	if (model_generation_enabled) {
		const TargetMapEditor previous_editor = target_map_editor;
		mutable_this->target_map_editor = BlaziumFGDFile::target_uses_trenchbroom_extensions(p_target_editor)
				? EDITOR_TRENCHBROOM
				: EDITOR_GENERIC;
		mutable_this->_generate_model();
		mutable_this->model_generation_enabled = false;
		mutable_this->target_map_editor = previous_editor;
	}
	return BlaziumFGDPointClass::build_def_text(p_target_editor);
}

void BlaziumFGDModelPointClass::_generate_model() {
	if (scene_file.is_null()) {
		return;
	}

	Ref<GLTFState> gltf_state;
	gltf_state.instantiate();
	const String path = _get_export_dir();
	Node3D *node = _get_node();
	if (node == nullptr) {
		return;
	}
	if (!_create_gltf_file(gltf_state, path, node)) {
		ERR_PRINT("could not create gltf file");
		memdelete(node);
		return;
	}
	memdelete(node);

	const real_t default_inverse_scale = GLOBAL_GET("blazium/trenchbroom/default_inverse_scale_factor");
	if (target_map_editor == EDITOR_TRENCHBROOM) {
		if (scale_expression.is_empty()) {
			meta_properties["model"] = vformat("{\"path\": \"%s\", \"scale\": %s }", _get_local_path(), String::num(default_inverse_scale));
		} else {
			meta_properties["model"] = vformat("{\"path\": \"%s\", \"scale\": %s }", _get_local_path(), scale_expression);
		}
	} else {
		meta_properties["studio"] = vformat("\"%s\"", _get_local_path());
	}

	if (generate_size_property) {
		meta_properties["size"] = _generate_size_from_aabb(gltf_state->get_meshes(), gltf_state->get_nodes());
	}
}

Node3D *BlaziumFGDModelPointClass::_get_node() {
	if (scene_file.is_null()) {
		return nullptr;
	}

	Node *node = scene_file->instantiate();
	Node3D *node_3d = Object::cast_to<Node3D>(node);
	if (node_3d == nullptr) {
		ERR_PRINT("Scene is not of type 'Node3D'");
		if (node) {
			memdelete(node);
		}
		return nullptr;
	}
	return node_3d;
}

String BlaziumFGDModelPointClass::_get_export_dir() const {
	return _get_game_path().path_join(_get_model_folder()).path_join(classname + ".glb");
}

String BlaziumFGDModelPointClass::_get_local_path() const {
	return _get_model_folder().path_join(classname + ".glb");
}

String BlaziumFGDModelPointClass::_get_model_folder() const {
	String model_dir = GLOBAL_GET("blazium/trenchbroom/model_point_class_save_path");
	if (!models_sub_folder.is_empty()) {
		model_dir = model_dir.path_join(models_sub_folder);
	}
	return model_dir;
}

String BlaziumFGDModelPointClass::_get_game_path() const {
	return TrenchbroomLocalConfig::get_setting(TrenchbroomLocalConfig::PROPERTY_MAP_EDITOR_GAME_PATH);
}

bool BlaziumFGDModelPointClass::_create_gltf_file(Ref<GLTFState> p_gltf_state, const String &p_path, Node3D *p_node) {
	Ref<GLTFDocument> gltf_document;
	gltf_document.instantiate();
	p_gltf_state->set_create_animations(false);

	p_node->rotate_x(Math::deg_to_rad(rotation_offset.x));
	p_node->rotate_y(Math::deg_to_rad(rotation_offset.y));
	p_node->rotate_z(Math::deg_to_rad(rotation_offset.z));

	if (target_map_editor != EDITOR_TRENCHBROOM) {
		const real_t default_inverse_scale = GLOBAL_GET("blazium/trenchbroom/default_inverse_scale_factor");
		const Vector3 scale_factor = _parse_scale_expression_vector(scale_expression, default_inverse_scale);
		p_node->set_scale(p_node->get_scale() * scale_factor);
	}

	const Error error = gltf_document->append_from_scene(p_node, p_gltf_state);
	if (error != OK) {
		ERR_PRINT(vformat("Failed appending to gltf document: %s", error));
		return false;
	}

	if (Engine::get_singleton()->is_editor_hint()) {
		call_deferred("_save_gltf_deferred", gltf_document, p_gltf_state, p_path);
	} else {
		_save_to_file_system(gltf_document, p_gltf_state, p_path);
	}
	return true;
}

void BlaziumFGDModelPointClass::_save_gltf_deferred(Ref<GLTFDocument> p_gltf_document, Ref<GLTFState> p_gltf_state, const String &p_path) {
	_save_to_file_system(p_gltf_document, p_gltf_state, p_path);
}

void BlaziumFGDModelPointClass::_save_to_file_system(Ref<GLTFDocument> p_gltf_document, Ref<GLTFState> p_gltf_state, const String &p_path) {
	if (DirAccess::make_dir_recursive_absolute(p_path.get_base_dir()) != OK) {
		ERR_PRINT("Failed creating dir");
		return;
	}

	const Error error = p_gltf_document->write_to_filesystem(p_gltf_state, p_path);
	if (error != OK) {
		ERR_PRINT(vformat("Failed writing to file system: %s", error));
		return;
	}
	print_line("Exported model to " + p_path);
}

AABB BlaziumFGDModelPointClass::_generate_size_from_aabb(const TypedArray<GLTFMesh> &p_meshes, const TypedArray<GLTFNode> &p_nodes) const {
	AABB aabb;
	for (int i = 0; i < p_meshes.size(); i++) {
		Ref<GLTFMesh> mesh = p_meshes[i];
		if (mesh.is_null() || mesh->get_mesh().is_null()) {
			continue;
		}
		Ref<ArrayMesh> array_mesh = mesh->get_mesh()->get_mesh();
		if (array_mesh.is_valid()) {
			aabb = aabb.merge(array_mesh->get_aabb());
		}
	}

	Vector3 pos_ofs;
	if (!p_nodes.is_empty()) {
		int count = 0;
		for (int i = 0; i < p_nodes.size(); i++) {
			Ref<GLTFNode> node = p_nodes[i];
			if (node.is_valid() && node->get_parent() == 0) {
				pos_ofs += node->get_position();
				count++;
			}
		}
		pos_ofs /= MAX(count, 1);
	}
	aabb.position += pos_ofs;

	AABB size_prop;
	size_prop.position = Vector3(aabb.position.z, aabb.position.x, aabb.position.y);
	size_prop.size = Vector3(aabb.size.z, aabb.size.x, aabb.size.y);

	const real_t default_inverse_scale = GLOBAL_GET("blazium/trenchbroom/default_inverse_scale_factor");
	Vector3 scale_factor = Vector3(1, 1, 1);
	if (target_map_editor == EDITOR_TRENCHBROOM) {
		scale_factor = _parse_scale_expression_vector(scale_expression, default_inverse_scale);
	}

	size_prop.position *= scale_factor;
	size_prop.size *= scale_factor;
	size_prop.size += size_prop.position;
	for (int i = 0; i < 3; i++) {
		size_prop.position[i] = Math::round(size_prop.position[i]);
		size_prop.size[i] = Math::round(size_prop.size[i]);
	}
	return size_prop;
}
