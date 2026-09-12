/**************************************************************************/
/*  blazium_fgd_entity_class.h                                            */
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

#include "blazium_fgd_file.h"

#include "core/io/resource.h"
#include "core/object/script_language.h"

class BlaziumFGDEntityClass : public Resource {
	GDCLASS(BlaziumFGDEntityClass, Resource);

protected:
	static void _bind_methods();

	String prefix;
	String classname;
	String description;
	bool blazium_internal = false;
	Array base_classes;
	Dictionary class_properties;
	Dictionary class_property_descriptions;
	bool auto_apply_to_matching_node_properties = false;
	Dictionary meta_properties;
	String node_class;
	String name_property;
	Vector<String> node_groups;
	Ref<Script> entity_extension_script;

public:
	void set_prefix(const String &p_prefix) { prefix = p_prefix; }
	String get_prefix() const { return prefix; }

	void set_classname(const String &p_classname) { classname = p_classname; }
	String get_classname() const { return classname; }

	void set_description(const String &p_description) { description = p_description; }
	String get_description() const { return description; }

	void set_blazium_internal(bool p_internal) { blazium_internal = p_internal; }
	bool get_blazium_internal() const { return blazium_internal; }

	void set_base_classes(const Array &p_classes) { base_classes = p_classes; }
	Array get_base_classes() const { return base_classes; }

	void set_class_properties(const Dictionary &p_props) { class_properties = p_props; }
	Dictionary get_class_properties() const { return class_properties; }

	void set_class_property_descriptions(const Dictionary &p_desc) { class_property_descriptions = p_desc; }
	Dictionary get_class_property_descriptions() const { return class_property_descriptions; }

	void set_auto_apply_to_matching_node_properties(bool p_value) { auto_apply_to_matching_node_properties = p_value; }
	bool get_auto_apply_to_matching_node_properties() const { return auto_apply_to_matching_node_properties; }

	void set_meta_properties(const Dictionary &p_meta) { meta_properties = p_meta; }
	Dictionary get_meta_properties() const { return meta_properties; }

	void set_node_class(const String &p_class) { node_class = p_class; }
	String get_node_class() const { return node_class; }

	void set_name_property(const String &p_prop) { name_property = p_prop; }
	String get_name_property() const { return name_property; }

	void set_node_groups(const Vector<String> &p_groups) { node_groups = p_groups; }
	Vector<String> get_node_groups() const { return node_groups; }

	void set_entity_extension_script(const Ref<Script> &p_script) { entity_extension_script = p_script; }
	Ref<Script> get_entity_extension_script() const { return entity_extension_script; }

	virtual String build_def_text(BlaziumFGDFile::TargetMapEditor p_target_editor) const;
	Dictionary retrieve_all_class_properties(Dictionary p_properties = Dictionary()) const;
	Dictionary retrieve_all_class_property_descriptions(Dictionary p_descriptions = Dictionary()) const;
};
