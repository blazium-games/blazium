/**************************************************************************/
/*  justamcp_json_rpc_helpers.cpp                                         */
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

#include "justamcp_json_rpc_helpers.h"

#ifdef TOOLS_ENABLED
#include "justamcp_tool_schema_cache.h"
#endif

bool JustAMCPJsonRpcHelpers::request_ids_equal(const Variant &p_a, const Variant &p_b) {
	if (p_a == p_b) {
		return true;
	}
	if (p_a.get_type() == Variant::FLOAT && p_b.get_type() == Variant::INT) {
		return int64_t(Math::round(double(p_a))) == int64_t(p_b);
	}
	if (p_a.get_type() == Variant::INT && p_b.get_type() == Variant::FLOAT) {
		return int64_t(p_a) == int64_t(Math::round(double(p_b)));
	}
	return String(p_a) == String(p_b);
}

String JustAMCPJsonRpcHelpers::request_id_to_string(const Variant &p_request_id) {
	if (p_request_id.get_type() == Variant::NIL) {
		return String();
	}
	return Variant::get_type_name(p_request_id.get_type()) + ":" + String(p_request_id);
}

Dictionary JustAMCPJsonRpcHelpers::extract_request_meta(const Dictionary &p_params) {
	if (p_params.has("_meta") && p_params["_meta"].get_type() == Variant::DICTIONARY) {
		return p_params["_meta"];
	}
	return Dictionary();
}

String JustAMCPJsonRpcHelpers::progress_token_from_meta(const Dictionary &p_meta) {
	if (!p_meta.has("progressToken")) {
		return String();
	}
	const Variant token_var = p_meta["progressToken"];
	if (token_var.get_type() == Variant::STRING) {
		return token_var;
	}
	if (token_var.get_type() == Variant::INT || token_var.get_type() == Variant::FLOAT) {
		return String(token_var);
	}
	return String();
}

String JustAMCPJsonRpcHelpers::get_tool_task_support(const String &p_tool_name) {
#ifdef TOOLS_ENABLED
	Dictionary schema = JustAMCPToolSchemaCache::find_tool_schema(p_tool_name, true);
	if (!schema.is_empty()) {
		if (schema.has("execution") && schema["execution"].get_type() == Variant::DICTIONARY) {
			const Dictionary execution = schema["execution"];
			return String(execution.get("taskSupport", "forbidden"));
		}
		return "forbidden";
	}
#else
	(void)p_tool_name;
#endif
	return "forbidden";
}

#ifdef TOOLS_ENABLED

#include "core/io/json.h"

static bool mcp_tool_settings_dirty = false;

Dictionary JustAMCPJsonRpcHelpers::format_tool_result(bool p_success, const Variant &p_result, const String &p_error) {
	Dictionary rpc_result;
	rpc_result["jsonrpc"] = "2.0";

	if (p_success) {
		Dictionary result;
		if (p_result.get_type() == Variant::DICTIONARY) {
			Dictionary payload = p_result;
			if (payload.has("structuredContent")) {
				result["structuredContent"] = payload["structuredContent"];
			}
			if (payload.has("content")) {
				Variant content_val = payload["content"];
				if (content_val.get_type() == Variant::ARRAY) {
					result["content"] = content_val;
				} else if (content_val.get_type() == Variant::STRING) {
					Array content;
					Dictionary content_item;
					content_item["type"] = "text";
					content_item["text"] = content_val;
					content.push_back(content_item);
					result["content"] = content;
				} else {
					Array content;
					Dictionary content_item;
					content_item["type"] = "text";
					content_item["text"] = JSON::stringify(content_val);
					content.push_back(content_item);
					result["content"] = content;
				}
				result["isError"] = payload.get("isError", false);
			} else if (!payload.has("structuredContent")) {
				Array content;
				Dictionary content_item;
				content_item["type"] = "text";
				content_item["text"] = JSON::stringify(payload);
				content.push_back(content_item);
				result["content"] = content;
				result["isError"] = false;
				result["structuredContent"] = payload;
			}
		} else {
			Array content;
			Dictionary content_item;
			content_item["type"] = "text";
			if (p_result.get_type() == Variant::STRING) {
				content_item["text"] = p_result;
			} else {
				content_item["text"] = JSON::stringify(p_result);
			}
			content.push_back(content_item);
			result["content"] = content;
			result["isError"] = false;
		}
		rpc_result["result"] = result;
	} else {
		Dictionary error_dict = p_result.get_type() == Variant::DICTIONARY ? Dictionary(p_result) : Dictionary();
		if (!error_dict.is_empty() && error_dict.has("code")) {
			Dictionary error;
			error["code"] = error_dict["code"];
			error["message"] = error_dict.get("message", p_error);
			rpc_result["error"] = error;
		} else {
			Dictionary result;
			Array content;
			Dictionary content_item;
			content_item["type"] = "text";
			content_item["text"] = p_error;
			content.push_back(content_item);
			result["content"] = content;
			result["isError"] = true;
			rpc_result["result"] = result;
		}
	}
	return rpc_result;
}

void JustAMCPJsonRpcHelpers::mark_mcp_tool_settings_dirty() {
	mcp_tool_settings_dirty = true;
	JustAMCPToolSchemaCache::mark_all_cached_categories_dirty();
}

bool JustAMCPJsonRpcHelpers::should_broadcast_tools_list_changed() {
	if (!mcp_tool_settings_dirty) {
		return false;
	}
	mcp_tool_settings_dirty = false;
	return true;
}

#endif
