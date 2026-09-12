/**************************************************************************/
/*  irc_user.cpp                                                          */
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

#include "irc_user.h"

void IRCUser::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_nick", "nick"), &IRCUser::set_nick);
	ClassDB::bind_method(D_METHOD("get_nick"), &IRCUser::get_nick);

	ClassDB::bind_method(D_METHOD("set_username", "username"), &IRCUser::set_username);
	ClassDB::bind_method(D_METHOD("get_username"), &IRCUser::get_username);

	ClassDB::bind_method(D_METHOD("set_hostname", "hostname"), &IRCUser::set_hostname);
	ClassDB::bind_method(D_METHOD("get_hostname"), &IRCUser::get_hostname);

	ClassDB::bind_method(D_METHOD("set_realname", "realname"), &IRCUser::set_realname);
	ClassDB::bind_method(D_METHOD("get_realname"), &IRCUser::get_realname);

	ClassDB::bind_method(D_METHOD("set_account", "account"), &IRCUser::set_account);
	ClassDB::bind_method(D_METHOD("get_account"), &IRCUser::get_account);

	ClassDB::bind_method(D_METHOD("set_away_message", "message"), &IRCUser::set_away_message);
	ClassDB::bind_method(D_METHOD("get_away_message"), &IRCUser::get_away_message);

	ClassDB::bind_method(D_METHOD("set_is_away", "away"), &IRCUser::set_is_away);
	ClassDB::bind_method(D_METHOD("get_is_away"), &IRCUser::get_is_away);

	ClassDB::bind_method(D_METHOD("get_prefix"), &IRCUser::get_prefix);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "nick"), "set_nick", "get_nick");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "username"), "set_username", "get_username");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "hostname"), "set_hostname", "get_hostname");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "realname"), "set_realname", "get_realname");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "account"), "set_account", "get_account");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "away_message"), "set_away_message", "get_away_message");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_away"), "set_is_away", "get_is_away");
}

void IRCUser::set_nick(const String &p_nick) {
	nick = p_nick;
}

String IRCUser::get_nick() const {
	return nick;
}

void IRCUser::set_username(const String &p_username) {
	username = p_username;
}

String IRCUser::get_username() const {
	return username;
}

void IRCUser::set_hostname(const String &p_hostname) {
	hostname = p_hostname;
}

String IRCUser::get_hostname() const {
	return hostname;
}

void IRCUser::set_realname(const String &p_realname) {
	realname = p_realname;
}

String IRCUser::get_realname() const {
	return realname;
}

void IRCUser::set_account(const String &p_account) {
	account = p_account;
}

String IRCUser::get_account() const {
	return account;
}

void IRCUser::set_away_message(const String &p_message) {
	away_message = p_message;
	is_away = !p_message.is_empty();
}

String IRCUser::get_away_message() const {
	return away_message;
}

void IRCUser::set_is_away(bool p_away) {
	is_away = p_away;
	if (!is_away) {
		away_message = "";
	}
}

bool IRCUser::get_is_away() const {
	return is_away;
}

String IRCUser::get_prefix() const {
	if (username.is_empty() || hostname.is_empty()) {
		return nick;
	}

	return nick + "!" + username + "@" + hostname;
}
