/**************************************************************************/
/*  justamcp_project_tools_settings.cpp                                   */
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

#include "../justamcp_pagination.h"
#include "justamcp_project_tools.h"

#include "core/config/project_settings.h"
#include "core/input/input_event.h"
#include "core/input/input_map.h"
#include "core/variant/typed_array.h"
#include "core/io/file_access.h"
#include "core/io/json.h"

static bool _justamcp_looks_like_json(const String &p_s) {
	const String t = p_s.strip_edges();
	if (t.is_empty()) {
		return false;
	}
	const char32_t c = t[0];
	if (c == '{' || c == '[' || c == '"') {
		return true;
	}
	if (t == "true" || t == "false" || t == "null") {
		return true;
	}
	if (c == '-' || (c >= '0' && c <= '9')) {
		return t.is_valid_float() || t.is_valid_int();
	}
	return false;
}

static Variant _justamcp_try_parse_json_quiet(const String &p_s) {
	const String t = p_s.strip_edges();
	if (!_justamcp_looks_like_json(t)) {
		return Variant();
	}
	Ref<JSON> json;
	json.instantiate();
	if (json->parse(t) != OK) {
		return Variant();
	}
	return json->get_data();
}

Dictionary JustAMCPProjectTools::list_settings(const Dictionary &p_args) {
	String category = p_args.get("category", "");
	const int max_results = CLAMP(int(p_args.get("max_results", category.is_empty() ? 500 : 2000)), 1, 10000);
	const String cursor = p_args.get("cursor", "");

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
		if (settings.size() >= max_results) {
			break;
		}
	}

	const bool truncated = settings.size() >= max_results;
	Dictionary page = justamcp_pagination_slice_array(settings, cursor, "settings");
	if (page.has("ok") && !bool(page.get("ok", true))) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = page.get("error", "Invalid pagination cursor.");
		err["error_code"] = page.get("error_code", -32602);
		return err;
	}

	Dictionary result;
	result["ok"] = true;
	result["settings"] = page.get("settings", Array());
	result["truncated"] = truncated && !page.has("nextCursor");
	if (page.has("nextCursor")) {
		result["nextCursor"] = page["nextCursor"];
	} else if (truncated) {
		result["truncated"] = true;
	}
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
	TypedArray<StringName> action_names = InputMap::get_singleton()->get_actions();
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
		Variant parsed = _justamcp_try_parse_json_quiet(json_str);
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
			Variant parsed = _justamcp_try_parse_json_quiet(item);
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

Dictionary JustAMCPProjectTools::get_project_info(const Dictionary &p_args) {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	ERR_FAIL_COND_V(!ps, Dictionary());

	Dictionary autoload_args;
	autoload_args["operation"] = "list";
	Dictionary autoload_res = manage_autoloads(autoload_args);

	Dictionary renderer;
	renderer["rendering_method"] = ps->get_setting("rendering/renderer/rendering_method", "forward_plus");
	renderer["anti_aliasing"] = ps->get_setting("rendering/anti_aliasing/quality/msaa_2d", 0);

	Dictionary viewport;
	viewport["viewport_width"] = ps->get_setting("display/window/size/viewport_width", 1152);
	viewport["viewport_height"] = ps->get_setting("display/window/size/viewport_height", 648);
	viewport["window_width_override"] = ps->get_setting("display/window/size/window_width_override", 0);
	viewport["window_height_override"] = ps->get_setting("display/window/size/window_height_override", 0);
	viewport["stretch_mode"] = ps->get_setting("display/window/stretch/mode", "canvas_items");

	Dictionary result;
	result["ok"] = true;
	result["project_name"] = ps->get_setting("application/config/name", "Untitled");
	result["main_scene"] = ps->get_setting("application/run/main_scene", "");
	result["config_version"] = ps->get_setting("config_version", 5);
	result["renderer"] = renderer;
	result["viewport"] = viewport;
	result["autoloads"] = autoload_res.get("autoloads", Array());
	return result;
}

Dictionary JustAMCPProjectTools::set_project_setting(const Dictionary &p_args) {
	String key = p_args.get("key", "");
	if (key.is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "key is required.";
		return ret;
	}

	Variant value = p_args.get("value", Variant());
	if (value.get_type() == Variant::STRING) {
		String s = value;
		Variant parsed = _justamcp_try_parse_json_quiet(s);
		if (parsed.get_type() != Variant::NIL) {
			value = parsed;
		}
	}

	ProjectSettings *ps = ProjectSettings::get_singleton();
	ERR_FAIL_COND_V(!ps, Dictionary());
	ps->set_setting(key, value);
	Error err = ps->save();

	Dictionary ret;
	ret["ok"] = err == OK;
	ret["key"] = key;
	ret["value"] = _serialize_value(value);
	if (err != OK) {
		ret["error"] = "Failed to save project settings.";
	}
	return ret;
}
