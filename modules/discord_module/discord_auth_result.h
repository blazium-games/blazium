/**************************************************************************/
/*  discord_auth_result.h                                                 */
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

#pragma once

#include "core/object/ref_counted.h"

class DiscordAuthResult : public RefCounted {
	GDCLASS(DiscordAuthResult, RefCounted);

	String jwt;
	String discord_id;
	String username;
	int64_t client_id = 0;
	int http_status = 0;
	String error_message;
	bool success = false;

protected:
	static void _bind_methods();

public:
	void set_jwt(const String &p_jwt) { jwt = p_jwt; }
	String get_jwt() const { return jwt; }

	void set_discord_id(const String &p_discord_id) { discord_id = p_discord_id; }
	String get_discord_id() const { return discord_id; }

	void set_username(const String &p_username) { username = p_username; }
	String get_username() const { return username; }

	void set_client_id(int64_t p_client_id) { client_id = p_client_id; }
	int64_t get_client_id() const { return client_id; }

	void set_http_status(int p_status) { http_status = p_status; }
	int get_http_status() const { return http_status; }

	void set_error_message(const String &p_error) { error_message = p_error; }
	String get_error_message() const { return error_message; }

	void set_success(bool p_success) { success = p_success; }
	bool is_success() const { return success; }
};
