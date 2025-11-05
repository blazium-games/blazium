/**************************************************************************/
/*  irc_user.h                                                            */
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

class IRCUser : public RefCounted {
	GDCLASS(IRCUser, RefCounted);

private:
	String nick;
	String username;
	String hostname;
	String realname;
	String account; // IRCv3 account name
	String away_message;
	bool is_away = false;

protected:
	static void _bind_methods();

public:
	void set_nick(const String &p_nick);
	String get_nick() const;

	void set_username(const String &p_username);
	String get_username() const;

	void set_hostname(const String &p_hostname);
	String get_hostname() const;

	void set_realname(const String &p_realname);
	String get_realname() const;

	void set_account(const String &p_account);
	String get_account() const;

	void set_away_message(const String &p_message);
	String get_away_message() const;

	void set_is_away(bool p_away);
	bool get_is_away() const;

	// Get full prefix (nick!user@host)
	String get_prefix() const;
};
