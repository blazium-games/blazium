/**************************************************************************/
/*  justamcp_script_resource_provider.cpp                                 */
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

#include "justamcp_script_resource_provider.h"

#include "../../justamcp_read_limits.h"
#include "justamcp_resource_json.h"

#include "core/io/file_access.h"

bool JustAMCPScriptResourceProvider::can_read(const String &p_canonical_uri) {
	return p_canonical_uri.begins_with("blazium://script/") || p_canonical_uri.begins_with("godot://script/");
}

Dictionary JustAMCPScriptResourceProvider::read(const String &p_uri, const String &p_canonical_uri) {
	String prefix = p_canonical_uri.begins_with("blazium://script/") ? "blazium://script/" : "godot://script/";
	String script_path = p_canonical_uri.substr(prefix.length());
	if (!script_path.begins_with("res://")) {
		script_path = "res://" + script_path;
	}
	if (!FileAccess::exists(script_path)) {
		return JustAMCPResourceJson::make_json_error(p_uri, "Script not found: " + script_path);
	}
	int64_t file_size = 0;
	if (!justamcp_file_within_read_limit(script_path, JUSTAMCP_MAX_SYNC_READ_BYTES, file_size)) {
		return justamcp_read_limit_error(script_path, file_size, JUSTAMCP_MAX_SYNC_READ_BYTES);
	}
	Ref<FileAccess> file = FileAccess::open(script_path, FileAccess::READ);
	if (file.is_null()) {
		return JustAMCPResourceJson::make_json_error(p_uri, "Cannot read script: " + script_path);
	}

	const String content = file->get_as_text();
	file->close();

	Dictionary payload;
	payload["path"] = script_path;
	payload["content"] = content;
	payload["line_count"] = content.get_slice_count("\n");
	payload["size"] = content.length();
	return JustAMCPResourceJson::make_json_contents(p_uri, payload);
}

#endif
