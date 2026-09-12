/**************************************************************************/
/*  twitch_moderation_requests.h                                          */
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

class TwitchModerationRequests : public TwitchRequestBase {
	GDCLASS(TwitchModerationRequests, TwitchRequestBase);

protected:
	static void _bind_methods();

public:
	void check_automod_status(const String &p_broadcaster_id, const Array &p_messages);
	void manage_held_automod_messages(const String &p_user_id, const String &p_msg_id, const String &p_action);
	void get_automod_settings(const String &p_broadcaster_id, const String &p_moderator_id);
	void update_automod_settings(const String &p_broadcaster_id, const String &p_moderator_id, const Dictionary &p_settings);
	void get_banned_users(const String &p_broadcaster_id, const Dictionary &p_params = Dictionary());
	void ban_user(const String &p_broadcaster_id, const String &p_moderator_id, const Dictionary &p_data);
	void unban_user(const String &p_broadcaster_id, const String &p_moderator_id, const String &p_user_id);
	void get_blocked_terms(const String &p_broadcaster_id, const String &p_moderator_id, const Dictionary &p_params = Dictionary());
	void add_blocked_term(const String &p_broadcaster_id, const String &p_moderator_id, const String &p_text);
	void remove_blocked_term(const String &p_broadcaster_id, const String &p_moderator_id, const String &p_id);
	void delete_chat_messages(const String &p_broadcaster_id, const String &p_moderator_id, const String &p_message_id = String());
	void get_moderated_channels(const String &p_user_id, const Dictionary &p_params = Dictionary());
	void get_moderators(const String &p_broadcaster_id, const Dictionary &p_params = Dictionary());
	void add_channel_moderator(const String &p_broadcaster_id, const String &p_user_id);
	void remove_channel_moderator(const String &p_broadcaster_id, const String &p_user_id);
	void get_vips(const String &p_broadcaster_id, const Dictionary &p_params = Dictionary());
	void add_channel_vip(const String &p_broadcaster_id, const String &p_user_id);
	void remove_channel_vip(const String &p_broadcaster_id, const String &p_user_id);
	void update_shield_mode_status(const String &p_broadcaster_id, const String &p_moderator_id, bool p_is_active);
	void get_shield_mode_status(const String &p_broadcaster_id, const String &p_moderator_id);
	void warn_chat_user(const String &p_broadcaster_id, const String &p_moderator_id, const String &p_user_id, const String &p_reason);

	TwitchModerationRequests() {}
	~TwitchModerationRequests() {}
};
