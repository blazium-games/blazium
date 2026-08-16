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

#include "tools/justamcp_tool_executor.h"

#include "core/config/project_settings.h" // IWYU pragma: keep
#include "core/crypto/crypto_core.h"
#include "core/input/input.h" // IWYU pragma: keep
#include "core/input/input_event.h"
#include "core/io/file_access.h" // IWYU pragma: keep
#include "core/io/image.h" // IWYU pragma: keep
#include "core/io/json.h" // IWYU pragma: keep
#include "core/math/expression.h" // IWYU pragma: keep
#include "core/object/class_db.h"
#include "core/object/message_queue.h" // IWYU pragma: keep
#include "core/object/script_language.h" // IWYU pragma: keep
#include "core/os/os.h" // IWYU pragma: keep
#include "core/os/time.h" // IWYU pragma: keep
#include "main/performance.h" // IWYU pragma: keep
#include "scene/gui/base_button.h" // IWYU pragma: keep
#include "scene/gui/control.h" // IWYU pragma: keep
#include "scene/main/multiplayer_api.h" // IWYU pragma: keep
#include "scene/main/viewport.h" // IWYU pragma: keep
#include "scene/main/window.h" // IWYU pragma: keep
#include "servers/audio/audio_server.h" // IWYU pragma: keep

#ifdef TOOLS_ENABLED
#include "editor/settings/editor_settings.h" // IWYU pragma: keep
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

	executor = memnew(JustAMCPToolExecutor);
	executor->set_as_active_instance();

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
