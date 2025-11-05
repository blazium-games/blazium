/**************************************************************************/
/*  twitch_moderation_requests.cpp                                        */
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

#include "twitch_moderation_requests.h"

#include "core/io/json.h"

void TwitchModerationRequests::_bind_methods() {
	ClassDB::bind_method(D_METHOD("check_automod_status", "broadcaster_id", "messages"), &TwitchModerationRequests::check_automod_status);
	ClassDB::bind_method(D_METHOD("manage_held_automod_messages", "user_id", "msg_id", "action"), &TwitchModerationRequests::manage_held_automod_messages);
	ClassDB::bind_method(D_METHOD("get_automod_settings", "broadcaster_id", "moderator_id"), &TwitchModerationRequests::get_automod_settings);
	ClassDB::bind_method(D_METHOD("update_automod_settings", "broadcaster_id", "moderator_id", "settings"), &TwitchModerationRequests::update_automod_settings);
	ClassDB::bind_method(D_METHOD("get_banned_users", "broadcaster_id", "params"), &TwitchModerationRequests::get_banned_users, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("ban_user", "broadcaster_id", "moderator_id", "data"), &TwitchModerationRequests::ban_user);
	ClassDB::bind_method(D_METHOD("unban_user", "broadcaster_id", "moderator_id", "user_id"), &TwitchModerationRequests::unban_user);
	ClassDB::bind_method(D_METHOD("get_blocked_terms", "broadcaster_id", "moderator_id", "params"), &TwitchModerationRequests::get_blocked_terms, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("add_blocked_term", "broadcaster_id", "moderator_id", "text"), &TwitchModerationRequests::add_blocked_term);
	ClassDB::bind_method(D_METHOD("remove_blocked_term", "broadcaster_id", "moderator_id", "id"), &TwitchModerationRequests::remove_blocked_term);
	ClassDB::bind_method(D_METHOD("delete_chat_messages", "broadcaster_id", "moderator_id", "message_id"), &TwitchModerationRequests::delete_chat_messages, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("get_moderated_channels", "user_id", "params"), &TwitchModerationRequests::get_moderated_channels, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("get_moderators", "broadcaster_id", "params"), &TwitchModerationRequests::get_moderators, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("add_channel_moderator", "broadcaster_id", "user_id"), &TwitchModerationRequests::add_channel_moderator);
	ClassDB::bind_method(D_METHOD("remove_channel_moderator", "broadcaster_id", "user_id"), &TwitchModerationRequests::remove_channel_moderator);
	ClassDB::bind_method(D_METHOD("get_vips", "broadcaster_id", "params"), &TwitchModerationRequests::get_vips, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("add_channel_vip", "broadcaster_id", "user_id"), &TwitchModerationRequests::add_channel_vip);
	ClassDB::bind_method(D_METHOD("remove_channel_vip", "broadcaster_id", "user_id"), &TwitchModerationRequests::remove_channel_vip);
	ClassDB::bind_method(D_METHOD("update_shield_mode_status", "broadcaster_id", "moderator_id", "is_active"), &TwitchModerationRequests::update_shield_mode_status);
	ClassDB::bind_method(D_METHOD("get_shield_mode_status", "broadcaster_id", "moderator_id"), &TwitchModerationRequests::get_shield_mode_status);
	ClassDB::bind_method(D_METHOD("warn_chat_user", "broadcaster_id", "moderator_id", "user_id", "reason"), &TwitchModerationRequests::warn_chat_user);
}

void TwitchModerationRequests::check_automod_status(const String &p_broadcaster_id, const Array &p_messages) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");

	Dictionary params;
	params["broadcaster_id"] = p_broadcaster_id;

	Dictionary body_data;
	body_data["data"] = p_messages;
	String body = JSON::stringify(body_data);

	_request_post("automod_status_checked", "/moderation/enforcements/status", params, body);
}

void TwitchModerationRequests::manage_held_automod_messages(const String &p_user_id, const String &p_msg_id, const String &p_action) {
	ERR_FAIL_COND_MSG(p_user_id.is_empty(), "user_id cannot be empty");
	ERR_FAIL_COND_MSG(p_msg_id.is_empty(), "msg_id cannot be empty");
	ERR_FAIL_COND_MSG(p_action.is_empty(), "action cannot be empty");

	Dictionary body_data;
	body_data["user_id"] = p_user_id;
	body_data["msg_id"] = p_msg_id;
	body_data["action"] = p_action;

	String body = JSON::stringify(body_data);
	_request_post("automod_message_managed", "/moderation/automod/message", Dictionary(), body);
}

void TwitchModerationRequests::get_automod_settings(const String &p_broadcaster_id, const String &p_moderator_id) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");
	ERR_FAIL_COND_MSG(p_moderator_id.is_empty(), "moderator_id cannot be empty");

	Dictionary params;
	params["broadcaster_id"] = p_broadcaster_id;
	params["moderator_id"] = p_moderator_id;
	_request_get("automod_settings_received", "/moderation/automod/settings", params);
}

void TwitchModerationRequests::update_automod_settings(const String &p_broadcaster_id, const String &p_moderator_id, const Dictionary &p_settings) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");
	ERR_FAIL_COND_MSG(p_moderator_id.is_empty(), "moderator_id cannot be empty");

	Dictionary params;
	params["broadcaster_id"] = p_broadcaster_id;
	params["moderator_id"] = p_moderator_id;

	String body = JSON::stringify(p_settings);
	_request_put("automod_settings_updated", "/moderation/automod/settings", params, body);
}

void TwitchModerationRequests::get_banned_users(const String &p_broadcaster_id, const Dictionary &p_params) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");

	Dictionary params = p_params;
	params["broadcaster_id"] = p_broadcaster_id;
	_request_get("banned_users_received", "/moderation/banned", params);
}

void TwitchModerationRequests::ban_user(const String &p_broadcaster_id, const String &p_moderator_id, const Dictionary &p_data) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");
	ERR_FAIL_COND_MSG(p_moderator_id.is_empty(), "moderator_id cannot be empty");

	Dictionary params;
	params["broadcaster_id"] = p_broadcaster_id;
	params["moderator_id"] = p_moderator_id;

	Dictionary body_data;
	body_data["data"] = p_data;
	String body = JSON::stringify(body_data);

	_request_post("user_banned", "/moderation/bans", params, body);
}

void TwitchModerationRequests::unban_user(const String &p_broadcaster_id, const String &p_moderator_id, const String &p_user_id) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");
	ERR_FAIL_COND_MSG(p_moderator_id.is_empty(), "moderator_id cannot be empty");
	ERR_FAIL_COND_MSG(p_user_id.is_empty(), "user_id cannot be empty");

	Dictionary params;
	params["broadcaster_id"] = p_broadcaster_id;
	params["moderator_id"] = p_moderator_id;
	params["user_id"] = p_user_id;

	_request_delete("user_unbanned", "/moderation/bans", params);
}

void TwitchModerationRequests::get_blocked_terms(const String &p_broadcaster_id, const String &p_moderator_id, const Dictionary &p_params) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");
	ERR_FAIL_COND_MSG(p_moderator_id.is_empty(), "moderator_id cannot be empty");

	Dictionary params = p_params;
	params["broadcaster_id"] = p_broadcaster_id;
	params["moderator_id"] = p_moderator_id;
	_request_get("blocked_terms_received", "/moderation/blocked_terms", params);
}

void TwitchModerationRequests::add_blocked_term(const String &p_broadcaster_id, const String &p_moderator_id, const String &p_text) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");
	ERR_FAIL_COND_MSG(p_moderator_id.is_empty(), "moderator_id cannot be empty");
	ERR_FAIL_COND_MSG(p_text.is_empty(), "text cannot be empty");

	Dictionary params;
	params["broadcaster_id"] = p_broadcaster_id;
	params["moderator_id"] = p_moderator_id;

	Dictionary body_data;
	body_data["text"] = p_text;
	String body = JSON::stringify(body_data);

	_request_post("blocked_term_added", "/moderation/blocked_terms", params, body);
}

void TwitchModerationRequests::remove_blocked_term(const String &p_broadcaster_id, const String &p_moderator_id, const String &p_id) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");
	ERR_FAIL_COND_MSG(p_moderator_id.is_empty(), "moderator_id cannot be empty");
	ERR_FAIL_COND_MSG(p_id.is_empty(), "id cannot be empty");

	Dictionary params;
	params["broadcaster_id"] = p_broadcaster_id;
	params["moderator_id"] = p_moderator_id;
	params["id"] = p_id;

	_request_delete("blocked_term_removed", "/moderation/blocked_terms", params);
}

void TwitchModerationRequests::delete_chat_messages(const String &p_broadcaster_id, const String &p_moderator_id, const String &p_message_id) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");
	ERR_FAIL_COND_MSG(p_moderator_id.is_empty(), "moderator_id cannot be empty");

	Dictionary params;
	params["broadcaster_id"] = p_broadcaster_id;
	params["moderator_id"] = p_moderator_id;
	if (!p_message_id.is_empty()) {
		params["message_id"] = p_message_id;
	}

	_request_delete("chat_messages_deleted", "/moderation/chat", params);
}

void TwitchModerationRequests::get_moderated_channels(const String &p_user_id, const Dictionary &p_params) {
	ERR_FAIL_COND_MSG(p_user_id.is_empty(), "user_id cannot be empty");

	Dictionary params = p_params;
	params["user_id"] = p_user_id;
	_request_get("moderated_channels_received", "/moderation/channels", params);
}

void TwitchModerationRequests::get_moderators(const String &p_broadcaster_id, const Dictionary &p_params) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");

	Dictionary params = p_params;
	params["broadcaster_id"] = p_broadcaster_id;
	_request_get("moderators_received", "/moderation/moderators", params);
}

void TwitchModerationRequests::add_channel_moderator(const String &p_broadcaster_id, const String &p_user_id) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");
	ERR_FAIL_COND_MSG(p_user_id.is_empty(), "user_id cannot be empty");

	Dictionary params;
	params["broadcaster_id"] = p_broadcaster_id;
	params["user_id"] = p_user_id;

	_request_post("channel_moderator_added", "/moderation/moderators", params, String());
}

void TwitchModerationRequests::remove_channel_moderator(const String &p_broadcaster_id, const String &p_user_id) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");
	ERR_FAIL_COND_MSG(p_user_id.is_empty(), "user_id cannot be empty");

	Dictionary params;
	params["broadcaster_id"] = p_broadcaster_id;
	params["user_id"] = p_user_id;

	_request_delete("channel_moderator_removed", "/moderation/moderators", params);
}

void TwitchModerationRequests::get_vips(const String &p_broadcaster_id, const Dictionary &p_params) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");

	Dictionary params = p_params;
	params["broadcaster_id"] = p_broadcaster_id;
	_request_get("vips_received", "/channels/vips", params);
}

void TwitchModerationRequests::add_channel_vip(const String &p_broadcaster_id, const String &p_user_id) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");
	ERR_FAIL_COND_MSG(p_user_id.is_empty(), "user_id cannot be empty");

	Dictionary params;
	params["broadcaster_id"] = p_broadcaster_id;
	params["user_id"] = p_user_id;

	_request_post("channel_vip_added", "/channels/vips", params, String());
}

void TwitchModerationRequests::remove_channel_vip(const String &p_broadcaster_id, const String &p_user_id) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");
	ERR_FAIL_COND_MSG(p_user_id.is_empty(), "user_id cannot be empty");

	Dictionary params;
	params["broadcaster_id"] = p_broadcaster_id;
	params["user_id"] = p_user_id;

	_request_delete("channel_vip_removed", "/channels/vips", params);
}

void TwitchModerationRequests::update_shield_mode_status(const String &p_broadcaster_id, const String &p_moderator_id, bool p_is_active) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");
	ERR_FAIL_COND_MSG(p_moderator_id.is_empty(), "moderator_id cannot be empty");

	Dictionary params;
	params["broadcaster_id"] = p_broadcaster_id;
	params["moderator_id"] = p_moderator_id;

	Dictionary body_data;
	body_data["is_active"] = p_is_active;
	String body = JSON::stringify(body_data);

	_request_put("shield_mode_status_updated", "/moderation/shield_mode", params, body);
}

void TwitchModerationRequests::get_shield_mode_status(const String &p_broadcaster_id, const String &p_moderator_id) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");
	ERR_FAIL_COND_MSG(p_moderator_id.is_empty(), "moderator_id cannot be empty");

	Dictionary params;
	params["broadcaster_id"] = p_broadcaster_id;
	params["moderator_id"] = p_moderator_id;
	_request_get("shield_mode_status_received", "/moderation/shield_mode", params);
}

void TwitchModerationRequests::warn_chat_user(const String &p_broadcaster_id, const String &p_moderator_id, const String &p_user_id, const String &p_reason) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");
	ERR_FAIL_COND_MSG(p_moderator_id.is_empty(), "moderator_id cannot be empty");
	ERR_FAIL_COND_MSG(p_user_id.is_empty(), "user_id cannot be empty");
	ERR_FAIL_COND_MSG(p_reason.is_empty(), "reason cannot be empty");

	Dictionary params;
	params["broadcaster_id"] = p_broadcaster_id;
	params["moderator_id"] = p_moderator_id;

	Dictionary data;
	data["user_id"] = p_user_id;
	data["reason"] = p_reason;

	Dictionary body_data;
	body_data["data"] = data;
	String body = JSON::stringify(body_data);

	_request_post("chat_user_warned", "/moderation/warnings", params, body);
}
