/**************************************************************************/
/*  autowork_config.cpp                                                   */
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
#include "autowork_config.h"

#include "core/io/file_access.h"
#include "core/io/json.h"

void AutoworkConfig::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load_options", "file_path"), &AutoworkConfig::load_options);
	ClassDB::bind_method(D_METHOD("write_options", "file_path"), &AutoworkConfig::write_options);
	ClassDB::bind_method(D_METHOD("apply_options", "runner"), &AutoworkConfig::apply_options);
	ClassDB::bind_method(D_METHOD("get_options"), &AutoworkConfig::get_options);
	ClassDB::bind_method(D_METHOD("set_options", "options"), &AutoworkConfig::set_options);
}

AutoworkConfig::AutoworkConfig() {
	options["background_color"] = "262626ff"; // Color(.15, .15, .15, 1).to_html()
	options["config_file"] = "res://.autoworkconfig.json";
	options["dirs"] = Array();
	options["disable_colors"] = false;
	options["double_strategy"] = "SCRIPT_ONLY";
	options["errors_do_not_cause_failure"] = false;
	options["hide_orphans"] = false;
	options["include_subdirs"] = false;
	options["inner_class"] = "";
	options["junit_xml_file"] = "";
	options["junit_xml_timestamp"] = false;
	options["log_level"] = 1;
	options["paint_after"] = 0.1;
	options["prefix"] = "test_";
	options["selected"] = "";
	options["suffix"] = ".gd";
	options["tests"] = Array();
	options["unit_test_name"] = "";
}

AutoworkConfig::~AutoworkConfig() {}

void AutoworkConfig::load_options(const String &p_file_path) {
	if (!FileAccess::exists(p_file_path)) {
		return;
	}

	Ref<FileAccess> f = FileAccess::open(p_file_path, FileAccess::READ);
	if (f.is_null()) {
		return;
	}

	String json_text = f->get_as_text();
	Ref<JSON> json;
	json.instantiate();
	Error err = json->parse(json_text);
	if (err == OK) {
		Variant results = json->get_data();
		if (results.get_type() == Variant::DICTIONARY) {
			Dictionary loaded = results;
			Array keys = loaded.keys();
			for (int i = 0; i < keys.size(); i++) {
				String k = keys[i];
				if (options.has(k)) {
					options[k] = loaded[k];
				}
			}
		}
	}
}

void AutoworkConfig::write_options(const String &p_file_path) {
	String content = JSON::stringify(options, "  ");
	Ref<FileAccess> f = FileAccess::open(p_file_path, FileAccess::WRITE);
	if (f.is_valid()) {
		f->store_string(content);
	}
}

void AutoworkConfig::apply_options(Autowork *p_runner) {
	if (!p_runner) {
		return;
	}

	// Apply equivalent options found in gut_config.gd's _apply_options
	if (options.has("dirs")) {
		Array dirs = options["dirs"];
		String prefix = options["prefix"];
		String suffix = options["suffix"];
		for (int i = 0; i < dirs.size(); i++) {
			p_runner->add_directory(dirs[i], prefix, suffix);
		}
	}

	if (options.has("tests")) {
		Array tests = options["tests"];
		for (int i = 0; i < tests.size(); i++) {
			p_runner->add_script(tests[i]);
		}
	}

	if (options.has("include_subdirs")) { // not explicitly in AutoworkMain yet, but map if present
		// Runner logic typically uses add_directory with recursive flag, handled in CLI. Make sure runner has properties.
	}
}
