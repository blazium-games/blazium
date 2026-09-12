/**************************************************************************/
/*  twitch_chat_requests.cpp                                              */
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

#include "twitch_chat_requests.h"

#include "core/io/json.h"

void TwitchChatRequests::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_chatters", "broadcaster_id", "moderator_id", "params"), &TwitchChatRequests::get_chatters, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("get_channel_emotes", "broadcaster_id"), &TwitchChatRequests::get_channel_emotes);
	ClassDB::bind_method(D_METHOD("get_global_emotes"), &TwitchChatRequests::get_global_emotes);
	ClassDB::bind_method(D_METHOD("get_emote_sets", "emote_set_id"), &TwitchChatRequests::get_emote_sets);
	ClassDB::bind_method(D_METHOD("get_channel_chat_badges", "broadcaster_id"), &TwitchChatRequests::get_channel_chat_badges);
	ClassDB::bind_method(D_METHOD("get_global_chat_badges"), &TwitchChatRequests::get_global_chat_badges);
	ClassDB::bind_method(D_METHOD("get_chat_settings", "broadcaster_id", "moderator_id"), &TwitchChatRequests::get_chat_settings, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("get_user_emotes", "user_id", "broadcaster_id"), &TwitchChatRequests::get_user_emotes, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("update_chat_settings", "broadcaster_id", "moderator_id", "settings"), &TwitchChatRequests::update_chat_settings);
	ClassDB::bind_method(D_METHOD("send_chat_announcement", "broadcaster_id", "moderator_id", "message", "color"), &TwitchChatRequests::send_chat_announcement, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("send_a_shoutout", "from_broadcaster_id", "to_broadcaster_id", "moderator_id"), &TwitchChatRequests::send_a_shoutout);
	ClassDB::bind_method(D_METHOD("send_chat_message", "broadcaster_id", "sender_id", "message", "reply_parent_message_id"), &TwitchChatRequests::send_chat_message, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("get_user_chat_color", "user_id"), &TwitchChatRequests::get_user_chat_color);
	ClassDB::bind_method(D_METHOD("update_user_chat_color", "user_id", "color"), &TwitchChatRequests::update_user_chat_color);
}

void TwitchChatRequests::get_chatters(const String &p_broadcaster_id, const String &p_moderator_id, const Dictionary &p_params) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");
	ERR_FAIL_COND_MSG(p_moderator_id.is_empty(), "moderator_id cannot be empty");

	Dictionary params = p_params;
	params["broadcaster_id"] = p_broadcaster_id;
	params["moderator_id"] = p_moderator_id;
	_request_get("chatters_received", "/chat/chatters", params);
}

void TwitchChatRequests::get_channel_emotes(const String &p_broadcaster_id) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");

	Dictionary params;
	params["broadcaster_id"] = p_broadcaster_id;
	_request_get("channel_emotes_received", "/chat/emotes", params);
}

void TwitchChatRequests::get_global_emotes() {
	_request_get("global_emotes_received", "/chat/emotes/global");
}

void TwitchChatRequests::get_emote_sets(const String &p_emote_set_id) {
	ERR_FAIL_COND_MSG(p_emote_set_id.is_empty(), "emote_set_id cannot be empty");

	Dictionary params;
	params["emote_set_id"] = p_emote_set_id;
	_request_get("emote_sets_received", "/chat/emotes/set", params);
}

void TwitchChatRequests::get_channel_chat_badges(const String &p_broadcaster_id) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");

	Dictionary params;
	params["broadcaster_id"] = p_broadcaster_id;
	_request_get("channel_chat_badges_received", "/chat/badges", params);
}

void TwitchChatRequests::get_global_chat_badges() {
	_request_get("global_chat_badges_received", "/chat/badges/global");
}

void TwitchChatRequests::get_chat_settings(const String &p_broadcaster_id, const String &p_moderator_id) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");

	Dictionary params;
	params["broadcaster_id"] = p_broadcaster_id;
	if (!p_moderator_id.is_empty()) {
		params["moderator_id"] = p_moderator_id;
	}
	_request_get("chat_settings_received", "/chat/settings", params);
}

void TwitchChatRequests::get_user_emotes(const String &p_user_id, const String &p_broadcaster_id) {
	ERR_FAIL_COND_MSG(p_user_id.is_empty(), "user_id cannot be empty");

	Dictionary params;
	params["user_id"] = p_user_id;
	if (!p_broadcaster_id.is_empty()) {
		params["broadcaster_id"] = p_broadcaster_id;
	}
	_request_get("user_emotes_received", "/chat/emotes/user", params);
}

void TwitchChatRequests::update_chat_settings(const String &p_broadcaster_id, const String &p_moderator_id, const Dictionary &p_settings) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");
	ERR_FAIL_COND_MSG(p_moderator_id.is_empty(), "moderator_id cannot be empty");

	Dictionary params;
	params["broadcaster_id"] = p_broadcaster_id;
	params["moderator_id"] = p_moderator_id;

	String body = JSON::stringify(p_settings);
	_request_patch("chat_settings_updated", "/chat/settings", params, body);
}

void TwitchChatRequests::send_chat_announcement(const String &p_broadcaster_id, const String &p_moderator_id, const String &p_message, const String &p_color) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");
	ERR_FAIL_COND_MSG(p_moderator_id.is_empty(), "moderator_id cannot be empty");
	ERR_FAIL_COND_MSG(p_message.is_empty(), "message cannot be empty");

	Dictionary params;
	params["broadcaster_id"] = p_broadcaster_id;
	params["moderator_id"] = p_moderator_id;

	Dictionary body_data;
	body_data["message"] = p_message;
	if (!p_color.is_empty()) {
		body_data["color"] = p_color;
	}

	String body = JSON::stringify(body_data);
	_request_post("chat_announcement_sent", "/chat/announcements", params, body);
}

void TwitchChatRequests::send_a_shoutout(const String &p_from_broadcaster_id, const String &p_to_broadcaster_id, const String &p_moderator_id) {
	ERR_FAIL_COND_MSG(p_from_broadcaster_id.is_empty(), "from_broadcaster_id cannot be empty");
	ERR_FAIL_COND_MSG(p_to_broadcaster_id.is_empty(), "to_broadcaster_id cannot be empty");
	ERR_FAIL_COND_MSG(p_moderator_id.is_empty(), "moderator_id cannot be empty");

	Dictionary params;
	params["from_broadcaster_id"] = p_from_broadcaster_id;
	params["to_broadcaster_id"] = p_to_broadcaster_id;
	params["moderator_id"] = p_moderator_id;

	_request_post("shoutout_sent", "/chat/shoutouts", params, String());
}

void TwitchChatRequests::send_chat_message(const String &p_broadcaster_id, const String &p_sender_id, const String &p_message, const String &p_reply_parent_message_id) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");
	ERR_FAIL_COND_MSG(p_sender_id.is_empty(), "sender_id cannot be empty");
	ERR_FAIL_COND_MSG(p_message.is_empty(), "message cannot be empty");

	Dictionary params;
	params["broadcaster_id"] = p_broadcaster_id;
	params["sender_id"] = p_sender_id;

	Dictionary body_data;
	body_data["message"] = p_message;
	if (!p_reply_parent_message_id.is_empty()) {
		body_data["reply_parent_message_id"] = p_reply_parent_message_id;
	}

	String body = JSON::stringify(body_data);
	_request_post("chat_message_sent", "/chat/messages", params, body);
}

void TwitchChatRequests::get_user_chat_color(const String &p_user_id) {
	ERR_FAIL_COND_MSG(p_user_id.is_empty(), "user_id cannot be empty");

	Dictionary params;
	params["user_id"] = p_user_id;
	_request_get("user_chat_color_received", "/chat/color", params);
}

void TwitchChatRequests::update_user_chat_color(const String &p_user_id, const String &p_color) {
	ERR_FAIL_COND_MSG(p_user_id.is_empty(), "user_id cannot be empty");
	ERR_FAIL_COND_MSG(p_color.is_empty(), "color cannot be empty");

	Dictionary params;
	params["user_id"] = p_user_id;
	params["color"] = p_color;

	_request_put("user_chat_color_updated", "/chat/color", params, String());
}
