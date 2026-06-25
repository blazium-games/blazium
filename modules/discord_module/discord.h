/**************************************************************************/
/*  discord.h                                                             */
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

#include "core/object/object.h"
#include "core/variant/dictionary.h"
#include "discord_api_loader.h"
#include "discord_auth_result.h"
#include "discord_types.h"

class DiscordAuthClient;

struct DiscordAuthFlow;

class Discord : public Object {
	GDCLASS(Discord, Object);

public:
	enum ConnectionState {
		STATE_DISCONNECTED,
		STATE_CONNECTING,
		STATE_CONNECTED,
		STATE_READY,
		STATE_RECONNECTING,
		STATE_DISCONNECTING,
		STATE_HTTP_WAIT,
	};

	enum AuthState {
		AUTH_NONE,
		AUTH_PENDING,
		AUTH_AUTHORIZED,
		AUTH_READY,
		AUTH_FAILED,
	};

private:
	static Discord *singleton;

	DiscordAPILoader loader;
	DiscordAuthClient *auth_client = nullptr;
	DiscordAuthFlow *auth_flow = nullptr;

	bool dll_available = false;
	bool initialized = false;
	bool ready_for_presence = false;
	bool ready_signal_emitted = false;
	bool debug_logging = false;
	bool client_active = false;
	bool log_callback_registered = false;
	bool authorize_dismiss_callback_registered = false;
	bool has_pending_presence = false;
	int64_t client_id = 0;
	int relationships_count = -1;
	int last_emitted_error = 0;
	int last_emitted_error_detail = 0;
	int last_polled_status = -1;
	ConnectionState last_emitted_state = STATE_DISCONNECTED;
	AuthState auth_state = AUTH_NONE;
	String access_token;
	String username;
	String requested_scopes;
	String granted_scopes;
	String oauth_scope_preset;
	uint64_t user_id = 0;
	ConnectionState connection_state = STATE_DISCONNECTED;
	String last_error;
	Dictionary pending_presence;
	Dictionary last_presence_sent;
	Discord_Client client;

	Vector<String> debug_log;
	static const int kMaxDebugLogEntries = 256;

	void _log_lifecycle(const String &p_message);
	void _log_debug(const String &p_message);
	void _set_last_error(const String &p_error);
	void _set_auth_state(AuthState p_state);
	void _report_authorization_failed(const String &p_error);
	Error _require_auth_ready(const char *p_operation);
	void _sync_connection_state(Discord_Client_Status p_status, int p_error, int p_error_detail, bool p_emit_signal);
	void _reset_session();
	void _clear_auth_flow();
	void _on_client_ready();
	void _apply_pending_presence();
	Error _send_rich_presence(const Dictionary &p_activity);
	void _log_game_activity_readback();
	Dictionary _read_game_activity();
	String _resolve_oauth_scopes();
	String _resolve_oauth_scope_preset();
	void _validate_granted_scopes();
	String _resolve_integration_type_label();
	bool _apply_integration_type(Discord_AuthorizationArgs *p_auth_args);
	void _refresh_relationships();
	void _register_log_callback();
	void _register_authorize_dismiss_callback();
	void _register_platform_hooks();
	static ConnectionState _map_client_status(Discord_Client_Status p_status);
	Error _initialize_client(int64_t p_client_id);
	void _run_client_callbacks();
	void _disconnect_client();
	void _start_token_exchange(DiscordAuthFlow *p_flow, const String &p_auth_code);

	static void _authorize_callback(Discord_ClientResult *result, Discord_String code, Discord_String redirect_uri, void *user_data);
	static void _token_exchange_callback(Discord_ClientResult *result,
			Discord_String access_token,
			Discord_String refresh_token,
			Discord_AuthorizationTokenType token_type,
			int32_t expires_in,
			Discord_String scopes,
			void *user_data);
	static void _update_token_callback(Discord_ClientResult *result, void *user_data);
	static void _status_changed_callback(Discord_Client_Status status, Discord_Client_Error error, int32_t error_detail, void *user_data);
	static void _log_callback(Discord_String message, Discord_LoggingSeverity severity, void *user_data);
	static void _authorize_device_screen_closed_callback(void *user_data);
	static void _update_rich_presence_callback(Discord_ClientResult *result, void *user_data);

protected:
	static void _bind_methods();

public:
	static Discord *get_singleton();

	bool is_available();
	Error initialize(int64_t p_client_id = 0);
	void shutdown();
	bool is_initialized() const { return initialized; }
	bool is_client_active() const { return client_active; }
	bool is_ready_for_presence() const { return ready_for_presence; }
	AuthState get_auth_state() const { return auth_state; }
	bool is_authorization_pending() const { return auth_state == AUTH_PENDING; }
	bool is_authorized() const { return auth_state == AUTH_AUTHORIZED || auth_state == AUTH_READY; }

	void set_debug_logging(bool p_enabled);
	bool is_debug_logging_enabled() const { return debug_logging; }
	Array get_debug_log() const;
	void clear_debug_log();

	void run_callbacks();
	bool is_frame_hook_connected() const;
	void note_frame_hook_connected();
	ConnectionState get_connection_state() const { return connection_state; }
	String get_access_token() const;
	String get_username() const;
	uint64_t get_user_id() const;
	int get_relationships_count() const;
	String get_last_error() const { return last_error; }
	String get_default_presence_scopes() const;
	String get_default_communication_scopes() const;
	String get_requested_scopes() const { return requested_scopes; }
	String get_granted_scopes() const { return granted_scopes; }
	Dictionary get_game_activity();

	Error update_rich_presence(const Dictionary &p_activity);
	void clear_rich_presence();

	Ref<DiscordAuthResult> authenticate_with_server(const String &p_url, const String &p_access_token, int64_t p_client_id);
	Dictionary get_version() const;

	Discord();
	~Discord();
};

VARIANT_ENUM_CAST(Discord::ConnectionState);
VARIANT_ENUM_CAST(Discord::AuthState);
