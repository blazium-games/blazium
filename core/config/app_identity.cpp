/**************************************************************************/
/*  app_identity.cpp                                                      */
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

#include "app_identity.h"

#include "core/config/project_settings.h"
#include "core/os/os.h"
#include "core/templates/list.h"
#include "core/version.h"

String AppIdentity::cmdline_flag_value(const String &p_flag) {
	if (!OS::get_singleton() || p_flag.is_empty()) {
		return String();
	}
	const String flag = p_flag.begins_with("--") ? p_flag : String("--") + p_flag;
	const String equals = flag + "=";
	const List<String> &args = OS::get_singleton()->get_cmdline_args();
	for (const List<String>::Element *E = args.front(); E; E = E->next()) {
		const String &arg = E->get();
		if (arg == flag && E->next()) {
			return E->next()->get();
		}
		if (arg.begins_with(equals)) {
			return arg.substr(equals.length());
		}
	}
	const List<String> &user_args = OS::get_singleton()->get_cmdline_user_args();
	for (const List<String>::Element *E = user_args.front(); E; E = E->next()) {
		const String &arg = E->get();
		if (arg == flag && E->next()) {
			return E->next()->get();
		}
		if (arg.begins_with(equals)) {
			return arg.substr(equals.length());
		}
	}
	return String();
}

String AppIdentity::cmdline_equals_value(const String &p_prefix) {
	if (!OS::get_singleton() || p_prefix.is_empty()) {
		return String();
	}
	const List<String> &args = OS::get_singleton()->get_cmdline_args();
	for (const List<String>::Element *E = args.front(); E; E = E->next()) {
		if (E->get().begins_with(p_prefix)) {
			return E->get().substr(p_prefix.length());
		}
	}
	return String();
}

String AppIdentity::env_first(const Vector<String> &p_names) {
	if (!OS::get_singleton()) {
		return String();
	}
	for (int i = 0; i < p_names.size(); i++) {
		if (OS::get_singleton()->has_environment(p_names[i])) {
			const String v = OS::get_singleton()->get_environment(p_names[i]).strip_edges();
			if (!v.is_empty()) {
				return v;
			}
		}
	}
	return String();
}

String AppIdentity::project_first(const Vector<String> &p_keys) {
	if (!ProjectSettings::get_singleton()) {
		return String();
	}
	for (int i = 0; i < p_keys.size(); i++) {
		if (!ProjectSettings::get_singleton()->has_setting(p_keys[i])) {
			continue;
		}
		const String v = String(ProjectSettings::get_singleton()->get_setting_with_override(p_keys[i])).strip_edges();
		if (!v.is_empty()) {
			return v;
		}
	}
	return String();
}

String AppIdentity::resolve_app_id(const String &p_baked, const String &p_fallback) {
	if (!p_baked.strip_edges().is_empty()) {
		return p_baked.strip_edges();
	}
	Vector<String> keys;
	keys.push_back("application/crash_reporter/app_id");
	keys.push_back("application/analytics/app_id");
	const String project = project_first(keys);
	if (!project.is_empty()) {
		return project;
	}
	return p_fallback;
}

String AppIdentity::resolve_build_id(const String &p_baked, const String &p_fallback) {
	if (!p_baked.strip_edges().is_empty()) {
		return p_baked.strip_edges();
	}
	Vector<String> keys;
	keys.push_back("application/crash_reporter/build_id");
	keys.push_back("application/analytics/build_id");
	const String project = project_first(keys);
	if (!project.is_empty()) {
		return project;
	}
	return p_fallback;
}

String AppIdentity::editor_fallback_app_id() {
	return String("custom_blazium_engine");
}

String AppIdentity::editor_fallback_build_id() {
	return String(VERSION_HASH);
}
