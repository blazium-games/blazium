/**************************************************************************/
/*  remote_control_server.h                                               */
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

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/os/mutex.h"
#include "core/string/print_string.h"
#include "core/templates/local_vector.h"
#include "modules/httpserver/http_request_context.h"
#include "modules/httpserver/http_response.h"

class RemoteControlServer : public Object {
	GDCLASS(RemoteControlServer, Object);

public:
	static const int LOG_MAX_RETAINED = 10000;
	static const int LOG_PAGE_MAX = 1000;
	static const int ERROR_BREAKS_MAX = 200;

private:
	struct LogEntry {
		uint64_t id = 0;
		uint64_t ref_id = 0; // 0 = owns text; otherwise references another entry's text
		int repeat = 0; // for ref entries: extra occurrences beyond the owner
		String text;
		String level;
		double time = 0.0;
	};

	static RemoteControlServer *singleton;

	bool started = false;
	bool owns_listen = false;
	int active_port = 0;
	String bind_address = "127.0.0.1";
	String token;
	String remote_instance_id;
	bool allow_eval = false;
	mutable Mutex server_state_mutex;

	PrintHandlerList print_handler;
	mutable Mutex log_mutex;
	LocalVector<LogEntry> log_entries;
	uint64_t next_log_id = 1;

	mutable Mutex error_breaks_mutex;
	LocalVector<Dictionary> error_breaks;

	mutable Mutex autowork_mutex;
	String autowork_job_id;
	String autowork_job_state = "idle"; // idle|running|done|failed
	Dictionary autowork_job_status;
	Dictionary autowork_job_results;
	String autowork_results_path = "user://autowork_results.json";
	bool autowork_job_running = false;

	bool _authorized(const Ref<HTTPRequestContext> &p_context, Ref<HTTPResponse> p_response) const;
	void _json_error(Ref<HTTPResponse> p_response, int p_status, const String &p_message) const;
	Dictionary _build_status() const;
	static bool _is_valid_instance_id(const String &p_id);

	void _handle_health(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response);
	void _handle_status(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response);
	void _handle_instance(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response);
	void _handle_commands(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response);
	void _handle_exec(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response);
	void _handle_eval(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response);
	void _handle_logs(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response);
	void _handle_cors(Ref<HTTPRequestContext> p_context, Ref<HTTPResponse> p_response);

	void _deferred_exec(int p_client_id, String p_command, Dictionary p_args);
	void _deferred_eval(int p_client_id, String p_expression, String p_language);
	void _deferred_run_autowork(Dictionary p_args);

	Dictionary _eval_gdscript(const String &p_expression) const;
	Dictionary _eval_luau(const String &p_expression) const;
	static String _normalize_language(const String &p_language);
	static bool _luau_eval_available();

	void _register_routes();
	void _unregister_routes();

	static void _print_handler_callback(void *p_user_data, const String &p_string, bool p_error, bool p_rich);
	void _append_log_line(const String &p_text, const String &p_level);
	String _entry_text_unlocked(const LogEntry &p_entry) const;
	Dictionary _log_entry_to_dict(const LogEntry &p_entry) const;

protected:
	static void _bind_methods();

public:
	static RemoteControlServer *get_singleton();

	Error start();
	void stop();
	bool is_started() const;
	int get_port() const;
	Dictionary get_status() const;

	void set_allow_eval(bool p_allow);
	bool get_allow_eval() const;
	void set_token(const String &p_token);
	String get_token() const;
	void set_remote_instance_id(const String &p_id);
	String get_remote_instance_id() const;

	Dictionary eval_expression(const String &p_expression, const String &p_language = "gdscript") const;

	Dictionary get_logs_page(uint64_t p_since, uint64_t p_cursor, int p_limit, bool p_errors_only) const;
	void clear_logs();

	void record_error_break(const String &p_source, int p_line, const String &p_message);
	Array get_error_breaks() const;
	void clear_error_breaks();

	Dictionary start_autowork_job(const Dictionary &p_args);
	Dictionary get_autowork_status() const;
	Dictionary get_autowork_results() const;
	bool is_autowork_running() const;

	static bool should_enable_from_cmdline_or_settings();
	static int configured_port();
	static String configured_bind_address();
	static String configured_token();
	static bool configured_allow_eval();

	RemoteControlServer();
	~RemoteControlServer();
};
