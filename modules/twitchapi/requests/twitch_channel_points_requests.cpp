/**************************************************************************/
/*  twitch_channel_points_requests.cpp                                    */
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

#include "twitch_channel_points_requests.h"

#include "core/io/json.h"

void TwitchChannelPointsRequests::_bind_methods() {
	ClassDB::bind_method(D_METHOD("create_custom_rewards", "broadcaster_id", "reward_data"), &TwitchChannelPointsRequests::create_custom_rewards);
	ClassDB::bind_method(D_METHOD("delete_custom_reward", "broadcaster_id", "id"), &TwitchChannelPointsRequests::delete_custom_reward);
	ClassDB::bind_method(D_METHOD("get_custom_reward", "broadcaster_id", "params"), &TwitchChannelPointsRequests::get_custom_reward, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("get_custom_reward_redemption", "broadcaster_id", "reward_id", "params"), &TwitchChannelPointsRequests::get_custom_reward_redemption, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("update_custom_reward", "broadcaster_id", "id", "reward_data"), &TwitchChannelPointsRequests::update_custom_reward);
	ClassDB::bind_method(D_METHOD("update_redemption_status", "broadcaster_id", "reward_id", "id", "status"), &TwitchChannelPointsRequests::update_redemption_status);
}

void TwitchChannelPointsRequests::create_custom_rewards(const String &p_broadcaster_id, const Dictionary &p_reward_data) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");

	Dictionary params;
	params["broadcaster_id"] = p_broadcaster_id;

	String body = JSON::stringify(p_reward_data);
	_request_post("custom_reward_created", "/channel_points/custom_rewards", params, body);
}

void TwitchChannelPointsRequests::delete_custom_reward(const String &p_broadcaster_id, const String &p_id) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");
	ERR_FAIL_COND_MSG(p_id.is_empty(), "id cannot be empty");

	Dictionary params;
	params["broadcaster_id"] = p_broadcaster_id;
	params["id"] = p_id;
	_request_delete("custom_reward_deleted", "/channel_points/custom_rewards", params);
}

void TwitchChannelPointsRequests::get_custom_reward(const String &p_broadcaster_id, const Dictionary &p_params) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");

	Dictionary params = p_params;
	params["broadcaster_id"] = p_broadcaster_id;
	_request_get("custom_reward_received", "/channel_points/custom_rewards", params);
}

void TwitchChannelPointsRequests::get_custom_reward_redemption(const String &p_broadcaster_id, const String &p_reward_id, const Dictionary &p_params) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");
	ERR_FAIL_COND_MSG(p_reward_id.is_empty(), "reward_id cannot be empty");

	Dictionary params = p_params;
	params["broadcaster_id"] = p_broadcaster_id;
	params["reward_id"] = p_reward_id;
	_request_get("custom_reward_redemption_received", "/channel_points/custom_rewards/redemptions", params);
}

void TwitchChannelPointsRequests::update_custom_reward(const String &p_broadcaster_id, const String &p_id, const Dictionary &p_reward_data) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");
	ERR_FAIL_COND_MSG(p_id.is_empty(), "id cannot be empty");

	Dictionary params;
	params["broadcaster_id"] = p_broadcaster_id;
	params["id"] = p_id;

	String body = JSON::stringify(p_reward_data);
	_request_patch("custom_reward_updated", "/channel_points/custom_rewards", params, body);
}

void TwitchChannelPointsRequests::update_redemption_status(const String &p_broadcaster_id, const String &p_reward_id, const String &p_id, const String &p_status) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");
	ERR_FAIL_COND_MSG(p_reward_id.is_empty(), "reward_id cannot be empty");
	ERR_FAIL_COND_MSG(p_id.is_empty(), "id cannot be empty");
	ERR_FAIL_COND_MSG(p_status.is_empty(), "status cannot be empty");

	Dictionary params;
	params["id"] = p_id;
	params["broadcaster_id"] = p_broadcaster_id;
	params["reward_id"] = p_reward_id;

	Dictionary body_data;
	body_data["status"] = p_status;
	String body = JSON::stringify(body_data);

	_request_patch("redemption_status_updated", "/channel_points/custom_rewards/redemptions", params, body);
}
