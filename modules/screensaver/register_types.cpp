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

#include "core/config/engine.h"
#include "core/object/message_queue.h"
#include "screensaver.h"

#ifdef TOOLS_ENABLED
#include "editor/export/windows_screensaver_export_platform.h"
#include "editor/plugins/editor_plugin.h"
#include "editor/screensaver_editor_plugin.h"
#endif

static Screensaver *screensaver_singleton = nullptr;

static void _screensaver_boot() {
	if (Screensaver::get_singleton()) {
		Screensaver::get_singleton()->setup_runtime();
	}
}

void initialize_screensaver_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_CORE) {
		Screensaver::register_project_settings();
		return;
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		GDREGISTER_VIRTUAL_CLASS(EditorExportPlatformWindowsScreensaver);
		GDREGISTER_CLASS(ScreensaverEditorPlugin);
		EditorPlugins::add_by_type<ScreensaverEditorPlugin>();
		return;
	}
#endif

	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	GDREGISTER_CLASS(Screensaver);
	screensaver_singleton = memnew(Screensaver);
	Engine::get_singleton()->add_singleton(Engine::Singleton("Screensaver", Screensaver::get_singleton()));
	if (MessageQueue::get_singleton()) {
		callable_mp_static(&_screensaver_boot).call_deferred();
	}
}

void uninitialize_screensaver_module(ModuleInitializationLevel p_level) {
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		return;
	}
#endif
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	Engine::get_singleton()->remove_singleton("Screensaver");
	if (screensaver_singleton) {
		memdelete(screensaver_singleton);
		screensaver_singleton = nullptr;
	}
}
