/**************************************************************************/
/*  twitch_bits_requests.cpp                                              */
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

#include "twitch_bits_requests.h"

void TwitchBitsRequests::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_bits_leaderboard", "params"), &TwitchBitsRequests::get_bits_leaderboard, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("get_cheermotes", "broadcaster_id"), &TwitchBitsRequests::get_cheermotes, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("get_extension_transactions", "extension_id", "params"), &TwitchBitsRequests::get_extension_transactions, DEFVAL(Dictionary()));
}

void TwitchBitsRequests::get_bits_leaderboard(const Dictionary &p_params) {
	_request_get("bits_leaderboard_received", "/bits/leaderboard", p_params);
}

void TwitchBitsRequests::get_cheermotes(const String &p_broadcaster_id) {
	Dictionary params;
	if (!p_broadcaster_id.is_empty()) {
		params["broadcaster_id"] = p_broadcaster_id;
	}
	_request_get("cheermotes_received", "/bits/cheermotes", params);
}

void TwitchBitsRequests::get_extension_transactions(const String &p_extension_id, const Dictionary &p_params) {
	ERR_FAIL_COND_MSG(p_extension_id.is_empty(), "extension_id cannot be empty");

	Dictionary params = p_params;
	params["extension_id"] = p_extension_id;
	_request_get("extension_transactions_received", "/extensions/transactions", params);
}
