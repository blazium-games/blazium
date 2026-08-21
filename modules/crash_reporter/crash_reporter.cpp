/**************************************************************************/
/*  crash_reporter.cpp                                                    */
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

#include "crash_reporter.h"

#include "breakpad_linuxbsd_windows.h"
#include "crash_reporter_http.h"
#include "crash_reporter_project_settings.h"
#include "crash_reporter_util.h"

#include "core/config/app_identity.h"
#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"
#include "core/string/print_string.h"
#include "core/version.h"
#include "servers/display_server.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_paths.h"
#endif

CrashReporter *CrashReporter::singleton = nullptr;

CrashReporter *CrashReporter::get_singleton() {
	return singleton;
}

CrashReporter::CrashReporter() {
	ERR_FAIL_COND(singleton != nullptr);
	singleton = this;
}

CrashReporter::~CrashReporter() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

void CrashReporter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_available"), &CrashReporter::is_available);
	ClassDB::bind_method(D_METHOD("is_breakpad_enabled"), &CrashReporter::is_breakpad_enabled);
	ClassDB::bind_method(D_METHOD("is_http_upload_available"), &CrashReporter::is_http_upload_available);
	ClassDB::bind_method(D_METHOD("get_crash_directory"), &CrashReporter::get_crash_directory);
	ClassDB::bind_method(D_METHOD("get_pending_reports"), &CrashReporter::get_pending_reports);
	ClassDB::bind_method(D_METHOD("has_pending_reports"), &CrashReporter::has_pending_reports);
	ClassDB::bind_method(D_METHOD("launch_reporter", "report_id"), &CrashReporter::launch_reporter, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("upload_report", "id"), &CrashReporter::upload_report);
	ClassDB::bind_method(D_METHOD("upload_pending"), &CrashReporter::upload_pending);
	ClassDB::bind_method(D_METHOD("cancel_upload"), &CrashReporter::cancel_upload);
	ClassDB::bind_method(D_METHOD("is_uploading"), &CrashReporter::is_uploading);
	ClassDB::bind_method(D_METHOD("mark_report_submitted", "id"), &CrashReporter::mark_report_submitted);
	ClassDB::bind_method(D_METHOD("discard_report", "id"), &CrashReporter::discard_report);
	ClassDB::bind_method(D_METHOD("get_resolved_config"), &CrashReporter::get_resolved_config);
	ClassDB::bind_method(D_METHOD("get_endpoint"), &CrashReporter::get_endpoint);
	ClassDB::bind_method(D_METHOD("get_app_id"), &CrashReporter::get_app_id);
	ClassDB::bind_method(D_METHOD("get_app_name"), &CrashReporter::get_app_name);
	ClassDB::bind_method(D_METHOD("get_app_version"), &CrashReporter::get_app_version);
	ClassDB::bind_method(D_METHOD("get_build_channel"), &CrashReporter::get_build_channel);
	ClassDB::bind_method(D_METHOD("get_contact_url"), &CrashReporter::get_contact_url);
	ClassDB::bind_method(D_METHOD("get_privacy_policy_url"), &CrashReporter::get_privacy_policy_url);
	ClassDB::bind_method(D_METHOD("get_reporter_path"), &CrashReporter::get_reporter_path);
	ClassDB::bind_method(D_METHOD("get_build_id"), &CrashReporter::get_build_id);
	ClassDB::bind_method(D_METHOD("get_upload_mode"), &CrashReporter::get_upload_mode);
	ClassDB::bind_method(D_METHOD("is_enabled"), &CrashReporter::is_enabled);
	ClassDB::bind_method(D_METHOD("write_minidump"), &CrashReporter::write_minidump);
	ClassDB::bind_method(D_METHOD("get_last_dump_path"), &CrashReporter::get_last_dump_path);
	ClassDB::bind_method(D_METHOD("induce_crash"), &CrashReporter::induce_crash);

	ADD_SIGNAL(MethodInfo("upload_started", PropertyInfo(Variant::STRING, "id")));
	ADD_SIGNAL(MethodInfo("upload_progress", PropertyInfo(Variant::STRING, "id"), PropertyInfo(Variant::INT, "sent"), PropertyInfo(Variant::INT, "total")));
	ADD_SIGNAL(MethodInfo("upload_succeeded", PropertyInfo(Variant::STRING, "id"), PropertyInfo(Variant::INT, "response_code")));
	ADD_SIGNAL(MethodInfo("upload_failed", PropertyInfo(Variant::STRING, "id"), PropertyInfo(Variant::STRING, "error"), PropertyInfo(Variant::INT, "response_code")));

	BIND_ENUM_CONSTANT(UPLOAD_DISABLED);
	BIND_ENUM_CONSTANT(UPLOAD_IN_ENGINE);
	BIND_ENUM_CONSTANT(UPLOAD_SIDECAR);
	BIND_ENUM_CONSTANT(UPLOAD_BOTH);
}

String CrashReporter::_resolve_env_or_baked(const String &p_env, const String &p_baked, const String &p_fallback) const {
#ifdef TOOLS_ENABLED
	if (!p_env.is_empty() && OS::get_singleton() && OS::get_singleton()->has_environment(p_env)) {
		return OS::get_singleton()->get_environment(p_env);
	}
	if (!p_baked.is_empty()) {
		return p_baked;
	}
	return p_fallback;
#else
	(void)p_env;
	(void)p_baked;
	return p_fallback;
#endif
}

String CrashReporter::_setting_string(const String &p_key, const String &p_fallback) const {
#ifdef TOOLS_ENABLED
	(void)p_key;
	return p_fallback;
#else
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting(p_key)) {
		const String v = ProjectSettings::get_singleton()->get_setting_with_override(p_key);
		if (!v.is_empty()) {
			return v;
		}
	}
	return p_fallback;
#endif
}

bool CrashReporter::_setting_bool(const String &p_key, bool p_fallback) const {
#ifdef TOOLS_ENABLED
	(void)p_key;
	return p_fallback;
#else
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting(p_key)) {
		return ProjectSettings::get_singleton()->get_setting_with_override(p_key);
	}
	return p_fallback;
#endif
}

int CrashReporter::_setting_int(const String &p_key, int p_fallback) const {
#ifdef TOOLS_ENABLED
	(void)p_key;
	return p_fallback;
#else
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting(p_key)) {
		return ProjectSettings::get_singleton()->get_setting_with_override(p_key);
	}
	return p_fallback;
#endif
}

#ifdef TOOLS_ENABLED
#define CR_BAKED(name) (String(CRASH_REPORTER_EDITOR_##name))
#else
#define CR_BAKED(name) String()
#endif

#ifndef CRASH_REPORTER_EDITOR_APP_ID
#define CRASH_REPORTER_EDITOR_APP_ID ""
#endif
#ifndef CRASH_REPORTER_EDITOR_APP_NAME
#define CRASH_REPORTER_EDITOR_APP_NAME ""
#endif
#ifndef CRASH_REPORTER_EDITOR_BUILD_ID
#define CRASH_REPORTER_EDITOR_BUILD_ID ""
#endif
#ifndef CRASH_REPORTER_EDITOR_BUILD_CHANNEL
#define CRASH_REPORTER_EDITOR_BUILD_CHANNEL ""
#endif
#ifndef CRASH_REPORTER_EDITOR_ENDPOINT
#define CRASH_REPORTER_EDITOR_ENDPOINT ""
#endif
#ifndef CRASH_REPORTER_EDITOR_CONTACT_URL
#define CRASH_REPORTER_EDITOR_CONTACT_URL ""
#endif
#ifndef CRASH_REPORTER_TEMPLATE_APP_ID
#define CRASH_REPORTER_TEMPLATE_APP_ID ""
#endif
#ifndef CRASH_REPORTER_TEMPLATE_BUILD_ID
#define CRASH_REPORTER_TEMPLATE_BUILD_ID ""
#endif
#ifndef CRASH_REPORTER_TEMPLATE_ENDPOINT
#define CRASH_REPORTER_TEMPLATE_ENDPOINT ""
#endif

bool CrashReporter::is_available() const {
	return true;
}

bool CrashReporter::is_breakpad_enabled() const {
#ifdef USE_BREAKPAD
	return true;
#else
	return false;
#endif
}

bool CrashReporter::is_http_upload_available() const {
#if defined(CRASH_REPORTER_ENABLED) && !defined(TOOLS_ENABLED)
	return true;
#else
	return false;
#endif
}

bool CrashReporter::is_enabled() const {
#ifdef TOOLS_ENABLED
	if (OS::get_singleton() && OS::get_singleton()->has_environment("BLAZIUM_CRASH_REPORTER_ENABLED")) {
		return OS::get_singleton()->get_environment("BLAZIUM_CRASH_REPORTER_ENABLED") == "1" || OS::get_singleton()->get_environment("BLAZIUM_CRASH_REPORTER_ENABLED").to_lower() == "true";
	}
	return true;
#else
	return _setting_bool("application/crash_reporter/enabled", false);
#endif
}

String CrashReporter::get_app_id() const {
#ifdef TOOLS_ENABLED
	const String baked = CR_BAKED(APP_ID).strip_edges();
	return baked.is_empty() ? AppIdentity::editor_fallback_app_id() : baked;
#else
	return AppIdentity::resolve_app_id(String(CRASH_REPORTER_TEMPLATE_APP_ID), _setting_string("application/config/name", String()));
#endif
}

String CrashReporter::get_build_id() const {
#ifdef TOOLS_ENABLED
	const String baked = CR_BAKED(BUILD_ID).strip_edges();
	return baked.is_empty() ? AppIdentity::editor_fallback_build_id() : baked;
#else
	return AppIdentity::resolve_build_id(String(CRASH_REPORTER_TEMPLATE_BUILD_ID), _setting_string("application/config/version", String()));
#endif
}

String CrashReporter::get_app_name() const {
	const String baked = CR_BAKED(APP_NAME);
	const String from_env = _resolve_env_or_baked("BLAZIUM_CRASH_REPORTER_APP_NAME", baked, String());
#ifdef TOOLS_ENABLED
	return from_env.is_empty() ? String("Blazium Editor") : from_env;
#else
	if (!from_env.is_empty()) {
		return from_env;
	}
	const String setting = _setting_string("application/crash_reporter/app_name", String());
	if (!setting.is_empty()) {
		return setting;
	}
	return _setting_string("application/config/name", String());
#endif
}

String CrashReporter::get_app_version() const {
	const String from_env = _resolve_env_or_baked("BLAZIUM_CRASH_REPORTER_APP_VERSION", String(), String());
#ifdef TOOLS_ENABLED
	return from_env.is_empty() ? String(VERSION_FULL_NAME) : from_env;
#else
	if (!from_env.is_empty()) {
		return from_env;
	}
	const String setting = _setting_string("application/crash_reporter/app_version", String());
	if (!setting.is_empty()) {
		return setting;
	}
	return _setting_string("application/config/version", String());
#endif
}

String CrashReporter::get_build_channel() const {
	const String baked = CR_BAKED(BUILD_CHANNEL);
	const String from_env = _resolve_env_or_baked("BLAZIUM_CRASH_REPORTER_BUILD_CHANNEL", baked, String());
#ifdef TOOLS_ENABLED
	return from_env.is_empty() ? String("dev") : from_env;
#else
	if (!from_env.is_empty()) {
		return from_env;
	}
	return _setting_string("application/crash_reporter/build_channel", "release");
#endif
}

String CrashReporter::get_contact_url() const {
	const String baked = CR_BAKED(CONTACT_URL);
	const String from_env = _resolve_env_or_baked("BLAZIUM_CRASH_REPORTER_CONTACT_URL", baked, String());
#ifdef TOOLS_ENABLED
	return from_env.is_empty() ? String("https://github.com/blazium-games/blazium/issues") : from_env;
#else
	if (!from_env.is_empty()) {
		return from_env;
	}
	return _setting_string("application/crash_reporter/contact_url", String());
#endif
}

String CrashReporter::get_privacy_policy_url() const {
	return _setting_string("application/crash_reporter/privacy_policy_url", String());
}

String CrashReporter::get_endpoint() const {
#ifdef TOOLS_ENABLED
	const String baked = CR_BAKED(ENDPOINT);
	return _resolve_env_or_baked("BLAZIUM_CRASH_REPORTER_ENDPOINT", baked, String());
#else
	const String baked = String(CRASH_REPORTER_TEMPLATE_ENDPOINT).strip_edges();
	if (!baked.is_empty()) {
		return baked;
	}
	const String from_env = _resolve_env_or_baked("BLAZIUM_CRASH_REPORTER_ENDPOINT", String(), String());
	if (!from_env.is_empty()) {
		return from_env;
	}
	return _setting_string("application/crash_reporter/endpoint", String());
#endif
}

String CrashReporter::get_crash_directory() const {
#ifdef TOOLS_ENABLED
	if (OS::get_singleton() && OS::get_singleton()->has_environment("BLAZIUM_CRASH_REPORTER_CRASH_DIR")) {
		return OS::get_singleton()->get_environment("BLAZIUM_CRASH_REPORTER_CRASH_DIR");
	}
	String base;
	if (EditorPaths::get_singleton()) {
		base = EditorPaths::get_singleton()->get_data_dir();
	} else if (OS::get_singleton()) {
		base = OS::get_singleton()->get_data_path();
	}
	return base.path_join("crashes");
#else
	String dir_name = _setting_string("application/crash_reporter/crash_dir_name", "crashes");
	if (dir_name.is_empty()) {
		dir_name = "crashes";
	}
	return OS::get_singleton()->get_user_data_dir().path_join(dir_name);
#endif
}

String CrashReporter::_resolve_reporter_path() const {
	if (reporter_path_cached) {
		return cached_reporter_path;
	}
	reporter_path_cached = true;
	cached_reporter_path = _compute_reporter_path();
	return cached_reporter_path;
}

String CrashReporter::_compute_reporter_path() const {
#ifdef TOOLS_ENABLED
	const String cli = AppIdentity::cmdline_flag_value("crash-reporter");
	if (cli.is_empty()) {
		return String();
	}
	if (cli.is_absolute_path()) {
		return cli;
	}
	return OS::get_singleton()->get_executable_path().get_base_dir().path_join(cli);
#else
	String filename = _setting_string("application/crash_reporter/reporter_filename", String()).strip_edges();
	const String path_setting = _setting_string("application/crash_reporter/reporter_path", String()).strip_edges();
	String rel;
	if (!filename.is_empty()) {
		rel = filename;
	} else if (!path_setting.is_empty()) {
		rel = path_setting;
	} else {
#ifdef WINDOWS_ENABLED
		rel = "crash_reporter.exe";
#else
		rel = "crash_reporter";
#endif
	}
	String path = rel;
	if (!path.is_absolute_path() && OS::get_singleton()) {
		path = OS::get_singleton()->get_executable_path().get_base_dir().path_join(rel);
	}
	const String expected_sha = _setting_string("application/crash_reporter/reporter_sha256", String());
	if (!CrashReporterUtil::sidecar_sha256_matches(path, expected_sha)) {
		ERR_PRINT(vformat("Crash reporter sidecar SHA-256 mismatch or missing file, skip spawn: %s", path));
		return String();
	}
	return path;
#endif
}

String CrashReporter::get_reporter_path() const {
	return _resolve_reporter_path();
}

int CrashReporter::get_upload_mode() const {
#ifdef TOOLS_ENABLED
	if (!_resolve_reporter_path().is_empty()) {
		return UPLOAD_SIDECAR;
	}
	return UPLOAD_DISABLED;
#else
	return _setting_int("application/crash_reporter/upload_mode", UPLOAD_DISABLED);
#endif
}

TypedArray<Dictionary> CrashReporter::get_pending_reports() const {
	return CrashReporterUtil::scan_pending_reports(get_crash_directory(), _setting_int("application/crash_reporter/max_pending", 10), _setting_int("application/crash_reporter/retain_days", 30));
}

bool CrashReporter::has_pending_reports() const {
	return get_pending_reports().size() > 0;
}

Dictionary CrashReporter::get_resolved_config() const {
	Dictionary d;
	d["enabled"] = is_enabled();
	d["app_id"] = get_app_id();
	d["app_name"] = get_app_name();
	d["app_version"] = get_app_version();
	d["build_id"] = get_build_id();
	d["build_channel"] = get_build_channel();
	d["contact_url"] = get_contact_url();
	d["privacy_policy_url"] = get_privacy_policy_url();
	d["endpoint"] = get_endpoint();
	d["reporter_path"] = get_reporter_path();
	d["crash_directory"] = get_crash_directory();
	d["upload_mode"] = get_upload_mode();
	d["breakpad_enabled"] = is_breakpad_enabled();
	d["http_upload_available"] = is_http_upload_available();
	return d;
}

void CrashReporter::_cache_breakpad_identity() {
#ifdef USE_BREAKPAD
	const CharString app_id = get_app_id().utf8();
	const CharString app_name = get_app_name().utf8();
	const CharString app_version = get_app_version().utf8();
	const CharString engine_version = String(VERSION_FULL_NAME).utf8();
	const CharString engine_hash = String(VERSION_HASH).utf8();
	const CharString os_name = OS::get_singleton()->get_name().utf8();
	const CharString arch = Engine::get_singleton()->get_architecture_name().utf8();
	const CharString channel = get_build_channel().utf8();
	const CharString contact = get_contact_url().utf8();
	const CharString build_id = get_build_id().utf8();
	breakpad_cache_identity(app_id.get_data(), app_name.get_data(), app_version.get_data(), engine_version.get_data(), engine_hash.get_data(), os_name.get_data(), arch.get_data(), channel.get_data(), contact.get_data(), build_id.get_data());

	const int mode = get_upload_mode();
	const bool spawn = is_enabled() && _setting_bool("application/crash_reporter/spawn_on_crash", true) && (mode == UPLOAD_SIDECAR || mode == UPLOAD_BOTH);
	const CharString reporter = _resolve_reporter_path().utf8();
	breakpad_cache_spawn(reporter.get_data(), spawn);
#endif
}

void CrashReporter::_print_console_dump(const String &p_path) {
	String msg = _setting_string("application/crash_reporter/console_message", "Crash dump created at: {path}\nPlease attach this file when reporting issues.");
	msg = msg.replace("{path}", p_path);
	print_error(msg);
	if (!get_app_id().is_empty()) {
		print_error(vformat("Crash reporter app_id: %s", get_app_id()));
	}
	if (!get_build_id().is_empty()) {
		print_error(vformat("Crash reporter build_id: %s", get_build_id()));
	}
	if (!get_endpoint().is_empty()) {
		print_error(vformat("Crash reporter endpoint: %s", get_endpoint()));
	}
}

void CrashReporter::_enrich_pending_metadata() {
	if (!_setting_bool("application/crash_reporter/include_system_info", true)) {
		return;
	}
	const TypedArray<Dictionary> pending = get_pending_reports();
	for (int i = 0; i < pending.size(); i++) {
		const Dictionary row = pending[i];
		const String meta_path = row.get("metadata_path", String());
		Dictionary meta = row.get("metadata", Dictionary());
		if (String(meta.get("app_id", String())).is_empty()) {
			meta["app_id"] = get_app_id();
		}
		if (String(meta.get("build_id", String())).is_empty()) {
			meta["build_id"] = get_build_id();
		}
		meta["os"] = OS::get_singleton()->get_name();
		meta["arch"] = Engine::get_singleton()->get_architecture_name();
		meta["locale"] = OS::get_singleton()->get_locale();
		meta["cpu"] = OS::get_singleton()->get_processor_name();
		const Vector<String> driver_info = OS::get_singleton()->get_video_adapter_driver_info();
		if (!driver_info.is_empty()) {
			meta["video_adapter"] = driver_info[0];
		}
		if (DisplayServer::get_singleton()) {
			meta["window_size"] = DisplayServer::get_singleton()->window_get_size();
		}
		if (_setting_bool("application/crash_reporter/include_project_settings_dump", false) && ProjectSettings::get_singleton()) {
			Dictionary sanitized;
			sanitized["name"] = ProjectSettings::get_singleton()->get_setting_with_override("application/config/name");
			sanitized["version"] = ProjectSettings::get_singleton()->get_setting_with_override("application/config/version");
			meta["project_settings"] = sanitized;
		}
		const Dictionary custom = ProjectSettings::get_singleton() ? Dictionary(ProjectSettings::get_singleton()->get_setting_with_override("application/crash_reporter/custom_metadata")) : Dictionary();
		if (!custom.is_empty()) {
			meta["custom"] = custom;
		}
		CrashReporterUtil::write_text_file(meta_path, CrashReporterUtil::metadata_to_json(meta));
	}
}

Error CrashReporter::_launch_reporter_internal(const String &p_report_id) {
	const String path = _resolve_reporter_path();
	if (path.is_empty() || !FileAccess::exists(path)) {
		return ERR_FILE_NOT_FOUND;
	}
	List<String> args;
	args.push_back("--crash-dir");
	args.push_back(get_crash_directory());
	if (!p_report_id.is_empty()) {
		args.push_back("--report-id");
		args.push_back(p_report_id);
	}
	if (!get_endpoint().is_empty()) {
		args.push_back("--endpoint");
		args.push_back(get_endpoint());
	}
	if (!get_app_id().is_empty()) {
		args.push_back("--app-id");
		args.push_back(get_app_id());
	}
	if (!get_build_id().is_empty()) {
		args.push_back("--build-id");
		args.push_back(get_build_id());
	}
	if (!get_contact_url().is_empty()) {
		args.push_back("--contact-url");
		args.push_back(get_contact_url());
	}
	if (!get_privacy_policy_url().is_empty()) {
		args.push_back("--privacy-url");
		args.push_back(get_privacy_policy_url());
	}
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("application/crash_reporter/reporter_args")) {
		const PackedStringArray extra = ProjectSettings::get_singleton()->get_setting_with_override("application/crash_reporter/reporter_args");
		for (int i = 0; i < extra.size(); i++) {
			args.push_back(extra[i]);
		}
	}
	const bool open_console = _setting_bool("application/crash_reporter/spawn_open_console", false);
	return OS::get_singleton()->create_process(path, args, nullptr, open_console);
}

Error CrashReporter::launch_reporter(const String &p_report_id) {
	return _launch_reporter_internal(p_report_id);
}

Error CrashReporter::mark_report_submitted(const String &p_id) {
	const String dump = get_crash_directory().path_join(p_id + ".dmp");
	if (!FileAccess::exists(dump)) {
		return ERR_FILE_NOT_FOUND;
	}
	return CrashReporterUtil::write_state(dump, CrashReporterUtil::STATE_SUBMITTED);
}

Error CrashReporter::discard_report(const String &p_id) {
	const String dump = get_crash_directory().path_join(p_id + ".dmp");
	if (!FileAccess::exists(dump)) {
		return ERR_FILE_NOT_FOUND;
	}
	return CrashReporterUtil::discard_report_files(dump);
}

Error CrashReporter::upload_report(const String &p_id) {
	if (!is_http_upload_available()) {
		return ERR_UNAVAILABLE;
	}
	if (get_endpoint().is_empty()) {
		return ERR_UNCONFIGURED;
	}
	const String dump = get_crash_directory().path_join(p_id + ".dmp");
	if (!FileAccess::exists(dump)) {
		return ERR_FILE_NOT_FOUND;
	}
	if (CrashReporterUtil::read_state(dump) == CrashReporterUtil::STATE_SUBMITTED) {
		return ERR_ALREADY_EXISTS;
	}
	const int64_t max_bytes = (int64_t)_setting_int("application/crash_reporter/max_upload_mb", 32) * 1024 * 1024;
	Error err = OK;
	const Vector<uint8_t> dump_bytes = CrashReporterUtil::read_file_capped(dump, max_bytes, &err);
	if (err != OK) {
		return err;
	}
	if (FileAccess::get_file_as_bytes(dump).size() > max_bytes) {
		return ERR_OUT_OF_MEMORY;
	}

	CrashReporterUtil::write_state(dump, CrashReporterUtil::STATE_UPLOADING);
	uploading = true;
	upload_cancel = false;
	uploading_id = p_id;
	emit_signal(SNAME("upload_started"), p_id);

	String meta_json = CrashReporterUtil::read_text_file(CrashReporterUtil::sidecar_path_for_dump(dump));
	if (meta_json.is_empty()) {
		Dictionary meta;
		meta["id"] = p_id;
		meta["app_id"] = get_app_id();
		meta["build_id"] = get_build_id();
		meta_json = CrashReporterUtil::metadata_to_json(meta);
	}

	Vector<uint8_t> log_bytes;
	if (_setting_bool("application/crash_reporter/include_logs", true)) {
		const String log_path = OS::get_singleton()->get_user_data_dir().path_join("logs").path_join("godot.log");
		if (FileAccess::exists(log_path)) {
			log_bytes = CrashReporterUtil::read_log_tail(log_path, _setting_int("application/crash_reporter/log_tail_kb", 64));
		}
	}

	const CrashReporterUtil::MultipartBody body = CrashReporterUtil::build_multipart(dump_bytes, p_id + ".dmp", meta_json, log_bytes, p_id + ".log", "----BlaziumCrashBoundary");
	emit_signal(SNAME("upload_progress"), p_id, (int64_t)0, (int64_t)body.data.size());

	const String ua = vformat("BlaziumCrashReporter/%s", VERSION_FULL_NAME);
	const CrashReporterHTTPResult result = CrashReporterHTTP::upload_report(get_endpoint(), get_app_id(), get_build_id(), ua, body.data, body.content_type, _setting_int("application/crash_reporter/timeout_sec", 30), _setting_bool("application/crash_reporter/verify_tls", true), _setting_int("application/crash_reporter/retry_count", 3), _setting_int("application/crash_reporter/retry_backoff_sec", 5), &upload_cancel);

	uploading = false;
	if (result.error == OK) {
		CrashReporterUtil::write_state(dump, CrashReporterUtil::STATE_SUBMITTED);
		if (_setting_bool("application/crash_reporter/delete_after_upload", true)) {
			CrashReporterUtil::discard_report_files(dump);
		}
		emit_signal(SNAME("upload_succeeded"), p_id, result.response_code);
		return OK;
	}
	CrashReporterUtil::write_state(dump, CrashReporterUtil::STATE_FAILED);
	emit_signal(SNAME("upload_failed"), p_id, result.message, result.response_code);
	return result.error;
}

Error CrashReporter::upload_pending() {
	if (!is_http_upload_available()) {
		return ERR_UNAVAILABLE;
	}
	const TypedArray<Dictionary> pending = get_pending_reports();
	Error last = ERR_DOES_NOT_EXIST;
	for (int i = 0; i < pending.size(); i++) {
		const Dictionary row = pending[i];
		last = upload_report(row.get("id", String()));
	}
	return last;
}

void CrashReporter::cancel_upload() {
	upload_cancel = true;
}

bool CrashReporter::is_uploading() const {
	return uploading;
}

void CrashReporter::_schedule_poll() {
	if (poll_scheduled.is_set()) {
		return;
	}
	poll_scheduled.set();
	callable_mp(this, &CrashReporter::_poll_upload).call_deferred();
}

void CrashReporter::_poll_upload() {
	poll_scheduled.clear();
}

void CrashReporter::_startup_actions() {
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	if (da.is_valid()) {
		da->make_dir_recursive(get_crash_directory());
	}
	CrashReporterUtil::prune_old_reports(get_crash_directory(), _setting_int("application/crash_reporter/max_pending", 10), _setting_int("application/crash_reporter/retain_days", 30));
	_cache_breakpad_identity();
#ifdef USE_BREAKPAD
	breakpad_set_dump_path(get_crash_directory().utf8().get_data());
#endif

#ifdef TOOLS_ENABLED
	if (!_resolve_reporter_path().is_empty() && is_enabled()) {
		_enrich_pending_metadata();
		if (has_pending_reports() && _setting_bool("application/crash_reporter/spawn_on_next_launch", true)) {
			_launch_reporter_internal(String());
		}
	}
	return;
#else
	if (!is_enabled()) {
		return;
	}
	_enrich_pending_metadata();
	const int mode = get_upload_mode();
	const bool has_pending = has_pending_reports();
	if (has_pending && (mode == UPLOAD_SIDECAR || mode == UPLOAD_BOTH) && _setting_bool("application/crash_reporter/spawn_on_next_launch", true)) {
		_launch_reporter_internal(String());
	}
	if (has_pending && (mode == UPLOAD_IN_ENGINE || mode == UPLOAD_BOTH) && !get_endpoint().is_empty() && _setting_bool("application/crash_reporter/upload_on_startup", true) && !_setting_bool("application/crash_reporter/require_user_consent", true)) {
		upload_pending();
	}
#endif
}

void CrashReporter::report_user_data_dir_ready() {
	_startup_actions();
}

String CrashReporter::write_minidump() {
#ifndef USE_BREAKPAD
	return String();
#else
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	if (da.is_valid()) {
		da->make_dir_recursive(get_crash_directory());
	}
	_cache_breakpad_identity();
	initialize_breakpad(false);
	breakpad_set_dump_path(get_crash_directory().utf8().get_data());
	breakpad_write_minidump();
	_enrich_pending_metadata();
	const String path = get_last_dump_path();
	if (!path.is_empty()) {
		_print_console_dump(path);
	}
	return path;
#endif
}

String CrashReporter::get_last_dump_path() const {
#ifdef USE_BREAKPAD
	const char *path = breakpad_last_dump_path();
	if (path && path[0]) {
		return String::utf8(path);
	}
#endif
	return String();
}

void CrashReporter::induce_crash() {
#ifdef USE_BREAKPAD
	volatile int *crash_ptr = nullptr;
	*crash_ptr = 0xDEAD;
#else
	ERR_PRINT("CrashReporter.induce_crash() requires a build with Breakpad (crash_reporter=yes or editor_crash_reporter=yes).");
#endif
}
