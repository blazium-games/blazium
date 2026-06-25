/**************************************************************************/
/*  discord_types.h                                                       */
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

#include "core/typedefs.h"

typedef struct Discord_String {
	uint8_t *ptr;
	size_t size;
} Discord_String;

typedef enum Discord_Client_Error {
	DISCORD_CLIENT_ERROR_NONE = 0,
	DISCORD_CLIENT_ERROR_CONNECTION_FAILED = 1,
	DISCORD_CLIENT_ERROR_UNEXPECTED_CLOSE = 2,
	DISCORD_CLIENT_ERROR_CONNECTION_CANCELED = 3,
} Discord_Client_Error;

typedef enum Discord_Client_Status {
	DISCORD_CLIENT_STATUS_DISCONNECTED = 0,
	DISCORD_CLIENT_STATUS_CONNECTING = 1,
	DISCORD_CLIENT_STATUS_CONNECTED = 2,
	DISCORD_CLIENT_STATUS_READY = 3,
	DISCORD_CLIENT_STATUS_RECONNECTING = 4,
	DISCORD_CLIENT_STATUS_DISCONNECTING = 5,
	DISCORD_CLIENT_STATUS_HTTP_WAIT = 6,
} Discord_Client_Status;

typedef enum Discord_AuthorizationTokenType {
	DISCORD_AUTHORIZATION_TOKEN_TYPE_USER = 0,
	DISCORD_AUTHORIZATION_TOKEN_TYPE_BEARER = 1,
} Discord_AuthorizationTokenType;

typedef enum Discord_IntegrationType {
	DISCORD_INTEGRATION_TYPE_GUILD_INSTALL = 0,
	DISCORD_INTEGRATION_TYPE_USER_INSTALL = 1,
} Discord_IntegrationType;

typedef struct Discord_Client {
	void *opaque;
} Discord_Client;

typedef struct Discord_ClientResult {
	void *opaque;
} Discord_ClientResult;

typedef struct Discord_AuthorizationCodeChallenge {
	void *opaque;
} Discord_AuthorizationCodeChallenge;

typedef struct Discord_AuthorizationCodeVerifier {
	void *opaque;
} Discord_AuthorizationCodeVerifier;

typedef struct Discord_AuthorizationArgs {
	void *opaque;
} Discord_AuthorizationArgs;

typedef struct Discord_UserHandle {
	void *opaque;
} Discord_UserHandle;

typedef void (*Discord_FreeFn)(void *ptr);

typedef void (*Discord_Client_AuthorizationCallback)(Discord_ClientResult *result,
		Discord_String code,
		Discord_String redirect_uri,
		void *user_data);

typedef void (*Discord_Client_TokenExchangeCallback)(Discord_ClientResult *result,
		Discord_String access_token,
		Discord_String refresh_token,
		Discord_AuthorizationTokenType token_type,
		int32_t expires_in,
		Discord_String scopes,
		void *user_data);

typedef void (*Discord_Client_UpdateTokenCallback)(Discord_ClientResult *result, void *user_data);

typedef void (*Discord_Client_OnStatusChanged)(Discord_Client_Status status,
		Discord_Client_Error error,
		int32_t error_detail,
		void *user_data);

typedef enum Discord_ActivityTypes {
	DISCORD_ACTIVITY_TYPE_PLAYING = 0,
	DISCORD_ACTIVITY_TYPE_STREAMING = 1,
	DISCORD_ACTIVITY_TYPE_LISTENING = 2,
	DISCORD_ACTIVITY_TYPE_WATCHING = 3,
	DISCORD_ACTIVITY_TYPE_CUSTOM_STATUS = 4,
	DISCORD_ACTIVITY_TYPE_COMPETING = 5,
	DISCORD_ACTIVITY_TYPE_HANG_STATUS = 6,
} Discord_ActivityTypes;

typedef enum Discord_LoggingSeverity {
	DISCORD_LOGGING_SEVERITY_VERBOSE = 1,
	DISCORD_LOGGING_SEVERITY_INFO = 2,
	DISCORD_LOGGING_SEVERITY_WARNING = 3,
	DISCORD_LOGGING_SEVERITY_ERROR = 4,
	DISCORD_LOGGING_SEVERITY_NONE = 5,
} Discord_LoggingSeverity;

typedef struct Discord_Activity {
	void *opaque;
} Discord_Activity;

typedef struct Discord_ActivityAssets {
	void *opaque;
} Discord_ActivityAssets;

typedef struct Discord_ActivityTimestamps {
	void *opaque;
} Discord_ActivityTimestamps;

typedef struct Discord_RelationshipHandle {
	void *opaque;
} Discord_RelationshipHandle;

typedef struct Discord_RelationshipHandleSpan {
	Discord_RelationshipHandle *ptr;
	size_t size;
} Discord_RelationshipHandleSpan;

typedef void (*Discord_Client_LogCallback)(Discord_String message,
		Discord_LoggingSeverity severity,
		void *user_data);

typedef void (*Discord_Client_UpdateRichPresenceCallback)(Discord_ClientResult *result,
		void *user_data);

typedef void (*Discord_Client_AuthorizeDeviceScreenClosedCallback)(void *user_data);
