/**************************************************************************/
/*  kick_moderation_requests.cpp                                          */
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

#include "kick_moderation_requests.h"

#include "core/io/json.h"

void KickModerationRequests::_bind_methods() {
	ClassDB::bind_method(D_METHOD("ban_user", "broadcaster_user_id", "user_id", "reason", "duration"),
			&KickModerationRequests::ban_user, DEFVAL(String()), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("unban_user", "broadcaster_user_id", "user_id"), &KickModerationRequests::unban_user);
}

void KickModerationRequests::ban_user(int p_broadcaster_user_id, int p_user_id, const String &p_reason, int p_duration) {
	ERR_FAIL_COND_MSG(p_broadcaster_user_id <= 0, "broadcaster_user_id must be positive");
	ERR_FAIL_COND_MSG(p_user_id <= 0, "user_id must be positive");

	if (p_duration > 0) {
		ERR_FAIL_COND_MSG(p_duration < 1 || p_duration > 10080, "Duration must be between 1 and 10080 minutes");
	}

	if (!p_reason.is_empty()) {
		ERR_FAIL_COND_MSG(p_reason.length() > 100, "Reason cannot exceed 100 characters");
	}

	Dictionary data;
	data["broadcaster_user_id"] = p_broadcaster_user_id;
	data["user_id"] = p_user_id;

	if (!p_reason.is_empty()) {
		data["reason"] = p_reason;
	}

	if (p_duration > 0) {
		data["duration"] = p_duration;
	}

	String body = JSON::stringify(data);
	_request_post("user_banned", "/moderation/bans", Dictionary(), body);
}

void KickModerationRequests::unban_user(int p_broadcaster_user_id, int p_user_id) {
	ERR_FAIL_COND_MSG(p_broadcaster_user_id <= 0, "broadcaster_user_id must be positive");
	ERR_FAIL_COND_MSG(p_user_id <= 0, "user_id must be positive");

	Dictionary params;
	params["broadcaster_user_id"] = p_broadcaster_user_id;
	params["user_id"] = p_user_id;

	_request_delete("user_unbanned", "/moderation/bans", params);
}
