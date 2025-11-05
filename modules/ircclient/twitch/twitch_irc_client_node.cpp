/**************************************************************************/
/*  twitch_irc_client_node.cpp                                            */
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

#include "twitch_irc_client_node.h"

void TwitchIRCClientNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_client"), &TwitchIRCClientNode::get_client);

	ClassDB::bind_method(D_METHOD("connect_to_twitch", "username", "oauth_token", "use_ssl"), &TwitchIRCClientNode::connect_to_twitch, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("disconnect_from_twitch", "quit_message"), &TwitchIRCClientNode::disconnect_from_twitch, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("is_irc_connected"), &TwitchIRCClientNode::is_irc_connected);

	ClassDB::bind_method(D_METHOD("join_channel", "channel"), &TwitchIRCClientNode::join_channel);
	ClassDB::bind_method(D_METHOD("part_channel", "channel"), &TwitchIRCClientNode::part_channel);
	ClassDB::bind_method(D_METHOD("send_message", "channel", "message"), &TwitchIRCClientNode::send_message);
	ClassDB::bind_method(D_METHOD("send_action", "channel", "action"), &TwitchIRCClientNode::send_action);
	ClassDB::bind_method(D_METHOD("send_whisper", "username", "message"), &TwitchIRCClientNode::send_whisper);

	ClassDB::bind_method(D_METHOD("timeout_user", "channel", "username", "duration", "reason"), &TwitchIRCClientNode::timeout_user, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("ban_user", "channel", "username", "reason"), &TwitchIRCClientNode::ban_user, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("unban_user", "channel", "username"), &TwitchIRCClientNode::unban_user);
	ClassDB::bind_method(D_METHOD("delete_message", "channel", "message_id"), &TwitchIRCClientNode::delete_message);
	ClassDB::bind_method(D_METHOD("clear_chat", "channel"), &TwitchIRCClientNode::clear_chat);

	ClassDB::bind_method(D_METHOD("set_slow_mode", "channel", "duration"), &TwitchIRCClientNode::set_slow_mode);
	ClassDB::bind_method(D_METHOD("disable_slow_mode", "channel"), &TwitchIRCClientNode::disable_slow_mode);
	ClassDB::bind_method(D_METHOD("set_followers_only", "channel", "duration_minutes"), &TwitchIRCClientNode::set_followers_only);
	ClassDB::bind_method(D_METHOD("disable_followers_only", "channel"), &TwitchIRCClientNode::disable_followers_only);
	ClassDB::bind_method(D_METHOD("set_emote_only", "channel", "enabled"), &TwitchIRCClientNode::set_emote_only);
	ClassDB::bind_method(D_METHOD("set_subscribers_only", "channel", "enabled"), &TwitchIRCClientNode::set_subscribers_only);
	ClassDB::bind_method(D_METHOD("set_r9k_mode", "channel", "enabled"), &TwitchIRCClientNode::set_r9k_mode);

	ClassDB::bind_method(D_METHOD("get_global_user_state"), &TwitchIRCClientNode::get_global_user_state);
	ClassDB::bind_method(D_METHOD("get_room_state", "channel"), &TwitchIRCClientNode::get_room_state);
	ClassDB::bind_method(D_METHOD("get_user_state", "channel"), &TwitchIRCClientNode::get_user_state);
	ClassDB::bind_method(D_METHOD("get_joined_channels"), &TwitchIRCClientNode::get_joined_channels);

	// Forward all Twitch signals
	ADD_SIGNAL(MethodInfo("twitch_ready"));
	ADD_SIGNAL(MethodInfo("twitch_connected"));
	ADD_SIGNAL(MethodInfo("twitch_disconnected", PropertyInfo(Variant::STRING, "reason")));

	ADD_SIGNAL(MethodInfo("twitch_globaluserstate", PropertyInfo(Variant::DICTIONARY, "tags")));
	ADD_SIGNAL(MethodInfo("twitch_roomstate", PropertyInfo(Variant::STRING, "channel"), PropertyInfo(Variant::DICTIONARY, "tags")));
	ADD_SIGNAL(MethodInfo("twitch_userstate", PropertyInfo(Variant::STRING, "channel"), PropertyInfo(Variant::DICTIONARY, "tags")));

	ADD_SIGNAL(MethodInfo("twitch_clearmsg", PropertyInfo(Variant::STRING, "channel"), PropertyInfo(Variant::STRING, "message"), PropertyInfo(Variant::STRING, "target_msg_id"), PropertyInfo(Variant::DICTIONARY, "tags")));
	ADD_SIGNAL(MethodInfo("twitch_clearchat", PropertyInfo(Variant::STRING, "channel"), PropertyInfo(Variant::STRING, "username"), PropertyInfo(Variant::INT, "duration"), PropertyInfo(Variant::DICTIONARY, "tags")));
	ADD_SIGNAL(MethodInfo("twitch_hosttarget", PropertyInfo(Variant::STRING, "hosting_channel"), PropertyInfo(Variant::STRING, "target_channel"), PropertyInfo(Variant::INT, "viewers")));
	ADD_SIGNAL(MethodInfo("twitch_reconnect"));

	ADD_SIGNAL(MethodInfo("twitch_usernotice", PropertyInfo(Variant::STRING, "channel"), PropertyInfo(Variant::STRING, "msg_id"), PropertyInfo(Variant::STRING, "system_msg"), PropertyInfo(Variant::DICTIONARY, "tags")));
	ADD_SIGNAL(MethodInfo("twitch_subscription", PropertyInfo(Variant::STRING, "channel"), PropertyInfo(Variant::STRING, "username"), PropertyInfo(Variant::STRING, "sub_plan"), PropertyInfo(Variant::DICTIONARY, "tags")));
	ADD_SIGNAL(MethodInfo("twitch_raid", PropertyInfo(Variant::STRING, "channel"), PropertyInfo(Variant::STRING, "raider"), PropertyInfo(Variant::INT, "viewer_count"), PropertyInfo(Variant::DICTIONARY, "tags")));

	ADD_SIGNAL(MethodInfo("twitch_notice", PropertyInfo(Variant::STRING, "channel"), PropertyInfo(Variant::STRING, "msg_id"), PropertyInfo(Variant::STRING, "message"), PropertyInfo(Variant::DICTIONARY, "tags")));
	ADD_SIGNAL(MethodInfo("twitch_whisper", PropertyInfo(Variant::STRING, "sender"), PropertyInfo(Variant::STRING, "message"), PropertyInfo(Variant::DICTIONARY, "tags")));

	ADD_SIGNAL(MethodInfo("twitch_message", PropertyInfo(Variant::STRING, "channel"), PropertyInfo(Variant::STRING, "sender"), PropertyInfo(Variant::STRING, "message"), PropertyInfo(Variant::DICTIONARY, "tags")));

	ADD_SIGNAL(MethodInfo("joined", PropertyInfo(Variant::STRING, "channel")));
	ADD_SIGNAL(MethodInfo("parted", PropertyInfo(Variant::STRING, "channel"), PropertyInfo(Variant::STRING, "message")));
	ADD_SIGNAL(MethodInfo("user_joined", PropertyInfo(Variant::STRING, "channel"), PropertyInfo(Variant::STRING, "user")));
	ADD_SIGNAL(MethodInfo("user_parted", PropertyInfo(Variant::STRING, "channel"), PropertyInfo(Variant::STRING, "user")));
}

void TwitchIRCClientNode::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			// Forward all signals from client
			client->connect("twitch_ready", Callable(this, "emit_signal").bind("twitch_ready"));
			client->connect("twitch_connected", Callable(this, "emit_signal").bind("twitch_connected"));
			client->connect("twitch_disconnected", Callable(this, "emit_signal").bind("twitch_disconnected"));

			client->connect("twitch_globaluserstate", Callable(this, "emit_signal").bind("twitch_globaluserstate"));
			client->connect("twitch_roomstate", Callable(this, "emit_signal").bind("twitch_roomstate"));
			client->connect("twitch_userstate", Callable(this, "emit_signal").bind("twitch_userstate"));

			client->connect("twitch_clearmsg", Callable(this, "emit_signal").bind("twitch_clearmsg"));
			client->connect("twitch_clearchat", Callable(this, "emit_signal").bind("twitch_clearchat"));
			client->connect("twitch_hosttarget", Callable(this, "emit_signal").bind("twitch_hosttarget"));
			client->connect("twitch_reconnect", Callable(this, "emit_signal").bind("twitch_reconnect"));

			client->connect("twitch_usernotice", Callable(this, "emit_signal").bind("twitch_usernotice"));
			client->connect("twitch_subscription", Callable(this, "emit_signal").bind("twitch_subscription"));
			client->connect("twitch_raid", Callable(this, "emit_signal").bind("twitch_raid"));

			client->connect("twitch_notice", Callable(this, "emit_signal").bind("twitch_notice"));
			client->connect("twitch_whisper", Callable(this, "emit_signal").bind("twitch_whisper"));

			client->connect("twitch_message", Callable(this, "emit_signal").bind("twitch_message"));

			client->connect("joined", Callable(this, "emit_signal").bind("joined"));
			client->connect("parted", Callable(this, "emit_signal").bind("parted"));
			client->connect("user_joined", Callable(this, "emit_signal").bind("user_joined"));
			client->connect("user_parted", Callable(this, "emit_signal").bind("user_parted"));
		} break;

		case NOTIFICATION_PROCESS: {
			// Automatic polling
			if (client.is_valid()) {
				client->poll();
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			if (client.is_valid() && client->is_irc_connected()) {
				client->disconnect_from_twitch();
			}
		} break;
	}
}

Ref<TwitchIRCClient> TwitchIRCClientNode::get_client() {
	return client;
}

Error TwitchIRCClientNode::connect_to_twitch(const String &p_username, const String &p_oauth_token, bool p_use_ssl) {
	return client->connect_to_twitch(p_username, p_oauth_token, p_use_ssl);
}

void TwitchIRCClientNode::disconnect_from_twitch(const String &p_quit_message) {
	client->disconnect_from_twitch(p_quit_message);
}

bool TwitchIRCClientNode::is_irc_connected() const {
	return client->is_irc_connected();
}

void TwitchIRCClientNode::join_channel(const String &p_channel) {
	client->join_channel(p_channel);
}

void TwitchIRCClientNode::part_channel(const String &p_channel) {
	client->part_channel(p_channel);
}

void TwitchIRCClientNode::send_message(const String &p_channel, const String &p_message) {
	client->send_message(p_channel, p_message);
}

void TwitchIRCClientNode::send_action(const String &p_channel, const String &p_action) {
	client->send_action(p_channel, p_action);
}

void TwitchIRCClientNode::send_whisper(const String &p_username, const String &p_message) {
	client->send_whisper(p_username, p_message);
}

void TwitchIRCClientNode::timeout_user(const String &p_channel, const String &p_username, int p_duration, const String &p_reason) {
	client->timeout_user(p_channel, p_username, p_duration, p_reason);
}

void TwitchIRCClientNode::ban_user(const String &p_channel, const String &p_username, const String &p_reason) {
	client->ban_user(p_channel, p_username, p_reason);
}

void TwitchIRCClientNode::unban_user(const String &p_channel, const String &p_username) {
	client->unban_user(p_channel, p_username);
}

void TwitchIRCClientNode::delete_message(const String &p_channel, const String &p_message_id) {
	client->delete_message(p_channel, p_message_id);
}

void TwitchIRCClientNode::clear_chat(const String &p_channel) {
	client->clear_chat(p_channel);
}

void TwitchIRCClientNode::set_slow_mode(const String &p_channel, int p_duration) {
	client->set_slow_mode(p_channel, p_duration);
}

void TwitchIRCClientNode::disable_slow_mode(const String &p_channel) {
	client->disable_slow_mode(p_channel);
}

void TwitchIRCClientNode::set_followers_only(const String &p_channel, int p_duration_minutes) {
	client->set_followers_only(p_channel, p_duration_minutes);
}

void TwitchIRCClientNode::disable_followers_only(const String &p_channel) {
	client->disable_followers_only(p_channel);
}

void TwitchIRCClientNode::set_emote_only(const String &p_channel, bool p_enabled) {
	client->set_emote_only(p_channel, p_enabled);
}

void TwitchIRCClientNode::set_subscribers_only(const String &p_channel, bool p_enabled) {
	client->set_subscribers_only(p_channel, p_enabled);
}

void TwitchIRCClientNode::set_r9k_mode(const String &p_channel, bool p_enabled) {
	client->set_r9k_mode(p_channel, p_enabled);
}

Dictionary TwitchIRCClientNode::get_global_user_state() const {
	return client->get_global_user_state();
}

Dictionary TwitchIRCClientNode::get_room_state(const String &p_channel) const {
	return client->get_room_state(p_channel);
}

Dictionary TwitchIRCClientNode::get_user_state(const String &p_channel) const {
	return client->get_user_state(p_channel);
}

PackedStringArray TwitchIRCClientNode::get_joined_channels() const {
	return client->get_joined_channels();
}

TwitchIRCClientNode::TwitchIRCClientNode() {
	client.instantiate();
	set_process(true);
}

TwitchIRCClientNode::~TwitchIRCClientNode() {
}
