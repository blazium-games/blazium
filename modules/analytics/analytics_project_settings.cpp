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

#include "core/config/project_settings.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_settings.h"
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
	GLOBAL_DEF_BASIC("application/analytics/api_key", String());
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "application/analytics/timeout_sec", PROPERTY_HINT_RANGE, "1,120,1"), 15);
	GLOBAL_DEF_BASIC("application/analytics/verify_tls", true);
}

#ifdef TOOLS_ENABLED
void analytics_register_editor_settings() {
	if (!EditorSettings::get_singleton()) {
		return;
	}
	EDITOR_DEF_BASIC("blazium/analytics/consent", "unset");
	EDITOR_DEF_BASIC("blazium/analytics/anonymous", true);
	EDITOR_DEF_BASIC("blazium/analytics/endpoint", String());
	EDITOR_DEF_BASIC("blazium/analytics/app_id", String());
	EDITOR_DEF_BASIC("blazium/analytics/build_id", String());
	EDITOR_DEF_BASIC("blazium/analytics/api_key", String());
	if (EditorSettings::get_singleton()) {
		EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "blazium/analytics/consent", PROPERTY_HINT_ENUM, "unset,accepted,declined"));
	}
}
#endif
