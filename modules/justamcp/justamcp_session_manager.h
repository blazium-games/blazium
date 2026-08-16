/**************************************************************************/
/*  justamcp_session_manager.h                                            */
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

#include "modules/modules_enabled.gen.h"

#include "justamcp_event_store.h"
#include "justamcp_request_router.h"

#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/variant/dictionary.h"

#if defined(MODULE_HTTPSERVER_ENABLED)
#include "modules/httpserver/http_request_context.h"
#include "modules/httpserver/http_response.h"
#endif

class JustAMCPServer;

struct MCPSSEStream {
	String stream_id;
	String session_id;
	int http_connection_id = -1;
	MCPEventStore event_store;
	bool is_post_stream = false;
	bool response_sent = false;
};

struct MCPSession {
	String session_id;
	String negotiated_protocol = "2025-11-25";
	uint64_t created_usec = 0;
	uint64_t last_activity_usec = 0;
	bool initialized = false;
	HashMap<String, MCPSSEStream> streams;
	HashMap<int, String> connection_to_stream;
	int active_tool_connection_id = -1;
	int last_registered_connection_id = -1;
};

class MCPSessionManager {
	JustAMCPServer *owner = nullptr;
	HashMap<String, MCPSession> sessions;
	HashMap<int, String> connection_to_session;
	Mutex mutex;
	uint64_t next_stream_counter = 1;

	struct PendingPostSse {
		String body;
		String session_id;
		bool wants_sse_response = false;
		bool requires_json_and_sse_accept = true;

		bool claim_armed = false;
		uint64_t created_usec = 0;
	};
	HashMap<String, PendingPostSse> pending_post_sse_by_session;

	HashSet<String> post_sse_upgrade_sessions;
	HashSet<int> all_sse_connection_ids;

	JustAMCPRequestRouter request_router;

public:
	explicit MCPSessionManager(JustAMCPServer *p_owner);

	void clear_all();

	static const char *hardcoded_latest_protocol_version() { return "2025-11-25"; }
	static bool is_supported_protocol_version(const String &p_version);
	static String latest_protocol_version();
	static String negotiate_protocol_version(const String &p_client_version);
	static bool set_cli_protocol_version_override(const String &p_version);
	static void clear_cli_protocol_version_override();

#if defined(MODULE_HTTPSERVER_ENABLED)
	static String get_header(const Ref<HTTPRequestContext> &p_context, const String &p_name);
	static bool validate_origin(const Ref<HTTPRequestContext> &p_context);
	static bool is_allowed_origin_string(const String &p_origin);
	static bool accepts_json_and_sse_header(const String &p_accept);
	static bool validate_protocol_header(const Ref<HTTPRequestContext> &p_context, String &r_error);
	static bool accepts_json_and_sse(const Ref<HTTPRequestContext> &p_context);
	static bool accepts_event_stream(const Ref<HTTPRequestContext> &p_context);

	void apply_cors_headers(Ref<HTTPResponse> p_response, const Ref<HTTPRequestContext> &p_context) const;
	void handle_cors_preflight(const Ref<HTTPRequestContext> &p_context, Ref<HTTPResponse> p_response) const;

	bool handle_mcp_get(const Ref<HTTPRequestContext> &p_context, Ref<HTTPResponse> p_response);
	bool handle_mcp_post(const Ref<HTTPRequestContext> &p_context, Ref<HTTPResponse> p_response);
	bool handle_mcp_delete(const Ref<HTTPRequestContext> &p_context, Ref<HTTPResponse> p_response);

	void on_sse_connection_opened(int p_connection_id, const String &p_path, const Dictionary &p_headers);
	void on_sse_connection_closed(int p_connection_id);
	void complete_post_sse_stream(int p_connection_id);
	bool is_post_stream_connection(int p_connection_id) const;
	void complete_post_sse_stream_if_needed(int p_connection_id);

	bool send_json_on_connection(int p_connection_id, const String &p_json, const String &p_event_type = "message");
	bool send_json_to_session(const String &p_session_id, const String &p_json, int p_preferred_connection_id = -1);
	bool send_json_legacy_connection(int p_connection_id, const String &p_json);

	void set_session_active_tool_connection(const String &p_session_id, int p_connection_id);
	void bind_request_tool_route(const Variant &p_request_id, const String &p_session_id, int p_connection_id);
	bool get_request_tool_route(const Variant &p_request_id, String &r_session_id, int &r_connection_id) const;
	void clear_request_tool_route(const Variant &p_request_id);
	void clear_request_routes_for_connection(int p_connection_id);

	bool session_exists(const String &p_session_id) const;
	void mark_session_initialized(const String &p_session_id);
	void deferred_replay_stream_events(int p_connection_id, const String &p_last_event_id);
	Vector<int> collect_session_connections(const String &p_session_id) const;
	Vector<int> collect_all_broadcast_connection_ids() const;
	Vector<int> snapshot_broadcast_targets() const;
	void track_legacy_broadcast_connection(int p_connection_id);
	void untrack_legacy_broadcast_connection(int p_connection_id);
	void prune_request_routes();

#ifdef TESTS_ENABLED
	bool test_has_pending_post_sse_for_session(const String &p_session_id) const;
	void test_set_pending_post_sse(const String &p_session_id, const String &p_body);
	void test_arm_pending_post_sse_claim(const String &p_session_id);
	void test_set_pending_post_sse_created_usec(const String &p_session_id, uint64_t p_created_usec);
	void test_prune_expired_pending_post_sse();
	String test_peek_pending_post_sse_body(const String &p_session_id) const;
	bool test_register_stream_for_session(const String &p_session_id, int p_connection_id);
	int test_get_active_tool_connection_id(const String &p_session_id) const;
	int test_get_last_registered_connection_id(const String &p_session_id) const;
	int test_connection_map_size() const;
	bool test_has_connection_mapping(int p_connection_id) const;
	int test_count_send_json_on_connection_calls() const;
	void test_simulate_session_delete(const String &p_session_id);
	bool test_try_set_pending_post_sse(const String &p_session_id, const String &p_body);
	void test_backdate_request_route(const Variant &p_request_id, uint64_t p_age_usec);
	void test_set_request_route_created_usec(const Variant &p_request_id, uint64_t p_created_usec);
	void test_prune_request_routes_usec(uint64_t p_ttl_usec);
#endif
#endif

private:
#if defined(MODULE_HTTPSERVER_ENABLED)
	String _generate_session_id() const;
	String _generate_stream_id();
	MCPSession *_get_session(const String &p_session_id);
	const MCPSession *_get_session(const String &p_session_id) const;
	MCPSSEStream *_get_stream_for_connection(int p_connection_id);
	void _touch_session(MCPSession &p_session);
	void _expire_sessions();
	void _prune_expired_pending_post_sse();
	bool _register_stream(MCPSession &p_session, int p_connection_id, bool p_is_post_stream, MCPSSEStream *&r_stream);
	void _unregister_connection(int p_connection_id);
	void _replay_stream_events(MCPSSEStream &p_stream, const String &p_last_event_id);
	void _send_priming_event(int p_connection_id);
	void _process_post_sse_opened(int p_connection_id, const String &p_session_id);
	int _resolve_active_tool_connection_for_session(const MCPSession &p_session) const;
	void _process_get_sse_opened(int p_connection_id, const String &p_session_id, const String &p_last_event_id);

#ifdef TESTS_ENABLED
	mutable int test_send_json_on_connection_calls = 0;
#endif
#endif
};
