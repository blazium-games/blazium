/**************************************************************************/
/*  autowork_collector.cpp                                                */
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
#include "autowork_collector.h"
#include "autowork_test.h"
#include "core/io/dir_access.h"
#include "core/io/resource_loader.h"
#include "modules/gdscript/gdscript.h"

void AutoworkCollector::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_script", "path"), &AutoworkCollector::add_script);
	ClassDB::bind_method(D_METHOD("process_directory", "path"), &AutoworkCollector::process_directory);
	ClassDB::bind_method(D_METHOD("get_scripts"), &AutoworkCollector::get_scripts);
	ClassDB::bind_method(D_METHOD("set_script_prefix", "prefix"), &AutoworkCollector::set_script_prefix);
	ClassDB::bind_method(D_METHOD("set_script_suffix", "suffix"), &AutoworkCollector::set_script_suffix);
	ClassDB::bind_method(D_METHOD("set_include_subdirectories", "enable"), &AutoworkCollector::set_include_subdirectories);
}

AutoworkCollector::AutoworkCollector() {
}

AutoworkCollector::~AutoworkCollector() {
}

void AutoworkCollector::clear() {
	scripts.clear();
}

void AutoworkCollector::set_script_prefix(const String &p_prefix) {
	script_prefix = p_prefix;
}

void AutoworkCollector::set_script_suffix(const String &p_suffix) {
	script_suffix = p_suffix;
}

void AutoworkCollector::set_include_subdirectories(bool p_enable) {
	include_subdirectories = p_enable;
}

void AutoworkCollector::add_script(const String &p_path) {
	if (!script_pattern.is_empty() && !p_path.get_file().contains(script_pattern)) {
		return;
	}

	Ref<Script> collected_script = ResourceLoader::load(p_path);
	if (collected_script.is_null()) {
		return;
	}

	// We only collect scripts that inherit AutoworkTest
	bool is_test = false;
	Ref<Script> base = collected_script;
	while (base.is_valid()) {
		if (base->get_instance_base_type() == "AutoworkTest" || base->get_global_name() == "AutoworkTest") {
			is_test = true;
			break;
		}
		base = base->get_base_script();
	}

	if (!is_test) {
		// Fallback: check instance_base_type
		if (collected_script->get_instance_base_type() != "AutoworkTest") {
			return;
		}
	}

	auto extract_methods = [&](const Ref<Script> &s_script, const String &p_inner_name) {
		if (s_script.is_null()) {
			return;
		}
		if (!inner_class_pattern.is_empty() && !p_inner_name.is_empty() && !p_inner_name.contains(inner_class_pattern)) {
			return;
		}

		List<MethodInfo> methods;
		s_script->get_script_method_list(&methods);

		TypedArray<String> test_methods;
		for (const MethodInfo &mi : methods) {
			if (mi.name.begins_with("test_")) {
				if (!test_pattern.is_empty() && !mi.name.contains(test_pattern)) {
					continue;
				}
				test_methods.push_back(mi.name);
			}
		}

		if (test_methods.is_empty()) {
			return;
		}

		Dictionary script_info;
		script_info["path"] = p_inner_name.is_empty() ? p_path : p_path + "[" + p_inner_name + "]";
		script_info["tests"] = test_methods;
		script_info["script"] = s_script;
		script_info["inner_class"] = p_inner_name;

		scripts.push_back(script_info);
	};

	extract_methods(collected_script, "");

	HashMap<StringName, Variant> constants;
	collected_script->get_constants(&constants);

	for (const KeyValue<StringName, Variant> &E : constants) {
		String key = E.key;
		if (key.begins_with("Test")) {
			Variant v = E.value;
			if (v.get_type() == Variant::OBJECT) {
				Object *obj = v.get_validated_object();
				if (obj && Object::cast_to<Script>(obj)) {
					Ref<Script> inner_script(Object::cast_to<Script>(obj));
					bool inner_is_test = false;
					Ref<Script> inner_base = inner_script;
					while (inner_base.is_valid()) {
						if (inner_base->get_instance_base_type() == "AutoworkTest" || inner_base->get_global_name() == "AutoworkTest") {
							inner_is_test = true;
							break;
						}
						inner_base = inner_base->get_base_script();
					}
					if (!inner_is_test && inner_script->get_instance_base_type() == "AutoworkTest") {
						inner_is_test = true;
					}

					if (inner_is_test) {
						extract_methods(inner_script, key);
					}
				}
			}
		}
	}
}

void AutoworkCollector::process_directory(const String &p_path) {
	Ref<DirAccess> da = DirAccess::open(p_path);
	if (da.is_null()) {
		return;
	}

	da->list_dir_begin();
	String file = da->get_next();
	while (!file.is_empty()) {
		if (file == "." || file == "..") {
			file = da->get_next();
			continue;
		}

		String full_path = p_path.path_join(file);
		if (da->current_is_dir()) {
			if (include_subdirectories) {
				process_directory(full_path);
			}
		} else if (file.ends_with(script_suffix)) {
			if (script_prefix.is_empty() || file.begins_with(script_prefix)) {
				add_script(full_path);
			}
		}
		file = da->get_next();
	}
}

Array AutoworkCollector::get_scripts() const {
	return scripts;
}
