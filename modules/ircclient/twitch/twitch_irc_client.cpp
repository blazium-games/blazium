/**************************************************************************/
/*  twitch_irc_client.cpp                                                 */
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

#include "twitch_irc_client.h"

#include "twitch_constants.h"

void TwitchIRCClient::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_irc_client"), &TwitchIRCClient::get_irc_client);

	ClassDB::bind_method(D_METHOD("connect_to_twitch", "username", "oauth_token", "use_ssl"), &TwitchIRCClient::connect_to_twitch, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("disconnect_from_twitch", "quit_message"), &TwitchIRCClient::disconnect_from_twitch, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("is_irc_connected"), &TwitchIRCClient::is_irc_connected);

	ClassDB::bind_method(D_METHOD("poll"), &TwitchIRCClient::poll);

	ClassDB::bind_method(D_METHOD("join_channel", "channel"), &TwitchIRCClient::join_channel);
	ClassDB::bind_method(D_METHOD("part_channel", "channel"), &TwitchIRCClient::part_channel);
	ClassDB::bind_method(D_METHOD("send_message", "channel", "message"), &TwitchIRCClient::send_message);
	ClassDB::bind_method(D_METHOD("send_action", "channel", "action"), &TwitchIRCClient::send_action);
	ClassDB::bind_method(D_METHOD("send_whisper", "username", "message"), &TwitchIRCClient::send_whisper);

	ClassDB::bind_method(D_METHOD("timeout_user", "channel", "username", "duration", "reason"), &TwitchIRCClient::timeout_user, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("ban_user", "channel", "username", "reason"), &TwitchIRCClient::ban_user, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("unban_user", "channel", "username"), &TwitchIRCClient::unban_user);
	ClassDB::bind_method(D_METHOD("delete_message", "channel", "message_id"), &TwitchIRCClient::delete_message);
	ClassDB::bind_method(D_METHOD("clear_chat", "channel"), &TwitchIRCClient::clear_chat);

	ClassDB::bind_method(D_METHOD("set_slow_mode", "channel", "duration"), &TwitchIRCClient::set_slow_mode);
	ClassDB::bind_method(D_METHOD("disable_slow_mode", "channel"), &TwitchIRCClient::disable_slow_mode);
	ClassDB::bind_method(D_METHOD("set_followers_only", "channel", "duration_minutes"), &TwitchIRCClient::set_followers_only);
	ClassDB::bind_method(D_METHOD("disable_followers_only", "channel"), &TwitchIRCClient::disable_followers_only);
	ClassDB::bind_method(D_METHOD("set_emote_only", "channel", "enabled"), &TwitchIRCClient::set_emote_only);
	ClassDB::bind_method(D_METHOD("set_subscribers_only", "channel", "enabled"), &TwitchIRCClient::set_subscribers_only);
	ClassDB::bind_method(D_METHOD("set_r9k_mode", "channel", "enabled"), &TwitchIRCClient::set_r9k_mode);

	ClassDB::bind_method(D_METHOD("get_global_user_state"), &TwitchIRCClient::get_global_user_state);
	ClassDB::bind_method(D_METHOD("get_room_state", "channel"), &TwitchIRCClient::get_room_state);
	ClassDB::bind_method(D_METHOD("get_user_state", "channel"), &TwitchIRCClient::get_user_state);

	ClassDB::bind_method(D_METHOD("request_twitch_capabilities"), &TwitchIRCClient::request_twitch_capabilities);
	ClassDB::bind_method(D_METHOD("get_joined_channels"), &TwitchIRCClient::get_joined_channels);

	// Twitch-specific signals
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

	// Forward standard IRC signals
	ADD_SIGNAL(MethodInfo("joined", PropertyInfo(Variant::STRING, "channel")));
	ADD_SIGNAL(MethodInfo("parted", PropertyInfo(Variant::STRING, "channel"), PropertyInfo(Variant::STRING, "message")));
	ADD_SIGNAL(MethodInfo("user_joined", PropertyInfo(Variant::STRING, "channel"), PropertyInfo(Variant::STRING, "user")));
	ADD_SIGNAL(MethodInfo("user_parted", PropertyInfo(Variant::STRING, "channel"), PropertyInfo(Variant::STRING, "user")));
}

Ref<IRCClient> TwitchIRCClient::get_irc_client() {
	return irc_client;
}

Error TwitchIRCClient::connect_to_twitch(const String &p_username, const String &p_oauth_token, bool p_use_ssl) {
	username = p_username.to_lower(); // Twitch usernames are lowercase
	oauth_token = p_oauth_token;

	// Ensure token has oauth: prefix
	String token = oauth_token;
	if (!token.begins_with("oauth:")) {
		token = "oauth:" + token;
	}

	// Connect to Twitch IRC server
	int port = p_use_ssl ? TwitchIRC::PORT_SSL : TwitchIRC::PORT_NON_SSL;

	Error err = irc_client->connect_to_server(
			TwitchIRC::SERVER_SSL,
			port,
			p_use_ssl,
			username,
			username,
			username,
			token);

	if (err != OK) {
		return err;
	}

	// Request Twitch capabilities automatically
	request_twitch_capabilities();

	return OK;
}

void TwitchIRCClient::disconnect_from_twitch(const String &p_quit_message) {
	irc_client->disconnect_from_server(p_quit_message);
}

bool TwitchIRCClient::is_irc_connected() const {
	return irc_client->is_irc_connected();
}

Error TwitchIRCClient::poll() {
	return irc_client->poll();
}

void TwitchIRCClient::join_channel(const String &p_channel) {
	// Twitch channels should start with #
	String channel = p_channel;
	if (!channel.begins_with("#")) {
		channel = "#" + channel;
	}

	irc_client->join_channel(channel);
}

void TwitchIRCClient::part_channel(const String &p_channel) {
	String channel = p_channel;
	if (!channel.begins_with("#")) {
		channel = "#" + channel;
	}

	irc_client->part_channel(channel);
}

void TwitchIRCClient::send_message(const String &p_channel, const String &p_message) {
	String channel = p_channel;
	if (!channel.begins_with("#")) {
		channel = "#" + channel;
	}

	irc_client->send_privmsg(channel, p_message);
}

void TwitchIRCClient::send_action(const String &p_channel, const String &p_action) {
	String channel = p_channel;
	if (!channel.begins_with("#")) {
		channel = "#" + channel;
	}

	irc_client->send_action(channel, p_action);
}

void TwitchIRCClient::send_whisper(const String &p_username, const String &p_message) {
	// Whispers are sent as PRIVMSG to #jtv or directly
	irc_client->send_privmsg(p_username, p_message);
}

void TwitchIRCClient::timeout_user(const String &p_channel, const String &p_username, int p_duration, const String &p_reason) {
	String channel = p_channel;
	if (!channel.begins_with("#")) {
		channel = "#" + channel;
	}

	String command = "/timeout " + p_username + " " + String::num_int64(p_duration);
	if (!p_reason.is_empty()) {
		command += " " + p_reason;
	}

	irc_client->send_privmsg(channel, command);
}

void TwitchIRCClient::ban_user(const String &p_channel, const String &p_username, const String &p_reason) {
	String channel = p_channel;
	if (!channel.begins_with("#")) {
		channel = "#" + channel;
	}

	String command = "/ban " + p_username;
	if (!p_reason.is_empty()) {
		command += " " + p_reason;
	}

	irc_client->send_privmsg(channel, command);
}

void TwitchIRCClient::unban_user(const String &p_channel, const String &p_username) {
	String channel = p_channel;
	if (!channel.begins_with("#")) {
		channel = "#" + channel;
	}

	irc_client->send_privmsg(channel, "/unban " + p_username);
}

void TwitchIRCClient::delete_message(const String &p_channel, const String &p_message_id) {
	String channel = p_channel;
	if (!channel.begins_with("#")) {
		channel = "#" + channel;
	}

	irc_client->send_privmsg(channel, "/delete " + p_message_id);
}

void TwitchIRCClient::clear_chat(const String &p_channel) {
	String channel = p_channel;
	if (!channel.begins_with("#")) {
		channel = "#" + channel;
	}

	irc_client->send_privmsg(channel, "/clear");
}

void TwitchIRCClient::set_slow_mode(const String &p_channel, int p_duration) {
	String channel = p_channel;
	if (!channel.begins_with("#")) {
		channel = "#" + channel;
	}

	irc_client->send_privmsg(channel, "/slow " + String::num_int64(p_duration));
}

void TwitchIRCClient::disable_slow_mode(const String &p_channel) {
	String channel = p_channel;
	if (!channel.begins_with("#")) {
		channel = "#" + channel;
	}

	irc_client->send_privmsg(channel, "/slowoff");
}

void TwitchIRCClient::set_followers_only(const String &p_channel, int p_duration_minutes) {
	String channel = p_channel;
	if (!channel.begins_with("#")) {
		channel = "#" + channel;
	}

	irc_client->send_privmsg(channel, "/followers " + String::num_int64(p_duration_minutes));
}

void TwitchIRCClient::disable_followers_only(const String &p_channel) {
	String channel = p_channel;
	if (!channel.begins_with("#")) {
		channel = "#" + channel;
	}

	irc_client->send_privmsg(channel, "/followersoff");
}

void TwitchIRCClient::set_emote_only(const String &p_channel, bool p_enabled) {
	String channel = p_channel;
	if (!channel.begins_with("#")) {
		channel = "#" + channel;
	}

	irc_client->send_privmsg(channel, p_enabled ? "/emoteonly" : "/emoteonlyoff");
}

void TwitchIRCClient::set_subscribers_only(const String &p_channel, bool p_enabled) {
	String channel = p_channel;
	if (!channel.begins_with("#")) {
		channel = "#" + channel;
	}

	irc_client->send_privmsg(channel, p_enabled ? "/subscribers" : "/subscribersoff");
}

void TwitchIRCClient::set_r9k_mode(const String &p_channel, bool p_enabled) {
	String channel = p_channel;
	if (!channel.begins_with("#")) {
		channel = "#" + channel;
	}

	irc_client->send_privmsg(channel, p_enabled ? "/r9kbeta" : "/r9kbetaoff");
}

Dictionary TwitchIRCClient::get_global_user_state() const {
	return global_user_state;
}

Dictionary TwitchIRCClient::get_room_state(const String &p_channel) const {
	String channel = p_channel;
	if (!channel.begins_with("#")) {
		channel = "#" + channel;
	}

	if (room_states.has(channel)) {
		return room_states[channel];
	}

	return Dictionary();
}

Dictionary TwitchIRCClient::get_user_state(const String &p_channel) const {
	String channel = p_channel;
	if (!channel.begins_with("#")) {
		channel = "#" + channel;
	}

	if (user_states.has(channel)) {
		return user_states[channel];
	}

	return Dictionary();
}

void TwitchIRCClient::request_twitch_capabilities() {
	// Request all Twitch capabilities
	irc_client->request_capability(TwitchIRC::Capabilities::COMMANDS);
	irc_client->request_capability(TwitchIRC::Capabilities::MEMBERSHIP);
	irc_client->request_capability(TwitchIRC::Capabilities::TAGS);
}

PackedStringArray TwitchIRCClient::get_joined_channels() const {
	return irc_client->get_joined_channels();
}

void TwitchIRCClient::_on_irc_connected() {
	emit_signal("twitch_connected");
}

void TwitchIRCClient::_on_irc_disconnected(const String &p_reason) {
	emit_signal("twitch_disconnected", p_reason);

	// Clear Twitch state
	global_user_state.clear();
	room_states.clear();
	user_states.clear();
}

void TwitchIRCClient::_on_irc_message_received(const Ref<IRCMessage> &p_message) {
	if (TwitchMessage::is_twitch_command(p_message)) {
		_handle_twitch_message(p_message);
	} else {
		// Handle standard IRC messages with Twitch tags
		String command = p_message->get_command();

		if (command == "PRIVMSG") {
			PackedStringArray params = p_message->get_params();
			if (params.size() >= 2) {
				String sender = p_message->get_nick();
				String channel = params[0];
				String message = params[1];
				Dictionary tags = p_message->get_tags();

				emit_signal("twitch_message", channel, sender, message, tags);
			}
		} else if (command == "NOTICE") {
			_handle_notice(p_message);
		} else if (command == "JOIN") {
			PackedStringArray params = p_message->get_params();
			if (params.size() >= 1) {
				String nick = p_message->get_nick();
				String channel = params[0];

				if (nick == username) {
					emit_signal("joined", channel);
				} else {
					emit_signal("user_joined", channel, nick);
				}
			}
		} else if (command == "PART") {
			PackedStringArray params = p_message->get_params();
			if (params.size() >= 1) {
				String nick = p_message->get_nick();
				String channel = params[0];
				String message = params.size() >= 2 ? params[1] : "";

				if (nick == username) {
					emit_signal("parted", channel, message);
				} else {
					emit_signal("user_parted", channel, nick);
				}
			}
		}
	}
}

void TwitchIRCClient::_handle_twitch_message(const Ref<IRCMessage> &p_message) {
	String command = p_message->get_command();

	if (command == TwitchIRC::Commands::GLOBALUSERSTATE) {
		_handle_globaluserstate(p_message);
	} else if (command == TwitchIRC::Commands::ROOMSTATE) {
		_handle_roomstate(p_message);
	} else if (command == TwitchIRC::Commands::USERSTATE) {
		_handle_userstate(p_message);
	} else if (command == TwitchIRC::Commands::CLEARMSG) {
		_handle_clearmsg(p_message);
	} else if (command == TwitchIRC::Commands::CLEARCHAT) {
		_handle_clearchat(p_message);
	} else if (command == TwitchIRC::Commands::HOSTTARGET) {
		_handle_hosttarget(p_message);
	} else if (command == TwitchIRC::Commands::RECONNECT) {
		_handle_reconnect(p_message);
	} else if (command == TwitchIRC::Commands::USERNOTICE) {
		_handle_usernotice(p_message);
	} else if (command == TwitchIRC::Commands::WHISPER) {
		_handle_whisper(p_message);
	}
}

void TwitchIRCClient::_handle_globaluserstate(const Ref<IRCMessage> &p_message) {
	Dictionary tags = p_message->get_tags();
	global_user_state = tags;

	emit_signal("twitch_globaluserstate", tags);
	emit_signal("twitch_ready"); // Twitch is ready after GLOBALUSERSTATE
}

void TwitchIRCClient::_handle_roomstate(const Ref<IRCMessage> &p_message) {
	PackedStringArray params = p_message->get_params();
	if (params.size() >= 1) {
		String channel = params[0];
		Dictionary tags = p_message->get_tags();

		room_states[channel] = tags;
		emit_signal("twitch_roomstate", channel, tags);
	}
}

void TwitchIRCClient::_handle_userstate(const Ref<IRCMessage> &p_message) {
	PackedStringArray params = p_message->get_params();
	if (params.size() >= 1) {
		String channel = params[0];
		Dictionary tags = p_message->get_tags();

		user_states[channel] = tags;
		emit_signal("twitch_userstate", channel, tags);
	}
}

void TwitchIRCClient::_handle_clearmsg(const Ref<IRCMessage> &p_message) {
	PackedStringArray params = p_message->get_params();
	Dictionary tags = p_message->get_tags();

	if (params.size() >= 2) {
		String channel = params[0];
		String message = params[1];
		String target_msg_id = tags.get(TwitchIRC::Tags::TARGET_MSG_ID, "");

		emit_signal("twitch_clearmsg", channel, message, target_msg_id, tags);
	}
}

void TwitchIRCClient::_handle_clearchat(const Ref<IRCMessage> &p_message) {
	PackedStringArray params = p_message->get_params();
	Dictionary tags = p_message->get_tags();

	String channel = params.size() >= 1 ? params[0] : "";
	String username_cleared = params.size() >= 2 ? params[1] : "";

	// If no username, it's a full chat clear
	// If username present, it's a timeout/ban
	int duration = 0;
	if (tags.has("ban-duration")) {
		duration = String(tags["ban-duration"]).to_int();
	}

	emit_signal("twitch_clearchat", channel, username_cleared, duration, tags);
}

void TwitchIRCClient::_handle_hosttarget(const Ref<IRCMessage> &p_message) {
	PackedStringArray params = p_message->get_params();

	if (params.size() >= 2) {
		String hosting_channel = params[0];
		String target_info = params[1];

		// Format: "target_channel viewer_count" or "- viewer_count" to stop hosting
		PackedStringArray parts = target_info.split(" ", false);
		String target_channel = parts.size() >= 1 ? parts[0] : "";
		int viewers = parts.size() >= 2 ? parts[1].to_int() : 0;

		emit_signal("twitch_hosttarget", hosting_channel, target_channel, viewers);
	}
}

void TwitchIRCClient::_handle_reconnect(const Ref<IRCMessage> &p_message) {
	// Twitch is requesting a reconnect
	emit_signal("twitch_reconnect");
}

void TwitchIRCClient::_handle_usernotice(const Ref<IRCMessage> &p_message) {
	PackedStringArray params = p_message->get_params();
	Dictionary tags = p_message->get_tags();

	String channel = params.size() >= 1 ? params[0] : "";
	String msg_id = TwitchMessage::get_msg_id(tags);
	String system_msg = tags.get(TwitchIRC::Tags::SYSTEM_MSG, "");
	String parsed_system_msg = TwitchMessage::parse_system_message(system_msg);

	emit_signal("twitch_usernotice", channel, msg_id, parsed_system_msg, tags);

	// Emit specific signals for common events
	if (msg_id == TwitchIRC::UserNoticeIDs::SUB || msg_id == TwitchIRC::UserNoticeIDs::RESUB) {
		String login_name = tags.get(TwitchIRC::Tags::LOGIN, "");
		String sub_plan = tags.get(TwitchIRC::Tags::MSG_PARAM_SUB_PLAN, "");
		emit_signal("twitch_subscription", channel, login_name, sub_plan, tags);
	} else if (msg_id == TwitchIRC::UserNoticeIDs::RAID) {
		String raider = tags.get(TwitchIRC::Tags::MSG_PARAM_LOGIN, "");
		int viewer_count = String(tags.get(TwitchIRC::Tags::MSG_PARAM_VIEWERCOUNT, "0")).to_int();
		emit_signal("twitch_raid", channel, raider, viewer_count, tags);
	}
}

void TwitchIRCClient::_handle_whisper(const Ref<IRCMessage> &p_message) {
	PackedStringArray params = p_message->get_params();
	Dictionary tags = p_message->get_tags();

	if (params.size() >= 2) {
		String sender = p_message->get_nick();
		String message = params[1];

		emit_signal("twitch_whisper", sender, message, tags);
	}
}

void TwitchIRCClient::_handle_notice(const Ref<IRCMessage> &p_message) {
	PackedStringArray params = p_message->get_params();
	Dictionary tags = p_message->get_tags();

	String channel = params.size() >= 1 ? params[0] : "";
	String message = params.size() >= 2 ? params[1] : "";
	String msg_id = TwitchMessage::get_msg_id(tags);

	emit_signal("twitch_notice", channel, msg_id, message, tags);
}

TwitchIRCClient::TwitchIRCClient() {
	irc_client.instantiate();

	// Connect IRC client signals
	irc_client->connect("connected", callable_mp(this, &TwitchIRCClient::_on_irc_connected));
	irc_client->connect("disconnected", callable_mp(this, &TwitchIRCClient::_on_irc_disconnected));
	irc_client->connect("message_received", callable_mp(this, &TwitchIRCClient::_on_irc_message_received));
}

TwitchIRCClient::~TwitchIRCClient() {
	if (irc_client.is_valid()) {
		irc_client->disconnect_from_server();
	}
}
