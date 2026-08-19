/**************************************************************************/
/*  crash_reporter.h                                                      */
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

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/templates/safe_refcount.h"
#include "core/variant/typed_array.h"

class CrashReporter : public Object {
	GDCLASS(CrashReporter, Object);

public:
	enum UploadMode {
		UPLOAD_DISABLED = 0,
		UPLOAD_IN_ENGINE = 1,
		UPLOAD_SIDECAR = 2,
		UPLOAD_BOTH = 3,
	};

private:
	static CrashReporter *singleton;

	bool uploading = false;
	bool upload_cancel = false;
	String uploading_id;
	SafeFlag poll_scheduled;

	void _schedule_poll();
	void _poll_upload();
	void _startup_actions();
	void _print_console_dump(const String &p_path);
	String _resolve_env_or_baked(const String &p_env, const String &p_baked, const String &p_fallback) const;
	String _setting_string(const String &p_key, const String &p_fallback) const;
	bool _setting_bool(const String &p_key, bool p_fallback) const;
	int _setting_int(const String &p_key, int p_fallback) const;
	String _resolve_reporter_path() const;
	void _cache_breakpad_identity();
	void _enrich_pending_metadata();
	Error _launch_reporter_internal(const String &p_report_id);

protected:
	static void _bind_methods();

public:
	static CrashReporter *get_singleton();

	void report_user_data_dir_ready();

	bool is_available() const;
	bool is_breakpad_enabled() const;
	bool is_http_upload_available() const;

	String get_crash_directory() const;
	TypedArray<Dictionary> get_pending_reports() const;
	bool has_pending_reports() const;

	Error launch_reporter(const String &p_report_id = String());
	Error upload_report(const String &p_id);
	Error upload_pending();
	void cancel_upload();
	bool is_uploading() const;

	Error mark_report_submitted(const String &p_id);
	Error discard_report(const String &p_id);

	Dictionary get_resolved_config() const;

	String get_endpoint() const;
	String get_app_id() const;
	String get_app_name() const;
	String get_app_version() const;
	String get_build_channel() const;
	String get_contact_url() const;
	String get_privacy_policy_url() const;
	String get_reporter_path() const;
	String get_build_id() const;
	int get_upload_mode() const;
	bool is_enabled() const;

	String write_minidump();
	String get_last_dump_path() const;
	void induce_crash();

	CrashReporter();
	~CrashReporter();
};

VARIANT_ENUM_CAST(CrashReporter::UploadMode);
