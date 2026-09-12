/**************************************************************************/
/*  navimesh_exporter.cpp                                                 */
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

#ifdef TOOLS_ENABLED

#include "navimesh_exporter.h"

#include "navimesh_export_collect.h"
#include "navimesh_export_serialize.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "scene/2d/navigation/navigation_link_2d.h"
#include "scene/2d/navigation/navigation_region_2d.h"
#include "scene/3d/navigation/navigation_link_3d.h"
#include "scene/3d/navigation/navigation_region_3d.h"
#include "scene/main/node.h"
#include "core/object/class_db.h"

NavimeshExporter *NavimeshExporter::singleton = nullptr;

NavimeshExporter *NavimeshExporter::get_singleton() {
	return singleton;
}

NavimeshExporter::NavimeshExporter() {
	ERR_FAIL_COND_MSG(singleton != nullptr, "NavimeshExporter singleton already exists.");
	singleton = this;
}

NavimeshExporter::~NavimeshExporter() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

String NavimeshExporter::dimension_to_string(Dimension p_dimension) {
	switch (p_dimension) {
		case DIMENSION_2D:
			return "2d";
		case DIMENSION_3D:
			return "3d";
		default:
			return "both";
	}
}

NavimeshExporter::Dimension NavimeshExporter::dimension_from_string(const String &p_value) {
	const String value = p_value.to_lower();
	if (value == "2d" || value == "2") {
		return DIMENSION_2D;
	}
	if (value == "3d" || value == "3") {
		return DIMENSION_3D;
	}
	return DIMENSION_BOTH;
}

String NavimeshExporter::format_to_string(Format p_format) {
	switch (p_format) {
		case FORMAT_JSON:
			return "json";
		case FORMAT_BIN:
			return "bin";
		default:
			return "both";
	}
}

NavimeshExporter::Format NavimeshExporter::format_from_string(const String &p_value) {
	const String value = p_value.to_lower();
	if (value == "json") {
		return FORMAT_JSON;
	}
	if (value == "bin" || value == "binary") {
		return FORMAT_BIN;
	}
	return FORMAT_BOTH;
}

String NavimeshExporter::mode_to_string(Mode p_mode) {
	return p_mode == MODE_COMBINED ? "combined" : "individual";
}

NavimeshExporter::Mode NavimeshExporter::mode_from_string(const String &p_value) {
	return p_value.to_lower() == "combined" ? MODE_COMBINED : MODE_INDIVIDUAL;
}

Error NavimeshExporter::write_payload(const Dictionary &p_payload, const String &p_base_path, Format p_format, PackedStringArray &r_written) {
	const String base = p_base_path.get_basename();
	const String dir = base.get_base_dir();
	if (!dir.is_empty()) {
		DirAccess::make_dir_recursive_absolute(ProjectSettings::get_singleton()->globalize_path(dir));
	}

	Error err = OK;
	if (p_format == FORMAT_JSON || p_format == FORMAT_BOTH) {
		const String json_path = base + ".nav.json";
		err = NavimeshExportSerialize::write_json(p_payload, json_path);
		if (err != OK) {
			return err;
		}
		r_written.push_back(json_path);
	}
	if (p_format == FORMAT_BIN || p_format == FORMAT_BOTH) {
		const String bin_path = base + ".nav.bin";
		err = NavimeshExportSerialize::write_binary(p_payload, bin_path);
		if (err != OK) {
			return err;
		}
		r_written.push_back(bin_path);
	}
	return OK;
}

Error NavimeshExporter::bake_node(Node *p_node) {
	const Error err = NavimeshExportCollect::bake_node(p_node);
	if (err == OK) {
		emit_signal(SNAME("bake_finished"), p_node);
	}
	return err;
}

static void _bake_matching_regions(Node *p_node, NavimeshExporter::Dimension p_dimension, int &r_baked) {
	if (!p_node) {
		return;
	}
	if (Object::cast_to<NavigationRegion3D>(p_node) && p_dimension != NavimeshExporter::DIMENSION_2D) {
		NavimeshExportCollect::bake_node(p_node);
		r_baked++;
	} else if (Object::cast_to<NavigationRegion2D>(p_node) && p_dimension != NavimeshExporter::DIMENSION_3D) {
		NavimeshExportCollect::bake_node(p_node);
		r_baked++;
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		_bake_matching_regions(p_node->get_child(i), p_dimension, r_baked);
	}
}

Dictionary NavimeshExporter::bake_scene(const String &p_scene_path, Dimension p_dimension) {
	Dictionary result;
	Error load_err = OK;
	Node *instance = NavimeshExportCollect::instantiate_scene(p_scene_path, load_err);
	if (!instance) {
		result["ok"] = false;
		result["error"] = vformat("Failed to load scene '%s'.", p_scene_path);
		result["baked"] = 0;
		return result;
	}
	int baked = 0;
	_bake_matching_regions(instance, p_dimension, baked);
	result["ok"] = true;
	result["baked"] = baked;
	result["scene"] = p_scene_path;
	NavimeshExportCollect::free_instance(instance);
	return result;
}

Dictionary NavimeshExporter::collect_from_node(Node *p_node, Dimension p_dimension) {
	ERR_FAIL_NULL_V(p_node, Dictionary());
	const bool single_region = Object::cast_to<NavigationRegion3D>(p_node) || Object::cast_to<NavigationRegion2D>(p_node);
	return NavimeshExportCollect::collect_from_node(p_node, p_dimension, single_region);
}

Error NavimeshExporter::export_node(Node *p_node, const String &p_output_path, Format p_format) {
	ERR_FAIL_NULL_V(p_node, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V_MSG(p_output_path.is_empty(), ERR_INVALID_PARAMETER, "NavimeshExporter: output path is empty.");

	Dimension dimension = DIMENSION_BOTH;
	if (Object::cast_to<NavigationRegion2D>(p_node) || Object::cast_to<NavigationLink2D>(p_node)) {
		dimension = DIMENSION_2D;
	} else if (Object::cast_to<NavigationRegion3D>(p_node) || Object::cast_to<NavigationLink3D>(p_node)) {
		dimension = DIMENSION_3D;
	}

	const bool single_region = Object::cast_to<NavigationRegion3D>(p_node) || Object::cast_to<NavigationRegion2D>(p_node);
	if (single_region) {
		bake_node(p_node);
	} else {
		Node *root = p_node;
		while (root->get_parent()) {
			root = root->get_parent();
		}
		int baked = 0;
		_bake_matching_regions(root, dimension, baked);
	}

	const Dictionary payload = NavimeshExportCollect::collect_from_node(p_node, dimension, single_region);
	PackedStringArray written;
	const Error err = write_payload(payload, p_output_path, p_format, written);
	Dictionary summary;
	summary["ok"] = err == OK;
	summary["paths"] = written;
	emit_signal(SNAME("export_finished"), written, summary);
	return err;
}

Dictionary NavimeshExporter::export_scene(const String &p_scene_path, const String &p_output_dir, Format p_format, Dimension p_dimension) {
	Dictionary result;
	result["scene"] = p_scene_path;
	Error load_err = OK;
	Node *instance = NavimeshExportCollect::instantiate_scene(p_scene_path, load_err);
	if (!instance) {
		result["ok"] = false;
		result["error"] = vformat("Failed to load scene '%s'.", p_scene_path);
		result["skipped"] = true;
		return result;
	}

	if (!NavimeshExportCollect::scene_has_nav(instance, p_dimension)) {
		result["ok"] = true;
		result["skipped"] = true;
		result["reason"] = "no matching navigation regions";
		NavimeshExportCollect::free_instance(instance);
		return result;
	}

	int baked = 0;
	_bake_matching_regions(instance, p_dimension, baked);
	Dictionary payload = NavimeshExportCollect::collect_from_node(instance, p_dimension, false);
	Dictionary source = payload.get("source", Dictionary());
	source["scene_path"] = p_scene_path;
	payload["source"] = source;

	const String stem = p_scene_path.get_file().get_basename();
	const String out_dir = p_output_dir.is_empty() ? String("res://.navimesh_export/") : p_output_dir;
	const String base = out_dir.path_join(stem);
	PackedStringArray written;
	const Error err = write_payload(payload, base, p_format, written);
	result["ok"] = err == OK;
	result["skipped"] = false;
	result["baked"] = baked;
	result["paths"] = written;
	if (err != OK) {
		result["error"] = vformat("Failed to write export for '%s'.", p_scene_path);
	}
	emit_signal(SNAME("export_finished"), written, result);
	NavimeshExportCollect::free_instance(instance);
	return result;
}

PackedStringArray NavimeshExporter::list_scenes(const String &p_root, Dimension p_dimension) {
	PackedStringArray all;
	NavimeshExportCollect::collect_scene_paths(p_root.is_empty() ? String("res://") : p_root, all);
	PackedStringArray matched;
	for (int i = 0; i < all.size(); i++) {
		if (NavimeshExportCollect::scene_file_has_nav(all[i], p_dimension)) {
			matched.push_back(all[i]);
		}
	}
	return matched;
}

Dictionary NavimeshExporter::export_project(const String &p_scenes, const String &p_output_dir, Mode p_mode, Format p_format, Dimension p_dimension) {
	Dictionary result;
	const String scenes_root = p_scenes.is_empty() ? String("res://") : p_scenes;
	const String out_dir = p_output_dir.is_empty() ? String("res://.navimesh_export/") : p_output_dir;
	PackedStringArray candidates;
	NavimeshExportCollect::collect_scene_paths(scenes_root, candidates);

	Array exported;
	Array skipped;
	Array levels;
	PackedStringArray all_paths;

	for (int i = 0; i < candidates.size(); i++) {
		const String scene_path = candidates[i];
		if (!NavimeshExportCollect::scene_file_has_nav(scene_path, p_dimension)) {
			Dictionary skip;
			skip["scene"] = scene_path;
			skip["reason"] = "no matching navigation regions";
			skipped.push_back(skip);
			continue;
		}

		if (p_mode == MODE_INDIVIDUAL) {
			Dictionary scene_result = export_scene(scene_path, out_dir, p_format, p_dimension);
			if (scene_result.get("skipped", false)) {
				skipped.push_back(scene_result);
			} else {
				exported.push_back(scene_result);
				const PackedStringArray paths = scene_result.get("paths", PackedStringArray());
				for (int p = 0; p < paths.size(); p++) {
					all_paths.push_back(paths[p]);
				}
			}
		} else {
			Error load_err = OK;
			Node *instance = NavimeshExportCollect::instantiate_scene(scene_path, load_err);
			if (!instance || !NavimeshExportCollect::scene_has_nav(instance, p_dimension)) {
				Dictionary skip;
				skip["scene"] = scene_path;
				skip["reason"] = "no matching navigation regions";
				skipped.push_back(skip);
				NavimeshExportCollect::free_instance(instance);
				continue;
			}
			int baked = 0;
			_bake_matching_regions(instance, p_dimension, baked);
			Dictionary payload = NavimeshExportCollect::collect_from_node(instance, p_dimension, false);
			Dictionary source = payload.get("source", Dictionary());
			source["scene_path"] = scene_path;
			payload["source"] = source;
			levels.push_back(payload);
			Dictionary scene_result;
			scene_result["ok"] = true;
			scene_result["scene"] = scene_path;
			scene_result["baked"] = baked;
			exported.push_back(scene_result);
			NavimeshExportCollect::free_instance(instance);
		}
	}

	if (p_mode == MODE_COMBINED) {
		Dictionary bundle;
		bundle["format"] = NavimeshExportSerialize::FORMAT_BUNDLE;
		bundle["version"] = (int)NavimeshExportSerialize::VERSION;
		Dictionary source;
		source["engine"] = "blazium";
		source["scene_path"] = scenes_root;
		bundle["source"] = source;
		NavimeshExportCollect::collect_maps(bundle);
		bundle["levels"] = levels;
		bundle["regions"] = Array();
		bundle["links"] = Array();
		bundle["obstacles"] = Array();
		const Error err = write_payload(bundle, out_dir.path_join("navimesh_bundle"), p_format, all_paths);
		result["write_error"] = err;
	}

	result["ok"] = true;
	result["exported"] = exported;
	result["skipped"] = skipped;
	result["paths"] = all_paths;
	result["mode"] = mode_to_string(p_mode);
	emit_signal(SNAME("export_finished"), all_paths, result);
	return result;
}

void NavimeshExporter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("bake_node", "node"), &NavimeshExporter::bake_node);
	ClassDB::bind_method(D_METHOD("bake_scene", "scene_path", "dimension"), &NavimeshExporter::bake_scene, DEFVAL(DIMENSION_BOTH));
	ClassDB::bind_method(D_METHOD("collect_from_node", "node", "dimension"), &NavimeshExporter::collect_from_node, DEFVAL(DIMENSION_BOTH));
	ClassDB::bind_method(D_METHOD("export_node", "node", "output_path", "format"), &NavimeshExporter::export_node, DEFVAL(FORMAT_BOTH));
	ClassDB::bind_method(D_METHOD("export_scene", "scene_path", "output_dir", "format", "dimension"), &NavimeshExporter::export_scene, DEFVAL(FORMAT_BOTH), DEFVAL(DIMENSION_BOTH));
	ClassDB::bind_method(D_METHOD("export_project", "scenes", "output_dir", "mode", "format", "dimension"), &NavimeshExporter::export_project, DEFVAL("res://"), DEFVAL("res://.navimesh_export/"), DEFVAL(MODE_INDIVIDUAL), DEFVAL(FORMAT_BOTH), DEFVAL(DIMENSION_BOTH));
	ClassDB::bind_method(D_METHOD("list_scenes", "root", "dimension"), &NavimeshExporter::list_scenes, DEFVAL("res://"), DEFVAL(DIMENSION_BOTH));

	ADD_SIGNAL(MethodInfo("bake_finished", PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "Node")));
	ADD_SIGNAL(MethodInfo("export_finished", PropertyInfo(Variant::PACKED_STRING_ARRAY, "paths"), PropertyInfo(Variant::DICTIONARY, "summary")));

	BIND_ENUM_CONSTANT(DIMENSION_2D);
	BIND_ENUM_CONSTANT(DIMENSION_3D);
	BIND_ENUM_CONSTANT(DIMENSION_BOTH);
	BIND_ENUM_CONSTANT(FORMAT_JSON);
	BIND_ENUM_CONSTANT(FORMAT_BIN);
	BIND_ENUM_CONSTANT(FORMAT_BOTH);
	BIND_ENUM_CONSTANT(MODE_INDIVIDUAL);
	BIND_ENUM_CONSTANT(MODE_COMBINED);
}

#endif
