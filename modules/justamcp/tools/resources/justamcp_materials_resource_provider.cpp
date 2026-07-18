/**************************************************************************/
/*  justamcp_materials_resource_provider.cpp                              */
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

#include "justamcp_materials_resource_provider.h"

#include "../../justamcp_pagination.h"
#include "core/io/json.h"
#include "editor/editor_file_system.h"

static Dictionary g_materials_cache_payload;
static bool g_materials_cache_valid = false;
static const int JUSTAMCP_MATERIALS_CACHE_MAX = 2000;

static bool _is_material_resource_type(const StringName &p_type) {
	if (p_type == StringName()) {
		return true;
	}
	const String type = String(p_type);
	return type.contains("Material") || type == "Shader";
}

static void _collect_materials_from_efs(EditorFileSystemDirectory *p_dir, Array &r_materials) {
	if (!p_dir || r_materials.size() >= JUSTAMCP_MATERIALS_CACHE_MAX) {
		return;
	}
	for (int i = 0; i < p_dir->get_file_count() && r_materials.size() < JUSTAMCP_MATERIALS_CACHE_MAX; i++) {
		const String path = p_dir->get_file_path(i);
		const String lower = path.to_lower();
		if (!lower.ends_with(".tres") && !lower.ends_with(".res") && !lower.ends_with(".material")) {
			continue;
		}
		const StringName file_type = p_dir->get_file_type(i);
		if (!_is_material_resource_type(file_type)) {
			continue;
		}
		Dictionary info;
		info["path"] = path;
		info["type"] = file_type == StringName() ? String("Material") : String(file_type);
		r_materials.push_back(info);
	}
	for (int i = 0; i < p_dir->get_subdir_count() && r_materials.size() < JUSTAMCP_MATERIALS_CACHE_MAX; i++) {
		_collect_materials_from_efs(p_dir->get_subdir(i), r_materials);
	}
}

static void _rebuild_materials_cache() {
	Array materials;
	if (EditorFileSystem::get_singleton() && EditorFileSystem::get_singleton()->get_filesystem()) {
		_collect_materials_from_efs(EditorFileSystem::get_singleton()->get_filesystem(), materials);
	}
	g_materials_cache_payload["materials"] = materials;
	g_materials_cache_payload["count"] = materials.size();
	g_materials_cache_payload["truncated"] = materials.size() >= JUSTAMCP_MATERIALS_CACHE_MAX;
	g_materials_cache_valid = true;
}

static Dictionary _materials_json_contents(const String &p_uri, const Dictionary &p_payload) {
	Dictionary result;
	result["ok"] = true;
	Array contents;
	Dictionary content;
	content["uri"] = p_uri;
	content["mimeType"] = "application/json";
	content["text"] = JSON::stringify(p_payload, "\t");
	contents.push_back(content);
	result["contents"] = contents;
	return result;
}

static String _materials_cursor_from_uri(const String &p_uri) {
	const String prefix = "blazium://materials/cursor/";
	if (p_uri.begins_with(prefix)) {
		return justamcp_pagination_cursor_from_uri_suffix(p_uri.substr(prefix.length()));
	}
	return String();
}

void JustAMCPMaterialsResourceProvider::invalidate_cache() {
	g_materials_cache_valid = false;
	g_materials_cache_payload = Dictionary();
}

void JustAMCPMaterialsResourceProvider::invalidate_cache_for_path(const String &p_changed_path) {
	if (p_changed_path.is_empty()) {
		invalidate_cache();
		return;
	}
	const String lower = p_changed_path.to_lower();
	if (lower == "res://" || lower.ends_with(".tres") || lower.ends_with(".res") || lower.ends_with(".material")) {
		g_materials_cache_valid = false;
		g_materials_cache_payload = Dictionary();
	}
}

bool JustAMCPMaterialsResourceProvider::can_read(const String &p_canonical_uri) {
	return p_canonical_uri == "blazium://materials" || p_canonical_uri.begins_with("blazium://materials/cursor/");
}

Dictionary JustAMCPMaterialsResourceProvider::read(const String &p_uri, const String &p_canonical_uri) {
	(void)p_canonical_uri;
	if (!g_materials_cache_valid) {
		_rebuild_materials_cache();
	}
	const String cursor = _materials_cursor_from_uri(p_uri);
	const Array all_materials = g_materials_cache_payload.get("materials", Array());
	const Dictionary page = justamcp_pagination_slice_array(all_materials, cursor, "materials");
	Dictionary payload;
	payload["materials"] = page.get("materials", Array());
	payload["count"] = int(payload["materials"].operator Array().size());
	if (page.has("nextCursor")) {
		payload["nextCursor"] = page["nextCursor"];
		payload["nextUri"] = justamcp_pagination_next_uri("blazium://materials", String(page["nextCursor"]));
	}
	return _materials_json_contents(p_uri, payload);
}

#endif
