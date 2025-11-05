/**************************************************************************/
/*  twitch_analytics_requests.cpp                                         */
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

#include "twitch_analytics_requests.h"

void TwitchAnalyticsRequests::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_extension_analytics", "params"), &TwitchAnalyticsRequests::get_extension_analytics, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("get_game_analytics", "params"), &TwitchAnalyticsRequests::get_game_analytics, DEFVAL(Dictionary()));
}

void TwitchAnalyticsRequests::get_extension_analytics(const Dictionary &p_params) {
	_request_get("extension_analytics_received", "/analytics/extensions", p_params);
}

void TwitchAnalyticsRequests::get_game_analytics(const Dictionary &p_params) {
	_request_get("game_analytics_received", "/analytics/games", p_params);
}
