/**************************************************************************/
/*  kick_events_requests.cpp                                              */
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

#include "kick_events_requests.h"

#include "core/io/json.h"

void KickEventsRequests::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_subscriptions"), &KickEventsRequests::get_subscriptions);
	ClassDB::bind_method(D_METHOD("create_subscriptions", "events", "method", "broadcaster_user_id"),
			&KickEventsRequests::create_subscriptions, DEFVAL("webhook"), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("delete_subscription", "subscription_id"), &KickEventsRequests::delete_subscription);
}

void KickEventsRequests::get_subscriptions() {
	_request_get("event_subscriptions_received", "/events/subscriptions");
}

void KickEventsRequests::create_subscriptions(const Array &p_events, const String &p_method, int p_broadcaster_user_id) {
	ERR_FAIL_COND_MSG(p_events.is_empty(), "Events array cannot be empty");

	Dictionary data;
	data["events"] = p_events;

	if (!p_method.is_empty()) {
		data["method"] = p_method;
	}

	if (p_broadcaster_user_id > 0) {
		data["broadcaster_user_id"] = p_broadcaster_user_id;
	}

	String body = JSON::stringify(data);
	_request_post("event_subscriptions_created", "/events/subscriptions", Dictionary(), body);
}

void KickEventsRequests::delete_subscription(const String &p_subscription_id) {
	ERR_FAIL_COND_MSG(p_subscription_id.is_empty(), "subscription_id cannot be empty");

	String path = "/events/subscriptions/" + p_subscription_id;
	_request_delete("event_subscription_deleted", path);
}
