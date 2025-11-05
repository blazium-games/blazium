/**************************************************************************/
/*  crowd_control.cpp                                                     */
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

#include "crowd_control.h"

#include "core/core_bind.h"
#include "core/io/json.h"
#include "core/os/os.h"
#include "core/os/time.h"

void CrowdControl::_bind_methods() {
	// Connection Management
	ClassDB::bind_method(D_METHOD("connect_to_crowdcontrol"), &CrowdControl::connect_to_crowdcontrol);
	ClassDB::bind_method(D_METHOD("close"), &CrowdControl::close);
	ClassDB::bind_method(D_METHOD("poll"), &CrowdControl::poll);
	ClassDB::bind_method(D_METHOD("is_websocket_connected"), &CrowdControl::is_websocket_connected);
	ClassDB::bind_method(D_METHOD("is_authenticated"), &CrowdControl::is_authenticated);

	// Authentication Flow
	ClassDB::bind_method(D_METHOD("request_authentication"), &CrowdControl::request_authentication);
	ClassDB::bind_method(D_METHOD("get_authentication_url"), &CrowdControl::get_authentication_url);

	// Session Management
	ClassDB::bind_method(D_METHOD("start_game_session", "game_pack_id"), &CrowdControl::start_game_session);
	ClassDB::bind_method(D_METHOD("stop_game_session"), &CrowdControl::stop_game_session);
	ClassDB::bind_method(D_METHOD("get_game_session_id"), &CrowdControl::get_game_session_id);
	ClassDB::bind_method(D_METHOD("request_interact_link"), &CrowdControl::request_interact_link);

	// Effect Response
	ClassDB::bind_method(D_METHOD("respond_to_effect_instant", "request_id", "status", "message"), &CrowdControl::respond_to_effect_instant, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("respond_to_effect_timed", "request_id", "status", "time_remaining", "message"), &CrowdControl::respond_to_effect_timed, DEFVAL(""));

	// Effect Reports
	ClassDB::bind_method(D_METHOD("report_effects", "effect_ids", "status", "identifier_type"), &CrowdControl::report_effects, DEFVAL("effect"));

	// Getters
	ClassDB::bind_method(D_METHOD("get_cc_uid"), &CrowdControl::get_cc_uid);
	ClassDB::bind_method(D_METHOD("get_username"), &CrowdControl::get_username);
	ClassDB::bind_method(D_METHOD("get_connection_id"), &CrowdControl::get_connection_id);
	ClassDB::bind_method(D_METHOD("get_auth_token"), &CrowdControl::get_auth_token);

	// Signals - Connection
	ADD_SIGNAL(MethodInfo("connection_established"));
	ADD_SIGNAL(MethodInfo("connection_closed", PropertyInfo(Variant::INT, "code"), PropertyInfo(Variant::STRING, "reason")));
	ADD_SIGNAL(MethodInfo("connection_error", PropertyInfo(Variant::STRING, "error")));

	// Signals - Authentication
	ADD_SIGNAL(MethodInfo("authentication_url_ready", PropertyInfo(Variant::STRING, "url")));
	ADD_SIGNAL(MethodInfo("authenticated", PropertyInfo(Variant::STRING, "token"), PropertyInfo(Variant::STRING, "cc_uid"), PropertyInfo(Variant::STRING, "username")));

	// Signals - Session
	ADD_SIGNAL(MethodInfo("game_session_started", PropertyInfo(Variant::STRING, "session_id")));
	ADD_SIGNAL(MethodInfo("game_session_stopped"));
	ADD_SIGNAL(MethodInfo("interact_link_received", PropertyInfo(Variant::STRING, "link")));

	// Signals - Effects
	ADD_SIGNAL(MethodInfo("effect_requested", PropertyInfo(Variant::DICTIONARY, "effect_data")));

	// Signals - Errors
	ADD_SIGNAL(MethodInfo("rpc_error", PropertyInfo(Variant::STRING, "method"), PropertyInfo(Variant::STRING, "error")));

	// Enums - EffectStatus
	BIND_ENUM_CONSTANT(STATUS_SUCCESS);
	BIND_ENUM_CONSTANT(STATUS_FAILURE);
	BIND_ENUM_CONSTANT(STATUS_UNAVAILABLE);
	BIND_ENUM_CONSTANT(STATUS_RETRY);
	BIND_ENUM_CONSTANT(STATUS_QUEUE);
	BIND_ENUM_CONSTANT(STATUS_TIMED_BEGIN);
	BIND_ENUM_CONSTANT(STATUS_TIMED_PAUSE);
	BIND_ENUM_CONSTANT(STATUS_TIMED_RESUME);
	BIND_ENUM_CONSTANT(STATUS_TIMED_END);

	// Enums - EffectReportStatus
	BIND_ENUM_CONSTANT(MENU_VISIBLE);
	BIND_ENUM_CONSTANT(MENU_HIDDEN);
	BIND_ENUM_CONSTANT(MENU_AVAILABLE);
	BIND_ENUM_CONSTANT(MENU_UNAVAILABLE);
}

// Note: Polling must be called manually from the game's _process() or similar
// This is because CrowdControl inherits from Object, not Node, so it doesn't receive notifications

// Connection Management

Error CrowdControl::connect_to_crowdcontrol() {
	if (ws.is_valid() && ws->get_ready_state() != WebSocketPeer::STATE_CLOSED) {
		WARN_PRINT("CrowdControl: Already connected or connecting.");
		return ERR_ALREADY_IN_USE;
	}

	ws = Ref<WebSocketPeer>(WebSocketPeer::create());
	ERR_FAIL_COND_V(ws.is_null(), ERR_CANT_CREATE);

	// Reset connection flag
	connection_signal_emitted = false;

	// Set User-Agent header
	Vector<String> headers;
	headers.push_back("User-Agent: Blazium-CrowdControl/1.0");
	ws->set_handshake_headers(headers);

	// Connect to Crowd Control PubSub WebSocket
	Ref<TLSOptions> tls_options = TLSOptions::client();
	Error err = ws->connect_to_url("wss://pubsub.crowdcontrol.live/", tls_options);

	if (err != OK) {
		emit_signal("connection_error", "Failed to initiate WebSocket connection");
		ws.unref();
		return err;
	}

	return OK;
}

void CrowdControl::close() {
	if (ws.is_valid() && ws->get_ready_state() != WebSocketPeer::STATE_CLOSED) {
		ws->close(1000, "Client disconnect");

		// Poll until closed
		while (ws->get_ready_state() != WebSocketPeer::STATE_CLOSED) {
			ws->poll();
			OS::get_singleton()->delay_usec(10000); // 10ms
		}

		ws.unref();
	}

	// Reset state
	authenticated = false;
	connection_signal_emitted = false;
	connection_id = "";
	auth_token = "";
	cc_uid = "";
	username = "";
	game_session_id = "";
	game_pack_id = "";
}

void CrowdControl::poll() {
	if (ws.is_null()) {
		return;
	}

	ws->poll();

	WebSocketPeer::State state = ws->get_ready_state();

	switch (state) {
		case WebSocketPeer::STATE_CONNECTING:
			// Still connecting...
			break;

		case WebSocketPeer::STATE_OPEN: {
			// Connection established - emit signal once per connection
			if (!connection_signal_emitted) {
				emit_signal("connection_established");
				connection_signal_emitted = true;
			}
			// Process incoming messages
			_process_messages();
		} break;

		case WebSocketPeer::STATE_CLOSING:
			// Closing handshake in progress
			break;

		case WebSocketPeer::STATE_CLOSED: {
			// Connection closed
			int code = ws->get_close_code();
			String reason = ws->get_close_reason();
			emit_signal("connection_closed", code, reason);
		} break;
	}
}

bool CrowdControl::is_websocket_connected() const {
	return ws.is_valid() && ws->get_ready_state() == WebSocketPeer::STATE_OPEN;
}

bool CrowdControl::is_authenticated() const {
	return authenticated;
}

// Authentication Flow

Error CrowdControl::request_authentication() {
	ERR_FAIL_COND_V(!is_websocket_connected(), ERR_UNCONFIGURED);

	// Send whoami request
	Dictionary request;
	request["action"] = "whoami";

	String json_str = JSON::stringify(request);
	Error err = ws->send_text(json_str);

	if (err != OK) {
		emit_signal("connection_error", "Failed to send whoami request");
	}

	return err;
}

String CrowdControl::get_authentication_url() const {
	if (connection_id.is_empty()) {
		return "";
	}
	return "https://auth.crowdcontrol.live/?connectionID=" + connection_id;
}

// Session Management

Error CrowdControl::start_game_session(const String &p_game_pack_id) {
	ERR_FAIL_COND_V(!is_authenticated(), ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(p_game_pack_id.is_empty(), ERR_INVALID_PARAMETER);

	game_pack_id = p_game_pack_id;

	Dictionary session_args;
	session_args["id"] = _generate_unique_id();
	session_args["gamePackID"] = p_game_pack_id;
	session_args["stamp"] = _get_unix_timestamp();

	Array args;
	args.push_back(session_args);

	_send_rpc("startGameSession", args);

	return OK;
}

Error CrowdControl::stop_game_session() {
	ERR_FAIL_COND_V(!is_authenticated(), ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(game_session_id.is_empty(), ERR_UNCONFIGURED);

	Dictionary session_args;
	session_args["id"] = _generate_unique_id();
	session_args["gameSessionID"] = game_session_id;
	session_args["stamp"] = _get_unix_timestamp();

	Array args;
	args.push_back(session_args);

	_send_rpc("stopGameSession", args);

	return OK;
}

String CrowdControl::get_game_session_id() const {
	return game_session_id;
}

void CrowdControl::request_interact_link() {
	if (!is_authenticated()) {
		ERR_PRINT("CrowdControl: Must be authenticated to request interact link");
		return;
	}

	Array args; // Empty args for getInteractLink
	_send_rpc("getInteractLink", args);
}

// Effect Response

Error CrowdControl::respond_to_effect_instant(const String &p_request_id, EffectStatus p_status, const String &p_message) {
	ERR_FAIL_COND_V(!is_authenticated(), ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(p_request_id.is_empty(), ERR_INVALID_PARAMETER);

	Dictionary response_args;
	response_args["id"] = _generate_unique_id();
	response_args["request"] = p_request_id;
	response_args["status"] = _effect_status_to_string(p_status);
	response_args["message"] = p_message;
	response_args["stamp"] = _get_unix_timestamp();

	Array args;
	args.push_back(response_args);

	_send_rpc("effectResponse", args);

	return OK;
}

Error CrowdControl::respond_to_effect_timed(const String &p_request_id, EffectStatus p_status, int p_time_remaining, const String &p_message) {
	ERR_FAIL_COND_V(!is_authenticated(), ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(p_request_id.is_empty(), ERR_INVALID_PARAMETER);

	Dictionary response_args;
	response_args["id"] = _generate_unique_id();
	response_args["request"] = p_request_id;
	response_args["status"] = _effect_status_to_string(p_status);
	response_args["message"] = p_message;
	response_args["stamp"] = _get_unix_timestamp();

	// Add timeRemaining for timed statuses (except timedEnd)
	if (p_status != STATUS_TIMED_END) {
		response_args["timeRemaining"] = p_time_remaining;
	}

	Array args;
	args.push_back(response_args);

	_send_rpc("effectResponse", args);

	return OK;
}

// Effect Reports

Error CrowdControl::report_effects(const PackedStringArray &p_effect_ids, EffectReportStatus p_status, const String &p_identifier_type) {
	ERR_FAIL_COND_V(!is_authenticated(), ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(p_effect_ids.size() == 0, ERR_INVALID_PARAMETER);

	Array ids_array;
	for (int i = 0; i < p_effect_ids.size(); i++) {
		ids_array.push_back(p_effect_ids[i]);
	}

	Dictionary report_args;
	report_args["id"] = _generate_unique_id();
	report_args["identifierType"] = p_identifier_type;
	report_args["ids"] = ids_array;
	report_args["status"] = _report_status_to_string(p_status);
	report_args["stamp"] = _get_unix_timestamp();

	Array args;
	args.push_back(report_args);

	_send_rpc("effectReport", args);

	return OK;
}

// Getters

String CrowdControl::get_cc_uid() const {
	return cc_uid;
}

String CrowdControl::get_username() const {
	return username;
}

String CrowdControl::get_connection_id() const {
	return connection_id;
}

String CrowdControl::get_auth_token() const {
	return auth_token;
}

// Internal Methods

void CrowdControl::_process_messages() {
	while (ws->get_available_packet_count() > 0) {
		const uint8_t *buffer;
		int buffer_size;

		if (ws->get_packet(&buffer, buffer_size) == OK) {
			if (ws->was_string_packet()) {
				String message;
				message.parse_utf8((const char *)buffer, buffer_size);
				_handle_message(message);
			}
		}
	}
}

void CrowdControl::_handle_message(const String &p_message) {
	Ref<JSON> json;
	json.instantiate();
	Error err = json->parse(p_message);

	if (err != OK) {
		ERR_PRINT(vformat("CrowdControl: Failed to parse JSON message: %s", p_message));
		return;
	}

	Dictionary data = json->get_data();

	if (!data.has("domain") || !data.has("type")) {
		ERR_PRINT("CrowdControl: Received message without domain or type");
		return;
	}

	String domain = data["domain"];
	String type = data["type"];
	Dictionary payload = data.has("payload") ? Dictionary(data["payload"]) : Dictionary();

	if (domain == "direct") {
		_handle_direct_event(type, payload);
	} else if (domain == "pub") {
		_handle_pub_event(type, payload);
	}
}

void CrowdControl::_handle_direct_event(const String &p_type, const Dictionary &p_payload) {
	if (p_type == "whoami") {
		// Store connection ID
		if (p_payload.has("connectionID")) {
			connection_id = p_payload["connectionID"];
			String auth_url = get_authentication_url();
			emit_signal("authentication_url_ready", auth_url);
		}
	} else if (p_type == "login-success") {
		// Store authentication token
		if (p_payload.has("token")) {
			auth_token = p_payload["token"];
			authenticated = true;

			// Parse JWT to extract ccUID and username
			// JWT format: header.payload.signature
			PackedStringArray parts = auth_token.split(".");
			if (parts.size() >= 2) {
				String payload_b64 = parts[1];
				// Decode base64 JWT payload
				Vector<uint8_t> decoded = core_bind::Marshalls::get_singleton()->base64_to_raw(payload_b64);
				String payload_str;
				payload_str.parse_utf8((const char *)decoded.ptr(), decoded.size());

				Ref<JSON> jwt_json;
				jwt_json.instantiate();
				if (jwt_json->parse(payload_str) == OK) {
					Dictionary jwt_data = jwt_json->get_data();
					if (jwt_data.has("ccUID")) {
						cc_uid = jwt_data["ccUID"];
					}
					if (jwt_data.has("name")) {
						username = jwt_data["name"];
					}
				}
			}

			emit_signal("authenticated", auth_token, cc_uid, username);

			// Auto-subscribe to pub domain
			_send_subscribe();
		}
	} else if (p_type == "subscription-result") {
		// Subscription confirmed
		if (p_payload.has("success")) {
			Array success = p_payload["success"];
			// Could emit signal or log if needed
		}
		if (p_payload.has("failure")) {
			Array failure = p_payload["failure"];
			if (failure.size() > 0) {
				ERR_PRINT("CrowdControl: Some subscriptions failed");
			}
		}
	}
}

void CrowdControl::_handle_pub_event(const String &p_type, const Dictionary &p_payload) {
	if (p_type == "game-session-start") {
		if (p_payload.has("gameSessionID")) {
			game_session_id = p_payload["gameSessionID"];
			emit_signal("game_session_started", game_session_id);
		}
	} else if (p_type == "game-session-stop") {
		game_session_id = "";
		emit_signal("game_session_stopped");
	} else if (p_type == "effect-request") {
		// Build effect data dictionary
		Dictionary effect_data;

		if (p_payload.has("effect")) {
			Dictionary effect = p_payload["effect"];
			effect_data["effect_id"] = effect.has("effectID") ? effect["effectID"] : Variant("");
			effect_data["name"] = effect.has("name") ? effect["name"] : Variant("");
			effect_data["duration"] = effect.has("duration") ? effect["duration"] : Variant(0);
			effect_data["parameters"] = effect.has("parameters") ? effect["parameters"] : Variant(Array());
		}

		if (p_payload.has("request")) {
			Dictionary request = p_payload["request"];
			effect_data["request_id"] = request.has("id") ? request["id"] : Variant("");
			effect_data["viewer_name"] = "";
			effect_data["viewer_profile"] = "";
			effect_data["viewer_profile_id"] = "";

			if (request.has("viewer")) {
				Dictionary viewer = request["viewer"];
				effect_data["viewer_name"] = viewer.has("name") ? viewer["name"] : Variant("");
				effect_data["viewer_profile"] = viewer.has("profile") ? viewer["profile"] : Variant("");
				effect_data["viewer_profile_id"] = viewer.has("profileID") ? viewer["profileID"] : Variant("");
			}

			effect_data["cost"] = request.has("cost") ? request["cost"] : Variant(0);
			effect_data["message"] = request.has("message") ? request["message"] : Variant("");
		}

		emit_signal("effect_requested", effect_data);
	}
}

void CrowdControl::_send_rpc(const String &p_method, const Array &p_args) {
	if (!is_websocket_connected()) {
		ERR_PRINT("CrowdControl: Cannot send RPC, not connected");
		return;
	}

	Dictionary call;
	call["type"] = "call";
	call["id"] = _generate_unique_id();
	call["method"] = p_method;
	call["args"] = p_args;

	Dictionary data;
	data["token"] = auth_token;
	data["call"] = call;

	Dictionary request;
	request["action"] = "rpc";
	request["data"] = data;

	String json_str = JSON::stringify(request);
	Error err = ws->send_text(json_str);

	if (err != OK) {
		emit_signal("rpc_error", p_method, "Failed to send RPC request");
	}
}

void CrowdControl::_send_subscribe() {
	if (!is_websocket_connected() || cc_uid.is_empty()) {
		return;
	}

	Array topics;
	topics.push_back("pub/" + cc_uid);

	Dictionary data;
	data["token"] = auth_token;
	data["topics"] = topics;

	Dictionary request;
	request["action"] = "subscribe";
	request["data"] = data;

	String json_str = JSON::stringify(request);
	ws->send_text(json_str);
}

String CrowdControl::_generate_unique_id() {
	uint64_t timestamp = OS::get_singleton()->get_ticks_msec();
	rpc_id_counter++;
	return String::num_uint64(timestamp) + "-" + String::num_uint64(rpc_id_counter);
}

int64_t CrowdControl::_get_unix_timestamp() {
	return Time::get_singleton()->get_unix_time_from_system();
}

String CrowdControl::_effect_status_to_string(EffectStatus p_status) {
	switch (p_status) {
		case STATUS_SUCCESS:
			return "success";
		case STATUS_FAILURE:
			return "failure";
		case STATUS_UNAVAILABLE:
			return "unavailable";
		case STATUS_RETRY:
			return "retry";
		case STATUS_QUEUE:
			return "queue";
		case STATUS_TIMED_BEGIN:
			return "timedBegin";
		case STATUS_TIMED_PAUSE:
			return "timedPause";
		case STATUS_TIMED_RESUME:
			return "timedResume";
		case STATUS_TIMED_END:
			return "timedEnd";
		default:
			return "failure";
	}
}

String CrowdControl::_report_status_to_string(EffectReportStatus p_status) {
	switch (p_status) {
		case MENU_VISIBLE:
			return "menuVisible";
		case MENU_HIDDEN:
			return "menuHidden";
		case MENU_AVAILABLE:
			return "menuAvailable";
		case MENU_UNAVAILABLE:
			return "menuUnavailable";
		default:
			return "menuUnavailable";
	}
}

CrowdControl::~CrowdControl() {
	close();
}
