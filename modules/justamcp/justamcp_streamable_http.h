/**************************************************************************/
/*  justamcp_streamable_http.h                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#pragma once

#include "justamcp_event_store.h"

#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
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
	String negotiated_protocol = "2024-11-05";
	uint64_t created_usec = 0;
	uint64_t last_activity_usec = 0;
	bool initialized = false;
	HashMap<String, MCPSSEStream> streams;
	HashMap<int, String> connection_to_stream;
	int active_tool_connection_id = -1;
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
	};
	PendingPostSse pending_post_sse;
	bool has_pending_post_sse = false;

	String tool_route_session_id;
	int tool_route_connection_id = -1;

public:
	explicit MCPSessionManager(JustAMCPServer *p_owner);

	void clear_all();

#if defined(MODULE_HTTPSERVER_ENABLED)
	static String get_header(const Ref<HTTPRequestContext> &p_context, const String &p_name);
	static bool validate_origin(const Ref<HTTPRequestContext> &p_context);
	static bool validate_protocol_header(const Ref<HTTPRequestContext> &p_context, String &r_error);
	static bool accepts_json_and_sse(const Ref<HTTPRequestContext> &p_context);
	static bool accepts_event_stream(const Ref<HTTPRequestContext> &p_context);
	static String negotiate_protocol_version(const String &p_client_version);

	void apply_cors_headers(Ref<HTTPResponse> p_response, const Ref<HTTPRequestContext> &p_context) const;
	void handle_cors_preflight(const Ref<HTTPRequestContext> &p_context, Ref<HTTPResponse> p_response) const;

	bool handle_mcp_get(const Ref<HTTPRequestContext> &p_context, Ref<HTTPResponse> p_response);
	bool handle_mcp_post(const Ref<HTTPRequestContext> &p_context, Ref<HTTPResponse> p_response);
	bool handle_mcp_delete(const Ref<HTTPRequestContext> &p_context, Ref<HTTPResponse> p_response);

	void on_sse_connection_opened(int p_connection_id, const String &p_path, const Dictionary &p_headers);
	void on_sse_connection_closed(int p_connection_id);

	bool send_json_on_connection(int p_connection_id, const String &p_json, const String &p_event_type = "message");
	bool send_json_to_session(const String &p_session_id, const String &p_json, int p_preferred_connection_id = -1);
	bool send_json_legacy_connection(int p_connection_id, const String &p_json);

	void bind_tool_route(const String &p_session_id, int p_connection_id);
	void clear_tool_route();
	String get_tool_route_session_id() const;
	int get_tool_route_connection_id() const;

	bool session_exists(const String &p_session_id) const;
	void broadcast_json(const String &p_json);
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
	bool _register_stream(MCPSession &p_session, int p_connection_id, bool p_is_post_stream, MCPSSEStream *&r_stream);
	void _unregister_connection(int p_connection_id);
	void _replay_stream_events(MCPSSEStream &p_stream, const String &p_last_event_id);
	void _send_priming_event(int p_connection_id);
	void _process_post_sse_opened(int p_connection_id);
	void _process_get_sse_opened(int p_connection_id, const String &p_session_id, const String &p_last_event_id);
#endif
};
