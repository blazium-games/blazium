/**************************************************************************/
/*  blazium_fgd_point_class.h                                             */
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
#include "blazium_fgd_point_class_display_descriptor.h"

#include "scene/resources/packed_scene.h"

class BlaziumFGDPointClass : public BlaziumFGDEntityClass {
	GDCLASS(BlaziumFGDPointClass, BlaziumFGDEntityClass);

protected:
	static void _bind_methods();

	Ref<PackedScene> scene_file;
	Ref<Script> script_class;
	bool apply_rotation_on_map_build = true;
	bool apply_scale_on_map_build = true;
	TypedArray<BlaziumFGDPointClassDisplayDescriptor> display_descriptors;

	String _build_model_branch_text(const Ref<BlaziumFGDPointClassDisplayDescriptor> &p_descriptor) const;
	String _build_model_text() const;
	String _build_studio_text() const;

public:
	BlaziumFGDPointClass();

	void set_scene_file(const Ref<PackedScene> &p_scene) { scene_file = p_scene; }
	Ref<PackedScene> get_scene_file() const { return scene_file; }

	void set_script_class(const Ref<Script> &p_script) { script_class = p_script; }
	Ref<Script> get_script_class() const { return script_class; }

	void set_apply_rotation_on_map_build(bool p_apply) { apply_rotation_on_map_build = p_apply; }
	bool get_apply_rotation_on_map_build() const { return apply_rotation_on_map_build; }

	void set_apply_scale_on_map_build(bool p_apply) { apply_scale_on_map_build = p_apply; }
	bool get_apply_scale_on_map_build() const { return apply_scale_on_map_build; }

	void set_display_descriptors(const TypedArray<BlaziumFGDPointClassDisplayDescriptor> &p_descriptors) { display_descriptors = p_descriptors; }
	TypedArray<BlaziumFGDPointClassDisplayDescriptor> get_display_descriptors() const { return display_descriptors; }

	virtual String build_def_text(BlaziumFGDFile::TargetMapEditor p_target_editor) const override;
};
