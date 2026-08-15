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

#include "core/object/class_db.h"
#include "register_types.h"

#include "remote_control_registry.h"
#include "remote_control_server.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"

#ifdef TOOLS_ENABLED
#include "editor/plugins/editor_plugin.h"
#include "remote_control_editor_plugin.h"
#endif

static RemoteControlRegistry *registry = nullptr;
static RemoteControlServer *server = nullptr;

void initialize_remote_control_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(RemoteControlRegistry);
		GDREGISTER_CLASS(RemoteControlServer);

		GLOBAL_DEF_BASIC("blazium/remote_control/server_enabled", false);
		GLOBAL_DEF_BASIC("blazium/remote_control/server_port", 6507);
		GLOBAL_DEF_BASIC("blazium/remote_control/bind_address", "127.0.0.1");
		GLOBAL_DEF_BASIC("blazium/remote_control/token", String());
		GLOBAL_DEF_BASIC("blazium/remote_control/allow_eval", false);
		GLOBAL_DEF_BASIC("blazium/remote_control/allow_runtime", false);

		registry = memnew(RemoteControlRegistry);
		Engine::get_singleton()->add_singleton(Engine::Singleton("RemoteControlRegistry", registry));

		server = memnew(RemoteControlServer);
		Engine::get_singleton()->add_singleton(Engine::Singleton("RemoteControlServer", server));

		bool should_start = RemoteControlServer::should_enable_from_cmdline_or_settings();
#ifndef TOOLS_ENABLED
		const bool allow_runtime = ProjectSettings::get_singleton()->has_setting("blazium/remote_control/allow_runtime")
				? bool(GLOBAL_GET("blazium/remote_control/allow_runtime"))
				: false;
		should_start = should_start && allow_runtime;
#endif
		if (should_start) {
			server->call_deferred("start");
		}
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		EditorPlugins::add_by_type<RemoteControlEditorPlugin>();
	}
#endif
}

void uninitialize_remote_control_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		if (server) {
			server->stop();
			Engine::get_singleton()->remove_singleton("RemoteControlServer");
			memdelete(server);
			server = nullptr;
		}
		if (registry) {
			Engine::get_singleton()->remove_singleton("RemoteControlRegistry");
			memdelete(registry);
			registry = nullptr;
		}
	}
}
