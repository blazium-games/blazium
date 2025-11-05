/**************************************************************************/
/*  irc_message.h                                                         */
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
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

class IRCMessage : public RefCounted {
	GDCLASS(IRCMessage, RefCounted);

private:
	Dictionary tags; // IRCv3 message tags
	String prefix; // Message source (nick!user@host or server)
	String command; // Command or numeric
	PackedStringArray params; // Command parameters

	static String _unescape_tag_value(const String &p_value);
	static String _escape_tag_value(const String &p_value);
	static Dictionary _parse_tags(const String &p_tags_string);

protected:
	static void _bind_methods();

public:
	// Parse a raw IRC message into an IRCMessage object
	static Ref<IRCMessage> parse(const String &p_raw_message);

	// Convert back to wire format
	virtual String to_string() override; // Override Object::to_string() - must match base signature

	// Getters and setters
	void set_tags(const Dictionary &p_tags);
	Dictionary get_tags() const;

	void set_prefix(const String &p_prefix);
	String get_prefix() const;

	void set_command(const String &p_command);
	String get_command() const;

	void set_params(const PackedStringArray &p_params);
	PackedStringArray get_params() const;

	// Helper methods
	bool is_numeric() const;
	int get_numeric() const;

	// Extract components from prefix (nick!user@host)
	String get_nick() const;
	String get_username() const;
	String get_hostname() const;

	// Check if this is a CTCP message
	bool is_ctcp() const;
	String get_ctcp_command() const;
	String get_ctcp_params() const;

	// Create CTCP message
	static String encode_ctcp(const String &p_command, const String &p_params);

	// IRCv3 tag value helpers
	static bool is_base64(const String &p_value);
	static String decode_base64_string(const String &p_value);
	static bool is_json(const String &p_value);
	static Variant parse_json_string(const String &p_value);

	// Get processed tag values (handles base64/JSON automatically)
	Dictionary get_processed_tags() const;
	Variant get_tag_value(const String &p_key) const; // Returns String or Dictionary
};
