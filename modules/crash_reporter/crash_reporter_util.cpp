/**************************************************************************/
/*  crash_reporter_util.cpp                                               */
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

#include "crash_reporter_util.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/os/time.h"
#include <cstring>

namespace CrashReporterUtil {

String report_id_from_dump_path(const String &p_dump_path) {
	return p_dump_path.get_file().get_basename();
}

String sidecar_path_for_dump(const String &p_dump_path) {
	return p_dump_path.get_basename() + ".json";
}

String state_path_for_dump(const String &p_dump_path) {
	return p_dump_path.get_basename() + ".state";
}

String state_to_string(ReportState p_state) {
	switch (p_state) {
		case STATE_UPLOADING:
			return "uploading";
		case STATE_SUBMITTED:
			return "submitted";
		case STATE_FAILED:
			return "failed";
		case STATE_PENDING:
		default:
			return "pending";
	}
}

ReportState state_from_string(const String &p_state) {
	const String s = p_state.strip_edges().to_lower();
	if (s == "uploading") {
		return STATE_UPLOADING;
	}
	if (s == "submitted") {
		return STATE_SUBMITTED;
	}
	if (s == "failed") {
		return STATE_FAILED;
	}
	return STATE_PENDING;
}

Error write_text_file(const String &p_path, const String &p_text) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	ERR_FAIL_COND_V(f.is_null(), ERR_CANT_CREATE);
	f->store_string(p_text);
	return OK;
}

String read_text_file(const String &p_path, Error *r_err) {
	Error err = OK;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);
	if (r_err) {
		*r_err = err;
	}
	if (f.is_null()) {
		return String();
	}
	return f->get_as_utf8_string();
}

Error write_state(const String &p_dump_path, ReportState p_state) {
	return write_text_file(state_path_for_dump(p_dump_path), state_to_string(p_state));
}

ReportState read_state(const String &p_dump_path) {
	Error err = OK;
	const String text = read_text_file(state_path_for_dump(p_dump_path), &err);
	if (err != OK) {
		return STATE_PENDING;
	}
	return state_from_string(text);
}

Dictionary parse_metadata_json(const String &p_json) {
	Ref<JSON> json;
	json.instantiate();
	const Error err = json->parse(p_json);
	if (err != OK || json->get_data().get_type() != Variant::DICTIONARY) {
		return Dictionary();
	}
	return json->get_data();
}

String metadata_to_json(const Dictionary &p_meta) {
	return JSON::stringify(p_meta, "\t", false);
}

static void _append_ascii(Vector<uint8_t> &r_out, const String &p_text) {
	const CharString cs = p_text.utf8();
	const int64_t start = r_out.size();
	r_out.resize(start + cs.length());
	memcpy(r_out.ptrw() + start, cs.get_data(), cs.length());
}

MultipartBody build_multipart(const Vector<uint8_t> &p_dump, const String &p_dump_filename, const String &p_metadata_json, const Vector<uint8_t> &p_log, const String &p_log_filename, const String &p_boundary) {
	MultipartBody body;
	body.content_type = "multipart/form-data; boundary=" + p_boundary;

	_append_ascii(body.data, "--" + p_boundary + "\r\n");
	_append_ascii(body.data, "Content-Disposition: form-data; name=\"metadata\"\r\n");
	_append_ascii(body.data, "Content-Type: application/json\r\n\r\n");
	_append_ascii(body.data, p_metadata_json);
	_append_ascii(body.data, "\r\n");

	_append_ascii(body.data, "--" + p_boundary + "\r\n");
	_append_ascii(body.data, "Content-Disposition: form-data; name=\"dump\"; filename=\"" + p_dump_filename + "\"\r\n");
	_append_ascii(body.data, "Content-Type: application/octet-stream\r\n\r\n");
	{
		const int64_t start = body.data.size();
		body.data.resize(start + p_dump.size());
		if (p_dump.size()) {
			memcpy(body.data.ptrw() + start, p_dump.ptr(), p_dump.size());
		}
	}
	_append_ascii(body.data, "\r\n");

	if (!p_log.is_empty()) {
		_append_ascii(body.data, "--" + p_boundary + "\r\n");
		_append_ascii(body.data, "Content-Disposition: form-data; name=\"log\"; filename=\"" + p_log_filename + "\"\r\n");
		_append_ascii(body.data, "Content-Type: text/plain\r\n\r\n");
		{
			const int64_t start = body.data.size();
			body.data.resize(start + p_log.size());
			memcpy(body.data.ptrw() + start, p_log.ptr(), p_log.size());
		}
		_append_ascii(body.data, "\r\n");
	}

	_append_ascii(body.data, "--" + p_boundary + "--\r\n");
	return body;
}

static uint64_t _file_mtime(const String &p_path) {
	return FileAccess::get_modified_time(p_path);
}

TypedArray<Dictionary> scan_pending_reports(const String &p_crash_dir, int p_max_pending, int p_retain_days) {
	TypedArray<Dictionary> out;
	Ref<DirAccess> dir = DirAccess::open(p_crash_dir);
	if (dir.is_null()) {
		return out;
	}

	const uint64_t now = Time::get_singleton() ? Time::get_singleton()->get_unix_time_from_system() : 0;
	const uint64_t retain_sec = p_retain_days > 0 ? (uint64_t)p_retain_days * 86400 : 0;

	dir->list_dir_begin();
	String fname = dir->get_next();
	Vector<String> dumps;
	while (!fname.is_empty()) {
		if (!dir->current_is_dir() && fname.get_extension().to_lower() == "dmp") {
			dumps.push_back(p_crash_dir.path_join(fname));
		}
		fname = dir->get_next();
	}
	dir->list_dir_end();

	dumps.sort();
	for (int i = 0; i < dumps.size(); i++) {
		const String dump_path = dumps[i];
		if (retain_sec > 0 && now > 0) {
			const uint64_t mtime = _file_mtime(dump_path);
			if (mtime > 0 && now > mtime && (now - mtime) > retain_sec) {
				continue;
			}
		}
		const ReportState st = read_state(dump_path);
		if (st == STATE_SUBMITTED) {
			continue;
		}
		Dictionary row;
		row["id"] = report_id_from_dump_path(dump_path);
		row["dump_path"] = dump_path;
		row["metadata_path"] = sidecar_path_for_dump(dump_path);
		row["state"] = state_to_string(st);
		Error err = OK;
		const String meta_text = read_text_file(sidecar_path_for_dump(dump_path), &err);
		if (err == OK) {
			row["metadata"] = parse_metadata_json(meta_text);
		} else {
			row["metadata"] = Dictionary();
		}
		out.push_back(row);
		if (p_max_pending > 0 && out.size() >= p_max_pending) {
			break;
		}
	}
	return out;
}

void prune_old_reports(const String &p_crash_dir, int p_max_pending, int p_retain_days) {
	Ref<DirAccess> dir = DirAccess::open(p_crash_dir);
	if (dir.is_null()) {
		return;
	}
	dir->list_dir_begin();
	Vector<String> dumps;
	String fname = dir->get_next();
	while (!fname.is_empty()) {
		if (!dir->current_is_dir() && fname.get_extension().to_lower() == "dmp") {
			dumps.push_back(p_crash_dir.path_join(fname));
		}
		fname = dir->get_next();
	}
	dir->list_dir_end();
	dumps.sort();

	const uint64_t now = Time::get_singleton() ? Time::get_singleton()->get_unix_time_from_system() : 0;
	const uint64_t retain_sec = p_retain_days > 0 ? (uint64_t)p_retain_days * 86400 : 0;

	int kept = 0;
	for (int i = dumps.size() - 1; i >= 0; i--) {
		const String dump_path = dumps[i];
		bool drop = false;
		if (retain_sec > 0 && now > 0) {
			const uint64_t mtime = _file_mtime(dump_path);
			if (mtime > 0 && now > mtime && (now - mtime) > retain_sec) {
				drop = true;
			}
		}
		if (!drop) {
			kept++;
			if (p_max_pending > 0 && kept > p_max_pending) {
				drop = true;
			}
		}
		if (drop) {
			discard_report_files(dump_path);
		}
	}
}

Error discard_report_files(const String &p_dump_path) {
	Error err = OK;
	Ref<DirAccess> dir = DirAccess::open(p_dump_path.get_base_dir());
	if (dir.is_null()) {
		return ERR_CANT_OPEN;
	}
	dir->remove(p_dump_path.get_file());
	dir->remove(sidecar_path_for_dump(p_dump_path).get_file());
	dir->remove(state_path_for_dump(p_dump_path).get_file());
	return err;
}

Vector<uint8_t> read_file_capped(const String &p_path, int64_t p_max_bytes, Error *r_err) {
	Error err = OK;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);
	if (r_err) {
		*r_err = err;
	}
	if (f.is_null()) {
		return Vector<uint8_t>();
	}
	const int64_t len = MIN((int64_t)f->get_length(), p_max_bytes);
	Vector<uint8_t> data;
	data.resize(len);
	if (len > 0) {
		f->get_buffer(data.ptrw(), len);
	}
	return data;
}

Vector<uint8_t> read_log_tail(const String &p_path, int p_tail_kb) {
	Error err = OK;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);
	if (f.is_null()) {
		return Vector<uint8_t>();
	}
	const int64_t cap = MAX((int64_t)p_tail_kb, 1) * 1024;
	const int64_t len = f->get_length();
	const int64_t start = len > cap ? (len - cap) : 0;
	f->seek(start);
	Vector<uint8_t> data;
	data.resize(len - start);
	if (!data.is_empty()) {
		f->get_buffer(data.ptrw(), data.size());
	}
	return data;
}

bool sidecar_sha256_matches(const String &p_path, const String &p_expected_sha256) {
	const String expected = p_expected_sha256.strip_edges().to_lower();
	if (expected.is_empty()) {
		return true;
	}
	if (p_path.is_empty() || !FileAccess::exists(p_path)) {
		return false;
	}
	return FileAccess::get_sha256(p_path).to_lower() == expected;
}

} //namespace CrashReporterUtil
