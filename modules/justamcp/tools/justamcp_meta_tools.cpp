/**************************************************************************/
/*  justamcp_meta_tools.cpp                                               */
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

#include "justamcp_meta_tools.h"

#include "../justamcp_server.h"
#include "justamcp_resource_executor.h"
#include "justamcp_tool_executor.h"
#include "justamcp_tool_schema_cache.h"
#include "justamcp_toolset_registry.h"

#include "core/config/project_settings.h"

bool JustAMCPMetaTools::handles(const String &p_internal_name) {
	return p_internal_name == "search_tools" ||
			p_internal_name == "execute_tool" ||
			p_internal_name == "get_guide" ||
			p_internal_name == "list_toolsets" ||
			p_internal_name == "describe_toolset" ||
			p_internal_name == "call_toolset";
}

Dictionary JustAMCPMetaTools::execute(JustAMCPToolExecutor *p_executor, const String &p_internal_name, const Dictionary &p_args) {
	Dictionary result;
	ERR_FAIL_NULL_V(p_executor, result);

	if (p_internal_name == "search_tools") {
		const String query = p_args.get("query", "");
		const Array all_schemas = JustAMCPToolSchemaCache::get_schemas(false, true, false, false);
		Array matched;
		for (int i = 0; i < all_schemas.size(); i++) {
			const Dictionary schema = all_schemas[i];
			const String name = schema["name"];
			const String desc = schema["description"];
			if (query.is_empty() || name.containsn(query) || desc.containsn(query)) {
				matched.push_back(schema);
			}
		}
		result["ok"] = true;
		result["result"] = matched;
		return result;
	}
	if (p_internal_name == "execute_tool") {
		const String target_tool = p_args.get("tool_name", "");
		const Dictionary target_args = p_args.get("arguments", Dictionary());
		bool allow_bypass = false;
		if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/justamcp/allow_execute_tool_bypass")) {
			allow_bypass = GLOBAL_GET("blazium/justamcp/allow_execute_tool_bypass");
		}
		if (allow_bypass) {
			return p_executor->execute_tool_direct(target_tool, target_args);
		}
		return p_executor->execute_tool(target_tool, target_args);
	}
	if (p_internal_name == "get_guide") {
		Array topics;
		topics.push_back("testing-loop");
		topics.push_back("scene-editing");
		topics.push_back("asset-generation");
		topics.push_back("troubleshooting");
		topics.push_back("tool-index");
		topics.push_back("asset-tagging");
		const String topic = p_args.get("topic", p_args.get("slug", ""));
		if (topic.is_empty()) {
			result["ok"] = true;
			result["topics"] = topics;
			result["resource_template"] = "blazium://guide/{topic}";
			return result;
		}
		JustAMCPResourceExecutor *resource_executor = nullptr;
		if (JustAMCPServer::get_singleton()) {
			resource_executor = JustAMCPServer::get_singleton()->get_resource_executor();
		}
		Dictionary resource;
		if (resource_executor) {
			resource = resource_executor->read_resource("blazium://guide/" + topic);
		} else {
			JustAMCPResourceExecutor local_executor;
			resource = local_executor.read_resource("blazium://guide/" + topic);
		}
		if (!resource.get("ok", false)) {
			return resource;
		}
		const Array contents = resource.get("contents", Array());
		result["ok"] = true;
		result["topic"] = topic;
		result["contents"] = contents;
		if (!contents.is_empty()) {
			const Dictionary first = contents[0];
			result["text"] = first.get("text", "");
			result["mime_type"] = first.get("mimeType", "text/markdown");
		}
		return result;
	}
	if (p_internal_name == "list_toolsets") {
		if (!JustAMCPToolsetRegistry::get_singleton()) {
			result["ok"] = false;
			result["error"] = "Toolset registry unavailable";
			return result;
		}
		return JustAMCPToolsetRegistry::get_singleton()->list_toolsets();
	}
	if (p_internal_name == "describe_toolset") {
		if (!JustAMCPToolsetRegistry::get_singleton()) {
			result["ok"] = false;
			result["error"] = "Toolset registry unavailable";
			return result;
		}
		return JustAMCPToolsetRegistry::get_singleton()->describe_toolset(p_args.get("toolset_name", ""));
	}
	if (p_internal_name == "call_toolset") {
		if (!JustAMCPToolsetRegistry::get_singleton()) {
			result["ok"] = false;
			result["error"] = "Toolset registry unavailable";
			return result;
		}
		return JustAMCPToolsetRegistry::get_singleton()->call_toolset(
				p_args.get("toolset_name", ""),
				p_args.get("tool_name", ""),
				p_args.get("arguments", Dictionary()));
	}
	return result;
}

#endif
