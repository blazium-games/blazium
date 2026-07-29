/**************************************************************************/
/*  remote_control_server.cpp                                             */
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

#include "remote_control_server.h"

#include "remote_control_registry.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/math/expression.h"
#include "core/object/class_db.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "core/string/print_string.h"
#include "modules/httpserver/http_server.h"
#include "modules/modules_enabled.gen.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

#if defined(MODULE_AUTOWORK_ENABLED)
#include "modules/autowork/autowork_main.h"
#endif

#if defined(MODULE_LUAU_MODULE_ENABLED)
#include "modules/luau_module/lua_state.h"
#endif

#ifdef TOOLS_ENABLED
#include "editor/editor_settings.h"
#endif

RemoteControlServer *RemoteControlServer::singleton = nullptr;

static const char *_EVAL_BLOCKED_PATTERNS[] = {
	"OS.execute",
	"OS.create_process",
	"OS.kill",
	"DirAccess",
	"FileAccess",
	"JavaScriptBridge",
	"ClassDB.instantiate",
	nullptr
};

static const char *_LUAU_EVAL_BLOCKED_PATTERNS[] = {
	"os.execute",
	"os.remove",
	"os.rename",
	"io.open",
	"io.popen",
	"loadfile",
	"dofile",
	"package.loadlib",
	nullptr
};

RemoteControlServer *RemoteControlServer::get_singleton() {
	return singleton;
}

void RemoteControlServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start"), &RemoteControlServer::start);
	ClassDB::bind_method(D_METHOD("stop"), &RemoteControlServer::stop);
	ClassDB::bind_method(D_METHOD("is_started"), &RemoteControlServer::is_started);
	ClassDB::bind_method(D_METHOD("get_port"), &RemoteControlServer::get_port);
	ClassDB::bind_method(D_METHOD("get_status"), &RemoteControlServer::get_status);
	ClassDB::bind_method(D_METHOD("set_allow_eval", "allow"), &RemoteControlServer::set_allow_eval);
	ClassDB::bind_method(D_METHOD("get_allow_eval"), &RemoteControlServer::get_allow_eval);
	ClassDB::bind_method(D_METHOD("set_token", "token"), &RemoteControlServer::set_token);
	ClassDB::bind_method(D_METHOD("get_token"), &RemoteControlServer::get_token);
	ClassDB::bind_method(D_METHOD("set_remote_instance_id", "id"), &RemoteControlServer::set_remote_instance_id);
	ClassDB::bind_method(D_METHOD("get_remote_instance_id"), &RemoteControlServer::get_remote_instance_id);
	ClassDB::bind_method(D_METHOD("eval_expression", "expression", "language"), &RemoteControlServer::eval_expression, DEFVAL("gdscript"));
	ClassDB::bind_method(D_METHOD("_deferred_exec", "client_id", "command", "args"), &RemoteControlServer::_deferred_exec);
	ClassDB::bind_method(D_METHOD("_deferred_eval", "client_id", "expression", "language"), &RemoteControlServer::_deferred_eval);
	ClassDB::bind_method(D_METHOD("_deferred_run_autowork", "args"), &RemoteControlServer::_deferred_run_autowork);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "allow_eval"), "set_allow_eval", "get_allow_eval");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "token"), "set_token", "get_token");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "remote_instance_id"), "set_remote_instance_id", "get_remote_instance_id");
}

bool RemoteControlServer::should_enable_from_cmdline_or_settings() {
	if (OS::get_singleton() && OS::get_singleton()->get_cmdline_args().find("--enable-remote-control")) {
		return true;
	}
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/remote_control/server_enabled")) {
		return bool(GLOBAL_GET("blazium/remote_control/server_enabled"));
	}
	return false;
}

int RemoteControlServer::configured_port() {
	List<String> args = OS::get_singleton()->get_cmdline_args();
	for (const String &arg : args) {
		if (arg.begins_with("--remote-control-port=")) {
			return arg.get_slice("=", 1).to_int();
		}
	}
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/remote_control/server_port")) {
		return int(GLOBAL_GET("blazium/remote_control/server_port"));
	}
	return 6507;
}

String RemoteControlServer::configured_bind_address() {
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/remote_control/bind_address")) {
		return String(GLOBAL_GET("blazium/remote_control/bind_address"));
	}
	return "127.0.0.1";
}

String RemoteControlServer::configured_token() {
	List<String> args = OS::get_singleton()->get_cmdline_args();
	for (const String &arg : args) {
		if (arg.begins_with("--remote-control-token=")) {
			return arg.get_slice("=", 1);
		}
	}
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/remote_control/token")) {
		return String(GLOBAL_GET("blazium/remote_control/token"));
	}
	return String();
}

bool RemoteControlServer::configured_allow_eval() {
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/remote_control/allow_eval")) {
		return bool(GLOBAL_GET("blazium/remote_control/allow_eval"));
	}
	return false;
}

bool RemoteControlServer::_authorized(const Ref<HTTPRequestContext> &p_context, Ref<HTTPResponse> p_response) const {
	if (token.is_empty()) {
		return true;
	}
	String auth = p_context->get_header("Authorization");
	String expected = "Bearer " + token;
	String alt = p_context->get_header("X-Remote-Control-Token");
	if (auth == expected || alt == token) {
		return true;
	}
	_json_error(p_response, 401, "Unauthorized");
	return false;
}

void RemoteControlServer::_json_error(Ref<HTTPResponse> p_response, int p_status, const String &p_message) const {
	Dictionary err;
	err["ok"] = false;
	err["error"] = p_message;
	p_response->set_status(p_status);
	p_response->set_json(err);
}

Dictionary RemoteControlServer::_build_status() const {
	Dictionary status;
	status["ok"] = true;
	status["module"] = "remote_control";
	status["started"] = started;
	status["port"] = active_port > 0 ? active_port : (HTTPServer::get_singleton() ? HTTPServer::get_singleton()->get_port() : configured_port());
	status["bind_address"] = bind_address;
	status["pid"] = OS::get_singleton()->get_process_id();
	status["project_path"] = ProjectSettings::get_singleton()->get_resource_path();
	status["project_name"] = ProjectSettings::get_singleton()->get_setting("application/config/name", "");
	status["engine_version"] = Engine::get_singleton()->get_version_info();
	status["allow_eval"] = allow_eval;
	status["luau_eval_available"] = _luau_eval_available();
	status["token_required"] = !token.is_empty();
	status["instance_id"] = remote_instance_id;
#ifdef TOOLS_ENABLED
	status["mode"] = "editor";
#else
	status["mode"] = "runtime";
#endif
	status["timestamp"] = Time::get_singleton()->get_unix_time_from_system();
	return status;
}

bool RemoteControlServer::_is_valid_instance_id(const String &p_id) {
	if (p_id.length() != 6) {
		return false;
	}
	static const char *alphabet = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";
	for (int i = 0; i < p_id.length(); i++) {
		const char32_t c = p_id[i];
		bool ok = false;
		for (const char *p = alphabet; *p; p++) {
			if (c == (char32_t)*p) {
				ok = true;
				break;
			}
		}
		if (!ok) {
			return false;
		}
	}
	return true;
}

void RemoteControlServer::_handle_health(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	if (!_authorized(p_context, p_response)) {
		return;
	}
	Dictionary body;
	body["ok"] = true;
	body["status"] = "ok";
	body["instance_id"] = remote_instance_id;
	p_response->set_json(body);
}

void RemoteControlServer::_handle_status(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	if (!_authorized(p_context, p_response)) {
		return;
	}
	p_response->set_json(_build_status());
}

void RemoteControlServer::_handle_instance(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	if (!_authorized(p_context, p_response)) {
		return;
	}
	JSON json;
	Error err = json.parse(p_context->get_body());
	if (err != OK) {
		_json_error(p_response, 400, "Invalid JSON body");
		return;
	}
	Dictionary body = json.get_data();
	String id = String(body.get("instance_id", "")).strip_edges().to_upper();
	if (!_is_valid_instance_id(id)) {
		_json_error(p_response, 400, "instance_id must be 6 chars from 23456789ABCDEFGHJKLMNPQRSTUVWXYZ");
		return;
	}
	remote_instance_id = id;
	Dictionary ret;
	ret["ok"] = true;
	ret["instance_id"] = remote_instance_id;
	p_response->set_json(ret);
}

void RemoteControlServer::_handle_commands(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	if (!_authorized(p_context, p_response)) {
		return;
	}
	Dictionary body;
	body["ok"] = true;
	if (RemoteControlRegistry::get_singleton()) {
		body["commands"] = RemoteControlRegistry::get_singleton()->list_commands();
	} else {
		body["commands"] = Array();
	}
	p_response->set_json(body);
}

void RemoteControlServer::_deferred_exec(int p_client_id, String p_command, Dictionary p_args) {
	Dictionary result;
	if (!RemoteControlRegistry::get_singleton()) {
		result["ok"] = false;
		result["error"] = "RemoteControlRegistry not available";
	} else {
		result = RemoteControlRegistry::get_singleton()->execute(p_command, p_args);
	}
	Ref<HTTPResponse> response;
	response.instantiate();
	response->set_status(bool(result.get("ok", false)) ? 200 : 400);
	response->set_json(result);
	if (HTTPServer::get_singleton()) {
		HTTPServer::get_singleton()->complete_response(p_client_id, response);
	}
}

void RemoteControlServer::_deferred_eval(int p_client_id, String p_expression, String p_language) {
	Dictionary result = eval_expression(p_expression, p_language);
	Ref<HTTPResponse> response;
	response.instantiate();
	response->set_status(bool(result.get("ok", false)) ? 200 : 400);
	response->set_json(result);
	if (HTTPServer::get_singleton()) {
		HTTPServer::get_singleton()->complete_response(p_client_id, response);
	}
}

void RemoteControlServer::_handle_exec(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	if (!_authorized(p_context, p_response)) {
		return;
	}
	if (!RemoteControlRegistry::get_singleton()) {
		_json_error(p_response, 500, "RemoteControlRegistry not available");
		return;
	}

	JSON json;
	Error err = json.parse(p_context->get_body());
	if (err != OK) {
		_json_error(p_response, 400, "Invalid JSON body");
		return;
	}
	Dictionary body = json.get_data();
	String command = body.get("command", "");
	Dictionary args = body.get("args", Dictionary());
	if (command.is_empty()) {
		_json_error(p_response, 400, "Missing 'command'");
		return;
	}

	Dictionary result = RemoteControlRegistry::get_singleton()->execute(command, args);
	p_response->set_status(bool(result.get("ok", false)) ? 200 : 400);
	p_response->set_json(result);
}

void RemoteControlServer::_handle_eval(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	if (!_authorized(p_context, p_response)) {
		return;
	}
	if (!allow_eval) {
		_json_error(p_response, 403, "Eval is disabled. Enable blazium/remote_control/allow_eval.");
		return;
	}

	JSON json;
	Error err = json.parse(p_context->get_body());
	if (err != OK) {
		_json_error(p_response, 400, "Invalid JSON body");
		return;
	}
	Dictionary body = json.get_data();
	String expression = body.get("expression", body.get("expr", ""));
	String language = body.get("language", body.get("lang", "gdscript"));
	Dictionary result = eval_expression(expression, language);
	int status = 400;
	if (bool(result.get("ok", false))) {
		status = 200;
	} else if (String(result.get("error", "")).contains("not available")) {
		status = 503;
	}
	p_response->set_status(status);
	p_response->set_json(result);
}

void RemoteControlServer::_handle_cors(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	p_response->set_status(204);
	p_response->set_body("");
}

void RemoteControlServer::_handle_logs(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response) {
	if (!_authorized(p_context, p_response)) {
		return;
	}
	const uint64_t since = (uint64_t)p_context->get_query_param("since", "0").to_int();
	const uint64_t cursor = (uint64_t)p_context->get_query_param("cursor", "0").to_int();
	int limit = p_context->get_query_param("limit", "200").to_int();
	const bool errors_only = p_context->get_query_param("errors_only", "0") == "1" || p_context->get_query_param("errors_only", "") == "true";
	p_response->set_json(get_logs_page(since, cursor, limit, errors_only));
}

String RemoteControlServer::_normalize_language(const String &p_language) {
	const String lang = p_language.strip_edges().to_lower();
	if (lang.is_empty() || lang == "gdscript" || lang == "gd") {
		return "gdscript";
	}
	if (lang == "luau" || lang == "lua") {
		return "luau";
	}
	return String();
}

bool RemoteControlServer::_luau_eval_available() {
#if defined(MODULE_LUAU_MODULE_ENABLED)
	return ClassDB::class_exists("LuaState");
#else
	return false;
#endif
}

Dictionary RemoteControlServer::_eval_gdscript(const String &p_expression) const {
	Dictionary ret;
	ret["language"] = "gdscript";
	if (p_expression.is_empty()) {
		ret["ok"] = false;
		ret["error"] = "Missing expression";
		return ret;
	}
	String lower = p_expression.to_lower();
	for (int i = 0; _EVAL_BLOCKED_PATTERNS[i]; i++) {
		if (lower.contains(String(_EVAL_BLOCKED_PATTERNS[i]).to_lower())) {
			ret["ok"] = false;
			ret["error"] = vformat("Expression contains disallowed pattern: %s", _EVAL_BLOCKED_PATTERNS[i]);
			return ret;
		}
	}

	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	Node *context = nullptr;
	if (tree && tree->get_root()) {
		context = tree->get_root();
	}

	Expression expr;
	Error err = expr.parse(p_expression);
	if (err != OK) {
		ret["ok"] = false;
		ret["error"] = "Parse error: " + expr.get_error_text();
		return ret;
	}
	Variant result_v = expr.execute(Array(), context);
	if (expr.has_execute_failed()) {
		ret["ok"] = false;
		ret["error"] = "Execution error: " + expr.get_error_text();
		return ret;
	}
	ret["ok"] = true;
	ret["type"] = "eval_result";
	ret["result"] = result_v;
	ret["result_string"] = String(result_v);
	return ret;
}

Dictionary RemoteControlServer::_eval_luau(const String &p_expression) const {
	Dictionary ret;
	ret["language"] = "luau";
	if (p_expression.is_empty()) {
		ret["ok"] = false;
		ret["error"] = "Missing expression";
		return ret;
	}
#if !defined(MODULE_LUAU_MODULE_ENABLED)
	ret["ok"] = false;
	ret["error"] = "Luau module not available";
	return ret;
#else
	if (!_luau_eval_available()) {
		ret["ok"] = false;
		ret["error"] = "Luau module not available";
		return ret;
	}

	String lower = p_expression.to_lower();
	for (int i = 0; _LUAU_EVAL_BLOCKED_PATTERNS[i]; i++) {
		if (lower.contains(String(_LUAU_EVAL_BLOCKED_PATTERNS[i]).to_lower())) {
			ret["ok"] = false;
			ret["error"] = vformat("Expression contains disallowed pattern: %s", _LUAU_EVAL_BLOCKED_PATTERNS[i]);
			return ret;
		}
	}

	Ref<luau_module::LuaState> state;
	state.instantiate();
	state->open_libs(luau_module::LuaState::LIB_ALL);

	state->sandbox();

	const int top_before = state->get_top();
	const String trimmed = p_expression.strip_edges();
	const String lower_trim = trimmed.to_lower();
	const bool looks_like_statement = trimmed.contains(";") || trimmed.contains("\n") ||
			lower_trim.begins_with("local ") || lower_trim.begins_with("return ") ||
			lower_trim.begins_with("function ") || lower_trim.begins_with("if ") ||
			lower_trim.begins_with("for ") || lower_trim.begins_with("while ") ||
			lower_trim.begins_with("repeat ") || lower_trim.begins_with("do ");

	luau_module::LuaState::Status status = luau_module::LuaState::STATUS_ERRSYNTAX;
	bool used_return_wrap = false;
	if (!looks_like_statement) {
		used_return_wrap = true;
		status = state->do_string(vformat("return %s", p_expression), "@RemoteControlEval");
		if (status != luau_module::LuaState::STATUS_OK) {
			state->set_top(top_before);
			used_return_wrap = false;
		}
	}
	if (status != luau_module::LuaState::STATUS_OK) {
		status = state->do_string(p_expression, "@RemoteControlEval");
	}

	if (status == luau_module::LuaState::STATUS_OK) {
		const int results = state->get_top() - top_before;
		Variant result_v;
		if (results >= 1) {
			result_v = state->to_variant(-1);
		}
		state->set_top(top_before);
		ret["ok"] = true;
		ret["type"] = "eval_result";
		ret["result"] = result_v;
		ret["result_string"] = String(result_v);
		ret["return_wrapped"] = used_return_wrap;
		return ret;
	}

	String err_msg = "Luau execution failed";
	if (state->get_top() > top_before && state->is_string(-1)) {
		err_msg = state->to_string_inplace(-1);
	}
	state->set_top(top_before);
	ret["ok"] = false;
	ret["error"] = err_msg;
	return ret;
#endif
}

Dictionary RemoteControlServer::eval_expression(const String &p_expression, const String &p_language) const {
	const String language = _normalize_language(p_language);
	if (language.is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Unsupported language (use gdscript or luau)";
		ret["language"] = p_language;
		return ret;
	}
	if (language == "luau") {
		return _eval_luau(p_expression);
	}
	return _eval_gdscript(p_expression);
}

void RemoteControlServer::_register_routes() {
	HTTPServer *http = HTTPServer::get_singleton();
	ERR_FAIL_NULL(http);

	http->register_route("GET", "/v1/health", callable_mp(this, &RemoteControlServer::_handle_health));
	http->register_route("GET", "/v1/status", callable_mp(this, &RemoteControlServer::_handle_status));
	http->register_route("POST", "/v1/instance", callable_mp(this, &RemoteControlServer::_handle_instance));
	http->register_route("GET", "/v1/commands", callable_mp(this, &RemoteControlServer::_handle_commands));
	http->register_route("POST", "/v1/exec", callable_mp(this, &RemoteControlServer::_handle_exec));
	http->register_route("POST", "/v1/eval", callable_mp(this, &RemoteControlServer::_handle_eval));
	http->register_route("GET", "/v1/logs", callable_mp(this, &RemoteControlServer::_handle_logs));
	http->register_route("OPTIONS", "/v1/health", callable_mp(this, &RemoteControlServer::_handle_cors));
	http->register_route("OPTIONS", "/v1/status", callable_mp(this, &RemoteControlServer::_handle_cors));
	http->register_route("OPTIONS", "/v1/instance", callable_mp(this, &RemoteControlServer::_handle_cors));
	http->register_route("OPTIONS", "/v1/commands", callable_mp(this, &RemoteControlServer::_handle_cors));
	http->register_route("OPTIONS", "/v1/exec", callable_mp(this, &RemoteControlServer::_handle_cors));
	http->register_route("OPTIONS", "/v1/eval", callable_mp(this, &RemoteControlServer::_handle_cors));
	http->register_route("OPTIONS", "/v1/logs", callable_mp(this, &RemoteControlServer::_handle_cors));
}

void RemoteControlServer::_unregister_routes() {
	HTTPServer *http = HTTPServer::get_singleton();
	if (!http) {
		return;
	}
	http->unregister_route("GET", "/v1/health");
	http->unregister_route("GET", "/v1/status");
	http->unregister_route("POST", "/v1/instance");
	http->unregister_route("GET", "/v1/commands");
	http->unregister_route("POST", "/v1/exec");
	http->unregister_route("POST", "/v1/eval");
	http->unregister_route("GET", "/v1/logs");
	http->unregister_route("OPTIONS", "/v1/health");
	http->unregister_route("OPTIONS", "/v1/status");
	http->unregister_route("OPTIONS", "/v1/instance");
	http->unregister_route("OPTIONS", "/v1/commands");
	http->unregister_route("OPTIONS", "/v1/exec");
	http->unregister_route("OPTIONS", "/v1/eval");
	http->unregister_route("OPTIONS", "/v1/logs");
}

Error RemoteControlServer::start() {
	if (started) {
		return OK;
	}

	HTTPServer *http = HTTPServer::get_singleton();
	ERR_FAIL_NULL_V_MSG(http, ERR_UNAVAILABLE, "HTTPServer module singleton is not available.");

	token = configured_token();
	allow_eval = configured_allow_eval();
	bind_address = configured_bind_address();
	const int port = configured_port();

	owns_listen = false;
	if (!http->is_listening()) {
		Error err = http->listen(port, bind_address, false);
		if (err != OK) {
			ERR_PRINT(vformat("RemoteControl: failed to listen on %s:%d (error %d)", bind_address, port, err));
			return err;
		}
		owns_listen = true;
		active_port = port;
	} else {
		active_port = http->get_port();
	}

	http->set_cors_enabled(true);
	_register_routes();

	if (RemoteControlRegistry::get_singleton()) {
		RemoteControlRegistry::get_singleton()->register_builtins();
	}

	started = true;
	print_line(vformat("RemoteControl: listening on %s:%d (routes /v1/*)", bind_address, active_port));
	return OK;
}

void RemoteControlServer::stop() {
	if (!started) {
		return;
	}
	_unregister_routes();
	HTTPServer *http = HTTPServer::get_singleton();
	if (http && owns_listen && http->is_listening()) {
		http->stop();
	}
	owns_listen = false;
	started = false;
	active_port = 0;
	print_line("RemoteControl: stopped");
}

bool RemoteControlServer::is_started() const {
	return started;
}

int RemoteControlServer::get_port() const {
	return active_port;
}

Dictionary RemoteControlServer::get_status() const {
	return _build_status();
}

void RemoteControlServer::set_allow_eval(bool p_allow) {
	allow_eval = p_allow;
}

bool RemoteControlServer::get_allow_eval() const {
	return allow_eval;
}

void RemoteControlServer::set_token(const String &p_token) {
	token = p_token;
}

String RemoteControlServer::get_token() const {
	return token;
}

void RemoteControlServer::set_remote_instance_id(const String &p_id) {
	const String id = p_id.strip_edges().to_upper();
	ERR_FAIL_COND_MSG(!_is_valid_instance_id(id) && !id.is_empty(), "Invalid remote_control instance_id");
	remote_instance_id = id;
}

String RemoteControlServer::get_remote_instance_id() const {
	return remote_instance_id;
}

void RemoteControlServer::_print_handler_callback(void *p_user_data, const String &p_string, bool p_error, bool p_rich) {
	RemoteControlServer *server = static_cast<RemoteControlServer *>(p_user_data);
	if (!server) {
		return;
	}
	(void)p_rich;
	String level = "info";
	String text = p_string.strip_escapes();
	if (p_error) {
		level = "error";
		if (!text.begins_with("[ERROR]")) {
			text = "[ERROR] " + text;
		}
	} else if (text.contains("[WARNING]") || text.begins_with("WARNING:")) {
		level = "warning";
	}
	server->_append_log_line(text, level);
}

String RemoteControlServer::_entry_text_unlocked(const LogEntry &p_entry) const {
	if (p_entry.ref_id == 0) {
		return p_entry.text;
	}
	for (uint32_t i = 0; i < log_entries.size(); i++) {
		if (log_entries[i].id == p_entry.ref_id) {
			return log_entries[i].text;
		}
	}
	return String();
}

void RemoteControlServer::_append_log_line(const String &p_text, const String &p_level) {
	MutexLock lock(log_mutex);
	const double now = Time::get_singleton()->get_unix_time_from_system();

	if (!log_entries.is_empty()) {
		const LogEntry &prev = log_entries[log_entries.size() - 1];
		const String prev_text = _entry_text_unlocked(prev);
		if (prev_text == p_text) {
			if (prev.ref_id != 0) {
				log_entries[log_entries.size() - 1].repeat += 1;
				return;
			}
			LogEntry ref;
			ref.id = next_log_id++;
			ref.ref_id = prev.id;
			ref.repeat = 1;
			ref.level = p_level;
			ref.time = now;
			log_entries.push_back(ref);
			while ((int)log_entries.size() > LOG_MAX_RETAINED) {
				log_entries.remove_at(0);
			}
			return;
		}
	}

	LogEntry entry;
	entry.id = next_log_id++;
	entry.ref_id = 0;
	entry.repeat = 0;
	entry.text = p_text;
	entry.level = p_level;
	entry.time = now;
	log_entries.push_back(entry);
	while ((int)log_entries.size() > LOG_MAX_RETAINED) {
		log_entries.remove_at(0);
	}
}

Dictionary RemoteControlServer::_log_entry_to_dict(const LogEntry &p_entry) const {
	Dictionary d;
	d["id"] = (int64_t)p_entry.id;
	d["level"] = p_entry.level;
	d["time"] = p_entry.time;
	if (p_entry.ref_id != 0) {
		d["ref"] = (int64_t)p_entry.ref_id;
		d["repeat"] = p_entry.repeat;
	} else {
		d["text"] = p_entry.text;
	}
	return d;
}

Dictionary RemoteControlServer::get_logs_page(uint64_t p_since, uint64_t p_cursor, int p_limit, bool p_errors_only) const {
	int limit = CLAMP(p_limit <= 0 ? 200 : p_limit, 1, LOG_PAGE_MAX);
	MutexLock lock(log_mutex);

	Array entries;
	uint64_t oldest_id = 0;
	uint64_t newest_id = 0;
	if (!log_entries.is_empty()) {
		oldest_id = log_entries[0].id;
		newest_id = log_entries[log_entries.size() - 1].id;
	}

	uint64_t next_cursor = 0;
	bool truncated = false;
	for (uint32_t i = 0; i < log_entries.size(); i++) {
		const LogEntry &e = log_entries[i];
		if (p_since > 0 && e.id <= p_since) {
			continue;
		}
		if (p_cursor > 0 && e.id < p_cursor) {
			continue;
		}
		if (p_errors_only && e.level != "error" && e.level != "warning") {
			continue;
		}
		if (entries.size() >= limit) {
			truncated = true;
			next_cursor = e.id;
			break;
		}
		entries.push_back(_log_entry_to_dict(e));
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["entries"] = entries;
	ret["since_applied"] = (int64_t)p_since;
	ret["oldest_id"] = (int64_t)oldest_id;
	ret["newest_id"] = (int64_t)newest_id;
	ret["retained"] = (int)log_entries.size();
	ret["max_retained"] = LOG_MAX_RETAINED;
	ret["truncated"] = truncated;
	if (truncated) {
		ret["next_cursor"] = (int64_t)next_cursor;
	}
	return ret;
}

void RemoteControlServer::clear_logs() {
	MutexLock lock(log_mutex);
	log_entries.clear();
}

void RemoteControlServer::record_error_break(const String &p_source, int p_line, const String &p_message) {
	MutexLock lock(error_breaks_mutex);
	Dictionary d;
	d["time"] = Time::get_singleton()->get_unix_time_from_system();
	d["source"] = p_source;
	d["line"] = p_line;
	d["message"] = p_message;
	error_breaks.push_back(d);
	while ((int)error_breaks.size() > ERROR_BREAKS_MAX) {
		error_breaks.remove_at(0);
	}
}

Array RemoteControlServer::get_error_breaks() const {
	MutexLock lock(error_breaks_mutex);
	Array out;
	for (uint32_t i = 0; i < error_breaks.size(); i++) {
		out.push_back(error_breaks[i]);
	}
	return out;
}

void RemoteControlServer::clear_error_breaks() {
	MutexLock lock(error_breaks_mutex);
	error_breaks.clear();
}

bool RemoteControlServer::is_autowork_running() const {
	return autowork_job_running;
}

Dictionary RemoteControlServer::get_autowork_status() const {
	Dictionary ret = autowork_job_status;
	ret["ok"] = true;
	ret["job_id"] = autowork_job_id;
	ret["state"] = autowork_job_state;
	ret["results_path"] = autowork_results_path;
	return ret;
}

Dictionary RemoteControlServer::get_autowork_results() const {
	Dictionary ret;
	ret["ok"] = true;
	ret["job_id"] = autowork_job_id;
	ret["state"] = autowork_job_state;
	ret["results"] = autowork_job_results;
	ret["results_path"] = autowork_results_path;
	return ret;
}

Dictionary RemoteControlServer::start_autowork_job(const Dictionary &p_args) {
#if !defined(MODULE_AUTOWORK_ENABLED)
	Dictionary err;
	err["ok"] = false;
	err["error"] = "Autowork module not available";
	return err;
#else
	if (autowork_job_running) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "autowork already running";
		err["job_id"] = autowork_job_id;
		err["state"] = autowork_job_state;
		return err;
	}

	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (tree && tree->get_root()) {
		for (int i = 0; i < tree->get_root()->get_child_count(); i++) {
			Node *child = tree->get_root()->get_child(i);
			if (child && child->is_class("Autowork")) {
				Dictionary err;
				err["ok"] = false;
				err["error"] = "autowork already running";
				return err;
			}
		}
	}

	autowork_job_id = vformat("aw-%d", (int64_t)Time::get_singleton()->get_unix_time_from_system());
	autowork_job_state = "running";
	autowork_job_running = true;
	autowork_job_results = Dictionary();
	autowork_job_status = Dictionary();
	autowork_job_status["pass_count"] = 0;
	autowork_job_status["fail_count"] = 0;
	autowork_job_status["assert_count"] = 0;
	autowork_job_status["test_count"] = 0;
	autowork_job_status["pending_count"] = 0;

	call_deferred(SNAME("_deferred_run_autowork"), p_args);

	Dictionary ret;
	ret["ok"] = true;
	ret["job_id"] = autowork_job_id;
	ret["state"] = "running";
	return ret;
#endif
}

void RemoteControlServer::_deferred_run_autowork(Dictionary p_args) {
#if !defined(MODULE_AUTOWORK_ENABLED)
	(void)p_args;
	autowork_job_running = false;
	autowork_job_state = "failed";
	autowork_job_status["error"] = "Autowork module not available";
#else
	Autowork *aw = memnew(Autowork);
	String dir = p_args.get("dir", "");
	String file = p_args.get("file", "");
	String test_name = p_args.get("test_name", p_args.get("select", ""));
	String prefix = p_args.get("prefix", "");
	String suffix = p_args.get("suffix", "");
	(void)p_args.get("include_subdirs", true);

	if (!file.is_empty()) {
		aw->add_script(file);
	} else if (!dir.is_empty()) {
		aw->add_directory(dir, prefix, suffix);
	} else if (DirAccess::exists("res://test")) {
		aw->add_directory("res://test", prefix, suffix);
	} else {
		aw->add_directory("res://", prefix, suffix);
	}
	if (!test_name.is_empty()) {
		aw->set_test(test_name);
	}

	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (tree && tree->get_root()) {
		aw->set_name("RemoteControl_AutoworkInstance");
		tree->get_root()->add_child(aw);
	}

	aw->run_tests();

	Dictionary payload;
	payload["pass_count"] = aw->get_pass_count();
	payload["fail_count"] = aw->get_fail_count();
	payload["assert_count"] = aw->get_assert_count();
	payload["test_count"] = aw->get_test_count();
	payload["pending_count"] = aw->get_pending_count();

	Array failures;
	if (aw->get_logger().is_valid()) {
		const Vector<AutoworkTestMethodResult> &test_results = aw->get_logger()->get_test_results();
		for (int i = 0; i < test_results.size(); i++) {
			const AutoworkTestMethodResult &tr = test_results[i];
			if (tr.fails > 0 || tr.fail_messages.size() > 0) {
				Dictionary f;
				f["script"] = tr.script_name;
				f["method"] = tr.method_name;
				Array messages;
				for (int j = 0; j < tr.fail_messages.size(); j++) {
					messages.push_back(tr.fail_messages[j]);
				}
				f["messages"] = messages;
				failures.push_back(f);
			}
		}
		aw->get_logger()->export_json(autowork_results_path);
	}
	payload["failures"] = failures;

	autowork_job_status = payload;
	autowork_job_results = payload;
	autowork_job_state = "done";
	autowork_job_running = false;

	if (tree && tree->get_root() && aw->get_parent()) {
		tree->get_root()->remove_child(aw);
	}
	aw->queue_free();
#endif
}

RemoteControlServer::RemoteControlServer() {
	singleton = this;
	print_handler.printfunc = _print_handler_callback;
	print_handler.userdata = this;
	add_print_handler(&print_handler);
}

RemoteControlServer::~RemoteControlServer() {
	remove_print_handler(&print_handler);
	stop();
	if (singleton == this) {
		singleton = nullptr;
	}
}
