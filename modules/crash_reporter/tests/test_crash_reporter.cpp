/**************************************************************************/
/*  test_crash_reporter.cpp                                               */
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

#include "test_crash_reporter.h"

#include "modules/crash_reporter/crash_reporter.h"
#include "modules/crash_reporter/crash_reporter_util.h"

#ifdef MODULE_ANALYTICS_ENABLED
#include "modules/analytics/analytics.h"
#endif

#include "core/config/app_identity.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"

void test_crash_reporter_multipart() {
	Vector<uint8_t> dump;
	dump.resize(4);
	dump.write[0] = 'M';
	dump.write[1] = 'D';
	dump.write[2] = 'M';
	dump.write[3] = 'P';
	Vector<uint8_t> log;
	const CrashReporterUtil::MultipartBody body = CrashReporterUtil::build_multipart(dump, "id.dmp", "{\"id\":\"id\"}", log, "id.log", "BOUND");
	CHECK(body.content_type.contains("BOUND"));
	const String as_text = String::utf8((const char *)body.data.ptr(), body.data.size());
	CHECK(as_text.contains("name=\"metadata\""));
	CHECK(as_text.contains("name=\"dump\""));
	CHECK(as_text.contains("MDMP"));
	CHECK(as_text.contains("--BOUND--"));
}

void test_crash_reporter_state_and_scan() {
	const String dir = OS::get_singleton()->get_cache_path().path_join("crash_reporter_test");
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	da->make_dir_recursive(dir);
	const String dump = dir.path_join("2026-01-01T000000_test.dmp");
	{
		Ref<FileAccess> f = FileAccess::open(dump, FileAccess::WRITE);
		REQUIRE(f.is_valid());
		f->store_buffer((const uint8_t *)"MDMP", 4);
	}
	CHECK(CrashReporterUtil::report_id_from_dump_path(dump) == "2026-01-01T000000_test");
	CHECK(CrashReporterUtil::read_state(dump) == CrashReporterUtil::STATE_PENDING);
	CHECK(CrashReporterUtil::write_state(dump, CrashReporterUtil::STATE_UPLOADING) == OK);
	CHECK(CrashReporterUtil::read_state(dump) == CrashReporterUtil::STATE_UPLOADING);
	CHECK(CrashReporterUtil::write_state(dump, CrashReporterUtil::STATE_SUBMITTED) == OK);
	CHECK(CrashReporterUtil::read_state(dump) == CrashReporterUtil::STATE_SUBMITTED);

	const TypedArray<Dictionary> pending = CrashReporterUtil::scan_pending_reports(dir, 10, 0);
	CHECK(pending.is_empty());

	CHECK(CrashReporterUtil::write_state(dump, CrashReporterUtil::STATE_PENDING) == OK);
	const TypedArray<Dictionary> again = CrashReporterUtil::scan_pending_reports(dir, 10, 0);
	CHECK(again.size() == 1);

	CHECK(CrashReporterUtil::discard_report_files(dump) == OK);
	CHECK(!FileAccess::exists(dump));
}

void test_crash_reporter_metadata_json() {
	Dictionary meta;
	meta["id"] = "abc";
	meta["app_id"] = "mygame";
	meta["build_id"] = "build-9";
	const String json = CrashReporterUtil::metadata_to_json(meta);
	CHECK(json.contains("abc"));
	const Dictionary parsed = CrashReporterUtil::parse_metadata_json(json);
	CHECK(String(parsed.get("id", String())) == "abc");
	CHECK(String(parsed.get("app_id", String())) == "mygame");
	CHECK(String(parsed.get("build_id", String())) == "build-9");
	CHECK(CrashReporterUtil::parse_metadata_json("not-json").is_empty());
}

void test_crash_reporter_shared_identity() {
	CrashReporter *cr = CrashReporter::get_singleton();
	REQUIRE(cr != nullptr);
	const Dictionary cfg = cr->get_resolved_config();
	CHECK(cfg.has("app_id"));
	CHECK(cfg.has("build_id"));
	CHECK(!cfg.has("api_key"));

	if (!ProjectSettings::get_singleton()) {
		return;
	}
	ProjectSettings::get_singleton()->set("application/crash_reporter/app_id", String());
	ProjectSettings::get_singleton()->set("application/crash_reporter/build_id", String());
	ProjectSettings::get_singleton()->set("application/analytics/app_id", "analytics-only-app");
	ProjectSettings::get_singleton()->set("application/analytics/build_id", "analytics-only-build");
	CHECK(AppIdentity::resolve_app_id(String(), String(), "fallback") == "analytics-only-app");
	CHECK(AppIdentity::resolve_build_id(String(), String(), "fallback") == "analytics-only-build");

	ProjectSettings::get_singleton()->set("application/crash_reporter/app_id", "crash-only-app");
	ProjectSettings::get_singleton()->set("application/crash_reporter/build_id", "crash-only-build");
	CHECK(AppIdentity::resolve_app_id(String(), String(), "fallback") == "crash-only-app");
	CHECK(AppIdentity::resolve_build_id(String(), String(), "fallback") == "crash-only-build");

#ifdef MODULE_ANALYTICS_ENABLED
	Analytics *a = Analytics::get_singleton();
	REQUIRE(a != nullptr);
	const Dictionary acfg = a->get_resolved_config();
	CHECK(!acfg.has("api_key"));
	CHECK(String(cr->get_app_id()) == String(a->get_app_id()));
	CHECK(String(cr->get_build_id()) == String(a->get_build_id()));
#endif
}

void test_crash_reporter_dump_apis() {
	CrashReporter *cr = CrashReporter::get_singleton();
	REQUIRE(cr != nullptr);
	CHECK(cr->is_available());
	const String last_before = cr->get_last_dump_path();
	(void)last_before;
#ifdef USE_BREAKPAD
	CHECK(cr->is_breakpad_enabled());
	const String path = cr->write_minidump();
	CHECK(!path.is_empty());
	CHECK(FileAccess::exists(path));
	CHECK(cr->get_last_dump_path() == path);
	const String meta = path.get_basename() + ".json";
	CHECK(FileAccess::exists(meta));
#else
	CHECK(!cr->is_breakpad_enabled());
	CHECK(cr->write_minidump().is_empty());
#endif
}
