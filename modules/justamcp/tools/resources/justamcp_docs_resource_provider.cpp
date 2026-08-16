/**************************************************************************/
/*  justamcp_docs_resource_provider.cpp                                   */
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

#include "justamcp_docs_resource_provider.h"

#include "../justamcp_documentation_tools.h"

#include "core/io/json.h"

static Dictionary _docs_json_contents(const String &p_uri, const Dictionary &p_payload) {
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

static Dictionary _docs_json_error(const String &p_uri, const String &p_error) {
	Dictionary result;
	result["ok"] = false;
	result["error"] = p_error;
	result["uri"] = p_uri;
	return result;
}

bool JustAMCPDocsResourceProvider::can_read(const String &p_canonical_uri) {
	return p_canonical_uri == "blazium://docs/classes" ||
			p_canonical_uri.begins_with("blazium://docs/search/") ||
			p_canonical_uri.begins_with("blazium://docs/class/") ||
			p_canonical_uri.begins_with("blazium://docs/member/");
}

Dictionary JustAMCPDocsResourceProvider::read(const String &p_uri, const String &p_canonical_uri) {
	if (p_canonical_uri == "blazium://docs/classes") {
		JustAMCPDocumentationTools docs;
		Dictionary payload = docs.list_classes(Dictionary());
		if (!payload.get("ok", false)) {
			return _docs_json_error(p_uri, payload.get("error", "Failed to read documentation classes."));
		}
		payload.erase("ok");
		return _docs_json_contents(p_uri, payload);
	}

	if (p_canonical_uri.begins_with("blazium://docs/search/")) {
		const String query = p_canonical_uri.substr(String("blazium://docs/search/").length()).replace("%20", " ");
		JustAMCPDocumentationTools docs;
		Dictionary args;
		args["query"] = query;
		args["limit"] = 50;
		Dictionary payload = docs.search_documentation(args);
		if (!payload.get("ok", false)) {
			return _docs_json_error(p_uri, payload.get("error", "Failed to search documentation."));
		}
		payload.erase("ok");
		return _docs_json_contents(p_uri, payload);
	}

	if (p_canonical_uri.begins_with("blazium://docs/class/")) {
		const String class_name = p_canonical_uri.substr(String("blazium://docs/class/").length());
		JustAMCPDocumentationTools docs;
		Dictionary args;
		args["class_name"] = class_name;
		args["include_members"] = true;
		Dictionary payload = docs.get_class_documentation(args);
		if (!payload.get("ok", false)) {
			return _docs_json_error(p_uri, payload.get("error", "Failed to read class documentation."));
		}
		payload.erase("ok");
		return _docs_json_contents(p_uri, payload);
	}

	if (p_canonical_uri.begins_with("blazium://docs/member/")) {
		const String member_path = p_canonical_uri.substr(String("blazium://docs/member/").length());
		if (member_path.get_slice_count("/") < 3) {
			return _docs_json_error(p_uri, "Member documentation URI requires class_name/member_type/member_name.");
		}
		JustAMCPDocumentationTools docs;
		Dictionary args;
		args["class_name"] = member_path.get_slice("/", 0);
		args["member_type"] = member_path.get_slice("/", 1);
		args["member_name"] = member_path.get_slice("/", 2);
		Dictionary payload = docs.get_member_documentation(args);
		if (!payload.get("ok", false)) {
			return _docs_json_error(p_uri, payload.get("error", "Failed to read member documentation."));
		}
		payload.erase("ok");
		return _docs_json_contents(p_uri, payload);
	}

	return _docs_json_error(p_uri, "Unsupported docs resource URI");
}

#endif
