/**************************************************************************/
/*  justamcp_gap_fill.cpp                                                 */
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

#ifdef TOOLS_ENABLED

#include "justamcp_gap_fill.h"

#include "../justamcp_editor_scene_access.h"
#include "../justamcp_mcp_tool_macros.h"
#include "../justamcp_play_clock.h"
#include "justamcp_agent_helpers.h"
#include "justamcp_agent_policy.h"

#include "editor/scene/3d/node_3d_editor_viewport.h"

#include "modules/modules_enabled.gen.h"
#ifdef MODULE_GDSCRIPT_ENABLED
#include "modules/gdscript/gdscript.h"
#endif
#ifdef MODULE_LUAU_MODULE_ENABLED
#include "modules/luau_module/luau.h"
#endif

#include "core/config/project_settings.h"
#include "core/crypto/crypto.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/http_client.h"
#include "core/io/json.h"
#include "core/io/pck_packer.h"
#include "core/object/class_db.h"
#include "core/object/script_language.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "editor/editor_data.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/scene/3d/node_3d_editor_plugin.h"
#include "editor/script/script_editor_plugin.h"
#include "editor/settings/editor_command_palette.h"
#include "editor/settings/editor_settings.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/visual_instance_3d.h"
#include "scene/gui/dialogs.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "scene/resources/material.h"

#ifdef MODULE_ZIP_ENABLED
#include "modules/zip/zip_reader.h"
#endif

static Dictionary _err(const String &p_message) {
	Dictionary result;
	result["ok"] = false;
	result["error"] = p_message;
	return result;
}

static Vector3 _vector3(const Variant &p_value, const Vector3 &p_default) {
	if (p_value.get_type() == Variant::VECTOR3) {
		return p_value;
	}
	if (p_value.get_type() == Variant::DICTIONARY) {
		Dictionary d = p_value;
		return Vector3(d.get("x", p_default.x), d.get("y", p_default.y), d.get("z", p_default.z));
	}
	if (p_value.get_type() == Variant::ARRAY) {
		Array arr = p_value;
		if (arr.size() >= 3) {
			return Vector3(arr[0], arr[1], arr[2]);
		}
	}
	return p_default;
}

static SubViewport *_editor_viewport_3d(int p_index) {
	Node3DEditor *editor = Node3DEditor::get_singleton();
	if (!editor) {
		return nullptr;
	}
	const int index = CLAMP(p_index, 0, static_cast<int>(Node3DEditor::VIEWPORTS_COUNT) - 1);
	Node3DEditorViewport *viewport = editor->get_editor_viewport(index);
	if (!viewport) {
		return nullptr;
	}
	return viewport->get_viewport_node();
}

static void _collect_dialogs(Node *p_node, Window *p_skip, Array &r_dialogs, int p_limit, int p_depth) {
	if (!p_node || r_dialogs.size() >= p_limit || p_depth > 8) {
		return;
	}
	Window *window = Object::cast_to<Window>(p_node);
	if (window && window != p_skip && window->is_visible() && !String(window->get_title()).is_empty()) {
		Dictionary info;
		info["title"] = String(window->get_title());
		info["class"] = window->get_class();
		r_dialogs.push_back(info);
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		_collect_dialogs(p_node->get_child(i), p_skip, r_dialogs, p_limit, p_depth + 1);
	}
}

static bool _contained_in(const String &p_absolute, const String &p_root) {
	String absolute = p_absolute.simplify_path().replace("\\", "/");
	String root = p_root.simplify_path().replace("\\", "/");
	if (root.ends_with("/")) {
		root = root.trim_suffix("/");
	}
	return absolute == root || absolute.begins_with(root + "/");
}

static bool _sandbox_absolute(const String &p_path, String &r_absolute, String &r_error) {
	String canonical;
	if (!justamcp_canonical_sandbox_path(p_path, canonical, r_error)) {
		return false;
	}
	ProjectSettings *ps = ProjectSettings::get_singleton();
	if (!ps) {
		r_error = "Project settings are unavailable.";
		return false;
	}
	const String absolute = ps->globalize_path(canonical);
	const String root = canonical.begins_with("user://") ? ps->globalize_path("user://") : ps->get_resource_path();
	if (!_contained_in(absolute, root)) {
		r_error = "Path is outside the open project.";
		return false;
	}
	r_absolute = absolute.simplify_path();
	return true;
}

static String _url_host(const String &p_url) {
	String rest = p_url.strip_edges();
	if (rest.begins_with("https://")) {
		rest = rest.trim_prefix("https://");
	} else if (rest.begins_with("http://")) {
		rest = rest.trim_prefix("http://");
	} else {
		return String();
	}
	const int slash = rest.find("/");
	String host = slash >= 0 ? rest.substr(0, slash) : rest;
	const int colon = host.rfind(":");
	if (colon > 0) {
		host = host.substr(0, colon);
	}
	return host.to_lower();
}

static bool _headless_source_allowed(const String &p_source, String &r_error) {
	String first_code;
	const PackedStringArray lines = p_source.split("\n");
	for (int i = 0; i < lines.size(); i++) {
		const String line = lines[i].strip_edges();
		if (line.is_empty() || line.begins_with("#")) {
			continue;
		}
		first_code = line;
		break;
	}
	if (!first_code.begins_with("extends SceneTree")) {
		r_error = "Headless source must start with extends SceneTree.";
		return false;
	}
	const String lower = p_source.to_lower();
	const char *blocked[] = { "os.execute", "os.create_process", "os.shell_open", "os.kill", "classdb.", nullptr };
	for (int i = 0; blocked[i] != nullptr; i++) {
		if (lower.contains(blocked[i])) {
			r_error = String("Headless source refuses ") + blocked[i] + ".";
			return false;
		}
	}
	return true;
}

static String _asset_library_base() {
	if (!EditorSettings::get_singleton() || !EditorSettings::get_singleton()->has_setting("asset_library/available_urls")) {
		return String();
	}
	Variant stored = EditorSettings::get_singleton()->get_setting("asset_library/available_urls");
	if (stored.get_type() != Variant::DICTIONARY) {
		return String();
	}
	Dictionary urls = stored;
	Array keys = urls.keys();
	if (keys.is_empty()) {
		return String();
	}
	return String(urls[keys[0]]).trim_suffix("/");
}

static Error _http_get(const String &p_url, int p_timeout_ms, int p_max_bytes, PackedByteArray &r_body, String &r_error) {
	String url = p_url.trim_prefix("https://").trim_prefix("http://");
	const bool tls = p_url.begins_with("https://");
	const int slash = url.find("/");
	String host = slash >= 0 ? url.substr(0, slash) : url;
	String path = slash >= 0 ? url.substr(slash) : "/";
	int port = tls ? 443 : 80;
	const int colon = host.rfind(":");
	if (colon > 0 && host.substr(colon + 1).is_valid_int()) {
		port = host.substr(colon + 1).to_int();
		host = host.substr(0, colon);
	}
	HTTPClient *http = HTTPClient::create();
	if (!http) {
		r_error = "HTTP client is unavailable.";
		return ERR_CANT_CREATE;
	}
	struct HTTPClientDelete {
		HTTPClient *client = nullptr;
		~HTTPClientDelete() {
			if (client) {
				memdelete(client);
			}
		}
	} http_delete{ http };
	Ref<TLSOptions> tls_options;
	if (tls) {
		tls_options = TLSOptions::client();
	}
	Error err = http->connect_to_host(host, port, tls_options);
	if (err != OK) {
		r_error = "Asset Library connection failed.";
		return err;
	}
	const uint64_t deadline = OS::get_singleton()->get_ticks_msec() + uint64_t(CLAMP(p_timeout_ms, 1000, 20000));
	while (http->get_status() == HTTPClient::STATUS_CONNECTING || http->get_status() == HTTPClient::STATUS_RESOLVING) {
		if (OS::get_singleton()->get_ticks_msec() > deadline) {
			r_error = "Asset Library connection timed out.";
			return ERR_TIMEOUT;
		}
		http->poll();
		OS::get_singleton()->delay_usec(1000);
	}
	if (http->get_status() != HTTPClient::STATUS_CONNECTED) {
		r_error = "Asset Library connection failed.";
		return FAILED;
	}
	err = http->request(HTTPClient::METHOD_GET, path, Vector<String>(), nullptr, 0);
	if (err != OK) {
		r_error = "Asset Library request failed.";
		return err;
	}
	while (http->get_status() == HTTPClient::STATUS_REQUESTING) {
		if (OS::get_singleton()->get_ticks_msec() > deadline) {
			r_error = "Asset Library request timed out.";
			return ERR_TIMEOUT;
		}
		http->poll();
		OS::get_singleton()->delay_usec(1000);
	}
	if (http->get_status() != HTTPClient::STATUS_BODY && http->has_response()) {
		if (http->get_response_code() >= 400) {
			r_error = "Asset Library returned HTTP " + String::num_int64(http->get_response_code());
			return FAILED;
		}
	}
	while (http->get_status() == HTTPClient::STATUS_BODY) {
		if (OS::get_singleton()->get_ticks_msec() > deadline) {
			r_error = "Asset Library response timed out.";
			return ERR_TIMEOUT;
		}
		http->poll();
		PackedByteArray chunk = http->read_response_body_chunk();
		if (r_body.size() + chunk.size() > p_max_bytes) {
			r_error = "Asset Library response exceeded the size cap.";
			return ERR_OUT_OF_MEMORY;
		}
		r_body.append_array(chunk);
		OS::get_singleton()->delay_usec(1000);
	}
	return OK;
}

static Dictionary _asset_get_json(const String &p_path) {
	const String base = _asset_library_base();
	if (base.is_empty()) {
		return _err("No Asset Library repository is configured in Editor Settings.");
	}
	PackedByteArray body;
	String error;
	if (_http_get(base + p_path, 8000, 2 * 1024 * 1024, body, error) != OK) {
		return _err(error.is_empty() ? String("Asset Library request failed.") : error);
	}
	Variant parsed = JSON::parse_string(String::utf8((const char *)body.ptr(), body.size()));
	if (parsed.get_type() != Variant::DICTIONARY) {
		return _err("Asset Library response was not JSON.");
	}
	Dictionary result;
	result["ok"] = true;
	result["data"] = Dictionary(parsed);
	return result;
}

bool justamcp_gdscript_source_compiles(const String &p_source, String &r_error) {
#ifdef MODULE_GDSCRIPT_ENABLED
	Object *obj = ClassDB::instantiate("GDScript");
	if (!obj) {
		r_error = "GDScript is not available.";
		return false;
	}
	Ref<Script> script = Object::cast_to<Script>(obj);
	if (script.is_null()) {
		memdelete(obj);
		r_error = "GDScript is not available.";
		return false;
	}
	script->set_source_code(p_source);
	const Error err = script->reload(false);
	if (err != OK) {
		r_error = "Compilation failed (" + String::num_int64(err) + ").";
		return false;
	}
	return true;
#else
	r_error = "GDScript is not compiled in.";
	return false;
#endif
}

bool justamcp_script_write_requires_validate(const String &p_path, const Dictionary &p_params) {
	if (!p_path.ends_with(".gd")) {
		return false;
	}
	if (p_params.has("validate") && !bool(p_params["validate"])) {
		return false;
	}
	return true;
}

static bool _bare_line_prefix(const String &p_source, const String &p_prefix) {
	const Vector<String> lines = p_source.split("\n");
	for (int i = 0; i < lines.size(); i++) {
		const String line = lines[i].strip_edges();
		if (line.begins_with(p_prefix) && (line.length() == p_prefix.length() || line[p_prefix.length()] == ' ' || line[p_prefix.length()] == '\t')) {
			return true;
		}
	}
	return false;
}

static bool _token_at(const String &p_source, const String &p_token) {
	int from = 0;
	while (true) {
		const int at = p_source.find(p_token, from);
		if (at < 0) {
			return false;
		}
		const bool left_ok = at == 0 || !(is_ascii_alphanumeric_char(p_source[at - 1]) || p_source[at - 1] == '_');
		const int end = at + p_token.length();
		const bool right_ok = end >= p_source.length() || !(is_ascii_alphanumeric_char(p_source[end]) || p_source[end] == '_');
		if (left_ok && right_ok) {
			return true;
		}
		from = at + 1;
	}
}

static String _godot3_hint(const String &p_source) {
	if (p_source.contains("yield(")) {
		return "yield() is Godot 3. Use await.";
	}
	if (_bare_line_prefix(p_source, "export var")) {
		return "export var is Godot 3. Use @export var.";
	}
	if (_bare_line_prefix(p_source, "onready var")) {
		return "onready var is Godot 3. Use @onready var.";
	}
	if (p_source.contains(".instance(")) {
		return "PackedScene.instance() is Godot 3. Use instantiate().";
	}
	if (_token_at(p_source, "KinematicBody2D") || _token_at(p_source, "KinematicBody")) {
		return "KinematicBody is Godot 3. Use CharacterBody2D or CharacterBody3D.";
	}
	if (_token_at(p_source, "Spatial")) {
		return "Spatial is Godot 3. Use Node3D.";
	}
	if (_token_at(p_source, "Sprite")) {
		return "Sprite is Godot 3. Use Sprite2D or Sprite3D.";
	}
	if (p_source.contains("File.new(") || p_source.contains("Directory.new(")) {
		return "File.new() and Directory.new() are Godot 3. Use FileAccess and DirAccess.";
	}
	const char *pools[] = { "PoolByteArray", "PoolIntArray", "PoolRealArray", "PoolStringArray", "PoolVector2Array", "PoolVector3Array", "PoolColorArray", nullptr };
	for (int i = 0; pools[i]; i++) {
		if (_token_at(p_source, pools[i])) {
			return String(pools[i]) + " is Godot 3. Use the Packed*Array equivalent.";
		}
	}
	const int connect_at = p_source.find("connect(\"");
	if (connect_at >= 0) {
		const String window = p_source.substr(connect_at, 96);
		if (window.contains(", self,")) {
			return "connect(\"name\", self, \"_method\") is Godot 3. Use signal_name.connect(_method).";
		}
	}
	return String();
}

#ifndef MODULE_LUAU_MODULE_ENABLED
static int _count_token(const String &p_source, const String &p_token) {
	int count = 0;
	int from = 0;
	while (true) {
		const int at = p_source.find(p_token, from);
		if (at < 0) {
			return count;
		}
		const bool left_ok = at == 0 || !(is_ascii_alphanumeric_char(p_source[at - 1]) || p_source[at - 1] == '_');
		const int end = at + p_token.length();
		const bool right_ok = end >= p_source.length() || !(is_ascii_alphanumeric_char(p_source[end]) || p_source[end] == '_');
		if (left_ok && right_ok) {
			count++;
		}
		from = at + 1;
	}
}
#endif

Dictionary justamcp_guard_gdscript_write(const String &p_path, const String &p_content, const Dictionary &p_params) {
	const String ext = p_path.get_extension().to_lower();
	if ((ext == "luau" || ext == "lua") && !(p_params.has("validate") && !bool(p_params["validate"]))) {
#ifdef MODULE_LUAU_MODULE_ENABLED
		const luau_module::LuauCompileResult compiled = luau_module::Luau::compile_with_diagnostics(p_content);
		if (!compiled.succeeded()) {
			String message = compiled.error_message.is_empty() ? String("Luau parse error.") : compiled.error_message;
			if (compiled.error_line >= 0) {
				message = "Luau parse error: line " + String::num_int64(compiled.error_line) + ": " + message;
			} else if (!message.begins_with("Luau")) {
				message = "Luau parse error: " + message;
			}
			return _err(message);
		}
#else
		const int functions = _count_token(p_content, "function");
		const int ends = _count_token(p_content, "end");
		int paren = 0;
		for (int i = 0; i < p_content.length(); i++) {
			if (p_content[i] == '(') {
				paren++;
			} else if (p_content[i] == ')') {
				paren--;
			}
		}
		if (functions != ends || paren != 0) {
			return _err("Luau parse error: unbalanced function/end or parentheses.");
		}
#endif
	}
	if (!justamcp_script_write_requires_validate(p_path, p_params)) {
		return Dictionary();
	}
	const String hint = _godot3_hint(p_content);
	if (!hint.is_empty()) {
		return _err(hint);
	}
	String error;
	if (!justamcp_gdscript_source_compiles(p_content, error)) {
		return _err("GDScript validation failed: " + error);
	}
	return Dictionary();
}

static String _quoted_ids(const String &p_text, const String &p_header) {
	Vector<String> ids;
	const Vector<String> lines = p_text.split("\n");
	for (int i = 0; i < lines.size(); i++) {
		const String line = lines[i].strip_edges();
		if (!line.begins_with(p_header)) {
			continue;
		}
		const int id_at = line.find("id=\"");
		if (id_at < 0) {
			continue;
		}
		const int start = id_at + 4;
		const int end = line.find("\"", start);
		if (end > start) {
			ids.push_back(line.substr(start, end - start));
		}
	}
	ids.sort();
	return String(",").join(ids);
}

static String _uid_tokens(const String &p_text) {
	Vector<String> uids;
	int from = 0;
	while (true) {
		const int at = p_text.find("uid://", from);
		if (at < 0) {
			break;
		}
		int end = at + 6;
		while (end < p_text.length()) {
			const char32_t c = p_text[end];
			if (!is_ascii_alphanumeric_char(c) && c != '_') {
				break;
			}
			end++;
		}
		uids.push_back(p_text.substr(at, end - at));
		from = end;
	}
	uids.sort();
	return String(",").join(uids);
}

static String _load_steps(const String &p_text) {
	const int at = p_text.find("load_steps=");
	if (at < 0) {
		return String();
	}
	int end = at + 11;
	while (end < p_text.length() && is_digit(p_text[end])) {
		end++;
	}
	return p_text.substr(at, end - at);
}

static bool _scene_path(const String &p_path) {
	const String ext = p_path.get_extension().to_lower();
	return ext == "tscn" || ext == "tres";
}

Dictionary justamcp_guard_scene_text(const String &p_path, const String &p_previous, const String &p_next) {
	if (!_scene_path(p_path)) {
		return Dictionary();
	}
	const bool structural = p_next.contains("uid://") || p_next.contains("load_steps=") || p_next.contains("[ext_resource") || p_next.contains("[sub_resource");
	if (p_previous.is_empty()) {
		if (structural) {
			return _err("Refusing to invent uid://, load_steps, or ext_resource/sub_resource ids in " + p_path + ". Use scene tools for scene structure. Scalar properties and [connection] lines can be written as text.");
		}
		return Dictionary();
	}
	const String prev_uid = _uid_tokens(p_previous);
	const String next_uid = _uid_tokens(p_next);
	if (prev_uid != next_uid) {
		return _err("Refusing to change uid:// values in " + p_path + ". Use scene tools for scene structure.");
	}
	if (_load_steps(p_previous) != _load_steps(p_next)) {
		return _err("Refusing to change load_steps in " + p_path + ". Use scene tools for scene structure.");
	}
	if (_quoted_ids(p_previous, "[ext_resource") != _quoted_ids(p_next, "[ext_resource") || _quoted_ids(p_previous, "[sub_resource") != _quoted_ids(p_next, "[sub_resource")) {
		return _err("Refusing to change ext_resource or sub_resource ids in " + p_path + ". Use scene tools for scene structure.");
	}
	return Dictionary();
}

int justamcp_export_smoke_timeout_ms(const Dictionary &p_args) {
	const int requested = int(p_args.get("timeout_ms", 8000));
	return CLAMP(requested, 1000, 30000);
}

Dictionary justamcp_editor_get_camera(const Dictionary &p_args) {
	SubViewport *viewport = _editor_viewport_3d(int(p_args.get("index", 0)));
	Camera3D *camera = viewport ? viewport->get_camera_3d() : nullptr;
	if (!camera) {
		return _err("3D editor camera is unavailable.");
	}
	Dictionary result;
	result["ok"] = true;
	result["position"] = camera->get_global_position();
	result["rotation"] = camera->get_global_rotation_degrees();
	result["fov"] = camera->get_fov();
	result["current"] = camera->is_current();
	return result;
}

Dictionary justamcp_editor_set_camera(const Dictionary &p_args) {
	SubViewport *viewport = _editor_viewport_3d(int(p_args.get("index", 0)));
	Camera3D *camera = viewport ? viewport->get_camera_3d() : nullptr;
	if (!camera) {
		return _err("3D editor camera is unavailable.");
	}
	if (p_args.has("position")) {
		camera->set_global_position(_vector3(p_args["position"], camera->get_global_position()));
	}
	if (p_args.has("rotation")) {
		camera->set_global_rotation_degrees(_vector3(p_args["rotation"], camera->get_global_rotation_degrees()));
	}
	if (p_args.has("fov")) {
		camera->set_fov(float(p_args["fov"]));
	}
	return justamcp_editor_get_camera(p_args);
}

Dictionary justamcp_editor_list_dialogs(const Dictionary &p_args) {
	(void)p_args;
	if (!EditorNode::get_singleton()) {
		return _err("Editor interface is unavailable.");
	}
	Control *base = EditorNode::get_singleton()->get_gui_base();
	Array dialogs;
	if (base && base->is_inside_tree()) {
		_collect_dialogs(base, base->get_window(), dialogs, 32, 0);
	}
	Dictionary result;
	result["ok"] = true;
	result["dialogs"] = dialogs;
	result["count"] = dialogs.size();
	return result;
}

Dictionary justamcp_editor_dismiss_dialog(const Dictionary &p_args) {
	const String title = String(p_args.get("title", ""));
	if (title.is_empty() || title.length() > 256) {
		return _err("title is required and must be at most 256 characters.");
	}
	Dictionary listed = justamcp_editor_list_dialogs(Dictionary());
	if (!bool(listed.get("ok", false))) {
		return listed;
	}
	if (!EditorNode::get_singleton()) {
		return _err("Editor interface is unavailable.");
	}
	Control *base = EditorNode::get_singleton()->get_gui_base();
	Window *found = nullptr;
	if (base) {
		List<Node *> stack;
		stack.push_back(base);
		while (!stack.is_empty() && !found) {
			Node *node = stack.front()->get();
			stack.pop_front();
			Window *window = Object::cast_to<Window>(node);
			if (window && window != base->get_window() && window->is_visible() && String(window->get_title()) == title) {
				found = window;
				break;
			}
			for (int i = 0; i < node->get_child_count(); i++) {
				stack.push_back(node->get_child(i));
			}
		}
	}
	if (!found) {
		return _err("No visible dialog titled: " + title);
	}
	found->hide();
	Dictionary result;
	result["ok"] = true;
	result["title"] = title;
	result["dismissed"] = true;
	return result;
}

Dictionary justamcp_editor_list_actions(const Dictionary &p_args) {
	EditorCommandPalette *palette = EditorCommandPalette::get_singleton();
	if (!palette) {
		return _err("Editor command palette is unavailable.");
	}
	List<String> names;
	palette->get_actions_list(&names);
	const int limit = CLAMP(int(p_args.get("limit", 200)), 1, 500);
	Array actions;
	for (const String &name : names) {
		if (actions.size() >= limit) {
			break;
		}
		actions.push_back(name);
	}
	Dictionary result;
	result["ok"] = true;
	result["actions"] = actions;
	result["count"] = actions.size();
	return result;
}

Dictionary justamcp_editor_invoke_action(const Dictionary &p_args) {
	const String name = String(p_args.get("name", p_args.get("action", "")));
	if (name.is_empty() || name.length() > 256) {
		return _err("name is required and must be at most 256 characters.");
	}
	EditorCommandPalette *palette = EditorCommandPalette::get_singleton();
	if (!palette) {
		return _err("Editor command palette is unavailable.");
	}
	palette->execute_command(name);
	Dictionary result;
	result["ok"] = true;
	result["name"] = name;
	return result;
}

Dictionary justamcp_editor_unsaved_state(const Dictionary &p_args) {
	(void)p_args;
	Array scenes;
	if (EditorNode::get_singleton()) {
		EditorData &data = EditorNode::get_editor_data();
		for (int i = 0; i < data.get_edited_scene_count(); i++) {
			if (!data.is_scene_changed(i)) {
				continue;
			}
			Dictionary scene;
			scene["path"] = data.get_scene_path(i);
			scene["title"] = data.get_scene_title(i);
			scenes.push_back(scene);
		}
	}
	Array scripts;
	if (EditorInterface::get_singleton() && EditorInterface::get_singleton()->get_script_editor()) {
		PackedStringArray unsaved = EditorInterface::get_singleton()->get_script_editor()->get_unsaved_files();
		for (int i = 0; i < unsaved.size(); i++) {
			scripts.push_back(unsaved[i]);
		}
	}
	Dictionary result;
	result["ok"] = true;
	result["unsaved_scenes"] = scenes;
	result["unsaved_scripts"] = scripts;
	return result;
}

Dictionary justamcp_editor_save_all(const Dictionary &p_args) {
	(void)p_args;
	if (!EditorNode::get_singleton() || !EditorInterface::get_singleton()) {
		return _err("Editor interface is unavailable.");
	}
	EditorInterface::get_singleton()->save_all_scenes();
	if (EditorInterface::get_singleton()->get_script_editor()) {
		EditorInterface::get_singleton()->get_script_editor()->save_all_scripts();
	}
	Dictionary result;
	result["ok"] = true;
	result["message"] = "Open scenes and scripts saved.";
	return result;
}

Dictionary justamcp_editor_surface_snapshot() {
	Dictionary snapshot;
	snapshot["camera"] = justamcp_editor_get_camera(Dictionary());
	snapshot["dialogs"] = justamcp_editor_list_dialogs(Dictionary());
	snapshot["unsaved"] = justamcp_editor_unsaved_state(Dictionary());
	return snapshot;
}

Dictionary justamcp_scene3d_render_probe(const Dictionary &p_args) {
	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return _err("No active scene.");
	}
	const String node_path = String(p_args.get("node_path", "."));
	Node *start = node_path == "." ? root : root->get_node_or_null(NodePath(node_path));
	if (!start) {
		return _err("Node not found: " + node_path);
	}
	Camera3D *camera = nullptr;
	if (SubViewport *viewport = _editor_viewport_3d(0)) {
		camera = viewport->get_camera_3d();
	}
	const int limit = CLAMP(int(p_args.get("limit", 128)), 1, 256);
	Array nodes;
	List<Node *> stack;
	stack.push_back(start);
	int visited = 0;
	bool truncated = false;
	while (!stack.is_empty()) {
		if (nodes.size() >= limit) {
			truncated = true;
			break;
		}
		Node *node = stack.front()->get();
		stack.pop_front();
		visited++;
		if (Node3D *node3d = Object::cast_to<Node3D>(node)) {
			Dictionary info;
			info["path"] = String(node3d->get_path());
			info["visible"] = node3d->is_visible();
			info["visible_in_tree"] = node3d->is_visible_in_tree();
			uint32_t layers = 1;
			if (VisualInstance3D *visual = Object::cast_to<VisualInstance3D>(node3d)) {
				layers = visual->get_layer_mask();
			}
			info["layers"] = layers;
			bool in_frustum = false;
			bool cull_mask_match = false;
			if (camera) {
				in_frustum = camera->is_position_in_frustum(node3d->get_global_position());
				cull_mask_match = (camera->get_cull_mask() & layers) != 0;
			}
			info["in_frustum"] = in_frustum;
			info["cull_mask_match"] = cull_mask_match;
			String material_cull = "";
			if (MeshInstance3D *mesh = Object::cast_to<MeshInstance3D>(node3d)) {
				Ref<BaseMaterial3D> material = mesh->get_active_material(0);
				if (material.is_valid()) {
					switch (material->get_cull_mode()) {
						case BaseMaterial3D::CULL_BACK:
							material_cull = "back";
							break;
						case BaseMaterial3D::CULL_FRONT:
							material_cull = "front";
							break;
						case BaseMaterial3D::CULL_DISABLED:
							material_cull = "disabled";
							break;
						default:
							break;
					}
				}
			}
			info["material_cull"] = material_cull;
			nodes.push_back(info);
		}
		if (visited >= 4096) {
			truncated = node->get_child_count() > 0 || !stack.is_empty();
			break;
		}
		for (int i = 0; i < node->get_child_count(); i++) {
			stack.push_back(node->get_child(i));
		}
	}
	Dictionary result;
	result["ok"] = true;
	result["nodes"] = nodes;
	result["count"] = nodes.size();
	result["has_camera"] = camera != nullptr;
	result["truncated"] = truncated;
	return result;
}

Dictionary justamcp_scene3d_set_debug_draw(const Dictionary &p_args) {
	SubViewport *viewport = _editor_viewport_3d(int(p_args.get("index", 0)));
	if (!viewport) {
		return _err("3D editor viewport is unavailable.");
	}
	const String mode = String(p_args.get("mode", "")).to_lower();
	Viewport::DebugDraw draw = Viewport::DEBUG_DRAW_DISABLED;
	bool known = true;
	if (mode == "disabled") {
		draw = Viewport::DEBUG_DRAW_DISABLED;
	} else if (mode == "unshaded") {
		draw = Viewport::DEBUG_DRAW_UNSHADED;
	} else if (mode == "lighting") {
		draw = Viewport::DEBUG_DRAW_LIGHTING;
	} else if (mode == "overdraw") {
		draw = Viewport::DEBUG_DRAW_OVERDRAW;
	} else if (mode == "wireframe") {
		draw = Viewport::DEBUG_DRAW_WIREFRAME;
	} else {
		known = false;
	}
	if (!known) {
		return _err("Unknown debug draw mode. Use disabled, unshaded, lighting, overdraw, or wireframe.");
	}
	viewport->set_debug_draw(draw);
	Dictionary result;
	result["ok"] = true;
	result["mode"] = mode;
	return result;
}

Dictionary justamcp_spatial_snap_to_surface(const Dictionary &p_args) {
	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return _err("No active scene.");
	}
	const String node_path = String(p_args.get("node_path", ""));
	Node3D *node = Object::cast_to<Node3D>(node_path == "." ? root : root->get_node_or_null(NodePath(node_path)));
	if (!node) {
		return _err("Node3D not found: " + node_path);
	}
	const float offset = float(p_args.get("offset", 0.0));
	const String target_path = String(p_args.get("target_path", ""));
	Vector3 snapped = node->get_global_position();
	bool found = false;
	if (!target_path.is_empty()) {
		if (VisualInstance3D *target = Object::cast_to<VisualInstance3D>(root->get_node_or_null(NodePath(target_path)))) {
			const AABB aabb = target->get_global_transform().xform(target->get_aabb());
			snapped.y = aabb.position.y + aabb.size.y + offset;
			snapped.x = aabb.get_center().x;
			snapped.z = aabb.get_center().z;
			found = true;
		}
	} else {
		const float max_distance = float(p_args.get("max_distance", 100.0));
		float best = max_distance;
		List<Node *> stack;
		stack.push_back(root);
		int visited = 0;
		while (!stack.is_empty() && visited < 512) {
			visited++;
			Node *cursor = stack.front()->get();
			stack.pop_front();
			if (cursor != node) {
				if (VisualInstance3D *visual = Object::cast_to<VisualInstance3D>(cursor)) {
					const AABB aabb = visual->get_global_transform().xform(visual->get_aabb());
					const float top = aabb.position.y + aabb.size.y;
					const float drop = node->get_global_position().y - top;
					const Vector2 flat(node->get_global_position().x - aabb.get_center().x, node->get_global_position().z - aabb.get_center().z);
					if (drop >= 0.0 && drop < best && flat.length() < MAX(aabb.size.x, aabb.size.z) + 0.5) {
						best = drop;
						snapped = Vector3(node->get_global_position().x, top + offset, node->get_global_position().z);
						found = true;
					}
				}
			}
			for (int i = 0; i < cursor->get_child_count(); i++) {
				stack.push_back(cursor->get_child(i));
			}
		}
	}
	if (!found) {
		return _err("No surface found to snap onto.");
	}
	node->set_global_position(snapped);
	Dictionary result;
	result["ok"] = true;
	result["position"] = snapped;
	return result;
}

Dictionary justamcp_spatial_repeat_along(const Dictionary &p_args) {
	const int count = int(p_args.get("count", 0));
	if (count < 1 || count > 32) {
		return _err("count must be from 1 to 32.");
	}
	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return _err("No active scene.");
	}
	const String node_path = String(p_args.get("node_path", ""));
	Node *node = node_path == "." ? root : root->get_node_or_null(NodePath(node_path));
	Node3D *node3d = Object::cast_to<Node3D>(node);
	if (!node || !node->get_parent()) {
		return _err("Node not found: " + node_path);
	}
	const Vector3 from = _vector3(p_args.get("from", node3d ? Variant(node3d->get_global_position()) : Variant()), Vector3());
	const Vector3 to = _vector3(p_args.get("to", from), from);
	Array created;
	for (int i = 0; i < count; i++) {
		const float t = count == 1 ? 1.0f : float(i + 1) / float(count);
		Node *copy = node->duplicate();
		copy->set_name(String(node->get_name()) + "_" + String::num_int64(i + 1));
		JustAMCPEditorSceneAccess::add_child_with_undo(copy, node->get_parent(), root, "Repeat Node");
		if (Node3D *copy3d = Object::cast_to<Node3D>(copy)) {
			copy3d->set_global_position(from.lerp(to, t));
		}
		created.push_back(String(copy->get_path()));
	}
	Dictionary result;
	result["ok"] = true;
	result["created"] = created;
	result["count"] = created.size();
	return result;
}

Dictionary justamcp_export_patch_pck(const Dictionary &p_args) {
	const String output = String(p_args.get("output", ""));
	if (output.is_empty()) {
		return _err("output is required.");
	}
	String path_error;
	String absolute_output;
	if (!_sandbox_absolute(output, absolute_output, path_error)) {
		return _err(path_error.is_empty() ? String("output must stay inside the project or user://.") : path_error);
	}
	Vector<String> files;
	if (p_args.has("files") && p_args["files"].get_type() == Variant::ARRAY) {
		Array listed = p_args["files"];
		for (int i = 0; i < listed.size() && files.size() < 2000; i++) {
			files.push_back(String(listed[i]));
		}
	} else if (p_args.has("base_pack")) {
		const String base = String(p_args["base_pack"]);
		String base_abs;
		if (!_sandbox_absolute(base, base_abs, path_error)) {
			return _err(path_error.is_empty() ? String("base_pack must stay inside the project or user://.") : path_error);
		}
		if (!FileAccess::exists(base_abs)) {
			return _err("Base pack was not found.");
		}
		const uint64_t base_time = FileAccess::get_modified_time(base_abs);
		if (DirAccess::open("res://").is_null()) {
			return _err("Cannot read the project directory.");
		}
		Vector<String> pending;
		pending.push_back("res://");
		while (!pending.is_empty() && files.size() < 2000) {
			const String current = pending[pending.size() - 1];
			pending.remove_at(pending.size() - 1);
			Ref<DirAccess> cursor = DirAccess::open(current);
			if (cursor.is_null()) {
				continue;
			}
			cursor->list_dir_begin();
			for (String name = cursor->get_next(); !name.is_empty(); name = cursor->get_next()) {
				if (name == "." || name == ".." || name.begins_with(".")) {
					continue;
				}
				const String child = current.path_join(name);
				if (cursor->current_is_dir()) {
					pending.push_back(child);
				} else if (FileAccess::get_modified_time(child) > base_time) {
					files.push_back(child);
				}
			}
		}
	} else {
		return _err("Provide files or base_pack.");
	}
	Vector<String> sources;
	Vector<String> targets;
	sources.resize(files.size());
	targets.resize(files.size());
	for (int i = 0; i < files.size(); i++) {
		String source;
		String target;
		String file_error;
		if (!_sandbox_absolute(files[i], source, file_error)) {
			return _err(file_error.is_empty() ? String("Packed file is outside the project.") : file_error);
		}
		if (!justamcp_canonical_sandbox_path(files[i], target, file_error)) {
			return _err(file_error);
		}
		sources.write[i] = source;
		targets.write[i] = target;
	}
	Ref<PCKPacker> packer;
	packer.instantiate();
	if (packer->pck_start(absolute_output) != OK) {
		return _err("Could not start the patch pack.");
	}
	Array packed;
	for (int i = 0; i < files.size(); i++) {
		if (!FileAccess::exists(sources[i])) {
			continue;
		}
		if (packer->add_file(targets[i], sources[i]) == OK) {
			packed.push_back(targets[i]);
		}
	}
	if (packer->flush() != OK) {
		return _err("Could not flush the patch pack.");
	}
	Dictionary result;
	result["ok"] = true;
	result["output"] = output;
	result["files"] = packed;
	result["count"] = packed.size();
	return result;
}

Dictionary justamcp_asset_lib_search(const Dictionary &p_args) {
	const String query = String(p_args.get("query", p_args.get("filter", "")));
	const int max_results = CLAMP(int(p_args.get("max_results", 20)), 1, 50);
	Dictionary fetched = _asset_get_json("/asset?filter=" + query.uri_encode() + "&max_results=" + String::num_int64(max_results));
	if (!bool(fetched.get("ok", false))) {
		return fetched;
	}
	Dictionary data = fetched["data"];
	Array raw = data.get("result", Array());
	Array assets;
	for (int i = 0; i < raw.size() && assets.size() < max_results; i++) {
		if (raw[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary item = raw[i];
		Dictionary brief;
		brief["asset_id"] = item.get("asset_id", 0);
		brief["title"] = item.get("title", "");
		brief["author"] = item.get("author", "");
		brief["category"] = item.get("category", "");
		assets.push_back(brief);
	}
	Dictionary result;
	result["ok"] = true;
	result["assets"] = assets;
	result["count"] = assets.size();
	return result;
}

Dictionary justamcp_asset_lib_info(const Dictionary &p_args) {
	const int asset_id = int(p_args.get("asset_id", p_args.get("id", 0)));
	if (asset_id <= 0) {
		return _err("asset_id is required.");
	}
	return _asset_get_json("/asset/" + String::num_int64(asset_id));
}

Dictionary justamcp_asset_lib_install(const Dictionary &p_args) {
#ifndef MODULE_ZIP_ENABLED
	(void)p_args;
	return _err("Zip support is not compiled in.");
#else
	const int asset_id = int(p_args.get("asset_id", p_args.get("id", 0)));
	if (asset_id <= 0) {
		return _err("asset_id is required.");
	}
	Dictionary info = justamcp_asset_lib_info(p_args);
	if (!bool(info.get("ok", false))) {
		return info;
	}
	Dictionary data = info.get("data", Dictionary());
	const String download_url = String(data.get("download_url", ""));
	if (download_url.is_empty()) {
		return _err("Asset Library entry has no download URL.");
	}
	const String library_host = _url_host(_asset_library_base());
	const String download_host = _url_host(download_url);
	if (library_host.is_empty() || download_host != library_host) {
		return _err("Asset download host does not match the configured repository.");
	}
	String destination = String(p_args.get("destination", "res://addons/asset_" + String::num_int64(asset_id)));
	if (!destination.begins_with("res://")) {
		return _err("destination must be a res:// path inside the open project.");
	}
	String path_error;
	String absolute_dest;
	if (!_sandbox_absolute(destination, absolute_dest, path_error)) {
		return _err(path_error);
	}
	PackedByteArray zip_bytes;
	String error;
	if (_http_get(download_url, 20000, 32 * 1024 * 1024, zip_bytes, error) != OK) {
		return _err(error.is_empty() ? String("Asset download failed.") : error);
	}
	const String zip_path = ProjectSettings::get_singleton()->globalize_path("user://justamcp_asset_lib_" + String::num_int64(asset_id) + ".zip");
	Ref<FileAccess> zip_file = FileAccess::open(zip_path, FileAccess::WRITE);
	if (zip_file.is_null()) {
		return _err("Could not store the asset download.");
	}
	zip_file->store_buffer(zip_bytes);
	zip_file.unref();
	Ref<ZIPReader> reader;
	reader.instantiate();
	if (reader->open(zip_path) != OK) {
		return _err("Could not read the asset archive.");
	}
	DirAccess::make_dir_recursive_absolute(absolute_dest);
	PackedStringArray entries = reader->get_files();
	Array written;
	for (int i = 0; i < entries.size() && written.size() < 2000; i++) {
		const String entry = entries[i].replace("\\", "/");
		if (entry.contains("..") || entry.begins_with("/") || entry.contains(":")) {
			continue;
		}
		const String target = absolute_dest.path_join(entry).simplify_path();
		if (!_contained_in(target, absolute_dest)) {
			continue;
		}
		if (entry.ends_with("/")) {
			DirAccess::make_dir_recursive_absolute(target);
			continue;
		}
		DirAccess::make_dir_recursive_absolute(target.get_base_dir());
		JustAMCPAgentPolicy::note_file_undo(destination.path_join(entry));
		Ref<FileAccess> out = FileAccess::open(target, FileAccess::WRITE);
		if (out.is_null()) {
			continue;
		}
		out->store_buffer(reader->read_file(entry, true));
		written.push_back(destination.path_join(entry));
	}
	reader->close();
	Dictionary result;
	result["ok"] = true;
	result["asset_id"] = asset_id;
	result["destination"] = destination;
	result["files"] = written;
	result["count"] = written.size();
	return result;
#endif
}

Dictionary justamcp_extract_zip(const Dictionary &p_args) {
#ifndef MODULE_ZIP_ENABLED
	(void)p_args;
	return _err("Zip support is not compiled in.");
#else
	const String zip_path = String(p_args.get("zip_path", ""));
	const String destination = String(p_args.get("destination", ""));
	if (zip_path.is_empty() || destination.is_empty()) {
		return _err("zip_path and destination are required.");
	}
	if (zip_path.contains("..") || destination.contains("..")) {
		return _err("zip_path and destination must stay inside the project.");
	}
	if (!(zip_path.begins_with("res://") || zip_path.begins_with("user://"))) {
		return _err("zip_path must be a res:// or user:// path.");
	}
	if (!(destination.begins_with("res://") || destination.begins_with("user://"))) {
		return _err("destination must be a res:// or user:// path.");
	}
	String zip_error;
	String absolute_zip;
	if (!_sandbox_absolute(zip_path, absolute_zip, zip_error)) {
		return _err(zip_error);
	}
	String dest_error;
	String absolute_dest;
	if (!_sandbox_absolute(destination, absolute_dest, dest_error)) {
		return _err(dest_error);
	}
	if (!FileAccess::exists(absolute_zip)) {
		return _err("Zip file was not found.");
	}
	Ref<ZIPReader> reader;
	reader.instantiate();
	if (reader->open(absolute_zip) != OK) {
		return _err("Could not read the zip archive.");
	}
	DirAccess::make_dir_recursive_absolute(absolute_dest);
	const PackedStringArray entries = reader->get_files();
	Array written;
	for (int i = 0; i < entries.size() && written.size() < 256; i++) {
		const String entry = entries[i].replace("\\", "/");
		if (entry.is_empty() || entry.contains("..") || entry.begins_with("/") || entry.contains(":")) {
			continue;
		}
		const String target = absolute_dest.path_join(entry).simplify_path();
		if (!_contained_in(target, absolute_dest)) {
			continue;
		}
		if (entry.ends_with("/")) {
			DirAccess::make_dir_recursive_absolute(target);
			continue;
		}
		DirAccess::make_dir_recursive_absolute(target.get_base_dir());
		const String project_target = destination.path_join(entry).simplify_path();
		JustAMCPAgentPolicy::note_file_undo(project_target);
		Ref<FileAccess> out = FileAccess::open(target, FileAccess::WRITE);
		if (out.is_null()) {
			continue;
		}
		out->store_buffer(reader->read_file(entry, true));
		written.push_back(project_target);
	}
	reader->close();
	Dictionary result;
	result["ok"] = true;
	result["destination"] = destination;
	result["files"] = written;
	result["count"] = written.size();
	return result;
#endif
}

Dictionary justamcp_remote_control_run_headless_script(const Dictionary &p_args) {
	String script = String(p_args.get("script", p_args.get("path", "")));
	const String source = String(p_args.get("source", ""));
	if (!source.is_empty()) {
		if (source.length() > 65536) {
			return _err("source is capped at 65536 characters.");
		}
		String source_error;
		if (!_headless_source_allowed(source, source_error)) {
			return _err(source_error);
		}
		script = "res://.justamcp/headless_scene_tree.gd";
		String path_error;
		String absolute_script;
		if (!_sandbox_absolute(script, absolute_script, path_error)) {
			return _err(path_error);
		}
		DirAccess::make_dir_recursive_absolute(absolute_script.get_base_dir());
		Ref<FileAccess> file = FileAccess::open(absolute_script, FileAccess::WRITE);
		if (file.is_null()) {
			return _err("Could not write the headless script inside the project.");
		}
		file->store_string(source);
	}
	if (!script.begins_with("res://") || !script.ends_with(".gd")) {
		return _err("script must be a res:// .gd path inside the open project.");
	}
	String path_error;
	String absolute_script;
	if (!_sandbox_absolute(script, absolute_script, path_error)) {
		return _err(path_error);
	}
	if (!FileAccess::exists(absolute_script)) {
		return _err("Headless script was not found.");
	}
	const String text = FileAccess::get_file_as_string(absolute_script);
	String source_error;
	if (!_headless_source_allowed(text, source_error)) {
		return _err(source_error);
	}
	const int timeout_ms = CLAMP(int(p_args.get("timeout_ms", 15000)), 1000, 60000);
	List<String> args;
	args.push_back("--headless");
	args.push_back("--path");
	args.push_back(ProjectSettings::get_singleton()->get_resource_path());
	args.push_back("--script");
	args.push_back(absolute_script);
	ProcessID pid = 0;
	const Error err = OS::get_singleton()->create_process(OS::get_singleton()->get_executable_path(), args, &pid);
	if (err != OK) {
		return _err("Could not start the headless editor process.");
	}
	const uint64_t deadline = OS::get_singleton()->get_ticks_msec() + uint64_t(timeout_ms);
	bool timed_out = false;
	while (OS::get_singleton()->is_process_running(pid)) {
		if (OS::get_singleton()->get_ticks_msec() > deadline) {
			OS::get_singleton()->kill(pid);
			timed_out = true;
			break;
		}
		OS::get_singleton()->delay_usec(50000);
	}
	Dictionary result;
	result["ok"] = !timed_out;
	result["script"] = script;
	result["timed_out"] = timed_out;
	if (timed_out) {
		result["error"] = "Headless script exceeded timeout_ms.";
	}
	return result;
}

#endif
