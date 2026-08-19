/**************************************************************************/
/*  test_analytics.cpp                                                    */
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

#include "test_analytics.h"

#include "modules/analytics/analytics.h"
#include "modules/analytics/analytics_queue.h"

#ifdef MODULE_CRASH_REPORTER_ENABLED
#include "modules/crash_reporter/crash_reporter.h"
#endif

#include "core/config/app_identity.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/os/os.h"

static String _analytics_test_dir(const String &p_suffix) {
	return OS::get_singleton()->get_cache_path().path_join("analytics_module_test").path_join(p_suffix);
}

static void _reset_queue_dir(const String &p_dir) {
	OS::get_singleton()->set_environment("BLAZIUM_ANALYTICS_QUEUE_DIR", p_dir);
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	REQUIRE(da.is_valid());
	da->make_dir_recursive(p_dir);
	AnalyticsQueue::clear(p_dir);
}

#ifdef ANALYTICS_ENABLED
static void _enable_game_analytics(bool p_require_consent) {
	if (!ProjectSettings::get_singleton()) {
		return;
	}
	ProjectSettings::get_singleton()->set("application/analytics/enabled", true);
	ProjectSettings::get_singleton()->set("application/analytics/require_user_consent", p_require_consent);
	ProjectSettings::get_singleton()->set("application/analytics/anonymous", true);
	ProjectSettings::get_singleton()->set("application/analytics/app_id", "analytics-unit-test");
	ProjectSettings::get_singleton()->set("application/analytics/build_id", "test");
}
#endif

void test_analytics_queue_roundtrip() {
	const String dir = _analytics_test_dir("queue");
	_reset_queue_dir(dir);
	Dictionary ev;
	ev["app_id"] = "queue-test";
	ev["event"] = "ping";
	ev["anonymous"] = true;
	CHECK(AnalyticsQueue::append(dir, ev) == OK);
	CHECK(AnalyticsQueue::size(dir) == 1);
	const TypedArray<Dictionary> loaded = AnalyticsQueue::load(dir);
	REQUIRE(loaded.size() == 1);
	const Dictionary row = loaded[0];
	CHECK(String(row.get("app_id", String())) == "queue-test");
	CHECK(String(row.get("event", String())) == "ping");
	CHECK(AnalyticsQueue::clear(dir) == OK);
	CHECK(AnalyticsQueue::size(dir) == 0);
}

void test_analytics_consent_and_identity() {
#ifdef ANALYTICS_ENABLED
	Analytics *a = Analytics::get_singleton();
	REQUIRE(a != nullptr);
	_enable_game_analytics(true);
	const String dir = _analytics_test_dir("consent");
	_reset_queue_dir(dir);
	a->set_consent(false);
	a->set_anonymous(true);
	const int before = a->get_queue_size();
	a->track("should_not_queue");
	CHECK(a->get_queue_size() == before);
	a->set_consent(true);
	a->track("should_queue");
	CHECK(a->get_queue_size() == before + 1);
	CHECK(a->is_available());
	CHECK(a->get_consent() == "accepted");
#else
	CHECK(Analytics::get_singleton() != nullptr);
#endif
}

void test_analytics_anonymous_omits_device_uid() {
#ifdef ANALYTICS_ENABLED
	Analytics *a = Analytics::get_singleton();
	REQUIRE(a != nullptr);
	_enable_game_analytics(true);
	const String dir = _analytics_test_dir("anon");
	_reset_queue_dir(dir);
	a->set_consent(true);
	a->set_anonymous(true);
	a->identify("player-1");
	a->track("anon_event");
	const TypedArray<Dictionary> loaded = AnalyticsQueue::load(dir);
	REQUIRE(loaded.size() >= 1);
	const Dictionary row = loaded[loaded.size() - 1];
	CHECK(bool(row.get("anonymous", false)) == true);
	CHECK(!row.has("device_uid"));
	CHECK(a->get_device_uid().is_empty());
	const Dictionary props = row.get("properties", Dictionary());
	CHECK(!props.has("user_id"));
#else
	CHECK(Analytics::get_singleton() != nullptr);
#endif
}

void test_analytics_identified_includes_device_uid() {
#ifdef ANALYTICS_ENABLED
	Analytics *a = Analytics::get_singleton();
	REQUIRE(a != nullptr);
	_enable_game_analytics(true);
	const String dir = _analytics_test_dir("identified");
	_reset_queue_dir(dir);
	a->set_consent(true);
	a->set_anonymous(false);
	a->track("id_event");
	const TypedArray<Dictionary> loaded = AnalyticsQueue::load(dir);
	REQUIRE(loaded.size() >= 1);
	const Dictionary row = loaded[loaded.size() - 1];
	CHECK(bool(row.get("anonymous", true)) == false);
	REQUIRE(row.has("device_uid"));
	const String uid = String(row.get("device_uid", String()));
	CHECK(!uid.is_empty());
	CHECK(uid == OS::get_singleton()->get_unique_id());
	CHECK(a->get_device_uid() == uid);
#else
	CHECK(Analytics::get_singleton() != nullptr);
#endif
}

void test_analytics_payload_shape() {
#ifdef ANALYTICS_ENABLED
	Analytics *a = Analytics::get_singleton();
	REQUIRE(a != nullptr);
	_enable_game_analytics(true);
	const String dir = _analytics_test_dir("shape");
	_reset_queue_dir(dir);
	a->set_consent(true);
	a->set_anonymous(true);
	Dictionary extra;
	extra["level"] = 3;
	a->track("level_complete", extra);
	const TypedArray<Dictionary> loaded = AnalyticsQueue::load(dir);
	REQUIRE(loaded.size() >= 1);
	const Dictionary row = loaded[loaded.size() - 1];
	CHECK(row.has("app_id"));
	CHECK(row.has("build_id"));
	CHECK(row.has("engine_version"));
	CHECK(row.has("session_id"));
	CHECK(row.has("anonymous"));
	CHECK(String(row.get("event", String())) == "level_complete");
	CHECK(row.has("timestamp"));
	CHECK(row.has("properties"));
	CHECK(!row.has("device_uid"));
	const Dictionary props = row.get("properties", Dictionary());
	CHECK(int(props.get("level", 0)) == 3);
	const Dictionary cfg = a->get_resolved_config();
	CHECK(cfg.has("app_id"));
	CHECK(cfg.has("build_id"));
	CHECK(cfg.has("endpoint"));
	CHECK(cfg.has("queue_dir"));
	CHECK(!cfg.has("api_key"));
#else
	CHECK(Analytics::get_singleton() != nullptr);
#endif
}

void test_analytics_shared_identity() {
#ifdef ANALYTICS_ENABLED
	Analytics *a = Analytics::get_singleton();
	REQUIRE(a != nullptr);
	if (ProjectSettings::get_singleton()) {
		ProjectSettings::get_singleton()->set("application/analytics/app_id", String());
		ProjectSettings::get_singleton()->set("application/analytics/build_id", String());
		ProjectSettings::get_singleton()->set("application/crash_reporter/app_id", "crash-ns-app");
		ProjectSettings::get_singleton()->set("application/crash_reporter/build_id", "crash-ns-build");
	}
	CHECK(AppIdentity::resolve_app_id(String(), String(), "fallback") == "crash-ns-app");
	CHECK(AppIdentity::resolve_build_id(String(), String(), "fallback") == "crash-ns-build");
	if (ProjectSettings::get_singleton()) {
		ProjectSettings::get_singleton()->set("application/crash_reporter/app_id", String());
		ProjectSettings::get_singleton()->set("application/crash_reporter/build_id", String());
		ProjectSettings::get_singleton()->set("application/analytics/app_id", "analytics-ns-app");
		ProjectSettings::get_singleton()->set("application/analytics/build_id", "analytics-ns-build");
	}
	CHECK(AppIdentity::resolve_app_id(String(), String(), "fallback") == "analytics-ns-app");
	CHECK(AppIdentity::resolve_build_id(String(), String(), "fallback") == "analytics-ns-build");
#ifdef MODULE_CRASH_REPORTER_ENABLED
	CrashReporter *cr = CrashReporter::get_singleton();
	REQUIRE(cr != nullptr);
	CHECK(String(a->get_app_id()) == String(cr->get_app_id()));
	CHECK(String(a->get_build_id()) == String(cr->get_build_id()));
	CHECK(!cr->get_resolved_config().has("api_key"));
#endif
	CHECK(!a->get_resolved_config().has("api_key"));
#else
	CHECK(Analytics::get_singleton() != nullptr);
#endif
}
