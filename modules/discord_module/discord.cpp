/**************************************************************************/
/*  discord.cpp                                                           */
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

#include "discord.h"

#include "core/os/os.h"
#include "core/os/time.h"
#include "discord_auth_client.h"

Discord *Discord::singleton = nullptr;

namespace {

struct DiscordAuthFlow {
	Discord *owner = nullptr;
	DiscordAPILoader *loader = nullptr;
	Discord_Client *client = nullptr;
	int64_t client_id = 0;
	String code_verifier;
	String redirect_uri;
	String pending_token;
	Discord_AuthorizationTokenType pending_token_type = DISCORD_AUTHORIZATION_TOKEN_TYPE_BEARER;
	bool authorize_done = false;
	bool authorize_ok = false;
	bool token_done = false;
	bool token_ok = false;
	bool update_token_done = false;
	bool update_token_ok = false;
};

} //namespace

void Discord::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_available"), &Discord::is_available);
	ClassDB::bind_method(D_METHOD("initialize", "client_id"), &Discord::initialize, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("shutdown"), &Discord::shutdown);
	ClassDB::bind_method(D_METHOD("is_initialized"), &Discord::is_initialized);
	ClassDB::bind_method(D_METHOD("set_debug_logging", "enabled"), &Discord::set_debug_logging);
	ClassDB::bind_method(D_METHOD("is_debug_logging_enabled"), &Discord::is_debug_logging_enabled);
	ClassDB::bind_method(D_METHOD("get_debug_log"), &Discord::get_debug_log);
	ClassDB::bind_method(D_METHOD("clear_debug_log"), &Discord::clear_debug_log);
	ClassDB::bind_method(D_METHOD("run_callbacks"), &Discord::run_callbacks);
	ClassDB::bind_method(D_METHOD("get_connection_state"), &Discord::get_connection_state);
	ClassDB::bind_method(D_METHOD("get_access_token"), &Discord::get_access_token);
	ClassDB::bind_method(D_METHOD("get_username"), &Discord::get_username);
	ClassDB::bind_method(D_METHOD("get_user_id"), &Discord::get_user_id);
	ClassDB::bind_method(D_METHOD("get_last_error"), &Discord::get_last_error);
	ClassDB::bind_method(D_METHOD("authenticate_with_server", "url", "access_token", "client_id"), &Discord::authenticate_with_server);
	ClassDB::bind_method(D_METHOD("get_version"), &Discord::get_version);

	BIND_ENUM_CONSTANT(STATE_DISCONNECTED);
	BIND_ENUM_CONSTANT(STATE_CONNECTING);
	BIND_ENUM_CONSTANT(STATE_CONNECTED);
	BIND_ENUM_CONSTANT(STATE_READY);
	BIND_ENUM_CONSTANT(STATE_RECONNECTING);
	BIND_ENUM_CONSTANT(STATE_DISCONNECTING);
	BIND_ENUM_CONSTANT(STATE_HTTP_WAIT);
}

Discord *Discord::get_singleton() {
	return singleton;
}

Discord::Discord() {
	singleton = this;
	auth_client = memnew(DiscordAuthClient);
}

Discord::~Discord() {
	if (initialized) {
		shutdown();
	}
	memdelete(auth_client);
	auth_client = nullptr;
	if (singleton == this) {
		singleton = nullptr;
	}
}

void Discord::_log_debug(const String &p_message) {
	if (!debug_logging) {
		return;
	}
	print_line(vformat("[Discord] %s", p_message));
	if (debug_log.size() >= kMaxDebugLogEntries) {
		debug_log.remove_at(0);
	}
	debug_log.push_back(p_message);
}

void Discord::_set_last_error(const String &p_error) {
	last_error = p_error;
	_log_debug(p_error);
}

Discord::ConnectionState Discord::_map_client_status(Discord_Client_Status p_status) {
	switch (p_status) {
		case DISCORD_CLIENT_STATUS_DISCONNECTED:
			return STATE_DISCONNECTED;
		case DISCORD_CLIENT_STATUS_CONNECTING:
			return STATE_CONNECTING;
		case DISCORD_CLIENT_STATUS_CONNECTED:
			return STATE_CONNECTED;
		case DISCORD_CLIENT_STATUS_READY:
			return STATE_READY;
		case DISCORD_CLIENT_STATUS_RECONNECTING:
			return STATE_RECONNECTING;
		case DISCORD_CLIENT_STATUS_DISCONNECTING:
			return STATE_DISCONNECTING;
		case DISCORD_CLIENT_STATUS_HTTP_WAIT:
			return STATE_HTTP_WAIT;
		default:
			return STATE_DISCONNECTED;
	}
}

void Discord::_authorize_callback(Discord_ClientResult *result, Discord_String code, Discord_String redirect_uri, void *user_data) {
	DiscordAuthFlow *flow = (DiscordAuthFlow *)user_data;
	flow->authorize_done = true;
	flow->authorize_ok = flow->loader->client_result_successful(result);
	if (!flow->authorize_ok) {
		flow->owner->_set_last_error(vformat("Discord authorize failed: %s", flow->loader->client_result_error(result)));
		flow->loader->client_result_drop(result);
		return;
	}

	flow->redirect_uri = DiscordAPILoader::to_godot_string(redirect_uri);
	const String auth_code = DiscordAPILoader::to_godot_string(code);
	flow->loader->client_result_drop(result);

	flow->loader->client_get_token(
			flow->client,
			(uint64_t)flow->client_id,
			auth_code,
			flow->code_verifier,
			flow->redirect_uri,
			Discord::_token_exchange_callback,
			user_data);
}

void Discord::_token_exchange_callback(Discord_ClientResult *result,
		Discord_String access_token,
		Discord_String refresh_token,
		Discord_AuthorizationTokenType token_type,
		int32_t expires_in,
		Discord_String scopes,
		void *user_data) {
	(void)refresh_token;
	(void)expires_in;
	(void)scopes;
	DiscordAuthFlow *flow = (DiscordAuthFlow *)user_data;
	flow->token_done = true;
	flow->token_ok = flow->loader->client_result_successful(result);
	if (!flow->token_ok) {
		flow->owner->_set_last_error(vformat("Discord token exchange failed: %s", flow->loader->client_result_error(result)));
		flow->loader->client_result_drop(result);
		return;
	}
	flow->pending_token = DiscordAPILoader::to_godot_string(access_token);
	flow->pending_token_type = token_type;
	flow->loader->client_result_drop(result);

	flow->loader->client_update_token(
			flow->client,
			flow->pending_token_type,
			flow->pending_token,
			Discord::_update_token_callback,
			user_data);
}

void Discord::_update_token_callback(Discord_ClientResult *result, void *user_data) {
	DiscordAuthFlow *flow = (DiscordAuthFlow *)user_data;
	flow->update_token_done = true;
	flow->update_token_ok = flow->loader->client_result_successful(result);
	if (!flow->update_token_ok) {
		flow->owner->_set_last_error(vformat("Discord UpdateToken failed: %s", flow->loader->client_result_error(result)));
		flow->loader->client_result_drop(result);
		return;
	}
	flow->loader->client_result_drop(result);
	flow->owner->access_token = flow->pending_token;
	flow->loader->client_connect(flow->client);
}

void Discord::_status_changed_callback(Discord_Client_Status status, Discord_Client_Error error, int32_t error_detail, void *user_data) {
	Discord *discord = (Discord *)user_data;
	discord->connection_state = Discord::_map_client_status(status);
	if (error != DISCORD_CLIENT_ERROR_NONE) {
		discord->_set_last_error(vformat("Discord status error=%d detail=%d", (int)error, (int)error_detail));
	} else {
		discord->_log_debug(vformat("Discord status changed to %d", (int)status));
	}
}

void Discord::_reset_session() {
	_disconnect_client();
	access_token = String();
	username = String();
	user_id = 0;
	connection_state = STATE_DISCONNECTED;
	last_error = String();
	initialized = false;
	client_id = 0;
}

bool Discord::is_available() {
	if (dll_available) {
		return true;
	}
	dll_available = loader.try_load();
	if (dll_available) {
		_log_debug("Discord Social SDK library loaded");
	} else {
		_log_debug("Discord Social SDK library not found; Discord features disabled");
	}
	return dll_available;
}

Error Discord::_initialize_client(int64_t p_client_id) {
	if (client_active) {
		return OK;
	}

	loader.client_init(&client);
	client_active = true;
	loader.client_set_application_id(&client, (uint64_t)p_client_id);
	loader.client_set_status_changed_callback(&client, Discord::_status_changed_callback, this);

	Discord_AuthorizationCodeVerifier verifier;
	loader.client_create_authorization_code_verifier(&client, &verifier);

	Discord_AuthorizationCodeChallenge challenge;
	loader.authorization_code_verifier_challenge(&verifier, &challenge);

	DiscordAuthFlow flow;
	flow.owner = this;
	flow.loader = &loader;
	flow.client = &client;
	flow.client_id = p_client_id;
	flow.code_verifier = loader.authorization_code_verifier_verifier(&verifier);

	Discord_AuthorizationArgs auth_args;
	loader.authorization_args_init(&auth_args);
	loader.authorization_args_set_client_id(&auth_args, (uint64_t)p_client_id);
	loader.authorization_args_set_scopes(&auth_args, loader.get_default_communication_scopes());
	loader.authorization_args_set_code_challenge(&auth_args, &challenge);

	connection_state = STATE_CONNECTING;
	loader.client_authorize(&client, &auth_args, Discord::_authorize_callback, &flow);
	loader.authorization_args_drop(&auth_args);
	loader.authorization_code_verifier_drop(&verifier);

	const double start_usec = Time::get_singleton()->get_ticks_usec();
	const double timeout_sec = 120.0;
	while (true) {
		loader.run_callbacks();
		connection_state = _map_client_status(loader.client_get_status(&client));

		if (flow.authorize_done && !flow.authorize_ok) {
			return ERR_CANT_CREATE;
		}
		if (flow.token_done && !flow.token_ok) {
			return ERR_CANT_CREATE;
		}
		if (flow.update_token_done && !flow.update_token_ok) {
			return ERR_CANT_CREATE;
		}
		if (connection_state == STATE_READY) {
			Discord_UserHandle user;
			if (loader.client_get_current_user_v2(&client, &user)) {
				user_id = loader.user_handle_id(&user);
				username = loader.user_handle_display_name(&user);
				if (username.is_empty()) {
					username = loader.user_handle_username(&user);
				}
				loader.user_handle_drop(&user);
			}
			return OK;
		}

		if ((Time::get_singleton()->get_ticks_usec() - start_usec) / 1000000.0 > timeout_sec) {
			_set_last_error("Timed out waiting for Discord SDK to become ready");
			return ERR_TIMEOUT;
		}
		::OS::get_singleton()->delay_usec(1000);
	}
}

Error Discord::initialize(int64_t p_client_id) {
	if (!is_available()) {
		last_error = "Discord Social SDK runtime library not available";
		return ERR_UNAVAILABLE;
	}
	if (initialized) {
		return OK;
	}
	if (p_client_id <= 0) {
		last_error = "client_id must be a positive Discord application ID";
		return ERR_INVALID_PARAMETER;
	}

	client_id = p_client_id;
	Error err = _initialize_client(p_client_id);
	if (err != OK) {
		_reset_session();
		return err;
	}

	initialized = true;
	_log_debug(vformat("Discord initialized (client_id=%s)", String::num_int64(p_client_id)));
	return OK;
}

void Discord::_disconnect_client() {
	if (!client_active) {
		return;
	}
	loader.client_drop(&client);
	client = Discord_Client();
	client_active = false;
}

void Discord::shutdown() {
	if (!initialized && !client_active) {
		return;
	}
	_reset_session();
	_log_debug("Discord shutdown");
}

void Discord::set_debug_logging(bool p_enabled) {
	debug_logging = p_enabled;
}

Array Discord::get_debug_log() const {
	Array out;
	for (int i = 0; i < debug_log.size(); i++) {
		out.push_back(debug_log[i]);
	}
	return out;
}

void Discord::clear_debug_log() {
	debug_log.clear();
}

void Discord::_run_client_callbacks() {
	if (!client_active) {
		return;
	}
	loader.run_callbacks();
	connection_state = _map_client_status(loader.client_get_status(&client));
}

void Discord::run_callbacks() {
	if (initialized) {
		_run_client_callbacks();
	}
}

String Discord::get_access_token() const {
	return access_token;
}

String Discord::get_username() const {
	return username;
}

uint64_t Discord::get_user_id() const {
	return user_id;
}

Ref<DiscordAuthResult> Discord::authenticate_with_server(const String &p_url, const String &p_access_token, int64_t p_client_id) {
	const int64_t resolved_client_id = p_client_id > 0 ? p_client_id : client_id;
	_log_debug(vformat("Authenticating with server: %s", p_url));
	return auth_client->authenticate_sync(p_url, p_access_token, resolved_client_id, debug_logging);
}

Dictionary Discord::get_version() const {
	Dictionary version;
	version["sdk_enabled"] = loader.is_loaded();
	version["major"] = 0;
	version["minor"] = 0;
	version["patch"] = 0;
	version["hash"] = String();
	if (loader.is_loaded()) {
		loader.fill_version_dict(version);
	}
	version["runtime_library"] = DiscordAPILoader::get_runtime_library_name();
	version["runtime_present"] = DiscordAPILoader::is_runtime_library_present();
	return version;
}
