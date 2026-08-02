/**************************************************************************/
/*  remote_control_builtins.cpp                                           */
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

#include "remote_control_builtins.h"

#include "remote_control_registry.h"
#include "remote_control_server.h"

#include "core/config/project_settings.h"
#include "core/crypto/crypto_core.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/os/os.h"
#include "scene/gui/control.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "servers/display_server.h"

#ifdef TOOLS_ENABLED
#include "editor/debugger/editor_debugger_node.h"
#include "editor/debugger/script_editor_debugger.h"
#include "editor/editor_file_system.h"
#include "editor/editor_interface.h"
#include "editor/editor_main_screen.h"
#include "editor/editor_node.h"
#include "editor/gui/editor_run_bar.h"
#include "editor/plugins/embedded_process.h"
#include "scene/gui/button.h"
#endif

#include "modules/modules_enabled.gen.h"

#ifdef MODULE_JUSTAMCP_ENABLED
#ifdef TOOLS_ENABLED
#include "modules/justamcp/justamcp_server.h"
#endif
#endif

static Dictionary _ok(const Dictionary &p_extra = Dictionary()) {
	Dictionary ret = p_extra;
	ret["ok"] = true;
	return ret;
}

static Dictionary _err(const String &p_message) {
	Dictionary ret;
	ret["ok"] = false;
	ret["type"] = "error";
	ret["error"] = p_message;
	ret["message"] = p_message;
	return ret;
}

static SceneTree *_scene_tree() {
	return Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
}

static Node *_resolve_node(const String &p_path) {
	SceneTree *tree = _scene_tree();
	if (!tree || !tree->get_root()) {
		return nullptr;
	}
	if (p_path.is_empty() || p_path == "/root" || p_path == ".") {
		return Object::cast_to<Node>(tree->get_root());
	}
	return tree->get_root()->get_node_or_null(NodePath(p_path));
}

static Dictionary _serialize_node_shallow(Node *p_node, int p_depth, int p_max_depth) {
	Dictionary d;
	if (!p_node) {
		return d;
	}
	d["name"] = p_node->get_name();
	d["path"] = String(p_node->get_path());
	d["class"] = p_node->get_class();
	d["child_count"] = p_node->get_child_count();
	if (p_depth < p_max_depth) {
		Array children;
		for (int i = 0; i < p_node->get_child_count(); i++) {
			children.push_back(_serialize_node_shallow(p_node->get_child(i), p_depth + 1, p_max_depth));
		}
		d["children"] = children;
	}
	return d;
}

Dictionary remote_control_cmd_ping(const Dictionary &p_args) {
	Dictionary ret;
	ret["type"] = "pong";
	ret["timestamp"] = OS::get_singleton()->get_unix_time();
	(void)p_args;
	return _ok(ret);
}

Dictionary remote_control_cmd_get_status(const Dictionary &p_args) {
	if (RemoteControlServer::get_singleton()) {
		return _ok(RemoteControlServer::get_singleton()->get_status());
	}
	return _err("RemoteControlServer not available");
}

Dictionary remote_control_cmd_list_commands(const Dictionary &p_args) {
	Dictionary ret;
	if (!RemoteControlRegistry::get_singleton()) {
		return _err("RemoteControlRegistry not available");
	}
	ret["commands"] = RemoteControlRegistry::get_singleton()->list_commands();
	return _ok(ret);
}

Dictionary remote_control_cmd_get_project_path(const Dictionary &p_args) {
	Dictionary ret;
	ret["project_path"] = ProjectSettings::get_singleton()->get_resource_path();
	ret["project_name"] = ProjectSettings::get_singleton()->get_setting("application/config/name", "");
	return _ok(ret);
}

Dictionary remote_control_cmd_get_scene_tree(const Dictionary &p_args) {
	int max_depth = p_args.get("depth", 3);
	String root_path = p_args.get("root", "/root");

	Node *root = _resolve_node(root_path);
#ifdef TOOLS_ENABLED
	if (root_path == "edited" && EditorInterface::get_singleton()) {
		root = EditorInterface::get_singleton()->get_edited_scene_root();
	}
#endif
	if (!root) {
		return _err("Node not found: " + root_path);
	}
	Dictionary ret;
	ret["type"] = "tree";
	ret["root"] = _serialize_node_shallow(root, 0, max_depth);
	return _ok(ret);
}

Dictionary remote_control_cmd_get_node(const Dictionary &p_args) {
	String path = p_args.get("path", "");
	if (path.is_empty()) {
		return _err("Node path required");
	}
	Node *node = _resolve_node(path);
	if (!node) {
		return _err("Node not found: " + path);
	}
	Dictionary ret;
	ret["type"] = "node";
	ret["name"] = node->get_name();
	ret["path"] = String(node->get_path());
	ret["class"] = node->get_class();
	ret["child_count"] = node->get_child_count();
	return _ok(ret);
}

Dictionary remote_control_cmd_set_property(const Dictionary &p_args) {
	String path = p_args.get("path", "");
	String property = p_args.get("property", "");
	Variant value = p_args.get("value", Variant());
	if (path.is_empty() || property.is_empty()) {
		return _err("Node path and property required");
	}
	Node *node = _resolve_node(path);
	if (!node) {
		return _err("Node not found: " + path);
	}
	bool valid = false;
	node->get(property, &valid);
	if (!valid) {
		return _err("Property not found: " + property);
	}
	node->set(property, value);
	Dictionary ret;
	ret["path"] = path;
	ret["property"] = property;
	ret["value"] = value;
	return _ok(ret);
}

static bool _method_allowed(const String &p_method) {
	static const char *blocked[] = {
		"queue_free", "free", "call_deferred", "set_script", "duplicate", nullptr
	};
	for (int i = 0; blocked[i]; i++) {
		if (p_method == blocked[i]) {
			return false;
		}
	}
	return true;
}

Dictionary remote_control_cmd_call_method(const Dictionary &p_args) {
	String path = p_args.get("path", "");
	String method = p_args.get("method", "");
	Array method_args = p_args.get("args", Array());
	if (path.is_empty() || method.is_empty()) {
		return _err("Node path and method required");
	}
	if (!_method_allowed(method)) {
		return _err("Method not allowed: " + method);
	}
	Node *node = _resolve_node(path);
	if (!node) {
		return _err("Node not found: " + path);
	}
	if (!node->has_method(method)) {
		return _err("Method not found: " + method);
	}
	Variant result = node->callv(method, method_args);
	Dictionary ret;
	ret["path"] = path;
	ret["method"] = method;
	ret["result"] = result;
	return _ok(ret);
}

#ifdef TOOLS_ENABLED
static Dictionary _play_status_fields() {
	Dictionary ret;
	EditorInterface *ei = EditorInterface::get_singleton();
	const bool playing = ei && ei->is_playing_scene();
	ret["playing"] = playing;
	if (ei) {
		ret["scene"] = ei->get_playing_scene();
	} else {
		ret["scene"] = String();
	}
	bool paused = false;
	if (EditorRunBar::get_singleton() && EditorRunBar::get_singleton()->get_pause_button()) {
		paused = EditorRunBar::get_singleton()->get_pause_button()->is_pressed();
	}
	if (EditorDebuggerNode::get_singleton()) {
		ScriptEditorDebugger *dbg = EditorDebuggerNode::get_singleton()->get_current_debugger();
		if (dbg && dbg->is_breaked()) {
			paused = true;
		}
	}
	ret["paused"] = paused;
	return ret;
}
#endif

Dictionary remote_control_cmd_play_main_scene(const Dictionary &p_args) {
#ifdef TOOLS_ENABLED
	(void)p_args;
	if (!EditorInterface::get_singleton()) {
		return _err("EditorInterface not available");
	}
	EditorInterface::get_singleton()->call_deferred("play_main_scene");
	Dictionary ret = _play_status_fields();
	ret["playing"] = true;
	ret["deferred"] = true;
	return _ok(ret);
#else
	(void)p_args;
	return _err("play_main_scene is only available in the editor");
#endif
}

Dictionary remote_control_cmd_play_current_scene(const Dictionary &p_args) {
#ifdef TOOLS_ENABLED
	(void)p_args;
	if (!EditorInterface::get_singleton()) {
		return _err("EditorInterface not available");
	}
	EditorInterface::get_singleton()->call_deferred("play_current_scene");
	Dictionary ret = _play_status_fields();
	ret["playing"] = true;
	ret["deferred"] = true;
	return _ok(ret);
#else
	(void)p_args;
	return _err("play_current_scene is only available in the editor");
#endif
}

Dictionary remote_control_cmd_play_custom_scene(const Dictionary &p_args) {
#ifdef TOOLS_ENABLED
	if (!EditorInterface::get_singleton()) {
		return _err("EditorInterface not available");
	}
	String path = String(p_args.get("scene", p_args.get("path", ""))).strip_edges();
	if (path.is_empty()) {
		return _err("scene/path is required for play_custom_scene");
	}
	const String ext = path.get_extension().to_lower();
	if (ext != "tscn" && ext != "scn") {
		return _err("scene path must end with .tscn or .scn");
	}
	EditorInterface::get_singleton()->call_deferred("play_custom_scene", path);
	Dictionary ret = _play_status_fields();
	ret["playing"] = true;
	ret["scene"] = path;
	ret["deferred"] = true;
	return _ok(ret);
#else
	(void)p_args;
	return _err("play_custom_scene is only available in the editor");
#endif
}

Dictionary remote_control_cmd_stop_playing(const Dictionary &p_args) {
#ifdef TOOLS_ENABLED
	(void)p_args;
	if (!EditorInterface::get_singleton()) {
		return _err("EditorInterface not available");
	}
	EditorInterface::get_singleton()->call_deferred("stop_playing_scene");
	Dictionary ret = _play_status_fields();
	ret["playing"] = false;
	ret["paused"] = false;
	ret["deferred"] = true;
	return _ok(ret);
#else
	(void)p_args;
	return _err("stop_playing is only available in the editor");
#endif
}

Dictionary remote_control_cmd_pause_playing(const Dictionary &p_args) {
#ifdef TOOLS_ENABLED
	(void)p_args;
	if (!EditorInterface::get_singleton()) {
		return _err("EditorInterface not available");
	}
	if (!EditorInterface::get_singleton()->is_playing_scene()) {
		return _err("No scene is currently playing");
	}
	if (!EditorRunBar::get_singleton() || !EditorRunBar::get_singleton()->get_pause_button()) {
		return _err("EditorRunBar pause button not available");
	}
	if (!EditorDebuggerNode::get_singleton()) {
		return _err("EditorDebuggerNode not available");
	}
	Button *pause_button = EditorRunBar::get_singleton()->get_pause_button();
	pause_button->set_pressed(true);
	ScriptEditorDebugger *dbg = EditorDebuggerNode::get_singleton()->get_current_debugger();
	if (dbg && !dbg->is_breaked()) {
		EditorDebuggerNode::get_singleton()->debug_break();
	}
	Dictionary ret = _play_status_fields();
	ret["paused"] = true;
	return _ok(ret);
#else
	(void)p_args;
	return _err("pause_playing is only available in the editor");
#endif
}

Dictionary remote_control_cmd_resume_playing(const Dictionary &p_args) {
#ifdef TOOLS_ENABLED
	(void)p_args;
	if (!EditorInterface::get_singleton()) {
		return _err("EditorInterface not available");
	}
	if (!EditorInterface::get_singleton()->is_playing_scene()) {
		return _err("No scene is currently playing");
	}
	if (!EditorRunBar::get_singleton() || !EditorRunBar::get_singleton()->get_pause_button()) {
		return _err("EditorRunBar pause button not available");
	}
	if (!EditorDebuggerNode::get_singleton()) {
		return _err("EditorDebuggerNode not available");
	}
	Button *pause_button = EditorRunBar::get_singleton()->get_pause_button();
	pause_button->set_pressed(false);
	ScriptEditorDebugger *dbg = EditorDebuggerNode::get_singleton()->get_current_debugger();
	if (dbg && dbg->is_breaked()) {
		EditorDebuggerNode::get_singleton()->debug_continue();
	}
	Dictionary ret = _play_status_fields();
	ret["paused"] = false;
	return _ok(ret);
#else
	(void)p_args;
	return _err("resume_playing is only available in the editor");
#endif
}

Dictionary remote_control_cmd_play_status(const Dictionary &p_args) {
#ifdef TOOLS_ENABLED
	(void)p_args;
	if (!EditorInterface::get_singleton()) {
		return _err("EditorInterface not available");
	}
	return _ok(_play_status_fields());
#else
	(void)p_args;
	return _err("play_status is only available in the editor");
#endif
}

static Dictionary _snapshot_from_image(const Ref<Image> &p_img, const String &p_source, const String &p_optional_path) {
	if (p_img.is_null() || p_img->is_empty()) {
		return _err("Captured image is empty");
	}
	Vector<uint8_t> png = p_img->save_png_to_buffer();
	if (png.is_empty()) {
		return _err("Failed to encode PNG");
	}
	Dictionary ret;
	ret["png_base64"] = CryptoCore::b64_encode_str(png.ptr(), png.size());
	ret["width"] = p_img->get_width();
	ret["height"] = p_img->get_height();
	ret["mime"] = "image/png";
	ret["source"] = p_source;

	const String path = p_optional_path.strip_edges();
	if (!path.is_empty()) {
		Error err = p_img->save_png(path);
		if (err != OK) {
			return _err(vformat("Failed to save PNG to %s", path));
		}
		ret["path"] = path;
	}
	return _ok(ret);
}

static Dictionary _snapshot_runtime_viewport(const String &p_optional_path) {
	SceneTree *tree = _scene_tree();
	if (!tree || !tree->get_root()) {
		return _err("SceneTree root not available");
	}
	Ref<ViewportTexture> texture = tree->get_root()->get_texture();
	if (texture.is_null()) {
		return _err("Root viewport texture not available");
	}
	Ref<Image> img = texture->get_image();
	return _snapshot_from_image(img, "runtime_viewport", p_optional_path);
}

#ifdef TOOLS_ENABLED
static EmbeddedProcess *_find_embedded_process(Node *p_node) {
	if (!p_node) {
		return nullptr;
	}
	EmbeddedProcess *ep = Object::cast_to<EmbeddedProcess>(p_node);
	if (ep && ep->is_embedding_completed()) {
		return ep;
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		EmbeddedProcess *found = _find_embedded_process(p_node->get_child(i));
		if (found) {
			return found;
		}
	}
	return nullptr;
}
#endif

Dictionary remote_control_cmd_snapshot_editor(const Dictionary &p_args) {
#ifdef TOOLS_ENABLED
	const String path = String(p_args.get("path", "")).strip_edges();
	if (!EditorNode::get_singleton() || !EditorNode::get_editor_main_screen()) {
		return _err("Editor main screen not available");
	}
	Control *main_screen_control = EditorNode::get_editor_main_screen()->get_control();
	if (!main_screen_control) {
		return _err("Cannot get the editor main screen control");
	}
	Viewport *viewport = main_screen_control->get_viewport();
	if (!viewport) {
		return _err("Cannot get a viewport from the editor main screen");
	}
	Ref<ViewportTexture> texture = viewport->get_texture();
	if (texture.is_null()) {
		return _err("Cannot get a viewport texture from the editor main screen");
	}
	Ref<Image> img = texture->get_image();
	return _snapshot_from_image(img, "editor_viewport", path);
#else
	(void)p_args;
	return _err("snapshot_editor is only available in the editor");
#endif
}

Dictionary remote_control_cmd_snapshot_scene(const Dictionary &p_args) {
	const String path = String(p_args.get("path", "")).strip_edges();

#if !defined(TOOLS_ENABLED)
	return _snapshot_runtime_viewport(path);
#else

	if (!EditorInterface::get_singleton()) {
		Dictionary ret = _snapshot_runtime_viewport(path);
		if (bool(ret.get("ok", false))) {
			return ret;
		}
		return _err("EditorInterface not available");
	}

	Dictionary status;
	if (RemoteControlServer::get_singleton()) {
		status = RemoteControlServer::get_singleton()->get_status();
	}
	if (String(status.get("mode", "editor")) == "runtime") {
		Dictionary ret = _snapshot_runtime_viewport(path);
		if (bool(ret.get("ok", false))) {
			Dictionary play = _play_status_fields();
			ret["playing"] = play.get("playing", false);
			ret["paused"] = play.get("paused", false);
			ret["scene"] = play.get("scene", String());
		}
		return ret;
	}

	if (!EditorInterface::get_singleton()->is_playing_scene()) {
		return _err("No scene is currently playing");
	}

	Dictionary play = _play_status_fields();
	Dictionary ret;

	EmbeddedProcess *embedded = nullptr;
	if (EditorInterface::get_singleton()->get_base_control()) {
		embedded = _find_embedded_process(EditorInterface::get_singleton()->get_base_control());
	}
	if (!embedded && EditorNode::get_singleton()) {
		embedded = _find_embedded_process(EditorNode::get_singleton());
	}

	if (embedded && DisplayServer::get_singleton()) {
		const Rect2i rect = embedded->get_screen_embedded_window_rect();
		if (rect.size.x > 0 && rect.size.y > 0) {
			Ref<Image> img = DisplayServer::get_singleton()->screen_get_image_rect(rect);
			ret = _snapshot_from_image(img, "embedded_game", path);
			if (bool(ret.get("ok", false))) {
				ret["playing"] = play.get("playing", true);
				ret["paused"] = play.get("paused", false);
				ret["scene"] = play.get("scene", String());
				return ret;
			}
		}
	}

	if (!DisplayServer::get_singleton()) {
		return _err("DisplayServer not available");
	}
	const int screen = DisplayServer::get_singleton()->get_primary_screen();
	Ref<Image> img = DisplayServer::get_singleton()->screen_get_image(screen);
	ret = _snapshot_from_image(img, "screen", path);
	if (bool(ret.get("ok", false))) {
		ret["playing"] = play.get("playing", true);
		ret["paused"] = play.get("paused", false);
		ret["scene"] = play.get("scene", String());
	}
	return ret;
#endif
}

Dictionary remote_control_cmd_save_scene(const Dictionary &p_args) {
#ifdef TOOLS_ENABLED
	(void)p_args;
	if (!EditorInterface::get_singleton()) {
		return _err("EditorInterface not available");
	}
	EditorInterface::get_singleton()->call_deferred("save_scene");
	Dictionary ret;
	ret["saved"] = true;
	ret["deferred"] = true;
	if (EditorInterface::get_singleton()->get_edited_scene_root()) {
		ret["scene"] = EditorInterface::get_singleton()->get_edited_scene_root()->get_scene_file_path();
	}
	return _ok(ret);
#else
	(void)p_args;
	return _err("save_scene is only available in the editor");
#endif
}

Dictionary remote_control_cmd_reload_filesystem(const Dictionary &p_args) {
#ifdef TOOLS_ENABLED
	(void)p_args;
	if (!EditorFileSystem::get_singleton()) {
		return _err("EditorFileSystem not available");
	}
	EditorFileSystem::get_singleton()->call_deferred("scan");
	Dictionary ret;
	ret["scanning"] = true;
	ret["deferred"] = true;
	return _ok(ret);
#else
	(void)p_args;
	return _err("reload_filesystem is only available in the editor");
#endif
}

static uint64_t _arg_u64(const Dictionary &p_args, const String &p_key, uint64_t p_default = 0) {
	if (!p_args.has(p_key)) {
		return p_default;
	}
	return (uint64_t)(int64_t)p_args[p_key];
}

Dictionary remote_control_cmd_get_logs(const Dictionary &p_args) {
	if (!RemoteControlServer::get_singleton()) {
		return _err("RemoteControlServer not available");
	}
	const uint64_t since = _arg_u64(p_args, "since");
	const uint64_t cursor = _arg_u64(p_args, "cursor");
	const int limit = p_args.get("limit", 200);
	return RemoteControlServer::get_singleton()->get_logs_page(since, cursor, limit, false);
}

Dictionary remote_control_cmd_get_errors(const Dictionary &p_args) {
	if (!RemoteControlServer::get_singleton()) {
		return _err("RemoteControlServer not available");
	}
	const uint64_t since = _arg_u64(p_args, "since");
	const uint64_t cursor = _arg_u64(p_args, "cursor");
	const int limit = p_args.get("limit", 200);
	return RemoteControlServer::get_singleton()->get_logs_page(since, cursor, limit, true);
}

Dictionary remote_control_cmd_debugger_info(const Dictionary &p_args) {
	(void)p_args;
	Dictionary ret;
	ret["session_active"] = false;
	ret["breaked"] = false;
	ret["can_debug"] = false;
	ret["skip_breakpoints"] = false;
	ret["reason"] = "";
	ret["error_count"] = 0;
	ret["warning_count"] = 0;
	Dictionary stack;
	stack["file"] = "";
	stack["line"] = 0;
	stack["frame"] = 0;
	stack["frames"] = Array();
	ret["stack"] = stack;
	ret["errors"] = Array();
	ret["breakpoints"] = Array();
	ret["error_breaks"] = RemoteControlServer::get_singleton() ? RemoteControlServer::get_singleton()->get_error_breaks() : Array();

#ifdef TOOLS_ENABLED
	EditorDebuggerNode *debugger_node = EditorDebuggerNode::get_singleton();
	if (debugger_node) {
		ScriptEditorDebugger *dbg = debugger_node->get_current_debugger();
		if (dbg) {
			ret["session_active"] = dbg->is_session_active();
			ret["breaked"] = dbg->is_breaked();
			ret["can_debug"] = dbg->is_debuggable();
			ret["skip_breakpoints"] = dbg->is_skip_breakpoints();
			ret["reason"] = dbg->get_break_reason();
			ret["error_count"] = dbg->get_error_count();
			ret["warning_count"] = dbg->get_warning_count();
			stack["file"] = dbg->get_stack_script_file();
			stack["line"] = dbg->get_stack_script_line();
			stack["frame"] = dbg->get_stack_script_frame();
			stack["frames"] = dbg->is_breaked() ? dbg->get_stack_frames() : Array();
			ret["stack"] = stack;
			ret["errors"] = dbg->get_error_list();
		}
		ret["breakpoints"] = debugger_node->get_breakpoints_list();
		ret["skip_breakpoints"] = debugger_node->is_skip_breakpoints();
	}
#endif
	return _ok(ret);
}

Dictionary remote_control_cmd_debugger_clear(const Dictionary &p_args) {
	const bool clear_errors = p_args.get("errors", true);
	const bool clear_error_breaks = p_args.get("error_breaks", true);
	const bool clear_logs = p_args.get("logs", false);

	Dictionary cleared;
	cleared["errors"] = false;
	cleared["error_breaks"] = false;
	cleared["logs"] = false;

#ifdef TOOLS_ENABLED
	if (clear_errors && EditorDebuggerNode::get_singleton()) {
		EditorDebuggerNode::get_singleton()->clear_errors();
		cleared["errors"] = true;
	}
#else
	(void)clear_errors;
#endif
	if (clear_error_breaks && RemoteControlServer::get_singleton()) {
		RemoteControlServer::get_singleton()->clear_error_breaks();
		cleared["error_breaks"] = true;
	}
	if (clear_logs && RemoteControlServer::get_singleton()) {
		RemoteControlServer::get_singleton()->clear_logs();
		cleared["logs"] = true;
	}

	Dictionary ret;
	ret["cleared"] = cleared;
	return _ok(ret);
}

Dictionary remote_control_cmd_debugger_status(const Dictionary &p_args) {
	Dictionary full = remote_control_cmd_debugger_info(p_args);
	Dictionary ret;
	ret["ok"] = true;
	ret["session_active"] = full.get("session_active", false);
	ret["breaked"] = full.get("breaked", false);
	ret["can_debug"] = full.get("can_debug", false);
	ret["skip_breakpoints"] = full.get("skip_breakpoints", false);
	ret["reason"] = full.get("reason", "");
	ret["error_count"] = full.get("error_count", 0);
	ret["warning_count"] = full.get("warning_count", 0);
	ret["stack"] = full.get("stack", Dictionary());
	return ret;
}

Dictionary remote_control_cmd_debugger_stack(const Dictionary &p_args) {
	Dictionary full = remote_control_cmd_debugger_info(p_args);
	Dictionary ret;
	ret["ok"] = true;
	ret["breaked"] = full.get("breaked", false);
	ret["stack"] = full.get("stack", Dictionary());
	return ret;
}

Dictionary remote_control_cmd_debugger_list_breakpoints(const Dictionary &p_args) {
	(void)p_args;
	Dictionary ret;
	ret["breakpoints"] = Array();
#ifdef TOOLS_ENABLED
	if (EditorDebuggerNode::get_singleton()) {
		ret["breakpoints"] = EditorDebuggerNode::get_singleton()->get_breakpoints_list();
	}
#endif
	return _ok(ret);
}

Dictionary remote_control_cmd_debugger_error_breaks(const Dictionary &p_args) {
	(void)p_args;
	Dictionary ret;
	ret["error_breaks"] = RemoteControlServer::get_singleton() ? RemoteControlServer::get_singleton()->get_error_breaks() : Array();
	return _ok(ret);
}

static String _read_file_excerpt(const String &p_path, int p_max_chars = 32000) {
	if (!FileAccess::exists(p_path)) {
		return String();
	}
	Error err = OK;
	String text = FileAccess::get_file_as_string(p_path, &err);
	if (err != OK) {
		return String();
	}
	if (text.length() > p_max_chars) {
		return text.substr(text.length() - p_max_chars, p_max_chars);
	}
	return text;
}

Dictionary remote_control_cmd_get_failed_run(const Dictionary &p_args) {
	(void)p_args;
	Dictionary ret;
	ret["instance_id"] = RemoteControlServer::get_singleton() ? RemoteControlServer::get_singleton()->get_remote_instance_id() : String();
	ret["playing"] = false;
#ifdef TOOLS_ENABLED
	if (EditorRunBar::get_singleton()) {
		ret["playing"] = EditorRunBar::get_singleton()->is_playing();
	}
#endif

	Dictionary logs;
	if (RemoteControlServer::get_singleton()) {
		logs["errors"] = RemoteControlServer::get_singleton()->get_logs_page(0, 0, 200, true).get("entries", Array());
		logs["tail"] = RemoteControlServer::get_singleton()->get_logs_page(0, 0, 100, false).get("entries", Array());
	} else {
		logs["errors"] = Array();
		logs["tail"] = Array();
	}
	ret["logs"] = logs;

	Dictionary dbg = remote_control_cmd_debugger_info(Dictionary());
	dbg.erase("ok");
	ret["debugger"] = dbg;

	Dictionary aw;
	if (RemoteControlServer::get_singleton()) {
		aw["job"] = RemoteControlServer::get_singleton()->get_autowork_status();
		aw["results"] = RemoteControlServer::get_singleton()->get_autowork_results().get("results", Variant());
		aw["results_path"] = "user://autowork_results.json";
	} else {
		aw["job"] = Dictionary();
		aw["results"] = Variant();
		aw["results_path"] = "user://autowork_results.json";
	}
	ret["autowork"] = aw;

	Array files;
	struct Evidence {
		const char *path;
		const char *kind;
	};
	const Evidence evidence[] = {
		{ "user://logs/blazium.log", "log" },
		{ "user://logs/godot.log", "log" },
		{ "user://autowork_results.json", "json" },
		{ nullptr, nullptr }
	};
	for (int i = 0; evidence[i].path; i++) {
		String content = _read_file_excerpt(evidence[i].path);
		if (content.is_empty() && !FileAccess::exists(evidence[i].path)) {
			continue;
		}
		Dictionary f;
		f["path"] = evidence[i].path;
		f["kind"] = evidence[i].kind;
		f["content_or_excerpt"] = content;
		files.push_back(f);
	}
	ret["files"] = files;
	return _ok(ret);
}

Dictionary remote_control_cmd_autowork_run(const Dictionary &p_args) {
	if (!RemoteControlServer::get_singleton()) {
		return _err("RemoteControlServer not available");
	}
	return RemoteControlServer::get_singleton()->start_autowork_job(p_args);
}

Dictionary remote_control_cmd_autowork_status(const Dictionary &p_args) {
	(void)p_args;
	if (!RemoteControlServer::get_singleton()) {
		return _err("RemoteControlServer not available");
	}
	return RemoteControlServer::get_singleton()->get_autowork_status();
}

Dictionary remote_control_cmd_autowork_results(const Dictionary &p_args) {
	(void)p_args;
	if (!RemoteControlServer::get_singleton()) {
		return _err("RemoteControlServer not available");
	}
	return RemoteControlServer::get_singleton()->get_autowork_results();
}

Dictionary remote_control_cmd_focus_window(const Dictionary &p_args) {
	(void)p_args;
	if (!DisplayServer::get_singleton()) {
		return _err("DisplayServer not available");
	}
	DisplayServer::get_singleton()->window_move_to_foreground();
	DisplayServer::get_singleton()->window_request_attention();
	Dictionary ret;
	ret["focused"] = true;
	return _ok(ret);
}

Dictionary remote_control_cmd_mcp_status(const Dictionary &p_args) {
	(void)p_args;
	Dictionary ret;
#ifdef MODULE_JUSTAMCP_ENABLED
#ifdef TOOLS_ENABLED
	JustAMCPServer *server = JustAMCPServer::get_singleton();
	const bool started = server && server->is_server_started();
	ret["available"] = true;
	ret["started"] = started;
	ret["port"] = started && server ? server->get_listening_port() : -1;
#else
	ret["available"] = false;
	ret["started"] = false;
	ret["port"] = -1;
	ret["reason"] = "JustAMCP is editor-only";
#endif
#else
	ret["available"] = false;
	ret["started"] = false;
	ret["port"] = -1;
	ret["reason"] = "JustAMCP module not compiled in";
#endif
	return _ok(ret);
}

void remote_control_register_builtin_commands(RemoteControlRegistry *p_registry) {
	ERR_FAIL_NULL(p_registry);

	p_registry->register_command("ping", callable_mp_static(remote_control_cmd_ping), "Health probe");
	p_registry->register_command("get_status", callable_mp_static(remote_control_cmd_get_status), "Instance status payload");
	p_registry->register_command("list_commands", callable_mp_static(remote_control_cmd_list_commands), "List registered commands");
	p_registry->register_command("get_project_path", callable_mp_static(remote_control_cmd_get_project_path), "Project filesystem path");
	p_registry->register_command("get_scene_tree", callable_mp_static(remote_control_cmd_get_scene_tree), "Scene tree summary");
	p_registry->register_command("get_node", callable_mp_static(remote_control_cmd_get_node), "Node info by path");
	p_registry->register_command("set_property", callable_mp_static(remote_control_cmd_set_property), "Set a node property");
	p_registry->register_command("call_method", callable_mp_static(remote_control_cmd_call_method), "Call an allowlisted node method");
	p_registry->register_command("play_main_scene", callable_mp_static(remote_control_cmd_play_main_scene), "Play project main/default scene (editor)");
	p_registry->register_command("play_default_scene", callable_mp_static(remote_control_cmd_play_main_scene), "Alias for play_main_scene");
	p_registry->register_command("play_current_scene", callable_mp_static(remote_control_cmd_play_current_scene), "Play currently edited scene (editor)");
	p_registry->register_command("play_custom_scene", callable_mp_static(remote_control_cmd_play_custom_scene), "Play scene by path args.scene/path (editor)");
	p_registry->register_command("stop_playing", callable_mp_static(remote_control_cmd_stop_playing), "Stop play mode (editor)");
	p_registry->register_command("stop_scene", callable_mp_static(remote_control_cmd_stop_playing), "Alias for stop_playing");
	p_registry->register_command("pause_playing", callable_mp_static(remote_control_cmd_pause_playing), "Pause running project (editor debugger break)");
	p_registry->register_command("pause_scene", callable_mp_static(remote_control_cmd_pause_playing), "Alias for pause_playing");
	p_registry->register_command("resume_playing", callable_mp_static(remote_control_cmd_resume_playing), "Resume from pause (editor)");
	p_registry->register_command("resume_scene", callable_mp_static(remote_control_cmd_resume_playing), "Alias for resume_playing");
	p_registry->register_command("play_status", callable_mp_static(remote_control_cmd_play_status), "Report play/pause state (editor)");
	p_registry->register_command("snapshot_editor", callable_mp_static(remote_control_cmd_snapshot_editor), "Capture editor main-screen PNG (base64)");
	p_registry->register_command("take_editor_screenshot", callable_mp_static(remote_control_cmd_snapshot_editor), "Alias for snapshot_editor");
	p_registry->register_command("snapshot_scene", callable_mp_static(remote_control_cmd_snapshot_scene), "Capture playing/paused scene PNG (base64)");
	p_registry->register_command("snapshot_play", callable_mp_static(remote_control_cmd_snapshot_scene), "Alias for snapshot_scene");
	p_registry->register_command("take_game_screenshot", callable_mp_static(remote_control_cmd_snapshot_scene), "Alias for snapshot_scene");
	p_registry->register_command("save_scene", callable_mp_static(remote_control_cmd_save_scene), "Save current edited scene (editor)");
	p_registry->register_command("reload_filesystem", callable_mp_static(remote_control_cmd_reload_filesystem), "Rescan editor filesystem");

	p_registry->register_command("get_logs", callable_mp_static(remote_control_cmd_get_logs), "Paged engine logs (since/cursor/limit)");
	p_registry->register_command("get_errors", callable_mp_static(remote_control_cmd_get_errors), "Paged error/warning logs");
	p_registry->register_command("debugger_info", callable_mp_static(remote_control_cmd_debugger_info), "Full debugger dump");
	p_registry->register_command("debugger_clear", callable_mp_static(remote_control_cmd_debugger_clear), "Clear debugger errors / rings");
	p_registry->register_command("debugger_status", callable_mp_static(remote_control_cmd_debugger_status), "Debugger status subset");
	p_registry->register_command("debugger_stack", callable_mp_static(remote_control_cmd_debugger_stack), "Stack frames subset");
	p_registry->register_command("debugger_list_breakpoints", callable_mp_static(remote_control_cmd_debugger_list_breakpoints), "List breakpoints");
	p_registry->register_command("debugger_error_breaks", callable_mp_static(remote_control_cmd_debugger_error_breaks), "Error-breakpoint hits");
	p_registry->register_command("get_failed_run", callable_mp_static(remote_control_cmd_get_failed_run), "Failed-run evidence bundle");
	p_registry->register_command("autowork_run", callable_mp_static(remote_control_cmd_autowork_run), "Start Autowork job");
	p_registry->register_command("autowork_status", callable_mp_static(remote_control_cmd_autowork_status), "Autowork job status");
	p_registry->register_command("autowork_results", callable_mp_static(remote_control_cmd_autowork_results), "Autowork job results");
	p_registry->register_command("focus_window", callable_mp_static(remote_control_cmd_focus_window), "Bring editor/game window to front");
	p_registry->register_command("bring_to_front", callable_mp_static(remote_control_cmd_focus_window), "Alias for focus_window");
	p_registry->register_command("mcp_status", callable_mp_static(remote_control_cmd_mcp_status), "JustAMCP server status (port/started)");
}
