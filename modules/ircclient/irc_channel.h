/**************************************************************************/
/*  irc_channel.h                                                         */
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
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/variant/typed_array.h"

class IRCChannel : public RefCounted {
	GDCLASS(IRCChannel, RefCounted);

private:
	String name;
	String topic;
	String topic_setter;
	uint64_t topic_time = 0;
	String modes;

	// User modes in channel (nick -> mode string like "@" for op, "+" for voice)
	HashMap<String, String> user_modes;

protected:
	static void _bind_methods();

public:
	void set_name(const String &p_name);
	String get_name() const;

	void set_topic(const String &p_topic);
	String get_topic() const;

	void set_topic_setter(const String &p_setter);
	String get_topic_setter() const;

	void set_topic_time(uint64_t p_time);
	uint64_t get_topic_time() const;

	void set_modes(const String &p_modes);
	String get_modes() const;

	// User management
	void add_user(const String &p_nick, const String &p_modes = "");
	void remove_user(const String &p_nick);
	bool has_user(const String &p_nick) const;
	PackedStringArray get_users() const;

	// Mode management
	void set_user_modes(const String &p_nick, const String &p_modes);
	String get_user_modes(const String &p_nick) const;
	bool is_user_operator(const String &p_nick) const;
	bool is_user_voiced(const String &p_nick) const;
};
