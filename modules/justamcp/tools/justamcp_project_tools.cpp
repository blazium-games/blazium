/**************************************************************************/
/*  justamcp_project_tools.cpp                                            */
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

#include "justamcp_project_tools.h"
#include "../justamcp_editor_plugin.h"

#include "core/config/project_settings.h"
#include "core/input/input_event.h"
#include "core/input/input_map.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "modules/regex/regex.h"

void JustAMCPProjectTools::_bind_methods() {}

Dictionary JustAMCPProjectTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	if (p_tool_name == "map_project") {
		return map_project(p_args);
	}
	if (p_tool_name == "map_scenes") {
		return map_scenes(p_args);
	}
	if (p_tool_name == "list_settings") {
		return list_settings(p_args);
	}
	if (p_tool_name == "update_settings") {
		return update_settings(p_args);
	}
	if (p_tool_name == "manage_autoloads") {
		return manage_autoloads(p_args);
	}
	if (p_tool_name == "get_collision_layers") {
		return get_collision_layers(p_args);
	}
	if (p_tool_name == "project_get_input_actions") {
		return get_input_actions(p_args);
	}
	if (p_tool_name == "project_set_input_action") {
		return set_input_action(p_args);
	}
	if (p_tool_name == "project_remove_input_action") {
		return remove_input_action(p_args);
	}

	Dictionary ret;
	ret["ok"] = false;
	ret["error"] = "Unknown project tool: " + p_tool_name;
	return ret;
}

Dictionary JustAMCPProjectTools::map_project(const Dictionary &p_args) {
	String root_path = p_args.get("root", "res://");
	bool include_addons = p_args.get("include_addons", false);
	int lod = p_args.get("lod", 1); // Level of Detail: 0=Paths only, 1=Structure, 2=Full parsing

	if (!root_path.begins_with("res://")) {
		root_path = "res://" + root_path;
	}

	Array script_paths;
	_collect_scripts(root_path, script_paths, include_addons);

	Array nodes;
	Dictionary class_map;

	for (int i = 0; i < script_paths.size(); i++) {
		String path = script_paths[i];
		Dictionary info = _parse_script(path, lod);
		nodes.push_back(info);
		if (info.has("class_name") && !String(info["class_name"]).is_empty()) {
			class_map[info["class_name"]] = path;
		}
	}

	Dictionary result;
	result["ok"] = true;
	result["nodes"] = nodes;
	result["total_scripts"] = nodes.size();
	return result;
}

void JustAMCPProjectTools::_collect_scripts(const String &p_path, Array &r_results, bool p_include_addons) {
	Ref<DirAccess> dir = DirAccess::open(p_path);
	if (dir.is_null()) {
		return;
	}

	dir->list_dir_begin();
	String name = dir->get_next();
	while (!name.is_empty()) {
		if (name.begins_with(".")) {
			name = dir->get_next();
			continue;
		}
		String full_path = p_path.path_join(name);
		if (dir->current_is_dir()) {
			if (name == "addons" && !p_include_addons) {
				name = dir->get_next();
				continue;
			}
			_collect_scripts(full_path, r_results, p_include_addons);
		} else if (name.ends_with(".gd")) {
			r_results.push_back(full_path);
		}
		name = dir->get_next();
	}
	dir->list_dir_end();
}

Dictionary JustAMCPProjectTools::_parse_script(const String &p_path, int p_lod) {
	Dictionary info;
	info["path"] = p_path;
	if (p_lod == 0) {
		return info;
	}

	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);
	if (f.is_null()) {
		return info;
	}

	String content = f->get_as_text();
	f->close();

	if (p_lod >= 1) {
		Ref<RegEx> re_class;
		re_class.instantiate();
		re_class->compile("^class_name\\s+(\\w+)");
		Ref<RegEx> re_extends;
		re_extends.instantiate();
		re_extends->compile("^extends\\s+(\\w+)");

		Ref<RegExMatch> m_class = re_class->search(content);
		if (m_class.is_valid()) {
			info["class_name"] = m_class->get_string(1);
		}

		Ref<RegExMatch> m_ext = re_extends->search(content);
		if (m_ext.is_valid()) {
			info["extends"] = m_ext->get_string(1);
		}
	}

	if (p_lod >= 2) {
		// Full parsing (methods, variables)
		Array methods;
		Ref<RegEx> re_func;
		re_func.instantiate();
		re_func->compile("^func\\s+(\\w+)");
		TypedArray<RegExMatch> matches = re_func->search_all(content);
		for (int i = 0; i < matches.size(); i++) {
			Ref<RegExMatch> m = matches[i];
			methods.push_back(m->get_string(1));
		}
		info["methods"] = methods;
	}

	return info;
}

Dictionary JustAMCPProjectTools::map_scenes(const Dictionary &p_args) {
	String root_path = p_args.get("root", "res://");
	bool include_addons = p_args.get("include_addons", false);

	if (!root_path.begins_with("res://")) {
		root_path = "res://" + root_path;
	}

	Array scene_paths;
	_collect_scenes(root_path, scene_paths, include_addons);

	Array scenes;
	for (int i = 0; i < scene_paths.size(); i++) {
		scenes.push_back(_parse_scene(scene_paths[i]));
	}

	Dictionary result;
	result["ok"] = true;
	result["scenes"] = scenes;
	result["total_scenes"] = scenes.size();
	return result;
}

void JustAMCPProjectTools::_collect_scenes(const String &p_path, Array &r_results, bool p_include_addons) {
	Ref<DirAccess> dir = DirAccess::open(p_path);
	if (dir.is_null()) {
		return;
	}

	dir->list_dir_begin();
	String name = dir->get_next();
	while (!name.is_empty()) {
		if (name.begins_with(".")) {
			name = dir->get_next();
			continue;
		}
		String full_path = p_path.path_join(name);
		if (dir->current_is_dir()) {
			if (name == "addons" && !p_include_addons) {
				name = dir->get_next();
				continue;
			}
			_collect_scenes(full_path, r_results, p_include_addons);
		} else if (name.ends_with(".tscn") || name.ends_with(".scn")) {
			r_results.push_back(full_path);
		}
		name = dir->get_next();
	}
	dir->list_dir_end();
}

Dictionary JustAMCPProjectTools::_parse_scene(const String &p_path) {
	Dictionary info;
	info["path"] = p_path;
	info["filename"] = p_path.get_file();

	// Basic TSCN parsing could be added here similar to godot-mcp
	return info;
}

Dictionary JustAMCPProjectTools::list_settings(const Dictionary &p_args) {
	String category = p_args.get("category", "");

	Array settings;
	List<PropertyInfo> props;
	ProjectSettings::get_singleton()->get_property_list(&props);

	for (const PropertyInfo &pi : props) {
		if (!category.is_empty() && !pi.name.begins_with(category + "/")) {
			continue;
		}

		Dictionary s;
		s["path"] = pi.name;
		s["type"] = _type_to_string(pi.type);
		s["value"] = _serialize_value(ProjectSettings::get_singleton()->get_setting(pi.name));
		settings.push_back(s);
	}

	Dictionary result;
	result["ok"] = true;
	result["settings"] = settings;
	return result;
}

Dictionary JustAMCPProjectTools::update_settings(const Dictionary &p_args) {
	Dictionary settings = p_args.get("settings", Dictionary());
	Array keys = settings.keys();

	for (int i = 0; i < keys.size(); i++) {
		String key = keys[i];
		ProjectSettings::get_singleton()->set_setting(key, settings[key]);
	}

	ProjectSettings::get_singleton()->save();

	Dictionary result;
	result["ok"] = true;
	result["updated_count"] = keys.size();
	return result;
}

Dictionary JustAMCPProjectTools::manage_autoloads(const Dictionary &p_args) {
	String op = p_args.get("operation", "list");
	ProjectSettings *settings = ProjectSettings::get_singleton();

	if (op == "list") {
		Array list;
		List<PropertyInfo> props;
		settings->get_property_list(&props);
		for (const PropertyInfo &pi : props) {
			if (pi.name.begins_with("autoload/")) {
				String value = settings->get_setting(pi.name);
				Dictionary al;
				al["name"] = pi.name.substr(9);
				al["path"] = value.begins_with("*") ? value.substr(1) : value;
				al["singleton"] = value.begins_with("*");
				al["order"] = settings->get_order(pi.name);
				list.push_back(al);
			}
		}
		Dictionary res;
		res["ok"] = true;
		res["autoloads"] = list;
		return res;
	}

	if (op == "add") {
		String name = p_args.get("name", "");
		String path = p_args.get("path", "");
		bool singleton = p_args.get("singleton", true);
		if (name.is_empty() || name.contains("/") || name.contains("\\")) {
			Dictionary res;
			res["ok"] = false;
			res["error"] = "Autoload 'name' is required and must not contain path separators.";
			return res;
		}
		if (!path.begins_with("res://") || !FileAccess::exists(path)) {
			Dictionary res;
			res["ok"] = false;
			res["error"] = "Autoload 'path' must be an existing res:// file.";
			return res;
		}
		String setting_name = "autoload/" + name;
		if (settings->has_setting(setting_name)) {
			Dictionary res;
			res["ok"] = false;
			res["error"] = "Autoload already exists: " + name;
			return res;
		}
		settings->set_setting(setting_name, singleton ? "*" + path : path);
		if (p_args.has("order")) {
			settings->set_order(setting_name, p_args["order"]);
		}
		Error err = settings->save();
		Dictionary res;
		res["ok"] = err == OK;
		res["operation"] = op;
		res["name"] = name;
		res["path"] = path;
		res["singleton"] = singleton;
		if (err != OK) {
			res["error"] = "Failed to save project settings.";
		}
		return res;
	}

	if (op == "remove") {
		String name = p_args.get("name", "");
		String setting_name = name.begins_with("autoload/") ? name : "autoload/" + name;
		if (name.is_empty() || !settings->has_setting(setting_name)) {
			Dictionary res;
			res["ok"] = false;
			res["error"] = "Autoload not found: " + name;
			return res;
		}
		String removed_path = settings->get_setting(setting_name);
		settings->clear(setting_name);
		Error err = settings->save();
		Dictionary res;
		res["ok"] = err == OK;
		res["operation"] = op;
		res["name"] = name;
		res["path"] = removed_path.begins_with("*") ? removed_path.substr(1) : removed_path;
		if (err != OK) {
			res["error"] = "Failed to save project settings.";
		}
		return res;
	}

	if (op == "update") {
		String name = p_args.get("name", "");
		String setting_name = name.begins_with("autoload/") ? name : "autoload/" + name;
		if (name.is_empty() || !settings->has_setting(setting_name)) {
			Dictionary res;
			res["ok"] = false;
			res["error"] = "Autoload not found: " + name;
			return res;
		}

		String current_value = settings->get_setting(setting_name);
		String path = p_args.get("path", current_value.begins_with("*") ? current_value.substr(1) : current_value);
		bool singleton = p_args.get("singleton", current_value.begins_with("*"));
		if (!path.begins_with("res://") || !FileAccess::exists(path)) {
			Dictionary res;
			res["ok"] = false;
			res["error"] = "Autoload 'path' must be an existing res:// file.";
			return res;
		}

		String new_name = p_args.get("new_name", name);
		String new_setting_name = new_name.begins_with("autoload/") ? new_name : "autoload/" + new_name;
		if (new_name.is_empty() || new_name.contains("/") || new_name.contains("\\")) {
			Dictionary res;
			res["ok"] = false;
			res["error"] = "Autoload 'new_name' must not contain path separators.";
			return res;
		}
		if (new_setting_name != setting_name && settings->has_setting(new_setting_name)) {
			Dictionary res;
			res["ok"] = false;
			res["error"] = "Autoload already exists: " + new_name;
			return res;
		}

		int order = p_args.get("order", settings->get_order(setting_name));
		if (new_setting_name != setting_name) {
			settings->clear(setting_name);
		}
		settings->set_setting(new_setting_name, singleton ? "*" + path : path);
		settings->set_order(new_setting_name, order);
		Error err = settings->save();
		Dictionary res;
		res["ok"] = err == OK;
		res["operation"] = op;
		res["name"] = new_name;
		res["path"] = path;
		res["singleton"] = singleton;
		res["order"] = order;
		if (err != OK) {
			res["error"] = "Failed to save project settings.";
		}
		return res;
	}

	Dictionary res;
	res["ok"] = false;
	res["error"] = "Unknown autoload operation: " + op;
	return res;
}

Dictionary JustAMCPProjectTools::get_collision_layers(const Dictionary &p_args) {
	Dictionary res;
	res["ok"] = true;
	Array layers2d;
	Array layers3d;
	for (int i = 1; i <= 32; i++) {
		String name2d = ProjectSettings::get_singleton()->get_setting("layer_names/2d_physics/layer_" + itos(i));
		if (!name2d.is_empty()) {
			Dictionary d;
			d["index"] = i;
			d["name"] = name2d;
			layers2d.push_back(d);
		}

		String name3d = ProjectSettings::get_singleton()->get_setting("layer_names/3d_physics/layer_" + itos(i));
		if (!name3d.is_empty()) {
			Dictionary d;
			d["index"] = i;
			d["name"] = name3d;
			layers3d.push_back(d);
		}
	}
	res["layers_2d"] = layers2d;
	res["layers_3d"] = layers3d;
	return res;
}

Dictionary JustAMCPProjectTools::get_input_actions(const Dictionary &p_args) {
	Dictionary actions;
	if (!InputMap::get_singleton()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "InputMap singleton is unavailable.";
		return ret;
	}

	bool include_builtin = p_args.get("include_builtin", false);
	List<StringName> action_names = InputMap::get_singleton()->get_actions();
	for (const StringName &action_name : action_names) {
		String action = action_name;
		if (!include_builtin && action.begins_with("ui_")) {
			continue;
		}
		Array events;
		const List<Ref<InputEvent>> *action_events = InputMap::get_singleton()->action_get_events(action_name);
		if (action_events) {
			for (const Ref<InputEvent> &event : *action_events) {
				if (event.is_valid()) {
					events.push_back(event->as_text());
				}
			}
		}
		Dictionary info;
		info["deadzone"] = InputMap::get_singleton()->action_get_deadzone(action_name);
		info["events"] = events;
		info["event_count"] = events.size();
		actions[action] = info;
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["actions"] = actions;
	ret["count"] = actions.size();
	return ret;
}

static Ref<InputEvent> _input_event_from_dictionary(const Dictionary &p_dict) {
	String type = p_dict.get("type", "");
	if (type.is_empty()) {
		String device = p_dict.get("device", "");
		if (device == "mouse" || p_dict.has("button_index")) {
			type = "InputEventMouseButton";
		} else if (p_dict.has("keycode") || p_dict.has("physical_keycode")) {
			type = "InputEventKey";
		}
	}
	if (type.contains("MouseButton") || type == "mouse_button") {
		Ref<InputEventMouseButton> ev;
		ev.instantiate();
		if (p_dict.has("button_index")) {
			ev->set_button_index((MouseButton)(int)p_dict["button_index"]);
		}
		if (p_dict.has("pressed")) {
			ev->set_pressed(p_dict["pressed"]);
		}
		return ev;
	}
	if (type.contains("MouseMotion") || type == "mouse_motion") {
		Ref<InputEventMouseMotion> ev;
		ev.instantiate();
		return ev;
	}
	if (type.contains("Key") || type == "key") {
		Ref<InputEventKey> ev;
		ev.instantiate();
		if (p_dict.has("keycode")) {
			ev->set_keycode((Key)(int)p_dict["keycode"]);
		}
		if (p_dict.has("physical_keycode")) {
			ev->set_physical_keycode((Key)(int)p_dict["physical_keycode"]);
		}
		if (p_dict.has("pressed")) {
			ev->set_pressed(p_dict["pressed"]);
		}
		return ev;
	}
	return Ref<InputEvent>();
}

static Array _parse_input_events_variant(const Variant &p_events) {
	Array out;
	Array source;
	if (p_events.get_type() == Variant::STRING) {
		String json_str = p_events;
		Variant parsed = JSON::parse_string(json_str);
		if (parsed.get_type() == Variant::ARRAY) {
			source = parsed;
		} else if (parsed.get_type() == Variant::DICTIONARY) {
			source.push_back(parsed);
		}
	} else if (p_events.get_type() == Variant::ARRAY) {
		source = p_events;
	} else {
		return out;
	}
	for (int i = 0; i < source.size(); i++) {
		Variant item = source[i];
		if (item.get_type() == Variant::OBJECT) {
			Ref<InputEvent> ev = item;
			if (ev.is_valid()) {
				out.push_back(ev);
			}
			continue;
		}
		if (item.get_type() == Variant::DICTIONARY) {
			Ref<InputEvent> ev = _input_event_from_dictionary(item);
			if (ev.is_valid()) {
				out.push_back(ev);
			}
			continue;
		}
		if (item.get_type() == Variant::STRING) {
			Variant parsed = JSON::parse_string(item);
			if (parsed.get_type() == Variant::DICTIONARY) {
				Ref<InputEvent> ev = _input_event_from_dictionary(parsed);
				if (ev.is_valid()) {
					out.push_back(ev);
				}
			}
		}
	}
	return out;
}

Dictionary JustAMCPProjectTools::set_input_action(const Dictionary &p_args) {
	String action = p_args.get("action", "");
	if (action.is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "action is required.";
		return ret;
	}
	if (!InputMap::get_singleton()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "InputMap singleton is unavailable.";
		return ret;
	}

	float deadzone = p_args.get("deadzone", InputMap::DEFAULT_DEADZONE);
	if (!InputMap::get_singleton()->has_action(action)) {
		InputMap::get_singleton()->add_action(action, deadzone);
	} else {
		InputMap::get_singleton()->action_set_deadzone(action, deadzone);
	}
	if (p_args.get("replace_events", false)) {
		InputMap::get_singleton()->action_erase_events(action);
	}

	Array parsed_events = _parse_input_events_variant(p_args.get("events", Array()));
	for (int i = 0; i < parsed_events.size(); i++) {
		Ref<InputEvent> ev = parsed_events[i];
		if (ev.is_valid()) {
			InputMap::get_singleton()->action_add_event(action, ev);
		}
	}

	// Persist the caller-provided event descriptors so editor/project settings retain the intended binding data.
	Dictionary setting;
	setting["deadzone"] = deadzone;
	if (parsed_events.size() > 0) {
		setting["events"] = parsed_events;
	} else {
		setting["events"] = p_args.get("events", Array());
	}
	ProjectSettings::get_singleton()->set_setting("input/" + action, setting);
	ProjectSettings::get_singleton()->save();

	Dictionary ret;
	ret["ok"] = true;
	ret["action"] = action;
	ret["deadzone"] = deadzone;
	ret["event_descriptors"] = setting["events"];
	return ret;
}

Dictionary JustAMCPProjectTools::remove_input_action(const Dictionary &p_args) {
	String action = p_args.get("action", "");
	if (action.is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "action is required.";
		return ret;
	}
	if (!InputMap::get_singleton()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "InputMap singleton is unavailable.";
		return ret;
	}
	bool existed = InputMap::get_singleton()->has_action(action);
	if (existed) {
		InputMap::get_singleton()->erase_action(action);
	}
	ProjectSettings::get_singleton()->clear("input/" + action);
	ProjectSettings::get_singleton()->save();

	Dictionary ret;
	ret["ok"] = true;
	ret["action"] = action;
	ret["removed"] = existed;
	return ret;
}

String JustAMCPProjectTools::_type_to_string(int p_type_id) {
	return Variant::get_type_name(Variant::Type(p_type_id));
}

Variant JustAMCPProjectTools::_serialize_value(const Variant &p_value) {
	return p_value; // Simplified for now
}

JustAMCPProjectTools::JustAMCPProjectTools() {}
JustAMCPProjectTools::~JustAMCPProjectTools() {}
