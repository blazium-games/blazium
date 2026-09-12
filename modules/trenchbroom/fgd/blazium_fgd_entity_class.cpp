/**************************************************************************/
/*  blazium_fgd_entity_class.cpp                                          */
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

#include "blazium_fgd_entity_class.h"

#include "blazium_fgd_file.h"
#include "core/io/json.h"
#include "core/object/class_db.h"
#include "core/string/print_string.h"
#include "modules/trenchbroom/util/trenchbroom_util.h"
#include "scene/resources/material.h"
#include "scene/resources/texture.h"
#include "servers/audio/audio_stream.h"

void BlaziumFGDEntityClass::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_prefix", "prefix"), &BlaziumFGDEntityClass::set_prefix);
	ClassDB::bind_method(D_METHOD("get_prefix"), &BlaziumFGDEntityClass::get_prefix);
	ClassDB::bind_method(D_METHOD("set_classname", "classname"), &BlaziumFGDEntityClass::set_classname);
	ClassDB::bind_method(D_METHOD("get_classname"), &BlaziumFGDEntityClass::get_classname);
	ClassDB::bind_method(D_METHOD("set_description", "description"), &BlaziumFGDEntityClass::set_description);
	ClassDB::bind_method(D_METHOD("get_description"), &BlaziumFGDEntityClass::get_description);
	ClassDB::bind_method(D_METHOD("set_blazium_internal", "blazium_internal"), &BlaziumFGDEntityClass::set_blazium_internal);
	ClassDB::bind_method(D_METHOD("get_blazium_internal"), &BlaziumFGDEntityClass::get_blazium_internal);
	ClassDB::bind_method(D_METHOD("set_base_classes", "base_classes"), &BlaziumFGDEntityClass::set_base_classes);
	ClassDB::bind_method(D_METHOD("get_base_classes"), &BlaziumFGDEntityClass::get_base_classes);
	ClassDB::bind_method(D_METHOD("set_class_properties", "class_properties"), &BlaziumFGDEntityClass::set_class_properties);
	ClassDB::bind_method(D_METHOD("get_class_properties"), &BlaziumFGDEntityClass::get_class_properties);
	ClassDB::bind_method(D_METHOD("set_class_property_descriptions", "class_property_descriptions"), &BlaziumFGDEntityClass::set_class_property_descriptions);
	ClassDB::bind_method(D_METHOD("get_class_property_descriptions"), &BlaziumFGDEntityClass::get_class_property_descriptions);
	ClassDB::bind_method(D_METHOD("set_auto_apply_to_matching_node_properties", "auto_apply_to_matching_node_properties"), &BlaziumFGDEntityClass::set_auto_apply_to_matching_node_properties);
	ClassDB::bind_method(D_METHOD("get_auto_apply_to_matching_node_properties"), &BlaziumFGDEntityClass::get_auto_apply_to_matching_node_properties);
	ClassDB::bind_method(D_METHOD("set_meta_properties", "meta_properties"), &BlaziumFGDEntityClass::set_meta_properties);
	ClassDB::bind_method(D_METHOD("get_meta_properties"), &BlaziumFGDEntityClass::get_meta_properties);
	ClassDB::bind_method(D_METHOD("set_node_class", "node_class"), &BlaziumFGDEntityClass::set_node_class);
	ClassDB::bind_method(D_METHOD("get_node_class"), &BlaziumFGDEntityClass::get_node_class);
	ClassDB::bind_method(D_METHOD("set_name_property", "name_property"), &BlaziumFGDEntityClass::set_name_property);
	ClassDB::bind_method(D_METHOD("get_name_property"), &BlaziumFGDEntityClass::get_name_property);
	ClassDB::bind_method(D_METHOD("set_node_groups", "node_groups"), &BlaziumFGDEntityClass::set_node_groups);
	ClassDB::bind_method(D_METHOD("get_node_groups"), &BlaziumFGDEntityClass::get_node_groups);
	ClassDB::bind_method(D_METHOD("set_entity_extension_script", "entity_extension_script"), &BlaziumFGDEntityClass::set_entity_extension_script);
	ClassDB::bind_method(D_METHOD("get_entity_extension_script"), &BlaziumFGDEntityClass::get_entity_extension_script);
	ClassDB::bind_method(D_METHOD("build_def_text", "target_editor"), &BlaziumFGDEntityClass::build_def_text, DEFVAL(BlaziumFGDFile::EDITOR_TRENCHBROOM));
	ClassDB::bind_method(D_METHOD("retrieve_all_class_properties", "properties"), &BlaziumFGDEntityClass::retrieve_all_class_properties, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("retrieve_all_class_property_descriptions", "descriptions"), &BlaziumFGDEntityClass::retrieve_all_class_property_descriptions, DEFVAL(Dictionary()));

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "prefix"), "set_prefix", "get_prefix");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "classname"), "set_classname", "get_classname");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "description", PROPERTY_HINT_MULTILINE_TEXT), "set_description", "get_description");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "blazium_internal"), "set_blazium_internal", "get_blazium_internal");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "base_classes", PROPERTY_HINT_ARRAY_TYPE, "BlaziumFGDEntityClass"), "set_base_classes", "get_base_classes");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "class_properties"), "set_class_properties", "get_class_properties");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "class_property_descriptions"), "set_class_property_descriptions", "get_class_property_descriptions");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_apply_to_matching_node_properties"), "set_auto_apply_to_matching_node_properties", "get_auto_apply_to_matching_node_properties");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "meta_properties"), "set_meta_properties", "get_meta_properties");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "node_class"), "set_node_class", "get_node_class");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "name_property"), "set_name_property", "get_name_property");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "node_groups"), "set_node_groups", "get_node_groups");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "entity_extension_script", PROPERTY_HINT_RESOURCE_TYPE, "Script"), "set_entity_extension_script", "get_entity_extension_script");
}

String BlaziumFGDEntityClass::build_def_text(BlaziumFGDFile::TargetMapEditor p_target_editor) const {
	String res = prefix;
	Dictionary meta_props = meta_properties.duplicate();

	if (entity_extension_script.is_valid() && entity_extension_script->can_instantiate()) {
		Object *ex = ClassDB::instantiate(entity_extension_script->get_instance_base_type());
		if (ex) {
			entity_extension_script->instance_create(ex);
			if (ex->has_method("_trenchbroom_attach_properties")) {
				ex->call("_trenchbroom_attach_properties", const_cast<BlaziumFGDEntityClass *>(this));
			} else if (ex->has_method("_func_godot_attach_properties")) {
				ex->call("_func_godot_attach_properties", const_cast<BlaziumFGDEntityClass *>(this));
			}
			memdelete(ex);
		}
	}

	String base_str;
	Array bases = base_classes;
	for (int i = 0; i < bases.size(); i++) {
		Ref<BlaziumFGDEntityClass> base_class = bases[i];
		if (!base_class.is_valid() || base_class->get_classname().is_empty()) {
			continue;
		}
		base_str += base_class->get_classname();
		if (i < bases.size() - 1) {
			base_str += ", ";
		}
	}
	if (!base_str.is_empty()) {
		meta_props["base"] = base_str;
	}

	Array meta_keys = meta_props.keys();
	for (int i = 0; i < meta_keys.size(); i++) {
		const String prop = meta_keys[i];
		if (prefix == "@SolidClass" && (prop == "size" || prop == "model")) {
			continue;
		}
		if (prop == "model" && !BlaziumFGDFile::target_uses_trenchbroom_extensions(p_target_editor)) {
			continue;
		}

		Variant value = meta_props[prop];
		res += " " + prop + "(";
		if (value.get_type() == Variant::AABB) {
			AABB aabb = value;
			res += vformat("%s %s %s, %s %s %s", aabb.position.x, aabb.position.y, aabb.position.z, aabb.size.x, aabb.size.y, aabb.size.z);
		} else if (value.get_type() == Variant::COLOR) {
			Color color = value;
			res += vformat("%d %d %d", color.get_r8(), color.get_g8(), color.get_b8());
		} else if (value.get_type() == Variant::STRING) {
			res += String(value);
		} else if (value.get_type() == Variant::DICTIONARY && BlaziumFGDFile::target_uses_trenchbroom_extensions(p_target_editor)) {
			res += JSON::stringify(value);
		}
		res += ")";
	}

	res += " = " + classname;
	if (prefix != "@BaseClass") {
		String normalized_description = description.replace("\"", "'");
		if (!normalized_description.is_empty()) {
			res += vformat(" : \"%s\" ", normalized_description);
		} else {
			res += " : \"" + classname + "\" ";
		}
	}

	if (class_properties.size() > 0) {
		res += TrenchbroomUtil::newline() + "[" + TrenchbroomUtil::newline();
	} else {
		res += "[";
	}

	Array prop_keys = class_properties.keys();
	for (int pi = 0; pi < prop_keys.size(); pi++) {
		const String prop = prop_keys[pi];
		Variant value = class_properties[prop];
		String prop_val;
		String prop_type;
		String prop_description = "\"\"";

		if (class_property_descriptions.has(prop)) {
			Variant desc = class_property_descriptions[prop];
			if (value.get_type() == Variant::DICTIONARY && desc.get_type() == Variant::ARRAY) {
				Array prop_arr = desc;
				if (prop_arr.size() > 1 && (prop_arr[1].get_type() == Variant::INT || prop_arr[1].get_type() == Variant::STRING)) {
					String value_str = prop_arr[1].get_type() == Variant::INT ? String::num_int64(prop_arr[1]) : vformat("\"%s\"", String(prop_arr[1]));
					prop_description = vformat("\"%s\" : %s", String(prop_arr[0]), value_str);
				} else {
					prop_description = "\"\" : 0";
					ERR_PRINT(vformat("%s has incorrect description format.", prop));
				}
			} else {
				prop_description = vformat("\"%s\"", String(desc));
			}
		}

		switch (value.get_type()) {
			case Variant::INT:
				prop_type = "integer";
				prop_val = String::num_int64(value);
				break;
			case Variant::FLOAT:
				prop_type = "float";
				prop_val = vformat("\"%s\"", String::num(value));
				break;
			case Variant::STRING:
				prop_type = "string";
				prop_val = vformat("\"%s\"", String(value));
				break;
			case Variant::BOOL:
				prop_type = "choices";
				prop_val = TrenchbroomUtil::newline() + "\t[" + TrenchbroomUtil::newline();
				prop_val += "\t\t0 : \"No\"" + TrenchbroomUtil::newline();
				prop_val += "\t\t1 : \"Yes\"" + TrenchbroomUtil::newline();
				prop_val += "\t]";
				break;
			case Variant::VECTOR2:
			case Variant::VECTOR2I: {
				prop_type = "string";
				Vector2 vec = value;
				prop_val = vformat("\"%s %s\"", vec.x, vec.y);
			} break;
			case Variant::VECTOR3:
			case Variant::VECTOR3I: {
				prop_type = "string";
				Vector3 vec = value;
				prop_val = vformat("\"%s %s %s\"", vec.x, vec.y, vec.z);
			} break;
			case Variant::VECTOR4:
			case Variant::VECTOR4I: {
				prop_type = "string";
				Vector4 vec = value;
				prop_val = vformat("\"%s %s %s %s\"", vec.x, vec.y, vec.z, vec.w);
			} break;
			case Variant::COLOR: {
				prop_type = "color255";
				Color color = value;
				prop_val = vformat("\"%d %d %d\"", color.get_r8(), color.get_g8(), color.get_b8());
			} break;
			case Variant::DICTIONARY: {
				prop_type = "choices";
				prop_val = TrenchbroomUtil::newline() + "\t[" + TrenchbroomUtil::newline();
				Dictionary choices = value;
				Array choice_keys = choices.keys();
				for (int ci = 0; ci < choice_keys.size(); ci++) {
					String choice = choice_keys[ci];
					Variant choice_val = choices[choice];
					if (choice_val.get_type() == Variant::STRING) {
						String choice_str = choice_val;
						if (!choice_str.begins_with("\"")) {
							choice_val = vformat("\"%s\"", choice_str);
						}
					}
					prop_val += vformat("\t\t%s : \"%s\"", String(choice_val), choice) + TrenchbroomUtil::newline();
				}
				prop_val += "\t]";
			} break;
			case Variant::ARRAY: {
				prop_type = "flags";
				prop_val = TrenchbroomUtil::newline() + "\t[" + TrenchbroomUtil::newline();
				Array flags = value;
				for (int fi = 0; fi < flags.size(); fi++) {
					Array arr_val = flags[fi];
					if (arr_val.size() >= 3) {
						prop_val += vformat("\t\t%s : \"%s\" : %s", String(arr_val[1]), String(arr_val[0]), arr_val[2] ? String("1") : String("0")) + TrenchbroomUtil::newline();
					}
				}
				prop_val += "\t]";
			} break;
			case Variant::NODE_PATH:
				prop_type = "target_destination";
				prop_val = "\"\"";
				break;
			case Variant::OBJECT: {
				Ref<Resource> resource = value;
				if (resource.is_valid()) {
					prop_val = vformat("\"%s\"", resource->get_path());
					if (Object::cast_to<Material>(resource.ptr())) {
						prop_type = BlaziumFGDFile::target_uses_jack_shader_properties(p_target_editor) ? "shader" : "material";
					} else if (Object::cast_to<Texture2D>(resource.ptr())) {
						prop_type = "decal";
					} else if (Object::cast_to<AudioStream>(resource.ptr())) {
						prop_type = "sound";
					}
				} else {
					prop_type = "target_source";
					prop_val = "\"\"";
				}
			} break;
			default:
				break;
		}

		if (!prop_val.is_empty()) {
			res += "\t" + prop + "(" + prop_type + ")";
			if (value.get_type() != Variant::ARRAY && (value.get_type() != Variant::DICTIONARY || prop_description != "\"\"")) {
				res += " : " + prop_description;
			}
			if (value.get_type() == Variant::BOOL) {
				res += (bool)value ? " : 1 = " : " : 0 = ";
			} else if (value.get_type() == Variant::DICTIONARY || value.get_type() == Variant::ARRAY) {
				res += " = ";
			} else {
				res += " : ";
			}
			res += prop_val + TrenchbroomUtil::newline();
		}
	}

	res += "]" + TrenchbroomUtil::newline();
	return res;
}

Dictionary BlaziumFGDEntityClass::retrieve_all_class_properties(Dictionary p_properties) const {
	Array keys = class_properties.keys();
	for (int i = 0; i < keys.size(); i++) {
		p_properties[keys[i]] = class_properties[keys[i]];
	}
	Array bases = base_classes;
	for (int i = 0; i < bases.size(); i++) {
		Ref<BlaziumFGDEntityClass> base = bases[i];
		if (base.is_valid()) {
			p_properties = base->retrieve_all_class_properties(p_properties);
		}
	}
	return p_properties;
}

Dictionary BlaziumFGDEntityClass::retrieve_all_class_property_descriptions(Dictionary p_descriptions) const {
	Array keys = class_property_descriptions.keys();
	for (int i = 0; i < keys.size(); i++) {
		p_descriptions[keys[i]] = class_property_descriptions[keys[i]];
	}
	Array bases = base_classes;
	for (int i = 0; i < bases.size(); i++) {
		Ref<BlaziumFGDEntityClass> base = bases[i];
		if (base.is_valid()) {
			p_descriptions = base->retrieve_all_class_property_descriptions(p_descriptions);
		}
	}
	return p_descriptions;
}
