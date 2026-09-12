/**************************************************************************/
/*  kick_chat_requests.cpp                                                */
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

#include "kick_chat_requests.h"

#include "core/io/json.h"

void KickChatRequests::_bind_methods() {
	ClassDB::bind_method(D_METHOD("send_message", "content", "type", "broadcaster_user_id", "reply_to_message_id"),
			&KickChatRequests::send_message, DEFVAL(0), DEFVAL(String()));
}

void KickChatRequests::send_message(const String &p_content, const String &p_type, int p_broadcaster_user_id, const String &p_reply_to_message_id) {
	ERR_FAIL_COND_MSG(p_content.is_empty(), "Content cannot be empty");
	ERR_FAIL_COND_MSG(p_content.length() > 500, "Content cannot exceed 500 characters");
	ERR_FAIL_COND_MSG(p_type != "user" && p_type != "bot", "Type must be 'user' or 'bot'");

	if (p_type == "user") {
		ERR_FAIL_COND_MSG(p_broadcaster_user_id <= 0, "broadcaster_user_id is required when type is 'user'");
	}

	Dictionary data;
	data["type"] = p_type;
	data["content"] = p_content;

	if (p_broadcaster_user_id > 0) {
		data["broadcaster_user_id"] = p_broadcaster_user_id;
	}

	if (!p_reply_to_message_id.is_empty()) {
		data["reply_to_message_id"] = p_reply_to_message_id;
	}

	String body = JSON::stringify(data);
	_request_post("chat_message_sent", "/chat", Dictionary(), body);
}
