/**************************************************************************/
/*  justamcp_resource_tools_io.cpp                                        */
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

#include "../justamcp_editor_filesystem.h"
#include "../justamcp_editor_plugin.h"
#include "../justamcp_read_limits.h"
#include "justamcp_resource_tools.h"

#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"

#ifdef MODULE_ASSETTAGS_ENABLED
#include "modules/assettags/asset_tag_registry.h"
#endif
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/image_loader.h"
#include "core/io/json.h"
#include "core/io/resource_saver.h"
#include "core/os/thread.h"
#include "editor/editor_file_system.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
#include "scene/2d/sprite_2d.h"
#include "scene/2d/tile_map.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/sprite_3d.h"
#include "scene/resources/2d/tile_set.h"
#include "scene/resources/material.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/shader.h"
#include "scene/resources/texture.h"
#include "scene/resources/theme.h"

String JustAMCPResourceTools::_ensure_res_path(const String &p_path) {
	if (p_path.begins_with("res://")) {
		return p_path;
	}
	if (p_path.begins_with("/")) {
		String project_abs = ProjectSettings::get_singleton()->globalize_path("res://");
		if (p_path.begins_with(project_abs)) {
			String rel = p_path.substr(project_abs.length());
			return "res://" + rel;
		}
	}
	return "res://" + p_path;
}

void JustAMCPResourceTools::_refresh_filesystem(const String &p_path) {
	JustAMCPEditorFilesystem::refresh_path(p_path);
}

Variant JustAMCPResourceTools::_parse_value(const Variant &p_value) {
	if (p_value.get_type() == Variant::DICTIONARY) {
		Dictionary value = p_value;
		String t;
		if (value.has("type")) {
			t = value["type"];
		} else if (value.has("_type")) {
			t = value["_type"];
		}

		if (!t.is_empty()) {
			if (t == "Vector2") {
				return Vector2(value.get("x", 0), value.get("y", 0));
			}
			if (t == "Vector3") {
				return Vector3(value.get("x", 0), value.get("y", 0), value.get("z", 0));
			}
			if (t == "Color") {
				return Color(value.get("r", 1), value.get("g", 1), value.get("b", 1), value.get("a", 1));
			}
			if (t == "Vector2i") {
				return Vector2i(value.get("x", 0), value.get("y", 0));
			}
			if (t == "Vector3i") {
				return Vector3i(value.get("x", 0), value.get("y", 0), value.get("z", 0));
			}
			if (t == "Rect2") {
				return Rect2(value.get("x", 0), value.get("y", 0), value.get("width", 0), value.get("height", 0));
			}
			if (t == "NodePath") {
				return NodePath(String(value.get("path", "")));
			}
		}
	} else if (p_value.get_type() == Variant::ARRAY) {
		Array arr = p_value;
		Array result;
		for (int i = 0; i < arr.size(); i++) {
			result.push_back(_parse_value(arr[i]));
		}
		return result;
	}
	return p_value;
}

void JustAMCPResourceTools::_set_resource_properties(Ref<Resource> p_resource, const Variant &p_properties) {
	if (p_resource.is_null()) {
		return;
	}

	Variant props = p_properties;
	if (p_properties.get_type() == Variant::STRING) {
		Ref<JSON> json;
		json.instantiate();
		if (json->parse(p_properties) == OK) {
			props = json->get_data();
		} else {
			return;
		}
	}

	if (props.get_type() != Variant::DICTIONARY) {
		return;
	}

	Dictionary dict = props;
	Array keys = dict.keys();
	for (int i = 0; i < keys.size(); i++) {
		String key = keys[i];
		Variant val = _parse_value(dict[key]);
		p_resource->set(key, val);
	}
}

Dictionary JustAMCPResourceTools::_parse_properties_dict(const Variant &p_raw) {
	if (p_raw.get_type() == Variant::DICTIONARY) {
		return p_raw;
	}
	if (p_raw.get_type() == Variant::STRING) {
		String text = p_raw;
		if (!text.is_empty()) {
			Ref<JSON> json;
			json.instantiate();
			if (json->parse(text) == OK) {
				Variant parsed_data = json->get_data();
				if (parsed_data.get_type() == Variant::DICTIONARY) {
					return parsed_data;
				}
			}
		}
	}
	return Dictionary();
}

Ref<Theme> JustAMCPResourceTools::_load_theme(const String &p_theme_path) {
	Ref<Theme> theme = ResourceLoader::load(p_theme_path);
	if (theme.is_valid()) {
		return theme;
	}
	Ref<Theme> new_theme;
	new_theme.instantiate();
	return new_theme;
}

Error JustAMCPResourceTools::_save_scene_root(Node *p_root, const String &p_scene_path) {
	Ref<PackedScene> packed;
	packed.instantiate();
	Error err = packed->pack(p_root);
	if (err != OK) {
		return err;
	}
	return ResourceSaver::save(packed, p_scene_path);
}

void JustAMCPResourceTools::_list_resources_recursive(const String &p_path, const String &p_type_filter, Array &r_results, int p_max_results) {
	if (p_max_results > 0 && r_results.size() >= p_max_results) {
		return;
	}
	Ref<DirAccess> dir = DirAccess::open(p_path);
	if (dir.is_null()) {
		return;
	}

	dir->list_dir_begin();
	String file_name = dir->get_next();
	while (!file_name.is_empty()) {
		if (p_max_results > 0 && r_results.size() >= p_max_results) {
			break;
		}
		if (file_name.begins_with(".")) {
			file_name = dir->get_next();
			continue;
		}
		String full_path = p_path.path_join(file_name);
		if (dir->current_is_dir()) {
			_list_resources_recursive(full_path, p_type_filter, r_results, p_max_results);
		} else {
			String ext = file_name.get_extension().to_lower();
			if (ext == "tres" || ext == "res" || ext == "tscn" || ext == "material" || ext == "mesh") {
				Dictionary item;
				item["path"] = full_path;
				item["extension"] = ext;
				if (!p_type_filter.is_empty()) {
					String efs_type;
					if (Thread::is_main_thread() && EditorFileSystem::get_singleton()) {
						efs_type = EditorFileSystem::get_singleton()->get_file_type(full_path);
					}
					if (!efs_type.is_empty()) {
						if (efs_type != p_type_filter && !ClassDB::is_parent_class(StringName(efs_type), StringName(p_type_filter))) {
							file_name = dir->get_next();
							continue;
						}
						item["type"] = efs_type;
					} else {
						Ref<Resource> resource = ResourceLoader::load(full_path);
						if (resource.is_null() || !resource->is_class(p_type_filter)) {
							file_name = dir->get_next();
							continue;
						}
						item["type"] = resource->get_class();
					}
				}
				r_results.push_back(item);
			}
		}
		file_name = dir->get_next();
	}
	dir->list_dir_end();
}

Dictionary JustAMCPResourceTools::get_resource_info(const Dictionary &p_args) {
	String type = p_args.get("type", "");
	String path = p_args.get("path", p_args.get("resource_path", ""));
	if (type.is_empty() && path.is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "type or path is required";
		return ret;
	}

	if (type.is_empty() && !path.is_empty()) {
		Ref<Resource> resource = ResourceLoader::load(path);
		if (resource.is_valid()) {
			type = resource->get_class();
		}
	}

	if (type.is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Unable to resolve resource type";
		return ret;
	}

	if (!ClassDB::class_exists(StringName(type))) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Unknown class: " + type;
		return ret;
	}

	List<PropertyInfo> props;
	ClassDB::get_property_list(StringName(type), &props);

	Array properties;
	for (const PropertyInfo &E : props) {
		if (!(E.usage & PROPERTY_USAGE_EDITOR)) {
			continue;
		}
		Dictionary d;
		d["name"] = E.name;
		d["type"] = Variant::get_type_name(E.type);
		properties.push_back(d);
	}

	Dictionary data;
	data["type"] = type;
	data["properties"] = properties;
	data["can_instantiate"] = ClassDB::can_instantiate(StringName(type));
	data["parent"] = ClassDB::get_parent_class(StringName(type));
	if (!path.is_empty()) {
		data["path"] = path;
#ifdef MODULE_ASSETTAGS_ENABLED
		if (AssetTagRegistry *registry = AssetTagRegistry::get_singleton()) {
			data["tags"] = registry->get_tags_for_asset(path);
		}
#endif
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["data"] = data;
	return ret;
}

Dictionary JustAMCPResourceTools::read_resource_file(const Dictionary &p_args) {
	String res_path = _ensure_res_path(p_args.get("path", p_args.get("resource_path", "")));
	if (res_path == "res://") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "path is required.";
		return ret;
	}
	if (!FileAccess::exists(res_path)) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Resource file not found: " + res_path;
		return ret;
	}

	int64_t file_size = 0;
	if (!justamcp_file_within_read_limit(res_path, JUSTAMCP_MAX_SYNC_READ_BYTES, file_size)) {
		return justamcp_read_limit_error(res_path, file_size, JUSTAMCP_MAX_SYNC_READ_BYTES);
	}

	Ref<FileAccess> file = FileAccess::open(res_path, FileAccess::READ);
	if (file.is_valid()) {
		String content = file->get_as_text();
		file->close();
		Dictionary ret;
		ret["ok"] = true;
		ret["path"] = res_path;
		ret["content"] = content;
		ret["size"] = content.length();
		return ret;
	}

	Ref<Resource> resource = ResourceLoader::load(res_path);
	if (resource.is_valid()) {
		Dictionary data;
		data["class"] = resource->get_class();
		data["path"] = resource->get_path();
		data["resource_name"] = resource->get_name();
		Dictionary ret;
		ret["ok"] = true;
		ret["path"] = res_path;
		ret["resource"] = data;
		return ret;
	}

	Dictionary ret;
	ret["ok"] = false;
	ret["error"] = "Failed to read resource: " + res_path;
	return ret;
}

Dictionary JustAMCPResourceTools::edit_resource_file(const Dictionary &p_args) {
	String res_path = _ensure_res_path(p_args.get("path", p_args.get("resource_path", "")));
	if (res_path == "res://") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "path is required.";
		return ret;
	}
	if (!FileAccess::exists(res_path)) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Resource file not found: " + res_path;
		return ret;
	}

	if (p_args.has("content")) {
		Ref<FileAccess> file = FileAccess::open(res_path, FileAccess::WRITE);
		if (file.is_null()) {
			Dictionary ret;
			ret["ok"] = false;
			ret["error"] = "Failed to open resource for writing.";
			return ret;
		}
		String content = p_args["content"];
		file->store_string(content);
		file->close();
		_refresh_filesystem(res_path);
		Dictionary ret;
		ret["ok"] = true;
		ret["path"] = res_path;
		ret["bytes_written"] = content.utf8().length();
		return ret;
	}

	Ref<Resource> resource = ResourceLoader::load(res_path);
	if (resource.is_null()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Failed to load resource for property editing.";
		return ret;
	}
	if (p_args.has("properties")) {
		_set_resource_properties(resource, p_args["properties"]);
	}
	Error err = ResourceSaver::save(resource, res_path);

	Dictionary ret;
	ret["ok"] = err == OK;
	ret["path"] = res_path;
	if (err != OK) {
		ret["error"] = "Failed to save resource: " + itos(err);
		return ret;
	}
	_refresh_filesystem(res_path);
	return ret;
}

Dictionary JustAMCPResourceTools::get_resource_preview(const Dictionary &p_args) {
	String res_path = _ensure_res_path(p_args.get("path", p_args.get("resource_path", "")));
	Dictionary ret;
	ret["path"] = res_path;
	if (res_path == "res://") {
		ret["ok"] = false;
		ret["error"] = "path is required.";
		return ret;
	}
	Ref<Resource> resource = ResourceLoader::load(res_path);
	if (resource.is_null()) {
		ret["ok"] = false;
		ret["error"] = "Failed to load resource: " + res_path;
		return ret;
	}
	ret["ok"] = true;
	ret["class"] = resource->get_class();
	ret["resource_name"] = resource->get_name();
	ret["resource_path"] = resource->get_path();
	ret["description"] = vformat("%s resource at %s", resource->get_class(), res_path);
	return ret;
}

Dictionary JustAMCPResourceTools::list_resource_files(const Dictionary &p_args) {
	String path = _ensure_res_path(p_args.get("path", "res://"));
	String type_filter = p_args.get("type_filter", "");
	int max_results = int(p_args.get("max_results", 500));
	if (max_results <= 0) {
		max_results = 500;
	}
	if (max_results > 5000) {
		max_results = 5000;
	}
	Array resources;
	_list_resources_recursive(path, type_filter, resources, max_results);

	Dictionary ret;
	ret["ok"] = true;
	ret["resources"] = resources;
	ret["count"] = resources.size();
	ret["path"] = path;
	ret["max_results"] = max_results;
	ret["truncated"] = resources.size() >= max_results;
	if (!type_filter.is_empty()) {
		ret["type_filter"] = type_filter;
	}
	return ret;
}

Dictionary JustAMCPResourceTools::save_resource_as(const Dictionary &p_args) {
	String source_path = _ensure_res_path(p_args.get("resource_path", p_args.get("source_path", p_args.get("path", ""))));
	String dest_path = _ensure_res_path(p_args.get("dest_path", p_args.get("save_path", "")));
	if (source_path == "res://" || dest_path == "res://") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "resource_path/source_path and dest_path/save_path are required.";
		return ret;
	}

	Ref<Resource> resource = ResourceLoader::load(source_path);
	if (resource.is_null()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Resource not found: " + source_path;
		return ret;
	}
	String dir_path = dest_path.get_base_dir();
	if (!DirAccess::dir_exists_absolute(dir_path)) {
		DirAccess::make_dir_recursive_absolute(dir_path);
	}
	Error err = ResourceSaver::save(resource, dest_path);

	Dictionary ret;
	ret["ok"] = err == OK;
	ret["source_path"] = source_path;
	ret["path"] = dest_path;
	if (err != OK) {
		ret["error"] = "Failed to save resource: " + itos(err);
		return ret;
	}
	_refresh_filesystem(dest_path);
	return ret;
}

Dictionary JustAMCPResourceTools::get_resource_dependencies(const Dictionary &p_args) {
	String path = _ensure_res_path(p_args.get("path", ""));
	if (path == "res://") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "path is required.";
		return ret;
	}

	List<String> deps;
	ResourceLoader::get_dependencies(path, &deps);
	Array dependencies;
	for (const String &dep : deps) {
		dependencies.push_back(dep);
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["path"] = path;
	ret["dependencies"] = dependencies;
	ret["count"] = dependencies.size();
	return ret;
}

Dictionary JustAMCPResourceTools::import_asset_copy(const Dictionary &p_args) {
	String source_path = p_args.get("source_path", "");
	String dest_path = _ensure_res_path(p_args.get("dest_path", ""));
	if (source_path.is_empty() || dest_path == "res://" || dest_path == "res://test") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "source_path and dest_path are required.";
		return ret;
	}
	if (dest_path.get_file().is_empty() || dest_path.get_extension().is_empty() || DirAccess::dir_exists_absolute(dest_path)) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "dest_path must be a file path with an extension, not a directory: " + dest_path;
		return ret;
	}
	if (!FileAccess::exists(source_path)) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "source_path not found: " + source_path;
		return ret;
	}
	String dir_path = dest_path.get_base_dir();
	if (!DirAccess::dir_exists_absolute(dir_path)) {
		DirAccess::make_dir_recursive_absolute(dir_path);
	}
	Error err = DirAccess::copy_absolute(source_path, dest_path);

	Dictionary ret;
	ret["ok"] = err == OK;
	ret["source"] = source_path;
	ret["destination"] = dest_path;
	if (err != OK) {
		ret["error"] = "Failed to copy asset: " + itos(err);
		return ret;
	}
	_refresh_filesystem(dest_path);
	return ret;
}

Dictionary JustAMCPResourceTools::manage_resource_autoloads(const Dictionary &p_args) {
	String action = p_args.get("action", p_args.get("operation", "list"));
	ProjectSettings *settings = ProjectSettings::get_singleton();
	if (action == "list") {
		Dictionary autoloads;
		List<PropertyInfo> props;
		settings->get_property_list(&props);
		for (const PropertyInfo &prop : props) {
			if (prop.name.begins_with("autoload/")) {
				autoloads[prop.name.substr(9)] = settings->get_setting(prop.name);
			}
		}
		Dictionary ret;
		ret["ok"] = true;
		ret["autoloads"] = autoloads;
		return ret;
	}
	String name = p_args.get("name", "");
	String path = p_args.get("path", "");
	if (name.is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "name is required.";
		return ret;
	}
	String setting_name = "autoload/" + name;
	if (action == "add") {
		if (path.is_empty()) {
			Dictionary ret;
			ret["ok"] = false;
			ret["error"] = "path is required for add.";
			return ret;
		}
		settings->set_setting(setting_name, path.begins_with("*") ? path : "*" + path);
	} else if (action == "remove") {
		settings->clear(setting_name);
	} else {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Unknown autoload action: " + action;
		return ret;
	}
	Error err = settings->save();
	Dictionary ret;
	ret["ok"] = err == OK;
	ret["action"] = action;
	ret["name"] = name;
	if (!path.is_empty()) {
		ret["path"] = path;
	}
	if (err != OK) {
		ret["error"] = "Failed to save project settings.";
	}
	return ret;
}

Dictionary JustAMCPResourceTools::resource_import_asset(const Dictionary &p_args) {
	String res_path = _ensure_res_path(p_args.get("path", ""));
	if (res_path == "res://" || res_path == "res://test") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "path is required for import_asset.";
		return ret;
	}
	if (res_path.get_file().is_empty() || res_path.get_extension().is_empty() || DirAccess::dir_exists_absolute(res_path)) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "path must be a file with an extension, not a directory: " + res_path;
		return ret;
	}

	if (editor_plugin && editor_plugin->get_editor_interface() && EditorFileSystem::get_singleton()) {
		Vector<String> files;
		files.push_back(res_path);

		EditorFileSystem::get_singleton()->call_deferred(SNAME("reimport_files"), files);
		Dictionary ret;
		ret["ok"] = true;
		ret["path"] = res_path;
		ret["message"] = "Reimport triggered asynchronously.";
		return ret;
	}

	Dictionary ret;
	ret["ok"] = false;
	ret["error"] = "Editor file system or interface unavailable.";
	return ret;
}

#endif
