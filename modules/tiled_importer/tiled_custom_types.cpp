/**************************************************************************/
/*  tiled_custom_types.cpp                                                */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
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

#include "tiled_custom_types.h"
#include "tiled_common.h"
#include "tiled_dictionary_builder.h"

TiledCustomTypes::TiledCustomTypes() {
}

TiledCustomTypes::~TiledCustomTypes() {
}

void TiledCustomTypes::load_custom_types(const String &p_project_file) {
	PackedByteArray proj_file_content = DataLoader::get_tiled_file_content(p_project_file, "");
	if (proj_file_content.is_empty()) {
		ERR_PRINT("ERROR: Tiled project file '" + p_project_file + "' not found.");
		return;
	}

	Dictionary project_file_as_dictionary = TiledDictionaryBuilder::get_dictionary(proj_file_content, p_project_file);
	if (project_file_as_dictionary.has("propertyTypes")) {
		_custom_types = project_file_as_dictionary["propertyTypes"];
	}
}

void TiledCustomTypes::unload_custom_types() {
	_custom_types.clear();
}

void TiledCustomTypes::merge_custom_properties(Dictionary &p_obj, const String &p_scope) {
	if (_custom_types.is_empty()) {
		return;
	}

	String class_string = p_obj.get("class", "");
	if (class_string.is_empty()) {
		class_string = p_obj.get("type", "").operator String();
	}

	Array properties;
	bool new_key = false;
	if (p_obj.has("properties")) {
		properties = p_obj["properties"];
	} else {
		new_key = true;
	}

	for (int i = 0; i < _custom_types.size(); i++) {
		Dictionary ct_prop = _custom_types[i];
		String pt_name = ct_prop.get("name", "");
		String pt_type = ct_prop.get("type", "");
		Array pt_scope = ct_prop.get("useAs", Array());

		if (pt_name != class_string || pt_type != "class" || !pt_scope.has(p_scope)) {
			continue;
		}

		Array pt_members = ct_prop.get("members", Array());
		for (int m = 0; m < pt_members.size(); m++) {
			Dictionary mem = pt_members[m];
			String name = mem.get("name", "");
			bool append = true;
			for (int p = 0; p < properties.size(); p++) {
				Dictionary prop = properties[p];
				if (name == prop.get("name", "").operator String()) {
					append = false;
					break;
				}
			}
			if (!append) {
				continue;
			}
			if (new_key) {
				p_obj["properties"] = Array();
				new_key = false;
			}
			Array current_props = p_obj["properties"];
			current_props.push_back(mem);
			p_obj["properties"] = current_props;
		}
	}
}
