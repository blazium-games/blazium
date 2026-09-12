/**************************************************************************/
/*  blazium_fgd_file.cpp                                                  */
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

#include "blazium_fgd_file.h"

#include "blazium_fgd_entity_class.h"
#include "blazium_fgd_model_point_class.h"
#include "blazium_fgd_point_class.h"
#include "blazium_fgd_solid_class.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/class_db.h"
#include "core/string/print_string.h"
#include "modules/trenchbroom/trenchbroom_local_config.h"

void BlaziumFGDFile::_bind_methods() {
	ClassDB::bind_method(D_METHOD("export_fgd"), &BlaziumFGDFile::export_fgd);
	ClassDB::bind_method(D_METHOD("_get_export_fgd_func"), &BlaziumFGDFile::_get_export_fgd_func);
	ClassDB::bind_method(D_METHOD("do_export_file", "target_editor", "fgd_output_folder"), &BlaziumFGDFile::do_export_file, DEFVAL(EDITOR_TRENCHBROOM), DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("build_class_text", "target_editor"), &BlaziumFGDFile::build_class_text, DEFVAL(EDITOR_TRENCHBROOM));
	ClassDB::bind_method(D_METHOD("get_fgd_classes"), &BlaziumFGDFile::get_fgd_classes);
	ClassDB::bind_method(D_METHOD("get_entity_definitions"), &BlaziumFGDFile::get_entity_definitions);
	ClassDB::bind_method(D_METHOD("set_target_map_editor", "target_map_editor"), &BlaziumFGDFile::set_target_map_editor);
	ClassDB::bind_method(D_METHOD("get_target_map_editor"), &BlaziumFGDFile::get_target_map_editor);
	ClassDB::bind_method(D_METHOD("set_fgd_name", "fgd_name"), &BlaziumFGDFile::set_fgd_name);
	ClassDB::bind_method(D_METHOD("get_fgd_name"), &BlaziumFGDFile::get_fgd_name);
	ClassDB::bind_method(D_METHOD("set_base_fgd_files", "base_fgd_files"), &BlaziumFGDFile::set_base_fgd_files);
	ClassDB::bind_method(D_METHOD("get_base_fgd_files"), &BlaziumFGDFile::get_base_fgd_files);
	ClassDB::bind_method(D_METHOD("set_entity_definitions", "entity_definitions"), &BlaziumFGDFile::set_entity_definitions);
	ClassDB::bind_method(D_METHOD("get_entity_definitions_array"), &BlaziumFGDFile::get_entity_definitions_array);
	ClassDB::bind_method(D_METHOD("set_generate_model_point_class_models", "generate_model_point_class_models"), &BlaziumFGDFile::set_generate_model_point_class_models);
	ClassDB::bind_method(D_METHOD("get_generate_model_point_class_models"), &BlaziumFGDFile::get_generate_model_point_class_models);

	BIND_ENUM_CONSTANT(EDITOR_OTHER);
	BIND_ENUM_CONSTANT(EDITOR_TRENCHBROOM);
	BIND_ENUM_CONSTANT(EDITOR_JACK);
	BIND_ENUM_CONSTANT(EDITOR_NET_RADIANT_CUSTOM);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "target_map_editor", PROPERTY_HINT_ENUM, "Other,Trenchbroom,Jack,NetRadiant Custom"), "set_target_map_editor", "get_target_map_editor");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "fgd_name"), "set_fgd_name", "get_fgd_name");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "base_fgd_files", PROPERTY_HINT_ARRAY_TYPE, "BlaziumFGDFile"), "set_base_fgd_files", "get_base_fgd_files");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "entity_definitions", PROPERTY_HINT_ARRAY_TYPE, "BlaziumFGDEntityClass"), "set_entity_definitions", "get_entity_definitions_array");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "generate_model_point_class_models"), "set_generate_model_point_class_models", "get_generate_model_point_class_models");
	ADD_PROPERTY(PropertyInfo(Variant::CALLABLE, "_export_fgd_func", PROPERTY_HINT_TOOL_BUTTON, "Export FGD", PROPERTY_USAGE_EDITOR), "", "_get_export_fgd_func");
}

Callable BlaziumFGDFile::_get_export_fgd_func() const {
	return callable_mp(const_cast<BlaziumFGDFile *>(this), &BlaziumFGDFile::export_fgd);
}

void BlaziumFGDFile::export_fgd() {
	do_export_file(target_map_editor);
}

bool BlaziumFGDFile::target_uses_trenchbroom_extensions(TargetMapEditor p_target_editor) {
	return p_target_editor == EDITOR_TRENCHBROOM;
}

bool BlaziumFGDFile::target_uses_jack_shader_properties(TargetMapEditor p_target_editor) {
	return p_target_editor == EDITOR_JACK;
}

Error BlaziumFGDFile::do_export_file(TargetMapEditor p_target_editor, const String &p_fgd_output_folder) {
	String output_folder = p_fgd_output_folder;
	if (output_folder.is_empty()) {
		output_folder = TrenchbroomLocalConfig::get_setting(TrenchbroomLocalConfig::PROPERTY_FGD_OUTPUT_FOLDER);
	}
	if (output_folder.is_empty()) {
		ERR_PRINT("Skipping export: No FGD output folder");
		return ERR_DOES_NOT_EXIST;
	}
	if (fgd_name.is_empty()) {
		ERR_PRINT("Skipping export: Empty FGD name");
		return ERR_INVALID_PARAMETER;
	}
	if (!DirAccess::dir_exists_absolute(output_folder)) {
		if (DirAccess::make_dir_recursive_absolute(output_folder) != OK) {
			ERR_PRINT("Skipping export: Failed to create directory");
			return ERR_CANT_CREATE;
		}
	}

	String content = build_class_text(p_target_editor);
	if (content.is_empty()) {
		return ERR_INVALID_DATA;
	}

	String fgd_file = output_folder.path_join(fgd_name + ".fgd");
	Ref<FileAccess> file = FileAccess::open(fgd_file, FileAccess::WRITE);
	if (file.is_null()) {
		ERR_PRINT("Failed to open file for writing: " + fgd_file);
		return ERR_FILE_CANT_OPEN;
	}
	print_line("Exporting FGD to " + fgd_file);
	file->store_string(content);
	return OK;
}

String BlaziumFGDFile::build_class_text(TargetMapEditor p_target_editor) const {
	String res;
	for (int base_index = 0; base_index < base_fgd_files.size(); base_index++) {
		Ref<BlaziumFGDFile> base_fgd = base_fgd_files[base_index];
		if (base_fgd.is_valid()) {
			res += base_fgd->build_class_text(p_target_editor);
		} else {
			ERR_PRINT(vformat("FGD base files array contains invalid type at position %s", base_index));
		}
	}

	Array entities = get_fgd_classes();
	HashMap<String, int> classnames;
	bool failure = false;

	for (int ent_index = 0; ent_index < entities.size(); ent_index++) {
		Ref<BlaziumFGDEntityClass> ent = entities[ent_index];
		if (!ent.is_valid()) {
			continue;
		}
		if (ent->get_blazium_internal()) {
			continue;
		}
		Ref<BlaziumFGDModelPointClass> model_point = ent;
		if (model_point.is_valid()) {
			const_cast<BlaziumFGDModelPointClass *>(model_point.ptr())->set_model_generation_enabled(generate_model_point_class_models);
		}
		if (ent->get_classname().is_empty()) {
			ERR_PRINT(vformat("FGD class cannot be exported with empty classname (in position %s)", ent_index));
			failure = true;
			continue;
		}
		if (classnames.has(ent->get_classname())) {
			ERR_PRINT(vformat("Duplicate class name found: %s", ent->get_classname()));
			failure = true;
			continue;
		}
		classnames[ent->get_classname()] = ent_index;
		res += ent->build_def_text(p_target_editor);
		if (ent_index < entities.size() - 1) {
			res += "\n";
		}
	}

	if (failure) {
		return String();
	}
	return res;
}

Array BlaziumFGDFile::get_fgd_classes() const {
	Array res;
	for (int i = 0; i < entity_definitions.size(); i++) {
		Ref<BlaziumFGDEntityClass> cur_ent_def = entity_definitions[i];
		if (cur_ent_def.is_valid()) {
			res.push_back(cur_ent_def);
		}
	}
	return res;
}

Dictionary BlaziumFGDFile::get_entity_definitions() const {
	Dictionary res;
	for (int i = 0; i < base_fgd_files.size(); i++) {
		Ref<BlaziumFGDFile> base_fgd = base_fgd_files[i];
		if (base_fgd.is_valid()) {
			Dictionary base_defs = base_fgd->get_entity_definitions();
			Array keys = base_defs.keys();
			for (int k = 0; k < keys.size(); k++) {
				res[keys[k]] = base_defs[keys[k]];
			}
		}
	}

	Array classes = get_fgd_classes();
	for (int i = 0; i < classes.size(); i++) {
		Ref<BlaziumFGDEntityClass> ent = classes[i];
		if (ent->get_classname().strip_edges().is_empty()) {
			ERR_PRINT("Skipping entity with empty classname");
			continue;
		}
		Ref<BlaziumFGDPointClass> point = ent;
		Ref<BlaziumFGDSolidClass> solid = ent;
		if (!point.is_valid() && !solid.is_valid()) {
			continue;
		}

		Ref<BlaziumFGDEntityClass> entity_def = ent->duplicate();
		Dictionary meta_properties;
		Dictionary class_properties;
		Dictionary class_property_descriptions;

		Array base_list = _generate_base_class_list(entity_def);
		for (int b = 0; b < base_list.size(); b++) {
			Ref<BlaziumFGDEntityClass> base_class = base_list[b];
			Array meta_keys = base_class->get_meta_properties().keys();
			for (int mk = 0; mk < meta_keys.size(); mk++) {
				meta_properties[meta_keys[mk]] = base_class->get_meta_properties()[meta_keys[mk]];
			}
			Array prop_keys = base_class->get_class_properties().keys();
			for (int pk = 0; pk < prop_keys.size(); pk++) {
				class_properties[prop_keys[pk]] = base_class->get_class_properties()[prop_keys[pk]];
			}
			Array desc_keys = base_class->get_class_property_descriptions().keys();
			for (int dk = 0; dk < desc_keys.size(); dk++) {
				class_property_descriptions[desc_keys[dk]] = base_class->get_class_property_descriptions()[desc_keys[dk]];
			}
		}

		Array entity_meta_keys = entity_def->get_meta_properties().keys();
		for (int mk = 0; mk < entity_meta_keys.size(); mk++) {
			meta_properties[entity_meta_keys[mk]] = entity_def->get_meta_properties()[entity_meta_keys[mk]];
		}
		Array entity_prop_keys = entity_def->get_class_properties().keys();
		for (int pk = 0; pk < entity_prop_keys.size(); pk++) {
			class_properties[entity_prop_keys[pk]] = entity_def->get_class_properties()[entity_prop_keys[pk]];
		}
		Array entity_desc_keys = entity_def->get_class_property_descriptions().keys();
		for (int dk = 0; dk < entity_desc_keys.size(); dk++) {
			class_property_descriptions[entity_desc_keys[dk]] = entity_def->get_class_property_descriptions()[entity_desc_keys[dk]];
		}

		entity_def->set_meta_properties(meta_properties);
		entity_def->set_class_properties(class_properties);
		entity_def->set_class_property_descriptions(class_property_descriptions);
		res[ent->get_classname()] = entity_def;
	}
	return res;
}

Array BlaziumFGDFile::_generate_base_class_list(const Ref<BlaziumFGDEntityClass> &p_entity_def, Vector<String> p_visited) const {
	Array base_classes;
	p_visited.push_back(p_entity_def->get_classname());
	if (p_entity_def->get_base_classes().is_empty()) {
		return base_classes;
	}
	Array bases = p_entity_def->get_base_classes();
	for (int i = 0; i < bases.size(); i++) {
		Ref<BlaziumFGDEntityClass> base_class = bases[i];
		if (!base_class.is_valid()) {
			continue;
		}
		if (p_visited.has(base_class->get_classname())) {
			ERR_PRINT(vformat("Entity '%s' contains cycle/duplicate to Entity '%s'", p_entity_def->get_classname(), base_class->get_classname()));
			continue;
		}
		base_classes.push_back(base_class);
		Array nested = _generate_base_class_list(base_class, p_visited);
		for (int n = 0; n < nested.size(); n++) {
			base_classes.push_back(nested[n]);
		}
	}
	return base_classes;
}
