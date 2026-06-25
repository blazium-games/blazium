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
#include "discord.h"
#include "discord_auth_result.h"
#include "discord_frame_hook.h"
#include "scene/main/scene_tree.h"

#ifdef TESTS_ENABLED
#include "tests/test_discord_module.h"
#endif

static Discord *discord_singleton = nullptr;
static bool discord_frame_hook_connected = false;

namespace {

void discord_frame_callback() {
	if (discord_singleton == nullptr || !discord_singleton->is_client_active()) {
		return;
	}
	discord_singleton->run_callbacks();
}

} //namespace

void discord_try_connect_frame_hook() {
	if (discord_frame_hook_connected) {
		return;
	}

	SceneTree *scene_tree = SceneTree::get_singleton();
	if (scene_tree == nullptr) {
		return;
	}

	scene_tree->connect("process_frame", callable_mp_static(&discord_frame_callback));
	discord_frame_hook_connected = true;

	if (discord_singleton != nullptr) {
		discord_singleton->note_frame_hook_connected();
	}
}

bool discord_is_frame_hook_connected() {
	return discord_frame_hook_connected;
}

namespace {

void disconnect_discord_frame_hook() {
	if (!discord_frame_hook_connected) {
		return;
	}

	SceneTree *scene_tree = SceneTree::get_singleton();
	if (scene_tree != nullptr) {
		scene_tree->disconnect("process_frame", callable_mp_static(&discord_frame_callback));
	}

	discord_frame_hook_connected = false;
}

} //namespace

void initialize_discord_module_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(DiscordAuthResult);
		GDREGISTER_CLASS(Discord);
		discord_singleton = memnew(Discord);
		Engine::get_singleton()->add_singleton(Engine::Singleton("Discord", Discord::get_singleton()));
		discord_try_connect_frame_hook();
	}
}

void uninitialize_discord_module_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		disconnect_discord_frame_hook();
		if (discord_singleton) {
			Engine::get_singleton()->remove_singleton("Discord");
			memdelete(discord_singleton);
			discord_singleton = nullptr;
		}
	}
}
