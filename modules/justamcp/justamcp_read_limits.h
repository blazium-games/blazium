/**************************************************************************/
/*  justamcp_read_limits.h                                                */
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

#pragma once

#include "core/io/file_access.h"
#include "core/variant/dictionary.h"

static const int JUSTAMCP_MAX_SYNC_READ_BYTES = 1048576;

inline bool justamcp_file_within_read_limit(const String &p_path, int p_max_bytes, int64_t &r_size) {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	if (file.is_null()) {
		r_size = -1;
		return false;
	}
	r_size = int64_t(file->get_length());
	file->close();
	return r_size >= 0 && r_size <= p_max_bytes;
}

inline Dictionary justamcp_read_limit_error(const String &p_path, int64_t p_size, int p_max_bytes) {
	Dictionary err;
	err["ok"] = false;
	err["error_code"] = -32602;
	err["error"] = vformat("File exceeds read limit (%d bytes): %s (%d bytes). Use pagination or a smaller scope.", p_max_bytes, p_path, int(p_size));
	err["max_bytes"] = p_max_bytes;
	err["size"] = p_size;
	return err;
}

inline bool justamcp_read_utf8_within_limit(const String &p_path, int p_max_bytes, String &r_text, int64_t &r_size, Dictionary &r_error) {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	if (file.is_null()) {
		r_size = -1;
		r_error["ok"] = false;
		r_error["error_code"] = -32000;
		r_error["error"] = "Cannot open file: " + p_path;
		return false;
	}
	r_size = int64_t(file->get_length());
	if (r_size < 0 || r_size > p_max_bytes) {
		file->close();
		r_error = justamcp_read_limit_error(p_path, r_size, p_max_bytes);
		return false;
	}
	r_text = file->get_as_utf8_string();
	file->close();
	return true;
}
