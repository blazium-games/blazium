/**************************************************************************/
/*  analytics_queue.cpp                                                   */
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

#include "analytics_queue.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"

String AnalyticsQueue::events_path(const String &p_dir) {
	return p_dir.path_join("events.jsonl");
}

Error AnalyticsQueue::append(const String &p_dir, const Dictionary &p_event) {
	if (p_dir.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	Error err = DirAccess::make_dir_recursive_absolute(p_dir);
	if (err != OK && err != ERR_ALREADY_EXISTS) {
		return err;
	}
	const String path = events_path(p_dir);
	Ref<FileAccess> f;
	if (FileAccess::exists(path)) {
		f = FileAccess::open(path, FileAccess::READ_WRITE, &err);
		if (f.is_valid()) {
			f->seek_end();
		}
	} else {
		f = FileAccess::open(path, FileAccess::WRITE, &err);
	}
	if (f.is_null()) {
		return err != OK ? err : ERR_CANT_CREATE;
	}
	f->store_line(JSON::stringify(p_event, "", false));
	return OK;
}

TypedArray<Dictionary> AnalyticsQueue::load(const String &p_dir) {
	TypedArray<Dictionary> out;
	const String path = events_path(p_dir);
	if (!FileAccess::exists(path)) {
		return out;
	}
	Error err = OK;
	Ref<FileAccess> f = FileAccess::open(path, FileAccess::READ, &err);
	if (f.is_null()) {
		return out;
	}
	while (!f->eof_reached()) {
		const String line = f->get_line().strip_edges();
		if (line.is_empty()) {
			continue;
		}
		Ref<JSON> json;
		json.instantiate();
		if (json->parse(line) != OK) {
			continue;
		}
		const Variant data = json->get_data();
		if (data.get_type() == Variant::DICTIONARY) {
			out.push_back(data);
		}
	}
	return out;
}

int AnalyticsQueue::size(const String &p_dir) {
	return load(p_dir).size();
}

Error AnalyticsQueue::clear(const String &p_dir) {
	const String path = events_path(p_dir);
	if (!FileAccess::exists(path)) {
		return OK;
	}
	return DirAccess::remove_absolute(path);
}
