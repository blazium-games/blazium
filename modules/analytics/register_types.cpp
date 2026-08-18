/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "register_types.h"

#include "analytics.h"
#include "analytics_project_settings.h"

#include "core/config/engine.h"
#include "core/object/class_db.h"

#ifdef TOOLS_ENABLED
#include "analytics_editor_export_plugin.h"
#include "editor/export/editor_export.h"
#endif

static Analytics *analytics = nullptr;
#ifdef TOOLS_ENABLED
static Ref<AnalyticsEditorExportPlugin> analytics_export_plugin;
#endif

void initialize_analytics_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_CORE) {
		GDREGISTER_CLASS(Analytics);
		analytics_register_project_settings();
		analytics = memnew(Analytics);
		Engine::get_singleton()->add_singleton(Engine::Singleton("Analytics", analytics));
		return;
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		analytics_register_editor_settings();
		if (EditorExport::get_singleton()) {
			analytics_export_plugin.instantiate();
			EditorExport::get_singleton()->add_export_plugin(analytics_export_plugin);
		}
		if (analytics) {
			analytics->notify_editor_ready();
		}
	}
#endif
}

void uninitialize_analytics_module(ModuleInitializationLevel p_level) {
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		if (analytics) {
			analytics->notify_shutdown();
		}
		if (analytics_export_plugin.is_valid() && EditorExport::get_singleton()) {
			EditorExport::get_singleton()->remove_export_plugin(analytics_export_plugin);
		}
		analytics_export_plugin.unref();
		return;
	}
#endif
	if (p_level != MODULE_INITIALIZATION_LEVEL_CORE) {
		return;
	}
	if (analytics) {
		analytics->notify_shutdown();
		Engine::get_singleton()->remove_singleton("Analytics");
		memdelete(analytics);
		analytics = nullptr;
	}
}
