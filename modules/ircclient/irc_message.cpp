/**************************************************************************/
/*  irc_message.cpp                                                       */
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

#include "irc_message.h"

#include "core/crypto/crypto_core.h"
#include "core/io/json.h"

void IRCMessage::_bind_methods() {
	ClassDB::bind_static_method("IRCMessage", D_METHOD("parse", "raw_message"), &IRCMessage::parse);
	ClassDB::bind_static_method("IRCMessage", D_METHOD("encode_ctcp", "command", "params"), &IRCMessage::encode_ctcp, DEFVAL(""));

	ClassDB::bind_static_method("IRCMessage", D_METHOD("is_base64", "value"), &IRCMessage::is_base64);
	ClassDB::bind_static_method("IRCMessage", D_METHOD("decode_base64_string", "value"), &IRCMessage::decode_base64_string);
	ClassDB::bind_static_method("IRCMessage", D_METHOD("is_json", "value"), &IRCMessage::is_json);
	ClassDB::bind_static_method("IRCMessage", D_METHOD("parse_json_string", "value"), &IRCMessage::parse_json_string);

	// Note: to_string() is inherited from Object and automatically available - don't bind it again

	ClassDB::bind_method(D_METHOD("set_tags", "tags"), &IRCMessage::set_tags);
	ClassDB::bind_method(D_METHOD("get_tags"), &IRCMessage::get_tags);

	ClassDB::bind_method(D_METHOD("set_prefix", "prefix"), &IRCMessage::set_prefix);
	ClassDB::bind_method(D_METHOD("get_prefix"), &IRCMessage::get_prefix);

	ClassDB::bind_method(D_METHOD("set_command", "command"), &IRCMessage::set_command);
	ClassDB::bind_method(D_METHOD("get_command"), &IRCMessage::get_command);

	ClassDB::bind_method(D_METHOD("set_params", "params"), &IRCMessage::set_params);
	ClassDB::bind_method(D_METHOD("get_params"), &IRCMessage::get_params);

	ClassDB::bind_method(D_METHOD("is_numeric"), &IRCMessage::is_numeric);
	ClassDB::bind_method(D_METHOD("get_numeric"), &IRCMessage::get_numeric);

	ClassDB::bind_method(D_METHOD("get_nick"), &IRCMessage::get_nick);
	ClassDB::bind_method(D_METHOD("get_username"), &IRCMessage::get_username);
	ClassDB::bind_method(D_METHOD("get_hostname"), &IRCMessage::get_hostname);

	ClassDB::bind_method(D_METHOD("is_ctcp"), &IRCMessage::is_ctcp);
	ClassDB::bind_method(D_METHOD("get_ctcp_command"), &IRCMessage::get_ctcp_command);
	ClassDB::bind_method(D_METHOD("get_ctcp_params"), &IRCMessage::get_ctcp_params);

	ClassDB::bind_method(D_METHOD("get_processed_tags"), &IRCMessage::get_processed_tags);
	ClassDB::bind_method(D_METHOD("get_tag_value", "key"), &IRCMessage::get_tag_value);

	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "tags"), "set_tags", "get_tags");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "prefix"), "set_prefix", "get_prefix");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "command"), "set_command", "get_command");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "params"), "set_params", "get_params");
}

String IRCMessage::_unescape_tag_value(const String &p_value) {
	String result;
	for (int i = 0; i < p_value.length(); i++) {
		if (p_value[i] == '\\' && i + 1 < p_value.length()) {
			char32_t next = p_value[i + 1];
			switch (next) {
				case ':':
					result += ';';
					break;
				case 's':
					result += ' ';
					break;
				case '\\':
					result += '\\';
					break;
				case 'r':
					result += '\r';
					break;
				case 'n':
					result += '\n';
					break;
				default:
					result += next;
					break;
			}
			i++; // Skip next character
		} else {
			result += p_value[i];
		}
	}
	return result;
}

String IRCMessage::_escape_tag_value(const String &p_value) {
	String result;
	for (int i = 0; i < p_value.length(); i++) {
		char32_t c = p_value[i];
		switch (c) {
			case ';':
				result += "\\:";
				break;
			case ' ':
				result += "\\s";
				break;
			case '\\':
				result += "\\\\";
				break;
			case '\r':
				result += "\\r";
				break;
			case '\n':
				result += "\\n";
				break;
			default:
				result += c;
				break;
		}
	}
	return result;
}

Dictionary IRCMessage::_parse_tags(const String &p_tags_string) {
	Dictionary result;

	// Tags are in format: tag1=value1;tag2=value2;tag3
	PackedStringArray tag_pairs = p_tags_string.split(";", false);

	for (int i = 0; i < tag_pairs.size(); i++) {
		String tag_pair = tag_pairs[i];
		int equals_pos = tag_pair.find("=");

		if (equals_pos != -1) {
			String key = tag_pair.substr(0, equals_pos);
			String value = tag_pair.substr(equals_pos + 1);
			result[key] = _unescape_tag_value(value);
		} else {
			// Tag without value (just presence)
			result[tag_pair] = true;
		}
	}

	return result;
}

Ref<IRCMessage> IRCMessage::parse(const String &p_raw_message) {
	Ref<IRCMessage> message;
	message.instantiate();

	if (p_raw_message.is_empty()) {
		return message;
	}

	// Don't use strip_edges() - it may strip control characters like \x01 (CTCP delimiter)
	String remaining = p_raw_message;
	int pos = 0;

	// Parse IRCv3 tags (@tags)
	if (remaining[pos] == '@') {
		int space_pos = remaining.find(" ");
		if (space_pos == -1) {
			// Malformed message - only tags
			return message;
		}

		String tags_string = remaining.substr(1, space_pos - 1);
		message->tags = _parse_tags(tags_string);

		// Don't strip_edges - preserve control characters
		remaining = remaining.substr(space_pos + 1);
		pos = 0;
	}

	// Parse prefix (:prefix)
	if (!remaining.is_empty() && remaining[pos] == ':') {
		int space_pos = remaining.find(" ");
		if (space_pos == -1) {
			// Malformed message - only prefix
			return message;
		}

		message->prefix = remaining.substr(1, space_pos - 1);
		// Don't strip_edges - preserve control characters
		remaining = remaining.substr(space_pos + 1);
		pos = 0;
	}

	// Parse command
	int space_pos = remaining.find(" ");
	if (space_pos == -1) {
		// Command only, no parameters
		message->command = remaining;
		return message;
	}

	message->command = remaining.substr(0, space_pos);
	// Don't strip_edges - preserve control characters like \x01 (CTCP)
	remaining = remaining.substr(space_pos + 1);

	// Parse parameters
	PackedStringArray params_array;
	while (!remaining.is_empty()) {
		if (remaining[0] == ':') {
			// Trailing parameter - rest of the message (don't strip, preserve all characters including CTCP \x01)
			params_array.push_back(remaining.substr(1));
			break;
		}

		space_pos = remaining.find(" ");
		if (space_pos == -1) {
			// Last parameter
			params_array.push_back(remaining);
			break;
		}

		params_array.push_back(remaining.substr(0, space_pos));
		// Skip the space but don't strip_edges() - preserve control characters like \x01
		remaining = remaining.substr(space_pos + 1);
	}

	message->params = params_array;
	return message;
}

String IRCMessage::to_string() {
	String result;

	// Add tags
	if (!tags.is_empty()) {
		result += "@";
		Array keys = tags.keys();
		for (int i = 0; i < keys.size(); i++) {
			if (i > 0) {
				result += ";";
			}
			String key = keys[i];
			result += key;

			Variant value = tags[key];
			if (value.get_type() != Variant::BOOL || !(bool)value) {
				result += "=" + _escape_tag_value(value.operator String());
			}
		}
		result += " ";
	}

	// Add prefix
	if (!prefix.is_empty()) {
		result += ":" + prefix + " ";
	}

	// Add command
	result += command;

	// Add parameters
	for (int i = 0; i < params.size(); i++) {
		result += " ";
		const String &param = params[i];

		// Last parameter should always be trailing (prefixed with :) for consistency
		// This matches common IRC client behavior and RFC recommendations
		if (i == params.size() - 1) {
			result += ":" + param;
		} else {
			result += param;
		}
	}

	return result;
}

void IRCMessage::set_tags(const Dictionary &p_tags) {
	tags = p_tags;
}

Dictionary IRCMessage::get_tags() const {
	return tags;
}

void IRCMessage::set_prefix(const String &p_prefix) {
	prefix = p_prefix;
}

String IRCMessage::get_prefix() const {
	return prefix;
}

void IRCMessage::set_command(const String &p_command) {
	command = p_command;
}

String IRCMessage::get_command() const {
	return command;
}

void IRCMessage::set_params(const PackedStringArray &p_params) {
	params = p_params;
}

PackedStringArray IRCMessage::get_params() const {
	return params;
}

bool IRCMessage::is_numeric() const {
	if (command.length() != 3) {
		return false;
	}

	for (int i = 0; i < 3; i++) {
		if (!is_digit(command[i])) {
			return false;
		}
	}

	return true;
}

int IRCMessage::get_numeric() const {
	if (!is_numeric()) {
		return -1;
	}

	return command.to_int();
}

String IRCMessage::get_nick() const {
	if (prefix.is_empty()) {
		return "";
	}

	int exclamation_pos = prefix.find("!");
	if (exclamation_pos == -1) {
		// Prefix is just a server name or nick without user@host
		return prefix;
	}

	return prefix.substr(0, exclamation_pos);
}

String IRCMessage::get_username() const {
	int exclamation_pos = prefix.find("!");
	if (exclamation_pos == -1) {
		return "";
	}

	int at_pos = prefix.find("@", exclamation_pos);
	if (at_pos == -1) {
		// No hostname, just nick!user
		return prefix.substr(exclamation_pos + 1);
	}

	return prefix.substr(exclamation_pos + 1, at_pos - exclamation_pos - 1);
}

String IRCMessage::get_hostname() const {
	int at_pos = prefix.find("@");
	if (at_pos == -1) {
		return "";
	}

	return prefix.substr(at_pos + 1);
}

bool IRCMessage::is_ctcp() const {
	if (command != "PRIVMSG" && command != "NOTICE") {
		return false;
	}

	if (params.size() < 2) {
		return false;
	}

	const String &text = params[1];
	return text.length() >= 2 && text[0] == 0x01 && text[text.length() - 1] == 0x01;
}

String IRCMessage::get_ctcp_command() const {
	if (!is_ctcp()) {
		return "";
	}

	const String &text = params[1];
	String ctcp_content = text.substr(1, text.length() - 2); // Remove surrounding 0x01

	int space_pos = ctcp_content.find(" ");
	if (space_pos == -1) {
		return ctcp_content;
	}

	return ctcp_content.substr(0, space_pos);
}

String IRCMessage::get_ctcp_params() const {
	if (!is_ctcp()) {
		return "";
	}

	const String &text = params[1];
	String ctcp_content = text.substr(1, text.length() - 2); // Remove surrounding 0x01

	int space_pos = ctcp_content.find(" ");
	if (space_pos == -1) {
		return "";
	}

	return ctcp_content.substr(space_pos + 1);
}

String IRCMessage::encode_ctcp(const String &p_command, const String &p_params) {
	String result;
	result += (char32_t)0x01;
	result += p_command.to_upper();

	if (!p_params.is_empty()) {
		result += " " + p_params;
	}

	result += (char32_t)0x01;
	return result;
}

bool IRCMessage::is_base64(const String &p_value) {
	if (p_value.is_empty()) {
		return false;
	}

	// Base64 characters are A-Z, a-z, 0-9, +, /, and = for padding
	// Check if string only contains valid base64 characters
	for (int i = 0; i < p_value.length(); i++) {
		char32_t c = p_value[i];
		if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
					(c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=')) {
			return false;
		}
	}

	// Base64 length should be multiple of 4
	if (p_value.length() % 4 != 0) {
		return false;
	}

	// Try to decode - if it fails, it's not valid base64
	int strlen = p_value.length();
	CharString cstr = p_value.ascii();
	Vector<uint8_t> buf;
	buf.resize(strlen / 4 * 3 + 1);
	uint8_t *w = buf.ptrw();
	size_t arr_len = 0;

	return CryptoCore::b64_decode(&w[0], buf.size(), &arr_len, (unsigned char *)cstr.get_data(), strlen) == OK && arr_len > 0;
}

String IRCMessage::decode_base64_string(const String &p_value) {
	int strlen = p_value.length();
	CharString cstr = p_value.ascii();

	Vector<uint8_t> buf;
	buf.resize(strlen / 4 * 3 + 1 + 1); // +1 for null terminator
	uint8_t *w = buf.ptrw();
	size_t arr_len = 0;

	if (CryptoCore::b64_decode(&w[0], buf.size(), &arr_len, (unsigned char *)cstr.get_data(), strlen) != OK) {
		return "";
	}

	w[arr_len] = 0; // Null terminate
	return String::utf8((const char *)&w[0]);
}

bool IRCMessage::is_json(const String &p_value) {
	if (p_value.is_empty()) {
		return false;
	}

	// Quick check: JSON objects start with { or arrays with [
	String trimmed = p_value.strip_edges();
	if (trimmed.is_empty()) {
		return false;
	}

	char32_t first = trimmed[0];
	if (first != '{' && first != '[') {
		return false;
	}

	// Try to parse as JSON
	JSON json;
	Error err = json.parse(p_value);
	return err == OK;
}

Variant IRCMessage::parse_json_string(const String &p_value) {
	JSON json;
	Error err = json.parse(p_value);

	if (err != OK) {
		return Variant(); // Return null variant on error
	}

	return json.get_data();
}

Dictionary IRCMessage::get_processed_tags() const {
	Dictionary processed;

	Array keys = tags.keys();
	for (int i = 0; i < keys.size(); i++) {
		String key = keys[i];
		Variant value = tags[key];

		// Skip non-string values (e.g., boolean tags)
		if (value.get_type() != Variant::STRING) {
			processed[key] = value;
			continue;
		}

		String str_value = value;

		// Check if it's base64
		if (is_base64(str_value)) {
			String decoded = decode_base64_string(str_value);

			// Check if decoded value is JSON
			if (is_json(decoded)) {
				Variant json_data = parse_json_string(decoded);
				processed[key] = json_data;
			} else {
				// Return decoded string
				processed[key] = decoded;
			}
		} else {
			// Return as-is
			processed[key] = str_value;
		}
	}

	return processed;
}

Variant IRCMessage::get_tag_value(const String &p_key) const {
	if (!tags.has(p_key)) {
		return Variant(); // Return null variant if key doesn't exist
	}

	Variant value = tags[p_key];

	// Skip non-string values
	if (value.get_type() != Variant::STRING) {
		return value;
	}

	String str_value = value;

	// Check if it's base64
	if (is_base64(str_value)) {
		String decoded = decode_base64_string(str_value);

		// Check if decoded value is JSON
		if (is_json(decoded)) {
			return parse_json_string(decoded);
		} else {
			// Return decoded string
			return decoded;
		}
	}

	// Return as-is
	return str_value;
}
