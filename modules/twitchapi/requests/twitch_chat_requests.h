/**************************************************************************/
/*  twitch_chat_requests.h                                                */
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

#include "twitch_request_base.h"

class TwitchChatRequests : public TwitchRequestBase {
	GDCLASS(TwitchChatRequests, TwitchRequestBase);

protected:
	static void _bind_methods();

public:
	void get_chatters(const String &p_broadcaster_id, const String &p_moderator_id, const Dictionary &p_params = Dictionary());
	void get_channel_emotes(const String &p_broadcaster_id);
	void get_global_emotes();
	void get_emote_sets(const String &p_emote_set_id);
	void get_channel_chat_badges(const String &p_broadcaster_id);
	void get_global_chat_badges();
	void get_chat_settings(const String &p_broadcaster_id, const String &p_moderator_id = String());
	void get_user_emotes(const String &p_user_id, const String &p_broadcaster_id = String());
	void update_chat_settings(const String &p_broadcaster_id, const String &p_moderator_id, const Dictionary &p_settings);
	void send_chat_announcement(const String &p_broadcaster_id, const String &p_moderator_id, const String &p_message, const String &p_color = String());
	void send_a_shoutout(const String &p_from_broadcaster_id, const String &p_to_broadcaster_id, const String &p_moderator_id);
	void send_chat_message(const String &p_broadcaster_id, const String &p_sender_id, const String &p_message, const String &p_reply_parent_message_id = String());
	void get_user_chat_color(const String &p_user_id);
	void update_user_chat_color(const String &p_user_id, const String &p_color);

	TwitchChatRequests() {}
	~TwitchChatRequests() {}
};
