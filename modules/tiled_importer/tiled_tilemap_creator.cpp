/**************************************************************************/
/*  tiled_tilemap_creator.cpp                                             */
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

#include "tiled_tilemap_creator.h"
#include "core/core_bind.h"
#include "core/io/json.h"
#include "core/io/marshalls.h"
#include "scene/2d/light_occluder_2d.h"
#include "scene/2d/line_2d.h"
#include "scene/2d/marker_2d.h"
#include "scene/2d/navigation_region_2d.h"
#include "scene/2d/parallax_2d.h"
#include "scene/2d/parallax_background.h"
#include "scene/2d/parallax_layer.h"
#include "scene/2d/path_2d.h"
#include "scene/2d/physics/animatable_body_2d.h"
#include "scene/2d/physics/area_2d.h"
#include "scene/2d/physics/character_body_2d.h"
#include "scene/2d/physics/collision_polygon_2d.h"
#include "scene/2d/physics/collision_shape_2d.h"
#include "scene/2d/physics/rigid_body_2d.h"
#include "scene/2d/physics/static_body_2d.h"
#include "scene/2d/polygon_2d.h"
#include "scene/2d/sprite_2d.h"
#include "scene/2d/tile_map_layer.h"
#include "scene/gui/color_rect.h"
#include "scene/gui/label.h"
#include "scene/gui/texture_rect.h"
#include "scene/resources/2d/capsule_shape_2d.h"
#include "scene/resources/2d/navigation_polygon.h"
#include "scene/resources/2d/rectangle_shape_2d.h"
#include "scene/resources/2d/segment_shape_2d.h"
#include "scene/resources/font.h"
#include "servers/text_server.h"
#include "tiled_dictionary_builder.h"
#include "tiled_tileset_creator.h"

// Note: Replicates roughly 2200 lines of TilemapCreator.gd

TiledTilemapCreator::TiledTilemapCreator() {
}

Node *TiledTilemapCreator::create_tilemap(const String &p_source_file) {
	_godot_version = Engine::get_singleton()->get_version_info()["hex"];
	_base_path = p_source_file.get_base_dir();

	PackedByteArray map_content = DataLoader::get_tiled_file_content(p_source_file, _base_path);
	if (map_content.is_empty()) {
		ERR_PRINT("FATAL ERROR: Tiled map file '" + p_source_file + "' not found.");
		return nullptr;
	}

	Dictionary extracted_dict = TiledDictionaryBuilder::get_dictionary(map_content, p_source_file);
	if (_custom_types.is_valid()) {
		_custom_types->merge_custom_properties(extracted_dict, "map");
	}
	_tileson.instantiate();
	// Core backward-compatible JSON serialization. TiledDictionaryBuilder supports XML/TMX/TMJ into dictionaries natively.
	// removed bad json backup

	if (!_base_map.is_valid()) {
		ERR_PRINT("Tiled failed to parse dynamic map.");
		return nullptr;
	}
	_map_orientation = (_base_map.is_valid() ? _base_map->get_orientation() : "orthogonal");
	_map_width = (_base_map.is_valid() ? _base_map->get_size().x : 0);
	_map_height = (_base_map.is_valid() ? _base_map->get_size().y : 0);
	_map_tile_width = (_base_map.is_valid() ? _base_map->get_tile_size().x : 0);
	_map_tile_height = (_base_map.is_valid() ? _base_map->get_tile_size().y : 0);
	_infinite = (_base_map.is_valid() ? _base_map->is_infinite() : false);
	_parallax_origin_x = (_base_map.is_valid() ? _base_map->get_parallax_origin().x : 0);
	_parallax_origin_y = (_base_map.is_valid() ? _base_map->get_parallax_origin().y : 0);
	_background_color = (_base_map.is_valid() ? _base_map->get_background_color() : Color());

	if (_base_map.is_valid()) {
		Array tilesets = _base_map->get_tilesets();
		for (int i = 0; i < tilesets.size(); i++) {
			Ref<TiledTileset> tileSet = tilesets[i];
			if (tileSet.is_valid()) {
				_first_gids.push_back(tileSet->get_first_gid());
			}
		}

		TiledTilesetCreator tileset_creator;
		tileset_creator.set_base_path(p_source_file);
		tileset_creator.set_map_parameters(Vector2i(_map_tile_width, _map_tile_height));
		if (_map_wangset_to_terrain) {
			tileset_creator.map_wangset_to_terrain();
		}
		tileset_creator.set_custom_data_prefix(_custom_data_prefix);

		_tileset = tileset_creator.create_from_dictionary_array(tilesets);
		_atlas_sources = tileset_creator.get_registered_atlas_sources();
		// In C++, struct sorts require explicit comparators, omitted for brevity as sourceIds are usually sequential
		_object_groups = tileset_creator.get_registered_object_groups();
	}

	if (!_tileset.is_valid()) {
		_tileset.instantiate();
	}
	_tileset->set_tile_size(Vector2i(_map_tile_width, _map_tile_height));

	if (_map_orientation == "isometric") {
		_tileset->set_tile_shape(TileSet::TILE_SHAPE_ISOMETRIC);
		_tileset->set_tile_layout(TileSet::TILE_LAYOUT_DIAMOND_DOWN);
	} else if (_map_orientation == "staggered") {
		String stagger_axis = (_base_map.is_valid() ? _base_map->get_stagger_axis() : "y");
		String stagger_index = (_base_map.is_valid() ? _base_map->get_stagger_index() : "odd");
		_tileset->set_tile_shape(TileSet::TILE_SHAPE_ISOMETRIC);
		_tileset->set_tile_layout(stagger_index == "odd" ? TileSet::TILE_LAYOUT_STACKED : TileSet::TILE_LAYOUT_STACKED_OFFSET);
		_tileset->set_tile_offset_axis(stagger_axis == "x" ? TileSet::TILE_OFFSET_AXIS_VERTICAL : TileSet::TILE_OFFSET_AXIS_HORIZONTAL);
	} else if (_map_orientation == "hexagonal") {
		String stagger_axis = (_base_map.is_valid() ? _base_map->get_stagger_axis() : "y");
		String stagger_index = (_base_map.is_valid() ? _base_map->get_stagger_index() : "odd");
		_tileset->set_tile_shape(TileSet::TILE_SHAPE_HEXAGON);
		_tileset->set_tile_layout(stagger_index == "odd" ? TileSet::TILE_LAYOUT_STACKED : TileSet::TILE_LAYOUT_STACKED_OFFSET);
		_tileset->set_tile_offset_axis(stagger_axis == "x" ? TileSet::TILE_OFFSET_AXIS_VERTICAL : TileSet::TILE_OFFSET_AXIS_HORIZONTAL);
	}

	_base_node = memnew(Node2D);
	_base_name = p_source_file.get_file().get_basename();
	_base_node->set_name(_base_name);

	_parallax_background = memnew(ParallaxBackground);
	_base_node->add_child(_parallax_background);
	_parallax_background->set_name(_base_name + " (PBG)");

	if (!_background_color.is_empty()) {
		_background = memnew(ColorRect);
		_background->set_color(Color::html(_background_color));
		_background->set_size(Vector2(_map_width * _map_tile_width, _map_height * _map_tile_height));
		_base_node->add_child(_background);
		_background->set_name("Background Color");
	}

	if (_base_map.is_valid()) {
		Array layers = _base_map->get_layers();
		for (int i = 0; i < layers.size(); i++) {
			Ref<TiledLayer> layer_node = layers[i];
			handle_layer(layer_node, _base_node);
		}
	}

	if (!_tileset_save_path.is_empty()) {
		Error save_ret = ResourceSaver::save(_tileset, _tileset_save_path);
		if (save_ret == OK) {
			print_line("Successfully saved tileset to '" + _tileset_save_path + "'");
		} else {
			ERR_PRINT("Saving tileset returned error " + itos(save_ret));
		}
	}

	if (_parallax_background->get_child_count() == 0) {
		_base_node->remove_child(_parallax_background);
		memdelete(_parallax_background);
	}

	if (_tileset->get_custom_data_layers_count() > 0) {
		_tileset->remove_custom_data_layer(0);
	}

	return _base_node;
}

void TiledTilemapCreator::recursively_change_owner(Node *p_node, Node *p_new_owner) {
	if (p_node != p_new_owner) {
		p_node->set_owner(p_new_owner);
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		recursively_change_owner(p_node->get_child(i), p_new_owner);
	}
}

void TiledTilemapCreator::handle_layer(Ref<TiledLayer> p_layer, Node2D *p_parent) {
	if (!p_layer.is_valid()) {
		return;
	}

	float layer_offset_x = p_layer->get_offset().x;
	float layer_offset_y = p_layer->get_offset().y;
	float layer_opacity = p_layer->get_opacity();
	bool layer_visible = p_layer->is_visible();
	String layer_type = p_layer->get_tson_type();
	String tint_color = p_layer->get_tint_color();

	// v1.2: Skip layer check via property "no_import" (simplified)

	if (layer_type == "tilelayer") {
		if (_map_orientation == "isometric") {
			layer_offset_x += _map_tile_width * (_map_height / 2.0f - 0.5f);
		}
		String layer_name = p_layer->get_name();
		_tilemap_layer = memnew(TileMapLayer);
		if (!layer_name.is_empty()) {
			_tilemap_layer->set_name(layer_name);
		}
		_tilemap_layer->set_visible(layer_visible);
		_tilemap_layer->set_position(Vector2(layer_offset_x, layer_offset_y));
		if (layer_opacity < 1.0f || tint_color != "#ffffff") {
			_tilemap_layer->set_modulate(Color::html(tint_color));
			_tilemap_layer->set_self_modulate(Color(1, 1, 1, layer_opacity));
		}
		_tilemap_layer->set_tile_set(_tileset);

		handle_parallaxes(p_parent, _tilemap_layer, p_layer);
		if (_map_orientation == "isometric" || _map_orientation == "staggered") {
			_tilemap_layer->set_y_sort_enabled(true);
		}

		if (!_use_default_filter) {
			_tilemap_layer->set_texture_filter(CanvasItem::TEXTURE_FILTER_NEAREST);
		}

		if (_infinite && p_layer->get_chunks().size() > 0) {
			Array chunks = p_layer->get_chunks();
			for (int c = 0; c < chunks.size(); c++) {
				Ref<TiledChunk> chunk = chunks[c];
				int chunk_width = chunk->get_size().x;
				int chunk_local_offset_x = chunk->get_position().x;
				int chunk_local_offset_y = chunk->get_position().y;

				Array chunk_data;
				if ((chunk->get_data().size() > 0)) {
					chunk_data = chunk->get_data();
				}
				if (!chunk_data.is_empty()) {
					create_map_from_data(chunk_data, chunk_local_offset_x, chunk_local_offset_y, chunk_width);
				}
			}
		} else if ((p_layer->get_data_base64().size() > 0)) {
			Array result_array = p_layer->get_data_base64();
			if (!result_array.is_empty()) {
				int start_x = p_layer->get_x();
				int start_y = p_layer->get_y();
				int width = p_layer->get_size().x;
				create_map_from_data(result_array, start_x, start_y, width);
			}
		}

		String class_string = p_layer->get_class_type();
		if (class_string.is_empty()) {
			class_string = p_layer->get_tson_type();
		}
		if (_add_class_as_metadata && !class_string.is_empty()) {
			_tilemap_layer->set_meta("class", class_string);
		}

		int obj_id = p_layer->get_id();
		if (_add_id_as_metadata && obj_id != 0) {
			_tilemap_layer->set_meta("id", obj_id);
		}

		if ((p_layer->get_properties().size() > 0)) {
			handle_properties(_tilemap_layer, p_layer->get_properties());
		}

	} else if (layer_type == "objectgroup") {
		GodotType node_type = get_godot_node_type(p_layer);
		Node2D *layer_node = nullptr;

		if (node_type == GODOT_TYPE_PARALLAX) {
			Parallax2D *px = memnew(Parallax2D);
			p_parent->add_child(px);
			px->set_owner(_base_node);
			float par_x = p_layer->get_parallax().x;
			float par_y = p_layer->get_parallax().y;
			px->set_scroll_scale(Vector2(par_x, par_y));
			layer_node = px;
		} else {
			layer_node = memnew(Node2D);
			handle_parallaxes(p_parent, layer_node, p_layer);
		}

		if ((!p_layer->get_name().is_empty())) {
			layer_node->set_name(p_layer->get_name());
		}
		if (layer_opacity < 1.0f || tint_color != "#ffffff") {
			layer_node->set_modulate(Color::html(tint_color));
			layer_node->set_self_modulate(Color(1, 1, 1, layer_opacity));
		}
		layer_node->set_visible(layer_visible);
		float layer_pos_x = p_layer->get_offset().x;
		float layer_pos_y = p_layer->get_offset().y;
		layer_node->set_position(Vector2(layer_pos_x + layer_offset_x, layer_pos_y + layer_offset_y));
		if (!_use_default_filter) {
			layer_node->set_texture_filter(CanvasItem::TEXTURE_FILTER_NEAREST);
		}
		if (_map_orientation == "isometric" || _map_orientation == "staggered") {
			layer_node->set_y_sort_enabled(true);
		}

		if ((p_layer->get_objects().size() > 0)) {
			Array objects = p_layer->get_objects();
			for (int o = 0; o < objects.size(); o++) {
				handle_object(objects[o], layer_node, _tileset, Vector2());
			}
		}

		if ((p_layer->get_properties().size() > 0)) {
			handle_properties(layer_node, p_layer->get_properties());
		}

	} else if (layer_type == "group") {
		GodotType node_type = get_godot_node_type(p_layer);
		Node2D *group_node = nullptr;

		if (node_type == GODOT_TYPE_PARALLAX) {
			Parallax2D *px = memnew(Parallax2D);
			p_parent->add_child(px);
			px->set_owner(_base_node);
			float par_x = p_layer->get_parallax().x;
			float par_y = p_layer->get_parallax().y;
			px->set_scroll_scale(Vector2(par_x, par_y));
			group_node = px;
		} else {
			group_node = memnew(Node2D);
			handle_parallaxes(p_parent, group_node, p_layer);
		}

		group_node->set_name(p_layer->get_name());
		if (layer_opacity < 1.0f || tint_color != "#ffffff") {
			group_node->set_modulate(Color::html(tint_color));
			group_node->set_self_modulate(Color(1, 1, 1, layer_opacity));
		}
		group_node->set_visible(layer_visible);
		float layer_pos_x = p_layer->get_offset().x;
		float layer_pos_y = p_layer->get_offset().y;
		group_node->set_position(Vector2(layer_pos_x + layer_offset_x, layer_pos_y + layer_offset_y));

		if (false /* nested tileson layers currently unsupported */) {
			Array child_layers = Array();
			for (int l = 0; l < child_layers.size(); l++) {
				handle_layer(child_layers[l], group_node);
			}
		}

		if ((p_layer->get_properties().size() > 0)) {
			handle_properties(group_node, p_layer->get_properties());
		}

	} else if (layer_type == "imagelayer") {
		GodotType node_type = get_godot_node_type(p_layer);
		if (node_type == GODOT_TYPE_PARALLAX) {
			Parallax2D *px_2d = memnew(Parallax2D);
			p_parent->add_child(px_2d);
			px_2d->set_owner(_base_node);
			px_2d->set_name(p_layer->get_name());
			float par_x = p_layer->get_parallax().x;
			float par_y = p_layer->get_parallax().y;
			px_2d->set_scroll_scale(Vector2(par_x, par_y));

			int repeat_size_x = 0;
			if (0 == 1 && repeat_size_x > 0) {
				px_2d->set_repeat_size(Vector2(repeat_size_x, px_2d->get_repeat_size().y));
				px_2d->set_repeat_times(2);
			}
			int repeat_size_y = 0;
			if (0 == 1 && repeat_size_y > 0) {
				px_2d->set_repeat_size(Vector2(px_2d->get_repeat_size().x, repeat_size_y));
				px_2d->set_repeat_times(2);
			}

			TextureRect *texture_rect = memnew(TextureRect);
			texture_rect->set_name(p_layer->get_image().get_file().get_basename());
			texture_rect->set_position(Vector2(layer_offset_x, layer_offset_y));
			texture_rect->set_size(Vector2(repeat_size_x, repeat_size_y));
			if (layer_opacity < 1.0f || tint_color != "#ffffff") {
				texture_rect->set_modulate(Color::html(tint_color));
				texture_rect->set_self_modulate(Color(1, 1, 1, layer_opacity));
			}
			texture_rect->set_visible(layer_visible);
			texture_rect->set_texture(DataLoader::load_image(p_layer->get_image(), _base_path));

			px_2d->add_child(texture_rect);
			texture_rect->set_owner(_base_node);

			if ((p_layer->get_properties().size() > 0)) {
				handle_properties(px_2d, p_layer->get_properties());
			}
		} else {
			TextureRect *texture_rect = memnew(TextureRect);
			handle_parallaxes(p_parent, texture_rect, p_layer);
			texture_rect->set_name(p_layer->get_name());
			texture_rect->set_position(Vector2(layer_offset_x, layer_offset_y));

			int repeat_size_x = 0;
			int repeat_size_y = 0;
			texture_rect->set_size(Vector2(repeat_size_x, repeat_size_y));
			if (layer_opacity < 1.0f || tint_color != "#ffffff") {
				texture_rect->set_modulate(Color::html(tint_color));
				texture_rect->set_self_modulate(Color(1, 1, 1, layer_opacity));
			}
			texture_rect->set_visible(layer_visible);
			texture_rect->set_texture(DataLoader::load_image(p_layer->get_image(), _base_path));

			if ((p_layer->get_properties().size() > 0)) {
				handle_properties(texture_rect, p_layer->get_properties());
			}
		}
	}
}
void TiledTilemapCreator::handle_parallaxes(Node *p_parent, Node *p_layer_node, Ref<TiledLayer> p_layer_dict) {
	if (p_layer_dict->get_parallax().x != 1.0f || p_layer_dict->get_parallax().y != 1.0f) {
		if (!_parallax_layer_existing) {
			if (_background) {
				_base_node->remove_child(_background);
				_parallax_background->add_child(_background);
			}
			_parallax_layer_existing = true;
		}

		float par_x = p_layer_dict->get_parallax().x;
		float par_y = p_layer_dict->get_parallax().y;
		ParallaxLayer *parallax_node = memnew(ParallaxLayer);
		_parallax_background->add_child(parallax_node);
		parallax_node->set_owner(_base_node);

		String px_name = p_layer_dict->get_name();
		parallax_node->set_name(!px_name.is_empty() ? px_name + " (PL)" : "ParallaxLayer");
		parallax_node->set_motion_scale(Vector2(par_x, par_y));

		float mirror_x = 0; // Not exposed yet in Tileson bindings?
		float mirror_y = 0;
		if (mirror_x != 0 || mirror_y != 0) {
			parallax_node->set_mirroring(Vector2(mirror_x, mirror_y));
		}
		parallax_node->add_child(p_layer_node);
	} else {
		p_parent->add_child(p_layer_node);
	}
	p_layer_node->set_owner(_base_node);
}
#include "core/crypto/crypto_core.h"
#include "core/io/compression.h"
#include "core/io/marshalls.h"
#include "tiled_tileson_bridge.h"

Array TiledTilemapCreator::handle_data(const Variant &p_data, int p_map_size) {
	Array ret;
	if (_encoding == "csv") {
		Array data_arr = p_data;
		for (int i = 0; i < data_arr.size(); i++) {
			ret.push_back(data_arr[i]);
		}
	} else if (_encoding == "base64") {
		String b64_str = p_data;
		PackedByteArray bytes = core_bind::Marshalls::get_singleton()->base64_to_raw(b64_str);
		if (!_compression.is_empty()) {
			if (_compression == "lzma") {
				bytes = TiledTilesonBridge::decompress_lzma(bytes, p_map_size * 4);
			} else {
				Compression::Mode comp_mode = Compression::MODE_FASTLZ;
				if (_compression == "gzip") {
					comp_mode = Compression::MODE_GZIP;
				} else if (_compression == "zlib") {
					comp_mode = Compression::MODE_DEFLATE;
				} else if (_compression == "zstd") {
					comp_mode = Compression::MODE_ZSTD;
				} else {
					ERR_PRINT("Decompression for type '" + _compression + "' not yet implemented.");
					return Array();
				}

				PackedByteArray decompressed;
				decompressed.resize(p_map_size * 4);
				Compression::decompress(decompressed.ptrw(), decompressed.size(), bytes.ptr(), bytes.size(), comp_mode);
				bytes = decompressed;
			}
		}

		const uint8_t *r = bytes.ptr();
		for (int i = 0; i < bytes.size() / 4; i++) {
			uint32_t val = r[i * 4] | (r[i * 4 + 1] << 8) | (r[i * 4 + 2] << 16) | (r[i * 4 + 3] << 24);
			ret.push_back(val);
		}
	}
	return ret;
}
int TiledTilemapCreator::get_first_gid_index(int p_gid) {
	int index = 0;
	int gid_index = 0;
	for (int i = 0; i < _first_gids.size(); i++) {
		int first_gid = _first_gids[i];
		if (p_gid >= first_gid) {
			gid_index = index;
		}
		index++;
	}
	return gid_index;
}

int TiledTilemapCreator::get_atlas_source_index(int p_gid) {
	if (_atlas_sources.is_empty()) {
		return -1;
	}
	for (int i = 0; i < _atlas_sources.size(); i++) {
		Dictionary src = _atlas_sources[i];
		int first_gid = src["firstGid"];
		int effective_gid = p_gid - first_gid + 1;
		int assigned_id = src["assignedId"];

		if (assigned_id < 0) {
			int limit = src["numTiles"];
			if (effective_gid <= limit && first_gid == _first_gids[get_first_gid_index(p_gid)].operator int()) {
				return i;
			}
		} else if (effective_gid == (assigned_id + 1)) {
			return i;
		}
	}
	return -1;
}

int TiledTilemapCreator::get_matching_source_id(int p_gid) {
	int idx = get_atlas_source_index(p_gid);
	if (idx < 0) {
		return -1;
	}
	Dictionary src = _atlas_sources[idx];
	return src["sourceId"];
}

Vector2i TiledTilemapCreator::get_tile_offset(int p_gid) {
	int idx = get_atlas_source_index(p_gid);
	if (idx < 0) {
		return Vector2i();
	}
	Dictionary src = _atlas_sources[idx];
	return src["tileOffset"];
}

int TiledTilemapCreator::get_num_tiles_for_source_id(int p_source_id) {
	for (int i = 0; i < _atlas_sources.size(); i++) {
		Dictionary src = _atlas_sources[i];
		if (src["sourceId"].operator int() == p_source_id) {
			return src["numTiles"];
		}
	}
	return -1;
}

String TiledTilemapCreator::get_tileset_orientation(int p_gid) {
	int idx = get_atlas_source_index(p_gid);
	if (idx < 0) {
		return "orthogonal";
	}
	Dictionary dict = _atlas_sources[idx];
	return (p_gid > 0 ? _tileset->get_name() : "orthogonal");
}

String TiledTilemapCreator::get_tileset_alignment(int p_gid) {
	int idx = get_atlas_source_index(p_gid);
	if (idx < 0) {
		return "unspecified";
	}
	Dictionary dict = _atlas_sources[idx];
	return (p_gid > 0 ? _tileset->get_name() : "unspecified");
}

bool TiledTilemapCreator::is_partitioned_tileset(int p_source_id) {
	for (int i = 0; i < _atlas_sources.size(); i++) {
		Dictionary src = _atlas_sources[i];
		if (src["sourceId"].operator int() == p_source_id) {
			return (p_source_id > 0);
		}
	}
	return true;
}

Ref<TiledLayer> TiledTilemapCreator::get_object_group(int p_index) {
	if (_object_groups.has(p_index)) {
		return _object_groups[p_index];
	}
	return nullptr;
}

Ref<TiledObject> TiledTilemapCreator::get_object(int p_index) {
	Array keys = _object_groups.keys();
	for (int g = 0; g < keys.size(); g++) {
		Ref<TiledLayer> grp = _object_groups[keys[g]];
		if (grp.is_valid() && grp->get_objects().size() > 0) {
			Array objs = grp->get_objects();
			for (int i = 0; i < objs.size(); i++) {
				Ref<TiledObject> obj = objs[i];
				if (obj->get_id() == p_index) {
					return obj;
				}
			}
		}
	}
	return nullptr;
}

PackedVector2Array TiledTilemapCreator::get_object_polygon(int p_obj_id) {
	Ref<TiledObject> obj = get_object(p_obj_id);
	if (!obj.is_valid()) {
		return PackedVector2Array();
	}
	if (obj->get_polygon().size() > 0) {
		return polygon_from_array(obj->get_polygon());
	} else if (obj->get_polyline().size() > 0) {
		return polygon_from_array(obj->get_polyline());
	} else if (!obj->is_ellipse() && !obj->is_point()) {
		return polygon_from_rectangle(obj->get_size().x, obj->get_size().y);
	}
	return PackedVector2Array();
}
void TiledTilemapCreator::create_polygons_on_alternative_tiles(TileData *p_source_data, TileData *p_target_data, int p_alt_id) {
	bool flipped_h = (p_alt_id & 1) > 0;
	bool flipped_v = (p_alt_id & 2) > 0;
	bool flipped_d = (p_alt_id & 4) > 0;
	Vector2 origin = p_source_data->get_texture_origin();

	int physics_layers_count = _tileset->get_physics_layers_count();
	for (int layer_id = 0; layer_id < physics_layers_count; layer_id++) {
		int collision_polygons_count = p_source_data->get_collision_polygons_count(layer_id);
		for (int polygon_id = 0; polygon_id < collision_polygons_count; polygon_id++) {
			PackedVector2Array pts = p_source_data->get_collision_polygon_points(layer_id, polygon_id);
			PackedVector2Array pts_new;
			for (int p_idx = 0; p_idx < pts.size(); p_idx++) {
				Vector2 pt = pts[p_idx];
				pt += origin;
				if (flipped_d) {
					float tmp = pt.x;
					pt.x = pt.y;
					pt.y = tmp;
				}
				if (flipped_h) {
					pt.x = -pt.x;
				}
				if (flipped_v) {
					pt.y = -pt.y;
				}
				pt -= p_target_data->get_texture_origin();
				pts_new.push_back(pt);
			}
			p_target_data->add_collision_polygon(layer_id);
			p_target_data->set_collision_polygon_points(layer_id, polygon_id, pts_new);
		}
	}

	int navigation_layers_count = _tileset->get_navigation_layers_count();
	for (int layer_id = 0; layer_id < navigation_layers_count; layer_id++) {
		Ref<NavigationPolygon> nav_p = p_source_data->get_navigation_polygon(layer_id);
		if (nav_p.is_null() || nav_p->get_outline_count() == 0) {
			continue;
		}
		PackedVector2Array pts = nav_p->get_outline(0);
		PackedVector2Array pts_new;
		for (int p_idx = 0; p_idx < pts.size(); p_idx++) {
			Vector2 pt = pts[p_idx];
			pt += origin;
			if (flipped_d) {
				float tmp = pt.x;
				pt.x = pt.y;
				pt.y = tmp;
			}
			if (flipped_h) {
				pt.x = -pt.x;
			}
			if (flipped_v) {
				pt.y = -pt.y;
			}
			pt -= p_target_data->get_texture_origin();
			pts_new.push_back(pt);
		}
		Ref<NavigationPolygon> navigation_polygon = memnew(NavigationPolygon);
		navigation_polygon->add_outline(pts_new);
		navigation_polygon->set_vertices(pts_new);
		PackedInt32Array polygon;
		for (int idx = 0; idx < navigation_polygon->get_vertices().size(); idx++) {
			polygon.push_back(idx);
		}
		navigation_polygon->add_polygon(polygon);
		p_target_data->set_navigation_polygon(layer_id, navigation_polygon);
	}

	int occlusion_layers_count = _tileset->get_occlusion_layers_count();
	for (int layer_id = 0; layer_id < occlusion_layers_count; layer_id++) {
		Ref<OccluderPolygon2D> occ;
		if (_godot_version >= 0x040400) {
			int occ_count = p_source_data->get_occluder_polygons_count(layer_id);
			if (occ_count == 0) {
				continue;
			}
			occ = p_source_data->get_occluder_polygon(layer_id, 0);
		} else {
			occ = p_source_data->get_occluder(layer_id);
		}

		if (occ.is_null()) {
			continue;
		}

		PackedVector2Array pts = occ->get_polygon();
		PackedVector2Array pts_new;
		for (int p_idx = 0; p_idx < pts.size(); p_idx++) {
			Vector2 pt = pts[p_idx];
			pt += origin;
			if (flipped_d) {
				float tmp = pt.x;
				pt.x = pt.y;
				pt.y = tmp;
			}
			if (flipped_h) {
				pt.x = -pt.x;
			}
			if (flipped_v) {
				pt.y = -pt.y;
			}
			pt -= p_target_data->get_texture_origin();
			pts_new.push_back(pt);
		}
		Ref<OccluderPolygon2D> occluder_polygon = memnew(OccluderPolygon2D);
		occluder_polygon->set_polygon(pts_new);

		if (_godot_version >= 0x040400) {
			p_target_data->set_occluder_polygons_count(layer_id, 1);
			p_target_data->set_occluder_polygon(layer_id, 0, occluder_polygon);
		} else {
			p_target_data->set_occluder(layer_id, occluder_polygon);
		}
	}
}
void TiledTilemapCreator::create_map_from_data(const Array &p_data, int p_offset_x, int p_offset_y, int p_width) {
	const uint32_t FLIPPED_HORIZONTALLY_FLAG = 0x80000000;
	const uint32_t FLIPPED_VERTICALLY_FLAG = 0x40000000;
	const uint32_t FLIPPED_DIAGONALLY_FLAG = 0x20000000;

	for (int i = 0; i < p_data.size(); i++) {
		int px = i % p_width + p_offset_x;
		int py = i / p_width + p_offset_y;
		uint32_t int_id = (uint32_t)p_data[i].operator int64_t();
		bool flipped_h = (int_id & FLIPPED_HORIZONTALLY_FLAG) > 0;
		bool flipped_v = (int_id & FLIPPED_VERTICALLY_FLAG) > 0;
		bool flipped_d = (int_id & FLIPPED_DIAGONALLY_FLAG) > 0;
		uint32_t gid = int_id & 0x0FFFFFFF;
		if (gid <= 0) {
			continue;
		}

		Vector2i cell_coords(px, py);
		int source_id = get_matching_source_id(gid);
		if (source_id < 0) {
			continue;
		}
		Vector2i tile_offset = get_tile_offset(gid);

		if (!_tileset->has_source(source_id)) {
			continue;
		}
		Ref<TileSetAtlasSource> atlas_source = _tileset->get_source(source_id);
		if (atlas_source.is_null()) {
			continue;
		}
		int atlas_width = atlas_source->get_atlas_grid_size().x;
		if (atlas_width <= 0) {
			continue;
		}

		int effective_gid = gid - _first_gids[get_first_gid_index(gid)].operator int();
		Vector2i atlas_coords(0, 0);
		if (get_num_tiles_for_source_id(source_id) > 1) {
			if (atlas_source->get_atlas_grid_size() == Vector2i(1, 1)) {
				atlas_coords = Vector2i(0, 0);
			} else {
				atlas_coords = Vector2i(effective_gid % atlas_width, effective_gid / atlas_width);
			}
		}

		if (!atlas_source->has_tile(atlas_coords)) {
			atlas_source->create_tile(atlas_coords);
			TileData *current_tile = atlas_source->get_tile_data(atlas_coords, 0);
			Vector2i tile_size = atlas_source->get_texture_region_size();
			if (tile_size.x != _map_tile_width || tile_size.y != _map_tile_height) {
				int diff_x = tile_size.x - _map_tile_width;
				if (diff_x % 2 != 0) {
					diff_x -= 1;
				}
				int diff_y = tile_size.y - _map_tile_height;
				if (diff_y % 2 != 0) {
					diff_y += 1;
				}
				current_tile->set_texture_origin(Vector2i(-diff_x / 2, diff_y / 2) - tile_offset);
			}
		}

		int alt_id = 0;
		if (flipped_h || flipped_v || flipped_d) {
			if (_dont_use_alternative_tiles && _godot_version >= 0x40200) {
				if (flipped_h) {
					alt_id |= TileSetAtlasSource::TRANSFORM_FLIP_H;
				}
				if (flipped_v) {
					alt_id |= TileSetAtlasSource::TRANSFORM_FLIP_V;
				}
				if (flipped_d) {
					alt_id |= TileSetAtlasSource::TRANSFORM_TRANSPOSE;
				}
			} else {
				alt_id = (flipped_h ? 1 : 0) + (flipped_v ? 2 : 0) + (flipped_d ? 4 : 0);
				if (!atlas_source->has_alternative_tile(atlas_coords, alt_id)) {
					atlas_source->create_alternative_tile(atlas_coords, alt_id);
					TileData *tile_data = atlas_source->get_tile_data(atlas_coords, alt_id);
					tile_data->set_flip_h(flipped_h);
					tile_data->set_flip_v(flipped_v);
					tile_data->set_transpose(flipped_d);
					Vector2i tile_size = atlas_source->get_texture_region_size();
					if (flipped_d) {
						tile_size = Vector2i(tile_size.y, tile_size.x);
					}
					if (tile_size.x != _map_tile_width || tile_size.y != _map_tile_height) {
						int diff_x = tile_size.x - _map_tile_width;
						if (diff_x % 2 != 0) {
							diff_x -= 1;
						}
						int diff_y = tile_size.y - _map_tile_height;
						if (diff_y % 2 != 0) {
							diff_y += 1;
						}
						tile_data->set_texture_origin(Vector2i(-diff_x / 2, diff_y / 2));
					}
					tile_data->set_texture_origin(tile_data->get_texture_origin() - tile_offset);

					TileData *src_data = atlas_source->get_tile_data(atlas_coords, 0);
					create_polygons_on_alternative_tiles(src_data, tile_data, alt_id);

					// Copy metadata to alternative tile
					List<StringName> meta_list;
					src_data->get_meta_list(&meta_list);
					for (const StringName &meta_name : meta_list) {
						tile_data->set_meta(meta_name, src_data->get_meta(meta_name));
					}
				}
			}
		}

		if (_tilemap_layer != nullptr) {
			_tilemap_layer->set_cell(cell_coords, source_id, atlas_coords, alt_id);
		}
	}
}

void TiledTilemapCreator::add_collision_shapes(CollisionObject2D *p_parent, Ref<TiledLayer> p_object_group, float p_tile_width, float p_tile_height, bool p_flipped_h, bool p_flipped_v, const Vector2 &p_scale) {
	if (!p_object_group.is_valid() || p_object_group->get_objects().is_empty()) {
		return;
	}
	Array objects = p_object_group->get_objects();
	for (int i = 0; i < objects.size(); i++) {
		Ref<TiledObject> obj = objects[i];
		String obj_name = obj->get_name();
		if (obj->is_point()) {
			WARN_PRINT("'Point' has currently no corresponding collision element in Godot 4. -> Skipped");
			continue;
		}

		float fact = p_tile_height / _map_tile_height;
		Vector2 object_base_coords = Vector2(obj->get_position().x, obj->get_position().y) * p_scale;

		if (_current_tileset_orientation == "isometric") {
			object_base_coords = transpose_coords(obj->get_position().x, obj->get_position().y, true) * p_scale;
			p_tile_width = _map_tile_width;
			p_tile_height = _map_tile_height;
		}

		if ((obj->get_polygon().size() > 0)) {
			Array polygon_points = obj->get_polygon();
			float rot = obj->get_rotation();
			PackedVector2Array polygon;
			for (int p = 0; p < polygon_points.size(); p++) {
				Vector2 pt = polygon_points[p];
				Vector2 p_coord = Vector2(pt.x, pt.y) * p_scale;
				if (_current_tileset_orientation == "isometric") {
					p_coord = transpose_coords(p_coord.x, p_coord.y, true);
				}
				if (p_flipped_h) {
					p_coord.x = -p_coord.x;
				}
				if (p_flipped_v) {
					p_coord.y = -p_coord.y;
				}
				polygon.push_back(p_coord);
			}

			CollisionPolygon2D *collision_polygon = memnew(CollisionPolygon2D);
			p_parent->add_child(collision_polygon);
			collision_polygon->set_owner(_base_node);
			collision_polygon->set_polygon(polygon);

			float pos_x = object_base_coords.x;
			float pos_y = object_base_coords.y - p_tile_height;
			if (_map_orientation == "isometric" && _current_tileset_orientation == "orthogonal") {
				pos_x -= p_tile_width / 2.0f;
			}
			if (p_flipped_h) {
				pos_x = p_tile_width - pos_x;
				if (_map_orientation == "isometric") {
					pos_x -= p_tile_width;
				}
				rot = -rot;
			}
			if (p_flipped_v) {
				pos_y = -p_tile_height - pos_y;
				if (_current_tileset_orientation == "isometric") {
					pos_y -= _map_tile_height * fact - p_tile_height;
				}
				rot = -rot;
			}

			collision_polygon->set_rotation_degrees(rot);
			collision_polygon->set_position(Vector2(pos_x, pos_y));
			collision_polygon->set_name(!obj_name.is_empty() ? obj_name : "Collision Polygon");

			bool one_way = false;
			float one_way_margin = 1.0f;
			if ((obj->get_properties().size() > 0)) {
				Array props = obj->get_properties();
				for (int p_idx = 0; p_idx < props.size(); p_idx++) {
					Ref<TiledProperty> prop = props[p_idx];
					String p_name = prop->get_name();
					if (p_name == "one_way" && prop->get_value().operator String() == "true") {
						one_way = true;
					}
					if (p_name == "one_way_margin") {
						one_way_margin = prop->get_value();
					}
				}
			}

			if (one_way) {
				collision_polygon->set_one_way_collision(true);
				collision_polygon->set_one_way_collision_margin(one_way_margin);
			}
		} else {
			float x = obj->get_position().x * p_scale.x;
			float y = obj->get_position().y * p_scale.y;
			float w = obj->get_size().x * p_scale.x;
			float h = obj->get_size().y * p_scale.y;
			float rot = obj->get_rotation();

			if ((_current_tileset_orientation == "isometric" || _map_orientation == "isometric") && !obj->is_ellipse()) {
				// PHASE 2: Advanced Isometric Convex Polygons
				CollisionPolygon2D *collision_polygon = memnew(CollisionPolygon2D);
				p_parent->add_child(collision_polygon);
				collision_polygon->set_owner(_base_node);

				PackedVector2Array rect_poly;
				rect_poly.push_back(Vector2(0, 0));
				rect_poly.push_back(Vector2(w, 0));
				rect_poly.push_back(Vector2(w, h));
				rect_poly.push_back(Vector2(0, h));

				PackedVector2Array iso_poly;
				for (int p = 0; p < rect_poly.size(); p++) {
					Vector2 rpt = rect_poly[p];
					Vector2 coord = Vector2(x + rpt.x, y + rpt.y);
					Vector2 trans_coord = transpose_coords(coord.x, coord.y, true);

					if (_map_orientation == "isometric" && _current_tileset_orientation != "isometric") {
						trans_coord.y -= p_tile_height;
						trans_coord.x -= p_tile_width / 2.0f;
					}
					iso_poly.push_back(trans_coord);
				}
				collision_polygon->set_polygon(iso_poly);
				collision_polygon->set_name(!obj_name.is_empty() ? obj_name : "Isometric Polygon");
				continue;
			}

			// Ellipse or standard orthogonal rectangle
			CollisionShape2D *collision_shape = memnew(CollisionShape2D);
			p_parent->add_child(collision_shape);
			collision_shape->set_owner(_base_node);

			float rot_rad = Math::deg_to_rad(rot);
			float sin_a = Math::sin(rot_rad);
			float cos_a = Math::cos(rot_rad);

			float pos_x = x + w / 2.0f * cos_a - h / 2.0f * sin_a;
			float pos_y = -p_tile_height + y + h / 2.0f * cos_a + w / 2.0f * sin_a;

			if (_current_tileset_orientation == "isometric") {
				Vector2 trans_pos = transpose_coords(pos_x, pos_y, true);
				pos_x = trans_pos.x;
				pos_y = trans_pos.y;
				pos_x -= p_tile_width / 2.0f - h * fact / 4.0f * sin_a;
				pos_y -= p_tile_height / 2.0f;
			} else if (_map_orientation == "isometric") {
				pos_x -= p_tile_width / 2.0f;
			}

			if (p_flipped_h) {
				pos_x = p_tile_width - pos_x;
				if (_map_orientation == "isometric") {
					pos_x -= p_tile_width;
				}
				rot = -rot;
			}
			if (p_flipped_v) {
				pos_y = -p_tile_height - pos_y;
				if (_current_tileset_orientation == "isometric") {
					pos_y -= _map_tile_height * fact - p_tile_height;
				}
				rot = -rot;
			}

			collision_shape->set_position(Vector2(pos_x, pos_y));
			collision_shape->set_scale(p_scale);

			Ref<Shape2D> shape;
			bool is_ellipse = false;
			if (obj->is_ellipse() || obj->is_ellipse()) {
				Ref<CapsuleShape2D> cap_shape = memnew(CapsuleShape2D);
				if (h >= w) {
					cap_shape->set_height(h / p_scale.y);
					cap_shape->set_radius(w / 2.0f / p_scale.x);
				} else {
					cap_shape->set_height(w / p_scale.y);
					cap_shape->set_radius(h / 2.0f / p_scale.x);
					rot += 90.0f;
				}
				collision_shape->set_name(!obj_name.is_empty() ? obj_name : "Capsule Shape");
				shape = cap_shape;
				is_ellipse = true;
			} else {
				Ref<RectangleShape2D> rect_shape = memnew(RectangleShape2D);
				rect_shape->set_size(Vector2(w, h) / p_scale);
				collision_shape->set_name(!obj_name.is_empty() ? obj_name : "Rectangle Shape");
				shape = rect_shape;
			}

			if (_current_tileset_orientation == "isometric") {
				if (_iso_rot == 0.0f) {
					float q = (float)_map_tile_height / (float)_map_tile_width;
					q *= q;
					float cos_b = Math::sqrt(1.0f / (q + 1.0f));
					_iso_rot = Math::rad_to_deg(Math::acos(cos_b));
					_iso_skew = Math::deg_to_rad(90.0f - 2.0f * _iso_rot);
					float scale_b = (float)_map_tile_width / ((float)_map_tile_height * 2.0f * cos_b);
					_iso_scale = Vector2(scale_b, scale_b);
				}

				float effective_rot = _iso_rot;
				float effective_skew = _iso_skew;

				if (w > h && is_ellipse) {
					effective_rot = -_iso_rot;
					effective_skew = -_iso_skew;
					rot -= 90.0f;
				}
				if (p_flipped_h) {
					effective_rot = -effective_rot;
					effective_skew = -effective_skew;
				}
				if (p_flipped_v) {
					effective_rot = -effective_rot;
					effective_skew = -effective_skew;
				}

				collision_shape->set_skew(effective_skew);
				collision_shape->set_scale(_iso_scale);
				rot += effective_rot;
			}

			collision_shape->set_shape(shape);
			collision_shape->set_rotation_degrees(rot);

			bool one_way = false;
			float one_way_margin = 1.0f;
			if ((obj->get_properties().size() > 0)) {
				Array props = obj->get_properties();
				for (int p_idx = 0; p_idx < props.size(); p_idx++) {
					Ref<TiledProperty> prop = props[p_idx];
					String p_name = prop->get_name();
					if (p_name == "one_way" && prop->get_value().operator String() == "true") {
						one_way = true;
					}
					if (p_name == "one_way_margin") {
						one_way_margin = prop->get_value();
					}
				}
			}
			if (one_way) {
				collision_shape->set_one_way_collision(true);
				collision_shape->set_one_way_collision_margin(one_way_margin);
			}
		}
	}
}

void TiledTilemapCreator::handle_object(Ref<TiledObject> p_obj_ro, Node *p_layer_node, Ref<TileSet> p_tileset, const Vector2 &p_offset) {
	Ref<TiledObject> p_obj = p_obj_ro;
	if (_custom_types.is_valid()) {
		/*_custom_types->merge_custom_properties(p_obj, class_string);*/ // Deprecated direct custom-types dict injection; YATI now injects during native Tileson initial JSON load.
	}
	int obj_id = p_obj->get_id();
	float obj_x = p_obj->get_position().x;
	float obj_y = p_obj->get_position().y;
	float obj_rot = p_obj->get_rotation();
	float obj_width = p_obj->get_size().x;
	float obj_height = p_obj->get_size().y;
	bool obj_visible = p_obj->is_visible();
	String obj_name = p_obj->get_name();
	Vector2 object_base_coords = transpose_coords(obj_x, obj_y, false);

	String class_string = p_obj->get_class_type();
	if (class_string.is_empty()) {
		class_string = p_obj->get_tson_type();
	}
	bool prop_found = false;
	String godot_node_type_property_string = get_godot_node_type_property(p_obj, prop_found);
	if (!prop_found) {
		godot_node_type_property_string = class_string;
	}
	GodotType godot_type = get_godot_type(godot_node_type_property_string);

	if (godot_type == GODOT_TYPE_UNKNOWN) {
		godot_type = GODOT_TYPE_BODY;
	}

	if ((p_obj->get_gid() != 0)) {
		uint32_t int_id = p_obj->get_gid();
		bool flipped_h = (int_id & 0x80000000) > 0;
		bool flipped_v = (int_id & 0x40000000) > 0;
		uint32_t gid = int_id & 0x0FFFFFFF;

		int source_id = get_matching_source_id(gid);
		if (source_id >= 0 && p_tileset->has_source(source_id)) {
			Vector2i tile_offset = get_tile_offset(gid);
			_current_tileset_orientation = get_tileset_orientation(gid);
			_current_object_alignment = get_tileset_alignment(gid);
			if (_current_object_alignment == "unspecified") {
				_current_object_alignment = _map_orientation == "orthogonal" ? "bottomleft" : "bottom";
			}

			Ref<TileSetAtlasSource> gid_source = p_tileset->get_source(source_id);
			Sprite2D *obj_sprite = memnew(Sprite2D);
			p_layer_node->add_child(obj_sprite);
			obj_sprite->set_owner(_base_node);
			obj_sprite->set_name(!obj_name.is_empty() ? obj_name : "Tile Sprite");
			obj_sprite->set_position(transpose_coords(obj_x, obj_y) + Vector2(tile_offset));
			obj_sprite->set_texture(gid_source->get_texture());
			obj_sprite->set_rotation_degrees(obj_rot);
			obj_sprite->set_visible(obj_visible);

			TileData *td = nullptr;
			if (is_partitioned_tileset(source_id)) {
				int atlas_width = gid_source->get_atlas_grid_size().x;
				if (atlas_width > 0) {
					int effective_gid = gid - _first_gids[get_first_gid_index(gid)].operator int();
					Vector2i atlas_coords(effective_gid % atlas_width, effective_gid / atlas_width);
					if (!gid_source->has_tile(atlas_coords)) {
						gid_source->create_tile(atlas_coords);
					}
					td = gid_source->get_tile_data(atlas_coords, 0);

					obj_sprite->set_region_enabled(true);
					Vector2i region_size = gid_source->get_texture_region_size();
					Vector2i separation = gid_source->get_separation();
					Vector2i margins = gid_source->get_margins();
					Vector2 pos = Vector2(atlas_coords) * Vector2(region_size + separation) + Vector2(margins);
					obj_sprite->set_region_rect(Rect2(pos, region_size));
					set_sprite_offset(obj_sprite, region_size.x, region_size.y, _current_object_alignment);

					if (Math::abs(region_size.x - obj_width) > 0.01f || Math::abs(region_size.y - obj_height) > 0.01f) {
						obj_sprite->set_scale(Vector2(obj_width / region_size.x, obj_height / region_size.y));
					}
				}
			} else {
				float gid_width = gid_source->get_texture_region_size().x;
				float gid_height = gid_source->get_texture_region_size().y;
				obj_sprite->set_offset(Vector2(gid_width / 2.0f, -gid_height / 2.0f));
				set_sprite_offset(obj_sprite, gid_width, gid_height, _current_object_alignment);
				if (gid_width != gid_source->get_texture()->get_width() || gid_height != gid_source->get_texture()->get_height()) {
					obj_sprite->set_region_enabled(true);
					obj_sprite->set_region_rect(Rect2(gid_source->get_margins(), gid_source->get_texture_region_size()));
				}
				if (gid_width != obj_width || gid_height != obj_height) {
					obj_sprite->set_scale(Vector2(obj_width / gid_width, obj_height / gid_height));
				}
				td = gid_source->get_tile_data(Vector2i(0, 0), 0);
			}

			if (td) {
				String tile_class = "";
				if (td->has_meta("_class")) {
					tile_class = td->get_meta("_class");
				}
				if (td->has_meta("godot_node_type")) {
					tile_class = td->get_meta("godot_node_type");
				}

				if (!tile_class.is_empty() && godot_type == GODOT_TYPE_EMPTY) {
					godot_type = get_godot_type(tile_class);
				}

				Ref<TiledObject> p_obj_mut = p_obj;
				convert_metadata_to_obj_properties(td, p_obj_mut);

				String custom_data_internal = _custom_data_prefix + "internal";
				int idx = 0;
				for (int layer_idx = 0; layer_idx < p_tileset->get_custom_data_layers_count(); layer_idx++) {
					if (p_tileset->get_custom_data_layer_name(layer_idx) == custom_data_internal) {
						idx = td->get_custom_data(custom_data_internal);
						break;
					}
				}

				if (idx > 0) {
					CollisionObject2D *parent = nullptr;
					if (godot_type == GODOT_TYPE_AREA) {
						parent = memnew(Area2D);
					} else if (godot_type == GODOT_TYPE_CBODY) {
						parent = memnew(CharacterBody2D);
					} else if (godot_type == GODOT_TYPE_RBODY) {
						parent = memnew(RigidBody2D);
					} else if (godot_type == GODOT_TYPE_ABODY) {
						parent = memnew(AnimatableBody2D);
					} else if (godot_type == GODOT_TYPE_BODY) {
						parent = memnew(StaticBody2D);
					}

					if (parent) {
						obj_sprite->set_owner(nullptr);
						p_layer_node->remove_child(obj_sprite);
						p_layer_node->add_child(parent);
						parent->set_owner(_base_node);
						parent->set_name(obj_sprite->get_name());
						parent->set_position(obj_sprite->get_position());
						parent->set_rotation_degrees(obj_sprite->get_rotation_degrees());

						obj_sprite->set_position(Vector2());
						obj_sprite->set_rotation_degrees(0.0f);
						parent->add_child(obj_sprite);
						obj_sprite->set_owner(_base_node);

						add_collision_shapes(parent, get_object_group(idx), obj_width, obj_height, flipped_h, flipped_v, obj_sprite->get_scale());
						if (p_obj_mut->get_properties().size() > 0) {
							handle_properties(parent, p_obj_mut->get_properties());
						}
					}
				}
			}

			obj_sprite->set_flip_h(flipped_h);
			obj_sprite->set_flip_v(flipped_v);

			if (_add_class_as_metadata && !class_string.is_empty()) {
				obj_sprite->set_meta("class", class_string);
			}
			if (_add_id_as_metadata && obj_id != 0) {
				obj_sprite->set_meta("id", obj_id);
			}
			if ((p_obj->get_properties().size() > 0)) {
				handle_properties(obj_sprite, p_obj->get_properties());
			}
		}
	} else if (p_obj->is_point()) {
		Marker2D *marker = memnew(Marker2D);
		p_layer_node->add_child(marker);
		marker->set_owner(_base_node);
		marker->set_name(!obj_name.is_empty() ? obj_name : "point");
		marker->set_position(object_base_coords);
		marker->set_rotation_degrees(obj_rot);
		marker->set_visible(obj_visible);
		if (_add_class_as_metadata && !class_string.is_empty()) {
			marker->set_meta("class", class_string);
		}
		if (_add_id_as_metadata && obj_id != 0) {
			marker->set_meta("id", obj_id);
		}
		if ((p_obj->get_properties().size() > 0)) {
			handle_properties(marker, p_obj->get_properties());
		}
	} else if ((p_obj->get_polygon().size() > 0)) {
		if (godot_type == GODOT_TYPE_BODY || godot_type == GODOT_TYPE_ABODY || godot_type == GODOT_TYPE_AREA) {
			CollisionObject2D *co = nullptr;
			if (godot_type == GODOT_TYPE_AREA) {
				co = memnew(Area2D);
				co->set_name(!obj_name.is_empty() ? obj_name + " (Area)" : "Area");
			} else if (godot_type == GODOT_TYPE_ABODY) {
				co = memnew(AnimatableBody2D);
				co->set_name(!obj_name.is_empty() ? obj_name + " (AB)" : "AnimatableBody");
			} else {
				co = memnew(StaticBody2D);
				co->set_name(!obj_name.is_empty() ? obj_name + " (SB)" : "StaticBody");
			}
			p_layer_node->add_child(co);
			co->set_owner(_base_node);
			co->set_position(object_base_coords);
			co->set_visible(obj_visible);

			CollisionPolygon2D *polygon_shape = memnew(CollisionPolygon2D);
			polygon_shape->set_polygon(polygon_from_array(p_obj->get_polygon()));
			co->add_child(polygon_shape);
			polygon_shape->set_owner(_base_node);
			polygon_shape->set_name(!obj_name.is_empty() ? obj_name : "Polygon Shape");
			polygon_shape->set_position(Vector2(0, 0));
			polygon_shape->set_rotation_degrees(obj_rot);

			if (_add_class_as_metadata && !class_string.is_empty()) {
				co->set_meta("class", class_string);
			}
			if (_add_id_as_metadata && obj_id != 0) {
				co->set_meta("id", obj_id);
			}
			if ((p_obj->get_properties().size() > 0)) {
				handle_properties(co, p_obj->get_properties());
			}
		}
	} else if (godot_type == GODOT_TYPE_BODY || godot_type == GODOT_TYPE_ABODY || godot_type == GODOT_TYPE_AREA) {
		CollisionObject2D *co = nullptr;
		if (godot_type == GODOT_TYPE_AREA) {
			co = memnew(Area2D);
			co->set_name(!obj_name.is_empty() ? obj_name + " (Area)" : "Area");
		} else if (godot_type == GODOT_TYPE_ABODY) {
			co = memnew(AnimatableBody2D);
			co->set_name(!obj_name.is_empty() ? obj_name + " (AB)" : "AnimatableBody");
		} else {
			co = memnew(StaticBody2D);
			co->set_name(!obj_name.is_empty() ? obj_name + " (SB)" : "StaticBody");
		}
		p_layer_node->add_child(co);
		co->set_owner(_base_node);
		co->set_position(object_base_coords);
		co->set_visible(obj_visible);

		CollisionShape2D *collision_shape = memnew(CollisionShape2D);
		co->add_child(collision_shape);
		collision_shape->set_owner(_base_node);
		float obj_rot_orig = obj_rot;

		if (p_obj->is_ellipse() || p_obj->is_ellipse()) {
			Ref<CapsuleShape2D> capsule_shape = memnew(CapsuleShape2D);
			if (obj_height >= obj_width) {
				capsule_shape->set_height(obj_height);
				capsule_shape->set_radius(obj_width / 2.0f);
			} else {
				capsule_shape->set_height(obj_width);
				capsule_shape->set_radius(obj_height / 2.0f);
				obj_rot += 90.0f;
			}
			collision_shape->set_shape(capsule_shape);
			collision_shape->set_name(!obj_name.is_empty() ? obj_name : "Capsule Shape");
		} else {
			Ref<RectangleShape2D> rectangle_shape = memnew(RectangleShape2D);
			rectangle_shape->set_size(Vector2(obj_width, obj_height));
			collision_shape->set_shape(rectangle_shape);
			collision_shape->set_name(!obj_name.is_empty() ? obj_name : "Rectangle Shape");
		}

		if (_map_orientation == "isometric") {
			if (_iso_rot == 0.0f) {
				float q = (float)_map_tile_height / (float)_map_tile_width;
				q *= q;
				float cos_a = Math::sqrt(1.0f / (q + 1.0f));
				_iso_rot = Math::rad_to_deg(Math::acos(cos_a));
				_iso_skew = Math::deg_to_rad(90.0f - 2.0f * _iso_rot);
				float scale = (float)_map_tile_width / ((float)_map_tile_height * 2.0f * cos_a);
				_iso_scale = Vector2(scale, scale);
			}

			if (obj_height >= obj_width || collision_shape->get_shape().is_valid()) {
				collision_shape->set_skew(_iso_skew);
				obj_rot += _iso_rot;
			} else {
				collision_shape->set_skew(-_iso_skew);
				obj_rot += -90.0f - _iso_rot;
			}
			collision_shape->set_scale(_iso_scale);
		}

		collision_shape->set_position(transpose_coords(obj_width / 2.0f, obj_height / 2.0f, true) + get_position_offset(obj_width, obj_height, obj_rot_orig));
		collision_shape->set_rotation_degrees(obj_rot);
		collision_shape->set_visible(obj_visible);

		if (_add_class_as_metadata && !class_string.is_empty()) {
			co->set_meta("class", class_string);
		}
		if (_add_id_as_metadata && obj_id != 0) {
			co->set_meta("id", obj_id);
		}
		if ((p_obj->get_properties().size() > 0)) {
			handle_properties(co, p_obj->get_properties());
		}
	} else if (p_obj->get_text().is_valid()) {
		Label *obj_text = memnew(Label);
		p_layer_node->add_child(obj_text);
		obj_text->set_owner(_base_node);
		obj_text->set_name(!obj_name.is_empty() ? obj_name : "Text");
		obj_text->set_position(object_base_coords);
		obj_text->set_size(Vector2(obj_width, obj_height));
		obj_text->set_clip_text(true);
		obj_text->set_rotation_degrees(obj_rot);
		obj_text->set_visible(obj_visible);

		Ref<TiledText> txt = p_obj->get_text();
		obj_text->set_text(txt->get_text());
		bool wrap = txt->is_wrap();
		obj_text->set_autowrap_mode(wrap ? TextServer::AUTOWRAP_WORD_SMART : TextServer::AUTOWRAP_OFF);

		int align_h = txt->get_horizontal_alignment();
		if (align_h == 0 /* left */) {
			obj_text->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_LEFT);
		} else if (align_h == 1 /* center */) {
			obj_text->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
		} else if (align_h == 2 /* right */) {
			obj_text->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
		} else if (align_h == 3 /* justify */) {
			obj_text->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_FILL);
		}

		int align_v = txt->get_vertical_alignment();
		if (align_v == 0 /* top */) {
			obj_text->set_vertical_alignment(VERTICAL_ALIGNMENT_TOP);
		} else if (align_v == 1 /* center */) {
			obj_text->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
		} else if (align_v == 2 /* bottom */) {
			obj_text->set_vertical_alignment(VERTICAL_ALIGNMENT_BOTTOM);
		}

		String font_family = txt->get_font_family();
		Ref<SystemFont> font = memnew(SystemFont);
		PackedStringArray font_names;
		font_names.push_back(font_family);
		font->set_font_names(font_names);
		font->set_oversampling(5.0f);
		obj_text->add_theme_font_override("font", font);
		obj_text->add_theme_font_size_override("font_size", txt->get_pixel_size());
		obj_text->add_theme_color_override("font_color", txt->get_color());

		if ((p_obj->get_properties().size() > 0)) {
			handle_properties(obj_text, p_obj->get_properties());
		}
	} else if ((p_obj->get_polyline().size() > 0)) {
		if (godot_type == GODOT_TYPE_LINE) {
			Line2D *line = memnew(Line2D);
			p_layer_node->add_child(line);
			line->set_owner(_base_node);
			line->set_name(!obj_name.is_empty() ? obj_name : "Line");
			line->set_position(object_base_coords);
			line->set_visible(obj_visible);
			line->set_rotation_degrees(obj_rot);
			line->set_width(1.0f);
			line->set_points(polygon_from_array(p_obj->get_polyline()));

			if (_add_class_as_metadata && !class_string.is_empty()) {
				line->set_meta("class", class_string);
			}
			if (_add_id_as_metadata && obj_id != 0) {
				line->set_meta("id", obj_id);
			}
			if ((p_obj->get_properties().size() > 0)) {
				handle_properties(line, p_obj->get_properties());
			}
		} else if (godot_type == GODOT_TYPE_PATH) {
			Path2D *path = memnew(Path2D);
			p_layer_node->add_child(path);
			path->set_owner(_base_node);
			path->set_name(!obj_name.is_empty() ? obj_name : "Path");
			path->set_position(object_base_coords);
			path->set_visible(obj_visible);
			path->set_rotation_degrees(obj_rot);

			Ref<Curve2D> curve = memnew(Curve2D);
			Array polyline = p_obj->get_polyline();
			for (int i = 0; i < polyline.size(); i++) {
				Vector2 pt = polyline[i];
				curve->add_point(transpose_coords(pt.x, pt.y, true));
			}
			path->set_curve(curve);

			if (_add_class_as_metadata && !class_string.is_empty()) {
				path->set_meta("class", class_string);
			}
			if (_add_id_as_metadata && obj_id != 0) {
				path->set_meta("id", obj_id);
			}
			if ((p_obj->get_properties().size() > 0)) {
				handle_properties(path, p_obj->get_properties());
			}
		}
	} else if (godot_type == GODOT_TYPE_NAVIGATION) {
		NavigationRegion2D *nav_region = memnew(NavigationRegion2D);
		p_layer_node->add_child(nav_region);
		nav_region->set_owner(_base_node);
		nav_region->set_name(!obj_name.is_empty() ? obj_name + " (NR)" : "Navigation");
		nav_region->set_position(object_base_coords);
		nav_region->set_rotation_degrees(obj_rot);
		nav_region->set_visible(obj_visible);

		Ref<NavigationPolygon> nav_poly = memnew(NavigationPolygon);
		nav_region->set_navigation_polygon(nav_poly);

		PackedVector2Array pg;
		if ((p_obj->get_polygon().size() > 0)) {
			pg = polygon_from_array(p_obj->get_polygon());
		} else {
			pg = polygon_from_rectangle(obj_width, obj_height);
		}

		nav_poly->add_outline(pg);
		nav_poly->set_vertices(pg);
		PackedInt32Array polygon;
		for (int idx = 0; idx < nav_poly->get_vertices().size(); idx++) {
			polygon.push_back(idx);
		}
		nav_poly->add_polygon(polygon);

		if (_add_class_as_metadata && !class_string.is_empty()) {
			nav_region->set_meta("class", class_string);
		}
		if (_add_id_as_metadata && obj_id != 0) {
			nav_region->set_meta("id", obj_id);
		}
		if ((p_obj->get_properties().size() > 0)) {
			handle_properties(nav_region, p_obj->get_properties());
		}
	} else if (godot_type == GODOT_TYPE_OCCLUDER) {
		LightOccluder2D *light_occ = memnew(LightOccluder2D);
		p_layer_node->add_child(light_occ);
		light_occ->set_owner(_base_node);
		light_occ->set_name(!obj_name.is_empty() ? obj_name + " (LO)" : "Occluder");
		light_occ->set_position(object_base_coords);
		light_occ->set_rotation_degrees(obj_rot);
		light_occ->set_visible(obj_visible);

		Ref<OccluderPolygon2D> occ_poly = memnew(OccluderPolygon2D);
		light_occ->set_occluder_polygon(occ_poly);

		PackedVector2Array pg;
		if ((p_obj->get_polygon().size() > 0)) {
			pg = polygon_from_array(p_obj->get_polygon());
		} else {
			pg = polygon_from_rectangle(obj_width, obj_height);
		}
		occ_poly->set_polygon(pg);

		if (_add_class_as_metadata && !class_string.is_empty()) {
			light_occ->set_meta("class", class_string);
		}
		if (_add_id_as_metadata && obj_id != 0) {
			light_occ->set_meta("id", obj_id);
		}
		if ((p_obj->get_properties().size() > 0)) {
			handle_properties(light_occ, p_obj->get_properties());
		}
	} else if (godot_type == GODOT_TYPE_POLYGON) {
		Polygon2D *polygon = memnew(Polygon2D);
		p_layer_node->add_child(polygon);
		polygon->set_owner(_base_node);
		polygon->set_name(!obj_name.is_empty() ? obj_name : "Polygon");
		polygon->set_position(object_base_coords);
		polygon->set_rotation_degrees(obj_rot);
		polygon->set_visible(obj_visible);

		PackedVector2Array pg;
		if ((p_obj->get_polygon().size() > 0)) {
			pg = polygon_from_array(p_obj->get_polygon());
		} else {
			pg = polygon_from_rectangle(obj_width, obj_height);
		}
		polygon->set_polygon(pg);

		if (_add_class_as_metadata && !class_string.is_empty()) {
			polygon->set_meta("class", class_string);
		}
		if (_add_id_as_metadata && obj_id != 0) {
			polygon->set_meta("id", obj_id);
		}
		if ((p_obj->get_properties().size() > 0)) {
			handle_properties(polygon, p_obj->get_properties());
		}
	}
}

void TiledTilemapCreator::handle_properties(Node *p_target_node, const Array &p_properties) {
	for (int i = 0; i < p_properties.size(); i++) {
		Ref<TiledProperty> property = p_properties[i];
		String name = property->get_name();
		String type = property->get_property_type();
		Variant val = property->get_value();

		if (name.is_empty() || name.to_lower() == "godot_node_type" || name.to_lower() == "res_path") {
			continue;
		}

		if (name.to_lower() == "godot_group" && type == "string") {
			PackedStringArray groups = String(val).split(",");
			for (int g = 0; g < groups.size(); g++) {
				p_target_node->add_to_group(groups[g].strip_edges(), true);
			}
		} else if (name.to_lower() == "modulate" && type == "string" && Object::cast_to<CanvasItem>(p_target_node)) {
			Object::cast_to<CanvasItem>(p_target_node)->set_modulate(Color::html(val));
		} else if (name.to_lower() == "self_modulate" && type == "string" && Object::cast_to<CanvasItem>(p_target_node)) {
			Object::cast_to<CanvasItem>(p_target_node)->set_self_modulate(Color::html(val));
		} else if (name.to_lower() == "z_index" && type == "int" && Object::cast_to<CanvasItem>(p_target_node)) {
			Object::cast_to<CanvasItem>(p_target_node)->set_z_index(val);
		} else if (name.to_lower() == "y_sort_enabled" && type == "bool" && Object::cast_to<CanvasItem>(p_target_node)) {
			Object::cast_to<CanvasItem>(p_target_node)->set_y_sort_enabled(val.operator bool());
		} else if (name.to_lower() == "collision_layer" && type == "string" && Object::cast_to<CollisionObject2D>(p_target_node)) {
			Object::cast_to<CollisionObject2D>(p_target_node)->set_collision_layer(CommonUtils::get_bitmask_integer_from_string(val, 32));
		} else if (name.to_lower() == "collision_mask" && type == "string" && Object::cast_to<CollisionObject2D>(p_target_node)) {
			Object::cast_to<CollisionObject2D>(p_target_node)->set_collision_mask(CommonUtils::get_bitmask_integer_from_string(val, 32));
		} else if (name.to_lower() == "platform_floor_layers" && type == "string" && Object::cast_to<CharacterBody2D>(p_target_node)) {
			Object::cast_to<CharacterBody2D>(p_target_node)->set_platform_floor_layers(CommonUtils::get_bitmask_integer_from_string(val, 32));
		} else if (name.to_lower() == "platform_wall_layers" && type == "string" && Object::cast_to<CharacterBody2D>(p_target_node)) {
			Object::cast_to<CharacterBody2D>(p_target_node)->set_platform_wall_layers(CommonUtils::get_bitmask_integer_from_string(val, 32));
		} else if (name.to_lower() == "navigation_layers" && type == "string" && Object::cast_to<NavigationRegion2D>(p_target_node)) {
			Object::cast_to<NavigationRegion2D>(p_target_node)->set_navigation_layers(CommonUtils::get_bitmask_integer_from_string(val, 32));
		} else if (name.to_lower() == "light_mask" && type == "string" && Object::cast_to<LightOccluder2D>(p_target_node)) {
			Object::cast_to<LightOccluder2D>(p_target_node)->set_occluder_light_mask(CommonUtils::get_bitmask_integer_from_string(val, 20));
		} else {
			if (type == "object") {
				// Skipping object parsing on standard properties due to complexity, fallback to Variant
				p_target_node->set_meta(name, val);
			} else {
				p_target_node->set_meta(name, val);
			}
		}
	}
}

TiledTilemapCreator::GodotType TiledTilemapCreator::get_godot_type(const String &p_godot_type_string) {
	String gts = p_godot_type_string.to_lower();
	if (gts == "") {
		return GODOT_TYPE_EMPTY;
	}
	if (gts == "collision" || gts == "staticbody") {
		return GODOT_TYPE_BODY;
	}
	if (gts == "characterbody") {
		return GODOT_TYPE_CBODY;
	}
	if (gts == "rigidbody") {
		return GODOT_TYPE_RBODY;
	}
	if (gts == "animatablebody") {
		return GODOT_TYPE_ABODY;
	}
	if (gts == "area") {
		return GODOT_TYPE_AREA;
	}
	if (gts == "navigation") {
		return GODOT_TYPE_NAVIGATION;
	}
	if (gts == "occluder") {
		return GODOT_TYPE_OCCLUDER;
	}
	if (gts == "line") {
		return GODOT_TYPE_LINE;
	}
	if (gts == "path") {
		return GODOT_TYPE_PATH;
	}
	if (gts == "polygon") {
		return GODOT_TYPE_POLYGON;
	}
	if (gts == "instance") {
		return GODOT_TYPE_INSTANCE;
	}
	if (gts == "parallax") {
		return GODOT_TYPE_PARALLAX;
	}
	return GODOT_TYPE_UNKNOWN;
}
String TiledTilemapCreator::get_godot_node_type_property(Ref<TiledObject> p_obj, bool &r_property_found) {
	r_property_found = false;
	if ((p_obj->get_properties().size() > 0)) {
		Array props = p_obj->get_properties();
		for (int i = 0; i < props.size(); i++) {
			Ref<TiledProperty> prop = props[i];
			String name = prop->get_name();
			String type = prop->get_property_type();
			String val = prop->get_value();
			if (name.to_lower() == "godot_node_type" && type == "string") {
				r_property_found = true;
				return val;
			}
		}
	}
	return "";
}
TiledTilemapCreator::GodotType TiledTilemapCreator::get_godot_node_type(Ref<TiledObject> p_obj) {
	String class_string = p_obj->get_class_type();
	if (class_string.is_empty()) {
		class_string = p_obj->get_tson_type();
	}

	bool prop_found = false;
	String search_result = get_godot_node_type_property(p_obj, prop_found);
	if (!prop_found) {
		search_result = class_string;
	}

	return get_godot_type(search_result);
}

void TiledTilemapCreator::set_sprite_offset(Sprite2D *p_sprite, float p_width, float p_height, const String &p_alignment) {
	Vector2 offset;
	if (p_alignment == "bottomleft") {
		offset = Vector2(p_width / 2.0f, -p_height / 2.0f);
	} else if (p_alignment == "bottom") {
		offset = Vector2(0.0f, -p_height / 2.0f);
	} else if (p_alignment == "bottomright") {
		offset = Vector2(-p_width / 2.0f, -p_height / 2.0f);
	} else if (p_alignment == "left") {
		offset = Vector2(p_width / 2.0f, 0.0f);
	} else if (p_alignment == "center") {
		offset = Vector2(0.0f, 0.0f);
	} else if (p_alignment == "right") {
		offset = Vector2(-p_width / 2.0f, 0.0f);
	} else if (p_alignment == "topleft") {
		offset = Vector2(p_width / 2.0f, p_height / 2.0f);
	} else if (p_alignment == "top") {
		offset = Vector2(0.0f, p_height / 2.0f);
	} else if (p_alignment == "topright") {
		offset = Vector2(-p_width / 2.0f, p_height / 2.0f);
	} else {
		offset = Vector2(p_width / 2.0f, -p_height / 2.0f); // default bottomleft
	}
	p_sprite->set_offset(offset);
}

Vector2 TiledTilemapCreator::get_instance_offset(float p_width, float p_height, const String &p_r_alignment, const String &p_c_alignment) {
	Vector2 centered_alignment;
	if (p_c_alignment == "bottomleft") {
		centered_alignment = Vector2(p_width / 2.0f, -p_height / 2.0f);
	} else if (p_c_alignment == "bottom") {
		centered_alignment = Vector2(0.0f, -p_height / 2.0f);
	} else if (p_c_alignment == "bottomright") {
		centered_alignment = Vector2(-p_width / 2.0f, -p_height / 2.0f);
	} else if (p_c_alignment == "left") {
		centered_alignment = Vector2(p_width / 2.0f, 0.0f);
	} else if (p_c_alignment == "right") {
		centered_alignment = Vector2(-p_width / 2.0f, 0.0f);
	} else if (p_c_alignment == "topleft") {
		centered_alignment = Vector2(p_width / 2.0f, p_height / 2.0f);
	} else if (p_c_alignment == "top") {
		centered_alignment = Vector2(0.0f, p_height / 2.0f);
	} else if (p_c_alignment == "topright") {
		centered_alignment = Vector2(-p_width / 2.0f, p_height / 2.0f);
	}

	Vector2 right_alignment;
	if (p_r_alignment == "bottomleft") {
		right_alignment = Vector2(-p_width / 2.0f, p_height / 2.0f);
	} else if (p_r_alignment == "bottom") {
		right_alignment = Vector2(0.0f, p_height / 2.0f);
	} else if (p_r_alignment == "bottomright") {
		right_alignment = Vector2(p_width / 2.0f, p_height / 2.0f);
	} else if (p_r_alignment == "left") {
		right_alignment = Vector2(-p_width / 2.0f, 0.0f);
	} else if (p_r_alignment == "right") {
		right_alignment = Vector2(p_width / 2.0f, 0.0f);
	} else if (p_r_alignment == "topleft") {
		right_alignment = Vector2(-p_width / 2.0f, -p_height / 2.0f);
	} else if (p_r_alignment == "top") {
		right_alignment = Vector2(0.0f, -p_height / 2.0f);
	} else if (p_r_alignment == "topright") {
		right_alignment = Vector2(p_width / 2.0f, -p_height / 2.0f);
	}

	return centered_alignment + right_alignment;
}

Vector2 TiledTilemapCreator::get_position_offset(float p_width, float p_height, float p_rotation) {
	Vector2 orig_point(-p_width / 2.0f, -p_height / 2.0f);
	float rot_rad = Math::deg_to_rad(p_rotation);
	Vector2 new_point = orig_point.rotated(rot_rad);
	return orig_point - new_point;
}

void TiledTilemapCreator::convert_metadata_to_obj_properties(TileData *p_td, Ref<TiledObject> p_obj) {
	List<StringName> meta_list;
	p_td->get_meta_list(&meta_list);
	for (const StringName &meta_name_sn : meta_list) {
		String meta_name = meta_name_sn.operator String();
		if (meta_name.to_lower() == "godot_node_type") {
			continue;
		}
		if (meta_name.to_lower() == "_class" && !_add_class_as_metadata) {
			continue;
		}

		Variant meta_val = p_td->get_meta(meta_name);
		Variant::Type meta_type = meta_val.get_type();

		Dictionary prop_dict;
		prop_dict["name"] = meta_name;

		String prop_type = "string";
		if (meta_type == Variant::BOOL) {
			prop_type = "bool";
		} else if (meta_type == Variant::INT) {
			prop_type = "int";
		} else if (meta_type == Variant::FLOAT) {
			prop_type = "float";
		} else if (meta_type == Variant::COLOR) {
			prop_type = "color";
		}

		if (meta_name.to_lower() == "godot_script" || meta_name.to_lower() == "material" || meta_name.to_lower() == "physics_material_override") {
			prop_type = "file";
		}

		prop_dict["type"] = prop_type;
		prop_dict["value"] = meta_val;

		if ((p_obj->get_properties().size() > 0)) {
			Array props = p_obj->get_properties();
			bool found = false;
			for (int p = 0; p < props.size(); p++) {
				Ref<TiledProperty> prop = props[p];
				if (String(prop->get_name()).to_lower() == meta_name.to_lower()) {
					found = true;
					break;
				}
			}
			if (!found) {
				props.push_back(prop_dict);
			}
		} else {
			Array props;
			props.push_back(prop_dict);
			p_obj->get_properties() = props;
		}
	}
}

PackedVector2Array TiledTilemapCreator::polygon_from_array(const Array &p_poly_array) {
	PackedVector2Array polygon;
	for (int i = 0; i < p_poly_array.size(); i++) {
		Vector2 pt = p_poly_array[i];
		Vector2 p_coord = transpose_coords(pt.x, pt.y, true);
		polygon.push_back(p_coord);
	}
	return polygon;
}

PackedVector2Array TiledTilemapCreator::polygon_from_rectangle(float p_width, float p_height) {
	PackedVector2Array polygon;
	polygon.resize(4);
	polygon.set(0, Vector2(0, 0));
	polygon.set(1, Vector2(polygon[0].x, polygon[0].y + p_height));
	polygon.set(2, Vector2(polygon[0].x + p_width, polygon[1].y));
	polygon.set(3, Vector2(polygon[2].x, polygon[0].y));

	polygon.set(1, transpose_coords(polygon[1].x, polygon[1].y, true));
	polygon.set(2, transpose_coords(polygon[2].x, polygon[2].y, true));
	polygon.set(3, transpose_coords(polygon[3].x, polygon[3].y, true));
	return polygon;
}

Vector2 TiledTilemapCreator::transpose_coords(float p_x, float p_y, bool p_iso_offset) {
	if (_current_tileset_orientation == "isometric") {
		float trans_x = (p_x - p_y) * _map_tile_width / (float)_map_tile_height / 2.0f;
		float trans_y = (p_x + p_y) * 0.5f;
		if (p_iso_offset) {
			trans_y += (float)_map_tile_height / 2.0f;
		}
		return Vector2(trans_x, trans_y);
	}
	return Vector2(p_x, p_y);
}

PackedVector2Array TiledTilemapCreator::get_rotated_polygon(const PackedVector2Array &p_polygon, float p_rotation) {
	PackedVector2Array ret;
	float rot_rad = Math::deg_to_rad(p_rotation);
	for (int i = 0; i < p_polygon.size(); i++) {
		ret.push_back(p_polygon[i].rotated(rot_rad));
	}
	return ret;
}
