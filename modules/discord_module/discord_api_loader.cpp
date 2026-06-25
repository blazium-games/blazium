/**************************************************************************/
/*  discord_api_loader.cpp                                                */
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

#include "discord_api_loader.h"

#include "core/io/file_access.h"
#include "core/os/os.h"

String DiscordAPILoader::get_runtime_library_name() {
#ifdef WINDOWS_ENABLED
	return "discord_partner_sdk.dll";
#else
	return "libdiscord_partner_sdk.so";
#endif
}

bool DiscordAPILoader::is_runtime_library_present() {
	const String library_name = get_runtime_library_name();
	const String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();

	Vector<String> candidates;
	candidates.push_back(exe_dir.path_join(library_name));
	candidates.push_back(library_name);
#ifndef WINDOWS_ENABLED
	candidates.push_back(exe_dir.path_join("../lib").path_join(library_name));
#endif

	for (int i = 0; i < candidates.size(); i++) {
		if (FileAccess::exists(candidates[i])) {
			return true;
		}
	}
	return false;
}

Discord_String DiscordAPILoader::make_string(const String &p_value) {
	Discord_String out;
	CharString utf8 = p_value.utf8();
	out.ptr = (uint8_t *)utf8.ptr();
	out.size = utf8.length();
	return out;
}

String DiscordAPILoader::to_godot_string(const Discord_String &p_value) {
	if (p_value.ptr == nullptr || p_value.size == 0) {
		return String();
	}
	return String::utf8((const char *)p_value.ptr, p_value.size);
}

bool DiscordAPILoader::_load_symbol(const char *p_name, void *&r_symbol) {
	r_symbol = nullptr;
	if (!library_handle) {
		return false;
	}
	Error err = OS::get_singleton()->get_dynamic_library_symbol_handle(library_handle, p_name, r_symbol);
	return err == OK && r_symbol != nullptr;
}

bool DiscordAPILoader::try_load() {
	if (loaded) {
		return true;
	}

	Vector<String> candidates;
	candidates.push_back(get_runtime_library_name());

	String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
	for (int i = 0; i < candidates.size(); i++) {
		String path = exe_dir.path_join(candidates[i]);
		if (!FileAccess::exists(path)) {
			path = candidates[i];
		}
#ifndef WINDOWS_ENABLED
		if (!FileAccess::exists(path)) {
			path = exe_dir.path_join("../lib").path_join(candidates[i]);
		}
#endif
		if (!FileAccess::exists(path)) {
			continue;
		}
		Error err = OS::get_singleton()->open_dynamic_library(path, library_handle);
		if (err == OK) {
			break;
		}
	}

	if (!library_handle) {
		return false;
	}

#define LOAD_REQUIRED(name, member)        \
	{                                      \
		void *symbol = nullptr;            \
		if (!_load_symbol(name, symbol)) { \
			unload();                      \
			return false;                  \
		}                                  \
		member = (decltype(member))symbol; \
	}

	LOAD_REQUIRED("Discord_RunCallbacks", fn_run_callbacks);
	LOAD_REQUIRED("Discord_Client_Init", fn_client_init);
	LOAD_REQUIRED("Discord_Client_Drop", fn_client_drop);
	LOAD_REQUIRED("Discord_Client_SetApplicationId", fn_client_set_application_id);
	LOAD_REQUIRED("Discord_Client_SetStatusChangedCallback", fn_client_set_status_changed_callback);
	LOAD_REQUIRED("Discord_Client_CreateAuthorizationCodeVerifier", fn_client_create_authorization_code_verifier);
	LOAD_REQUIRED("Discord_AuthorizationArgs_Init", fn_authorization_args_init);
	LOAD_REQUIRED("Discord_AuthorizationArgs_Drop", fn_authorization_args_drop);
	LOAD_REQUIRED("Discord_AuthorizationArgs_SetClientId", fn_authorization_args_set_client_id);
	LOAD_REQUIRED("Discord_AuthorizationArgs_SetScopes", fn_authorization_args_set_scopes);
	LOAD_REQUIRED("Discord_AuthorizationArgs_SetCodeChallenge", fn_authorization_args_set_code_challenge);
	LOAD_REQUIRED("Discord_AuthorizationCodeVerifier_Drop", fn_authorization_code_verifier_drop);
	LOAD_REQUIRED("Discord_AuthorizationCodeVerifier_Challenge", fn_authorization_code_verifier_challenge);
	LOAD_REQUIRED("Discord_AuthorizationCodeVerifier_Verifier", fn_authorization_code_verifier_verifier);
	LOAD_REQUIRED("Discord_Client_Authorize", fn_client_authorize);
	LOAD_REQUIRED("Discord_Client_GetToken", fn_client_get_token);
	LOAD_REQUIRED("Discord_Client_UpdateToken", fn_client_update_token);
	LOAD_REQUIRED("Discord_Client_Connect", fn_client_connect);
	LOAD_REQUIRED("Discord_Client_GetStatus", fn_client_get_status);
	LOAD_REQUIRED("Discord_Client_GetCurrentUserV2", fn_client_get_current_user_v2);
	LOAD_REQUIRED("Discord_ClientResult_Drop", fn_client_result_drop);
	LOAD_REQUIRED("Discord_ClientResult_Successful", fn_client_result_successful);
	LOAD_REQUIRED("Discord_ClientResult_Error", fn_client_result_error);
	LOAD_REQUIRED("Discord_UserHandle_Drop", fn_user_handle_drop);
	LOAD_REQUIRED("Discord_UserHandle_Id", fn_user_handle_id);
	LOAD_REQUIRED("Discord_UserHandle_DisplayName", fn_user_handle_display_name);
	LOAD_REQUIRED("Discord_UserHandle_Username", fn_user_handle_username);
	LOAD_REQUIRED("Discord_UserHandle_GameActivity", fn_user_handle_game_activity);
	LOAD_REQUIRED("Discord_Activity_Type", fn_activity_type);
	LOAD_REQUIRED("Discord_Activity_Details", fn_activity_details);
	LOAD_REQUIRED("Discord_Activity_State", fn_activity_state);
	LOAD_REQUIRED("Discord_Client_GetDefaultCommunicationScopes", fn_client_get_default_communication_scopes);
	LOAD_REQUIRED("Discord_Client_GetDefaultPresenceScopes", fn_client_get_default_presence_scopes);
	LOAD_REQUIRED("Discord_Client_AddLogCallback", fn_client_add_log_callback);
	LOAD_REQUIRED("Discord_Client_GetRelationships", fn_client_get_relationships);
	LOAD_REQUIRED("Discord_Activity_Init", fn_activity_init);
	LOAD_REQUIRED("Discord_Activity_Drop", fn_activity_drop);
	LOAD_REQUIRED("Discord_Activity_SetType", fn_activity_set_type);
	LOAD_REQUIRED("Discord_Activity_SetDetails", fn_activity_set_details);
	LOAD_REQUIRED("Discord_Activity_SetState", fn_activity_set_state);
	LOAD_REQUIRED("Discord_Activity_SetAssets", fn_activity_set_assets);
	LOAD_REQUIRED("Discord_Activity_SetTimestamps", fn_activity_set_timestamps);
	LOAD_REQUIRED("Discord_ActivityAssets_Init", fn_activity_assets_init);
	LOAD_REQUIRED("Discord_ActivityAssets_Drop", fn_activity_assets_drop);
	LOAD_REQUIRED("Discord_ActivityAssets_SetLargeImage", fn_activity_assets_set_large_image);
	LOAD_REQUIRED("Discord_ActivityAssets_SetLargeText", fn_activity_assets_set_large_text);
	LOAD_REQUIRED("Discord_ActivityAssets_SetSmallImage", fn_activity_assets_set_small_image);
	LOAD_REQUIRED("Discord_ActivityAssets_SetSmallText", fn_activity_assets_set_small_text);
	LOAD_REQUIRED("Discord_ActivityTimestamps_Init", fn_activity_timestamps_init);
	LOAD_REQUIRED("Discord_ActivityTimestamps_Drop", fn_activity_timestamps_drop);
	LOAD_REQUIRED("Discord_ActivityTimestamps_SetStart", fn_activity_timestamps_set_start);
	LOAD_REQUIRED("Discord_ActivityTimestamps_SetEnd", fn_activity_timestamps_set_end);
	LOAD_REQUIRED("Discord_Client_UpdateRichPresence", fn_client_update_rich_presence);
	LOAD_REQUIRED("Discord_Client_ClearRichPresence", fn_client_clear_rich_presence);
	LOAD_REQUIRED("Discord_Client_GetVersionHash", fn_client_get_version_hash);
	LOAD_REQUIRED("Discord_Client_GetVersionMajor", fn_client_get_version_major);
	LOAD_REQUIRED("Discord_Client_GetVersionMinor", fn_client_get_version_minor);
	LOAD_REQUIRED("Discord_Client_GetVersionPatch", fn_client_get_version_patch);

#undef LOAD_REQUIRED

#define LOAD_OPTIONAL(name, member)            \
	{                                          \
		void *symbol = nullptr;                \
		if (_load_symbol(name, symbol)) {      \
			member = (decltype(member))symbol; \
		}                                      \
	}

	LOAD_OPTIONAL("Discord_AuthorizationArgs_SetIntegrationType", fn_authorization_args_set_integration_type);
	LOAD_OPTIONAL("Discord_Client_SetAuthorizeDeviceScreenClosedCallback",
			fn_client_set_authorize_device_screen_closed_callback);
	LOAD_OPTIONAL("Discord_Client_SetGameWindowPid", fn_client_set_game_window_pid);
	LOAD_OPTIONAL("Discord_Client_SetLogDir", fn_client_set_log_dir);

#undef LOAD_OPTIONAL

	loaded = true;
	return true;
}

void DiscordAPILoader::unload() {
	if (library_handle) {
		OS::get_singleton()->close_dynamic_library(library_handle);
		library_handle = nullptr;
	}
	loaded = false;

	fn_run_callbacks = nullptr;
	fn_client_init = nullptr;
	fn_client_drop = nullptr;
	fn_client_set_application_id = nullptr;
	fn_client_set_status_changed_callback = nullptr;
	fn_client_set_authorize_device_screen_closed_callback = nullptr;
	fn_client_create_authorization_code_verifier = nullptr;
	fn_authorization_args_init = nullptr;
	fn_authorization_args_drop = nullptr;
	fn_authorization_args_set_client_id = nullptr;
	fn_authorization_args_set_scopes = nullptr;
	fn_authorization_args_set_code_challenge = nullptr;
	fn_authorization_args_set_integration_type = nullptr;
	fn_authorization_code_verifier_drop = nullptr;
	fn_authorization_code_verifier_challenge = nullptr;
	fn_authorization_code_verifier_verifier = nullptr;
	fn_client_authorize = nullptr;
	fn_client_get_token = nullptr;
	fn_client_update_token = nullptr;
	fn_client_connect = nullptr;
	fn_client_get_status = nullptr;
	fn_client_get_current_user_v2 = nullptr;
	fn_client_result_drop = nullptr;
	fn_client_result_successful = nullptr;
	fn_client_result_error = nullptr;
	fn_user_handle_drop = nullptr;
	fn_user_handle_id = nullptr;
	fn_user_handle_display_name = nullptr;
	fn_user_handle_username = nullptr;
	fn_user_handle_game_activity = nullptr;
	fn_activity_type = nullptr;
	fn_activity_details = nullptr;
	fn_activity_state = nullptr;
	fn_client_get_default_communication_scopes = nullptr;
	fn_client_get_default_presence_scopes = nullptr;
	fn_client_add_log_callback = nullptr;
	fn_client_get_relationships = nullptr;
	fn_activity_init = nullptr;
	fn_activity_drop = nullptr;
	fn_activity_set_type = nullptr;
	fn_activity_set_details = nullptr;
	fn_activity_set_state = nullptr;
	fn_activity_set_assets = nullptr;
	fn_activity_set_timestamps = nullptr;
	fn_activity_assets_init = nullptr;
	fn_activity_assets_drop = nullptr;
	fn_activity_assets_set_large_image = nullptr;
	fn_activity_assets_set_large_text = nullptr;
	fn_activity_assets_set_small_image = nullptr;
	fn_activity_assets_set_small_text = nullptr;
	fn_activity_timestamps_init = nullptr;
	fn_activity_timestamps_drop = nullptr;
	fn_activity_timestamps_set_start = nullptr;
	fn_activity_timestamps_set_end = nullptr;
	fn_client_update_rich_presence = nullptr;
	fn_client_clear_rich_presence = nullptr;
	fn_client_get_version_hash = nullptr;
	fn_client_get_version_major = nullptr;
	fn_client_get_version_minor = nullptr;
	fn_client_get_version_patch = nullptr;
	fn_client_set_game_window_pid = nullptr;
	fn_client_set_log_dir = nullptr;
}

void DiscordAPILoader::run_callbacks() const {
	if (fn_run_callbacks) {
		fn_run_callbacks();
	}
}

void DiscordAPILoader::client_init(Discord_Client *self) const {
	fn_client_init(self);
}

void DiscordAPILoader::client_drop(Discord_Client *self) const {
	fn_client_drop(self);
}

void DiscordAPILoader::client_set_application_id(Discord_Client *self, uint64_t application_id) const {
	fn_client_set_application_id(self, application_id);
}

void DiscordAPILoader::client_set_status_changed_callback(Discord_Client *self,
		Discord_Client_OnStatusChanged callback,
		void *user_data) const {
	fn_client_set_status_changed_callback(self, callback, nullptr, user_data);
}

bool DiscordAPILoader::client_set_authorize_device_screen_closed_callback(Discord_Client *self,
		Discord_Client_AuthorizeDeviceScreenClosedCallback callback,
		void *user_data) const {
	if (!fn_client_set_authorize_device_screen_closed_callback) {
		return false;
	}
	fn_client_set_authorize_device_screen_closed_callback(self, callback, nullptr, user_data);
	return true;
}

void DiscordAPILoader::client_create_authorization_code_verifier(Discord_Client *self,
		Discord_AuthorizationCodeVerifier *return_value) const {
	fn_client_create_authorization_code_verifier(self, return_value);
}

void DiscordAPILoader::authorization_args_init(Discord_AuthorizationArgs *self) const {
	fn_authorization_args_init(self);
}

void DiscordAPILoader::authorization_args_drop(Discord_AuthorizationArgs *self) const {
	fn_authorization_args_drop(self);
}

void DiscordAPILoader::authorization_args_set_client_id(Discord_AuthorizationArgs *self, uint64_t value) const {
	fn_authorization_args_set_client_id(self, value);
}

void DiscordAPILoader::authorization_args_set_scopes(Discord_AuthorizationArgs *self, const String &p_scopes) const {
	CharString utf8 = p_scopes.utf8();
	Discord_String scopes;
	scopes.ptr = (uint8_t *)utf8.ptr();
	scopes.size = utf8.length();
	fn_authorization_args_set_scopes(self, scopes);
}

void DiscordAPILoader::authorization_args_set_code_challenge(Discord_AuthorizationArgs *self,
		Discord_AuthorizationCodeChallenge *value) const {
	fn_authorization_args_set_code_challenge(self, value);
}

void DiscordAPILoader::authorization_args_set_integration_type(Discord_AuthorizationArgs *self,
		Discord_IntegrationType value) const {
	if (!fn_authorization_args_set_integration_type) {
		return;
	}
	fn_authorization_args_set_integration_type(self, &value);
}

void DiscordAPILoader::authorization_code_verifier_drop(Discord_AuthorizationCodeVerifier *self) const {
	fn_authorization_code_verifier_drop(self);
}

void DiscordAPILoader::authorization_code_verifier_challenge(Discord_AuthorizationCodeVerifier *self,
		Discord_AuthorizationCodeChallenge *return_value) const {
	fn_authorization_code_verifier_challenge(self, return_value);
}

String DiscordAPILoader::authorization_code_verifier_verifier(Discord_AuthorizationCodeVerifier *self) const {
	Discord_String verifier;
	fn_authorization_code_verifier_verifier(self, &verifier);
	return to_godot_string(verifier);
}

void DiscordAPILoader::client_authorize(Discord_Client *self,
		Discord_AuthorizationArgs *args,
		Discord_Client_AuthorizationCallback callback,
		void *user_data) const {
	fn_client_authorize(self, args, callback, nullptr, user_data);
}

void DiscordAPILoader::client_get_token(Discord_Client *self,
		uint64_t application_id,
		const String &p_code,
		const String &p_code_verifier,
		const String &p_redirect_uri,
		Discord_Client_TokenExchangeCallback callback,
		void *user_data) const {
	CharString code_utf8 = p_code.utf8();
	CharString verifier_utf8 = p_code_verifier.utf8();
	CharString redirect_utf8 = p_redirect_uri.utf8();
	Discord_String code;
	code.ptr = (uint8_t *)code_utf8.ptr();
	code.size = code_utf8.length();
	Discord_String verifier;
	verifier.ptr = (uint8_t *)verifier_utf8.ptr();
	verifier.size = verifier_utf8.length();
	Discord_String redirect_uri;
	redirect_uri.ptr = (uint8_t *)redirect_utf8.ptr();
	redirect_uri.size = redirect_utf8.length();
	fn_client_get_token(self, application_id, code, verifier, redirect_uri, callback, nullptr, user_data);
}

void DiscordAPILoader::client_get_token(Discord_Client *self,
		uint64_t application_id,
		Discord_String code,
		Discord_String code_verifier,
		Discord_String redirect_uri,
		Discord_Client_TokenExchangeCallback callback,
		void *user_data) const {
	fn_client_get_token(self, application_id, code, code_verifier, redirect_uri, callback, nullptr, user_data);
}

void DiscordAPILoader::client_update_token(Discord_Client *self,
		Discord_AuthorizationTokenType token_type,
		const String &p_token,
		Discord_Client_UpdateTokenCallback callback,
		void *user_data) const {
	CharString token_utf8 = p_token.utf8();
	Discord_String token;
	token.ptr = (uint8_t *)token_utf8.ptr();
	token.size = token_utf8.length();
	fn_client_update_token(self, token_type, token, callback, nullptr, user_data);
}

void DiscordAPILoader::client_update_token(Discord_Client *self,
		Discord_AuthorizationTokenType token_type,
		Discord_String token,
		Discord_Client_UpdateTokenCallback callback,
		void *user_data) const {
	fn_client_update_token(self, token_type, token, callback, nullptr, user_data);
}

void DiscordAPILoader::client_connect(Discord_Client *self) const {
	fn_client_connect(self);
}

Discord_Client_Status DiscordAPILoader::client_get_status(Discord_Client *self) const {
	return fn_client_get_status(self);
}

bool DiscordAPILoader::client_get_current_user_v2(Discord_Client *self, Discord_UserHandle *return_value) const {
	return fn_client_get_current_user_v2(self, return_value);
}

void DiscordAPILoader::client_result_drop(Discord_ClientResult *self) const {
	fn_client_result_drop(self);
}

bool DiscordAPILoader::client_result_successful(Discord_ClientResult *self) const {
	return fn_client_result_successful(self);
}

String DiscordAPILoader::client_result_error(Discord_ClientResult *self) const {
	Discord_String error;
	fn_client_result_error(self, &error);
	return to_godot_string(error);
}

void DiscordAPILoader::user_handle_drop(Discord_UserHandle *self) const {
	fn_user_handle_drop(self);
}

uint64_t DiscordAPILoader::user_handle_id(Discord_UserHandle *self) const {
	return fn_user_handle_id(self);
}

String DiscordAPILoader::user_handle_display_name(Discord_UserHandle *self) const {
	Discord_String name;
	fn_user_handle_display_name(self, &name);
	return to_godot_string(name);
}

String DiscordAPILoader::user_handle_username(Discord_UserHandle *self) const {
	Discord_String name;
	fn_user_handle_username(self, &name);
	return to_godot_string(name);
}

bool DiscordAPILoader::user_handle_game_activity(Discord_UserHandle *self, Discord_Activity *return_value) const {
	return fn_user_handle_game_activity(self, return_value);
}

Discord_ActivityTypes DiscordAPILoader::activity_get_type(Discord_Activity *self) const {
	return fn_activity_type(self);
}

String DiscordAPILoader::activity_get_details(Discord_Activity *self) const {
	Discord_String value;
	if (!fn_activity_details(self, &value)) {
		return String();
	}
	return to_godot_string(value);
}

String DiscordAPILoader::activity_get_state(Discord_Activity *self) const {
	Discord_String value;
	if (!fn_activity_state(self, &value)) {
		return String();
	}
	return to_godot_string(value);
}

String DiscordAPILoader::get_default_communication_scopes() const {
	Discord_String scopes;
	fn_client_get_default_communication_scopes(&scopes);
	return to_godot_string(scopes);
}

String DiscordAPILoader::get_default_presence_scopes() const {
	Discord_String scopes;
	fn_client_get_default_presence_scopes(&scopes);
	return to_godot_string(scopes);
}

void DiscordAPILoader::client_add_log_callback(Discord_Client *self,
		Discord_Client_LogCallback callback,
		void *user_data,
		Discord_LoggingSeverity min_severity) const {
	fn_client_add_log_callback(self, callback, nullptr, user_data, min_severity);
}

void DiscordAPILoader::client_get_relationships(Discord_Client *self, Discord_RelationshipHandleSpan *return_value) const {
	fn_client_get_relationships(self, return_value);
}

void DiscordAPILoader::activity_init(Discord_Activity *self) const {
	fn_activity_init(self);
}

void DiscordAPILoader::activity_drop(Discord_Activity *self) const {
	fn_activity_drop(self);
}

void DiscordAPILoader::activity_set_type(Discord_Activity *self, Discord_ActivityTypes value) const {
	fn_activity_set_type(self, value);
}

void DiscordAPILoader::activity_set_details(Discord_Activity *self, const String &p_value) const {
	CharString utf8 = p_value.utf8();
	Discord_String str;
	str.ptr = (uint8_t *)utf8.ptr();
	str.size = utf8.length();
	fn_activity_set_details(self, &str);
}

void DiscordAPILoader::activity_set_state(Discord_Activity *self, const String &p_value) const {
	CharString utf8 = p_value.utf8();
	Discord_String str;
	str.ptr = (uint8_t *)utf8.ptr();
	str.size = utf8.length();
	fn_activity_set_state(self, &str);
}

void DiscordAPILoader::activity_set_assets(Discord_Activity *self, Discord_ActivityAssets *value) const {
	fn_activity_set_assets(self, value);
}

void DiscordAPILoader::activity_set_timestamps(Discord_Activity *self, Discord_ActivityTimestamps *value) const {
	fn_activity_set_timestamps(self, value);
}

void DiscordAPILoader::activity_assets_init(Discord_ActivityAssets *self) const {
	fn_activity_assets_init(self);
}

void DiscordAPILoader::activity_assets_drop(Discord_ActivityAssets *self) const {
	fn_activity_assets_drop(self);
}

void DiscordAPILoader::activity_assets_set_large_image(Discord_ActivityAssets *self, const String &p_value) const {
	CharString utf8 = p_value.utf8();
	Discord_String str;
	str.ptr = (uint8_t *)utf8.ptr();
	str.size = utf8.length();
	fn_activity_assets_set_large_image(self, &str);
}

void DiscordAPILoader::activity_assets_set_large_text(Discord_ActivityAssets *self, const String &p_value) const {
	CharString utf8 = p_value.utf8();
	Discord_String str;
	str.ptr = (uint8_t *)utf8.ptr();
	str.size = utf8.length();
	fn_activity_assets_set_large_text(self, &str);
}

void DiscordAPILoader::activity_assets_set_small_image(Discord_ActivityAssets *self, const String &p_value) const {
	CharString utf8 = p_value.utf8();
	Discord_String str;
	str.ptr = (uint8_t *)utf8.ptr();
	str.size = utf8.length();
	fn_activity_assets_set_small_image(self, &str);
}

void DiscordAPILoader::activity_assets_set_small_text(Discord_ActivityAssets *self, const String &p_value) const {
	CharString utf8 = p_value.utf8();
	Discord_String str;
	str.ptr = (uint8_t *)utf8.ptr();
	str.size = utf8.length();
	fn_activity_assets_set_small_text(self, &str);
}

void DiscordAPILoader::activity_timestamps_init(Discord_ActivityTimestamps *self) const {
	fn_activity_timestamps_init(self);
}

void DiscordAPILoader::activity_timestamps_drop(Discord_ActivityTimestamps *self) const {
	fn_activity_timestamps_drop(self);
}

void DiscordAPILoader::activity_timestamps_set_start(Discord_ActivityTimestamps *self, uint64_t value) const {
	fn_activity_timestamps_set_start(self, value);
}

void DiscordAPILoader::activity_timestamps_set_end(Discord_ActivityTimestamps *self, uint64_t value) const {
	fn_activity_timestamps_set_end(self, value);
}

void DiscordAPILoader::client_update_rich_presence(Discord_Client *self,
		Discord_Activity *activity,
		Discord_Client_UpdateRichPresenceCallback callback,
		void *user_data) const {
	fn_client_update_rich_presence(self, activity, callback, nullptr, user_data);
}

void DiscordAPILoader::client_clear_rich_presence(Discord_Client *self) const {
	fn_client_clear_rich_presence(self);
}

void DiscordAPILoader::client_set_game_window_pid(Discord_Client *self, int32_t pid) const {
	if (fn_client_set_game_window_pid) {
		fn_client_set_game_window_pid(self, pid);
	}
}

bool DiscordAPILoader::client_set_log_dir(Discord_Client *self, const String &p_path, Discord_LoggingSeverity min_severity) const {
	if (!fn_client_set_log_dir) {
		return false;
	}
	CharString path_utf8 = p_path.utf8();
	Discord_String path;
	path.ptr = (uint8_t *)path_utf8.ptr();
	path.size = path_utf8.length();
	return fn_client_set_log_dir(self, path, min_severity);
}

void DiscordAPILoader::fill_version_dict(Dictionary &p_version) const {
	p_version["sdk_enabled"] = true;
	p_version["major"] = fn_client_get_version_major ? fn_client_get_version_major() : 0;
	p_version["minor"] = fn_client_get_version_minor ? fn_client_get_version_minor() : 0;
	p_version["patch"] = fn_client_get_version_patch ? fn_client_get_version_patch() : 0;
	if (fn_client_get_version_hash) {
		Discord_String hash;
		fn_client_get_version_hash(&hash);
		p_version["hash"] = to_godot_string(hash);
	} else {
		p_version["hash"] = String();
	}
}
