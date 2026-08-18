/**************************************************************************/
/*  analytics.cpp                                                         */
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

#include "analytics.h"

#include "analytics_http.h"
#include "analytics_queue.h"

#include <cstring>

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/json.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "core/version.h"
#include "servers/display_server.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_paths.h"
#include "editor/editor_settings.h"
#endif

#ifndef ANALYTICS_EDITOR_APP_ID
#define ANALYTICS_EDITOR_APP_ID ""
#endif
#ifndef ANALYTICS_EDITOR_BUILD_ID
#define ANALYTICS_EDITOR_BUILD_ID ""
#endif
#ifndef ANALYTICS_EDITOR_BUILD_CHANNEL
#define ANALYTICS_EDITOR_BUILD_CHANNEL ""
#endif
#ifndef ANALYTICS_EDITOR_ENDPOINT
#define ANALYTICS_EDITOR_ENDPOINT ""
#endif
#ifndef ANALYTICS_EDITOR_API_KEY
#define ANALYTICS_EDITOR_API_KEY ""
#endif

Analytics *Analytics::singleton = nullptr;

Analytics *Analytics::get_singleton() {
	return singleton;
}

static String _make_session_id() {
	const uint64_t now = OS::get_singleton() ? OS::get_singleton()->get_unix_time() : 0;
	const uint64_t ticks = OS::get_singleton() ? OS::get_singleton()->get_ticks_usec() : 0;
	const uint64_t pid = OS::get_singleton() ? (uint64_t)OS::get_singleton()->get_process_id() : 0;
	const uint64_t tail = ticks & ((uint64_t)0x0000ffffffffffff);
	return vformat("%08x-%04x-%04x-%04x-%012x",
			(uint32_t)now,
			(uint16_t)((ticks >> 32) & 0xffff),
			(uint16_t)(((ticks >> 16) & 0x0fff) | 0x4000),
			(uint16_t)((pid ^ (ticks >> 8)) & 0xffff),
			tail);
}

Analytics::Analytics() {
	singleton = this;
	session_id = _make_session_id();
	session_start_msec = OS::get_singleton() ? OS::get_singleton()->get_ticks_msec() : 0;
}

Analytics::~Analytics() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

String Analytics::_cmdline_value(const String &p_prefix) const {
	if (!OS::get_singleton()) {
		return String();
	}
	List<String> args = OS::get_singleton()->get_cmdline_args();
	for (const String &arg : args) {
		if (arg.begins_with(p_prefix)) {
			return arg.substr(p_prefix.length());
		}
	}
	return String();
}

String Analytics::_env(const String &p_name) const {
	if (!OS::get_singleton() || !OS::get_singleton()->has_environment(p_name)) {
		return String();
	}
	return OS::get_singleton()->get_environment(p_name);
}

Analytics::Consent Analytics::_parse_consent(const String &p_value) const {
	const String v = p_value.strip_edges().to_lower();
	if (v == "accepted" || v == "accept" || v == "1" || v == "true" || v == "yes") {
		return CONSENT_ACCEPTED;
	}
	if (v == "declined" || v == "decline" || v == "0" || v == "false" || v == "no") {
		return CONSENT_DECLINED;
	}
	return CONSENT_UNSET;
}

bool Analytics::_parse_bool_env(const String &p_value, bool p_fallback) const {
	const String v = p_value.strip_edges().to_lower();
	if (v == "1" || v == "true" || v == "yes") {
		return true;
	}
	if (v == "0" || v == "false" || v == "no") {
		return false;
	}
	return p_fallback;
}

String Analytics::_setting_string(const String &p_key, const String &p_fallback) const {
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting(p_key)) {
		return String(GLOBAL_GET(p_key));
	}
	return p_fallback;
}

bool Analytics::_setting_bool(const String &p_key, bool p_fallback) const {
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting(p_key)) {
		return bool(GLOBAL_GET(p_key));
	}
	return p_fallback;
}

bool Analytics::_is_editor_context() const {
#ifdef TOOLS_ENABLED
	return Engine::get_singleton() && Engine::get_singleton()->is_editor_hint();
#else
	return false;
#endif
}

bool Analytics::_can_queue() const {
#ifdef ANALYTICS_ENABLED
	if (_is_editor_context()) {
		return get_consent() == "accepted";
	}
	if (!_setting_bool("application/analytics/enabled", false)) {
		return false;
	}
	if (_setting_bool("application/analytics/require_user_consent", false) && get_consent() != "accepted") {
		return false;
	}
	return true;
#else
	return false;
#endif
}

String Analytics::_iso_timestamp() const {
	if (!Time::get_singleton()) {
		return String();
	}
	return Time::get_singleton()->get_datetime_string_from_system(true, false) + "Z";
}

String Analytics::_size_string(const Size2i &p_size) const {
	return vformat("%dx%d", p_size.x, p_size.y);
}

void Analytics::_attach_editor_context(Dictionary &r_props) const {
	if (!OS::get_singleton()) {
		return;
	}
	r_props["os_name"] = OS::get_singleton()->get_name();
	const String os_version = OS::get_singleton()->get_version();
	if (!os_version.is_empty()) {
		r_props["os_version"] = os_version;
	}
	if (Engine::get_singleton()) {
		r_props["arch"] = Engine::get_singleton()->get_architecture_name();
	}
	const String dist = OS::get_singleton()->get_distribution_name();
	if (!dist.is_empty()) {
		r_props["distribution"] = dist;
	}
	r_props["locale"] = OS::get_singleton()->get_locale();
	if (DisplayServer::get_singleton() && DisplayServer::get_singleton()->get_screen_count() > 0) {
		r_props["display_server"] = DisplayServer::get_singleton()->get_name();
		r_props["screen_count"] = DisplayServer::get_singleton()->get_screen_count();
		r_props["primary_screen_size"] = _size_string(DisplayServer::get_singleton()->screen_get_size(0));
		r_props["hi_dpi"] = DisplayServer::get_singleton()->screen_get_scale(0) > 1.0f;
	}
	r_props["editor_video_driver"] = OS::get_singleton()->get_current_rendering_driver_name();
	r_props["renderer"] = OS::get_singleton()->get_current_rendering_method();
#ifdef REAL_T_IS_DOUBLE
	r_props["precision"] = "double";
#else
	r_props["precision"] = "float";
#endif
}

void Analytics::_attach_game_context(Dictionary &r_props) const {
	if (!OS::get_singleton()) {
		return;
	}
	r_props["os_name"] = OS::get_singleton()->get_name();
	if (Engine::get_singleton()) {
		r_props["arch"] = Engine::get_singleton()->get_architecture_name();
	}
	r_props["locale"] = OS::get_singleton()->get_locale();
	if (DisplayServer::get_singleton()) {
		r_props["display_server"] = DisplayServer::get_singleton()->get_name();
		if (DisplayServer::get_singleton()->get_screen_count() > 0) {
			r_props["screen_size"] = _size_string(DisplayServer::get_singleton()->screen_get_size(0));
		}
		if (DisplayServer::get_singleton()->get_window_list().size() > 0) {
			r_props["window_size"] = _size_string(DisplayServer::get_singleton()->window_get_size());
		}
	}
	const String renderer = OS::get_singleton()->get_current_rendering_method();
	if (!renderer.is_empty()) {
		r_props["renderer"] = renderer;
	}
}

Dictionary Analytics::_build_event(const String &p_event, const Dictionary &p_properties) const {
	Dictionary ev;
	ev["app_id"] = get_app_id();
	ev["build_id"] = get_build_id();
	ev["app_version"] = get_app_version();
	ev["build_channel"] = get_build_channel();
	ev["engine_version"] = String(VERSION_FULL_NAME);
	ev["session_id"] = session_id;
	const bool anonymous = is_anonymous();
	ev["anonymous"] = anonymous;
	if (!anonymous) {
		ev["device_uid"] = get_device_uid();
	}
	ev["event"] = p_event;
	ev["timestamp"] = _iso_timestamp();
	Dictionary props = p_properties.duplicate();
	if (!anonymous) {
		if (!user_id.is_empty()) {
			props["user_id"] = user_id;
		}
		const Array keys = user_properties.keys();
		for (int i = 0; i < keys.size(); i++) {
			props[keys[i]] = user_properties[keys[i]];
		}
	} else {
		props.erase("user_id");
		props.erase("device_uid");
	}
	ev["properties"] = props;
	return ev;
}

void Analytics::_enqueue(const String &p_event, const Dictionary &p_properties, bool p_editor_context) {
	if (!_can_queue() || p_event.is_empty()) {
		return;
	}
	Dictionary props = p_properties.duplicate();
	if (p_editor_context) {
		_attach_editor_context(props);
	} else {
		_attach_game_context(props);
	}
	const Error err = AnalyticsQueue::append(get_queue_directory(), _build_event(p_event, props));
	if (err != OK) {
		WARN_PRINT(vformat("Analytics: failed to queue event '%s' (%d).", p_event, err));
	}
}

void Analytics::report_user_data_dir_ready() {
	if (shutdown_notified || session_start_sent || _is_editor_context()) {
		return;
	}
	if (_can_queue()) {
		_enqueue("session_start", Dictionary(), false);
		session_start_sent = true;
	}
}

void Analytics::notify_editor_ready() {
#ifdef TOOLS_ENABLED
	if (shutdown_notified || editor_launched_sent || !_is_editor_context()) {
		return;
	}
	if (_can_queue()) {
		_enqueue("editor_launched", Dictionary(), true);
		editor_launched_sent = true;
	}
#endif
}

void Analytics::notify_shutdown() {
	if (shutdown_notified) {
		return;
	}
	shutdown_notified = true;
	const double session_sec = OS::get_singleton() ? (double)(OS::get_singleton()->get_ticks_msec() - session_start_msec) / 1000.0 : 0.0;
	Dictionary props;
	props["session_sec"] = session_sec;
	if (editor_launched_sent) {
		_enqueue("editor_session_ended", props, true);
	}
	if (session_start_sent) {
		_enqueue("session_end", props, false);
	}
}

bool Analytics::is_available() const {
	return true;
}

bool Analytics::is_enabled() const {
	return _can_queue();
}

String Analytics::get_consent() const {
	const Consent cli = _parse_consent(_cmdline_value("--analytics="));
	if (cli != CONSENT_UNSET) {
		return cli == CONSENT_ACCEPTED ? "accepted" : "declined";
	}
	const Consent env = _parse_consent(_env("BLAZIUM_ANALYTICS_CONSENT"));
	if (env != CONSENT_UNSET) {
		return env == CONSENT_ACCEPTED ? "accepted" : "declined";
	}
	if (has_consent_override) {
		if (consent_override == CONSENT_ACCEPTED) {
			return "accepted";
		}
		if (consent_override == CONSENT_DECLINED) {
			return "declined";
		}
	}
#ifdef TOOLS_ENABLED
	if (_is_editor_context() && EditorSettings::get_singleton() && EditorSettings::get_singleton()->has_setting("blazium/analytics/consent")) {
		const Consent persisted = _parse_consent(String(EditorSettings::get_singleton()->get_setting("blazium/analytics/consent")));
		if (persisted == CONSENT_ACCEPTED) {
			return "accepted";
		}
		if (persisted == CONSENT_DECLINED) {
			return "declined";
		}
	}
#endif
	return "unset";
}

void Analytics::set_consent(bool p_accepted) {
	has_consent_override = true;
	consent_override = p_accepted ? CONSENT_ACCEPTED : CONSENT_DECLINED;
#ifdef TOOLS_ENABLED
	if (_is_editor_context() && EditorSettings::get_singleton()) {
		EditorSettings::get_singleton()->set_setting("blazium/analytics/consent", p_accepted ? "accepted" : "declined");
	}
#endif
}

bool Analytics::is_anonymous() const {
	const String mode = _cmdline_value("--analytics-mode=").strip_edges().to_lower();
	if (mode == "anonymous") {
		return true;
	}
	if (mode == "identified") {
		return false;
	}
	const String env = _env("BLAZIUM_ANALYTICS_ANONYMOUS");
	if (!env.is_empty()) {
		return _parse_bool_env(env, true);
	}
	if (has_anonymous_override) {
		return anonymous_override;
	}
#ifdef TOOLS_ENABLED
	if (_is_editor_context() && EditorSettings::get_singleton() && EditorSettings::get_singleton()->has_setting("blazium/analytics/anonymous")) {
		return bool(EditorSettings::get_singleton()->get_setting("blazium/analytics/anonymous"));
	}
#endif
	return _setting_bool("application/analytics/anonymous", true);
}

void Analytics::set_anonymous(bool p_anonymous) {
	has_anonymous_override = true;
	anonymous_override = p_anonymous;
#ifdef TOOLS_ENABLED
	if (_is_editor_context() && EditorSettings::get_singleton()) {
		EditorSettings::get_singleton()->set_setting("blazium/analytics/anonymous", p_anonymous);
	}
#endif
}

String Analytics::get_app_id() const {
	const String cli = _cmdline_value("--analytics-app-id=");
	if (!cli.is_empty()) {
		return cli;
	}
	const String env = _env("BLAZIUM_ANALYTICS_APP_ID");
	if (!env.is_empty()) {
		return env;
	}
#ifdef TOOLS_ENABLED
	if (_is_editor_context()) {
		if (EditorSettings::get_singleton() && EditorSettings::get_singleton()->has_setting("blazium/analytics/app_id")) {
			const String override_id = String(EditorSettings::get_singleton()->get_setting("blazium/analytics/app_id"));
			if (!override_id.is_empty()) {
				return override_id;
			}
		}
		const String baked = String(ANALYTICS_EDITOR_APP_ID);
		return baked.is_empty() ? String("blazium-editor") : baked;
	}
#endif
	const String project_id = _setting_string("application/analytics/app_id", String());
	if (!project_id.is_empty()) {
		return project_id;
	}
	return _setting_string("application/config/name", String());
}

String Analytics::get_build_id() const {
	const String cli = _cmdline_value("--analytics-build-id=");
	if (!cli.is_empty()) {
		return cli;
	}
	const String env = _env("BLAZIUM_ANALYTICS_BUILD_ID");
	if (!env.is_empty()) {
		return env;
	}
#ifdef TOOLS_ENABLED
	if (_is_editor_context()) {
		if (EditorSettings::get_singleton() && EditorSettings::get_singleton()->has_setting("blazium/analytics/build_id")) {
			const String override_id = String(EditorSettings::get_singleton()->get_setting("blazium/analytics/build_id"));
			if (!override_id.is_empty()) {
				return override_id;
			}
		}
		const String baked = String(ANALYTICS_EDITOR_BUILD_ID);
		if (!baked.is_empty()) {
			return baked;
		}
		if (Engine::get_singleton()) {
			const Dictionary info = Engine::get_singleton()->get_version_info();
			const String hash = String(info.get("hash", String()));
			if (!hash.is_empty()) {
				return hash;
			}
		}
		return String(VERSION_HASH);
	}
#endif
	const String project_id = _setting_string("application/analytics/build_id", String());
	if (!project_id.is_empty()) {
		return project_id;
	}
	return _setting_string("application/config/version", String());
}

String Analytics::get_app_version() const {
#ifdef TOOLS_ENABLED
	if (_is_editor_context()) {
		return String(VERSION_FULL_NAME);
	}
#endif
	const String project_version = _setting_string("application/analytics/app_version", String());
	if (!project_version.is_empty()) {
		return project_version;
	}
	return _setting_string("application/config/version", String());
}

String Analytics::get_build_channel() const {
#ifdef TOOLS_ENABLED
	if (_is_editor_context()) {
		const String baked = String(ANALYTICS_EDITOR_BUILD_CHANNEL);
		return baked.is_empty() ? String("dev") : baked;
	}
#endif
	return _setting_string("application/analytics/build_channel", "release");
}

String Analytics::get_device_uid() const {
	if (is_anonymous() || !OS::get_singleton()) {
		return String();
	}
	return OS::get_singleton()->get_unique_id();
}

String Analytics::get_endpoint() const {
#ifdef TOOLS_ENABLED
	if (_is_editor_context()) {
		if (EditorSettings::get_singleton() && EditorSettings::get_singleton()->has_setting("blazium/analytics/endpoint")) {
			const String override_ep = String(EditorSettings::get_singleton()->get_setting("blazium/analytics/endpoint"));
			if (!override_ep.is_empty()) {
				return override_ep;
			}
		}
		return String(ANALYTICS_EDITOR_ENDPOINT);
	}
#endif
	const String env = _env("BLAZIUM_ANALYTICS_ENDPOINT");
	if (!env.is_empty()) {
		return env;
	}
	return _setting_string("application/analytics/endpoint", String());
}

String Analytics::get_api_key() const {
	const String env = _env("BLAZIUM_ANALYTICS_API_KEY");
	if (!env.is_empty()) {
		return env;
	}
#ifdef TOOLS_ENABLED
	if (_is_editor_context()) {
		if (EditorSettings::get_singleton() && EditorSettings::get_singleton()->has_setting("blazium/analytics/api_key")) {
			const String override_key = String(EditorSettings::get_singleton()->get_setting("blazium/analytics/api_key"));
			if (!override_key.is_empty()) {
				return override_key;
			}
		}
		return String(ANALYTICS_EDITOR_API_KEY);
	}
#endif
	return _setting_string("application/analytics/api_key", String());
}

String Analytics::get_queue_directory() const {
	const String env = _env("BLAZIUM_ANALYTICS_QUEUE_DIR");
	if (!env.is_empty()) {
		return env;
	}
#ifdef TOOLS_ENABLED
	if (_is_editor_context()) {
		String base;
		if (EditorPaths::get_singleton()) {
			base = EditorPaths::get_singleton()->get_data_dir();
		} else if (OS::get_singleton()) {
			base = OS::get_singleton()->get_data_path();
		}
		return base.path_join("analytics");
	}
#endif
	if (OS::get_singleton()) {
		return OS::get_singleton()->get_user_data_dir().path_join("analytics");
	}
	return String();
}

void Analytics::track(const String &p_event, const Dictionary &p_properties) {
	_enqueue(p_event, p_properties, _is_editor_context());
}

void Analytics::identify(const String &p_user_id) {
	if (is_anonymous()) {
		return;
	}
	user_id = p_user_id;
}

void Analytics::set_user_properties(const Dictionary &p_properties) {
	if (is_anonymous()) {
		return;
	}
	user_properties = p_properties.duplicate();
}

void Analytics::flush() {
	if (!_can_queue()) {
		emit_signal(SNAME("flush_failed"), "Analytics is not enabled.");
		return;
	}
	const String endpoint = get_endpoint();
	if (endpoint.is_empty()) {
		print_line("Analytics: flush dropped (empty endpoint); events remain queued.");
		emit_signal(SNAME("flush_failed"), "Empty endpoint.");
		return;
	}
	const TypedArray<Dictionary> events = AnalyticsQueue::load(get_queue_directory());
	if (events.is_empty()) {
		emit_signal(SNAME("flush_succeeded"), 0);
		return;
	}
	Dictionary body;
	body["events"] = events;
	const String json = JSON::stringify(body, "", false);
	const CharString utf8 = json.utf8();
	Vector<uint8_t> bytes;
	bytes.resize(utf8.length());
	memcpy(bytes.ptrw(), utf8.get_data(), utf8.length());

	const String ua = vformat("BlaziumAnalytics/%s", VERSION_FULL_NAME);
	int timeout_sec = 15;
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("application/analytics/timeout_sec")) {
		timeout_sec = MAX((int)GLOBAL_GET("application/analytics/timeout_sec"), 1);
	}
	const bool verify_tls = _setting_bool("application/analytics/verify_tls", true);
	const AnalyticsHTTPResult result = AnalyticsHTTP::post_events(endpoint, get_api_key(), ua, bytes, timeout_sec, verify_tls);
	if (result.error == OK) {
		AnalyticsQueue::clear(get_queue_directory());
		emit_signal(SNAME("flush_succeeded"), events.size());
	} else {
		emit_signal(SNAME("flush_failed"), result.message);
	}
}

int Analytics::get_queue_size() const {
	return AnalyticsQueue::size(get_queue_directory());
}

Dictionary Analytics::get_resolved_config() const {
	Dictionary cfg;
	cfg["available"] = is_available();
	cfg["enabled"] = is_enabled();
	cfg["consent"] = get_consent();
	cfg["anonymous"] = is_anonymous();
	cfg["app_id"] = get_app_id();
	cfg["build_id"] = get_build_id();
	cfg["app_version"] = get_app_version();
	cfg["build_channel"] = get_build_channel();
	cfg["engine_version"] = String(VERSION_FULL_NAME);
	cfg["session_id"] = session_id;
	cfg["device_uid"] = get_device_uid();
	cfg["endpoint"] = get_endpoint();
	cfg["queue_dir"] = get_queue_directory();
	cfg["queue_size"] = get_queue_size();
	cfg["editor_context"] = _is_editor_context();
	return cfg;
}

void Analytics::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_available"), &Analytics::is_available);
	ClassDB::bind_method(D_METHOD("is_enabled"), &Analytics::is_enabled);
	ClassDB::bind_method(D_METHOD("get_consent"), &Analytics::get_consent);
	ClassDB::bind_method(D_METHOD("set_consent", "accepted"), &Analytics::set_consent);
	ClassDB::bind_method(D_METHOD("is_anonymous"), &Analytics::is_anonymous);
	ClassDB::bind_method(D_METHOD("set_anonymous", "anonymous"), &Analytics::set_anonymous);
	ClassDB::bind_method(D_METHOD("get_app_id"), &Analytics::get_app_id);
	ClassDB::bind_method(D_METHOD("get_build_id"), &Analytics::get_build_id);
	ClassDB::bind_method(D_METHOD("get_app_version"), &Analytics::get_app_version);
	ClassDB::bind_method(D_METHOD("get_build_channel"), &Analytics::get_build_channel);
	ClassDB::bind_method(D_METHOD("get_device_uid"), &Analytics::get_device_uid);
	ClassDB::bind_method(D_METHOD("track", "event", "properties"), &Analytics::track, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("identify", "user_id"), &Analytics::identify);
	ClassDB::bind_method(D_METHOD("set_user_properties", "properties"), &Analytics::set_user_properties);
	ClassDB::bind_method(D_METHOD("flush"), &Analytics::flush);
	ClassDB::bind_method(D_METHOD("get_queue_size"), &Analytics::get_queue_size);
	ClassDB::bind_method(D_METHOD("get_resolved_config"), &Analytics::get_resolved_config);

	ADD_SIGNAL(MethodInfo("flush_succeeded", PropertyInfo(Variant::INT, "count")));
	ADD_SIGNAL(MethodInfo("flush_failed", PropertyInfo(Variant::STRING, "message")));

	BIND_ENUM_CONSTANT(CONSENT_UNSET);
	BIND_ENUM_CONSTANT(CONSENT_ACCEPTED);
	BIND_ENUM_CONSTANT(CONSENT_DECLINED);
}
