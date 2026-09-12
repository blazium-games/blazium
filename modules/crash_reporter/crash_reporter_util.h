/**************************************************************************/
/*  crash_reporter_util.h                                                 */
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
#include "core/variant/typed_array.h"

namespace CrashReporterUtil {

enum UploadMode {
	UPLOAD_DISABLED = 0,
	UPLOAD_IN_ENGINE = 1,
	UPLOAD_SIDECAR = 2,
	UPLOAD_BOTH = 3,
};

enum ReportState {
	STATE_PENDING,
	STATE_UPLOADING,
	STATE_SUBMITTED,
	STATE_FAILED,
};

struct MultipartBody {
	Vector<uint8_t> data;
	String content_type;
};

String report_id_from_dump_path(const String &p_dump_path);
String sidecar_path_for_dump(const String &p_dump_path);
String state_path_for_dump(const String &p_dump_path);
String state_to_string(ReportState p_state);
ReportState state_from_string(const String &p_state);

Error write_text_file(const String &p_path, const String &p_text);
String read_text_file(const String &p_path, Error *r_err = nullptr);

Error write_state(const String &p_dump_path, ReportState p_state);
ReportState read_state(const String &p_dump_path);

Dictionary parse_metadata_json(const String &p_json);
String metadata_to_json(const Dictionary &p_meta);

MultipartBody build_multipart(const Vector<uint8_t> &p_dump, const String &p_dump_filename, const String &p_metadata_json, const Vector<uint8_t> &p_log, const String &p_log_filename, const String &p_boundary);

TypedArray<Dictionary> scan_pending_reports(const String &p_crash_dir, int p_max_pending, int p_retain_days);
void prune_old_reports(const String &p_crash_dir, int p_max_pending, int p_retain_days);
Error discard_report_files(const String &p_dump_path);

Vector<uint8_t> read_file_capped(const String &p_path, int64_t p_max_bytes, Error *r_err = nullptr);
Vector<uint8_t> read_log_tail(const String &p_path, int p_tail_kb);

// Empty expected hash means no check. Expected is compared as lowercase hex.
bool sidecar_sha256_matches(const String &p_path, const String &p_expected_sha256);

} //namespace CrashReporterUtil
