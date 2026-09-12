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
#include "livewallpaper.h"

#ifdef TOOLS_ENABLED
#include "editor/export/windows_livewallpaper_export_platform.h"
#include "editor/livewallpaper_editor_plugin.h"
#include "editor/plugins/editor_plugin.h"
#endif

static LiveWallpaper *livewallpaper_singleton = nullptr;

static void _livewallpaper_boot() {
	if (LiveWallpaper::get_singleton()) {
		LiveWallpaper::get_singleton()->setup_runtime();
	}
}

void initialize_livewallpaper_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_CORE) {
		LiveWallpaper::register_project_settings();
		return;
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		GDREGISTER_VIRTUAL_CLASS(EditorExportPlatformWindowsLiveWallpaper);
		GDREGISTER_CLASS(LiveWallpaperEditorPlugin);
		EditorPlugins::add_by_type<LiveWallpaperEditorPlugin>();
		return;
	}
#endif

	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	GDREGISTER_CLASS(LiveWallpaper);
	livewallpaper_singleton = memnew(LiveWallpaper);
	Engine::get_singleton()->add_singleton(Engine::Singleton("LiveWallpaper", LiveWallpaper::get_singleton()));
	if (MessageQueue::get_singleton()) {
		callable_mp_static(&_livewallpaper_boot).call_deferred();
	}
}

void uninitialize_livewallpaper_module(ModuleInitializationLevel p_level) {
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		return;
	}
#endif
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	Engine::get_singleton()->remove_singleton("LiveWallpaper");
	if (livewallpaper_singleton) {
		memdelete(livewallpaper_singleton);
		livewallpaper_singleton = nullptr;
	}
}
