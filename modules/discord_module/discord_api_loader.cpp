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
	LOAD_REQUIRED("Discord_Client_GetDefaultCommunicationScopes", fn_client_get_default_communication_scopes);
	LOAD_REQUIRED("Discord_Client_GetVersionHash", fn_client_get_version_hash);
	LOAD_REQUIRED("Discord_Client_GetVersionMajor", fn_client_get_version_major);
	LOAD_REQUIRED("Discord_Client_GetVersionMinor", fn_client_get_version_minor);
	LOAD_REQUIRED("Discord_Client_GetVersionPatch", fn_client_get_version_patch);

#undef LOAD_REQUIRED

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
	fn_client_create_authorization_code_verifier = nullptr;
	fn_authorization_args_init = nullptr;
	fn_authorization_args_drop = nullptr;
	fn_authorization_args_set_client_id = nullptr;
	fn_authorization_args_set_scopes = nullptr;
	fn_authorization_args_set_code_challenge = nullptr;
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
	fn_client_get_default_communication_scopes = nullptr;
	fn_client_get_version_hash = nullptr;
	fn_client_get_version_major = nullptr;
	fn_client_get_version_minor = nullptr;
	fn_client_get_version_patch = nullptr;
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

String DiscordAPILoader::get_default_communication_scopes() const {
	Discord_String scopes;
	fn_client_get_default_communication_scopes(&scopes);
	return to_godot_string(scopes);
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
