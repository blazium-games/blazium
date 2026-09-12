/**************************************************************************/
/*  discord_embedded_app_client.h                                         */
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

#include "../third_party_client.h"
#include "discord_embedded_app_response.h"
#include "platform/web/api/javascript_bridge_singleton.h"
#include "scene/main/node.h"

// From https://github.com/discord/embedded-app-sdk/blob/main/src/Discord.ts v1.9.0
class DiscordEmbeddedAppClient : public ThirdPartyClient {
	GDCLASS(DiscordEmbeddedAppClient, ThirdPartyClient);

	enum Opcode {
		OP_HANDSHAKE = 0,
		OP_FRAME = 1,
		OP_CLOSE = 2,
		OP_HELLO = 3,
	};

	bool discord_ready = false;

	String user_id;
	String client_id;
	String user_instance_id;
	String custom_id;
	String referrer_id;
	String platform;
	String guild_id;
	String channel_id;
	String location_id;
	String sdk_version = "1.9.0";
	String mobile_app_version;

	String frame_id;
	Dictionary _commands;

	Ref<JavaScriptObject> callback;
	Ref<DiscordEmbeddedAppResponse> ready_response;
	Ref<JavaScriptObject> window;

	void _handle_message(Variant p_event);
	void _handle_dispatch(Dictionary p_data);
	void _send_command(String p_command, Dictionary p_args, String p_nonce);
	void _send_message(int p_opcode, Dictionary p_body);
	void _handshake();

protected:
	static void _bind_methods();

public:
	enum DiscordEmbeddedAppOrientationLockState {
		DISCORD_EMBEDDED_APP_ORIENTATION_LOCK_STATE_UNHANDLED = -1,
		DISCORD_EMBEDDED_APP_ORIENTATION_LOCK_STATE_UNLOCKED = 1,
		DISCORD_EMBEDDED_APP_ORIENTATION_LOCK_STATE_PORTRAIT = 2,
		DISCORD_EMBEDDED_APP_ORIENTATION_LOCK_STATE_LANDSCAPE = 3,
	};
	void subscribe_to_all_events();
	bool is_discord_environment();
	static bool static_is_discord_environment();
	void close(int p_code, String p_message);
	String get_user_id() { return user_id; }
	String get_client_id() { return client_id; }
	void set_client_id(String p_client_id) { return client_id = p_client_id; }
	static String static_find_client_id();
	String get_user_instance_id() { return user_instance_id; }
	String get_custom_id() { return custom_id; }
	String get_referrer_id() { return referrer_id; }
	String get_platform() { return platform; }
	String get_guild_id() { return guild_id; }
	String get_channel_id() { return channel_id; }
	String get_location_id() { return location_id; }
	String get_sdk_version() { return sdk_version; }
	String get_mobile_app_version() { return mobile_app_version; }
	String get_frame_id() { return frame_id; }

	Ref<DiscordEmbeddedAppResponse> is_ready();
	Ref<DiscordEmbeddedAppResponse> authenticate(String p_access_token);
	Ref<DiscordEmbeddedAppResponse> authorize(String p_response_type, String p_sate, String p_prompt, Array p_scope);
	Ref<DiscordEmbeddedAppResponse> capture_log(String p_level, String p_message);
	Ref<DiscordEmbeddedAppResponse> encourage_hardware_acceleration();
	Ref<DiscordEmbeddedAppResponse> get_channel(String p_channel_id);
	Ref<DiscordEmbeddedAppResponse> get_channel_permissions();
	Ref<DiscordEmbeddedAppResponse> get_entitlements();
	Ref<DiscordEmbeddedAppResponse> get_instance_connected_participants();
	Ref<DiscordEmbeddedAppResponse> get_platform_behaviours();
	Ref<DiscordEmbeddedAppResponse> get_skus();
	Ref<DiscordEmbeddedAppResponse> initiate_image_upload();
	Ref<DiscordEmbeddedAppResponse> open_external_link(String p_url);
	Ref<DiscordEmbeddedAppResponse> open_invite_dialog();
	Ref<DiscordEmbeddedAppResponse> open_share_moment_dialog(String p_media_url);
	Ref<DiscordEmbeddedAppResponse> set_activity(Dictionary p_activity);
	Ref<DiscordEmbeddedAppResponse> set_config(bool p_use_interactive_pip);
	Ref<DiscordEmbeddedAppResponse> set_orientation_lock_state(DiscordEmbeddedAppOrientationLockState p_lock_state, DiscordEmbeddedAppOrientationLockState p_picture_in_picture_lock_state, DiscordEmbeddedAppOrientationLockState p_grid_lock_state);
	Ref<DiscordEmbeddedAppResponse> start_purchase(String p_sku_id, String p_pid);
	Ref<DiscordEmbeddedAppResponse> user_settings_get_locale();
	DiscordEmbeddedAppClient();
};

VARIANT_ENUM_CAST(DiscordEmbeddedAppClient::DiscordEmbeddedAppOrientationLockState);
