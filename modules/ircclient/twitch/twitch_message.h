/**************************************************************************/
/*  twitch_message.h                                                      */
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

#include "../irc_message.h"

#include "core/object/ref_counted.h"
#include "core/variant/dictionary.h"

class TwitchMessage : public RefCounted {
	GDCLASS(TwitchMessage, RefCounted);

protected:
	static void _bind_methods();

public:
	// Check if message is Twitch-specific command
	static bool is_twitch_command(const Ref<IRCMessage> &p_message);
	static String get_twitch_command(const Ref<IRCMessage> &p_message);

	// Parse Twitch-specific tag formats
	static Dictionary parse_badges(const String &p_badges_string);
	static Dictionary parse_emotes(const String &p_emotes_string);
	static Array parse_emote_sets(const String &p_emote_sets_string);

	// Extract common Twitch tag values
	static String get_user_id(const Dictionary &p_tags);
	static String get_display_name(const Dictionary &p_tags);
	static String get_color(const Dictionary &p_tags);
	static String get_room_id(const Dictionary &p_tags);
	static String get_msg_id(const Dictionary &p_tags);
	static bool is_moderator(const Dictionary &p_tags);
	static bool is_subscriber(const Dictionary &p_tags);
	static bool is_vip(const Dictionary &p_tags);
	static bool is_turbo(const Dictionary &p_tags);

	// Parse system message (with escaped spaces)
	static String parse_system_message(const String &p_system_msg);

	TwitchMessage();
	~TwitchMessage();
};
