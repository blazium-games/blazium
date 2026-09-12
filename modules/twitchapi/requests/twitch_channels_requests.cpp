/**************************************************************************/
/*  twitch_channels_requests.cpp                                          */
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

#include "twitch_channels_requests.h"

#include "core/io/json.h"

void TwitchChannelsRequests::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_channel_information", "broadcaster_id"), &TwitchChannelsRequests::get_channel_information);
	ClassDB::bind_method(D_METHOD("modify_channel_information", "broadcaster_id", "data"), &TwitchChannelsRequests::modify_channel_information);
	ClassDB::bind_method(D_METHOD("get_channel_editors", "broadcaster_id"), &TwitchChannelsRequests::get_channel_editors);
	ClassDB::bind_method(D_METHOD("get_followed_channels", "user_id", "params"), &TwitchChannelsRequests::get_followed_channels, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("get_channel_followers", "broadcaster_id", "params"), &TwitchChannelsRequests::get_channel_followers, DEFVAL(Dictionary()));
}

void TwitchChannelsRequests::get_channel_information(const String &p_broadcaster_id) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");

	Dictionary params;
	params["broadcaster_id"] = p_broadcaster_id;
	_request_get("channel_information_received", "/channels", params);
}

void TwitchChannelsRequests::modify_channel_information(const String &p_broadcaster_id, const Dictionary &p_data) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");

	Dictionary params;
	params["broadcaster_id"] = p_broadcaster_id;

	String body = JSON::stringify(p_data);
	_request_patch("channel_information_modified", "/channels", params, body);
}

void TwitchChannelsRequests::get_channel_editors(const String &p_broadcaster_id) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");

	Dictionary params;
	params["broadcaster_id"] = p_broadcaster_id;
	_request_get("channel_editors_received", "/channels/editors", params);
}

void TwitchChannelsRequests::get_followed_channels(const String &p_user_id, const Dictionary &p_params) {
	ERR_FAIL_COND_MSG(p_user_id.is_empty(), "user_id cannot be empty");

	Dictionary params = p_params;
	params["user_id"] = p_user_id;
	_request_get("followed_channels_received", "/channels/followed", params);
}

void TwitchChannelsRequests::get_channel_followers(const String &p_broadcaster_id, const Dictionary &p_params) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");

	Dictionary params = p_params;
	params["broadcaster_id"] = p_broadcaster_id;
	_request_get("channel_followers_received", "/channels/followers", params);
}
