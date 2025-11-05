/**************************************************************************/
/*  kick_channels_requests.cpp                                            */
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

#include "kick_channels_requests.h"

#include "core/io/json.h"

void KickChannelsRequests::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_channels", "params"), &KickChannelsRequests::get_channels, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("update_channel", "data"), &KickChannelsRequests::update_channel);
}

void KickChannelsRequests::get_channels(const Dictionary &p_params) {
	// Supports broadcaster_user_id or slug parameters
	_request_get("channels_received", "/channels", p_params);
}

void KickChannelsRequests::update_channel(const Dictionary &p_data) {
	ERR_FAIL_COND_MSG(p_data.is_empty(), "Update data cannot be empty");

	String body = JSON::stringify(p_data);
	_request_patch("channel_updated", "/channels", Dictionary(), body);
}
