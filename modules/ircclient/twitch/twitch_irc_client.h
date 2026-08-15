/**************************************************************************/
/*  twitch_irc_client.h                                                   */
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

#include "../irc_channel.h"
#include "../irc_client.h"
#include "../irc_message.h"

#include "twitch_message.h"

#include "core/object/ref_counted.h"
#include "core/templates/hash_map.h"

class TwitchIRCClient : public RefCounted {
	GDCLASS(TwitchIRCClient, RefCounted);

private:
	Ref<IRCClient> irc_client;

	// Twitch authentication
	String oauth_token;
	String username;

	// Twitch state
	Dictionary global_user_state;
	HashMap<String, Dictionary> room_states; // channel -> ROOMSTATE tags
	HashMap<String, Dictionary> user_states; // channel -> USERSTATE tags

	// Connection callbacks
	void _on_irc_connected();
	void _on_irc_disconnected(const String &p_reason);
	void _on_irc_message_received(const Ref<IRCMessage> &p_message);

	// Twitch message handlers
	void _handle_twitch_message(const Ref<IRCMessage> &p_message);
	void _handle_globaluserstate(const Ref<IRCMessage> &p_message);
	void _handle_roomstate(const Ref<IRCMessage> &p_message);
	void _handle_userstate(const Ref<IRCMessage> &p_message);
	void _handle_clearmsg(const Ref<IRCMessage> &p_message);
	void _handle_clearchat(const Ref<IRCMessage> &p_message);
	void _handle_hosttarget(const Ref<IRCMessage> &p_message);
	void _handle_reconnect(const Ref<IRCMessage> &p_message);
	void _handle_usernotice(const Ref<IRCMessage> &p_message);
	void _handle_whisper(const Ref<IRCMessage> &p_message);
	void _handle_notice(const Ref<IRCMessage> &p_message);

protected:
	static void _bind_methods();

public:
	// Get underlying IRC client
	Ref<IRCClient> get_irc_client();

	// Twitch connection
	Error connect_to_twitch(const String &p_username, const String &p_oauth_token, bool p_use_ssl = true);
	void disconnect_from_twitch(const String &p_quit_message = "");
	bool is_irc_connected() const;

	// Polling (must be called regularly)
	Error poll();

	// Twitch-specific operations
	void join_channel(const String &p_channel);
	void part_channel(const String &p_channel);
	void send_message(const String &p_channel, const String &p_message);
	void send_action(const String &p_channel, const String &p_action);
	void send_whisper(const String &p_username, const String &p_message);

	// Channel moderation
	void timeout_user(const String &p_channel, const String &p_username, int p_duration, const String &p_reason = "");
	void ban_user(const String &p_channel, const String &p_username, const String &p_reason = "");
	void unban_user(const String &p_channel, const String &p_username);
	void delete_message(const String &p_channel, const String &p_message_id);
	void clear_chat(const String &p_channel);

	// Channel modes
	void set_slow_mode(const String &p_channel, int p_duration);
	void disable_slow_mode(const String &p_channel);
	void set_followers_only(const String &p_channel, int p_duration_minutes);
	void disable_followers_only(const String &p_channel);
	void set_emote_only(const String &p_channel, bool p_enabled);
	void set_subscribers_only(const String &p_channel, bool p_enabled);
	void set_r9k_mode(const String &p_channel, bool p_enabled);

	// State queries
	Dictionary get_global_user_state() const;
	Dictionary get_room_state(const String &p_channel) const;
	Dictionary get_user_state(const String &p_channel) const;

	// Configuration
	void request_twitch_capabilities();
	PackedStringArray get_joined_channels() const;

	TwitchIRCClient();
	~TwitchIRCClient();
};
