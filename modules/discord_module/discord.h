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
#include "discord_api_loader.h"
#include "discord_auth_result.h"
#include "discord_types.h"

class DiscordAuthClient;

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

private:
	static Discord *singleton;

	DiscordAPILoader loader;
	DiscordAuthClient *auth_client = nullptr;

	bool dll_available = false;
	bool initialized = false;
	bool debug_logging = false;
	bool client_active = false;
	int64_t client_id = 0;
	String access_token;
	String username;
	uint64_t user_id = 0;
	ConnectionState connection_state = STATE_DISCONNECTED;
	String last_error;
	Discord_Client client;

	Vector<String> debug_log;
	static const int kMaxDebugLogEntries = 256;

	void _log_debug(const String &p_message);
	void _set_last_error(const String &p_error);
	void _reset_session();
	static ConnectionState _map_client_status(Discord_Client_Status p_status);
	Error _initialize_client(int64_t p_client_id);
	void _run_client_callbacks();
	void _disconnect_client();

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

protected:
	static void _bind_methods();

public:
	static Discord *get_singleton();

	bool is_available();
	Error initialize(int64_t p_client_id = 0);
	void shutdown();
	bool is_initialized() const { return initialized; }

	void set_debug_logging(bool p_enabled);
	bool is_debug_logging_enabled() const { return debug_logging; }
	Array get_debug_log() const;
	void clear_debug_log();

	void run_callbacks();
	ConnectionState get_connection_state() const { return connection_state; }
	String get_access_token() const;
	String get_username() const;
	uint64_t get_user_id() const;
	String get_last_error() const { return last_error; }

	Ref<DiscordAuthResult> authenticate_with_server(const String &p_url, const String &p_access_token, int64_t p_client_id);
	Dictionary get_version() const;

	Discord();
	~Discord();
};

VARIANT_ENUM_CAST(Discord::ConnectionState);
