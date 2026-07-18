/**************************************************************************/
/*  justamcp_resource_project_file.cpp                                    */
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

#include "justamcp_resource_project_file.h"
#include "../../justamcp_read_limits.h"
#include "core/io/file_access.h"

void JustAMCPResourceProjectFile::_bind_methods() {}

JustAMCPResourceProjectFile::JustAMCPResourceProjectFile() {}
JustAMCPResourceProjectFile::~JustAMCPResourceProjectFile() {}

String JustAMCPResourceProjectFile::get_uri() const {
	return "res://{path}";
}

String JustAMCPResourceProjectFile::get_name() const {
	return "Blazium Project File";
}

bool JustAMCPResourceProjectFile::is_template() const {
	return true;
}

Dictionary JustAMCPResourceProjectFile::get_schema() const {
	Dictionary resource;
	resource["uriTemplate"] = get_uri();
	resource["name"] = get_name();
	resource["description"] = "Read any file inside the active res:// Blazium project path.";
	resource["mimeType"] = "text/plain";
	return resource;
}

Dictionary JustAMCPResourceProjectFile::read_resource(const String &p_uri) {
	Dictionary result;

	if (p_uri.begins_with("res://")) {
		if (FileAccess::exists(p_uri)) {
			int64_t file_size = 0;
			if (!justamcp_file_within_read_limit(p_uri, JUSTAMCP_MAX_SYNC_READ_BYTES, file_size)) {
				return justamcp_read_limit_error(p_uri, file_size, JUSTAMCP_MAX_SYNC_READ_BYTES);
			}
			Ref<FileAccess> file = FileAccess::open(p_uri, FileAccess::READ);
			if (file.is_valid()) {
				String raw_text = file->get_as_text();
				result["ok"] = true;

				Array contents;
				Dictionary text_content;
				text_content["uri"] = p_uri;
				text_content["mimeType"] = "text/plain";
				text_content["text"] = raw_text;
				contents.push_back(text_content);

				result["contents"] = contents;
				return result;
			}
		}
		result["ok"] = false;
		result["error_code"] = -32602;
		result["error"] = "File not found or unreadable: " + p_uri;
		return result;
	}

	result["ok"] = false;
	result["error_code"] = -32602;
	result["error"] = "Mismatch URI.";
	return result;
}

#endif
