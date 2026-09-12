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
	typedef void (*Discord_Client_SetAuthorizeDeviceScreenClosedCallbackFn)(Discord_Client *self,
			Discord_Client_AuthorizeDeviceScreenClosedCallback callback,
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
	typedef void (*Discord_AuthorizationArgs_SetIntegrationTypeFn)(Discord_AuthorizationArgs *self,
			Discord_IntegrationType *value);
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
	typedef bool (*Discord_UserHandle_GameActivityFn)(Discord_UserHandle *self, Discord_Activity *return_value);
	typedef Discord_ActivityTypes (*Discord_Activity_TypeFn)(Discord_Activity *self);
	typedef bool (*Discord_Activity_DetailsFn)(Discord_Activity *self, Discord_String *return_value);
	typedef bool (*Discord_Activity_StateFn)(Discord_Activity *self, Discord_String *return_value);
	typedef void (*Discord_Client_GetDefaultCommunicationScopesFn)(Discord_String *return_value);
	typedef void (*Discord_Client_GetDefaultPresenceScopesFn)(Discord_String *return_value);
	typedef void (*Discord_Client_AddLogCallbackFn)(Discord_Client *self,
			Discord_Client_LogCallback callback,
			Discord_FreeFn callback_user_data_free,
			void *callback_user_data,
			Discord_LoggingSeverity min_severity);
	typedef void (*Discord_Client_GetRelationshipsFn)(Discord_Client *self,
			Discord_RelationshipHandleSpan *return_value);
	typedef void (*Discord_Activity_InitFn)(Discord_Activity *self);
	typedef void (*Discord_Activity_DropFn)(Discord_Activity *self);
	typedef void (*Discord_Activity_SetTypeFn)(Discord_Activity *self, Discord_ActivityTypes value);
	typedef void (*Discord_Activity_SetDetailsFn)(Discord_Activity *self, Discord_String *value);
	typedef void (*Discord_Activity_SetStateFn)(Discord_Activity *self, Discord_String *value);
	typedef void (*Discord_Activity_SetAssetsFn)(Discord_Activity *self, Discord_ActivityAssets *value);
	typedef void (*Discord_Activity_SetTimestampsFn)(Discord_Activity *self, Discord_ActivityTimestamps *value);
	typedef void (*Discord_ActivityAssets_InitFn)(Discord_ActivityAssets *self);
	typedef void (*Discord_ActivityAssets_DropFn)(Discord_ActivityAssets *self);
	typedef void (*Discord_ActivityAssets_SetLargeImageFn)(Discord_ActivityAssets *self, Discord_String *value);
	typedef void (*Discord_ActivityAssets_SetLargeTextFn)(Discord_ActivityAssets *self, Discord_String *value);
	typedef void (*Discord_ActivityAssets_SetSmallImageFn)(Discord_ActivityAssets *self, Discord_String *value);
	typedef void (*Discord_ActivityAssets_SetSmallTextFn)(Discord_ActivityAssets *self, Discord_String *value);
	typedef void (*Discord_ActivityTimestamps_InitFn)(Discord_ActivityTimestamps *self);
	typedef void (*Discord_ActivityTimestamps_DropFn)(Discord_ActivityTimestamps *self);
	typedef void (*Discord_ActivityTimestamps_SetStartFn)(Discord_ActivityTimestamps *self, uint64_t value);
	typedef void (*Discord_ActivityTimestamps_SetEndFn)(Discord_ActivityTimestamps *self, uint64_t value);
	typedef void (*Discord_Client_UpdateRichPresenceFn)(Discord_Client *self,
			Discord_Activity *activity,
			Discord_Client_UpdateRichPresenceCallback callback,
			Discord_FreeFn callback_user_data_free,
			void *callback_user_data);
	typedef void (*Discord_Client_ClearRichPresenceFn)(Discord_Client *self);
	typedef void (*Discord_Client_GetVersionHashFn)(Discord_String *return_value);
	typedef int32_t (*Discord_Client_GetVersionMajorFn)();
	typedef int32_t (*Discord_Client_GetVersionMinorFn)();
	typedef int32_t (*Discord_Client_GetVersionPatchFn)();
	typedef void (*Discord_Client_SetGameWindowPidFn)(Discord_Client *self, int32_t pid);
	typedef bool (*Discord_Client_SetLogDirFn)(Discord_Client *self, Discord_String path, Discord_LoggingSeverity min_severity);
	typedef void (*Discord_Activity_SetNameFn)(Discord_Activity *self, Discord_String value);
	typedef void (*Discord_Activity_SetStatusDisplayTypeFn)(Discord_Activity *self, Discord_StatusDisplayTypes *value);
	typedef void (*Discord_Activity_SetStateUrlFn)(Discord_Activity *self, Discord_String *value);
	typedef void (*Discord_Activity_SetDetailsUrlFn)(Discord_Activity *self, Discord_String *value);
	typedef void (*Discord_Activity_SetPartyFn)(Discord_Activity *self, Discord_ActivityParty *value);
	typedef void (*Discord_Activity_SetSecretsFn)(Discord_Activity *self, Discord_ActivitySecrets *value);
	typedef void (*Discord_Activity_SetSupportedPlatformsFn)(Discord_Activity *self, Discord_ActivityGamePlatforms value);
	typedef void (*Discord_Activity_AddButtonFn)(Discord_Activity *self, Discord_ActivityButton const *button);
	typedef void (*Discord_ActivityAssets_SetLargeUrlFn)(Discord_ActivityAssets *self, Discord_String *value);
	typedef void (*Discord_ActivityAssets_SetSmallUrlFn)(Discord_ActivityAssets *self, Discord_String *value);
	typedef void (*Discord_ActivityAssets_SetInviteCoverImageFn)(Discord_ActivityAssets *self, Discord_String *value);
	typedef void (*Discord_ActivityParty_InitFn)(Discord_ActivityParty *self);
	typedef void (*Discord_ActivityParty_DropFn)(Discord_ActivityParty *self);
	typedef void (*Discord_ActivityParty_SetIdFn)(Discord_ActivityParty *self, Discord_String value);
	typedef void (*Discord_ActivityParty_SetCurrentSizeFn)(Discord_ActivityParty *self, int32_t value);
	typedef void (*Discord_ActivityParty_SetMaxSizeFn)(Discord_ActivityParty *self, int32_t value);
	typedef void (*Discord_ActivitySecrets_InitFn)(Discord_ActivitySecrets *self);
	typedef void (*Discord_ActivitySecrets_DropFn)(Discord_ActivitySecrets *self);
	typedef void (*Discord_ActivitySecrets_SetJoinFn)(Discord_ActivitySecrets *self, Discord_String value);
	typedef void (*Discord_ActivityButton_InitFn)(Discord_ActivityButton *self);
	typedef void (*Discord_ActivityButton_DropFn)(Discord_ActivityButton *self);
	typedef void (*Discord_ActivityButton_SetLabelFn)(Discord_ActivityButton *self, Discord_String value);
	typedef void (*Discord_ActivityButton_SetUrlFn)(Discord_ActivityButton *self, Discord_String value);
	typedef void (*Discord_ActivityInvite_InitFn)(Discord_ActivityInvite *self);
	typedef void (*Discord_ActivityInvite_DropFn)(Discord_ActivityInvite *self);
	typedef void (*Discord_ActivityInvite_SetSenderIdFn)(Discord_ActivityInvite *self, uint64_t value);
	typedef void (*Discord_ActivityInvite_SetChannelIdFn)(Discord_ActivityInvite *self, uint64_t value);
	typedef void (*Discord_ActivityInvite_SetMessageIdFn)(Discord_ActivityInvite *self, uint64_t value);
	typedef void (*Discord_ActivityInvite_SetTypeFn)(Discord_ActivityInvite *self, Discord_ActivityActionTypes value);
	typedef void (*Discord_ActivityInvite_SetApplicationIdFn)(Discord_ActivityInvite *self, uint64_t value);
	typedef void (*Discord_ActivityInvite_SetParentApplicationIdFn)(Discord_ActivityInvite *self, uint64_t value);
	typedef void (*Discord_ActivityInvite_SetPartyIdFn)(Discord_ActivityInvite *self, Discord_String value);
	typedef void (*Discord_ActivityInvite_SetSessionIdFn)(Discord_ActivityInvite *self, Discord_String value);
	typedef void (*Discord_ActivityInvite_SetIsValidFn)(Discord_ActivityInvite *self, bool value);
	typedef uint64_t (*Discord_ActivityInvite_SenderIdFn)(Discord_ActivityInvite *self);
	typedef uint64_t (*Discord_ActivityInvite_ChannelIdFn)(Discord_ActivityInvite *self);
	typedef uint64_t (*Discord_ActivityInvite_MessageIdFn)(Discord_ActivityInvite *self);
	typedef Discord_ActivityActionTypes (*Discord_ActivityInvite_TypeFn)(Discord_ActivityInvite *self);
	typedef uint64_t (*Discord_ActivityInvite_ApplicationIdFn)(Discord_ActivityInvite *self);
	typedef uint64_t (*Discord_ActivityInvite_ParentApplicationIdFn)(Discord_ActivityInvite *self);
	typedef void (*Discord_ActivityInvite_PartyIdFn)(Discord_ActivityInvite *self, Discord_String *return_value);
	typedef void (*Discord_ActivityInvite_SessionIdFn)(Discord_ActivityInvite *self, Discord_String *return_value);
	typedef bool (*Discord_ActivityInvite_IsValidFn)(Discord_ActivityInvite *self);
	typedef bool (*Discord_Client_RegisterLaunchCommandFn)(Discord_Client *self, uint64_t application_id, Discord_String command);
	typedef bool (*Discord_Client_RegisterLaunchSteamApplicationFn)(Discord_Client *self, uint64_t application_id, uint32_t steam_app_id);
	typedef void (*Discord_Client_SendActivityInviteFn)(Discord_Client *self,
			uint64_t user_id,
			Discord_String content,
			Discord_Client_SendActivityInviteCallback callback,
			Discord_FreeFn callback_user_data_free,
			void *callback_user_data);
	typedef void (*Discord_Client_SendActivityJoinRequestFn)(Discord_Client *self,
			uint64_t user_id,
			Discord_Client_SendActivityInviteCallback callback,
			Discord_FreeFn callback_user_data_free,
			void *callback_user_data);
	typedef void (*Discord_Client_SendActivityJoinRequestReplyFn)(Discord_Client *self,
			Discord_ActivityInvite *invite,
			Discord_Client_SendActivityInviteCallback callback,
			Discord_FreeFn callback_user_data_free,
			void *callback_user_data);
	typedef void (*Discord_Client_AcceptActivityInviteFn)(Discord_Client *self,
			Discord_ActivityInvite *invite,
			Discord_Client_AcceptActivityInviteCallback callback,
			Discord_FreeFn callback_user_data_free,
			void *callback_user_data);
	typedef void (*Discord_Client_CreateOrJoinLobbyFn)(Discord_Client *self,
			Discord_String secret,
			Discord_Client_CreateOrJoinLobbyCallback callback,
			Discord_FreeFn callback_user_data_free,
			void *callback_user_data);
	typedef void (*Discord_Client_SetActivityInviteCreatedCallbackFn)(Discord_Client *self,
			Discord_Client_ActivityInviteCallback cb,
			Discord_FreeFn cb_user_data_free,
			void *cb_user_data);
	typedef void (*Discord_Client_SetActivityInviteUpdatedCallbackFn)(Discord_Client *self,
			Discord_Client_ActivityInviteCallback cb,
			Discord_FreeFn cb_user_data_free,
			void *cb_user_data);
	typedef void (*Discord_Client_SetActivityJoinCallbackFn)(Discord_Client *self,
			Discord_Client_ActivityJoinCallback cb,
			Discord_FreeFn cb_user_data_free,
			void *cb_user_data);
	typedef void (*Discord_Client_SetLobbyCreatedCallbackFn)(Discord_Client *self,
			Discord_Client_LobbyCreatedCallback cb,
			Discord_FreeFn cb_user_data_free,
			void *cb_user_data);
	typedef void (*Discord_Client_GetRelationshipsByGroupFn)(Discord_Client *self,
			Discord_RelationshipGroupType group_type,
			Discord_RelationshipHandleSpan *return_value);
	typedef void (*Discord_Client_GetRelationshipHandleFn)(Discord_Client *self,
			uint64_t user_id,
			Discord_RelationshipHandle *return_value);
	typedef bool (*Discord_Client_GetUserFn)(Discord_Client *self, uint64_t user_id, Discord_UserHandle *return_value);
	typedef Discord_RelationshipType (*Discord_RelationshipHandle_DiscordRelationshipTypeFn)(Discord_RelationshipHandle *self);
	typedef Discord_RelationshipType (*Discord_RelationshipHandle_GameRelationshipTypeFn)(Discord_RelationshipHandle *self);
	typedef uint64_t (*Discord_RelationshipHandle_IdFn)(Discord_RelationshipHandle *self);
	typedef bool (*Discord_RelationshipHandle_IsSpamRequestFn)(Discord_RelationshipHandle *self);
	typedef bool (*Discord_RelationshipHandle_UserFn)(Discord_RelationshipHandle *self, Discord_UserHandle *return_value);
	typedef void (*Discord_RelationshipHandle_DropFn)(Discord_RelationshipHandle *self);
	typedef bool (*Discord_UserHandle_GlobalNameFn)(Discord_UserHandle *self, Discord_String *return_value);
	typedef Discord_StatusType (*Discord_UserHandle_StatusFn)(Discord_UserHandle *self);
	typedef bool (*Discord_UserHandle_IsProvisionalFn)(Discord_UserHandle *self);
	typedef void (*Discord_UserHandle_AvatarUrlFn)(Discord_UserHandle *self,
			Discord_UserHandle_AvatarType animated_type,
			Discord_UserHandle_AvatarType static_type,
			Discord_String *return_value);
	typedef void (*Discord_Client_AcceptDiscordFriendRequestFn)(Discord_Client *self,
			uint64_t user_id,
			Discord_Client_UpdateRelationshipCallback cb,
			Discord_FreeFn cb_user_data_free,
			void *cb_user_data);
	typedef void (*Discord_Client_AcceptGameFriendRequestFn)(Discord_Client *self,
			uint64_t user_id,
			Discord_Client_UpdateRelationshipCallback cb,
			Discord_FreeFn cb_user_data_free,
			void *cb_user_data);
	typedef void (*Discord_Client_BlockUserFn)(Discord_Client *self,
			uint64_t user_id,
			Discord_Client_UpdateRelationshipCallback cb,
			Discord_FreeFn cb_user_data_free,
			void *cb_user_data);
	typedef void (*Discord_Client_CancelDiscordFriendRequestFn)(Discord_Client *self,
			uint64_t user_id,
			Discord_Client_UpdateRelationshipCallback cb,
			Discord_FreeFn cb_user_data_free,
			void *cb_user_data);
	typedef void (*Discord_Client_CancelGameFriendRequestFn)(Discord_Client *self,
			uint64_t user_id,
			Discord_Client_UpdateRelationshipCallback cb,
			Discord_FreeFn cb_user_data_free,
			void *cb_user_data);
	typedef void (*Discord_Client_RejectDiscordFriendRequestFn)(Discord_Client *self,
			uint64_t user_id,
			Discord_Client_UpdateRelationshipCallback cb,
			Discord_FreeFn cb_user_data_free,
			void *cb_user_data);
	typedef void (*Discord_Client_RejectGameFriendRequestFn)(Discord_Client *self,
			uint64_t user_id,
			Discord_Client_UpdateRelationshipCallback cb,
			Discord_FreeFn cb_user_data_free,
			void *cb_user_data);
	typedef void (*Discord_Client_RemoveDiscordAndGameFriendFn)(Discord_Client *self,
			uint64_t user_id,
			Discord_Client_UpdateRelationshipCallback cb,
			Discord_FreeFn cb_user_data_free,
			void *cb_user_data);
	typedef void (*Discord_Client_RemoveGameFriendFn)(Discord_Client *self,
			uint64_t user_id,
			Discord_Client_UpdateRelationshipCallback cb,
			Discord_FreeFn cb_user_data_free,
			void *cb_user_data);
	typedef void (*Discord_Client_SendDiscordFriendRequestFn)(Discord_Client *self,
			Discord_String username,
			Discord_Client_SendFriendRequestCallback cb,
			Discord_FreeFn cb_user_data_free,
			void *cb_user_data);
	typedef void (*Discord_Client_SendDiscordFriendRequestByIdFn)(Discord_Client *self,
			uint64_t user_id,
			Discord_Client_UpdateRelationshipCallback cb,
			Discord_FreeFn cb_user_data_free,
			void *cb_user_data);
	typedef void (*Discord_Client_SendGameFriendRequestFn)(Discord_Client *self,
			Discord_String username,
			Discord_Client_SendFriendRequestCallback cb,
			Discord_FreeFn cb_user_data_free,
			void *cb_user_data);
	typedef void (*Discord_Client_SendGameFriendRequestByIdFn)(Discord_Client *self,
			uint64_t user_id,
			Discord_Client_UpdateRelationshipCallback cb,
			Discord_FreeFn cb_user_data_free,
			void *cb_user_data);
	typedef void (*Discord_Client_SetRelationshipCreatedCallbackFn)(Discord_Client *self,
			Discord_Client_RelationshipCreatedCallback cb,
			Discord_FreeFn cb_user_data_free,
			void *cb_user_data);
	typedef void (*Discord_Client_SetRelationshipDeletedCallbackFn)(Discord_Client *self,
			Discord_Client_RelationshipDeletedCallback cb,
			Discord_FreeFn cb_user_data_free,
			void *cb_user_data);
	typedef void (*Discord_Client_SetRelationshipGroupsUpdatedCallbackFn)(Discord_Client *self,
			Discord_Client_RelationshipGroupsUpdatedCallback cb,
			Discord_FreeFn cb_user_data_free,
			void *cb_user_data);
	typedef void (*Discord_Client_SetUserUpdatedCallbackFn)(Discord_Client *self,
			Discord_Client_UserUpdatedCallback cb,
			Discord_FreeFn cb_user_data_free,
			void *cb_user_data);
	typedef void (*Discord_Client_UnblockUserFn)(Discord_Client *self,
			uint64_t user_id,
			Discord_Client_UpdateRelationshipCallback cb,
			Discord_FreeFn cb_user_data_free,
			void *cb_user_data);

	Discord_RunCallbacksFn fn_run_callbacks = nullptr;
	Discord_Client_InitFn fn_client_init = nullptr;
	Discord_Client_DropFn fn_client_drop = nullptr;
	Discord_Client_SetApplicationIdFn fn_client_set_application_id = nullptr;
	Discord_Client_SetStatusChangedCallbackFn fn_client_set_status_changed_callback = nullptr;
	Discord_Client_SetAuthorizeDeviceScreenClosedCallbackFn fn_client_set_authorize_device_screen_closed_callback = nullptr;
	Discord_Client_CreateAuthorizationCodeVerifierFn fn_client_create_authorization_code_verifier = nullptr;
	Discord_AuthorizationArgs_InitFn fn_authorization_args_init = nullptr;
	Discord_AuthorizationArgs_DropFn fn_authorization_args_drop = nullptr;
	Discord_AuthorizationArgs_SetClientIdFn fn_authorization_args_set_client_id = nullptr;
	Discord_AuthorizationArgs_SetScopesFn fn_authorization_args_set_scopes = nullptr;
	Discord_AuthorizationArgs_SetCodeChallengeFn fn_authorization_args_set_code_challenge = nullptr;
	Discord_AuthorizationArgs_SetIntegrationTypeFn fn_authorization_args_set_integration_type = nullptr;
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
	Discord_UserHandle_GameActivityFn fn_user_handle_game_activity = nullptr;
	Discord_Activity_TypeFn fn_activity_type = nullptr;
	Discord_Activity_DetailsFn fn_activity_details = nullptr;
	Discord_Activity_StateFn fn_activity_state = nullptr;
	Discord_Client_GetDefaultCommunicationScopesFn fn_client_get_default_communication_scopes = nullptr;
	Discord_Client_GetDefaultPresenceScopesFn fn_client_get_default_presence_scopes = nullptr;
	Discord_Client_AddLogCallbackFn fn_client_add_log_callback = nullptr;
	Discord_Client_GetRelationshipsFn fn_client_get_relationships = nullptr;
	Discord_Activity_InitFn fn_activity_init = nullptr;
	Discord_Activity_DropFn fn_activity_drop = nullptr;
	Discord_Activity_SetTypeFn fn_activity_set_type = nullptr;
	Discord_Activity_SetDetailsFn fn_activity_set_details = nullptr;
	Discord_Activity_SetStateFn fn_activity_set_state = nullptr;
	Discord_Activity_SetAssetsFn fn_activity_set_assets = nullptr;
	Discord_Activity_SetTimestampsFn fn_activity_set_timestamps = nullptr;
	Discord_ActivityAssets_InitFn fn_activity_assets_init = nullptr;
	Discord_ActivityAssets_DropFn fn_activity_assets_drop = nullptr;
	Discord_ActivityAssets_SetLargeImageFn fn_activity_assets_set_large_image = nullptr;
	Discord_ActivityAssets_SetLargeTextFn fn_activity_assets_set_large_text = nullptr;
	Discord_ActivityAssets_SetSmallImageFn fn_activity_assets_set_small_image = nullptr;
	Discord_ActivityAssets_SetSmallTextFn fn_activity_assets_set_small_text = nullptr;
	Discord_ActivityTimestamps_InitFn fn_activity_timestamps_init = nullptr;
	Discord_ActivityTimestamps_DropFn fn_activity_timestamps_drop = nullptr;
	Discord_ActivityTimestamps_SetStartFn fn_activity_timestamps_set_start = nullptr;
	Discord_ActivityTimestamps_SetEndFn fn_activity_timestamps_set_end = nullptr;
	Discord_Client_UpdateRichPresenceFn fn_client_update_rich_presence = nullptr;
	Discord_Client_ClearRichPresenceFn fn_client_clear_rich_presence = nullptr;
	Discord_Client_GetVersionHashFn fn_client_get_version_hash = nullptr;
	Discord_Client_GetVersionMajorFn fn_client_get_version_major = nullptr;
	Discord_Client_GetVersionMinorFn fn_client_get_version_minor = nullptr;
	Discord_Client_GetVersionPatchFn fn_client_get_version_patch = nullptr;
	Discord_Client_SetGameWindowPidFn fn_client_set_game_window_pid = nullptr;
	Discord_Client_SetLogDirFn fn_client_set_log_dir = nullptr;
	Discord_Activity_SetNameFn fn_activity_set_name = nullptr;
	Discord_Activity_SetStatusDisplayTypeFn fn_activity_set_status_display_type = nullptr;
	Discord_Activity_SetStateUrlFn fn_activity_set_state_url = nullptr;
	Discord_Activity_SetDetailsUrlFn fn_activity_set_details_url = nullptr;
	Discord_Activity_SetPartyFn fn_activity_set_party = nullptr;
	Discord_Activity_SetSecretsFn fn_activity_set_secrets = nullptr;
	Discord_Activity_SetSupportedPlatformsFn fn_activity_set_supported_platforms = nullptr;
	Discord_Activity_AddButtonFn fn_activity_add_button = nullptr;
	Discord_ActivityAssets_SetLargeUrlFn fn_activity_assets_set_large_url = nullptr;
	Discord_ActivityAssets_SetSmallUrlFn fn_activity_assets_set_small_url = nullptr;
	Discord_ActivityAssets_SetInviteCoverImageFn fn_activity_assets_set_invite_cover_image = nullptr;
	Discord_ActivityParty_InitFn fn_activity_party_init = nullptr;
	Discord_ActivityParty_DropFn fn_activity_party_drop = nullptr;
	Discord_ActivityParty_SetIdFn fn_activity_party_set_id = nullptr;
	Discord_ActivityParty_SetCurrentSizeFn fn_activity_party_set_current_size = nullptr;
	Discord_ActivityParty_SetMaxSizeFn fn_activity_party_set_max_size = nullptr;
	Discord_ActivitySecrets_InitFn fn_activity_secrets_init = nullptr;
	Discord_ActivitySecrets_DropFn fn_activity_secrets_drop = nullptr;
	Discord_ActivitySecrets_SetJoinFn fn_activity_secrets_set_join = nullptr;
	Discord_ActivityButton_InitFn fn_activity_button_init = nullptr;
	Discord_ActivityButton_DropFn fn_activity_button_drop = nullptr;
	Discord_ActivityButton_SetLabelFn fn_activity_button_set_label = nullptr;
	Discord_ActivityButton_SetUrlFn fn_activity_button_set_url = nullptr;
	Discord_ActivityInvite_InitFn fn_activity_invite_init = nullptr;
	Discord_ActivityInvite_DropFn fn_activity_invite_drop = nullptr;
	Discord_ActivityInvite_SetSenderIdFn fn_activity_invite_set_sender_id = nullptr;
	Discord_ActivityInvite_SetChannelIdFn fn_activity_invite_set_channel_id = nullptr;
	Discord_ActivityInvite_SetMessageIdFn fn_activity_invite_set_message_id = nullptr;
	Discord_ActivityInvite_SetTypeFn fn_activity_invite_set_type = nullptr;
	Discord_ActivityInvite_SetApplicationIdFn fn_activity_invite_set_application_id = nullptr;
	Discord_ActivityInvite_SetParentApplicationIdFn fn_activity_invite_set_parent_application_id = nullptr;
	Discord_ActivityInvite_SetPartyIdFn fn_activity_invite_set_party_id = nullptr;
	Discord_ActivityInvite_SetSessionIdFn fn_activity_invite_set_session_id = nullptr;
	Discord_ActivityInvite_SetIsValidFn fn_activity_invite_set_is_valid = nullptr;
	Discord_ActivityInvite_SenderIdFn fn_activity_invite_sender_id = nullptr;
	Discord_ActivityInvite_ChannelIdFn fn_activity_invite_channel_id = nullptr;
	Discord_ActivityInvite_MessageIdFn fn_activity_invite_message_id = nullptr;
	Discord_ActivityInvite_TypeFn fn_activity_invite_type = nullptr;
	Discord_ActivityInvite_ApplicationIdFn fn_activity_invite_application_id = nullptr;
	Discord_ActivityInvite_ParentApplicationIdFn fn_activity_invite_parent_application_id = nullptr;
	Discord_ActivityInvite_PartyIdFn fn_activity_invite_party_id = nullptr;
	Discord_ActivityInvite_SessionIdFn fn_activity_invite_session_id = nullptr;
	Discord_ActivityInvite_IsValidFn fn_activity_invite_is_valid = nullptr;
	Discord_Client_RegisterLaunchCommandFn fn_client_register_launch_command = nullptr;
	Discord_Client_RegisterLaunchSteamApplicationFn fn_client_register_launch_steam_application = nullptr;
	Discord_Client_SendActivityInviteFn fn_client_send_activity_invite = nullptr;
	Discord_Client_SendActivityJoinRequestFn fn_client_send_activity_join_request = nullptr;
	Discord_Client_SendActivityJoinRequestReplyFn fn_client_send_activity_join_request_reply = nullptr;
	Discord_Client_AcceptActivityInviteFn fn_client_accept_activity_invite = nullptr;
	Discord_Client_CreateOrJoinLobbyFn fn_client_create_or_join_lobby = nullptr;
	Discord_Client_SetActivityInviteCreatedCallbackFn fn_client_set_activity_invite_created_callback = nullptr;
	Discord_Client_SetActivityInviteUpdatedCallbackFn fn_client_set_activity_invite_updated_callback = nullptr;
	Discord_Client_SetActivityJoinCallbackFn fn_client_set_activity_join_callback = nullptr;
	Discord_Client_SetLobbyCreatedCallbackFn fn_client_set_lobby_created_callback = nullptr;
	Discord_Client_GetRelationshipsByGroupFn fn_client_get_relationships_by_group = nullptr;
	Discord_Client_GetRelationshipHandleFn fn_client_get_relationship_handle = nullptr;
	Discord_Client_GetUserFn fn_client_get_user = nullptr;
	Discord_RelationshipHandle_DiscordRelationshipTypeFn fn_relationship_handle_discord_relationship_type = nullptr;
	Discord_RelationshipHandle_GameRelationshipTypeFn fn_relationship_handle_game_relationship_type = nullptr;
	Discord_RelationshipHandle_IdFn fn_relationship_handle_id = nullptr;
	Discord_RelationshipHandle_IsSpamRequestFn fn_relationship_handle_is_spam_request = nullptr;
	Discord_RelationshipHandle_UserFn fn_relationship_handle_user = nullptr;
	Discord_RelationshipHandle_DropFn fn_relationship_handle_drop = nullptr;
	Discord_UserHandle_GlobalNameFn fn_user_handle_global_name = nullptr;
	Discord_UserHandle_StatusFn fn_user_handle_status = nullptr;
	Discord_UserHandle_IsProvisionalFn fn_user_handle_is_provisional = nullptr;
	Discord_UserHandle_AvatarUrlFn fn_user_handle_avatar_url = nullptr;
	Discord_Client_AcceptDiscordFriendRequestFn fn_client_accept_discord_friend_request = nullptr;
	Discord_Client_AcceptGameFriendRequestFn fn_client_accept_game_friend_request = nullptr;
	Discord_Client_BlockUserFn fn_client_block_user = nullptr;
	Discord_Client_CancelDiscordFriendRequestFn fn_client_cancel_discord_friend_request = nullptr;
	Discord_Client_CancelGameFriendRequestFn fn_client_cancel_game_friend_request = nullptr;
	Discord_Client_RejectDiscordFriendRequestFn fn_client_reject_discord_friend_request = nullptr;
	Discord_Client_RejectGameFriendRequestFn fn_client_reject_game_friend_request = nullptr;
	Discord_Client_RemoveDiscordAndGameFriendFn fn_client_remove_discord_and_game_friend = nullptr;
	Discord_Client_RemoveGameFriendFn fn_client_remove_game_friend = nullptr;
	Discord_Client_SendDiscordFriendRequestFn fn_client_send_discord_friend_request = nullptr;
	Discord_Client_SendDiscordFriendRequestByIdFn fn_client_send_discord_friend_request_by_id = nullptr;
	Discord_Client_SendGameFriendRequestFn fn_client_send_game_friend_request = nullptr;
	Discord_Client_SendGameFriendRequestByIdFn fn_client_send_game_friend_request_by_id = nullptr;
	Discord_Client_SetRelationshipCreatedCallbackFn fn_client_set_relationship_created_callback = nullptr;
	Discord_Client_SetRelationshipDeletedCallbackFn fn_client_set_relationship_deleted_callback = nullptr;
	Discord_Client_SetRelationshipGroupsUpdatedCallbackFn fn_client_set_relationship_groups_updated_callback = nullptr;
	Discord_Client_SetUserUpdatedCallbackFn fn_client_set_user_updated_callback = nullptr;
	Discord_Client_UnblockUserFn fn_client_unblock_user = nullptr;

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
	bool client_set_authorize_device_screen_closed_callback(Discord_Client *self,
			Discord_Client_AuthorizeDeviceScreenClosedCallback callback,
			void *user_data) const;
	void client_create_authorization_code_verifier(Discord_Client *self,
			Discord_AuthorizationCodeVerifier *return_value) const;
	void authorization_args_init(Discord_AuthorizationArgs *self) const;
	void authorization_args_drop(Discord_AuthorizationArgs *self) const;
	void authorization_args_set_client_id(Discord_AuthorizationArgs *self, uint64_t value) const;
	void authorization_args_set_scopes(Discord_AuthorizationArgs *self, const String &p_scopes) const;
	void authorization_args_set_code_challenge(Discord_AuthorizationArgs *self,
			Discord_AuthorizationCodeChallenge *value) const;
	void authorization_args_set_integration_type(Discord_AuthorizationArgs *self,
			Discord_IntegrationType value) const;
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
	void client_get_token(Discord_Client *self,
			uint64_t application_id,
			Discord_String code,
			Discord_String code_verifier,
			Discord_String redirect_uri,
			Discord_Client_TokenExchangeCallback callback,
			void *user_data) const;
	void client_update_token(Discord_Client *self,
			Discord_AuthorizationTokenType token_type,
			const String &p_token,
			Discord_Client_UpdateTokenCallback callback,
			void *user_data) const;
	void client_update_token(Discord_Client *self,
			Discord_AuthorizationTokenType token_type,
			Discord_String token,
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
	bool user_handle_game_activity(Discord_UserHandle *self, Discord_Activity *return_value) const;
	Discord_ActivityTypes activity_get_type(Discord_Activity *self) const;
	String activity_get_details(Discord_Activity *self) const;
	String activity_get_state(Discord_Activity *self) const;
	String get_default_communication_scopes() const;
	String get_default_presence_scopes() const;
	void client_add_log_callback(Discord_Client *self,
			Discord_Client_LogCallback callback,
			void *user_data,
			Discord_LoggingSeverity min_severity) const;
	void client_get_relationships(Discord_Client *self, Discord_RelationshipHandleSpan *return_value) const;
	void activity_init(Discord_Activity *self) const;
	void activity_drop(Discord_Activity *self) const;
	void activity_set_type(Discord_Activity *self, Discord_ActivityTypes value) const;
	void activity_set_details(Discord_Activity *self, const String &p_value) const;
	void activity_set_state(Discord_Activity *self, const String &p_value) const;
	void activity_set_assets(Discord_Activity *self, Discord_ActivityAssets *value) const;
	void activity_set_timestamps(Discord_Activity *self, Discord_ActivityTimestamps *value) const;
	void activity_assets_init(Discord_ActivityAssets *self) const;
	void activity_assets_drop(Discord_ActivityAssets *self) const;
	void activity_assets_set_large_image(Discord_ActivityAssets *self, const String &p_value) const;
	void activity_assets_set_large_text(Discord_ActivityAssets *self, const String &p_value) const;
	void activity_assets_set_small_image(Discord_ActivityAssets *self, const String &p_value) const;
	void activity_assets_set_small_text(Discord_ActivityAssets *self, const String &p_value) const;
	void activity_timestamps_init(Discord_ActivityTimestamps *self) const;
	void activity_timestamps_drop(Discord_ActivityTimestamps *self) const;
	void activity_timestamps_set_start(Discord_ActivityTimestamps *self, uint64_t value) const;
	void activity_timestamps_set_end(Discord_ActivityTimestamps *self, uint64_t value) const;
	void client_update_rich_presence(Discord_Client *self,
			Discord_Activity *activity,
			Discord_Client_UpdateRichPresenceCallback callback,
			void *user_data) const;
	void client_clear_rich_presence(Discord_Client *self) const;
	void client_set_game_window_pid(Discord_Client *self, int32_t pid) const;
	bool client_set_log_dir(Discord_Client *self, const String &p_path, Discord_LoggingSeverity min_severity) const;
	void fill_version_dict(Dictionary &p_version) const;

	bool has_activity_extensions() const { return fn_activity_set_name != nullptr; }
	bool has_invite_api() const { return fn_client_register_launch_command != nullptr; }
	bool has_relationship_api() const { return fn_client_get_relationships_by_group != nullptr; }

	void activity_set_name(Discord_Activity *self, const String &p_value) const;
	void activity_set_status_display_type(Discord_Activity *self, Discord_StatusDisplayTypes value) const;
	void activity_set_state_url(Discord_Activity *self, const String &p_value) const;
	void activity_set_details_url(Discord_Activity *self, const String &p_value) const;
	void activity_set_party(Discord_Activity *self, Discord_ActivityParty *value) const;
	void activity_set_secrets(Discord_Activity *self, Discord_ActivitySecrets *value) const;
	void activity_set_supported_platforms(Discord_Activity *self, Discord_ActivityGamePlatforms value) const;
	void activity_add_button(Discord_Activity *self, Discord_ActivityButton const *button) const;
	void activity_assets_set_large_url(Discord_ActivityAssets *self, const String &p_value) const;
	void activity_assets_set_small_url(Discord_ActivityAssets *self, const String &p_value) const;
	void activity_assets_set_invite_cover_image(Discord_ActivityAssets *self, const String &p_value) const;
	void activity_party_init(Discord_ActivityParty *self) const;
	void activity_party_drop(Discord_ActivityParty *self) const;
	void activity_party_set_id(Discord_ActivityParty *self, const String &p_value) const;
	void activity_party_set_current_size(Discord_ActivityParty *self, int32_t value) const;
	void activity_party_set_max_size(Discord_ActivityParty *self, int32_t value) const;
	void activity_secrets_init(Discord_ActivitySecrets *self) const;
	void activity_secrets_drop(Discord_ActivitySecrets *self) const;
	void activity_secrets_set_join(Discord_ActivitySecrets *self, const String &p_value) const;
	void activity_button_init(Discord_ActivityButton *self) const;
	void activity_button_drop(Discord_ActivityButton *self) const;
	void activity_button_set_label(Discord_ActivityButton *self, const String &p_value) const;
	void activity_button_set_url(Discord_ActivityButton *self, const String &p_value) const;
	void activity_invite_init(Discord_ActivityInvite *self) const;
	void activity_invite_drop(Discord_ActivityInvite *self) const;
	void activity_invite_set_sender_id(Discord_ActivityInvite *self, uint64_t value) const;
	void activity_invite_set_channel_id(Discord_ActivityInvite *self, uint64_t value) const;
	void activity_invite_set_message_id(Discord_ActivityInvite *self, uint64_t value) const;
	void activity_invite_set_type(Discord_ActivityInvite *self, Discord_ActivityActionTypes value) const;
	void activity_invite_set_application_id(Discord_ActivityInvite *self, uint64_t value) const;
	void activity_invite_set_parent_application_id(Discord_ActivityInvite *self, uint64_t value) const;
	void activity_invite_set_party_id(Discord_ActivityInvite *self, const String &p_value) const;
	void activity_invite_set_session_id(Discord_ActivityInvite *self, const String &p_value) const;
	void activity_invite_set_is_valid(Discord_ActivityInvite *self, bool value) const;
	Dictionary activity_invite_to_dict(Discord_ActivityInvite *self) const;
	bool client_register_launch_command(Discord_Client *self, uint64_t application_id, const String &p_command) const;
	bool client_register_launch_steam_application(Discord_Client *self, uint64_t application_id, uint32_t steam_app_id) const;
	void client_send_activity_invite(Discord_Client *self,
			uint64_t user_id,
			const String &p_content,
			Discord_Client_SendActivityInviteCallback callback,
			void *user_data) const;
	void client_send_activity_join_request(Discord_Client *self,
			uint64_t user_id,
			Discord_Client_SendActivityInviteCallback callback,
			void *user_data) const;
	void client_send_activity_join_request_reply(Discord_Client *self,
			Discord_ActivityInvite *invite,
			Discord_Client_SendActivityInviteCallback callback,
			void *user_data) const;
	void client_accept_activity_invite(Discord_Client *self,
			Discord_ActivityInvite *invite,
			Discord_Client_AcceptActivityInviteCallback callback,
			void *user_data) const;
	void client_create_or_join_lobby(Discord_Client *self,
			const String &p_secret,
			Discord_Client_CreateOrJoinLobbyCallback callback,
			void *user_data) const;
	void client_set_activity_invite_created_callback(Discord_Client *self,
			Discord_Client_ActivityInviteCallback callback,
			void *user_data) const;
	void client_set_activity_invite_updated_callback(Discord_Client *self,
			Discord_Client_ActivityInviteCallback callback,
			void *user_data) const;
	void client_set_activity_join_callback(Discord_Client *self,
			Discord_Client_ActivityJoinCallback callback,
			void *user_data) const;
	void client_set_lobby_created_callback(Discord_Client *self,
			Discord_Client_LobbyCreatedCallback callback,
			void *user_data) const;
	void client_get_relationships_by_group(Discord_Client *self,
			Discord_RelationshipGroupType group_type,
			Discord_RelationshipHandleSpan *return_value) const;
	void client_get_relationship_handle(Discord_Client *self,
			uint64_t user_id,
			Discord_RelationshipHandle *return_value) const;
	bool client_get_user(Discord_Client *self, uint64_t user_id, Discord_UserHandle *return_value) const;
	void relationship_handle_drop(Discord_RelationshipHandle *self) const;
	Discord_RelationshipType relationship_handle_discord_relationship_type(Discord_RelationshipHandle *self) const;
	Discord_RelationshipType relationship_handle_game_relationship_type(Discord_RelationshipHandle *self) const;
	uint64_t relationship_handle_id(Discord_RelationshipHandle *self) const;
	bool relationship_handle_is_spam_request(Discord_RelationshipHandle *self) const;
	bool relationship_handle_user(Discord_RelationshipHandle *self, Discord_UserHandle *return_value) const;
	String user_handle_global_name(Discord_UserHandle *self) const;
	Discord_StatusType user_handle_status(Discord_UserHandle *self) const;
	bool user_handle_is_provisional(Discord_UserHandle *self) const;
	String user_handle_avatar_url(Discord_UserHandle *self) const;
	void client_accept_discord_friend_request(Discord_Client *self,
			uint64_t user_id,
			Discord_Client_UpdateRelationshipCallback callback,
			void *user_data) const;
	void client_accept_game_friend_request(Discord_Client *self,
			uint64_t user_id,
			Discord_Client_UpdateRelationshipCallback callback,
			void *user_data) const;
	void client_block_user(Discord_Client *self,
			uint64_t user_id,
			Discord_Client_UpdateRelationshipCallback callback,
			void *user_data) const;
	void client_cancel_discord_friend_request(Discord_Client *self,
			uint64_t user_id,
			Discord_Client_UpdateRelationshipCallback callback,
			void *user_data) const;
	void client_cancel_game_friend_request(Discord_Client *self,
			uint64_t user_id,
			Discord_Client_UpdateRelationshipCallback callback,
			void *user_data) const;
	void client_reject_discord_friend_request(Discord_Client *self,
			uint64_t user_id,
			Discord_Client_UpdateRelationshipCallback callback,
			void *user_data) const;
	void client_reject_game_friend_request(Discord_Client *self,
			uint64_t user_id,
			Discord_Client_UpdateRelationshipCallback callback,
			void *user_data) const;
	void client_remove_discord_and_game_friend(Discord_Client *self,
			uint64_t user_id,
			Discord_Client_UpdateRelationshipCallback callback,
			void *user_data) const;
	void client_remove_game_friend(Discord_Client *self,
			uint64_t user_id,
			Discord_Client_UpdateRelationshipCallback callback,
			void *user_data) const;
	void client_send_discord_friend_request(Discord_Client *self,
			const String &p_username,
			Discord_Client_SendFriendRequestCallback callback,
			void *user_data) const;
	void client_send_discord_friend_request_by_id(Discord_Client *self,
			uint64_t user_id,
			Discord_Client_UpdateRelationshipCallback callback,
			void *user_data) const;
	void client_send_game_friend_request(Discord_Client *self,
			const String &p_username,
			Discord_Client_SendFriendRequestCallback callback,
			void *user_data) const;
	void client_send_game_friend_request_by_id(Discord_Client *self,
			uint64_t user_id,
			Discord_Client_UpdateRelationshipCallback callback,
			void *user_data) const;
	void client_set_relationship_created_callback(Discord_Client *self,
			Discord_Client_RelationshipCreatedCallback callback,
			void *user_data) const;
	void client_set_relationship_deleted_callback(Discord_Client *self,
			Discord_Client_RelationshipDeletedCallback callback,
			void *user_data) const;
	void client_set_relationship_groups_updated_callback(Discord_Client *self,
			Discord_Client_RelationshipGroupsUpdatedCallback callback,
			void *user_data) const;
	void client_set_user_updated_callback(Discord_Client *self,
			Discord_Client_UserUpdatedCallback callback,
			void *user_data) const;
	void client_unblock_user(Discord_Client *self,
			uint64_t user_id,
			Discord_Client_UpdateRelationshipCallback callback,
			void *user_data) const;

	static Discord_String make_string(const String &p_value);
	static String to_godot_string(const Discord_String &p_value);
};
