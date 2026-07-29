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
#include "core/io/file_access.h"
#include "core/os/os.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

#ifdef TOOLS_ENABLED
#include "editor/debugger/editor_debugger_node.h"
#include "editor/debugger/script_editor_debugger.h"
#include "editor/editor_file_system.h"
#include "editor/editor_interface.h"
#include "editor/gui/editor_run_bar.h"
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

Dictionary remote_control_cmd_play_main_scene(const Dictionary &p_args) {
#ifdef TOOLS_ENABLED
	(void)p_args;
	if (!EditorInterface::get_singleton()) {
		return _err("EditorInterface not available");
	}
	EditorInterface::get_singleton()->call_deferred("play_main_scene");
	Dictionary ret;
	ret["playing"] = true;
	ret["deferred"] = true;
	return _ok(ret);
#else
	(void)p_args;
	return _err("play_main_scene is only available in the editor");
#endif
}

Dictionary remote_control_cmd_stop_playing(const Dictionary &p_args) {
#ifdef TOOLS_ENABLED
	(void)p_args;
	if (!EditorInterface::get_singleton()) {
		return _err("EditorInterface not available");
	}
	EditorInterface::get_singleton()->call_deferred("stop_playing_scene");
	Dictionary ret;
	ret["playing"] = false;
	ret["deferred"] = true;
	return _ok(ret);
#else
	(void)p_args;
	return _err("stop_playing is only available in the editor");
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
	p_registry->register_command("play_main_scene", callable_mp_static(remote_control_cmd_play_main_scene), "Start play mode (editor)");
	p_registry->register_command("stop_playing", callable_mp_static(remote_control_cmd_stop_playing), "Stop play mode (editor)");
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
}
