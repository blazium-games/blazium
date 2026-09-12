/**************************************************************************/
/*  justamcp_resource_ui.cpp                                              */
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

#include "justamcp_resource_ui.h"

#include "../../justamcp_mcp_apps.h"
#include "../../justamcp_read_limits.h"
#include "../justamcp_agent_helpers.h"

#include "core/io/file_access.h"

void JustAMCPResourceUI::_bind_methods() {}

JustAMCPResourceUI::JustAMCPResourceUI() {}
JustAMCPResourceUI::~JustAMCPResourceUI() {}

String JustAMCPResourceUI::get_uri() const {
	return "ui://.blazium/apps/{name}.html";
}

String JustAMCPResourceUI::get_name() const {
	return "JustAMCP MCP App UI";
}

bool JustAMCPResourceUI::is_template() const {
	return true;
}

Dictionary JustAMCPResourceUI::get_schema() const {
	Dictionary resource;
	resource["uriTemplate"] = get_uri();
	resource["name"] = get_name();
	resource["description"] = "Sandboxed MCP App HTML served from ui://.blazium/apps/*.html.";
	resource["mimeType"] = "text/html";
	return resource;
}

Dictionary JustAMCPResourceUI::read_resource(const String &p_uri) {
	Dictionary result;
	String path;
	String error;
	if (!JustAMCPMCPAppsHost::sanitize_ui_uri(p_uri, path, error)) {
		result["ok"] = false;
		result["error"] = error;
		return result;
	}

	if (path == JustAMCPMCPAppsHost::ui_apps_prefix() + "host.html") {
		Array contents;
		Dictionary text_content;
		text_content["uri"] = p_uri;
		text_content["mimeType"] = "text/html";
		text_content["text"] = JustAMCPMCPAppsHost::embedded_host_template();
		contents.push_back(text_content);
		result["ok"] = true;
		result["contents"] = contents;
		return result;
	}

	String file_path = "res://" + path;
	String canon;
	String sandbox_error;
	if (!justamcp_canonical_sandbox_path(file_path, canon, sandbox_error)) {
		result["ok"] = false;
		result["error"] = sandbox_error;
		return result;
	}
	file_path = canon;
	if (!FileAccess::exists(file_path)) {
		result["ok"] = false;
		result["error"] = "UI resource not found.";
		return result;
	}
	int64_t file_size = 0;
	if (!justamcp_file_within_read_limit(file_path, JUSTAMCP_MAX_SYNC_READ_BYTES, file_size)) {
		return justamcp_read_limit_error(file_path, file_size, JUSTAMCP_MAX_SYNC_READ_BYTES);
	}
	Ref<FileAccess> file = FileAccess::open(file_path, FileAccess::READ);
	if (file.is_null()) {
		result["ok"] = false;
		result["error"] = "Failed to open UI resource.";
		return result;
	}

	Array contents;
	Dictionary text_content;
	text_content["uri"] = p_uri;
	text_content["mimeType"] = "text/html";
	text_content["text"] = file->get_as_text();
	contents.push_back(text_content);
	result["ok"] = true;
	result["contents"] = contents;
	return result;
}

#endif
