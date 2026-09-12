/**************************************************************************/
/*  autowork_doubler.cpp                                                  */
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

#include "autowork_doubler.h"
#include "autowork_spy.h"
#include "autowork_stubber.h"
#include "core/io/resource_loader.h"
#include "modules/gdscript/gdscript.h"

void AutoworkDoubler::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_spy", "spy"), &AutoworkDoubler::set_spy);
	ClassDB::bind_method(D_METHOD("set_stubber", "stubber"), &AutoworkDoubler::set_stubber);
	ClassDB::bind_method(D_METHOD("double_script", "path"), &AutoworkDoubler::double_script);
	ClassDB::bind_method(D_METHOD("double_scene", "path"), &AutoworkDoubler::double_scene);
}

AutoworkDoubler::AutoworkDoubler() {
}

AutoworkDoubler::~AutoworkDoubler() {
}

void AutoworkDoubler::set_spy(const Ref<AutoworkSpy> &p_spy) {
	spy = p_spy;
}

void AutoworkDoubler::set_stubber(const Ref<AutoworkStubber> &p_stubber) {
	stubber = p_stubber;
}

// Helper to stringify variants into GDScript literals
static String _get_default_val_str(const Variant &p_val) {
	if (p_val.get_type() == Variant::STRING || p_val.get_type() == Variant::STRING_NAME) {
		return "\"" + p_val.operator String().replace("\"", "\\\"") + "\"";
	} else if (p_val.get_type() == Variant::NIL) {
		return "null";
	} else {
		return p_val.operator String();
	}
}

Variant AutoworkDoubler::double_script(const String &p_path) {
	Ref<Script> base_script = ResourceLoader::load(p_path);
	if (base_script.is_null()) {
		ERR_FAIL_V_MSG(Variant(), "Could not load script for doubling: " + p_path);
	}

	Ref<GDScript> gd_script = base_script;
	if (gd_script.is_null()) {
		ERR_FAIL_V_MSG(Variant(), "Only GDScript is supported for doubling currently: " + p_path);
	}

	String source_code = "extends \"" + p_path + "\"\n\n";
	source_code += "var __autowork_spy\n";
	source_code += "var __autowork_stubber\n\n";

	List<MethodInfo> methods;
	gd_script->get_script_method_list(&methods);

	for (const MethodInfo &mi : methods) {
		if (mi.name.begins_with("@") || mi.name == "_init" || mi.name == "_ready") {
			continue; // Skip internal and standard lifecycle methods for now
		}

		String sig = "func " + mi.name + "(";
		String call_args = "[";
		String super_args = "";

		int default_arg_start = mi.arguments.size() - mi.default_arguments.size();

		for (int i = 0; i < mi.arguments.size(); i++) {
			if (i > 0) {
				sig += ", ";
				call_args += ", ";
				super_args += ", ";
			}
			String arg_name = mi.arguments[i].name;
			if (arg_name.is_empty()) {
				arg_name = "arg" + itos(i);
			}

			sig += arg_name;
			call_args += arg_name;
			super_args += arg_name;

			if (i >= default_arg_start) {
				Variant def_val = mi.default_arguments[i - default_arg_start];
				sig += " = " + _get_default_val_str(def_val);
			}
		}
		sig += "):\n";
		call_args += "]";

		source_code += sig;
		source_code += "\tif __autowork_spy:\n";
		source_code += "\t\t__autowork_spy.add_call(self, \"" + mi.name + "\", " + call_args + ")\n";
		source_code += "\tif __autowork_stubber and __autowork_stubber.should_call_super(self, \"" + mi.name + "\", " + call_args + "):\n";
		source_code += "\t\treturn super(" + super_args + ")\n";
		source_code += "\tif __autowork_stubber:\n";
		source_code += "\t\treturn __autowork_stubber.get_return(self, \"" + mi.name + "\", " + call_args + ")\n";
		source_code += "\treturn null\n\n";
	}

	Ref<GDScript> doubled = memnew(GDScript);
	doubled->set_source_code(source_code);
	Error err = doubled->reload();
	if (err != OK) {
		ERR_FAIL_V_MSG(Variant(), "Failed to compile doubled script for: " + p_path);
	}

	return doubled;
}

Variant AutoworkDoubler::double_scene(const String &p_path) {
	// Basic scene doubling: load scene, double its script
	return Variant();
}
