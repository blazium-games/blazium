/**************************************************************************/
/*  crowd_control.h                                                       */
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
#include "modules/websocket/websocket_peer.h"

class CrowdControl : public Object {
	GDCLASS(CrowdControl, Object);

public:
	enum EffectStatus {
		STATUS_SUCCESS,
		STATUS_FAILURE,
		STATUS_UNAVAILABLE,
		STATUS_RETRY,
		STATUS_QUEUE,
		STATUS_TIMED_BEGIN,
		STATUS_TIMED_PAUSE,
		STATUS_TIMED_RESUME,
		STATUS_TIMED_END,
	};

	enum EffectReportStatus {
		MENU_VISIBLE,
		MENU_HIDDEN,
		MENU_AVAILABLE,
		MENU_UNAVAILABLE,
	};

private:
	Ref<WebSocketPeer> ws;
	String auth_token;
	String cc_uid;
	String username;
	String connection_id;
	String game_session_id;
	String game_pack_id;
	uint64_t rpc_id_counter = 0;
	bool authenticated = false;
	bool connection_signal_emitted = false;

	// Internal helper methods
	void _process_messages();
	void _handle_message(const String &p_message);
	void _handle_direct_event(const String &p_type, const Dictionary &p_payload);
	void _handle_pub_event(const String &p_type, const Dictionary &p_payload);
	void _send_rpc(const String &p_method, const Array &p_args);
	void _send_subscribe();
	String _generate_unique_id();
	int64_t _get_unix_timestamp();
	String _effect_status_to_string(EffectStatus p_status);
	String _report_status_to_string(EffectReportStatus p_status);

protected:
	static void _bind_methods();

public:
	static CrowdControl *get_singleton();

	// Connection Management
	Error connect_to_crowdcontrol();
	void close();
	void poll();
	bool is_websocket_connected() const;
	bool is_authenticated() const;

	// Authentication Flow
	Error request_authentication();
	String get_authentication_url() const;

	// Session Management
	Error start_game_session(const String &p_game_pack_id);
	Error stop_game_session();
	String get_game_session_id() const;
	void request_interact_link();

	// Effect Response
	Error respond_to_effect_instant(const String &p_request_id, EffectStatus p_status, const String &p_message = "");
	Error respond_to_effect_timed(const String &p_request_id, EffectStatus p_status, int p_time_remaining, const String &p_message = "");

	// Effect Reports
	Error report_effects(const PackedStringArray &p_effect_ids, EffectReportStatus p_status, const String &p_identifier_type = "effect");

	// Getters
	String get_cc_uid() const;
	String get_username() const;
	String get_connection_id() const;
	String get_auth_token() const;

	~CrowdControl();
};

VARIANT_ENUM_CAST(CrowdControl::EffectStatus);
VARIANT_ENUM_CAST(CrowdControl::EffectReportStatus);
