/**************************************************************************/
/*  navimesh_export_tools.cpp                                             */
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
#ifdef MODULE_JUSTAMCP_ENABLED

#include "navimesh_export_collect.h"
#include "navimesh_export_tools.h"
#include "navimesh_exporter.h"

#include "core/object/class_db.h"
#include "scene/main/node.h"

#include "modules/justamcp/tools/justamcp_tool_schema_builder.h"

static Dictionary _export_region_from_scene(NavimeshExporter *p_exporter, const String &p_scene, const String &p_region_path, const String &p_output, NavimeshExporter::Format p_format) {
	Dictionary result;
	Error load_err = OK;
	Node *instance = NavimeshExportCollect::instantiate_scene(p_scene, load_err);
	if (!instance) {
		result["ok"] = false;
		result["error"] = vformat("Failed to load scene '%s'.", p_scene);
		return result;
	}
	Node *region = instance->get_node_or_null(NodePath(p_region_path));
	if (!region) {
		result["ok"] = false;
		result["error"] = vformat("Region '%s' not found in '%s'.", p_region_path, p_scene);
		NavimeshExportCollect::free_instance(instance);
		return result;
	}
	const String stem = p_region_path.get_file().is_empty() ? String(region->get_name()) : p_region_path.get_file();
	const String base = p_output.path_join(stem.validate_filename());
	const Error err = p_exporter->export_node(region, base, p_format);
	result["ok"] = err == OK;
	result["scene"] = p_scene;
	result["region_path"] = p_region_path;
	if (err != OK) {
		result["error"] = vformat("Failed to export region '%s'.", p_region_path);
	}
	NavimeshExportCollect::free_instance(instance);
	return result;
}

void NavimeshExportTools::_bind_methods() {
	ClassDB::bind_method(D_METHOD("provide_tool_schemas", "register_only", "ignore_settings", "include_disabled_tools"), &NavimeshExportTools::provide_tool_schemas, DEFVAL(false), DEFVAL(false), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("execute_tool", "tool_name", "arguments"), &NavimeshExportTools::execute_tool);
}

Dictionary NavimeshExportTools::_make_error(const String &p_message) const {
	Dictionary result;
	result["ok"] = false;
	result["error"] = p_message;
	return result;
}

String NavimeshExportTools::_normalize_tool_name(const String &p_tool_name) const {
	if (p_tool_name.begins_with("blazium_")) {
		return p_tool_name.substr(8);
	}
	return p_tool_name;
}

Array NavimeshExportTools::provide_tool_schemas(bool p_register_only, bool p_ignore_settings, bool p_include_disabled_tools) {
	return get_tool_schemas(p_register_only, p_ignore_settings, p_include_disabled_tools);
}

Array NavimeshExportTools::get_tool_schemas(bool p_register_only, bool p_ignore_settings, bool p_include_disabled_tools) {
	Array tools;
	const String current_category = "navimesh_export_tools";
	const bool is_core = true;

	auto add_schema = [&](const String &p_name, const String &p_desc, const Vector<String> &p_props, const Vector<String> &p_req) {
		const String full_name = "blazium_" + p_name;
		if (p_register_only) {
			JustAMCPToolSchemaBuilder::register_tool_settings(current_category, full_name, is_core);
			return;
		}
		bool cat_enabled = true;
		bool tool_enabled = true;
		if (!JustAMCPToolSchemaBuilder::resolve_tool_enabled(current_category, full_name, p_ignore_settings, p_include_disabled_tools, cat_enabled, tool_enabled)) {
			return;
		}
		tools.push_back(JustAMCPToolSchemaBuilder::build_tool_schema(full_name, p_desc, current_category, cat_enabled && tool_enabled, p_props, p_req));
	};

	add_schema("navimesh_list_scenes", "Lists project scenes that contain NavigationRegion2D and/or NavigationRegion3D.",
			Vector<String>{ "root", "string", "dimension", "string" }, Vector<String>{});
	add_schema("navimesh_export_scene", "Bakes and exports navigation from a scene to JSON and/or binary files.",
			Vector<String>{ "scene", "string", "output", "string", "format", "string", "dimension", "string", "scope", "string", "region_path", "string" }, Vector<String>{ "scene" });
	add_schema("navimesh_export_project", "Bakes and exports navigation from many scenes as individual files or one combined bundle.",
			Vector<String>{ "scenes", "string", "output", "string", "mode", "string", "format", "string", "dimension", "string" }, Vector<String>{});

	return tools;
}

Dictionary NavimeshExportTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	NavimeshExporter *exporter = NavimeshExporter::get_singleton();
	if (!exporter) {
		return _make_error("NavimeshExporter is not available.");
	}

	const String tool_name = _normalize_tool_name(p_tool_name);
	const String dimension_str = p_args.get("dimension", "both");
	const NavimeshExporter::Dimension dimension = NavimeshExporter::dimension_from_string(dimension_str);
	const String format_str = p_args.get("format", "both");
	const NavimeshExporter::Format format = NavimeshExporter::format_from_string(format_str);

	if (tool_name == "navimesh_list_scenes") {
		const String root = p_args.get("root", "res://");
		PackedStringArray all;
		NavimeshExportCollect::collect_scene_paths(root.is_empty() ? String("res://") : String(root), all);
		PackedStringArray scenes;
		Array details;
		for (int i = 0; i < all.size(); i++) {
			const NavimeshExportCollect::SceneNavScan scan = NavimeshExportCollect::scan_scene_file(all[i]);
			if (!scan.matches(dimension)) {
				continue;
			}
			scenes.push_back(all[i]);
			Dictionary row;
			row["path"] = all[i];
			row["dimensions"] = scan.dimensions_string();
			details.push_back(row);
		}
		Dictionary result;
		result["ok"] = true;
		result["scenes"] = scenes;
		result["items"] = details;
		return result;
	}

	if (tool_name == "navimesh_export_scene") {
		const String scene = p_args.get("scene", "");
		if (scene.is_empty()) {
			return _make_error("scene is required.");
		}
		const String output = p_args.get("output", "res://.navimesh_export/");
		const String scope = String(p_args.get("scope", "scene")).to_lower();
		if (scope == "region") {
			const String region_path = p_args.get("region_path", "");
			if (region_path.is_empty()) {
				return _make_error("region_path is required when scope is region.");
			}
			return _export_region_from_scene(exporter, scene, region_path, output, format);
		}
		return exporter->export_scene(scene, output, format, dimension);
	}

	if (tool_name == "navimesh_export_project") {
		const String scenes = p_args.get("scenes", "res://");
		const String output = p_args.get("output", "res://.navimesh_export/");
		const NavimeshExporter::Mode mode = NavimeshExporter::mode_from_string(p_args.get("mode", "individual"));
		return exporter->export_project(scenes, output, mode, format, dimension);
	}

	return _make_error("Unknown navimesh export tool: " + tool_name);
}

#endif
#endif
