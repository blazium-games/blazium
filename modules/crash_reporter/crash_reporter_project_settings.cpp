/**************************************************************************/
/*  crash_reporter_project_settings.cpp                                   */
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

#include "crash_reporter_project_settings.h"

#include "core/config/project_settings.h"

#ifdef TOOLS_ENABLED
#include "core/config/app_identity.h"
#include "editor/editor_settings.h"

#ifndef CRASH_REPORTER_EDITOR_APP_ID
#define CRASH_REPORTER_EDITOR_APP_ID ""
#endif
#ifndef CRASH_REPORTER_EDITOR_BUILD_ID
#define CRASH_REPORTER_EDITOR_BUILD_ID ""
#endif
#endif

void crash_reporter_register_project_settings() {
	GLOBAL_DEF_BASIC("application/crash_reporter/enabled", false);
	GLOBAL_DEF_BASIC("application/crash_reporter/app_id", String());
	GLOBAL_DEF_BASIC("application/crash_reporter/app_name", String());
	GLOBAL_DEF_BASIC("application/crash_reporter/app_version", String());
	GLOBAL_DEF_BASIC("application/crash_reporter/build_id", String());
	GLOBAL_DEF_BASIC("application/crash_reporter/build_channel", "release");
	GLOBAL_DEF_BASIC("application/crash_reporter/contact_url", String());
	GLOBAL_DEF_BASIC("application/crash_reporter/privacy_policy_url", String());

	GLOBAL_DEF_BASIC(PropertyInfo(Variant::STRING, "application/crash_reporter/reporter_path", PROPERTY_HINT_FILE), String());
	GLOBAL_DEF_BASIC("application/crash_reporter/reporter_args", PackedStringArray());
	GLOBAL_DEF_BASIC("application/crash_reporter/spawn_on_crash", true);
	GLOBAL_DEF_BASIC("application/crash_reporter/spawn_on_next_launch", true);
	GLOBAL_DEF_BASIC("application/crash_reporter/spawn_open_console", false);

	GLOBAL_DEF_BASIC("application/crash_reporter/endpoint", String());
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "application/crash_reporter/timeout_sec", PROPERTY_HINT_RANGE, "1,300,1"), 30);
	GLOBAL_DEF_BASIC("application/crash_reporter/verify_tls", true);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "application/crash_reporter/upload_mode", PROPERTY_HINT_ENUM, "Disabled,InEngine,Sidecar,Both"), 0);
	GLOBAL_DEF_BASIC("application/crash_reporter/upload_on_startup", true);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "application/crash_reporter/retry_count", PROPERTY_HINT_RANGE, "0,10,1"), 3);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "application/crash_reporter/retry_backoff_sec", PROPERTY_HINT_RANGE, "1,120,1"), 5);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "application/crash_reporter/max_upload_mb", PROPERTY_HINT_RANGE, "1,256,1"), 32);
	GLOBAL_DEF_BASIC("application/crash_reporter/delete_after_upload", true);

	GLOBAL_DEF_BASIC("application/crash_reporter/require_user_consent", true);
	GLOBAL_DEF_BASIC("application/crash_reporter/include_logs", true);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "application/crash_reporter/log_tail_kb", PROPERTY_HINT_RANGE, "1,1024,1"), 64);
	GLOBAL_DEF_BASIC("application/crash_reporter/include_system_info", true);
	GLOBAL_DEF_BASIC("application/crash_reporter/include_project_settings_dump", false);
	GLOBAL_DEF_BASIC("application/crash_reporter/custom_metadata", Dictionary());
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::STRING, "application/crash_reporter/user_message", PROPERTY_HINT_MULTILINE_TEXT), String());

	GLOBAL_DEF_BASIC("application/crash_reporter/crash_dir_name", "crashes");
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "application/crash_reporter/max_pending", PROPERTY_HINT_RANGE, "1,100,1"), 10);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "application/crash_reporter/retain_days", PROPERTY_HINT_RANGE, "0,365,1"), 30);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::STRING, "application/crash_reporter/console_message", PROPERTY_HINT_MULTILINE_TEXT),
			String("Crash dump created at: {path}\nPlease attach this file when reporting issues."));
}

#ifdef TOOLS_ENABLED
void crash_reporter_register_editor_settings() {
	if (!EditorSettings::get_singleton()) {
		return;
	}
	EditorSettings *es = EditorSettings::get_singleton();
	String baked_app_id = String(CRASH_REPORTER_EDITOR_APP_ID).strip_edges();
	if (baked_app_id.is_empty()) {
		baked_app_id = AppIdentity::editor_fallback_app_id();
	}
	String baked_build_id = String(CRASH_REPORTER_EDITOR_BUILD_ID).strip_edges();
	if (baked_build_id.is_empty()) {
		baked_build_id = AppIdentity::editor_fallback_build_id();
	}
	EDITOR_DEF_BASIC("blazium/crash_reporter/app_id", baked_app_id);
	EDITOR_DEF_BASIC("blazium/crash_reporter/build_id", baked_build_id);
	es->set_setting("blazium/crash_reporter/app_id", baked_app_id);
	es->set_setting("blazium/crash_reporter/build_id", baked_build_id);
	const uint32_t read_only = PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_READ_ONLY;
	es->add_property_hint(PropertyInfo(Variant::STRING, "blazium/crash_reporter/app_id", PROPERTY_HINT_NONE, "", read_only));
	es->add_property_hint(PropertyInfo(Variant::STRING, "blazium/crash_reporter/build_id", PROPERTY_HINT_NONE, "", read_only));
}
#endif
