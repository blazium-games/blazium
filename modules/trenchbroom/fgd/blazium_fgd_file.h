/**************************************************************************/
/*  blazium_fgd_file.h                                                    */
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

#include "core/io/resource.h"

class BlaziumFGDEntityClass;

class BlaziumFGDFile : public Resource {
	GDCLASS(BlaziumFGDFile, Resource);

public:
	enum TargetMapEditor {
		EDITOR_OTHER,
		EDITOR_TRENCHBROOM,
		EDITOR_JACK,
		EDITOR_NET_RADIANT_CUSTOM,
	};

private:
	TargetMapEditor target_map_editor = EDITOR_TRENCHBROOM;
	String fgd_name = "Blazium";
	Array base_fgd_files;
	Array entity_definitions;
	bool generate_model_point_class_models = true;

protected:
	static void _bind_methods();

public:
	void set_target_map_editor(TargetMapEditor p_editor) { target_map_editor = p_editor; }
	TargetMapEditor get_target_map_editor() const { return target_map_editor; }

	void set_fgd_name(const String &p_name) { fgd_name = p_name; }
	String get_fgd_name() const { return fgd_name; }

	void set_base_fgd_files(const Array &p_files) { base_fgd_files = p_files; }
	Array get_base_fgd_files() const { return base_fgd_files; }

	void set_entity_definitions(const Array &p_defs) { entity_definitions = p_defs; }
	Array get_entity_definitions_array() const { return entity_definitions; }

	void set_generate_model_point_class_models(bool p_generate) { generate_model_point_class_models = p_generate; }
	bool get_generate_model_point_class_models() const { return generate_model_point_class_models; }

	void export_fgd();
	Error do_export_file(TargetMapEditor p_target_editor = EDITOR_TRENCHBROOM, const String &p_fgd_output_folder = String());

	static bool target_uses_trenchbroom_extensions(TargetMapEditor p_target_editor);
	static bool target_uses_jack_shader_properties(TargetMapEditor p_target_editor);

	String build_class_text(TargetMapEditor p_target_editor = EDITOR_TRENCHBROOM) const;
	Array get_fgd_classes() const;
	Dictionary get_entity_definitions() const;

private:
	Array _generate_base_class_list(const Ref<BlaziumFGDEntityClass> &p_entity_def, Vector<String> p_visited = Vector<String>()) const;
	Callable _get_export_fgd_func() const;
};

VARIANT_ENUM_CAST(BlaziumFGDFile::TargetMapEditor);
