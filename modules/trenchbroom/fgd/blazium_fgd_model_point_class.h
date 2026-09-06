/**************************************************************************/
/*  blazium_fgd_model_point_class.h                                       */
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

#include "blazium_fgd_point_class.h"

#include "modules/gltf/structures/gltf_mesh.h"
#include "modules/gltf/structures/gltf_node.h"
#include "scene/3d/node_3d.h"

class GLTFDocument;
class GLTFState;

class BlaziumFGDModelPointClass : public BlaziumFGDPointClass {
	GDCLASS(BlaziumFGDModelPointClass, BlaziumFGDPointClass);

public:
	enum TargetMapEditor {
		EDITOR_GENERIC,
		EDITOR_TRENCHBROOM,
	};

protected:
	static void _bind_methods();

	TargetMapEditor target_map_editor = EDITOR_GENERIC;
	String models_sub_folder;
	String scale_expression;
	bool generate_size_property = false;
	Vector3 rotation_offset;
	bool model_generation_enabled = false;

	void _generate_model();
	Node3D *_get_node();
	String _get_export_dir() const;
	String _get_local_path() const;
	String _get_model_folder() const;
	String _get_game_path() const;
	bool _create_gltf_file(Ref<GLTFState> p_gltf_state, const String &p_path, Node3D *p_node);
	void _save_to_file_system(Ref<GLTFDocument> p_gltf_document, Ref<GLTFState> p_gltf_state, const String &p_path);
	void _save_gltf_deferred(Ref<GLTFDocument> p_gltf_document, Ref<GLTFState> p_gltf_state, const String &p_path);
	AABB _generate_size_from_aabb(const TypedArray<GLTFMesh> &p_meshes, const TypedArray<GLTFNode> &p_nodes) const;

public:
	void set_target_map_editor(TargetMapEditor p_editor) { target_map_editor = p_editor; }
	TargetMapEditor get_target_map_editor() const { return target_map_editor; }

	void set_models_sub_folder(const String &p_folder) { models_sub_folder = p_folder; }
	String get_models_sub_folder() const { return models_sub_folder; }

	void set_scale_expression(const String &p_expression) { scale_expression = p_expression; }
	String get_scale_expression() const { return scale_expression; }

	void set_generate_size_property(bool p_generate) { generate_size_property = p_generate; }
	bool get_generate_size_property() const { return generate_size_property; }

	void set_rotation_offset(const Vector3 &p_offset) { rotation_offset = p_offset; }
	Vector3 get_rotation_offset() const { return rotation_offset; }

	void set_model_generation_enabled(bool p_enabled) { model_generation_enabled = p_enabled; }
	bool get_model_generation_enabled() const { return model_generation_enabled; }

	void generate_gd_ignore_file();
	Callable _get_generate_gd_ignore_file_func() const;

	virtual String build_def_text(BlaziumFGDFile::TargetMapEditor p_target_editor) const override;
};

VARIANT_ENUM_CAST(BlaziumFGDModelPointClass::TargetMapEditor);
