/**************************************************************************/
/*  discord_api_loader.h                                                  */
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

#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "discord_types.h"

class DiscordAPILoader {
public:
	static String get_runtime_library_name();
	static bool is_runtime_library_present();

private:
	void *library_handle = nullptr;
	bool loaded = false;

	typedef void (*Discord_RunCallbacksFn)();
	typedef void (*Discord_Client_InitFn)(Discord_Client *self);
	typedef void (*Discord_Client_DropFn)(Discord_Client *self);
	typedef void (*Discord_Client_SetApplicationIdFn)(Discord_Client *self, uint64_t application_id);
	typedef void (*Discord_Client_SetStatusChangedCallbackFn)(Discord_Client *self,
			Discord_Client_OnStatusChanged callback,
			Discord_FreeFn callback_user_data_free,
			void *callback_user_data);
	typedef void (*Discord_Client_CreateAuthorizationCodeVerifierFn)(Discord_Client *self,
			Discord_AuthorizationCodeVerifier *return_value);
	typedef void (*Discord_AuthorizationArgs_InitFn)(Discord_AuthorizationArgs *self);
	typedef void (*Discord_AuthorizationArgs_DropFn)(Discord_AuthorizationArgs *self);
	typedef void (*Discord_AuthorizationArgs_SetClientIdFn)(Discord_AuthorizationArgs *self, uint64_t value);
	typedef void (*Discord_AuthorizationArgs_SetScopesFn)(Discord_AuthorizationArgs *self, Discord_String value);
	typedef void (*Discord_AuthorizationArgs_SetCodeChallengeFn)(Discord_AuthorizationArgs *self,
			Discord_AuthorizationCodeChallenge *value);
	typedef void (*Discord_AuthorizationCodeVerifier_DropFn)(Discord_AuthorizationCodeVerifier *self);
	typedef void (*Discord_AuthorizationCodeVerifier_ChallengeFn)(Discord_AuthorizationCodeVerifier *self,
			Discord_AuthorizationCodeChallenge *return_value);
	typedef void (*Discord_AuthorizationCodeVerifier_VerifierFn)(Discord_AuthorizationCodeVerifier *self,
			Discord_String *return_value);
	typedef void (*Discord_Client_AuthorizeFn)(Discord_Client *self,
			Discord_AuthorizationArgs *args,
			Discord_Client_AuthorizationCallback callback,
			Discord_FreeFn callback_user_data_free,
			void *callback_user_data);
	typedef void (*Discord_Client_GetTokenFn)(Discord_Client *self,
			uint64_t application_id,
			Discord_String code,
			Discord_String code_verifier,
			Discord_String redirect_uri,
			Discord_Client_TokenExchangeCallback callback,
			Discord_FreeFn callback_user_data_free,
			void *callback_user_data);
	typedef void (*Discord_Client_UpdateTokenFn)(Discord_Client *self,
			Discord_AuthorizationTokenType token_type,
			Discord_String token,
			Discord_Client_UpdateTokenCallback callback,
			Discord_FreeFn callback_user_data_free,
			void *callback_user_data);
	typedef void (*Discord_Client_ConnectFn)(Discord_Client *self);
	typedef Discord_Client_Status (*Discord_Client_GetStatusFn)(Discord_Client *self);
	typedef bool (*Discord_Client_GetCurrentUserV2Fn)(Discord_Client *self, Discord_UserHandle *return_value);
	typedef void (*Discord_ClientResult_DropFn)(Discord_ClientResult *self);
	typedef bool (*Discord_ClientResult_SuccessfulFn)(Discord_ClientResult *self);
	typedef void (*Discord_ClientResult_ErrorFn)(Discord_ClientResult *self, Discord_String *return_value);
	typedef void (*Discord_UserHandle_DropFn)(Discord_UserHandle *self);
	typedef uint64_t (*Discord_UserHandle_IdFn)(Discord_UserHandle *self);
	typedef void (*Discord_UserHandle_DisplayNameFn)(Discord_UserHandle *self, Discord_String *return_value);
	typedef void (*Discord_UserHandle_UsernameFn)(Discord_UserHandle *self, Discord_String *return_value);
	typedef void (*Discord_Client_GetDefaultCommunicationScopesFn)(Discord_String *return_value);
	typedef void (*Discord_Client_GetVersionHashFn)(Discord_String *return_value);
	typedef int32_t (*Discord_Client_GetVersionMajorFn)();
	typedef int32_t (*Discord_Client_GetVersionMinorFn)();
	typedef int32_t (*Discord_Client_GetVersionPatchFn)();

	Discord_RunCallbacksFn fn_run_callbacks = nullptr;
	Discord_Client_InitFn fn_client_init = nullptr;
	Discord_Client_DropFn fn_client_drop = nullptr;
	Discord_Client_SetApplicationIdFn fn_client_set_application_id = nullptr;
	Discord_Client_SetStatusChangedCallbackFn fn_client_set_status_changed_callback = nullptr;
	Discord_Client_CreateAuthorizationCodeVerifierFn fn_client_create_authorization_code_verifier = nullptr;
	Discord_AuthorizationArgs_InitFn fn_authorization_args_init = nullptr;
	Discord_AuthorizationArgs_DropFn fn_authorization_args_drop = nullptr;
	Discord_AuthorizationArgs_SetClientIdFn fn_authorization_args_set_client_id = nullptr;
	Discord_AuthorizationArgs_SetScopesFn fn_authorization_args_set_scopes = nullptr;
	Discord_AuthorizationArgs_SetCodeChallengeFn fn_authorization_args_set_code_challenge = nullptr;
	Discord_AuthorizationCodeVerifier_DropFn fn_authorization_code_verifier_drop = nullptr;
	Discord_AuthorizationCodeVerifier_ChallengeFn fn_authorization_code_verifier_challenge = nullptr;
	Discord_AuthorizationCodeVerifier_VerifierFn fn_authorization_code_verifier_verifier = nullptr;
	Discord_Client_AuthorizeFn fn_client_authorize = nullptr;
	Discord_Client_GetTokenFn fn_client_get_token = nullptr;
	Discord_Client_UpdateTokenFn fn_client_update_token = nullptr;
	Discord_Client_ConnectFn fn_client_connect = nullptr;
	Discord_Client_GetStatusFn fn_client_get_status = nullptr;
	Discord_Client_GetCurrentUserV2Fn fn_client_get_current_user_v2 = nullptr;
	Discord_ClientResult_DropFn fn_client_result_drop = nullptr;
	Discord_ClientResult_SuccessfulFn fn_client_result_successful = nullptr;
	Discord_ClientResult_ErrorFn fn_client_result_error = nullptr;
	Discord_UserHandle_DropFn fn_user_handle_drop = nullptr;
	Discord_UserHandle_IdFn fn_user_handle_id = nullptr;
	Discord_UserHandle_DisplayNameFn fn_user_handle_display_name = nullptr;
	Discord_UserHandle_UsernameFn fn_user_handle_username = nullptr;
	Discord_Client_GetDefaultCommunicationScopesFn fn_client_get_default_communication_scopes = nullptr;
	Discord_Client_GetVersionHashFn fn_client_get_version_hash = nullptr;
	Discord_Client_GetVersionMajorFn fn_client_get_version_major = nullptr;
	Discord_Client_GetVersionMinorFn fn_client_get_version_minor = nullptr;
	Discord_Client_GetVersionPatchFn fn_client_get_version_patch = nullptr;

	bool _load_symbol(const char *p_name, void *&r_symbol);

public:
	bool try_load();
	void unload();
	bool is_loaded() const { return loaded; }

	void run_callbacks() const;
	void client_init(Discord_Client *self) const;
	void client_drop(Discord_Client *self) const;
	void client_set_application_id(Discord_Client *self, uint64_t application_id) const;
	void client_set_status_changed_callback(Discord_Client *self,
			Discord_Client_OnStatusChanged callback,
			void *user_data) const;
	void client_create_authorization_code_verifier(Discord_Client *self,
			Discord_AuthorizationCodeVerifier *return_value) const;
	void authorization_args_init(Discord_AuthorizationArgs *self) const;
	void authorization_args_drop(Discord_AuthorizationArgs *self) const;
	void authorization_args_set_client_id(Discord_AuthorizationArgs *self, uint64_t value) const;
	void authorization_args_set_scopes(Discord_AuthorizationArgs *self, const String &p_scopes) const;
	void authorization_args_set_code_challenge(Discord_AuthorizationArgs *self,
			Discord_AuthorizationCodeChallenge *value) const;
	void authorization_code_verifier_drop(Discord_AuthorizationCodeVerifier *self) const;
	void authorization_code_verifier_challenge(Discord_AuthorizationCodeVerifier *self,
			Discord_AuthorizationCodeChallenge *return_value) const;
	String authorization_code_verifier_verifier(Discord_AuthorizationCodeVerifier *self) const;
	void client_authorize(Discord_Client *self,
			Discord_AuthorizationArgs *args,
			Discord_Client_AuthorizationCallback callback,
			void *user_data) const;
	void client_get_token(Discord_Client *self,
			uint64_t application_id,
			const String &p_code,
			const String &p_code_verifier,
			const String &p_redirect_uri,
			Discord_Client_TokenExchangeCallback callback,
			void *user_data) const;
	void client_update_token(Discord_Client *self,
			Discord_AuthorizationTokenType token_type,
			const String &p_token,
			Discord_Client_UpdateTokenCallback callback,
			void *user_data) const;
	void client_connect(Discord_Client *self) const;
	Discord_Client_Status client_get_status(Discord_Client *self) const;
	bool client_get_current_user_v2(Discord_Client *self, Discord_UserHandle *return_value) const;
	void client_result_drop(Discord_ClientResult *self) const;
	bool client_result_successful(Discord_ClientResult *self) const;
	String client_result_error(Discord_ClientResult *self) const;
	void user_handle_drop(Discord_UserHandle *self) const;
	uint64_t user_handle_id(Discord_UserHandle *self) const;
	String user_handle_display_name(Discord_UserHandle *self) const;
	String user_handle_username(Discord_UserHandle *self) const;
	String get_default_communication_scopes() const;
	void fill_version_dict(Dictionary &p_version) const;

	static Discord_String make_string(const String &p_value);
	static String to_godot_string(const Discord_String &p_value);
};
