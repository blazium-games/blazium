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

#include "warcry_client.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/object/class_db.h"
#include "scene/main/scene_tree.h"

#ifdef TOOLS_ENABLED
#include "editor/plugins/editor_plugin.h"
#include "editor/warcry_editor_plugin.h"
#endif

static WarcryClient *warcry_singleton = nullptr;
static bool warcry_frame_hook_connected = false;

namespace {

void warcry_frame_callback() {
	if (warcry_singleton) {
		warcry_singleton->poll();
	}
}

void connect_warcry_frame_hook() {
	if (warcry_frame_hook_connected) {
		return;
	}
	SceneTree *tree = SceneTree::get_singleton();
	if (!tree) {
		return;
	}
	tree->connect(SNAME("process_frame"), callable_mp_static(&warcry_frame_callback));
	warcry_frame_hook_connected = true;
}

void disconnect_warcry_frame_hook() {
	if (!warcry_frame_hook_connected) {
		return;
	}
	SceneTree *tree = SceneTree::get_singleton();
	if (tree) {
		tree->disconnect(SNAME("process_frame"), callable_mp_static(&warcry_frame_callback));
	}
	warcry_frame_hook_connected = false;
}

} // namespace

void initialize_warcry_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(WarcryClient);

		GLOBAL_DEF_BASIC("blazium/warcry/host", "127.0.0.1");
		GLOBAL_DEF_BASIC("blazium/warcry/port", 27015);
		GLOBAL_DEF_BASIC("blazium/warcry/username", "editor");
		GLOBAL_DEF_BASIC("blazium/warcry/password", String());

		warcry_singleton = memnew(WarcryClient);
		Engine::get_singleton()->add_singleton(Engine::Singleton("Warcry", WarcryClient::get_singleton()));
		connect_warcry_frame_hook();
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		EditorPlugins::add_by_type<WarcryEditorPlugin>();
	}
#endif
}

void uninitialize_warcry_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		disconnect_warcry_frame_hook();
		if (warcry_singleton) {
			Engine::get_singleton()->remove_singleton("Warcry");
			memdelete(warcry_singleton);
			warcry_singleton = nullptr;
		}
	}
}
