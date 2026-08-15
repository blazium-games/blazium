/**************************************************************************/
/*  justamcp_resource_video_recordings.cpp                                */
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

#include "justamcp_resource_video_recordings.h"
#include "core/io/dir_access.h"
#include "core/io/json.h"

void JustAMCPResourceVideoRecordings::_bind_methods() {}

JustAMCPResourceVideoRecordings::JustAMCPResourceVideoRecordings() {}
JustAMCPResourceVideoRecordings::~JustAMCPResourceVideoRecordings() {}

String JustAMCPResourceVideoRecordings::get_uri() const {
	return "video://recordings";
}

String JustAMCPResourceVideoRecordings::get_name() const {
	return "Video Recordings Cache";
}

bool JustAMCPResourceVideoRecordings::is_template() const {
	return false;
}

Dictionary JustAMCPResourceVideoRecordings::get_schema() const {
	Dictionary result;
	result["uri"] = get_uri();
	result["name"] = get_name();
	result["description"] = "Returns an index of all sequential video snapshot frames captured by the runtime recording hook.";
	result["mimeType"] = "application/json";
	return result;
}

Dictionary JustAMCPResourceVideoRecordings::read_resource(const String &p_uri) {
	Dictionary result;
	Array files;
	Ref<DirAccess> dir = DirAccess::open("res://");
	if (dir.is_valid() && dir->dir_exists(".video_recordings")) {
		Ref<DirAccess> vid_dir = DirAccess::open("res://.video_recordings");
		if (vid_dir.is_valid()) {
			vid_dir->list_dir_begin();
			String file = vid_dir->get_next();
			while (!file.is_empty()) {
				if (!vid_dir->current_is_dir() && file.ends_with(".png")) {
					files.push_back(file);
				}
				file = vid_dir->get_next();
			}
			vid_dir->list_dir_end();
		}

		files.sort();
	}

	Dictionary payload;
	payload["available"] = dir.is_valid() && dir->dir_exists(".video_recordings");
	payload["frames_captured"] = files.size();
	payload["files"] = files;
	payload["path"] = "res://.video_recordings";

	Array contents;
	Dictionary item;
	item["uri"] = p_uri;
	item["mimeType"] = "application/json";
	item["text"] = JSON::stringify(payload);
	contents.push_back(item);

	result["ok"] = true;
	result["contents"] = contents;
	return result;
}

#endif
