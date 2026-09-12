/**************************************************************************/
/*  resource_importer_tiled.cpp                                           */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
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

#include "resource_importer_tiled.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/script_language.h"
#include "scene/resources/packed_scene.h"
#include "tiled_common.h"
#include "tiled_custom_types.h"
#include "tiled_dictionary_builder.h"
#include "tiled_tilemap_creator.h"
#include "tiled_tileset_creator.h"
#include "tiled_tileson_bridge.h"

static void _set_owner_recursive(Node *p_node, Node *p_owner) {
	if (p_node != p_owner) {
		p_node->set_owner(p_owner);
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		_set_owner_recursive(p_node->get_child(i), p_owner);
	}
}

String ResourceImporterTiled::get_importer_name() const {
	return "tiled_tilemap_importer";
}

String ResourceImporterTiled::get_visible_name() const {
	return "Tiled TileMap Scene";
}

void ResourceImporterTiled::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("tmx");
	p_extensions->push_back("tmj");
	p_extensions->push_back("world");
	p_extensions->push_back("lzma");
}

String ResourceImporterTiled::get_save_extension() const {
	return "scn"; // or res for PackedScene
}

String ResourceImporterTiled::get_resource_type() const {
	return "PackedScene";
}

int ResourceImporterTiled::get_preset_count() const {
	return 1;
}

String ResourceImporterTiled::get_preset_name(int p_idx) const {
	return "Default";
}

void ResourceImporterTiled::get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const {
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "use_default_filter"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "add_class_as_metadata"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "add_id_as_metadata"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "no_alternative_tiles"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "map_wangset_to_terrain"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::STRING, "custom_data_prefix"), "data_"));
	r_options->push_back(ImportOption(PropertyInfo(Variant::STRING, "tiled_project_file", PROPERTY_HINT_FILE, "*.tiled-project;Project File"), ""));
	r_options->push_back(ImportOption(PropertyInfo(Variant::STRING, "post_processor", PROPERTY_HINT_FILE, "*.gd;GDScript"), ""));
	r_options->push_back(ImportOption(PropertyInfo(Variant::STRING, "save_tileset_to", PROPERTY_HINT_SAVE_FILE, "*.tres;Resource File"), ""));
}

bool ResourceImporterTiled::get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const {
	return true;
}

Error ResourceImporterTiled::import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	// Gather options
	bool use_default_filter = p_options["use_default_filter"];
	bool add_class_as_metadata = p_options["add_class_as_metadata"];
	bool add_id_as_metadata = p_options["add_id_as_metadata"];
	bool no_alternative_tiles = p_options["no_alternative_tiles"];
	bool map_wangset_to_terrain = p_options["map_wangset_to_terrain"];
	String custom_data_prefix = p_options["custom_data_prefix"];
	String tiled_project_file = p_options["tiled_project_file"];
	String save_tileset_to = p_options["save_tileset_to"];

	TiledTilemapCreator creator;
	creator.set_use_default_filter(use_default_filter);
	creator.set_add_class_as_metadata(add_class_as_metadata);
	creator.set_add_id_as_metadata(add_id_as_metadata);
	creator.set_no_alternative_tiles(no_alternative_tiles);
	creator.set_map_wangset_to_terrain(map_wangset_to_terrain);
	if (!custom_data_prefix.is_empty()) {
		creator.set_custom_data_prefix(custom_data_prefix);
	}
	if (!save_tileset_to.is_empty()) {
		creator.set_save_tileset_to(save_tileset_to);
	}

	Ref<TiledCustomTypes> ct;
	if (!tiled_project_file.is_empty()) {
		ct.instantiate();
		ct->load_custom_types(tiled_project_file);
		creator.set_custom_types(ct);
	}

	Node *node2d = nullptr;
	if (p_source_file.get_extension() == "world") {
		Dictionary world_data = TiledTilesonBridge::parse_world(p_source_file);
		if (world_data["status"] == String("OK")) {
			node2d = memnew(Node2D);
			node2d->set_name(p_source_file.get_file().get_basename());

			Array maps = world_data["maps"];
			for (int i = 0; i < maps.size(); i++) {
				Dictionary map_dict = maps[i];
				String file_name = map_dict["file_name"];
				String sub_map_path = p_source_file.get_base_dir().path_join(file_name);

				if (!FileAccess::exists(sub_map_path)) {
					ERR_PRINT(vformat("World chunk '%s' not found.", sub_map_path));
					continue;
				}

				Node *sub_map = creator.create_tilemap(sub_map_path);
				if (sub_map) {
					Node2D *sub_map_2d = Object::cast_to<Node2D>(sub_map);
					if (sub_map_2d) {
						sub_map_2d->set_position(Vector2(map_dict["pos_x"], map_dict["pos_y"]));
					}
					node2d->add_child(sub_map);
					_set_owner_recursive(sub_map, node2d);
				} else {
					ERR_PRINT(vformat("World chunk '%s' failed to parse.", sub_map_path));
				}
			}
		} else {
			ERR_PRINT("Failed to parse .world file with tileson.");
			return ERR_PARSE_ERROR;
		}
	} else {
		node2d = creator.create_tilemap(p_source_file);
	}

	if (!node2d) {
		return FAILED;
	}

	String post_processor_path = p_options["post_processor"];
	if (!post_processor_path.is_empty()) {
		Ref<Script> gd_script = ResourceLoader::load(post_processor_path);
		if (gd_script.is_valid()) {
			Ref<RefCounted> dyn_script_instance = memnew(RefCounted);
			dyn_script_instance->set_script(gd_script);
			if (dyn_script_instance->has_method("_post_process")) {
				Variant args[] = { node2d };
				const Variant *argp[] = { &args[0] };
				Callable::CallError ce;
				Variant ret_val = dyn_script_instance->callp(StringName("_post_process"), argp, 1, ce);
				if (ce.error == Callable::CallError::CALL_OK && ret_val.get_type() == Variant::OBJECT) {
					Node *new_node = Object::cast_to<Node>(ret_val.operator Object *());
					if (new_node) {
						node2d = new_node;
					}
				}
			}
		}
	}

	Ref<PackedScene> packed_scene;
	packed_scene.instantiate();
	packed_scene->pack(node2d);

	Error err = ResourceSaver::save(packed_scene, p_save_path + "." + get_save_extension());
	memdelete(node2d);
	if (ct.is_valid()) {
		ct->unload_custom_types();
	}
	return err;
}

ResourceImporterTiled::ResourceImporterTiled() {
}

// --------------------------------------------------------------------------------------------------

String ResourceImporterTiledTileset::get_importer_name() const {
	return "tiled_tileset_importer";
}

String ResourceImporterTiledTileset::get_visible_name() const {
	return "Tiled TileSet";
}

void ResourceImporterTiledTileset::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("tsx");
	p_extensions->push_back("tsj");
}

String ResourceImporterTiledTileset::get_save_extension() const {
	return "res";
}

String ResourceImporterTiledTileset::get_resource_type() const {
	return "TileSet";
}

int ResourceImporterTiledTileset::get_preset_count() const {
	return 1;
}

String ResourceImporterTiledTileset::get_preset_name(int p_idx) const {
	return "Default";
}

void ResourceImporterTiledTileset::get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const {
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "custom_properties"), true));
}

bool ResourceImporterTiledTileset::get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const {
	return true;
}

Error ResourceImporterTiledTileset::import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	PackedByteArray content = DataLoader::get_tiled_file_content(p_source_file, p_source_file.get_base_dir());
	if (content.is_empty()) {
		ERR_PRINT("ERROR: Tileset file '" + p_source_file + "' not found.");
		return ERR_FILE_NOT_FOUND;
	}

	Dictionary tile_set_dict = TiledDictionaryBuilder::get_dictionary(content, p_source_file);
	if (tile_set_dict.is_empty()) {
		return ERR_PARSE_ERROR;
	}

	Array tileset_arr;
	tileset_arr.push_back(tile_set_dict);

	TiledTilesetCreator creator;
	creator.set_base_path(p_source_file);

	Ref<TileSet> tileset = creator.create_from_dictionary_array(tileset_arr);
	if (tileset.is_null()) {
		return FAILED;
	}

	return ResourceSaver::save(tileset, p_save_path + "." + get_save_extension());
}

ResourceImporterTiledTileset::ResourceImporterTiledTileset() {
}
