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
#include "scene/main/scene_tree.h"
#include "steam.h"
#include "steam_achievement_info.h"
#include "steam_auth_result.h"
#include "steam_inventory_item.h"
#include "steam_item_definition.h"

#ifdef TOOLS_ENABLED
#include "core/object/class_db.h"
#include "editor/plugins/editor_plugin.h"
#include "editor/steam_editor_plugin.h"
#endif

#ifdef TESTS_ENABLED
#include "tests/test_steam.h"
#endif

static Steam *steam_singleton = nullptr;
static bool steam_frame_hook_connected = false;

namespace {

void steam_frame_callback() {
	if (steam_singleton == nullptr || !steam_singleton->is_initialized()) {
		return;
	}
	steam_singleton->poll_callbacks();
}

void connect_steam_frame_hook() {
	if (steam_frame_hook_connected) {
		return;
	}

	SceneTree *scene_tree = SceneTree::get_singleton();
	if (scene_tree == nullptr) {
		return;
	}

	scene_tree->connect("process_frame", callable_mp_static(&steam_frame_callback));
	steam_frame_hook_connected = true;
}

void disconnect_steam_frame_hook() {
	if (!steam_frame_hook_connected) {
		return;
	}

	SceneTree *scene_tree = SceneTree::get_singleton();
	if (scene_tree != nullptr) {
		scene_tree->disconnect("process_frame", callable_mp_static(&steam_frame_callback));
	}

	steam_frame_hook_connected = false;
}

} //namespace

void initialize_steam_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(SteamAuthResult);
		GDREGISTER_CLASS(SteamAchievementInfo);
		GDREGISTER_CLASS(SteamInventoryItem);
		GDREGISTER_CLASS(SteamItemDefinition);
		GDREGISTER_CLASS(Steam);
		steam_singleton = memnew(Steam);
		Engine::get_singleton()->add_singleton(Engine::Singleton("Steam", Steam::get_singleton()));
		connect_steam_frame_hook();
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		GDREGISTER_CLASS(SteamEditorPlugin);
		EditorPlugins::add_by_type<SteamEditorPlugin>();
	}
#endif
}

void uninitialize_steam_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		disconnect_steam_frame_hook();
		if (steam_singleton) {
			Engine::get_singleton()->remove_singleton("Steam");
			memdelete(steam_singleton);
			steam_singleton = nullptr;
		}
	}
}
