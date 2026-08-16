/**************************************************************************/
/*  justamcp_mcp_spec.cpp                                                 */
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

#include "justamcp_mcp_spec.h"

static const int JUSTAMCP_TOOL_NAME_MAX_LEN = 64;

Array justamcp_default_icons() {
	Array icons;
	Dictionary icon;
	icon["src"] = "https://blazium.app/icon.png";
	icon["mimeType"] = "image/png";
	Array sizes;
	sizes.push_back("64x64");
	icon["sizes"] = sizes;
	icons.push_back(icon);
	return icons;
}

void justamcp_attach_icons(Dictionary &p_schema) {
	if (!p_schema.has("icons")) {
		p_schema["icons"] = justamcp_default_icons();
	}
}

bool justamcp_is_valid_mcp_tool_name(const String &p_name) {
	if (p_name.is_empty() || p_name.length() > JUSTAMCP_TOOL_NAME_MAX_LEN) {
		return false;
	}
	const char32_t first = p_name[0];
	if (first < 'a' || first > 'z') {
		return false;
	}
	for (int i = 1; i < p_name.length(); i++) {
		const char32_t c = p_name[i];
		const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
		if (!ok) {
			return false;
		}
	}
	return true;
}

String justamcp_invalid_mcp_tool_name_message(const String &p_name) {
	return "Invalid tool name '" + p_name + "'. Tool names must match ^[a-z][a-z0-9_]*$ and be at most 64 characters.";
}

Dictionary justamcp_confirm_enum_schema() {
	Dictionary schema;
	schema["type"] = "object";
	Dictionary properties;
	Dictionary confirmed;
	confirmed["type"] = "string";
	confirmed["title"] = "Confirm";
	confirmed["description"] = "Apply this change?";
	Array values;
	values.push_back("accept");
	values.push_back("decline");
	confirmed["enum"] = values;
	Array names;
	names.push_back("Yes, apply this change");
	names.push_back("No, cancel");
	confirmed["enumNames"] = names;
	confirmed["default"] = "decline";
	properties["confirmed"] = confirmed;
	schema["properties"] = properties;
	Array required;
	required.push_back("confirmed");
	schema["required"] = required;
	return schema;
}

static bool _value_in_enum(const Array &p_enum, const Variant &p_value) {
	for (int i = 0; i < p_enum.size(); i++) {
		if (p_enum[i] == p_value) {
			return true;
		}
	}
	return false;
}

Dictionary justamcp_apply_schema_defaults(const Dictionary &p_schema, const Dictionary &p_content) {
	Dictionary out = p_content.duplicate();
	if (!p_schema.has("properties") || p_schema["properties"].get_type() != Variant::DICTIONARY) {
		return out;
	}
	const Dictionary properties = p_schema["properties"];
	const Array keys = properties.keys();
	for (int i = 0; i < keys.size(); i++) {
		const String key = keys[i];
		if (out.has(key)) {
			continue;
		}
		const Dictionary prop = properties[key];
		if (prop.has("default")) {
			out[key] = prop["default"];
		}
	}
	return out;
}

bool justamcp_validate_elicit_content(const Dictionary &p_schema, const Dictionary &p_content, String &r_error) {
	const Dictionary content = justamcp_apply_schema_defaults(p_schema, p_content);
	if (p_schema.has("required") && p_schema["required"].get_type() == Variant::ARRAY) {
		const Array required = p_schema["required"];
		for (int i = 0; i < required.size(); i++) {
			const String key = required[i];
			if (!content.has(key)) {
				r_error = "Missing required elicitation field: " + key;
				return false;
			}
		}
	}
	if (!p_schema.has("properties") || p_schema["properties"].get_type() != Variant::DICTIONARY) {
		return true;
	}
	const Dictionary properties = p_schema["properties"];
	const Array keys = properties.keys();
	for (int i = 0; i < keys.size(); i++) {
		const String key = keys[i];
		if (!content.has(key)) {
			continue;
		}
		const Dictionary prop = properties[key];
		const Variant value = content[key];
		if (prop.has("enum") && prop["enum"].get_type() == Variant::ARRAY) {
			const Array enums = prop["enum"];
			if (value.get_type() == Variant::ARRAY) {
				const Array selected = value;
				for (int j = 0; j < selected.size(); j++) {
					if (!_value_in_enum(enums, selected[j])) {
						r_error = "Invalid enum value for " + key;
						return false;
					}
				}
			} else if (!_value_in_enum(enums, value)) {
				r_error = "Invalid enum value for " + key;
				return false;
			}
		}
	}
	return true;
}

bool justamcp_parse_elicit_result(const Dictionary &p_result, String &r_action, Dictionary &r_content, String &r_error) {
	r_action = String(p_result.get("action", ""));
	if (r_action != "accept" && r_action != "decline" && r_action != "cancel") {
		r_error = "ElicitResult.action must be accept, decline, or cancel.";
		return false;
	}
	if (p_result.has("content") && p_result["content"].get_type() == Variant::DICTIONARY) {
		r_content = p_result["content"];
	} else {
		r_content = Dictionary();
	}
	return true;
}

bool justamcp_elicit_content_is_confirmed(const Dictionary &p_content) {
	const Variant confirmed = p_content.get("confirmed", false);
	if (confirmed.get_type() == Variant::BOOL) {
		return bool(confirmed);
	}
	const String text = String(confirmed).strip_edges().to_lower();
	return text == "accept" || text == "yes" || text == "true";
}

Dictionary justamcp_url_elicitation_error_rpc(const Variant &p_request_id, const String &p_elicitation_id, const String &p_url, const String &p_message) {
	Dictionary rpc_result;
	rpc_result["jsonrpc"] = "2.0";
	rpc_result["id"] = p_request_id;
	Dictionary error;
	error["code"] = -32042;
	error["message"] = p_message;
	Dictionary error_data;
	error_data["url"] = p_url;
	error_data["elicitationId"] = p_elicitation_id;
	error["data"] = error_data;
	rpc_result["error"] = error;
	return rpc_result;
}
