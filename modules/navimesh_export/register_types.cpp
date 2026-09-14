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

#ifdef TOOLS_ENABLED
#include "navimesh_exporter.h"

#include "core/config/engine.h"
#include "core/object/class_db.h"
#include "editor/plugins/editor_plugin.h"

#include "modules/modules_enabled.gen.h"
#include "modules/navimesh_export/editor/navimesh_export_editor_plugin.h"
#ifdef MODULE_JUSTAMCP_ENABLED
#include "navimesh_export_tools.h"

#include "modules/justamcp/tools/justamcp_toolset_registry.h"
#endif
#ifdef MODULE_REMOTE_CONTROL_ENABLED
#include "navimesh_export_remote.h"

#include "core/object/callable_mp.h"
#endif

static NavimeshExporter *navimesh_exporter_singleton = nullptr;
#endif

void initialize_navimesh_export_module(ModuleInitializationLevel p_level) {
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		navimesh_exporter_singleton = memnew(NavimeshExporter);
		GDREGISTER_CLASS(NavimeshExporter);
		Engine::get_singleton()->add_singleton(Engine::Singleton("NavimeshExporter", navimesh_exporter_singleton));

		EditorPlugins::add_by_type<NavimeshExportEditorPlugin>();

#ifdef MODULE_JUSTAMCP_ENABLED
		GDREGISTER_CLASS(NavimeshExportTools);
		if (JustAMCPToolsetRegistry::get_singleton()) {
			NavimeshExportTools *bridge = memnew(NavimeshExportTools);
			JustAMCPToolsetRegistry::get_singleton()->register_toolset_with_owner(
					"NavimeshExport",
					"Bake and export 2D/3D navigation meshes for third-party servers.",
					callable_mp(bridge, &NavimeshExportTools::provide_tool_schemas),
					callable_mp(bridge, &NavimeshExportTools::execute_tool),
					bridge,
					"navimesh_export_tools");
		}
#endif
#ifdef MODULE_REMOTE_CONTROL_ENABLED
		NavimeshExportRemote::register_commands();
#endif
	}
#endif
}

void uninitialize_navimesh_export_module(ModuleInitializationLevel p_level) {
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		if (navimesh_exporter_singleton) {
			Engine::get_singleton()->remove_singleton("NavimeshExporter");
			memdelete(navimesh_exporter_singleton);
			navimesh_exporter_singleton = nullptr;
		}
	}
#endif
}
