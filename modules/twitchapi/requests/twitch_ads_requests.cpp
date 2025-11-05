/**************************************************************************/
/*  twitch_ads_requests.cpp                                               */
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

#include "twitch_ads_requests.h"

#include "core/io/json.h"

void TwitchAdsRequests::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start_commercial", "broadcaster_id", "length"), &TwitchAdsRequests::start_commercial);
	ClassDB::bind_method(D_METHOD("get_ad_schedule", "broadcaster_id"), &TwitchAdsRequests::get_ad_schedule);
	ClassDB::bind_method(D_METHOD("snooze_next_ad", "broadcaster_id"), &TwitchAdsRequests::snooze_next_ad);
}

void TwitchAdsRequests::start_commercial(const String &p_broadcaster_id, int p_length) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");
	ERR_FAIL_COND_MSG(p_length != 30 && p_length != 60 && p_length != 90 && p_length != 120 && p_length != 150 && p_length != 180,
			"Commercial length must be 30, 60, 90, 120, 150, or 180 seconds");

	Dictionary body_data;
	body_data["broadcaster_id"] = p_broadcaster_id;
	body_data["length"] = p_length;

	String body = JSON::stringify(body_data);
	_request_post("commercial_started", "/channels/commercial", Dictionary(), body);
}

void TwitchAdsRequests::get_ad_schedule(const String &p_broadcaster_id) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");

	Dictionary params;
	params["broadcaster_id"] = p_broadcaster_id;
	_request_get("ad_schedule_received", "/channels/ads", params);
}

void TwitchAdsRequests::snooze_next_ad(const String &p_broadcaster_id) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");

	Dictionary params;
	params["broadcaster_id"] = p_broadcaster_id;
	_request_post("ad_snoozed", "/channels/ads/schedule/snooze", params, String());
}
