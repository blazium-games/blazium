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

#include "core/config/project_settings.h"
#include "core/os/os.h"
#include "discord_auth_client.h"
#include "discord_frame_hook.h"

struct DiscordAuthFlow {
	Discord *owner = nullptr;
	DiscordAPILoader *loader = nullptr;
	Discord_Client *client = nullptr;
	int64_t client_id = 0;
	String code_verifier;
	String redirect_uri;
	String pending_token;
	Discord_AuthorizationTokenType pending_token_type = DISCORD_AUTHORIZATION_TOKEN_TYPE_BEARER;
	CharString auth_code_utf8;
	CharString verifier_utf8;
	CharString redirect_utf8;
	CharString token_utf8;
};

Discord *Discord::singleton = nullptr;

static const char *kDefaultOAuthRedirectUri = "http://127.0.0.1/callback";

void Discord::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_available"), &Discord::is_available);
	ClassDB::bind_method(D_METHOD("initialize", "client_id"), &Discord::initialize, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("shutdown"), &Discord::shutdown);
	ClassDB::bind_method(D_METHOD("is_initialized"), &Discord::is_initialized);
	ClassDB::bind_method(D_METHOD("is_client_active"), &Discord::is_client_active);
	ClassDB::bind_method(D_METHOD("is_ready_for_presence"), &Discord::is_ready_for_presence);
	ClassDB::bind_method(D_METHOD("get_auth_state"), &Discord::get_auth_state);
	ClassDB::bind_method(D_METHOD("is_authorization_pending"), &Discord::is_authorization_pending);
	ClassDB::bind_method(D_METHOD("is_authorized"), &Discord::is_authorized);
	ClassDB::bind_method(D_METHOD("set_debug_logging", "enabled"), &Discord::set_debug_logging);
	ClassDB::bind_method(D_METHOD("is_debug_logging_enabled"), &Discord::is_debug_logging_enabled);
	ClassDB::bind_method(D_METHOD("get_debug_log"), &Discord::get_debug_log);
	ClassDB::bind_method(D_METHOD("clear_debug_log"), &Discord::clear_debug_log);
	ClassDB::bind_method(D_METHOD("run_callbacks"), &Discord::run_callbacks);
	ClassDB::bind_method(D_METHOD("is_frame_hook_connected"), &Discord::is_frame_hook_connected);
	ClassDB::bind_method(D_METHOD("get_connection_state"), &Discord::get_connection_state);
	ClassDB::bind_method(D_METHOD("get_access_token"), &Discord::get_access_token);
	ClassDB::bind_method(D_METHOD("get_username"), &Discord::get_username);
	ClassDB::bind_method(D_METHOD("get_user_id"), &Discord::get_user_id);
	ClassDB::bind_method(D_METHOD("get_relationships_count"), &Discord::get_relationships_count);
	ClassDB::bind_method(D_METHOD("get_last_error"), &Discord::get_last_error);
	ClassDB::bind_method(D_METHOD("get_default_presence_scopes"), &Discord::get_default_presence_scopes);
	ClassDB::bind_method(D_METHOD("get_default_communication_scopes"), &Discord::get_default_communication_scopes);
	ClassDB::bind_method(D_METHOD("get_requested_scopes"), &Discord::get_requested_scopes);
	ClassDB::bind_method(D_METHOD("get_granted_scopes"), &Discord::get_granted_scopes);
	ClassDB::bind_method(D_METHOD("get_game_activity"), &Discord::get_game_activity);
	ClassDB::bind_method(D_METHOD("update_rich_presence", "activity"), &Discord::update_rich_presence);
	ClassDB::bind_method(D_METHOD("clear_rich_presence"), &Discord::clear_rich_presence);
	ClassDB::bind_method(D_METHOD("authenticate_with_server", "url", "access_token", "client_id"), &Discord::authenticate_with_server);
	ClassDB::bind_method(D_METHOD("get_version"), &Discord::get_version);

	ADD_SIGNAL(MethodInfo("connection_state_changed", PropertyInfo(Variant::INT, "state"), PropertyInfo(Variant::INT, "error"), PropertyInfo(Variant::INT, "error_detail")));
	ADD_SIGNAL(MethodInfo("authorization_started"));
	ADD_SIGNAL(MethodInfo("authorized"));
	ADD_SIGNAL(MethodInfo("authorization_failed", PropertyInfo(Variant::STRING, "error")));
	ADD_SIGNAL(MethodInfo("ready"));
	ADD_SIGNAL(MethodInfo("relationships_updated", PropertyInfo(Variant::INT, "count")));
	ADD_SIGNAL(MethodInfo("rich_presence_updated", PropertyInfo(Variant::BOOL, "success"), PropertyInfo(Variant::STRING, "error")));

	BIND_ENUM_CONSTANT(STATE_DISCONNECTED);
	BIND_ENUM_CONSTANT(STATE_CONNECTING);
	BIND_ENUM_CONSTANT(STATE_CONNECTED);
	BIND_ENUM_CONSTANT(STATE_READY);
	BIND_ENUM_CONSTANT(STATE_RECONNECTING);
	BIND_ENUM_CONSTANT(STATE_DISCONNECTING);
	BIND_ENUM_CONSTANT(STATE_HTTP_WAIT);

	BIND_ENUM_CONSTANT(AUTH_NONE);
	BIND_ENUM_CONSTANT(AUTH_PENDING);
	BIND_ENUM_CONSTANT(AUTH_AUTHORIZED);
	BIND_ENUM_CONSTANT(AUTH_READY);
	BIND_ENUM_CONSTANT(AUTH_FAILED);
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

void Discord::_log_lifecycle(const String &p_message) {
	print_line(vformat("[Discord] %s", p_message));
	if (debug_log.size() >= kMaxDebugLogEntries) {
		debug_log.remove_at(0);
	}
	debug_log.push_back(p_message);
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

void Discord::_set_auth_state(AuthState p_state) {
	if (auth_state == p_state) {
		return;
	}
	auth_state = p_state;
	_log_lifecycle(vformat("Auth state -> %d", (int)p_state));
}

Error Discord::_require_auth_ready(const char *p_operation) {
	if (ready_for_presence && auth_state == AUTH_READY) {
		return OK;
	}
	last_error = vformat("Discord is not ready for %s (auth_state=%d, connection=%d)", p_operation, (int)auth_state, (int)connection_state);
	return ERR_UNAUTHORIZED;
}

void Discord::_set_last_error(const String &p_error) {
	last_error = p_error;
	_log_debug(p_error);
}

void Discord::_report_authorization_failed(const String &p_error) {
	last_error = p_error;
	print_line(vformat("[Discord] %s", p_error));
	if (p_error.contains("redirect_uri")) {
		_log_lifecycle("OAuth redirect_uri error — verify Developer Portal OAuth2 settings:");
		_log_lifecycle("  - Redirect URI exactly: http://127.0.0.1/callback");
		_log_lifecycle("  - Public Client enabled");
		_log_lifecycle("  - Social SDK / Activities enabled for this application");
		_log_lifecycle("  - Restart Discord desktop after portal changes");
		_log_lifecycle(vformat("  - See SDK log: %s/discord.log", OS::get_singleton()->get_user_data_dir()));
	}
	if (auth_flow && auth_flow->owner) {
		auth_flow->owner->_set_auth_state(AUTH_FAILED);
	} else {
		_set_auth_state(AUTH_FAILED);
	}
	emit_signal(SNAME("authorization_failed"), p_error);
}

void Discord::_sync_connection_state(Discord_Client_Status p_status, int p_error, int p_error_detail, bool p_emit_signal) {
	const ConnectionState new_state = _map_client_status(p_status);
	connection_state = new_state;

	if (p_error != DISCORD_CLIENT_ERROR_NONE) {
		_set_last_error(vformat("Discord status error=%d detail=%d", p_error, p_error_detail));
	} else if ((int)p_status != last_polled_status) {
		last_polled_status = (int)p_status;
		_log_debug(vformat("Discord status changed to %d", (int)p_status));
	}

	if (p_emit_signal &&
			(new_state != last_emitted_state || p_error != last_emitted_error || p_error_detail != last_emitted_error_detail)) {
		last_emitted_state = new_state;
		last_emitted_error = p_error;
		last_emitted_error_detail = p_error_detail;
		emit_signal(SNAME("connection_state_changed"), new_state, p_error, p_error_detail);
	}

	if (connection_state == STATE_READY) {
		_on_client_ready();
	}
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
	if (!flow || !flow->owner || !flow->loader) {
		return;
	}
	if (!flow->loader->client_result_successful(result)) {
		flow->owner->_report_authorization_failed(vformat("Discord authorize failed: %s", flow->loader->client_result_error(result)));
		flow->loader->client_result_drop(result);
		return;
	}

	flow->redirect_uri = DiscordAPILoader::to_godot_string(redirect_uri);
	if (flow->redirect_uri.is_empty()) {
		flow->redirect_uri = kDefaultOAuthRedirectUri;
		flow->owner->_log_lifecycle(vformat("Using default OAuth redirect_uri: %s", flow->redirect_uri));
	}
	const String auth_code = DiscordAPILoader::to_godot_string(code);
	flow->loader->client_result_drop(result);
	flow->owner->_log_lifecycle("Authorization successful, exchanging code for token");
	flow->owner->_start_token_exchange(flow, auth_code);
}

void Discord::_start_token_exchange(DiscordAuthFlow *p_flow, const String &p_auth_code) {
	if (!p_flow || !p_flow->loader || !p_flow->client) {
		return;
	}
	p_flow->auth_code_utf8 = p_auth_code.utf8();
	p_flow->verifier_utf8 = p_flow->code_verifier.utf8();
	p_flow->redirect_utf8 = p_flow->redirect_uri.utf8();
	Discord_String code;
	code.ptr = (uint8_t *)p_flow->auth_code_utf8.ptr();
	code.size = p_flow->auth_code_utf8.length();
	Discord_String verifier;
	verifier.ptr = (uint8_t *)p_flow->verifier_utf8.ptr();
	verifier.size = p_flow->verifier_utf8.length();
	Discord_String redirect_uri;
	redirect_uri.ptr = (uint8_t *)p_flow->redirect_utf8.ptr();
	redirect_uri.size = p_flow->redirect_utf8.length();
	p_flow->loader->client_get_token(
			p_flow->client,
			(uint64_t)p_flow->client_id,
			code,
			verifier,
			redirect_uri,
			Discord::_token_exchange_callback,
			p_flow);
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
	DiscordAuthFlow *flow = (DiscordAuthFlow *)user_data;
	if (!flow || !flow->owner || !flow->loader) {
		return;
	}
	if (!flow->loader->client_result_successful(result)) {
		flow->owner->_report_authorization_failed(vformat("Discord token exchange failed: %s", flow->loader->client_result_error(result)));
		flow->loader->client_result_drop(result);
		return;
	}
	flow->pending_token = DiscordAPILoader::to_godot_string(access_token);
	flow->pending_token_type = token_type;
	flow->token_utf8 = flow->pending_token.utf8();
	flow->owner->granted_scopes = DiscordAPILoader::to_godot_string(scopes);
	flow->loader->client_result_drop(result);
	flow->owner->_log_lifecycle(vformat("Access token received (reported_type=%d, using Bearer for UpdateToken, granted_scopes='%s')",
			(int)token_type, flow->owner->granted_scopes));
	flow->owner->_validate_granted_scopes();

	Discord_String token;
	token.ptr = (uint8_t *)flow->token_utf8.ptr();
	token.size = flow->token_utf8.length();
	flow->loader->client_update_token(
			flow->client,
			DISCORD_AUTHORIZATION_TOKEN_TYPE_BEARER,
			token,
			Discord::_update_token_callback,
			user_data);
}

void Discord::_update_token_callback(Discord_ClientResult *result, void *user_data) {
	DiscordAuthFlow *flow = (DiscordAuthFlow *)user_data;
	if (!flow || !flow->owner || !flow->loader) {
		return;
	}
	if (!flow->loader->client_result_successful(result)) {
		flow->owner->_report_authorization_failed(vformat("Discord UpdateToken failed: %s", flow->loader->client_result_error(result)));
		flow->loader->client_result_drop(result);
		return;
	}
	flow->loader->client_result_drop(result);
	flow->owner->access_token = flow->pending_token;
	flow->owner->_set_auth_state(AUTH_AUTHORIZED);
	flow->owner->emit_signal(SNAME("authorized"));
	flow->owner->_log_lifecycle("Token updated, connecting to Discord...");
	flow->loader->client_connect(flow->client);
}

void Discord::_status_changed_callback(Discord_Client_Status status, Discord_Client_Error error, int32_t error_detail, void *user_data) {
	Discord *discord = (Discord *)user_data;
	if (!discord) {
		return;
	}
	discord->_sync_connection_state(status, (int)error, (int)error_detail, true);
}

void Discord::_authorize_device_screen_closed_callback(void *user_data) {
	Discord *discord = (Discord *)user_data;
	if (!discord) {
		return;
	}
	discord->_report_authorization_failed("Authorization dismissed");
}

void Discord::_log_callback(Discord_String message, Discord_LoggingSeverity severity, void *user_data) {
	(void)severity;
	Discord *discord = (Discord *)user_data;
	if (!discord) {
		return;
	}
	discord->_log_lifecycle(vformat("[SDK] %s", DiscordAPILoader::to_godot_string(message)));
}

void Discord::_update_rich_presence_callback(Discord_ClientResult *result, void *user_data) {
	Discord *discord = (Discord *)user_data;
	if (!discord) {
		return;
	}
	const bool success = discord->loader.client_result_successful(result);
	const String error = success ? String() : discord->loader.client_result_error(result);
	discord->loader.client_result_drop(result);
	if (success) {
		discord->_log_lifecycle("Rich Presence updated successfully");
		discord->_log_game_activity_readback();
	} else {
		discord->_log_lifecycle(vformat("Rich Presence update failed: %s", error));
	}
	discord->emit_signal(SNAME("rich_presence_updated"), success, error);
}

void Discord::_on_client_ready() {
	if (ready_for_presence) {
		return;
	}
	ready_for_presence = true;
	_set_auth_state(AUTH_READY);

	Discord_UserHandle user;
	if (loader.client_get_current_user_v2(&client, &user)) {
		user_id = loader.user_handle_id(&user);
		username = loader.user_handle_display_name(&user);
		if (username.is_empty()) {
			username = loader.user_handle_username(&user);
		}
		loader.user_handle_drop(&user);
		_log_lifecycle(vformat("Discord user ready: %s", username));
	}

	_refresh_relationships();

	if (!ready_signal_emitted) {
		ready_signal_emitted = true;
		_log_lifecycle("Client is ready for rich presence");
		emit_signal(SNAME("ready"));
	}

	const bool had_pending = has_pending_presence;
	_apply_pending_presence();
	if (!had_pending && last_presence_sent.is_empty()) {
		Dictionary sample;
		sample["type"] = DISCORD_ACTIVITY_TYPE_PLAYING;
		sample["details"] = "Rank: Diamond II";
		sample["state"] = "In Competitive Match";
		_send_rich_presence(sample);
	}
}

void Discord::_apply_pending_presence() {
	if (!ready_for_presence || !has_pending_presence) {
		return;
	}
	const Dictionary activity = pending_presence;
	has_pending_presence = false;
	pending_presence = Dictionary();
	_send_rich_presence(activity);
}

void Discord::_refresh_relationships() {
	if (!client_active || !ready_for_presence) {
		return;
	}
	Discord_RelationshipHandleSpan span;
	loader.client_get_relationships(&client, &span);
	relationships_count = (int)span.size;
	emit_signal(SNAME("relationships_updated"), relationships_count);
}

void Discord::_register_log_callback() {
	if (!client_active || log_callback_registered) {
		return;
	}
	loader.client_add_log_callback(&client, Discord::_log_callback, this, DISCORD_LOGGING_SEVERITY_INFO);
	log_callback_registered = true;
}

void Discord::_register_platform_hooks() {
#ifdef WINDOWS_ENABLED
	loader.client_set_game_window_pid(&client, (int32_t)OS::get_singleton()->get_process_id());
#endif
	if (debug_logging) {
		const String log_dir = OS::get_singleton()->get_user_data_dir();
		if (loader.client_set_log_dir(&client, log_dir, DISCORD_LOGGING_SEVERITY_INFO)) {
			_log_lifecycle(vformat("SDK file logging enabled: %s/discord.log", log_dir));
		}
	}
}

void Discord::_register_authorize_dismiss_callback() {
	if (!client_active || authorize_dismiss_callback_registered) {
		return;
	}
	if (loader.client_set_authorize_device_screen_closed_callback(&client, Discord::_authorize_device_screen_closed_callback, this)) {
		authorize_dismiss_callback_registered = true;
	}
}

void Discord::_clear_auth_flow() {
	if (auth_flow) {
		memdelete(auth_flow);
		auth_flow = nullptr;
	}
}

void Discord::_reset_session() {
	if (client_active && loader.is_loaded()) {
		loader.client_clear_rich_presence(&client);
	}
	_disconnect_client();
	_clear_auth_flow();
	access_token = String();
	username = String();
	requested_scopes = String();
	granted_scopes = String();
	oauth_scope_preset = String();
	user_id = 0;
	relationships_count = -1;
	connection_state = STATE_DISCONNECTED;
	last_error = String();
	initialized = false;
	ready_for_presence = false;
	ready_signal_emitted = false;
	log_callback_registered = false;
	authorize_dismiss_callback_registered = false;
	has_pending_presence = false;
	pending_presence = Dictionary();
	last_presence_sent = Dictionary();
	last_emitted_state = STATE_DISCONNECTED;
	last_emitted_error = 0;
	last_emitted_error_detail = 0;
	last_polled_status = -1;
	auth_state = AUTH_NONE;
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
	_register_log_callback();
	_register_authorize_dismiss_callback();
	_register_platform_hooks();

	Discord_AuthorizationCodeVerifier verifier;
	loader.client_create_authorization_code_verifier(&client, &verifier);

	Discord_AuthorizationCodeChallenge challenge;
	loader.authorization_code_verifier_challenge(&verifier, &challenge);

	_clear_auth_flow();
	auth_flow = memnew(DiscordAuthFlow);
	auth_flow->owner = this;
	auth_flow->loader = &loader;
	auth_flow->client = &client;
	auth_flow->client_id = p_client_id;
	auth_flow->code_verifier = loader.authorization_code_verifier_verifier(&verifier);

	Discord_AuthorizationArgs auth_args;
	loader.authorization_args_init(&auth_args);
	loader.authorization_args_set_client_id(&auth_args, (uint64_t)p_client_id);
	oauth_scope_preset = _resolve_oauth_scope_preset();
	requested_scopes = _resolve_oauth_scopes();
	loader.authorization_args_set_scopes(&auth_args, requested_scopes);
	loader.authorization_args_set_code_challenge(&auth_args, &challenge);
	const String integration_label = _resolve_integration_type_label();

	connection_state = STATE_CONNECTING;
	_set_auth_state(AUTH_PENDING);
	emit_signal(SNAME("authorization_started"));
	_log_lifecycle(vformat("Starting OAuth authorization (client_id=%s preset=%s scopes='%s' integration=%s redirect=%s)",
			String::num_int64(p_client_id), oauth_scope_preset, requested_scopes, integration_label, kDefaultOAuthRedirectUri));
	_apply_integration_type(&auth_args);
	loader.client_authorize(&client, &auth_args, Discord::_authorize_callback, auth_flow);
	loader.authorization_args_drop(&auth_args);
	loader.authorization_code_verifier_drop(&verifier);

	return OK;
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

	discord_try_connect_frame_hook();

	client_id = p_client_id;
	Error err = _initialize_client(p_client_id);
	if (err != OK) {
		_reset_session();
		return err;
	}

	initialized = true;
	_log_lifecycle(vformat("Discord auth started (client_id=%s)", String::num_int64(p_client_id)));
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
	_log_lifecycle("Discord shutdown");
}

void Discord::set_debug_logging(bool p_enabled) {
	debug_logging = p_enabled;
	if (p_enabled && client_active) {
		_register_platform_hooks();
	}
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
	discord_try_connect_frame_hook();
	if (!client_active) {
		return;
	}
	loader.run_callbacks();
	const Discord_Client_Status status = loader.client_get_status(&client);
	_sync_connection_state(status, DISCORD_CLIENT_ERROR_NONE, 0, false);
}

void Discord::run_callbacks() {
	discord_try_connect_frame_hook();
	if (client_active) {
		_run_client_callbacks();
	}
}

bool Discord::is_frame_hook_connected() const {
	return discord_is_frame_hook_connected();
}

void Discord::note_frame_hook_connected() {
	_log_lifecycle("Frame callback hook connected");
}

String Discord::get_access_token() const {
	return access_token;
}

String Discord::get_username() const {
	if (auth_state != AUTH_READY) {
		return String();
	}
	return username;
}

uint64_t Discord::get_user_id() const {
	if (auth_state != AUTH_READY) {
		return 0;
	}
	return user_id;
}

int Discord::get_relationships_count() const {
	if (auth_state != AUTH_READY) {
		return -1;
	}
	return relationships_count;
}

String Discord::get_default_presence_scopes() const {
	if (!loader.is_loaded()) {
		return String();
	}
	return loader.get_default_presence_scopes();
}

String Discord::get_default_communication_scopes() const {
	if (!loader.is_loaded()) {
		return String();
	}
	return loader.get_default_communication_scopes();
}

Error Discord::update_rich_presence(const Dictionary &p_activity) {
	if (_require_auth_ready("update_rich_presence") != OK) {
		pending_presence = p_activity;
		has_pending_presence = true;
		return ERR_UNAUTHORIZED;
	}
	return _send_rich_presence(p_activity);
}

Error Discord::_send_rich_presence(const Dictionary &p_activity) {
	if (_require_auth_ready("update_rich_presence") != OK) {
		return ERR_UNAUTHORIZED;
	}

	last_presence_sent = p_activity;
	const String details = p_activity.has("details") ? (String)p_activity["details"] : String();
	const String state = p_activity.has("state") ? (String)p_activity["state"] : String();
	const String large_image = p_activity.has("large_image") ? (String)p_activity["large_image"] : String();
	const String small_image = p_activity.has("small_image") ? (String)p_activity["small_image"] : String();
	_log_lifecycle(vformat("Sending Rich Presence: details='%s' state='%s' large='%s' small='%s'",
			details, state, large_image, small_image));

	Discord_Activity activity;
	loader.activity_init(&activity);

	const int activity_type = p_activity.has("type") ? (int)p_activity["type"] : DISCORD_ACTIVITY_TYPE_PLAYING;
	loader.activity_set_type(&activity, (Discord_ActivityTypes)activity_type);

	if (p_activity.has("details")) {
		loader.activity_set_details(&activity, (String)p_activity["details"]);
	}
	if (p_activity.has("state")) {
		loader.activity_set_state(&activity, (String)p_activity["state"]);
	}

	const bool has_assets = p_activity.has("large_image") || p_activity.has("large_text") ||
			p_activity.has("small_image") || p_activity.has("small_text");
	if (has_assets) {
		Discord_ActivityAssets assets;
		loader.activity_assets_init(&assets);
		if (p_activity.has("large_image")) {
			loader.activity_assets_set_large_image(&assets, (String)p_activity["large_image"]);
		}
		if (p_activity.has("large_text")) {
			loader.activity_assets_set_large_text(&assets, (String)p_activity["large_text"]);
		}
		if (p_activity.has("small_image")) {
			loader.activity_assets_set_small_image(&assets, (String)p_activity["small_image"]);
		}
		if (p_activity.has("small_text")) {
			loader.activity_assets_set_small_text(&assets, (String)p_activity["small_text"]);
		}
		loader.activity_set_assets(&activity, &assets);
		loader.activity_assets_drop(&assets);
	}

	const bool has_timestamps = p_activity.has("start_timestamp") || p_activity.has("end_timestamp");
	if (has_timestamps) {
		Discord_ActivityTimestamps timestamps;
		loader.activity_timestamps_init(&timestamps);
		if (p_activity.has("start_timestamp")) {
			loader.activity_timestamps_set_start(&timestamps, (uint64_t)(int64_t)p_activity["start_timestamp"]);
		}
		if (p_activity.has("end_timestamp")) {
			loader.activity_timestamps_set_end(&timestamps, (uint64_t)(int64_t)p_activity["end_timestamp"]);
		}
		loader.activity_set_timestamps(&activity, &timestamps);
		loader.activity_timestamps_drop(&timestamps);
	}

	loader.client_update_rich_presence(&client, &activity, Discord::_update_rich_presence_callback, this);
	loader.activity_drop(&activity);
	return OK;
}

void Discord::clear_rich_presence() {
	if (_require_auth_ready("clear_rich_presence") != OK) {
		return;
	}
	loader.client_clear_rich_presence(&client);
}

String Discord::_resolve_oauth_scope_preset() {
	if (ProjectSettings::get_singleton()->has_setting("discord/oauth_scope_preset")) {
		return ProjectSettings::get_singleton()->get_setting("discord/oauth_scope_preset", "presence");
	}
	return "presence";
}

String Discord::_resolve_oauth_scopes() {
	if (!loader.is_loaded()) {
		return String();
	}
	const String preset = _resolve_oauth_scope_preset();
	if (preset == "communication") {
		return loader.get_default_communication_scopes();
	}
	if (preset == "custom") {
		String custom;
		if (ProjectSettings::get_singleton()->has_setting("discord/oauth_scopes_custom")) {
			custom = ProjectSettings::get_singleton()->get_setting("discord/oauth_scopes_custom", "");
		}
		if (!custom.is_empty()) {
			return custom;
		}
		_log_lifecycle("oauth_scope_preset=custom but oauth_scopes_custom is empty; falling back to presence scopes");
	}
	return loader.get_default_presence_scopes();
}

String Discord::_resolve_integration_type_label() {
	String integration = "UserInstall";
	if (ProjectSettings::get_singleton()->has_setting("discord/oauth_integration_type")) {
		integration = ProjectSettings::get_singleton()->get_setting("discord/oauth_integration_type", "UserInstall");
	}
	if (integration == "None" || integration == "none" || integration.is_empty()) {
		return "None";
	}
	return integration;
}

bool Discord::_apply_integration_type(Discord_AuthorizationArgs *p_auth_args) {
	if (!p_auth_args || !loader.is_loaded()) {
		return false;
	}
	const String integration = _resolve_integration_type_label();
	if (integration == "None") {
		return false;
	}
	loader.authorization_args_set_integration_type(p_auth_args, DISCORD_INTEGRATION_TYPE_USER_INSTALL);
	return true;
}

void Discord::_validate_granted_scopes() {
	if (granted_scopes.is_empty()) {
		return;
	}
	const String preset = oauth_scope_preset.is_empty() ? _resolve_oauth_scope_preset() : oauth_scope_preset;
	if (preset == "presence" && !granted_scopes.contains("sdk.social_layer_presence")) {
		print_line("[Discord] Warning: granted scopes missing sdk.social_layer_presence — presence may not display; revoke app auth and re-authorize");
	} else if (preset == "communication" && !granted_scopes.contains("sdk.social_layer")) {
		print_line("[Discord] Warning: granted scopes missing sdk.social_layer — communication features may be unavailable; revoke app auth and re-authorize");
	}
}

Dictionary Discord::_read_game_activity() {
	Dictionary out;
	if (!ready_for_presence || !client_active || !loader.is_loaded()) {
		return out;
	}
	Discord_UserHandle user;
	if (!loader.client_get_current_user_v2(&client, &user)) {
		return out;
	}
	Discord_Activity activity;
	if (!loader.user_handle_game_activity(&user, &activity)) {
		loader.user_handle_drop(&user);
		return out;
	}
	out["type"] = (int)loader.activity_get_type(&activity);
	out["details"] = loader.activity_get_details(&activity);
	out["state"] = loader.activity_get_state(&activity);
	loader.activity_drop(&activity);
	loader.user_handle_drop(&user);
	return out;
}

void Discord::_log_game_activity_readback() {
	const Dictionary activity = _read_game_activity();
	if (activity.is_empty()) {
		_log_lifecycle("GameActivity readback: none (activity not set for this game)");
		return;
	}
	_log_lifecycle(vformat("GameActivity readback: type=%s details='%s' state='%s'",
			activity.get("type", 0), activity.get("details", ""), activity.get("state", "")));
}

Dictionary Discord::get_game_activity() {
	return _read_game_activity();
}

Ref<DiscordAuthResult> Discord::authenticate_with_server(const String &p_url, const String &p_access_token, int64_t p_client_id) {
	if (_require_auth_ready("authenticate_with_server") != OK) {
		Ref<DiscordAuthResult> result;
		result.instantiate();
		result->set_error_message(last_error);
		result->set_success(false);
		return result;
	}
	if (access_token.is_empty()) {
		last_error = "Discord access token is not available";
		Ref<DiscordAuthResult> result;
		result.instantiate();
		result->set_error_message(last_error);
		result->set_success(false);
		return result;
	}
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
