/**************************************************************************/
/*  analytics_project_settings.cpp                                        */
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

#include "analytics_project_settings.h"

#include "core/config/app_identity.h"
#include "core/config/project_settings.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_settings.h"

#ifndef ANALYTICS_EDITOR_APP_ID
#define ANALYTICS_EDITOR_APP_ID ""
#endif
#ifndef ANALYTICS_EDITOR_BUILD_ID
#define ANALYTICS_EDITOR_BUILD_ID ""
#endif
#ifndef ANALYTICS_EDITOR_ENDPOINT
#define ANALYTICS_EDITOR_ENDPOINT ""
#endif
#endif

void analytics_register_project_settings() {
	GLOBAL_DEF_BASIC("application/analytics/enabled", false);
	GLOBAL_DEF_BASIC("application/analytics/require_user_consent", false);
	GLOBAL_DEF_BASIC("application/analytics/anonymous", true);
	GLOBAL_DEF_BASIC("application/analytics/endpoint", String());
	GLOBAL_DEF_BASIC("application/analytics/app_id", String());
	GLOBAL_DEF_BASIC("application/analytics/build_id", String());
	GLOBAL_DEF_BASIC("application/analytics/app_version", String());
	GLOBAL_DEF_BASIC("application/analytics/build_channel", "release");
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "application/analytics/timeout_sec", PROPERTY_HINT_RANGE, "1,120,1"), 15);
	GLOBAL_DEF_BASIC("application/analytics/verify_tls", true);
}

#ifdef TOOLS_ENABLED
void analytics_register_editor_settings() {
	if (!EditorSettings::get_singleton()) {
		return;
	}
	EditorSettings *es = EditorSettings::get_singleton();
	EDITOR_DEF_BASIC("blazium/analytics/consent", "unset");
	EDITOR_DEF_BASIC("blazium/analytics/anonymous", true);
	const String baked_app_id = String(ANALYTICS_EDITOR_APP_ID).strip_edges();
	const String baked_build_id = String(ANALYTICS_EDITOR_BUILD_ID).strip_edges();
	const String baked_endpoint = String(ANALYTICS_EDITOR_ENDPOINT).strip_edges();
	const String display_app_id = baked_app_id.is_empty() ? AppIdentity::editor_fallback_app_id() : baked_app_id;
	const String display_build_id = baked_build_id.is_empty() ? AppIdentity::editor_fallback_build_id() : baked_build_id;
	EDITOR_DEF_BASIC("blazium/analytics/endpoint", baked_endpoint);
	EDITOR_DEF_BASIC("blazium/analytics/app_id", display_app_id);
	EDITOR_DEF_BASIC("blazium/analytics/build_id", display_build_id);
	es->set_setting("blazium/analytics/endpoint", baked_endpoint);
	es->set_setting("blazium/analytics/app_id", display_app_id);
	es->set_setting("blazium/analytics/build_id", display_build_id);
	es->add_property_hint(PropertyInfo(Variant::STRING, "blazium/analytics/consent", PROPERTY_HINT_ENUM, "unset,accepted,declined"));
	const uint32_t read_only = PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_READ_ONLY;
	es->add_property_hint(PropertyInfo(Variant::STRING, "blazium/analytics/app_id", PROPERTY_HINT_NONE, "", read_only));
	es->add_property_hint(PropertyInfo(Variant::STRING, "blazium/analytics/build_id", PROPERTY_HINT_NONE, "", read_only));
	es->add_property_hint(PropertyInfo(Variant::STRING, "blazium/analytics/endpoint", PROPERTY_HINT_NONE, "", read_only));
}
#endif
