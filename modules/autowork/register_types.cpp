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

#include "autowork_collector.h"
#include "autowork_config.h"
#include "autowork_doubler.h"
#include "autowork_e2e_config.h"
#include "autowork_e2e_server.h"
#include "autowork_hook_script.h"
#include "autowork_input_sender.h"
#include "autowork_logger.h"
#include "autowork_main.h"
#include "autowork_runtime_ui.h"
#include "autowork_signal_watcher.h"
#include "autowork_spy.h"
#include "autowork_stub_params.h"
#include "autowork_stubber.h"
#include "autowork_test.h"
#include "autowork_vscode_debugger.h"
#include "core/config/project_settings.h"

#ifdef TOOLS_ENABLED
#include "autowork_editor_plugin.h"
#include "editor/plugins/editor_plugin.h"
#endif

void initialize_autowork_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		ClassDB::register_class<Autowork>();
		ClassDB::register_class<AutoworkTest>();
		ClassDB::register_class<AutoworkSpy>();
		ClassDB::register_class<AutoworkStubber>();
		ClassDB::register_class<AutoworkDoubler>();
		ClassDB::register_class<AutoworkStubParams>();
		ClassDB::register_class<AutoworkCollector>();
		ClassDB::register_class<AutoworkLogger>();
		ClassDB::register_class<AutoworkSignalWatcher>();
		ClassDB::register_class<AutoworkSignalHook>();
		ClassDB::register_class<AutoworkInputSender>();
		ClassDB::register_class<AutoworkConfig>();
		ClassDB::register_class<AutoworkVSCodeDebugger>();
		ClassDB::register_class<AutoworkRuntimeUI>();
		ClassDB::register_class<AutoworkHookScript>();
		ClassDB::register_class<AutoworkE2EConfig>();
		ClassDB::register_class<AutoworkE2EServer>();

		GLOBAL_DEF_BASIC("blazium/autowork/e2e_enabled", false);

		if (AutoworkE2EConfig::is_enabled()) {
			ProjectSettings::get_singleton()->set("autoload/AutomationServer", "*AutoworkE2EServer");
		}
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		EditorPlugins::add_by_type<AutoworkEditorPlugin>();
	}
#endif
}

void uninitialize_autowork_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		// Nothing to uninitialize for now
	}
}
