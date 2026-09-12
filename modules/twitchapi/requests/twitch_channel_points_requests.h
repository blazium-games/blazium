/**************************************************************************/
/*  twitch_channel_points_requests.h                                      */
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

class TwitchChannelPointsRequests : public TwitchRequestBase {
	GDCLASS(TwitchChannelPointsRequests, TwitchRequestBase);

protected:
	static void _bind_methods();

public:
	void create_custom_rewards(const String &p_broadcaster_id, const Dictionary &p_reward_data);
	void delete_custom_reward(const String &p_broadcaster_id, const String &p_id);
	void get_custom_reward(const String &p_broadcaster_id, const Dictionary &p_params = Dictionary());
	void get_custom_reward_redemption(const String &p_broadcaster_id, const String &p_reward_id, const Dictionary &p_params = Dictionary());
	void update_custom_reward(const String &p_broadcaster_id, const String &p_id, const Dictionary &p_reward_data);
	void update_redemption_status(const String &p_broadcaster_id, const String &p_reward_id, const String &p_id, const String &p_status);

	TwitchChannelPointsRequests() {}
	~TwitchChannelPointsRequests() {}
};
