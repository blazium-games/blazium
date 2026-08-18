/**************************************************************************/
/*  analytics_editor_export_plugin.cpp                                    */
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

#ifdef TOOLS_ENABLED

#include "analytics_editor_export_plugin.h"

#include "analytics.h"

#include "core/os/os.h"
#include "editor/export/editor_export_platform.h"

void AnalyticsEditorExportPlugin::_export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) {
	(void)p_features;
	(void)p_path;
	(void)p_flags;
	export_debug = p_debug;
	export_start_msec = OS::get_singleton() ? OS::get_singleton()->get_ticks_msec() : 0;
	export_platform = String();
	const Ref<EditorExportPlatform> platform = get_export_platform();
	if (platform.is_valid()) {
		export_platform = platform->get_os_name();
	}
	if (!Analytics::get_singleton()) {
		return;
	}
	Dictionary props;
	props["export_platform"] = export_platform;
	props["export_debug"] = export_debug;
	Analytics::get_singleton()->track("editor_export_started", props);
}

void AnalyticsEditorExportPlugin::_export_end() {
	if (!Analytics::get_singleton()) {
		return;
	}
	Dictionary props;
	props["export_platform"] = export_platform;
	props["export_debug"] = export_debug;
	props["success"] = true;
	const uint64_t now = OS::get_singleton() ? OS::get_singleton()->get_ticks_msec() : export_start_msec;
	props["export_sec"] = (double)(now - export_start_msec) / 1000.0;
	Analytics::get_singleton()->track("editor_export_finished", props);
}

#endif
