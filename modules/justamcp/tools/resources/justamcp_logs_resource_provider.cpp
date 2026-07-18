/**************************************************************************/
/*  justamcp_logs_resource_provider.cpp                                   */
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

#include "justamcp_logs_resource_provider.h"

#include "../../justamcp_pagination.h"
#include "../../justamcp_server.h"
#include "core/io/json.h"

static bool _try_extract_log_cursor(const String &p_uri, const String &p_prefix, String &r_cursor) {
	if (!p_uri.begins_with(p_prefix)) {
		return false;
	}
	const String suffix = p_uri.substr(p_prefix.length());
	if (suffix == "/start" || suffix.is_empty()) {
		r_cursor = String();
		return true;
	}
	if (suffix.begins_with("/cursor/")) {
		r_cursor = justamcp_pagination_cursor_from_uri_suffix(suffix.substr(String("/cursor/").length()));
		return true;
	}
	if (suffix.begins_with("/")) {
		WARN_PRINT_ONCE("JustAMCP: legacy log pagination URI without /cursor/ is deprecated; use .../cursor/{token}.");
		r_cursor = justamcp_pagination_cursor_from_uri_suffix(suffix.substr(1));
		return true;
	}
	return false;
}

static Dictionary _logs_json_contents(const String &p_uri, const Dictionary &p_payload) {
	Dictionary result;
	result["ok"] = true;
	Array contents;
	Dictionary content;
	content["uri"] = p_uri;
	content["mimeType"] = "application/json";
	content["text"] = JSON::stringify(p_payload, "\t");
	contents.push_back(content);
	result["contents"] = contents;
	return result;
}

bool JustAMCPLogsResourceProvider::can_read(const String &p_canonical_uri) {
	String ignored;
	return _try_extract_log_cursor(p_canonical_uri, "blazium://logs/mcp", ignored) ||
			_try_extract_log_cursor(p_canonical_uri, "blazium://logs/recent", ignored) ||
			_try_extract_log_cursor(p_canonical_uri, "blazium://system/logs", ignored);
}

Dictionary JustAMCPLogsResourceProvider::read(const String &p_uri, const String &p_canonical_uri) {
	String log_cursor;
	if (_try_extract_log_cursor(p_canonical_uri, "blazium://logs/mcp", log_cursor)) {
		if (!JustAMCPServer::get_singleton()) {
			Dictionary payload;
			payload["notifications"] = Array();
			payload["count"] = 0;
			return _logs_json_contents(p_uri, payload);
		}
		const Dictionary page = JustAMCPServer::get_singleton()->get_mcp_notification_log_page(log_cursor);
		if (page.has("ok") && !bool(page.get("ok", true))) {
			Dictionary payload;
			payload["error"] = page.get("error", "Invalid pagination cursor.");
			return _logs_json_contents(p_uri, payload);
		}
		Dictionary payload;
		payload["notifications"] = page.get("notifications", Array());
		payload["count"] = Array(payload["notifications"]).size();
		if (page.has("nextCursor")) {
			payload["nextCursor"] = page["nextCursor"];
			payload["nextUri"] = justamcp_pagination_next_uri("blazium://logs/mcp", String(page["nextCursor"]));
		}
		return _logs_json_contents(p_uri, payload);
	}

	if (_try_extract_log_cursor(p_canonical_uri, "blazium://logs/recent", log_cursor)) {
		Dictionary page;
		if (JustAMCPServer::get_singleton()) {
			page = JustAMCPServer::get_singleton()->get_engine_logs_page(log_cursor);
		} else {
			page["ok"] = true;
			page["logs"] = Array();
		}
		if (page.has("ok") && !bool(page.get("ok", true))) {
			Dictionary payload;
			payload["error"] = page.get("error", "Invalid pagination cursor.");
			return _logs_json_contents(p_uri, payload);
		}
		Dictionary payload;
		payload["lines"] = page.get("logs", Array());
		payload["count"] = Array(payload["lines"]).size();
		if (page.has("nextCursor")) {
			payload["nextCursor"] = page["nextCursor"];
			payload["nextUri"] = justamcp_pagination_next_uri("blazium://logs/recent", String(page["nextCursor"]));
		}
		return _logs_json_contents(p_uri, payload);
	}

	if (_try_extract_log_cursor(p_canonical_uri, "blazium://system/logs", log_cursor)) {
		Dictionary page;
		if (JustAMCPServer::get_singleton()) {
			page = JustAMCPServer::get_singleton()->get_engine_logs_page(log_cursor);
		} else {
			page["ok"] = true;
			page["logs"] = Array();
		}
		if (page.has("ok") && !bool(page.get("ok", true))) {
			Dictionary payload;
			payload["error"] = page.get("error", "Invalid pagination cursor.");
			return _logs_json_contents(p_uri, payload);
		}
		Dictionary payload;
		payload["lines"] = page.get("logs", Array());
		payload["count"] = Array(payload["lines"]).size();
		if (page.has("nextCursor")) {
			payload["nextCursor"] = page["nextCursor"];
			payload["nextUri"] = justamcp_pagination_next_uri("blazium://system/logs", String(page["nextCursor"]));
		}
		return _logs_json_contents(p_uri, payload);
	}

	Dictionary payload;
	payload["error"] = "Unsupported logs resource URI";
	return _logs_json_contents(p_uri, payload);
}

#endif
