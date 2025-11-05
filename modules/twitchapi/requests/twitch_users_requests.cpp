/**************************************************************************/
/*  twitch_users_requests.cpp                                             */
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

#include "twitch_users_requests.h"

#include "core/io/json.h"

void TwitchUsersRequests::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_users", "params"), &TwitchUsersRequests::get_users, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("update_user", "description"), &TwitchUsersRequests::update_user);
	ClassDB::bind_method(D_METHOD("get_user_block_list", "broadcaster_id", "params"), &TwitchUsersRequests::get_user_block_list, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("block_user", "target_user_id", "params"), &TwitchUsersRequests::block_user, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("unblock_user", "target_user_id"), &TwitchUsersRequests::unblock_user);
	ClassDB::bind_method(D_METHOD("get_user_extensions"), &TwitchUsersRequests::get_user_extensions);
	ClassDB::bind_method(D_METHOD("get_user_active_extensions", "user_id"), &TwitchUsersRequests::get_user_active_extensions, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("update_user_extensions", "data"), &TwitchUsersRequests::update_user_extensions);
}

void TwitchUsersRequests::get_users(const Dictionary &p_params) {
	_request_get("users_received", "/users", p_params);
}

void TwitchUsersRequests::update_user(const String &p_description) {
	Dictionary params;
	if (!p_description.is_empty()) {
		params["description"] = p_description;
	}
	_request_put("user_updated", "/users", params, String());
}

void TwitchUsersRequests::get_user_block_list(const String &p_broadcaster_id, const Dictionary &p_params) {
	ERR_FAIL_COND_MSG(p_broadcaster_id.is_empty(), "broadcaster_id cannot be empty");

	Dictionary params = p_params;
	params["broadcaster_id"] = p_broadcaster_id;
	_request_get("user_block_list_received", "/users/blocks", params);
}

void TwitchUsersRequests::block_user(const String &p_target_user_id, const Dictionary &p_params) {
	ERR_FAIL_COND_MSG(p_target_user_id.is_empty(), "target_user_id cannot be empty");

	Dictionary params = p_params;
	params["target_user_id"] = p_target_user_id;
	_request_put("user_blocked", "/users/blocks", params, String());
}

void TwitchUsersRequests::unblock_user(const String &p_target_user_id) {
	ERR_FAIL_COND_MSG(p_target_user_id.is_empty(), "target_user_id cannot be empty");

	Dictionary params;
	params["target_user_id"] = p_target_user_id;
	_request_delete("user_unblocked", "/users/blocks", params);
}

void TwitchUsersRequests::get_user_extensions() {
	_request_get("user_extensions_received", "/users/extensions/list");
}

void TwitchUsersRequests::get_user_active_extensions(const String &p_user_id) {
	Dictionary params;
	if (!p_user_id.is_empty()) {
		params["user_id"] = p_user_id;
	}
	_request_get("user_active_extensions_received", "/users/extensions", params);
}

void TwitchUsersRequests::update_user_extensions(const Dictionary &p_data) {
	String body = JSON::stringify(p_data);
	_request_put("user_extensions_updated", "/users/extensions", Dictionary(), body);
}
