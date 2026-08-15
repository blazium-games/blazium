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
	LOAD_OPTIONAL("Discord_Activity_SetName", fn_activity_set_name);
	LOAD_OPTIONAL("Discord_Activity_SetStatusDisplayType", fn_activity_set_status_display_type);
	LOAD_OPTIONAL("Discord_Activity_SetStateUrl", fn_activity_set_state_url);
	LOAD_OPTIONAL("Discord_Activity_SetDetailsUrl", fn_activity_set_details_url);
	LOAD_OPTIONAL("Discord_Activity_SetParty", fn_activity_set_party);
	LOAD_OPTIONAL("Discord_Activity_SetSecrets", fn_activity_set_secrets);
	LOAD_OPTIONAL("Discord_Activity_SetSupportedPlatforms", fn_activity_set_supported_platforms);
	LOAD_OPTIONAL("Discord_Activity_AddButton", fn_activity_add_button);
	LOAD_OPTIONAL("Discord_ActivityAssets_SetLargeUrl", fn_activity_assets_set_large_url);
	LOAD_OPTIONAL("Discord_ActivityAssets_SetSmallUrl", fn_activity_assets_set_small_url);
	LOAD_OPTIONAL("Discord_ActivityAssets_SetInviteCoverImage", fn_activity_assets_set_invite_cover_image);
	LOAD_OPTIONAL("Discord_ActivityParty_Init", fn_activity_party_init);
	LOAD_OPTIONAL("Discord_ActivityParty_Drop", fn_activity_party_drop);
	LOAD_OPTIONAL("Discord_ActivityParty_SetId", fn_activity_party_set_id);
	LOAD_OPTIONAL("Discord_ActivityParty_SetCurrentSize", fn_activity_party_set_current_size);
	LOAD_OPTIONAL("Discord_ActivityParty_SetMaxSize", fn_activity_party_set_max_size);
	LOAD_OPTIONAL("Discord_ActivitySecrets_Init", fn_activity_secrets_init);
	LOAD_OPTIONAL("Discord_ActivitySecrets_Drop", fn_activity_secrets_drop);
	LOAD_OPTIONAL("Discord_ActivitySecrets_SetJoin", fn_activity_secrets_set_join);
	LOAD_OPTIONAL("Discord_ActivityButton_Init", fn_activity_button_init);
	LOAD_OPTIONAL("Discord_ActivityButton_Drop", fn_activity_button_drop);
	LOAD_OPTIONAL("Discord_ActivityButton_SetLabel", fn_activity_button_set_label);
	LOAD_OPTIONAL("Discord_ActivityButton_SetUrl", fn_activity_button_set_url);
	LOAD_OPTIONAL("Discord_ActivityInvite_Init", fn_activity_invite_init);
	LOAD_OPTIONAL("Discord_ActivityInvite_Drop", fn_activity_invite_drop);
	LOAD_OPTIONAL("Discord_ActivityInvite_SetSenderId", fn_activity_invite_set_sender_id);
	LOAD_OPTIONAL("Discord_ActivityInvite_SetChannelId", fn_activity_invite_set_channel_id);
	LOAD_OPTIONAL("Discord_ActivityInvite_SetMessageId", fn_activity_invite_set_message_id);
	LOAD_OPTIONAL("Discord_ActivityInvite_SetType", fn_activity_invite_set_type);
	LOAD_OPTIONAL("Discord_ActivityInvite_SetApplicationId", fn_activity_invite_set_application_id);
	LOAD_OPTIONAL("Discord_ActivityInvite_SetParentApplicationId", fn_activity_invite_set_parent_application_id);
	LOAD_OPTIONAL("Discord_ActivityInvite_SetPartyId", fn_activity_invite_set_party_id);
	LOAD_OPTIONAL("Discord_ActivityInvite_SetSessionId", fn_activity_invite_set_session_id);
	LOAD_OPTIONAL("Discord_ActivityInvite_SetIsValid", fn_activity_invite_set_is_valid);
	LOAD_OPTIONAL("Discord_ActivityInvite_SenderId", fn_activity_invite_sender_id);
	LOAD_OPTIONAL("Discord_ActivityInvite_ChannelId", fn_activity_invite_channel_id);
	LOAD_OPTIONAL("Discord_ActivityInvite_MessageId", fn_activity_invite_message_id);
	LOAD_OPTIONAL("Discord_ActivityInvite_Type", fn_activity_invite_type);
	LOAD_OPTIONAL("Discord_ActivityInvite_ApplicationId", fn_activity_invite_application_id);
	LOAD_OPTIONAL("Discord_ActivityInvite_ParentApplicationId", fn_activity_invite_parent_application_id);
	LOAD_OPTIONAL("Discord_ActivityInvite_PartyId", fn_activity_invite_party_id);
	LOAD_OPTIONAL("Discord_ActivityInvite_SessionId", fn_activity_invite_session_id);
	LOAD_OPTIONAL("Discord_ActivityInvite_IsValid", fn_activity_invite_is_valid);
	LOAD_OPTIONAL("Discord_Client_RegisterLaunchCommand", fn_client_register_launch_command);
	LOAD_OPTIONAL("Discord_Client_RegisterLaunchSteamApplication", fn_client_register_launch_steam_application);
	LOAD_OPTIONAL("Discord_Client_SendActivityInvite", fn_client_send_activity_invite);
	LOAD_OPTIONAL("Discord_Client_SendActivityJoinRequest", fn_client_send_activity_join_request);
	LOAD_OPTIONAL("Discord_Client_SendActivityJoinRequestReply", fn_client_send_activity_join_request_reply);
	LOAD_OPTIONAL("Discord_Client_AcceptActivityInvite", fn_client_accept_activity_invite);
	LOAD_OPTIONAL("Discord_Client_CreateOrJoinLobby", fn_client_create_or_join_lobby);
	LOAD_OPTIONAL("Discord_Client_SetActivityInviteCreatedCallback", fn_client_set_activity_invite_created_callback);
	LOAD_OPTIONAL("Discord_Client_SetActivityInviteUpdatedCallback", fn_client_set_activity_invite_updated_callback);
	LOAD_OPTIONAL("Discord_Client_SetActivityJoinCallback", fn_client_set_activity_join_callback);
	LOAD_OPTIONAL("Discord_Client_SetLobbyCreatedCallback", fn_client_set_lobby_created_callback);
	LOAD_OPTIONAL("Discord_Client_GetRelationshipsByGroup", fn_client_get_relationships_by_group);
	LOAD_OPTIONAL("Discord_Client_GetRelationshipHandle", fn_client_get_relationship_handle);
	LOAD_OPTIONAL("Discord_Client_GetUser", fn_client_get_user);
	LOAD_OPTIONAL("Discord_RelationshipHandle_DiscordRelationshipType", fn_relationship_handle_discord_relationship_type);
	LOAD_OPTIONAL("Discord_RelationshipHandle_GameRelationshipType", fn_relationship_handle_game_relationship_type);
	LOAD_OPTIONAL("Discord_RelationshipHandle_Id", fn_relationship_handle_id);
	LOAD_OPTIONAL("Discord_RelationshipHandle_IsSpamRequest", fn_relationship_handle_is_spam_request);
	LOAD_OPTIONAL("Discord_RelationshipHandle_User", fn_relationship_handle_user);
	LOAD_OPTIONAL("Discord_RelationshipHandle_Drop", fn_relationship_handle_drop);
	LOAD_OPTIONAL("Discord_UserHandle_GlobalName", fn_user_handle_global_name);
	LOAD_OPTIONAL("Discord_UserHandle_Status", fn_user_handle_status);
	LOAD_OPTIONAL("Discord_UserHandle_IsProvisional", fn_user_handle_is_provisional);
	LOAD_OPTIONAL("Discord_UserHandle_AvatarUrl", fn_user_handle_avatar_url);
	LOAD_OPTIONAL("Discord_Client_AcceptDiscordFriendRequest", fn_client_accept_discord_friend_request);
	LOAD_OPTIONAL("Discord_Client_AcceptGameFriendRequest", fn_client_accept_game_friend_request);
	LOAD_OPTIONAL("Discord_Client_BlockUser", fn_client_block_user);
	LOAD_OPTIONAL("Discord_Client_CancelDiscordFriendRequest", fn_client_cancel_discord_friend_request);
	LOAD_OPTIONAL("Discord_Client_CancelGameFriendRequest", fn_client_cancel_game_friend_request);
	LOAD_OPTIONAL("Discord_Client_RejectDiscordFriendRequest", fn_client_reject_discord_friend_request);
	LOAD_OPTIONAL("Discord_Client_RejectGameFriendRequest", fn_client_reject_game_friend_request);
	LOAD_OPTIONAL("Discord_Client_RemoveDiscordAndGameFriend", fn_client_remove_discord_and_game_friend);
	LOAD_OPTIONAL("Discord_Client_RemoveGameFriend", fn_client_remove_game_friend);
	LOAD_OPTIONAL("Discord_Client_SendDiscordFriendRequest", fn_client_send_discord_friend_request);
	LOAD_OPTIONAL("Discord_Client_SendDiscordFriendRequestById", fn_client_send_discord_friend_request_by_id);
	LOAD_OPTIONAL("Discord_Client_SendGameFriendRequest", fn_client_send_game_friend_request);
	LOAD_OPTIONAL("Discord_Client_SendGameFriendRequestById", fn_client_send_game_friend_request_by_id);
	LOAD_OPTIONAL("Discord_Client_SetRelationshipCreatedCallback", fn_client_set_relationship_created_callback);
	LOAD_OPTIONAL("Discord_Client_SetRelationshipDeletedCallback", fn_client_set_relationship_deleted_callback);
	LOAD_OPTIONAL("Discord_Client_SetRelationshipGroupsUpdatedCallback", fn_client_set_relationship_groups_updated_callback);
	LOAD_OPTIONAL("Discord_Client_SetUserUpdatedCallback", fn_client_set_user_updated_callback);
	LOAD_OPTIONAL("Discord_Client_UnblockUser", fn_client_unblock_user);

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
	fn_activity_set_name = nullptr;
	fn_activity_set_status_display_type = nullptr;
	fn_activity_set_state_url = nullptr;
	fn_activity_set_details_url = nullptr;
	fn_activity_set_party = nullptr;
	fn_activity_set_secrets = nullptr;
	fn_activity_set_supported_platforms = nullptr;
	fn_activity_add_button = nullptr;
	fn_activity_assets_set_large_url = nullptr;
	fn_activity_assets_set_small_url = nullptr;
	fn_activity_assets_set_invite_cover_image = nullptr;
	fn_activity_party_init = nullptr;
	fn_activity_party_drop = nullptr;
	fn_activity_party_set_id = nullptr;
	fn_activity_party_set_current_size = nullptr;
	fn_activity_party_set_max_size = nullptr;
	fn_activity_secrets_init = nullptr;
	fn_activity_secrets_drop = nullptr;
	fn_activity_secrets_set_join = nullptr;
	fn_activity_button_init = nullptr;
	fn_activity_button_drop = nullptr;
	fn_activity_button_set_label = nullptr;
	fn_activity_button_set_url = nullptr;
	fn_activity_invite_init = nullptr;
	fn_activity_invite_drop = nullptr;
	fn_activity_invite_set_sender_id = nullptr;
	fn_activity_invite_set_channel_id = nullptr;
	fn_activity_invite_set_message_id = nullptr;
	fn_activity_invite_set_type = nullptr;
	fn_activity_invite_set_application_id = nullptr;
	fn_activity_invite_set_parent_application_id = nullptr;
	fn_activity_invite_set_party_id = nullptr;
	fn_activity_invite_set_session_id = nullptr;
	fn_activity_invite_set_is_valid = nullptr;
	fn_activity_invite_sender_id = nullptr;
	fn_activity_invite_channel_id = nullptr;
	fn_activity_invite_message_id = nullptr;
	fn_activity_invite_type = nullptr;
	fn_activity_invite_application_id = nullptr;
	fn_activity_invite_parent_application_id = nullptr;
	fn_activity_invite_party_id = nullptr;
	fn_activity_invite_session_id = nullptr;
	fn_activity_invite_is_valid = nullptr;
	fn_client_register_launch_command = nullptr;
	fn_client_register_launch_steam_application = nullptr;
	fn_client_send_activity_invite = nullptr;
	fn_client_send_activity_join_request = nullptr;
	fn_client_send_activity_join_request_reply = nullptr;
	fn_client_accept_activity_invite = nullptr;
	fn_client_create_or_join_lobby = nullptr;
	fn_client_set_activity_invite_created_callback = nullptr;
	fn_client_set_activity_invite_updated_callback = nullptr;
	fn_client_set_activity_join_callback = nullptr;
	fn_client_set_lobby_created_callback = nullptr;
	fn_client_get_relationships_by_group = nullptr;
	fn_client_get_relationship_handle = nullptr;
	fn_client_get_user = nullptr;
	fn_relationship_handle_discord_relationship_type = nullptr;
	fn_relationship_handle_game_relationship_type = nullptr;
	fn_relationship_handle_id = nullptr;
	fn_relationship_handle_is_spam_request = nullptr;
	fn_relationship_handle_user = nullptr;
	fn_relationship_handle_drop = nullptr;
	fn_user_handle_global_name = nullptr;
	fn_user_handle_status = nullptr;
	fn_user_handle_is_provisional = nullptr;
	fn_user_handle_avatar_url = nullptr;
	fn_client_accept_discord_friend_request = nullptr;
	fn_client_accept_game_friend_request = nullptr;
	fn_client_block_user = nullptr;
	fn_client_cancel_discord_friend_request = nullptr;
	fn_client_cancel_game_friend_request = nullptr;
	fn_client_reject_discord_friend_request = nullptr;
	fn_client_reject_game_friend_request = nullptr;
	fn_client_remove_discord_and_game_friend = nullptr;
	fn_client_remove_game_friend = nullptr;
	fn_client_send_discord_friend_request = nullptr;
	fn_client_send_discord_friend_request_by_id = nullptr;
	fn_client_send_game_friend_request = nullptr;
	fn_client_send_game_friend_request_by_id = nullptr;
	fn_client_set_relationship_created_callback = nullptr;
	fn_client_set_relationship_deleted_callback = nullptr;
	fn_client_set_relationship_groups_updated_callback = nullptr;
	fn_client_set_user_updated_callback = nullptr;
	fn_client_unblock_user = nullptr;
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

#define DISCORD_LOADER_STRING_PTR(setter, fn_member)            \
	void DiscordAPILoader::setter(Discord_ActivityAssets *self, \
			const String &p_value) const {                      \
		if (!fn_member) {                                       \
			return;                                             \
		}                                                       \
		CharString utf8 = p_value.utf8();                       \
		Discord_String str;                                     \
		str.ptr = (uint8_t *)utf8.ptr();                        \
		str.size = utf8.length();                               \
		fn_member(self, &str);                                  \
	}

DISCORD_LOADER_STRING_PTR(activity_assets_set_large_url, fn_activity_assets_set_large_url)
DISCORD_LOADER_STRING_PTR(activity_assets_set_small_url, fn_activity_assets_set_small_url)
DISCORD_LOADER_STRING_PTR(activity_assets_set_invite_cover_image, fn_activity_assets_set_invite_cover_image)

#undef DISCORD_LOADER_STRING_PTR

void DiscordAPILoader::activity_set_name(Discord_Activity *self, const String &p_value) const {
	if (!fn_activity_set_name) {
		return;
	}
	Discord_String str = make_string(p_value);
	fn_activity_set_name(self, str);
}

void DiscordAPILoader::activity_set_status_display_type(Discord_Activity *self, Discord_StatusDisplayTypes value) const {
	if (!fn_activity_set_status_display_type) {
		return;
	}
	fn_activity_set_status_display_type(self, &value);
}

void DiscordAPILoader::activity_set_state_url(Discord_Activity *self, const String &p_value) const {
	if (!fn_activity_set_state_url) {
		return;
	}
	CharString utf8 = p_value.utf8();
	Discord_String str;
	str.ptr = (uint8_t *)utf8.ptr();
	str.size = utf8.length();
	fn_activity_set_state_url(self, &str);
}

void DiscordAPILoader::activity_set_details_url(Discord_Activity *self, const String &p_value) const {
	if (!fn_activity_set_details_url) {
		return;
	}
	CharString utf8 = p_value.utf8();
	Discord_String str;
	str.ptr = (uint8_t *)utf8.ptr();
	str.size = utf8.length();
	fn_activity_set_details_url(self, &str);
}

void DiscordAPILoader::activity_set_party(Discord_Activity *self, Discord_ActivityParty *value) const {
	if (fn_activity_set_party) {
		fn_activity_set_party(self, value);
	}
}

void DiscordAPILoader::activity_set_secrets(Discord_Activity *self, Discord_ActivitySecrets *value) const {
	if (fn_activity_set_secrets) {
		fn_activity_set_secrets(self, value);
	}
}

void DiscordAPILoader::activity_set_supported_platforms(Discord_Activity *self, Discord_ActivityGamePlatforms value) const {
	if (fn_activity_set_supported_platforms) {
		fn_activity_set_supported_platforms(self, value);
	}
}

void DiscordAPILoader::activity_add_button(Discord_Activity *self, Discord_ActivityButton const *button) const {
	if (fn_activity_add_button) {
		fn_activity_add_button(self, button);
	}
}

void DiscordAPILoader::activity_party_init(Discord_ActivityParty *self) const {
	if (fn_activity_party_init) {
		fn_activity_party_init(self);
	}
}

void DiscordAPILoader::activity_party_drop(Discord_ActivityParty *self) const {
	if (fn_activity_party_drop) {
		fn_activity_party_drop(self);
	}
}

void DiscordAPILoader::activity_party_set_id(Discord_ActivityParty *self, const String &p_value) const {
	if (fn_activity_party_set_id) {
		fn_activity_party_set_id(self, make_string(p_value));
	}
}

void DiscordAPILoader::activity_party_set_current_size(Discord_ActivityParty *self, int32_t value) const {
	if (fn_activity_party_set_current_size) {
		fn_activity_party_set_current_size(self, value);
	}
}

void DiscordAPILoader::activity_party_set_max_size(Discord_ActivityParty *self, int32_t value) const {
	if (fn_activity_party_set_max_size) {
		fn_activity_party_set_max_size(self, value);
	}
}

void DiscordAPILoader::activity_secrets_init(Discord_ActivitySecrets *self) const {
	if (fn_activity_secrets_init) {
		fn_activity_secrets_init(self);
	}
}

void DiscordAPILoader::activity_secrets_drop(Discord_ActivitySecrets *self) const {
	if (fn_activity_secrets_drop) {
		fn_activity_secrets_drop(self);
	}
}

void DiscordAPILoader::activity_secrets_set_join(Discord_ActivitySecrets *self, const String &p_value) const {
	if (fn_activity_secrets_set_join) {
		fn_activity_secrets_set_join(self, make_string(p_value));
	}
}

void DiscordAPILoader::activity_button_init(Discord_ActivityButton *self) const {
	if (fn_activity_button_init) {
		fn_activity_button_init(self);
	}
}

void DiscordAPILoader::activity_button_drop(Discord_ActivityButton *self) const {
	if (fn_activity_button_drop) {
		fn_activity_button_drop(self);
	}
}

void DiscordAPILoader::activity_button_set_label(Discord_ActivityButton *self, const String &p_value) const {
	if (fn_activity_button_set_label) {
		fn_activity_button_set_label(self, make_string(p_value));
	}
}

void DiscordAPILoader::activity_button_set_url(Discord_ActivityButton *self, const String &p_value) const {
	if (fn_activity_button_set_url) {
		fn_activity_button_set_url(self, make_string(p_value));
	}
}

void DiscordAPILoader::activity_invite_init(Discord_ActivityInvite *self) const {
	if (fn_activity_invite_init) {
		fn_activity_invite_init(self);
	}
}

void DiscordAPILoader::activity_invite_drop(Discord_ActivityInvite *self) const {
	if (fn_activity_invite_drop) {
		fn_activity_invite_drop(self);
	}
}

void DiscordAPILoader::activity_invite_set_sender_id(Discord_ActivityInvite *self, uint64_t value) const {
	if (fn_activity_invite_set_sender_id) {
		fn_activity_invite_set_sender_id(self, value);
	}
}

void DiscordAPILoader::activity_invite_set_channel_id(Discord_ActivityInvite *self, uint64_t value) const {
	if (fn_activity_invite_set_channel_id) {
		fn_activity_invite_set_channel_id(self, value);
	}
}

void DiscordAPILoader::activity_invite_set_message_id(Discord_ActivityInvite *self, uint64_t value) const {
	if (fn_activity_invite_set_message_id) {
		fn_activity_invite_set_message_id(self, value);
	}
}

void DiscordAPILoader::activity_invite_set_type(Discord_ActivityInvite *self, Discord_ActivityActionTypes value) const {
	if (fn_activity_invite_set_type) {
		fn_activity_invite_set_type(self, value);
	}
}

void DiscordAPILoader::activity_invite_set_application_id(Discord_ActivityInvite *self, uint64_t value) const {
	if (fn_activity_invite_set_application_id) {
		fn_activity_invite_set_application_id(self, value);
	}
}

void DiscordAPILoader::activity_invite_set_parent_application_id(Discord_ActivityInvite *self, uint64_t value) const {
	if (fn_activity_invite_set_parent_application_id) {
		fn_activity_invite_set_parent_application_id(self, value);
	}
}

void DiscordAPILoader::activity_invite_set_party_id(Discord_ActivityInvite *self, const String &p_value) const {
	if (fn_activity_invite_set_party_id) {
		fn_activity_invite_set_party_id(self, make_string(p_value));
	}
}

void DiscordAPILoader::activity_invite_set_session_id(Discord_ActivityInvite *self, const String &p_value) const {
	if (fn_activity_invite_set_session_id) {
		fn_activity_invite_set_session_id(self, make_string(p_value));
	}
}

void DiscordAPILoader::activity_invite_set_is_valid(Discord_ActivityInvite *self, bool value) const {
	if (fn_activity_invite_set_is_valid) {
		fn_activity_invite_set_is_valid(self, value);
	}
}

Dictionary DiscordAPILoader::activity_invite_to_dict(Discord_ActivityInvite *self) const {
	Dictionary out;
	if (!self) {
		return out;
	}
	if (fn_activity_invite_sender_id) {
		out["sender_id"] = (int64_t)fn_activity_invite_sender_id(self);
	}
	if (fn_activity_invite_channel_id) {
		out["channel_id"] = (int64_t)fn_activity_invite_channel_id(self);
	}
	if (fn_activity_invite_message_id) {
		out["message_id"] = (int64_t)fn_activity_invite_message_id(self);
	}
	if (fn_activity_invite_type) {
		out["type"] = (int)fn_activity_invite_type(self);
	}
	if (fn_activity_invite_application_id) {
		out["application_id"] = (int64_t)fn_activity_invite_application_id(self);
	}
	if (fn_activity_invite_parent_application_id) {
		out["parent_application_id"] = (int64_t)fn_activity_invite_parent_application_id(self);
	}
	if (fn_activity_invite_party_id) {
		Discord_String party_id;
		fn_activity_invite_party_id(self, &party_id);
		out["party_id"] = to_godot_string(party_id);
	}
	if (fn_activity_invite_session_id) {
		Discord_String session_id;
		fn_activity_invite_session_id(self, &session_id);
		out["session_id"] = to_godot_string(session_id);
	}
	if (fn_activity_invite_is_valid) {
		out["is_valid"] = fn_activity_invite_is_valid(self);
	}
	return out;
}

bool DiscordAPILoader::client_register_launch_command(Discord_Client *self, uint64_t application_id, const String &p_command) const {
	if (!fn_client_register_launch_command) {
		return false;
	}
	return fn_client_register_launch_command(self, application_id, make_string(p_command));
}

bool DiscordAPILoader::client_register_launch_steam_application(Discord_Client *self, uint64_t application_id, uint32_t steam_app_id) const {
	if (!fn_client_register_launch_steam_application) {
		return false;
	}
	return fn_client_register_launch_steam_application(self, application_id, steam_app_id);
}

void DiscordAPILoader::client_send_activity_invite(Discord_Client *self,
		uint64_t user_id,
		const String &p_content,
		Discord_Client_SendActivityInviteCallback callback,
		void *user_data) const {
	if (fn_client_send_activity_invite) {
		fn_client_send_activity_invite(self, user_id, make_string(p_content), callback, nullptr, user_data);
	}
}

void DiscordAPILoader::client_send_activity_join_request(Discord_Client *self,
		uint64_t user_id,
		Discord_Client_SendActivityInviteCallback callback,
		void *user_data) const {
	if (fn_client_send_activity_join_request) {
		fn_client_send_activity_join_request(self, user_id, callback, nullptr, user_data);
	}
}

void DiscordAPILoader::client_send_activity_join_request_reply(Discord_Client *self,
		Discord_ActivityInvite *invite,
		Discord_Client_SendActivityInviteCallback callback,
		void *user_data) const {
	if (fn_client_send_activity_join_request_reply) {
		fn_client_send_activity_join_request_reply(self, invite, callback, nullptr, user_data);
	}
}

void DiscordAPILoader::client_accept_activity_invite(Discord_Client *self,
		Discord_ActivityInvite *invite,
		Discord_Client_AcceptActivityInviteCallback callback,
		void *user_data) const {
	if (fn_client_accept_activity_invite) {
		fn_client_accept_activity_invite(self, invite, callback, nullptr, user_data);
	}
}

void DiscordAPILoader::client_create_or_join_lobby(Discord_Client *self,
		const String &p_secret,
		Discord_Client_CreateOrJoinLobbyCallback callback,
		void *user_data) const {
	if (fn_client_create_or_join_lobby) {
		fn_client_create_or_join_lobby(self, make_string(p_secret), callback, nullptr, user_data);
	}
}

void DiscordAPILoader::client_set_activity_invite_created_callback(Discord_Client *self,
		Discord_Client_ActivityInviteCallback callback,
		void *user_data) const {
	if (fn_client_set_activity_invite_created_callback) {
		fn_client_set_activity_invite_created_callback(self, callback, nullptr, user_data);
	}
}

void DiscordAPILoader::client_set_activity_invite_updated_callback(Discord_Client *self,
		Discord_Client_ActivityInviteCallback callback,
		void *user_data) const {
	if (fn_client_set_activity_invite_updated_callback) {
		fn_client_set_activity_invite_updated_callback(self, callback, nullptr, user_data);
	}
}

void DiscordAPILoader::client_set_activity_join_callback(Discord_Client *self,
		Discord_Client_ActivityJoinCallback callback,
		void *user_data) const {
	if (fn_client_set_activity_join_callback) {
		fn_client_set_activity_join_callback(self, callback, nullptr, user_data);
	}
}

void DiscordAPILoader::client_set_lobby_created_callback(Discord_Client *self,
		Discord_Client_LobbyCreatedCallback callback,
		void *user_data) const {
	if (fn_client_set_lobby_created_callback) {
		fn_client_set_lobby_created_callback(self, callback, nullptr, user_data);
	}
}

void DiscordAPILoader::client_get_relationships_by_group(Discord_Client *self,
		Discord_RelationshipGroupType group_type,
		Discord_RelationshipHandleSpan *return_value) const {
	if (fn_client_get_relationships_by_group) {
		fn_client_get_relationships_by_group(self, group_type, return_value);
	}
}

void DiscordAPILoader::client_get_relationship_handle(Discord_Client *self,
		uint64_t user_id,
		Discord_RelationshipHandle *return_value) const {
	if (fn_client_get_relationship_handle) {
		fn_client_get_relationship_handle(self, user_id, return_value);
	}
}

bool DiscordAPILoader::client_get_user(Discord_Client *self, uint64_t user_id, Discord_UserHandle *return_value) const {
	if (!fn_client_get_user) {
		return false;
	}
	return fn_client_get_user(self, user_id, return_value);
}

void DiscordAPILoader::relationship_handle_drop(Discord_RelationshipHandle *self) const {
	if (fn_relationship_handle_drop) {
		fn_relationship_handle_drop(self);
	}
}

Discord_RelationshipType DiscordAPILoader::relationship_handle_discord_relationship_type(Discord_RelationshipHandle *self) const {
	if (!fn_relationship_handle_discord_relationship_type) {
		return DISCORD_RELATIONSHIP_TYPE_NONE;
	}
	return fn_relationship_handle_discord_relationship_type(self);
}

Discord_RelationshipType DiscordAPILoader::relationship_handle_game_relationship_type(Discord_RelationshipHandle *self) const {
	if (!fn_relationship_handle_game_relationship_type) {
		return DISCORD_RELATIONSHIP_TYPE_NONE;
	}
	return fn_relationship_handle_game_relationship_type(self);
}

uint64_t DiscordAPILoader::relationship_handle_id(Discord_RelationshipHandle *self) const {
	if (!fn_relationship_handle_id) {
		return 0;
	}
	return fn_relationship_handle_id(self);
}

bool DiscordAPILoader::relationship_handle_is_spam_request(Discord_RelationshipHandle *self) const {
	if (!fn_relationship_handle_is_spam_request) {
		return false;
	}
	return fn_relationship_handle_is_spam_request(self);
}

bool DiscordAPILoader::relationship_handle_user(Discord_RelationshipHandle *self, Discord_UserHandle *return_value) const {
	if (!fn_relationship_handle_user) {
		return false;
	}
	return fn_relationship_handle_user(self, return_value);
}

String DiscordAPILoader::user_handle_global_name(Discord_UserHandle *self) const {
	if (!fn_user_handle_global_name) {
		return String();
	}
	Discord_String name;
	if (!fn_user_handle_global_name(self, &name)) {
		return String();
	}
	return to_godot_string(name);
}

Discord_StatusType DiscordAPILoader::user_handle_status(Discord_UserHandle *self) const {
	if (!fn_user_handle_status) {
		return DISCORD_STATUS_TYPE_UNKNOWN;
	}
	return fn_user_handle_status(self);
}

bool DiscordAPILoader::user_handle_is_provisional(Discord_UserHandle *self) const {
	if (!fn_user_handle_is_provisional) {
		return false;
	}
	return fn_user_handle_is_provisional(self);
}

String DiscordAPILoader::user_handle_avatar_url(Discord_UserHandle *self) const {
	if (!fn_user_handle_avatar_url) {
		return String();
	}
	Discord_String url;
	fn_user_handle_avatar_url(self,
			DISCORD_USER_HANDLE_AVATAR_TYPE_GIF,
			DISCORD_USER_HANDLE_AVATAR_TYPE_PNG,
			&url);
	return to_godot_string(url);
}

#define DISCORD_LOADER_RELATIONSHIP_ACTION(name, fn_member)         \
	void DiscordAPILoader::name(Discord_Client *self,               \
			uint64_t user_id,                                       \
			Discord_Client_UpdateRelationshipCallback callback,     \
			void *user_data) const {                                \
		if (fn_member) {                                            \
			fn_member(self, user_id, callback, nullptr, user_data); \
		}                                                           \
	}

DISCORD_LOADER_RELATIONSHIP_ACTION(client_accept_discord_friend_request, fn_client_accept_discord_friend_request)
DISCORD_LOADER_RELATIONSHIP_ACTION(client_accept_game_friend_request, fn_client_accept_game_friend_request)
DISCORD_LOADER_RELATIONSHIP_ACTION(client_block_user, fn_client_block_user)
DISCORD_LOADER_RELATIONSHIP_ACTION(client_cancel_discord_friend_request, fn_client_cancel_discord_friend_request)
DISCORD_LOADER_RELATIONSHIP_ACTION(client_cancel_game_friend_request, fn_client_cancel_game_friend_request)
DISCORD_LOADER_RELATIONSHIP_ACTION(client_reject_discord_friend_request, fn_client_reject_discord_friend_request)
DISCORD_LOADER_RELATIONSHIP_ACTION(client_reject_game_friend_request, fn_client_reject_game_friend_request)
DISCORD_LOADER_RELATIONSHIP_ACTION(client_remove_discord_and_game_friend, fn_client_remove_discord_and_game_friend)
DISCORD_LOADER_RELATIONSHIP_ACTION(client_remove_game_friend, fn_client_remove_game_friend)
DISCORD_LOADER_RELATIONSHIP_ACTION(client_send_discord_friend_request_by_id, fn_client_send_discord_friend_request_by_id)
DISCORD_LOADER_RELATIONSHIP_ACTION(client_send_game_friend_request_by_id, fn_client_send_game_friend_request_by_id)
DISCORD_LOADER_RELATIONSHIP_ACTION(client_unblock_user, fn_client_unblock_user)

#undef DISCORD_LOADER_RELATIONSHIP_ACTION

void DiscordAPILoader::client_send_discord_friend_request(Discord_Client *self,
		const String &p_username,
		Discord_Client_SendFriendRequestCallback callback,
		void *user_data) const {
	if (fn_client_send_discord_friend_request) {
		fn_client_send_discord_friend_request(self, make_string(p_username), callback, nullptr, user_data);
	}
}

void DiscordAPILoader::client_send_game_friend_request(Discord_Client *self,
		const String &p_username,
		Discord_Client_SendFriendRequestCallback callback,
		void *user_data) const {
	if (fn_client_send_game_friend_request) {
		fn_client_send_game_friend_request(self, make_string(p_username), callback, nullptr, user_data);
	}
}

void DiscordAPILoader::client_set_relationship_created_callback(Discord_Client *self,
		Discord_Client_RelationshipCreatedCallback callback,
		void *user_data) const {
	if (fn_client_set_relationship_created_callback) {
		fn_client_set_relationship_created_callback(self, callback, nullptr, user_data);
	}
}

void DiscordAPILoader::client_set_relationship_deleted_callback(Discord_Client *self,
		Discord_Client_RelationshipDeletedCallback callback,
		void *user_data) const {
	if (fn_client_set_relationship_deleted_callback) {
		fn_client_set_relationship_deleted_callback(self, callback, nullptr, user_data);
	}
}

void DiscordAPILoader::client_set_relationship_groups_updated_callback(Discord_Client *self,
		Discord_Client_RelationshipGroupsUpdatedCallback callback,
		void *user_data) const {
	if (fn_client_set_relationship_groups_updated_callback) {
		fn_client_set_relationship_groups_updated_callback(self, callback, nullptr, user_data);
	}
}

void DiscordAPILoader::client_set_user_updated_callback(Discord_Client *self,
		Discord_Client_UserUpdatedCallback callback,
		void *user_data) const {
	if (fn_client_set_user_updated_callback) {
		fn_client_set_user_updated_callback(self, callback, nullptr, user_data);
	}
}
