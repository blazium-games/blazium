/**************************************************************************/
/*  justamcp_prompt.cpp                                                   */
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

#ifdef TOOLS_ENABLED
#include "justamcp_prompt.h"
#include "../../justamcp_mcp_spec.h"
#include "../justamcp_resource_executor.h"
#include "core/object/class_db.h"

void JustAMCPPrompt::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_name"), &JustAMCPPrompt::get_name);
	ClassDB::bind_method(D_METHOD("get_prompt"), &JustAMCPPrompt::get_prompt);
	ClassDB::bind_method(D_METHOD("get_messages", "args"), &JustAMCPPrompt::get_messages);
	ClassDB::bind_method(D_METHOD("complete", "argument"), &JustAMCPPrompt::complete);
}

JustAMCPPrompt::JustAMCPPrompt() {}
JustAMCPPrompt::~JustAMCPPrompt() {}

Dictionary JustAMCPPrompt::_make_text_message(const String &p_text, const String &p_role) {
	Dictionary msg;
	msg["role"] = p_role;

	Dictionary content;
	content["type"] = "text";
	content["text"] = p_text;
	msg["content"] = content;
	return msg;
}

Dictionary JustAMCPPrompt::_make_resource_message(const String &p_uri) {
	JustAMCPResourceExecutor resource_executor;
	Dictionary resource_result = resource_executor.read_resource(p_uri);
	if (!resource_result.get("ok", false)) {
		return _make_text_message("Resource unavailable: " + p_uri + "\nReason: " + String(Variant(resource_result.get("error", "unknown error"))));
	}

	Array contents = resource_result.get("contents", Array());
	if (contents.is_empty()) {
		return _make_text_message("Resource unavailable: " + p_uri + "\nReason: resource returned no contents.");
	}

	Dictionary first_content = contents[0];
	Dictionary resource;
	resource["uri"] = first_content.get("uri", p_uri);
	resource["mimeType"] = first_content.get("mimeType", "text/plain");
	if (first_content.has("text")) {
		resource["text"] = first_content["text"];
	} else if (first_content.has("blob")) {
		resource["blob"] = first_content["blob"];
	} else {
		resource["text"] = "";
	}

	Dictionary content;
	if (justamcp_protocol_supports(justamcp_active_protocol_version(), JUSTAMCP_FEATURE_RESOURCE_LINKS)) {
		content = justamcp_resource_link_content(String(resource.get("uri", p_uri)), p_uri, String(resource.get("mimeType", "")));
	} else {
		content["type"] = "resource";
		content["resource"] = resource;
	}

	Dictionary msg;
	msg["role"] = "user";
	msg["content"] = content;
	return msg;
}

Dictionary JustAMCPPrompt::_make_prompt_argument(const String &p_name, const String &p_description, bool p_required) {
	Dictionary argument;
	argument["name"] = p_name;
	argument["description"] = p_description;
	argument["required"] = p_required;
	return argument;
}

Dictionary JustAMCPPrompt::_make_error_result(const String &p_error, int p_error_code) {
	Dictionary result;
	result["ok"] = false;
	result["error_code"] = p_error_code;
	result["error"] = p_error;
	return result;
}

bool JustAMCPPrompt::_has_non_empty_argument(const Dictionary &p_args, const String &p_name) {
	if (!p_args.has(p_name)) {
		return false;
	}
	Variant value = p_args[p_name];
	if (value.get_type() == Variant::NIL) {
		return false;
	}
	if (value.get_type() == Variant::STRING && String(value).strip_edges().is_empty()) {
		return false;
	}
	return true;
}

Dictionary JustAMCPPrompt::_validate_required_arguments(const Dictionary &p_args, const Vector<String> &p_required) {
	for (int i = 0; i < p_required.size(); i++) {
		if (!_has_non_empty_argument(p_args, p_required[i])) {
			return _make_error_result("Missing required prompt argument: " + p_required[i]);
		}
	}

	Dictionary result;
	result["ok"] = true;
	return result;
}

Dictionary JustAMCPPrompt::_make_completion(const Vector<String> &p_values, const String &p_prefix) {
	Dictionary completion;
	Array values;
	for (int i = 0; i < p_values.size(); i++) {
		if (p_prefix.is_empty() || p_values[i].begins_with(p_prefix)) {
			values.push_back(p_values[i]);
		}
	}
	completion["values"] = values;
	completion["total"] = values.size();
	completion["hasMore"] = false;
	return completion;
}

Dictionary JustAMCPPrompt::_make_empty_completion() {
	Dictionary completion;
	completion["values"] = Array();
	completion["total"] = 0;
	completion["hasMore"] = false;
	return completion;
}

#endif
