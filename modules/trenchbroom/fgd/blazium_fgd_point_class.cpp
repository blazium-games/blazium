/**************************************************************************/
/*  blazium_fgd_point_class.cpp                                           */
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

#include "blazium_fgd_point_class.h"

#include "blazium_fgd_point_class_display_descriptor.h"
#include "core/object/class_db.h"
#include "core/string/print_string.h"

void BlaziumFGDPointClass::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_scene_file", "scene_file"), &BlaziumFGDPointClass::set_scene_file);
	ClassDB::bind_method(D_METHOD("get_scene_file"), &BlaziumFGDPointClass::get_scene_file);
	ClassDB::bind_method(D_METHOD("set_script_class", "script_class"), &BlaziumFGDPointClass::set_script_class);
	ClassDB::bind_method(D_METHOD("get_script_class"), &BlaziumFGDPointClass::get_script_class);
	ClassDB::bind_method(D_METHOD("set_apply_rotation_on_map_build", "apply_rotation_on_map_build"), &BlaziumFGDPointClass::set_apply_rotation_on_map_build);
	ClassDB::bind_method(D_METHOD("get_apply_rotation_on_map_build"), &BlaziumFGDPointClass::get_apply_rotation_on_map_build);
	ClassDB::bind_method(D_METHOD("set_apply_scale_on_map_build", "apply_scale_on_map_build"), &BlaziumFGDPointClass::set_apply_scale_on_map_build);
	ClassDB::bind_method(D_METHOD("get_apply_scale_on_map_build"), &BlaziumFGDPointClass::get_apply_scale_on_map_build);
	ClassDB::bind_method(D_METHOD("set_display_descriptors", "display_descriptors"), &BlaziumFGDPointClass::set_display_descriptors);
	ClassDB::bind_method(D_METHOD("get_display_descriptors"), &BlaziumFGDPointClass::get_display_descriptors);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "scene_file", PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"), "set_scene_file", "get_scene_file");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "script_class", PROPERTY_HINT_RESOURCE_TYPE, "Script"), "set_script_class", "get_script_class");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "apply_rotation_on_map_build"), "set_apply_rotation_on_map_build", "get_apply_rotation_on_map_build");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "apply_scale_on_map_build"), "set_apply_scale_on_map_build", "get_apply_scale_on_map_build");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "display_descriptors", PROPERTY_HINT_ARRAY_TYPE, "BlaziumFGDPointClassDisplayDescriptor"), "set_display_descriptors", "get_display_descriptors");
}

BlaziumFGDPointClass::BlaziumFGDPointClass() {
	prefix = "@PointClass";
}

String BlaziumFGDPointClass::_build_model_branch_text(const Ref<BlaziumFGDPointClassDisplayDescriptor> &p_descriptor) const {
	if (!p_descriptor.is_valid()) {
		return String();
	}
	if (p_descriptor->get_scale().is_empty() && p_descriptor->get_skin().is_empty() && p_descriptor->get_frame().is_empty()) {
		return p_descriptor->get_display_asset_path();
	}
	String model_string = vformat("{ \"path\": %s", p_descriptor->get_display_asset_path());
	if (!p_descriptor->get_skin().is_empty()) {
		model_string += vformat(", \"skin\": %s", p_descriptor->get_skin());
	}
	if (!p_descriptor->get_frame().is_empty()) {
		model_string += vformat(", \"frame\": %s", p_descriptor->get_frame());
	}
	if (!p_descriptor->get_scale().is_empty()) {
		model_string += vformat(", \"scale\": %s", p_descriptor->get_scale());
	}
	model_string += " }";
	return model_string;
}

String BlaziumFGDPointClass::_build_model_text() const {
	if (display_descriptors.is_empty()) {
		return String();
	}
	if (display_descriptors.size() == 1) {
		Ref<BlaziumFGDPointClassDisplayDescriptor> descriptor = display_descriptors[0];
		return _build_model_branch_text(descriptor);
	}

	String model_string = "{{";
	Ref<BlaziumFGDPointClassDisplayDescriptor> default_display;
	for (int i = 0; i < display_descriptors.size(); i++) {
		Ref<BlaziumFGDPointClassDisplayDescriptor> descriptor = display_descriptors[i];
		if (descriptor->get_conditional().is_empty()) {
			if (!default_display.is_valid()) {
				default_display = descriptor;
			} else {
				ERR_PRINT(classname + " has a Point Class Display Descriptor without required conditionals set.");
			}
			continue;
		}
		model_string += vformat("%s -> %s, ", descriptor->get_conditional(), _build_model_branch_text(descriptor));
	}

	if (default_display.is_valid()) {
		model_string += _build_model_branch_text(default_display) + " }}";
	} else {
		if (model_string.ends_with(", ")) {
			model_string = model_string.substr(0, model_string.length() - 2);
		}
		model_string += " }}";
	}
	return model_string;
}

String BlaziumFGDPointClass::_build_studio_text() const {
	for (int i = 0; i < display_descriptors.size(); i++) {
		Ref<BlaziumFGDPointClassDisplayDescriptor> descriptor = display_descriptors[i];
		if (descriptor->get_display_asset_path().find("\"") != -1) {
			return descriptor->get_display_asset_path();
		}
		ERR_PRINT(classname + " attempting to set an invalid value to @studio format during FGD export.");
	}
	return String();
}

String BlaziumFGDPointClass::build_def_text(BlaziumFGDFile::TargetMapEditor p_target_editor) const {
	BlaziumFGDPointClass *mutable_this = const_cast<BlaziumFGDPointClass *>(this);
	if (!display_descriptors.is_empty()) {
		if (BlaziumFGDFile::target_uses_trenchbroom_extensions(p_target_editor)) {
			String display_string = _build_model_text();
			if (!display_string.is_empty()) {
				mutable_this->meta_properties["model"] = display_string;
			}
		} else {
			String display_string = _build_studio_text();
			if (!display_string.is_empty()) {
				mutable_this->meta_properties["studio"] = display_string;
			}
		}
	}
	return BlaziumFGDEntityClass::build_def_text(p_target_editor);
}
