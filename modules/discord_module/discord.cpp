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
#include "core/variant/array.h"
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
	ClassDB::bind_method(D_METHOD("accept_activity_invite", "invite"), &Discord::accept_activity_invite);
	ClassDB::bind_method(D_METHOD("accept_discord_friend_request", "user_id"), &Discord::accept_discord_friend_request);
	ClassDB::bind_method(D_METHOD("accept_game_friend_request", "user_id"), &Discord::accept_game_friend_request);
	ClassDB::bind_method(D_METHOD("authenticate_with_server", "url", "access_token", "client_id"), &Discord::authenticate_with_server);
	ClassDB::bind_method(D_METHOD("block_user", "user_id"), &Discord::block_user);
	ClassDB::bind_method(D_METHOD("cancel_discord_friend_request", "user_id"), &Discord::cancel_discord_friend_request);
	ClassDB::bind_method(D_METHOD("cancel_game_friend_request", "user_id"), &Discord::cancel_game_friend_request);
	ClassDB::bind_method(D_METHOD("clear_debug_log"), &Discord::clear_debug_log);
	ClassDB::bind_method(D_METHOD("clear_rich_presence"), &Discord::clear_rich_presence);
	ClassDB::bind_method(D_METHOD("create_or_join_lobby", "secret"), &Discord::create_or_join_lobby);
	ClassDB::bind_method(D_METHOD("get_access_token"), &Discord::get_access_token);
	ClassDB::bind_method(D_METHOD("get_auth_state"), &Discord::get_auth_state);
	ClassDB::bind_method(D_METHOD("get_connection_state"), &Discord::get_connection_state);
	ClassDB::bind_method(D_METHOD("get_debug_log"), &Discord::get_debug_log);
	ClassDB::bind_method(D_METHOD("get_default_communication_scopes"), &Discord::get_default_communication_scopes);
	ClassDB::bind_method(D_METHOD("get_default_presence_scopes"), &Discord::get_default_presence_scopes);
	ClassDB::bind_method(D_METHOD("get_game_activity"), &Discord::get_game_activity);
	ClassDB::bind_method(D_METHOD("get_granted_scopes"), &Discord::get_granted_scopes);
	ClassDB::bind_method(D_METHOD("get_last_error"), &Discord::get_last_error);
	ClassDB::bind_method(D_METHOD("get_relationship", "user_id"), &Discord::get_relationship);
	ClassDB::bind_method(D_METHOD("get_relationships"), &Discord::get_relationships);
	ClassDB::bind_method(D_METHOD("get_relationships_by_group", "group"), &Discord::get_relationships_by_group);
	ClassDB::bind_method(D_METHOD("get_relationships_count"), &Discord::get_relationships_count);
	ClassDB::bind_method(D_METHOD("get_requested_scopes"), &Discord::get_requested_scopes);
	ClassDB::bind_method(D_METHOD("get_user", "user_id"), &Discord::get_user);
	ClassDB::bind_method(D_METHOD("get_user_id"), &Discord::get_user_id);
	ClassDB::bind_method(D_METHOD("get_username"), &Discord::get_username);
	ClassDB::bind_method(D_METHOD("get_version"), &Discord::get_version);
	ClassDB::bind_method(D_METHOD("initialize", "client_id"), &Discord::initialize, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("initialize_presence_only", "client_id"), &Discord::initialize_presence_only);
	ClassDB::bind_method(D_METHOD("is_authorization_pending"), &Discord::is_authorization_pending);
	ClassDB::bind_method(D_METHOD("is_authorized"), &Discord::is_authorized);
	ClassDB::bind_method(D_METHOD("is_available"), &Discord::is_available);
	ClassDB::bind_method(D_METHOD("is_client_active"), &Discord::is_client_active);
	ClassDB::bind_method(D_METHOD("is_debug_logging_enabled"), &Discord::is_debug_logging_enabled);
	ClassDB::bind_method(D_METHOD("is_frame_hook_connected"), &Discord::is_frame_hook_connected);
	ClassDB::bind_method(D_METHOD("is_initialized"), &Discord::is_initialized);
	ClassDB::bind_method(D_METHOD("is_presence_only_mode"), &Discord::is_presence_only_mode);
	ClassDB::bind_method(D_METHOD("is_ready_for_presence"), &Discord::is_ready_for_presence);
	ClassDB::bind_method(D_METHOD("refresh_relationships"), &Discord::refresh_relationships);
	ClassDB::bind_method(D_METHOD("register_launch_command", "application_id", "command"), &Discord::register_launch_command);
	ClassDB::bind_method(D_METHOD("register_launch_steam_application", "application_id", "steam_app_id"), &Discord::register_launch_steam_application);
	ClassDB::bind_method(D_METHOD("reject_discord_friend_request", "user_id"), &Discord::reject_discord_friend_request);
	ClassDB::bind_method(D_METHOD("reject_game_friend_request", "user_id"), &Discord::reject_game_friend_request);
	ClassDB::bind_method(D_METHOD("remove_discord_and_game_friend", "user_id"), &Discord::remove_discord_and_game_friend);
	ClassDB::bind_method(D_METHOD("remove_game_friend", "user_id"), &Discord::remove_game_friend);
	ClassDB::bind_method(D_METHOD("run_callbacks"), &Discord::run_callbacks);
	ClassDB::bind_method(D_METHOD("send_activity_invite", "user_id", "content"), &Discord::send_activity_invite, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("send_activity_join_request", "user_id"), &Discord::send_activity_join_request);
	ClassDB::bind_method(D_METHOD("send_activity_join_request_reply", "invite"), &Discord::send_activity_join_request_reply);
	ClassDB::bind_method(D_METHOD("send_discord_friend_request", "username"), &Discord::send_discord_friend_request);
	ClassDB::bind_method(D_METHOD("send_discord_friend_request_by_id", "user_id"), &Discord::send_discord_friend_request_by_id);
	ClassDB::bind_method(D_METHOD("send_game_friend_request", "username"), &Discord::send_game_friend_request);
	ClassDB::bind_method(D_METHOD("send_game_friend_request_by_id", "user_id"), &Discord::send_game_friend_request_by_id);
	ClassDB::bind_method(D_METHOD("set_debug_logging", "enabled"), &Discord::set_debug_logging);
	ClassDB::bind_method(D_METHOD("shutdown"), &Discord::shutdown);
	ClassDB::bind_method(D_METHOD("unblock_user", "user_id"), &Discord::unblock_user);
	ClassDB::bind_method(D_METHOD("update_rich_presence", "activity"), &Discord::update_rich_presence);

	ADD_SIGNAL(MethodInfo("activity_invite_accepted", PropertyInfo(Variant::STRING, "join_secret")));
	ADD_SIGNAL(MethodInfo("activity_invite_created", PropertyInfo(Variant::DICTIONARY, "invite")));
	ADD_SIGNAL(MethodInfo("activity_invite_sent"));
	ADD_SIGNAL(MethodInfo("activity_invite_updated", PropertyInfo(Variant::DICTIONARY, "invite")));
	ADD_SIGNAL(MethodInfo("activity_join_requested", PropertyInfo(Variant::STRING, "join_secret")));
	ADD_SIGNAL(MethodInfo("authorization_failed", PropertyInfo(Variant::STRING, "error")));
	ADD_SIGNAL(MethodInfo("authorization_started"));
	ADD_SIGNAL(MethodInfo("authorized"));
	ADD_SIGNAL(MethodInfo("connection_state_changed", PropertyInfo(Variant::INT, "state"), PropertyInfo(Variant::INT, "error"), PropertyInfo(Variant::INT, "error_detail")));
	ADD_SIGNAL(MethodInfo("lobby_joined", PropertyInfo(Variant::INT, "lobby_id")));
	ADD_SIGNAL(MethodInfo("ready"));
	ADD_SIGNAL(MethodInfo("relationship_action_completed", PropertyInfo(Variant::STRING, "action"), PropertyInfo(Variant::INT, "user_id"), PropertyInfo(Variant::BOOL, "success"), PropertyInfo(Variant::STRING, "error")));
	ADD_SIGNAL(MethodInfo("relationship_created", PropertyInfo(Variant::INT, "user_id"), PropertyInfo(Variant::BOOL, "is_discord")));
	ADD_SIGNAL(MethodInfo("relationship_deleted", PropertyInfo(Variant::INT, "user_id"), PropertyInfo(Variant::BOOL, "is_discord")));
	ADD_SIGNAL(MethodInfo("relationship_groups_updated", PropertyInfo(Variant::INT, "user_id")));
	ADD_SIGNAL(MethodInfo("relationships_updated", PropertyInfo(Variant::INT, "count")));
	ADD_SIGNAL(MethodInfo("rich_presence_updated", PropertyInfo(Variant::BOOL, "success"), PropertyInfo(Variant::STRING, "error")));
	ADD_SIGNAL(MethodInfo("user_updated", PropertyInfo(Variant::INT, "user_id")));

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

	BIND_ENUM_CONSTANT(RELATIONSHIP_NONE);
	BIND_ENUM_CONSTANT(RELATIONSHIP_FRIEND);
	BIND_ENUM_CONSTANT(RELATIONSHIP_BLOCKED);
	BIND_ENUM_CONSTANT(RELATIONSHIP_PENDING_INCOMING);
	BIND_ENUM_CONSTANT(RELATIONSHIP_PENDING_OUTGOING);
	BIND_ENUM_CONSTANT(RELATIONSHIP_IMPLICIT);
	BIND_ENUM_CONSTANT(RELATIONSHIP_SUGGESTION);

	BIND_ENUM_CONSTANT(GROUP_ONLINE_PLAYING_GAME);
	BIND_ENUM_CONSTANT(GROUP_ONLINE_ELSEWHERE);
	BIND_ENUM_CONSTANT(GROUP_OFFLINE);

	BIND_ENUM_CONSTANT(STATUS_ONLINE);
	BIND_ENUM_CONSTANT(STATUS_OFFLINE);
	BIND_ENUM_CONSTANT(STATUS_BLOCKED);
	BIND_ENUM_CONSTANT(STATUS_IDLE);
	BIND_ENUM_CONSTANT(STATUS_DND);
	BIND_ENUM_CONSTANT(STATUS_INVISIBLE);
	BIND_ENUM_CONSTANT(STATUS_STREAMING);
	BIND_ENUM_CONSTANT(STATUS_UNKNOWN);
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

Error Discord::_require_presence_ready(const char *p_operation) {
	if (ready_for_presence && (presence_only_mode || auth_state == AUTH_READY)) {
		return OK;
	}
	last_error = vformat("Discord is not ready for %s (presence_only=%s, auth_state=%d, connection=%d)",
			p_operation, presence_only_mode ? "true" : "false", (int)auth_state, (int)connection_state);
	return ERR_UNAUTHORIZED;
}

Error Discord::_require_oauth_ready(const char *p_operation) {
	if (presence_only_mode) {
		last_error = vformat("Discord OAuth is required for %s (presence-only mode active)", p_operation);
		return ERR_UNAUTHORIZED;
	}
	if (ready_for_presence && auth_state == AUTH_READY) {
		return OK;
	}
	last_error = vformat("Discord OAuth is not ready for %s (auth_state=%d, connection=%d)",
			p_operation, (int)auth_state, (int)connection_state);
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

void Discord::_emit_ready_once() {
	if (ready_signal_emitted) {
		return;
	}
	ready_signal_emitted = true;
	_log_lifecycle("Client is ready for rich presence");
	emit_signal(SNAME("ready"));
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
	_emit_ready_once();

	const bool had_pending = has_pending_presence;
	_apply_pending_presence();
	if (!had_pending && last_presence_sent.is_empty() && !presence_only_mode) {
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
	presence_only_mode = false;
	log_callback_registered = false;
	authorize_dismiss_callback_registered = false;
	activity_callbacks_registered = false;
	relationship_callbacks_registered = false;
	has_pending_presence = false;
	pending_presence = Dictionary();
	last_presence_sent = Dictionary();
	last_emitted_state = STATE_DISCONNECTED;
	last_emitted_error = 0;
	last_emitted_error_detail = 0;
	last_polled_status = -1;
	auth_state = AUTH_NONE;
	pending_relationship_action = String();
	pending_relationship_user_id = 0;
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

Error Discord::_setup_client(int64_t p_client_id) {
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
	_register_activity_callbacks();
	if (!presence_only_mode) {
		_register_relationship_callbacks();
	}
	return OK;
}

Error Discord::_initialize_client_oauth(int64_t p_client_id) {
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
	presence_only_mode = false;
	Error err = _setup_client(p_client_id);
	if (err != OK) {
		_reset_session();
		return err;
	}
	err = _initialize_client_oauth(p_client_id);
	if (err != OK) {
		_reset_session();
		return err;
	}

	initialized = true;
	_log_lifecycle(vformat("Discord auth started (client_id=%s)", String::num_int64(p_client_id)));
	return OK;
}

Error Discord::initialize_presence_only(int64_t p_client_id) {
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
	presence_only_mode = true;
	Error err = _setup_client(p_client_id);
	if (err != OK) {
		_reset_session();
		return err;
	}

	initialized = true;
	ready_for_presence = true;
	_emit_ready_once();
	_apply_pending_presence();
	_log_lifecycle(vformat("Discord presence-only RPC started (client_id=%s)", String::num_int64(p_client_id)));
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
	if (_require_presence_ready("update_rich_presence") != OK) {
		pending_presence = p_activity;
		has_pending_presence = true;
		return ERR_UNAUTHORIZED;
	}
	return _send_rich_presence(p_activity);
}

Error Discord::_send_rich_presence(const Dictionary &p_activity) {
	if (_require_presence_ready("update_rich_presence") != OK) {
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

	if (p_activity.has("name")) {
		loader.activity_set_name(&activity, (String)p_activity["name"]);
	}
	if (p_activity.has("details")) {
		loader.activity_set_details(&activity, (String)p_activity["details"]);
	}
	if (p_activity.has("state")) {
		loader.activity_set_state(&activity, (String)p_activity["state"]);
	}
	if (p_activity.has("details_url")) {
		loader.activity_set_details_url(&activity, (String)p_activity["details_url"]);
	}
	if (p_activity.has("state_url")) {
		loader.activity_set_state_url(&activity, (String)p_activity["state_url"]);
	}
	if (p_activity.has("status_display_type")) {
		const Discord_StatusDisplayTypes display_type = _parse_status_display_type((String)p_activity["status_display_type"]);
		loader.activity_set_status_display_type(&activity, display_type);
	}
	if (p_activity.has("supported_platforms")) {
		const Discord_ActivityGamePlatforms platforms = _parse_supported_platforms(p_activity["supported_platforms"]);
		if (platforms != 0) {
			loader.activity_set_supported_platforms(&activity, platforms);
		}
	}

	const bool has_assets = p_activity.has("large_image") || p_activity.has("large_text") ||
			p_activity.has("small_image") || p_activity.has("small_text") ||
			p_activity.has("large_url") || p_activity.has("small_url") ||
			p_activity.has("invite_cover_image");
	if (has_assets) {
		Discord_ActivityAssets assets;
		loader.activity_assets_init(&assets);
		if (p_activity.has("large_image")) {
			loader.activity_assets_set_large_image(&assets, (String)p_activity["large_image"]);
		}
		if (p_activity.has("large_text")) {
			loader.activity_assets_set_large_text(&assets, (String)p_activity["large_text"]);
		}
		if (p_activity.has("large_url")) {
			loader.activity_assets_set_large_url(&assets, (String)p_activity["large_url"]);
		}
		if (p_activity.has("small_image")) {
			loader.activity_assets_set_small_image(&assets, (String)p_activity["small_image"]);
		}
		if (p_activity.has("small_text")) {
			loader.activity_assets_set_small_text(&assets, (String)p_activity["small_text"]);
		}
		if (p_activity.has("small_url")) {
			loader.activity_assets_set_small_url(&assets, (String)p_activity["small_url"]);
		}
		if (p_activity.has("invite_cover_image")) {
			loader.activity_assets_set_invite_cover_image(&assets, (String)p_activity["invite_cover_image"]);
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

	const bool has_party = p_activity.has("party_id") || p_activity.has("party_current") || p_activity.has("party_max");
	if (has_party && loader.has_activity_extensions()) {
		Discord_ActivityParty party;
		loader.activity_party_init(&party);
		if (p_activity.has("party_id")) {
			loader.activity_party_set_id(&party, (String)p_activity["party_id"]);
		}
		if (p_activity.has("party_current")) {
			loader.activity_party_set_current_size(&party, (int32_t)(int)p_activity["party_current"]);
		}
		if (p_activity.has("party_max")) {
			loader.activity_party_set_max_size(&party, (int32_t)(int)p_activity["party_max"]);
		}
		loader.activity_set_party(&activity, &party);
		loader.activity_party_drop(&party);
	}

	const bool has_secrets = p_activity.has("join_secret") || p_activity.has("spectate_secret") || p_activity.has("match_secret");
	if (has_secrets && loader.has_activity_extensions()) {
		Discord_ActivitySecrets secrets;
		loader.activity_secrets_init(&secrets);
		if (p_activity.has("join_secret")) {
			loader.activity_secrets_set_join(&secrets, (String)p_activity["join_secret"]);
		}
		if (p_activity.has("spectate_secret") || p_activity.has("match_secret")) {
			_log_debug("spectate_secret/match_secret are not supported by the loaded Discord SDK; use join_secret");
		}
		loader.activity_set_secrets(&activity, &secrets);
		loader.activity_secrets_drop(&secrets);
	}

	if (p_activity.has("buttons") && loader.has_activity_extensions()) {
		const Variant buttons_variant = p_activity["buttons"];
		if (buttons_variant.get_type() == Variant::ARRAY) {
			const Array buttons = buttons_variant;
			for (int i = 0; i < buttons.size(); i++) {
				if (buttons[i].get_type() != Variant::DICTIONARY) {
					continue;
				}
				const Dictionary button_dict = buttons[i];
				if (!button_dict.has("label") || !button_dict.has("url")) {
					continue;
				}
				Discord_ActivityButton button;
				loader.activity_button_init(&button);
				loader.activity_button_set_label(&button, (String)button_dict["label"]);
				loader.activity_button_set_url(&button, (String)button_dict["url"]);
				loader.activity_add_button(&activity, &button);
				loader.activity_button_drop(&button);
			}
		}
	}

	loader.client_update_rich_presence(&client, &activity, Discord::_update_rich_presence_callback, this);
	loader.activity_drop(&activity);
	return OK;
}

void Discord::clear_rich_presence() {
	if (_require_presence_ready("clear_rich_presence") != OK) {
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
	if (_require_oauth_ready("authenticate_with_server") != OK) {
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

void Discord::_register_activity_callbacks() {
	if (!client_active || activity_callbacks_registered) {
		return;
	}
	loader.client_set_activity_invite_created_callback(&client, Discord::_activity_invite_created_callback, this);
	loader.client_set_activity_invite_updated_callback(&client, Discord::_activity_invite_updated_callback, this);
	loader.client_set_activity_join_callback(&client, Discord::_activity_join_callback, this);
	loader.client_set_lobby_created_callback(&client, Discord::_lobby_created_callback, this);
	activity_callbacks_registered = true;
}

void Discord::_register_relationship_callbacks() {
	if (!client_active || relationship_callbacks_registered || !loader.has_relationship_api()) {
		return;
	}
	loader.client_set_relationship_created_callback(&client, Discord::_relationship_created_callback, this);
	loader.client_set_relationship_deleted_callback(&client, Discord::_relationship_deleted_callback, this);
	loader.client_set_relationship_groups_updated_callback(&client, Discord::_relationship_groups_updated_callback, this);
	loader.client_set_user_updated_callback(&client, Discord::_user_updated_callback, this);
	relationship_callbacks_registered = true;
}

Discord_StatusDisplayTypes Discord::_parse_status_display_type(const String &p_value) const {
	const String normalized = p_value.to_lower();
	if (normalized == "state") {
		return DISCORD_STATUS_DISPLAY_TYPE_STATE;
	}
	if (normalized == "details") {
		return DISCORD_STATUS_DISPLAY_TYPE_DETAILS;
	}
	return DISCORD_STATUS_DISPLAY_TYPE_NAME;
}

Discord_ActivityGamePlatforms Discord::_parse_supported_platforms(const Variant &p_value) const {
	if (p_value.get_type() == Variant::INT) {
		return (Discord_ActivityGamePlatforms)(int)p_value;
	}
	if (p_value.get_type() != Variant::ARRAY) {
		return (Discord_ActivityGamePlatforms)0;
	}
	int mask = 0;
	const Array platforms = p_value;
	for (int i = 0; i < platforms.size(); i++) {
		const String platform = String(platforms[i]).to_lower();
		if (platform == "desktop") {
			mask |= DISCORD_ACTIVITY_GAME_PLATFORM_DESKTOP;
		} else if (platform == "xbox") {
			mask |= DISCORD_ACTIVITY_GAME_PLATFORM_XBOX;
		} else if (platform == "ios") {
			mask |= DISCORD_ACTIVITY_GAME_PLATFORM_IOS;
		} else if (platform == "android") {
			mask |= DISCORD_ACTIVITY_GAME_PLATFORM_ANDROID;
		}
	}
	return (Discord_ActivityGamePlatforms)mask;
}

Dictionary Discord::_serialize_user(Discord_UserHandle *p_user) {
	Dictionary out;
	if (!p_user) {
		return out;
	}
	out["id"] = (int64_t)loader.user_handle_id(p_user);
	out["username"] = loader.user_handle_username(p_user);
	out["display_name"] = loader.user_handle_display_name(p_user);
	out["global_name"] = loader.user_handle_global_name(p_user);
	out["is_provisional"] = loader.user_handle_is_provisional(p_user);
	out["status"] = (int)loader.user_handle_status(p_user);
	out["avatar_url"] = loader.user_handle_avatar_url(p_user);
	Discord_Activity activity;
	out["has_game_activity"] = loader.user_handle_game_activity(p_user, &activity);
	if (out["has_game_activity"]) {
		loader.activity_drop(&activity);
	}
	return out;
}

Dictionary Discord::_serialize_relationship(Discord_RelationshipHandle *p_relationship) {
	Dictionary out;
	if (!p_relationship) {
		return out;
	}
	const uint64_t relationship_id = loader.relationship_handle_id(p_relationship);
	out["user_id"] = (int64_t)relationship_id;
	out["discord_relationship_type"] = (int)loader.relationship_handle_discord_relationship_type(p_relationship);
	out["game_relationship_type"] = (int)loader.relationship_handle_game_relationship_type(p_relationship);
	out["is_spam_request"] = loader.relationship_handle_is_spam_request(p_relationship);
	Discord_UserHandle user;
	if (loader.relationship_handle_user(p_relationship, &user)) {
		out["user"] = _serialize_user(&user);
		loader.user_handle_drop(&user);
	}
	return out;
}

Array Discord::_serialize_relationship_span(const Discord_RelationshipHandleSpan &p_span) {
	Array out;
	for (size_t i = 0; i < p_span.size; i++) {
		out.push_back(_serialize_relationship(&p_span.ptr[i]));
	}
	return out;
}

bool Discord::_build_activity_invite(const Dictionary &p_invite, Discord_ActivityInvite *r_invite) const {
	if (!r_invite || !loader.has_invite_api()) {
		return false;
	}
	loader.activity_invite_init(r_invite);
	if (p_invite.has("sender_id")) {
		loader.activity_invite_set_sender_id(r_invite, (uint64_t)(int64_t)p_invite["sender_id"]);
	}
	if (p_invite.has("channel_id")) {
		loader.activity_invite_set_channel_id(r_invite, (uint64_t)(int64_t)p_invite["channel_id"]);
	}
	if (p_invite.has("message_id")) {
		loader.activity_invite_set_message_id(r_invite, (uint64_t)(int64_t)p_invite["message_id"]);
	}
	if (p_invite.has("type")) {
		loader.activity_invite_set_type(r_invite, (Discord_ActivityActionTypes)(int)p_invite["type"]);
	}
	if (p_invite.has("application_id")) {
		loader.activity_invite_set_application_id(r_invite, (uint64_t)(int64_t)p_invite["application_id"]);
	}
	if (p_invite.has("parent_application_id")) {
		loader.activity_invite_set_parent_application_id(r_invite, (uint64_t)(int64_t)p_invite["parent_application_id"]);
	}
	if (p_invite.has("party_id")) {
		loader.activity_invite_set_party_id(r_invite, (String)p_invite["party_id"]);
	}
	if (p_invite.has("session_id")) {
		loader.activity_invite_set_session_id(r_invite, (String)p_invite["session_id"]);
	}
	if (p_invite.has("is_valid")) {
		loader.activity_invite_set_is_valid(r_invite, (bool)p_invite["is_valid"]);
	}
	return true;
}

void Discord::_emit_relationship_action_completed(Discord_ClientResult *result, const String &p_action, uint64_t p_user_id) {
	const bool success = result ? loader.client_result_successful(result) : false;
	const String error = success ? String() : (result ? loader.client_result_error(result) : String("Discord SDK relationship API unavailable"));
	if (result) {
		loader.client_result_drop(result);
	}
	emit_signal(SNAME("relationship_action_completed"), p_action, (int64_t)p_user_id, success, error);
}

bool Discord::register_launch_command(int64_t p_application_id, const String &p_command) {
	if (_require_presence_ready("register_launch_command") != OK) {
		return false;
	}
	const int64_t app_id = p_application_id > 0 ? p_application_id : client_id;
	return loader.client_register_launch_command(&client, (uint64_t)app_id, p_command);
}

bool Discord::register_launch_steam_application(int64_t p_application_id, int p_steam_app_id) {
	if (_require_presence_ready("register_launch_steam_application") != OK) {
		return false;
	}
	const int64_t app_id = p_application_id > 0 ? p_application_id : client_id;
	return loader.client_register_launch_steam_application(&client, (uint64_t)app_id, (uint32_t)p_steam_app_id);
}

void Discord::send_activity_invite(uint64_t p_user_id, const String &p_content) {
	if (_require_oauth_ready("send_activity_invite") != OK) {
		return;
	}
	loader.client_send_activity_invite(&client, p_user_id, p_content, Discord::_send_activity_invite_callback, this);
}

void Discord::send_activity_join_request(uint64_t p_user_id) {
	if (_require_oauth_ready("send_activity_join_request") != OK) {
		return;
	}
	loader.client_send_activity_join_request(&client, p_user_id, Discord::_send_activity_invite_callback, this);
}

void Discord::send_activity_join_request_reply(const Dictionary &p_invite) {
	if (_require_oauth_ready("send_activity_join_request_reply") != OK) {
		return;
	}
	Discord_ActivityInvite invite;
	if (!_build_activity_invite(p_invite, &invite)) {
		last_error = "Failed to build activity invite";
		return;
	}
	loader.client_send_activity_join_request_reply(&client, &invite, Discord::_send_activity_invite_callback, this);
	loader.activity_invite_drop(&invite);
}

void Discord::accept_activity_invite(const Dictionary &p_invite) {
	if (_require_oauth_ready("accept_activity_invite") != OK) {
		return;
	}
	Discord_ActivityInvite invite;
	if (!_build_activity_invite(p_invite, &invite)) {
		last_error = "Failed to build activity invite";
		return;
	}
	loader.client_accept_activity_invite(&client, &invite, Discord::_accept_activity_invite_callback, this);
	loader.activity_invite_drop(&invite);
}

void Discord::create_or_join_lobby(const String &p_secret) {
	if (_require_oauth_ready("create_or_join_lobby") != OK) {
		return;
	}
	loader.client_create_or_join_lobby(&client, p_secret, Discord::_create_or_join_lobby_callback, this);
}

Array Discord::get_relationships() {
	Array out;
	if (_require_oauth_ready("get_relationships") != OK) {
		return out;
	}
	Discord_RelationshipHandleSpan span;
	loader.client_get_relationships(&client, &span);
	return _serialize_relationship_span(span);
}

Array Discord::get_relationships_by_group(RelationshipGroupType p_group) {
	Array out;
	if (_require_oauth_ready("get_relationships_by_group") != OK) {
		return out;
	}
	Discord_RelationshipHandleSpan span;
	loader.client_get_relationships_by_group(&client, (Discord_RelationshipGroupType)p_group, &span);
	return _serialize_relationship_span(span);
}

Dictionary Discord::get_relationship(uint64_t p_user_id) {
	Dictionary out;
	if (_require_oauth_ready("get_relationship") != OK) {
		return out;
	}
	Discord_RelationshipHandle relationship;
	loader.client_get_relationship_handle(&client, p_user_id, &relationship);
	out = _serialize_relationship(&relationship);
	loader.relationship_handle_drop(&relationship);
	return out;
}

Dictionary Discord::get_user(uint64_t p_user_id) {
	Dictionary out;
	if (_require_oauth_ready("get_user") != OK) {
		return out;
	}
	Discord_UserHandle user;
	if (!loader.client_get_user(&client, p_user_id, &user)) {
		return out;
	}
	out = _serialize_user(&user);
	loader.user_handle_drop(&user);
	return out;
}

void Discord::refresh_relationships() {
	if (_require_oauth_ready("refresh_relationships") != OK) {
		return;
	}
	_refresh_relationships();
}

void Discord::send_game_friend_request(const String &p_username) {
	if (_require_oauth_ready("send_game_friend_request") != OK) {
		return;
	}
	pending_relationship_action = "send_game_friend_request";
	pending_relationship_user_id = 0;
	loader.client_send_game_friend_request(&client, p_username, Discord::_send_friend_request_callback, this);
}

void Discord::send_game_friend_request_by_id(uint64_t p_user_id) {
	if (_require_oauth_ready("send_game_friend_request_by_id") != OK) {
		return;
	}
	pending_relationship_action = "send_game_friend_request_by_id";
	pending_relationship_user_id = p_user_id;
	loader.client_send_game_friend_request_by_id(&client, p_user_id, Discord::_relationship_action_callback, this);
}

void Discord::send_discord_friend_request(const String &p_username) {
	if (_require_oauth_ready("send_discord_friend_request") != OK) {
		return;
	}
	pending_relationship_action = "send_discord_friend_request";
	pending_relationship_user_id = 0;
	loader.client_send_discord_friend_request(&client, p_username, Discord::_send_friend_request_callback, this);
}

void Discord::send_discord_friend_request_by_id(uint64_t p_user_id) {
	if (_require_oauth_ready("send_discord_friend_request_by_id") != OK) {
		return;
	}
	pending_relationship_action = "send_discord_friend_request_by_id";
	pending_relationship_user_id = p_user_id;
	loader.client_send_discord_friend_request_by_id(&client, p_user_id, Discord::_relationship_action_callback, this);
}

void Discord::accept_game_friend_request(uint64_t p_user_id) {
	if (_require_oauth_ready("accept_game_friend_request") != OK) {
		return;
	}
	pending_relationship_action = "accept_game_friend_request";
	pending_relationship_user_id = p_user_id;
	loader.client_accept_game_friend_request(&client, p_user_id, Discord::_relationship_action_callback, this);
}

void Discord::accept_discord_friend_request(uint64_t p_user_id) {
	if (_require_oauth_ready("accept_discord_friend_request") != OK) {
		return;
	}
	pending_relationship_action = "accept_discord_friend_request";
	pending_relationship_user_id = p_user_id;
	loader.client_accept_discord_friend_request(&client, p_user_id, Discord::_relationship_action_callback, this);
}

void Discord::reject_game_friend_request(uint64_t p_user_id) {
	if (_require_oauth_ready("reject_game_friend_request") != OK) {
		return;
	}
	pending_relationship_action = "reject_game_friend_request";
	pending_relationship_user_id = p_user_id;
	loader.client_reject_game_friend_request(&client, p_user_id, Discord::_relationship_action_callback, this);
}

void Discord::reject_discord_friend_request(uint64_t p_user_id) {
	if (_require_oauth_ready("reject_discord_friend_request") != OK) {
		return;
	}
	pending_relationship_action = "reject_discord_friend_request";
	pending_relationship_user_id = p_user_id;
	loader.client_reject_discord_friend_request(&client, p_user_id, Discord::_relationship_action_callback, this);
}

void Discord::cancel_game_friend_request(uint64_t p_user_id) {
	if (_require_oauth_ready("cancel_game_friend_request") != OK) {
		return;
	}
	pending_relationship_action = "cancel_game_friend_request";
	pending_relationship_user_id = p_user_id;
	loader.client_cancel_game_friend_request(&client, p_user_id, Discord::_relationship_action_callback, this);
}

void Discord::cancel_discord_friend_request(uint64_t p_user_id) {
	if (_require_oauth_ready("cancel_discord_friend_request") != OK) {
		return;
	}
	pending_relationship_action = "cancel_discord_friend_request";
	pending_relationship_user_id = p_user_id;
	loader.client_cancel_discord_friend_request(&client, p_user_id, Discord::_relationship_action_callback, this);
}

void Discord::remove_game_friend(uint64_t p_user_id) {
	if (_require_oauth_ready("remove_game_friend") != OK) {
		return;
	}
	pending_relationship_action = "remove_game_friend";
	pending_relationship_user_id = p_user_id;
	loader.client_remove_game_friend(&client, p_user_id, Discord::_relationship_action_callback, this);
}

void Discord::remove_discord_and_game_friend(uint64_t p_user_id) {
	if (_require_oauth_ready("remove_discord_and_game_friend") != OK) {
		return;
	}
	pending_relationship_action = "remove_discord_and_game_friend";
	pending_relationship_user_id = p_user_id;
	loader.client_remove_discord_and_game_friend(&client, p_user_id, Discord::_relationship_action_callback, this);
}

void Discord::block_user(uint64_t p_user_id) {
	if (_require_oauth_ready("block_user") != OK) {
		return;
	}
	pending_relationship_action = "block_user";
	pending_relationship_user_id = p_user_id;
	loader.client_block_user(&client, p_user_id, Discord::_relationship_action_callback, this);
}

void Discord::unblock_user(uint64_t p_user_id) {
	if (_require_oauth_ready("unblock_user") != OK) {
		return;
	}
	pending_relationship_action = "unblock_user";
	pending_relationship_user_id = p_user_id;
	loader.client_unblock_user(&client, p_user_id, Discord::_relationship_action_callback, this);
}

void Discord::_activity_invite_created_callback(Discord_ActivityInvite *invite, void *user_data) {
	Discord *discord = (Discord *)user_data;
	if (!discord || !invite) {
		return;
	}
	discord->emit_signal(SNAME("activity_invite_created"), discord->loader.activity_invite_to_dict(invite));
}

void Discord::_activity_invite_updated_callback(Discord_ActivityInvite *invite, void *user_data) {
	Discord *discord = (Discord *)user_data;
	if (!discord || !invite) {
		return;
	}
	discord->emit_signal(SNAME("activity_invite_updated"), discord->loader.activity_invite_to_dict(invite));
}

void Discord::_activity_join_callback(Discord_String join_secret, void *user_data) {
	Discord *discord = (Discord *)user_data;
	if (!discord) {
		return;
	}
	discord->emit_signal(SNAME("activity_join_requested"), DiscordAPILoader::to_godot_string(join_secret));
}

void Discord::_accept_activity_invite_callback(Discord_ClientResult *result, Discord_String join_secret, void *user_data) {
	Discord *discord = (Discord *)user_data;
	if (!discord) {
		return;
	}
	const bool success = discord->loader.client_result_successful(result);
	const String error = success ? String() : discord->loader.client_result_error(result);
	discord->loader.client_result_drop(result);
	if (success) {
		discord->emit_signal(SNAME("activity_invite_accepted"), DiscordAPILoader::to_godot_string(join_secret));
	} else if (!error.is_empty()) {
		discord->_set_last_error(error);
	}
}

void Discord::_send_activity_invite_callback(Discord_ClientResult *result, void *user_data) {
	Discord *discord = (Discord *)user_data;
	if (!discord) {
		return;
	}
	const bool success = discord->loader.client_result_successful(result);
	const String error = success ? String() : discord->loader.client_result_error(result);
	discord->loader.client_result_drop(result);
	if (success) {
		discord->emit_signal(SNAME("activity_invite_sent"));
	} else if (!error.is_empty()) {
		discord->_set_last_error(error);
	}
}

void Discord::_create_or_join_lobby_callback(Discord_ClientResult *result, uint64_t lobby_id, void *user_data) {
	Discord *discord = (Discord *)user_data;
	if (!discord) {
		return;
	}
	const bool success = discord->loader.client_result_successful(result);
	const String error = success ? String() : discord->loader.client_result_error(result);
	discord->loader.client_result_drop(result);
	if (success) {
		discord->emit_signal(SNAME("lobby_joined"), (int64_t)lobby_id);
	} else if (!error.is_empty()) {
		discord->_set_last_error(error);
	}
}

void Discord::_lobby_created_callback(uint64_t lobby_id, void *user_data) {
	Discord *discord = (Discord *)user_data;
	if (!discord) {
		return;
	}
	discord->emit_signal(SNAME("lobby_joined"), (int64_t)lobby_id);
}

void Discord::_relationship_created_callback(uint64_t user_id, bool is_discord, void *user_data) {
	Discord *discord = (Discord *)user_data;
	if (!discord) {
		return;
	}
	discord->emit_signal(SNAME("relationship_created"), (int64_t)user_id, is_discord);
}

void Discord::_relationship_deleted_callback(uint64_t user_id, bool is_discord, void *user_data) {
	Discord *discord = (Discord *)user_data;
	if (!discord) {
		return;
	}
	discord->emit_signal(SNAME("relationship_deleted"), (int64_t)user_id, is_discord);
}

void Discord::_relationship_groups_updated_callback(uint64_t user_id, void *user_data) {
	Discord *discord = (Discord *)user_data;
	if (!discord) {
		return;
	}
	discord->emit_signal(SNAME("relationship_groups_updated"), (int64_t)user_id);
}

void Discord::_user_updated_callback(uint64_t user_id, void *user_data) {
	Discord *discord = (Discord *)user_data;
	if (!discord) {
		return;
	}
	discord->emit_signal(SNAME("user_updated"), (int64_t)user_id);
}

void Discord::_relationship_action_callback(Discord_ClientResult *result, void *user_data) {
	Discord *discord = (Discord *)user_data;
	if (!discord) {
		return;
	}
	const String action = discord->pending_relationship_action;
	const uint64_t user_id = discord->pending_relationship_user_id;
	discord->pending_relationship_action = String();
	discord->pending_relationship_user_id = 0;
	discord->_emit_relationship_action_completed(result, action, user_id);
}

void Discord::_send_friend_request_callback(Discord_ClientResult *result, void *user_data) {
	Discord *discord = (Discord *)user_data;
	if (!discord) {
		return;
	}
	const String action = discord->pending_relationship_action;
	discord->pending_relationship_action = String();
	discord->_emit_relationship_action_completed(result, action, 0);
}
