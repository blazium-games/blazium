/**************************************************************************/
/*  tiled_tileset_creator.h                                               */
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

#pragma once

#include "scene/resources/2d/tile_set.h"

// Equivalent to TilesetCreator.gd

#include "scene/resources/texture.h"
#include "tiled_common.h"
#include "tiled_custom_types.h"
#include "tileson_gd_bindings.h"

class TiledTilesetCreator {
private:
	Ref<TileSet> _tileset;
	Ref<TileSetAtlasSource> _current_atlas_source;
	int _current_max_x = 0;
	int _current_max_y = 0;
	String _base_path_map;
	String _base_path_tileset;
	int _terrain_counter = 0;
	int _tile_count = 0;
	int _columns = 0;
	Vector2i _tile_size;
	int _physics_layer_counter = -1;
	int _navigation_layer_counter = -1;
	int _occlusion_layer_counter = -1;
	int _object_groups_counter = 0;
	bool _append = false;

	Vector2i _map_tile_size;
	Vector2i _grid_size;
	Vector2i _tile_offset;
	String _object_alignment;
	String _tileset_orientation;
	bool _map_wangset_to_terrain = false;
	String _custom_data_prefix;
	int _current_first_gid = -1;

	// Internal data tracking mimicking YATI states
	Array _atlas_sources;
	Dictionary _object_groups; // We'll keep this as a mapping of tile_id -> Ref<TiledLayer> wrapper
	Ref<TiledCustomTypes> _custom_types;

	void create_or_append(Ref<TiledTileset> p_tile_set_ro);
	void handle_tiles(const Array &p_tiles);
	void handle_wangsets_old_mapping(const Array &p_wangsets);
	void handle_wangsets(const Array &p_wangsets);
	void handle_animation(Array p_frames, int p_tile_id);
	void handle_objectgroup(Ref<TiledLayer> p_object_group, TileData *p_current_tile, int p_tile_id);
	void handle_tile_properties(const Array &p_properties, TileData *p_current_tile);
	void handle_tileset_properties(const Array &p_properties);

	void register_atlas_source(int p_source_id, int p_num_tiles, int p_assigned_tile_id, const Vector2i &p_tile_offset);
	void register_object_group(int p_tile_id, Ref<TiledLayer> p_object_group);

	TileData *create_tile_if_not_existing_and_get_tiledata(int p_tile_id);
	Vector2 transpose_coords(float p_x, float p_y);
	int get_special_property(const Array &p_properties, const String &p_property_name);
	void ensure_layer_existing(int p_tp, int p_layer);

public:
	Ref<TileSet> create_from_dictionary_array(const Array &p_tile_sets);

	void set_base_path(const String &p_source_file);
	void set_map_parameters(const Vector2i &p_map_tile_size);
	void map_wangset_to_terrain();
	void set_custom_data_prefix(const String &p_value);
	void set_custom_types(Ref<TiledCustomTypes> p_custom_types) { _custom_types = p_custom_types; }

	Array get_registered_atlas_sources() { return _atlas_sources; }
	Dictionary get_registered_object_groups() { return _object_groups; }

	TiledTilesetCreator();
};
