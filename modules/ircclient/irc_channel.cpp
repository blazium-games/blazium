/**************************************************************************/
/*  irc_channel.cpp                                                       */
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

#include "irc_channel.h"

void IRCChannel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_name", "name"), &IRCChannel::set_name);
	ClassDB::bind_method(D_METHOD("get_name"), &IRCChannel::get_name);

	ClassDB::bind_method(D_METHOD("set_topic", "topic"), &IRCChannel::set_topic);
	ClassDB::bind_method(D_METHOD("get_topic"), &IRCChannel::get_topic);

	ClassDB::bind_method(D_METHOD("set_topic_setter", "setter"), &IRCChannel::set_topic_setter);
	ClassDB::bind_method(D_METHOD("get_topic_setter"), &IRCChannel::get_topic_setter);

	ClassDB::bind_method(D_METHOD("set_topic_time", "time"), &IRCChannel::set_topic_time);
	ClassDB::bind_method(D_METHOD("get_topic_time"), &IRCChannel::get_topic_time);

	ClassDB::bind_method(D_METHOD("set_modes", "modes"), &IRCChannel::set_modes);
	ClassDB::bind_method(D_METHOD("get_modes"), &IRCChannel::get_modes);

	ClassDB::bind_method(D_METHOD("add_user", "nick", "modes"), &IRCChannel::add_user, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("remove_user", "nick"), &IRCChannel::remove_user);
	ClassDB::bind_method(D_METHOD("has_user", "nick"), &IRCChannel::has_user);
	ClassDB::bind_method(D_METHOD("get_users"), &IRCChannel::get_users);

	ClassDB::bind_method(D_METHOD("set_user_modes", "nick", "modes"), &IRCChannel::set_user_modes);
	ClassDB::bind_method(D_METHOD("get_user_modes", "nick"), &IRCChannel::get_user_modes);
	ClassDB::bind_method(D_METHOD("is_user_operator", "nick"), &IRCChannel::is_user_operator);
	ClassDB::bind_method(D_METHOD("is_user_voiced", "nick"), &IRCChannel::is_user_voiced);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "name"), "set_name", "get_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "topic"), "set_topic", "get_topic");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "topic_setter"), "set_topic_setter", "get_topic_setter");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "topic_time"), "set_topic_time", "get_topic_time");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "modes"), "set_modes", "get_modes");
}

void IRCChannel::set_name(const String &p_name) {
	name = p_name;
}

String IRCChannel::get_name() const {
	return name;
}

void IRCChannel::set_topic(const String &p_topic) {
	topic = p_topic;
}

String IRCChannel::get_topic() const {
	return topic;
}

void IRCChannel::set_topic_setter(const String &p_setter) {
	topic_setter = p_setter;
}

String IRCChannel::get_topic_setter() const {
	return topic_setter;
}

void IRCChannel::set_topic_time(uint64_t p_time) {
	topic_time = p_time;
}

uint64_t IRCChannel::get_topic_time() const {
	return topic_time;
}

void IRCChannel::set_modes(const String &p_modes) {
	modes = p_modes;
}

String IRCChannel::get_modes() const {
	return modes;
}

void IRCChannel::add_user(const String &p_nick, const String &p_modes) {
	user_modes[p_nick] = p_modes;
}

void IRCChannel::remove_user(const String &p_nick) {
	user_modes.erase(p_nick);
}

bool IRCChannel::has_user(const String &p_nick) const {
	return user_modes.has(p_nick);
}

PackedStringArray IRCChannel::get_users() const {
	PackedStringArray users;
	for (const KeyValue<String, String> &kv : user_modes) {
		users.push_back(kv.key);
	}
	return users;
}

void IRCChannel::set_user_modes(const String &p_nick, const String &p_modes) {
	if (user_modes.has(p_nick)) {
		user_modes[p_nick] = p_modes;
	}
}

String IRCChannel::get_user_modes(const String &p_nick) const {
	if (user_modes.has(p_nick)) {
		return user_modes[p_nick];
	}
	return "";
}

bool IRCChannel::is_user_operator(const String &p_nick) const {
	String nick_modes = get_user_modes(p_nick);
	return nick_modes.contains("@") || nick_modes.contains("~") || nick_modes.contains("&");
}

bool IRCChannel::is_user_voiced(const String &p_nick) const {
	String nick_modes = get_user_modes(p_nick);
	return nick_modes.contains("+");
}
