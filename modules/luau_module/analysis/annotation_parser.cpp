/**************************************************************************/
/*  annotation_parser.cpp                                                 */
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

#include "core/object/class_db.h"
#include "analysis/annotation_parser.h"

#include "luau_class_info.h"


using namespace luau_module;

namespace {

static Variant::Type luau_type_to_variant(const String &p_type) {
	if (p_type == "int") {
		return Variant::INT;
	}
	if (p_type == "number" || p_type == "float") {
		return Variant::FLOAT;
	}
	if (p_type == "boolean" || p_type == "bool") {
		return Variant::BOOL;
	}
	if (p_type == "string") {
		return Variant::STRING;
	}
	if (p_type == "Vector2") {
		return Variant::VECTOR2;
	}
	if (p_type == "Vector3") {
		return Variant::VECTOR3;
	}
	if (ClassDB::class_exists(p_type)) {
		return Variant::OBJECT;
	}
	return Variant::NIL;
}

static PropertyHint parse_property_hint(const String &p_hint) {
	if (p_hint == "range") {
		return PROPERTY_HINT_RANGE;
	}
	if (p_hint == "enum") {
		return PROPERTY_HINT_ENUM;
	}
	if (p_hint == "file") {
		return PROPERTY_HINT_FILE;
	}
	if (p_hint == "dir") {
		return PROPERTY_HINT_DIR;
	}
	if (p_hint == "multiline") {
		return PROPERTY_HINT_MULTILINE_TEXT;
	}
	return PROPERTY_HINT_NONE;
}

static void parse_property_args(const String &p_args, LuauClassProperty &r_prop) {
	PackedStringArray tokens = p_args.split(" ", false);
	for (int i = 0; i < tokens.size(); i++) {
		const String token = tokens[i];
		if (token.begins_with("name=")) {
			r_prop.info.name = StringName(token.substr(5));
		} else if (token.begins_with("type=")) {
			const String type_name = token.substr(5);
			const Variant::Type vt = luau_type_to_variant(type_name);
			if (vt != Variant::NIL) {
				r_prop.info.type = vt;
				if (vt == Variant::OBJECT) {
					r_prop.info.class_name = StringName(type_name);
				}
			}
		} else if (token.begins_with("default=")) {
			const String value = token.substr(8);
			if (value == "true" || value == "false") {
				r_prop.default_value = value == "true";
				r_prop.info.type = Variant::BOOL;
			} else if (value.is_valid_int()) {
				r_prop.default_value = value.to_int();
				if (r_prop.info.type == Variant::NIL) {
					r_prop.info.type = Variant::INT;
				}
			} else if (value.is_valid_float()) {
				r_prop.default_value = value.to_float();
				if (r_prop.info.type == Variant::NIL) {
					r_prop.info.type = Variant::FLOAT;
				}
			} else {
				r_prop.default_value = value;
				r_prop.info.type = Variant::STRING;
			}
		} else if (token.begins_with("hint=")) {
			r_prop.info.hint = parse_property_hint(token.substr(5));
		} else if (token.begins_with("hint_string=")) {
			r_prop.info.hint_string = token.substr(12);
		} else if (token == "export") {
			r_prop.info.usage |= PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_STORAGE;
		} else if (token == "onready") {
			r_prop.info.usage |= PROPERTY_USAGE_SCRIPT_VARIABLE;
		} else if (r_prop.info.name.is_empty()) {
			r_prop.info.name = StringName(token);
		} else if (r_prop.info.type == Variant::NIL) {
			const Variant::Type vt = luau_type_to_variant(token);
			if (vt != Variant::NIL) {
				r_prop.info.type = vt;
				if (vt == Variant::OBJECT) {
					r_prop.info.class_name = StringName(token);
				}
			}
		}
	}
	if (r_prop.info.type == Variant::NIL) {
		r_prop.info.type = Variant::NIL;
	}
	r_prop.info.usage |= PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SCRIPT_VARIABLE;
}

static void parse_rpc_args(const String &p_args, Dictionary &r_rpc) {
	PackedStringArray tokens = p_args.split(" ", false);
	if (!tokens.is_empty()) {
		r_rpc["mode"] = tokens[0].to_int();
	}
	if (tokens.size() > 1) {
		r_rpc["transfer_mode"] = tokens[1].to_int();
	}
	if (tokens.size() > 2) {
		r_rpc["call_local"] = tokens[2] == "true";
	}
}

} //namespace

bool LuauAnnotationParser::parse_source(const String &p_source, LuauClassInfo *r_info) {
	if (!r_info) {
		return false;
	}

	LuauClassInfo parsed;
	bool found = false;

	const PackedStringArray lines = p_source.split("\n");
	String pending_doc;
	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i].strip_edges();
		if (line.begins_with("---") && !line.begins_with("---@")) {
			const String doc_line = line.substr(3).strip_edges();
			if (!doc_line.begins_with("@")) {
				if (!pending_doc.is_empty()) {
					pending_doc += "\n";
				}
				pending_doc += doc_line;
				continue;
			}
		}
		if (!line.begins_with("---")) {
			pending_doc = String();
			continue;
		}
		line = line.substr(3).strip_edges();
		if (!line.begins_with("@")) {
			continue;
		}

		const int space = line.find(" ");
		const String tag = space >= 0 ? line.substr(1, space - 1) : line.substr(1);
		const String args = space >= 0 ? line.substr(space + 1).strip_edges() : String();
		const String doc_for_tag = pending_doc;
		pending_doc = String();

		if (tag == "class") {
			parsed.class_name = StringName(args.get_slice(" ", 0));
			parsed.class_description = doc_for_tag;
			found = true;
		} else if (tag == "extends") {
			parsed.extends = args.get_slice(" ", 0);
			found = true;
		} else if (tag == "tool") {
			parsed.tool = true;
			found = true;
		} else if (tag == "iconPath" || tag == "icon") {
			parsed.icon_path = args;
			found = true;
		} else if (tag == "abstract") {
			parsed.abstract = true;
			found = true;
		} else if (tag == "property") {
			LuauClassProperty prop;
			parse_property_args(args, prop);
			prop.description = doc_for_tag;
			if (!prop.info.name.is_empty()) {
				parsed.properties[prop.info.name] = prop;
				found = true;
			}
		} else if (tag == "signal") {
			const StringName sig_name(args.get_slice(" ", 0));
			if (!sig_name.is_empty()) {
				LuauClassSignal sig;
				sig.info.name = sig_name;
				sig.description = doc_for_tag;
				parsed.signals[sig_name] = sig;
				found = true;
			}
		} else if (tag == "registerMethod") {
			const StringName method_name(args.get_slice(" ", 0));
			if (!method_name.is_empty() && !parsed.methods.has(method_name)) {
				LuauClassMethod method;
				method.info.name = method_name;
				method.description = doc_for_tag;
				parsed.methods[method_name] = method;
				found = true;
			}
		} else if (tag == "registerConstant") {
			const StringName const_name(args.get_slice(" ", 0));
			if (!const_name.is_empty()) {
				Variant value;
				const String value_text = args.get_slice(" ", 1);
				if (value_text == "true" || value_text == "false") {
					value = value_text == "true";
				} else if (value_text.is_valid_int()) {
					value = value_text.to_int();
				} else if (value_text.is_valid_float()) {
					value = value_text.to_float();
				} else if (!value_text.is_empty()) {
					value = value_text;
				}
				parsed.constants[const_name] = value;
				found = true;
			}
		} else if (tag == "rpc") {
			const PackedStringArray rpc_tokens = args.split(" ", false);
			if (!rpc_tokens.is_empty()) {
				const StringName method_name(rpc_tokens[0]);
				Dictionary rpc_entry;
				if (rpc_tokens.size() > 1) {
					String rpc_args;
					for (int t = 1; t < rpc_tokens.size(); t++) {
						if (t > 1) {
							rpc_args += " ";
						}
						rpc_args += rpc_tokens[t];
					}
					parse_rpc_args(rpc_args, rpc_entry);
				}
				parsed.rpc_config[method_name] = rpc_entry;
				found = true;
			}
		}
	}

	if (found) {
		merge_into(r_info, parsed);
	}
	return found;
}

void LuauAnnotationParser::merge_into(LuauClassInfo *r_info, const LuauClassInfo &p_annotations) {
	if (!r_info) {
		return;
	}
	if (!p_annotations.class_name.is_empty() && r_info->class_name.is_empty()) {
		r_info->class_name = p_annotations.class_name;
	}
	if (!p_annotations.extends.is_empty() && (r_info->extends.is_empty() || r_info->extends == "RefCounted")) {
		r_info->extends = p_annotations.extends;
	}
	if (p_annotations.tool) {
		r_info->tool = true;
	}
	if (p_annotations.abstract) {
		r_info->abstract = true;
	}
	if (!p_annotations.icon_path.is_empty() && r_info->icon_path.is_empty()) {
		r_info->icon_path = p_annotations.icon_path;
	}
	if (!p_annotations.class_description.is_empty() && r_info->class_description.is_empty()) {
		r_info->class_description = p_annotations.class_description;
	}
	for (const KeyValue<StringName, LuauClassProperty> &pair : p_annotations.properties) {
		if (!r_info->properties.has(pair.key)) {
			r_info->properties[pair.key] = pair.value;
		} else {
			LuauClassProperty &existing = r_info->properties[pair.key];
			if (existing.info.type == Variant::NIL && pair.value.info.type != Variant::NIL) {
				existing.info.type = pair.value.info.type;
			}
			if (existing.info.hint == PROPERTY_HINT_NONE && pair.value.info.hint != PROPERTY_HINT_NONE) {
				existing.info.hint = pair.value.info.hint;
				existing.info.hint_string = pair.value.info.hint_string;
			}
			if (pair.value.default_value.get_type() != Variant::NIL) {
				existing.default_value = pair.value.default_value;
			}
			if (existing.description.is_empty() && !pair.value.description.is_empty()) {
				existing.description = pair.value.description;
			}
			existing.info.usage |= pair.value.info.usage;
		}
	}
	for (const KeyValue<StringName, LuauClassSignal> &pair : p_annotations.signals) {
		if (!r_info->signals.has(pair.key)) {
			r_info->signals[pair.key] = pair.value;
		}
	}
	for (const KeyValue<StringName, LuauClassMethod> &pair : p_annotations.methods) {
		if (!r_info->methods.has(pair.key)) {
			r_info->methods[pair.key] = pair.value;
		} else if (r_info->methods[pair.key].description.is_empty() && !pair.value.description.is_empty()) {
			r_info->methods[pair.key].description = pair.value.description;
		}
	}
	for (const KeyValue<StringName, Variant> &pair : p_annotations.constants) {
		if (!r_info->constants.has(pair.key)) {
			r_info->constants[pair.key] = pair.value;
		}
	}
	Array rpc_keys = p_annotations.rpc_config.keys();
	for (int i = 0; i < rpc_keys.size(); i++) {
		const StringName key = rpc_keys[i];
		if (!r_info->rpc_config.has(key)) {
			r_info->rpc_config[key] = p_annotations.rpc_config[key];
		}
	}
}
