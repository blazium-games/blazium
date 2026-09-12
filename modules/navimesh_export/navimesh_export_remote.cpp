/**************************************************************************/
/*  navimesh_export_remote.cpp                                            */
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

#include "modules/modules_enabled.gen.h"

#ifdef TOOLS_ENABLED
#ifdef MODULE_REMOTE_CONTROL_ENABLED

#include "navimesh_export_remote.h"

#include "navimesh_exporter.h"

#include "core/object/callable_mp.h"
#include "modules/remote_control/remote_control_registry.h"

static Dictionary _cmd_list_scenes(const Dictionary &p_args) {
	NavimeshExporter *exporter = NavimeshExporter::get_singleton();
	Dictionary result;
	if (!exporter) {
		result["ok"] = false;
		result["error"] = "NavimeshExporter is not available.";
		return result;
	}
	const NavimeshExporter::Dimension dimension = NavimeshExporter::dimension_from_string(p_args.get("dimension", "both"));
	result["ok"] = true;
	result["scenes"] = exporter->list_scenes(p_args.get("root", "res://"), dimension);
	return result;
}

static Dictionary _cmd_export_scene(const Dictionary &p_args) {
	NavimeshExporter *exporter = NavimeshExporter::get_singleton();
	if (!exporter) {
		Dictionary result;
		result["ok"] = false;
		result["error"] = "NavimeshExporter is not available.";
		return result;
	}
	const String scene = p_args.get("scene", "");
	if (scene.is_empty()) {
		Dictionary result;
		result["ok"] = false;
		result["error"] = "scene is required.";
		return result;
	}
	return exporter->export_scene(
			scene,
			p_args.get("output", "res://.navimesh_export/"),
			NavimeshExporter::format_from_string(p_args.get("format", "both")),
			NavimeshExporter::dimension_from_string(p_args.get("dimension", "both")));
}

static Dictionary _cmd_export_project(const Dictionary &p_args) {
	NavimeshExporter *exporter = NavimeshExporter::get_singleton();
	if (!exporter) {
		Dictionary result;
		result["ok"] = false;
		result["error"] = "NavimeshExporter is not available.";
		return result;
	}
	return exporter->export_project(
			p_args.get("scenes", "res://"),
			p_args.get("output", "res://.navimesh_export/"),
			NavimeshExporter::mode_from_string(p_args.get("mode", "individual")),
			NavimeshExporter::format_from_string(p_args.get("format", "both")),
			NavimeshExporter::dimension_from_string(p_args.get("dimension", "both")));
}

void NavimeshExportRemote::register_commands() {
	RemoteControlRegistry *registry = RemoteControlRegistry::get_singleton();
	if (!registry) {
		return;
	}
	registry->register_command("navimesh_list_scenes", callable_mp_static(_cmd_list_scenes), "List scenes that contain navigation regions.");
	registry->register_command("navimesh_export_scene", callable_mp_static(_cmd_export_scene), "Bake and export a scene navimesh.");
	registry->register_command("navimesh_export_project", callable_mp_static(_cmd_export_project), "Bake and export project navimeshes.");
}

#endif
#endif
