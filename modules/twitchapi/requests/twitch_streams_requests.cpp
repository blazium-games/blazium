/**************************************************************************/
/*  twitch_streams_requests.cpp                                           */
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

#include "twitch_streams_requests.h"

#include "core/io/json.h"

void TwitchStreamsRequests::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_stream_key", "broadcaster_id"), &TwitchStreamsRequests::get_stream_key);
	ClassDB::bind_method(D_METHOD("get_streams", "params"), &TwitchStreamsRequests::get_streams, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("get_followed_streams", "user_id", "params"), &TwitchStreamsRequests::get_followed_streams, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("create_stream_marker", "user_id", "description"), &TwitchStreamsRequests::create_stream_marker, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("get_stream_markers", "params"), &TwitchStreamsRequests::get_stream_markers);
}

void TwitchStreamsRequests::get_stream_key(const String &p_broadcaster_id) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");

	Dictionary params;
	params["broadcaster_id"] = p_broadcaster_id;
	_request_get("stream_key_received", "/streams/key", params);
}

void TwitchStreamsRequests::get_streams(const Dictionary &p_params) {
	_request_get("streams_received", "/streams", p_params);
}

void TwitchStreamsRequests::get_followed_streams(const String &p_user_id, const Dictionary &p_params) {
	ERR_FAIL_COND_MSG(p_user_id.is_empty(), "user_id cannot be empty");

	Dictionary params = p_params;
	params["user_id"] = p_user_id;
	_request_get("followed_streams_received", "/streams/followed", params);
}

void TwitchStreamsRequests::create_stream_marker(const String &p_user_id, const String &p_description) {
	ERR_FAIL_COND_MSG(p_user_id.is_empty(), "user_id cannot be empty");

	Dictionary body_data;
	body_data["user_id"] = p_user_id;
	if (!p_description.is_empty()) {
		body_data["description"] = p_description;
	}

	String body = JSON::stringify(body_data);
	_request_post("stream_marker_created", "/streams/markers", Dictionary(), body);
}

void TwitchStreamsRequests::get_stream_markers(const Dictionary &p_params) {
	_request_get("stream_markers_received", "/streams/markers", p_params);
}
