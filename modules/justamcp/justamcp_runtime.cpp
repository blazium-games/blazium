/**************************************************************************/
/*  justamcp_runtime.cpp                                                  */
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

#include "justamcp_runtime.h"

#include "justamcp_project_registry.h"
#include "justamcp_server.h"
#include "tools/justamcp_settings_resolver.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/crypto/crypto_core.h"
#include "core/input/input.h"
#include "core/input/input_event.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "core/math/expression.h"
#include "core/object/class_db.h"
#include "core/object/message_queue.h"
#include "core/object/script_language.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "main/performance.h"
#include "scene/gui/base_button.h"
#include "scene/gui/control.h"
#include "scene/main/multiplayer_api.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "servers/audio_server.h"
#ifdef TOOLS_ENABLED
#include "editor/editor_settings.h"
#include "tools/justamcp_tool_executor.h"
#endif

JustAMCPRuntime *JustAMCPRuntime::singleton = nullptr;

const char *JustAMCPRuntime::_BLOCKED_METHODS[] = {
	"execute", "create_process", "shell", "spawn", "create_thread",
	"load", "load_file", "load_buffer", "load_script", "load_resource_pack",
	"save", "save_file", "store_buffer", "open", "open_encrypted",
	"write", "write_buffer", "write_file", "delete_file", "move_file", "copy_file",
	"eval", "compile", "compile_file", "set_script",
	"system", "exec", "request_permissions", "quit", "crash",
	"set_meta", "remove_meta", "get_meta",
	nullptr
};

const char *JustAMCPRuntime::_EVAL_BLOCKED_PATTERNS[] = {
	"OS.", "Engine.", "FileAccess.", "DirAccess.", "Directory.", "ClassDB.",
	"create_process", "execute", "shell", "load(", "save(", "open(", "delete(",
	"write_file", "load_file", "save_file", "store_buffer", "set_script",
	"Thread.", "Mutex.", "Semaphore.", "HttpRequest.", "TCPServer.", "StreamPeer.",
	nullptr
};

JustAMCPRuntime *JustAMCPRuntime::get_singleton() {
	return singleton;
}

void JustAMCPRuntime::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_port", "port"), &JustAMCPRuntime::set_port);
	ClassDB::bind_method(D_METHOD("get_port"), &JustAMCPRuntime::get_port);
	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &JustAMCPRuntime::set_enabled);
	ClassDB::bind_method(D_METHOD("is_enabled"), &JustAMCPRuntime::is_enabled);
	ClassDB::bind_method(D_METHOD("execute_command", "command", "params"), &JustAMCPRuntime::execute_command);
	ClassDB::bind_method(D_METHOD("poll"), &JustAMCPRuntime::poll);

	ClassDB::bind_method(D_METHOD("register_custom_command", "name", "callable"), &JustAMCPRuntime::register_custom_command);
	ClassDB::bind_method(D_METHOD("unregister_custom_command", "name"), &JustAMCPRuntime::unregister_custom_command);
	ClassDB::bind_method(D_METHOD("register_tool", "name", "description", "input_schema", "callable"), &JustAMCPRuntime::register_tool);
	ClassDB::bind_method(D_METHOD("unregister_tool", "name"), &JustAMCPRuntime::unregister_tool);
	ClassDB::bind_method(D_METHOD("register_prompt", "name", "description", "callable"), &JustAMCPRuntime::register_prompt);
	ClassDB::bind_method(D_METHOD("unregister_prompt", "name"), &JustAMCPRuntime::unregister_prompt);
	ClassDB::bind_method(D_METHOD("list_tools"), &JustAMCPRuntime::list_tools);
	ClassDB::bind_method(D_METHOD("is_listening"), &JustAMCPRuntime::is_listening);
	ClassDB::bind_method(D_METHOD("load_project_mcp_scripts"), &JustAMCPRuntime::load_project_mcp_scripts);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "port"), "set_port", "get_port");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "is_enabled");

	ADD_SIGNAL(MethodInfo("client_connected"));
	ADD_SIGNAL(MethodInfo("client_disconnected"));
	ADD_SIGNAL(MethodInfo("command_received", PropertyInfo(Variant::STRING, "command"), PropertyInfo(Variant::DICTIONARY, "params")));
}

JustAMCPRuntime::JustAMCPRuntime() {
	const bool claim_singleton = (singleton == nullptr);
	if (claim_singleton) {
		singleton = this;
	}
	server.instantiate();
	if (!claim_singleton) {
		return;
	}

#ifdef TOOLS_ENABLED
	executor = memnew(JustAMCPToolExecutor);
	executor->set_as_active_instance();
#endif

	_print_handler.printfunc = _print_handler_callback;
	_print_handler.userdata = this;
	add_print_handler(&_print_handler);

	if (enabled) {
		_start_server();
	}
}

JustAMCPRuntime::~JustAMCPRuntime() {
	if (singleton != this && !executor) {
		return;
	}
	remove_print_handler(&_print_handler);
	if (singleton == this) {
		singleton = nullptr;
	}
	_cleanup();
}

void JustAMCPRuntime::register_tool(const String &p_name, const String &p_description, const Dictionary &p_input_schema, const Callable &p_callable) {
	JustAMCPProjectRegistry::register_tool(p_name, p_description, p_input_schema, p_callable);
#ifdef TOOLS_ENABLED
	if (JustAMCPServer::get_singleton() && JustAMCPServer::get_singleton()->is_server_started()) {
		JustAMCPServer::get_singleton()->broadcast_tools_list_changed();
	}
#endif
}

void JustAMCPRuntime::unregister_tool(const String &p_name) {
	JustAMCPProjectRegistry::unregister_tool(p_name);
}

void JustAMCPRuntime::register_prompt(const String &p_name, const String &p_description, const Callable &p_callable) {
	JustAMCPProjectRegistry::register_prompt(p_name, p_description, p_callable);
#ifdef TOOLS_ENABLED
	if (JustAMCPServer::get_singleton() && JustAMCPServer::get_singleton()->is_server_started()) {
		JustAMCPServer::get_singleton()->broadcast_prompts_list_changed();
	}
#endif
}

void JustAMCPRuntime::unregister_prompt(const String &p_name) {
	JustAMCPProjectRegistry::unregister_prompt(p_name);
#ifdef TOOLS_ENABLED
	if (JustAMCPServer::get_singleton() && JustAMCPServer::get_singleton()->is_server_started()) {
		JustAMCPServer::get_singleton()->broadcast_prompts_list_changed();
	}
#endif
}

void JustAMCPRuntime::_debug_print(const String &p_message) {
	if (JustAMCPSettingsResolver::resolve_bool("blazium/justamcp/enable_debug_logging", false)) {
		print_line(p_message);
	}
}

bool JustAMCPRuntime::is_valid_project_mcp_dir(const String &p_dir) {
	const String dir = p_dir.strip_edges();
	if (!dir.begins_with("res://")) {
		return false;
	}
	const String rest = dir.substr(String("res://").length());
	if (rest.is_empty()) {
		return true;
	}
	if (rest.begins_with("/") || rest.contains("..") || rest.contains("//")) {
		return false;
	}
	return true;
}

Array JustAMCPRuntime::list_tools() const {
	return JustAMCPProjectRegistry::list_tool_schemas();
}

bool JustAMCPRuntime::is_listening() const {
	if (http_host && http_host->is_server_started()) {
		return true;
	}
	return enabled && server.is_valid() && server->is_listening();
}

void JustAMCPRuntime::load_project_mcp_scripts() {
	if (project_scripts_loaded) {
		return;
	}
	project_scripts_loaded = true;
	const String dir = JustAMCPSettingsResolver::resolve_string("blazium/justamcp/project_mcp_dir", "res://mcp");
	if (!is_valid_project_mcp_dir(dir)) {
		ERR_PRINT("JustAMCP: blazium/justamcp/project_mcp_dir must be a res:// path without '..'.");
		return;
	}
	Vector<String> scripts;
	scripts.push_back(dir.path_join("register.gd"));
	scripts.push_back(dir.path_join("register.luau"));
	for (int i = 0; i < scripts.size(); i++) {
		const String path = scripts[i];
		if (!ResourceLoader::exists(path)) {
			continue;
		}
		Ref<Resource> res = ResourceLoader::load(path);
		Ref<Script> mcp_script = res;
		if (mcp_script.is_null() || !mcp_script->can_instantiate()) {
			WARN_PRINT("JustAMCP: Could not instantiate project MCP script " + path);
			continue;
		}
		const StringName base = mcp_script->get_instance_base_type();
		if (base == StringName() || !ClassDB::can_instantiate(base)) {
			WARN_PRINT("JustAMCP: Project MCP script " + path + " has no instantiable base type.");
			continue;
		}
		Object *inst = ClassDB::instantiate(base);
		if (!inst) {
			continue;
		}
		inst->set_script(mcp_script);
		if (Node *node = Object::cast_to<Node>(inst)) {
			if (SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop())) {
				if (tree->get_root()) {
					tree->get_root()->add_child(node);
				}
			}
		}
		project_mcp_instances.push_back(inst);
		if (inst->has_method("register")) {
			inst->call("register");
		}
		_debug_print("JustAMCP: Loaded project MCP script " + path);
	}
}
