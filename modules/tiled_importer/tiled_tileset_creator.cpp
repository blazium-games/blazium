/**************************************************************************/
/*  tiled_tileset_creator.cpp                                             */
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

#include "tiled_tileset_creator.h"
#include "scene/2d/light_occluder_2d.h"
#include "scene/2d/navigation/navigation_region_2d.h"
#include "scene/resources/2d/navigation_polygon.h"
#include "scene/resources/2d/polygon_path_finder.h"
#include "scene/resources/placeholder_textures.h"
#include "tiled_dictionary_builder.h"

// Note: Replicates roughly 800 lines of TilesetCreator.gd

TiledTilesetCreator::TiledTilesetCreator() {
}

void TiledTilesetCreator::set_base_path(const String &p_source_file) {
	_base_path_map = p_source_file.get_base_dir();
	_base_path_tileset = _base_path_map;
}

void TiledTilesetCreator::set_map_parameters(const Vector2i &p_map_tile_size) {
	_map_tile_size = p_map_tile_size;
}

void TiledTilesetCreator::map_wangset_to_terrain() {
	_map_wangset_to_terrain = true;
}

void TiledTilesetCreator::set_custom_data_prefix(const String &p_value) {
	_custom_data_prefix = p_value;
}

Ref<TileSet> TiledTilesetCreator::create_from_dictionary_array(const Array &p_tile_sets) {
	for (int i = 0; i < p_tile_sets.size(); ++i) {
		Ref<TiledTileset> tile_set = p_tile_sets[i];
		if (!tile_set.is_valid()) {
			continue;
		}
		create_or_append(tile_set);
		_append = true;
	}

	return _tileset;
}

void TiledTilesetCreator::create_or_append(Ref<TiledTileset> p_tile_set_ro) {
	Ref<TiledTileset> p_tile_set = p_tile_set_ro;
	if (p_tile_set->get_name() == "AutoMap Rules") {
		return;
	}

	if (!_append) {
		_tileset.instantiate();
		_tileset->add_custom_data_layer();
		_tileset->set_custom_data_layer_name(0, "__internal__");
		_tileset->set_custom_data_layer_type(0, Variant::INT);
	}

	_tile_size = _map_tile_size;
	if (!_append) {
		_tileset->set_tile_size(_map_tile_size);
	}

	_tile_count = p_tile_set->get_tile_count();
	_columns = p_tile_set->get_columns();
	_tileset_orientation = "orthogonal";
	_grid_size = _tile_size;

	_tile_offset = p_tile_set->get_tile_offset();

	_current_first_gid = p_tile_set->get_first_gid();

	if (p_tile_set->get_grid().is_valid()) {
		Ref<TiledGrid> grid = p_tile_set->get_grid();
		if ((!grid->get_orientation().is_empty())) {
			_tileset_orientation = grid->get_orientation();
		}
		_grid_size.x = grid->get_size().x;
		_grid_size.y = grid->get_size().y;
	}

	_object_alignment = "unspecified" /* TODO: convert enum */;

	if (_append) {
		_terrain_counter = 0;
	}

	if (!p_tile_set->get_image().is_empty()) {
		_current_atlas_source.instantiate();
		int added_source_id = _tileset->add_source(_current_atlas_source, get_special_property(p_tile_set->get_properties(), "godot_atlas_id"));
		_current_atlas_source->set_texture_region_size(_tile_size);

		if ((p_tile_set->get_margin() > 0)) {
			_current_atlas_source->set_margins(Vector2i(p_tile_set->get_margin(), p_tile_set->get_margin()));
		}
		if ((p_tile_set->get_spacing() > 0)) {
			_current_atlas_source->set_separation(Vector2i(p_tile_set->get_spacing(), p_tile_set->get_spacing()));
		}

		Ref<Texture2D> texture = DataLoader::load_image(p_tile_set->get_image(), _base_path_tileset);
		if (texture.is_valid()) {
			_current_atlas_source->set_texture(texture);
			_columns = texture->get_width() / _tile_size.x;
			_tile_count = _columns * texture->get_height() / _tile_size.y;

			register_atlas_source(added_source_id, _tile_count, -1, _tile_offset);
			Vector2i atlas_grid = _current_atlas_source->get_atlas_grid_size();
			_current_max_x = atlas_grid.x - 1;
			_current_max_y = atlas_grid.y - 1;
		}
	}

	if ((p_tile_set->get_tiles().size() > 0)) {
		handle_tiles(p_tile_set->get_tiles());
	}
	if ((p_tile_set->get_wang_sets().size() > 0)) {
		if (_map_wangset_to_terrain) {
			handle_wangsets_old_mapping(p_tile_set->get_wang_sets());
		} else {
			handle_wangsets(p_tile_set->get_wang_sets());
		}
	}
	if ((p_tile_set->get_properties().size() > 0)) {
		handle_tileset_properties(p_tile_set->get_properties());
	}
}

TileData *TiledTilesetCreator::create_tile_if_not_existing_and_get_tiledata(int p_tile_id) {
	if (p_tile_id < _tile_count) {
		int row = p_tile_id / _columns;
		int col = p_tile_id % _columns;
		Vector2i tile_coords(col, row);
		if (col > _current_max_x || row > _current_max_y) {
			ERR_PRINT("-- Tile " + itos(p_tile_id) + " at " + itos(col) + "," + itos(row) + " outside texture range. -> Skipped");
			return nullptr;
		}
		if (_current_atlas_source.is_null()) {
			ERR_PRINT("-- _current_atlas_source is null. -> Skipped");
			return nullptr;
		}
		Vector2i tile_at_coords = _current_atlas_source->get_tile_at_coords(tile_coords);
		if (tile_at_coords == Vector2i(-1, -1)) {
			_current_atlas_source->create_tile(tile_coords);
		} else if (tile_at_coords != tile_coords) {
			ERR_PRINT("WARNING: tile_at_coords not equal tile_coords! -> Tile skipped");
			return nullptr;
		}
		return _current_atlas_source->get_tile_data(tile_coords, 0);
	}
	ERR_PRINT("-- Tile id " + itos(p_tile_id) + " outside tile count range (0-" + itos(_tile_count - 1) + "). -> Skipped.");
	return nullptr;
}

void TiledTilesetCreator::handle_tiles(const Array &p_tiles) {
	for (int i = 0; i < p_tiles.size(); i++) {
		Ref<TiledTile> tile = p_tiles[i];
		if (_custom_types.is_valid()) {
		}
		int tile_id = tile->get_id();
		String tile_class = tile->get_class_type();
		if (tile_class.is_empty()) {
			tile_class = tile->get_tson_type();
		}

		TileData *current_tile = nullptr;
		if ((!tile->get_image().is_empty())) {
			_current_atlas_source.instantiate();
			int added_source_id = _tileset->add_source(_current_atlas_source, get_special_property(tile->get_properties(), "godot_atlas_id"));
			register_atlas_source(added_source_id, 1, tile_id, _tile_offset);

			String texture_path = tile->get_image();
			String ext = texture_path.get_extension().to_lower();
			if (ext == "tmx" || ext == "tmj") {
				Ref<PlaceholderTexture2D> placeholder_texture(memnew(PlaceholderTexture2D));
				int width = tile->get_image_size().x;
				int height = tile->get_image_size().y;
				placeholder_texture->set_size(Vector2(width, height));
				_current_atlas_source->set_texture(placeholder_texture);
			} else {
				_current_atlas_source->set_texture(DataLoader::load_image(texture_path, _base_path_tileset));
			}

			_current_atlas_source->set_name(texture_path.get_file().get_basename());
			int texture_width = _current_atlas_source->get_texture().is_valid() ? _current_atlas_source->get_texture()->get_width() : 0;
			if ((tile->get_image_size().x > 0)) {
				texture_width = tile->get_image_size().x;
			}

			int texture_height = _current_atlas_source->get_texture().is_valid() ? _current_atlas_source->get_texture()->get_height() : 0;
			if ((tile->get_image_size().y > 0)) {
				texture_height = tile->get_image_size().y;
			}

			_current_atlas_source->set_texture_region_size(Vector2i(texture_width, texture_height));

			int tile_offset_x = 0 /* x */;
			int tile_offset_y = 0 /* y */;
			_current_atlas_source->set_margins(Vector2i(tile_offset_x, tile_offset_y));
			_tile_size = _current_atlas_source->get_texture_region_size();

			_current_atlas_source->create_tile(Vector2i());
			current_tile = _current_atlas_source->get_tile_data(Vector2i(), 0);
			current_tile->set_probability(1.0 /* tileson doesn't expose hit prob */);
		} else {
			current_tile = create_tile_if_not_existing_and_get_tiledata(tile_id);
			if (!current_tile) {
				continue;
			}

			if (_tile_size.x != _map_tile_size.x || _tile_size.y != _map_tile_size.y) {
				int diff_x = _tile_size.x - _map_tile_size.x;
				if (diff_x % 2 != 0) {
					diff_x -= 1;
				}
				int diff_y = _tile_size.y - _map_tile_size.y;
				if (diff_y % 2 != 0) {
					diff_y += 1;
				}
				current_tile->set_texture_origin(Vector2i(-diff_x / 2, diff_y / 2) - _tile_offset);
			}
		}

		if (_tile_offset != Vector2i() && current_tile->get_texture_origin() == Vector2i()) {
			current_tile->set_texture_origin(current_tile->get_texture_origin() - _tile_offset);
		}

		if ((false /* tileson doesn't expose hit prob */)) {
			current_tile->set_probability(1.0);
		}
		if (tile->get_animation().is_valid()) {
			handle_animation(tile->get_animation()->get_frames(), tile_id);
		}
		if (tile->get_objectgroup().is_valid()) {
			handle_objectgroup(tile->get_objectgroup(), current_tile, tile_id);
		}

		if (!tile_class.is_empty()) {
			current_tile->set_meta("class", tile_class);
		}
		if ((tile->get_properties().size() > 0)) {
			handle_tile_properties(tile->get_properties(), current_tile);
		}
	}
}

void TiledTilesetCreator::handle_wangsets_old_mapping(const Array &p_wangsets) {}
void TiledTilesetCreator::handle_wangsets(const Array &p_wangsets) {}
void TiledTilesetCreator::handle_animation(Array p_frames, int p_tile_id) {
	if (p_frames.is_empty()) {
		return;
	}

	int frame_count = p_frames.size();
	Vector2i separation_vect;
	int anim_columns = 0;

	Vector2i tile_coords(p_tile_id % _columns, p_tile_id / _columns);
	Vector2i first_coords;
	Vector2i expected_step;
	Vector2i prev_coords;

	for (int i = 0; i < frame_count; i++) {
		Ref<TiledFrame> frame = p_frames[i];
		int frame_tile_id = frame->get_tile_id();
		Vector2i coords(frame_tile_id % _columns, frame_tile_id / _columns);

		if (i == 0) {
			first_coords = coords;
			// Godot 4 expects the tile coordinates defining the animation to match the 0th frame.
			if (first_coords != tile_coords) {
				ERR_PRINT("-- Animated tile " + itos(p_tile_id) + " 1st frame is NOT the tile itself. Skipped.");
				return;
			}
		} else if (i == 1) {
			expected_step = coords - first_coords;

			// Godot 4 requires progression to be either purely horizontal or purely vertical
			if ((expected_step.x != 0 && expected_step.y != 0) || (expected_step.x == 0 && expected_step.y == 0)) {
				ERR_PRINT("-- Animated tile " + itos(p_tile_id) + ": Diagonal or non-linear sequences not supported in Godot 4. -> Skipped");
				return;
			}

			// Negative steps are generally impossible to map cleanly using Godot sets
			if (expected_step.x < 0 || expected_step.y < 0) {
				ERR_PRINT("-- Animated tile " + itos(p_tile_id) + ": Negative progression direction. -> Skipped");
				return;
			}

			if (expected_step.y > 0) {
				anim_columns = 1; // Wraps every 1 column (progresses vertically)
				separation_vect = Vector2i(0, expected_step.y - 1);
			} else if (expected_step.x > 0) {
				anim_columns = frame_count; // Horizontal progression over multiple columns natively
				separation_vect = Vector2i(expected_step.x - 1, 0);
			}
		} else {
			// Validate subsequent frames follow the same exact Vector2i spacing
			if (coords != prev_coords + expected_step) {
				ERR_PRINT("-- Animated tile " + itos(p_tile_id) + ": Succession of tiles not equidistant/supported in Godot 4. -> Skipped");
				return;
			}
		}
		prev_coords = coords;
	}

	if (_current_atlas_source->has_room_for_tile(tile_coords, Vector2i(1, 1), anim_columns, separation_vect, frame_count, tile_coords)) {
		_current_atlas_source->set_tile_animation_separation(tile_coords, separation_vect);
		_current_atlas_source->set_tile_animation_columns(tile_coords, anim_columns);
		_current_atlas_source->set_tile_animation_frames_count(tile_coords, frame_count);
		for (int i = 0; i < frame_count; i++) {
			Ref<TiledFrame> frame = p_frames[i];
			float duration_in_secs = float(frame->get_duration()) / 1000.0f;
			_current_atlas_source->set_tile_animation_frame_duration(tile_coords, i, duration_in_secs);
		}
	} else {
		ERR_PRINT("-- TileId " + itos(p_tile_id) + ": Not enough room for all animation frames.");
	}
}

void TiledTilesetCreator::handle_objectgroup(Ref<TiledLayer> p_object_group, TileData *p_current_tile, int p_tile_id) {
	_object_groups_counter += 1;
	register_object_group(_object_groups_counter, p_object_group);
	p_current_tile->set_custom_data("__internal__", _object_groups_counter);

	Dictionary polygon_indices;
	Array objects = p_object_group->get_objects();

	for (int i = 0; i < objects.size(); i++) {
		Ref<TiledObject> obj = objects[i];
		if (obj->is_point()) {
			continue;
		}

		Vector2 object_base_coords(obj->get_position().x, obj->get_position().y);
		object_base_coords = transpose_coords(object_base_coords.x, object_base_coords.y);
		object_base_coords -= Vector2(p_current_tile->get_texture_origin());

		if (_tileset_orientation == "isometric") {
			object_base_coords.y -= _grid_size.y / 2.0;
			if (_grid_size.y != _tile_size.y) {
				object_base_coords.y += (_tile_size.y - _grid_size.y) / 2.0;
			}
		} else {
			object_base_coords -= Vector2(_tile_size) / 2.0;
		}

		float rot = obj->get_rotation();
		float sin_a = Math::sin(Math::deg_to_rad(rot));
		float cos_a = Math::cos(Math::deg_to_rad(rot));

		Vector<Vector2> polygon;

		if ((obj->get_polygon().size() > 0) || (obj->get_polyline().size() > 0)) {
			Array polygon_points = (obj->get_polygon().size() > 0) ? Array(obj->get_polygon()) : Array(obj->get_polyline());
			if (polygon_points.size() < 3) {
				ERR_PRINT("-- Skipped invalid polygon on tile " + itos(p_tile_id) + " (less than 3 points)");
				break;
			}
			for (int p_idx = 0; p_idx < polygon_points.size(); p_idx++) {
				Vector2 pt = polygon_points[p_idx];
				Vector2 p_coord = transpose_coords(pt.x, pt.y);
				Vector2 p_coord_rot(p_coord.x * cos_a - p_coord.y * sin_a, p_coord.x * sin_a + p_coord.y * cos_a);
				polygon.push_back(object_base_coords + p_coord_rot);
			}
		} else {
			polygon.resize(4);
			polygon.write[0] = Vector2();
			polygon.write[1].y = polygon[0].y + float(obj->get_size().y);
			polygon.write[1].x = polygon[0].x;
			polygon.write[2].y = polygon[1].y;
			polygon.write[2].x = polygon[0].x + float(obj->get_size().x);
			polygon.write[3].y = polygon[0].y;
			polygon.write[3].x = polygon[2].x;

			for (int p_idx = 0; p_idx < polygon.size(); p_idx++) {
				Vector2 pt_trans = transpose_coords(polygon[p_idx].x, polygon[p_idx].y);
				Vector2 pt_rot(pt_trans.x * cos_a - pt_trans.y * sin_a, pt_trans.x * sin_a + pt_trans.y * cos_a);
				polygon.write[p_idx] = object_base_coords + pt_rot;
			}
		}

		int nav = get_special_property(obj->get_properties(), "navigation_layer");
		if (nav >= 0) {
			Ref<NavigationPolygon> nav_p = memnew(NavigationPolygon);
			nav_p->add_outline(polygon);
			nav_p->set_vertices(polygon);
			PackedInt32Array pg;
			for (int idx = 0; idx < polygon.size(); idx++) {
				pg.push_back(idx);
			}
			nav_p->add_polygon(pg);

			ensure_layer_existing(1, nav);
			p_current_tile->set_navigation_polygon(nav, nav_p);
		}

		int occ = get_special_property(obj->get_properties(), "occlusion_layer");
		if (occ >= 0) {
			Ref<OccluderPolygon2D> occ_p = memnew(OccluderPolygon2D);
			occ_p->set_polygon(polygon);
			ensure_layer_existing(2, occ);
			p_current_tile->set_occluder(occ, occ_p);
		}

		int phys = get_special_property(obj->get_properties(), "physics_layer");
		if (phys < 0 && nav < 0 && occ < 0) {
			phys = 0;
		}
		if (phys < 0) {
			continue;
		}

		int polygon_index = polygon_indices.get(phys, 0);
		polygon_indices[phys] = polygon_index + 1;
		ensure_layer_existing(0, phys);
		p_current_tile->add_collision_polygon(phys);
		p_current_tile->set_collision_polygon_points(phys, polygon_index, polygon);

		if (!(obj->get_properties().size() > 0)) {
			continue;
		}
		Array obj_props = obj->get_properties();
		for (int p_idx = 0; p_idx < obj_props.size(); p_idx++) {
			Ref<TiledProperty> property = obj_props[p_idx];
			String name = property->get_name();
			String type = property->get_property_type();
			String val = property->get_value();
			if (name.is_empty()) {
				continue;
			}
			if (name.to_lower() == "one_way" && type == "bool") {
				p_current_tile->set_collision_polygon_one_way(phys, polygon_index, val.to_lower() == "true");
			} else if (name.to_lower() == "one_way_margin" && type == "int") {
				p_current_tile->set_collision_polygon_one_way_margin(phys, polygon_index, val.to_int());
			}
		}
	}
}
void TiledTilesetCreator::handle_tile_properties(const Array &p_properties, TileData *p_current_tile) {
	for (int i = 0; i < p_properties.size(); i++) {
		Ref<TiledProperty> property = p_properties[i];
		String name = property->get_name();
		String type = property->get_property_type();
		String val = property->get_value();

		if (name.is_empty()) {
			continue;
		}

		if (name.to_lower() == "texture_origin_x" && type == "int") {
			p_current_tile->set_texture_origin(Vector2i(val.to_int(), p_current_tile->get_texture_origin().y));
		} else if (name.to_lower() == "texture_origin_y" && type == "int") {
			p_current_tile->set_texture_origin(Vector2i(p_current_tile->get_texture_origin().x, val.to_int()));
		} else if (name.to_lower() == "modulate" && type == "string") {
			p_current_tile->set_modulate(Color::html(val));
		} else if (name.to_lower() == "material" && type == "file") {
			p_current_tile->set_material(DataLoader::load_resource_from_file(val, _base_path_tileset));
		} else if (name.to_lower() == "z_index" && type == "int") {
			p_current_tile->set_z_index(val.to_int());
		} else if (name.to_lower() == "y_sort_origin" && type == "int") {
			p_current_tile->set_y_sort_origin(val.to_int());
		} else if (name.to_lower() != "godot_atlas_id") {
			if (_custom_data_prefix.is_empty() || name.to_lower().begins_with(_custom_data_prefix)) {
				String layer_name = name;
				if (!_custom_data_prefix.is_empty()) {
					layer_name = name.substr(_custom_data_prefix.length());
				}

				int custom_layer = _tileset->get_custom_data_layer_by_name(layer_name);
				if (custom_layer < 0) {
					_tileset->add_custom_data_layer();
					custom_layer = _tileset->get_custom_data_layers_count() - 1;
					_tileset->set_custom_data_layer_name(custom_layer, layer_name);

					Variant::Type vtype = Variant::STRING;
					if (type == "bool") {
						vtype = Variant::BOOL;
					} else if (type == "int") {
						vtype = Variant::INT;
					} else if (type == "float") {
						vtype = Variant::FLOAT;
					} else if (type == "color") {
						vtype = Variant::COLOR;
					}
					_tileset->set_custom_data_layer_type(custom_layer, vtype);
				}

				// Standard explicit parsing placeholder since we don't have GetRightTypedValue from CommonUtils ported fully
				p_current_tile->set_custom_data(layer_name, val);
			}

			if (_custom_data_prefix.is_empty() || !name.to_lower().begins_with(_custom_data_prefix)) {
				p_current_tile->set_meta(name, val);
			}
		}
	}
}

void TiledTilesetCreator::handle_tileset_properties(const Array &p_properties) {
	for (int i = 0; i < p_properties.size(); i++) {
		Ref<TiledProperty> property = p_properties[i];
		String name = property->get_name();
		String type = property->get_property_type();
		String val = property->get_value();

		if (name.is_empty()) {
			continue;
		}

		int layer_index = 0;
		if (name.to_lower() == "collision_layer" && type == "string") {
			ensure_layer_existing(0, 0);
			_tileset->set_physics_layer_collision_layer(0, CommonUtils::get_bitmask_integer_from_string(val, 32));
		} else if (name.to_lower().begins_with("collision_layer_") && type == "string") {
			String suffix = name.substr(16);
			if (suffix.is_valid_int()) {
				layer_index = suffix.to_int();
				ensure_layer_existing(0, layer_index);
				_tileset->set_physics_layer_collision_layer(layer_index, CommonUtils::get_bitmask_integer_from_string(val, 32));
			}
		} else if (name.to_lower() == "collision_mask" && type == "string") {
			ensure_layer_existing(0, 0);
			_tileset->set_physics_layer_collision_mask(0, CommonUtils::get_bitmask_integer_from_string(val, 32));
		} else if (name.to_lower().begins_with("collision_mask_") && type == "string") {
			String suffix = name.substr(15);
			if (suffix.is_valid_int()) {
				layer_index = suffix.to_int();
				ensure_layer_existing(0, layer_index);
				_tileset->set_physics_layer_collision_mask(layer_index, CommonUtils::get_bitmask_integer_from_string(val, 32));
			}
		} else if (name.to_lower() == "layers" && type == "string") {
			ensure_layer_existing(1, 0);
			_tileset->set_navigation_layer_layers(0, CommonUtils::get_bitmask_integer_from_string(val, 32));
		} else if (name.to_lower().begins_with("layers_") && type == "string") {
			String suffix = name.substr(7);
			if (suffix.is_valid_int()) {
				layer_index = suffix.to_int();
				ensure_layer_existing(1, layer_index);
				_tileset->set_navigation_layer_layers(layer_index, CommonUtils::get_bitmask_integer_from_string(val, 32));
			}
		} else if (name.to_lower() == "light_mask" && type == "string") {
			ensure_layer_existing(2, 0);
			_tileset->set_occlusion_layer_light_mask(0, CommonUtils::get_bitmask_integer_from_string(val, 20));
		} else if (name.to_lower().begins_with("light_mask_") && type == "string") {
			String suffix = name.substr(11);
			if (suffix.is_valid_int()) {
				layer_index = suffix.to_int();
				ensure_layer_existing(2, layer_index);
				_tileset->set_occlusion_layer_light_mask(layer_index, CommonUtils::get_bitmask_integer_from_string(val, 20));
			}
		} else if (name.to_lower() == "sdf_collision_" && type == "bool") {
			ensure_layer_existing(2, 0);
			_tileset->set_occlusion_layer_sdf_collision(0, val.to_lower() == "true");
		} else if (name.to_lower().begins_with("sdf_collision_") && type == "bool") {
			String suffix = name.substr(14);
			if (suffix.is_valid_int()) {
				layer_index = suffix.to_int();
				ensure_layer_existing(2, layer_index);
				_tileset->set_occlusion_layer_sdf_collision(layer_index, val.to_lower() == "true");
			}
		} else if (name.to_lower() == "uv_clipping" && type == "bool") {
			_tileset->set_uv_clipping(val.to_lower() == "true");
		} else if (name.to_lower() == "resource_local_to_scene" && type == "bool") {
			_tileset->set_local_to_scene(val.to_lower() == "true");
		} else if (name.to_lower() == "resource_name" && type == "string") {
			_tileset->set_name(val);
		} else if (name.to_lower() != "godot_atlas_id") {
			_tileset->set_meta(name, val);
		}
	}
}

void TiledTilesetCreator::register_atlas_source(int p_source_id, int p_num_tiles, int p_assigned_tile_id, const Vector2i &p_tile_offset) {
	Dictionary item;
	item["sourceId"] = p_source_id;
	item["numTiles"] = p_num_tiles;
	item["assignedId"] = p_assigned_tile_id;
	item["tileOffset"] = p_tile_offset;
	item["tilesetOrientation"] = _tileset_orientation;
	item["objectAlignment"] = _object_alignment;
	item["firstGid"] = _current_first_gid;
	_atlas_sources.push_back(item);
}

void TiledTilesetCreator::register_object_group(int p_tile_id, Ref<TiledLayer> p_object_group) {
	_object_groups[p_tile_id] = p_object_group;
}

Vector2 TiledTilesetCreator::transpose_coords(float p_x, float p_y) {
	if (_tileset_orientation == "isometric") {
		float trans_x = (p_x - p_y) * _grid_size.x / _grid_size.y / 2.0;
		float trans_y = (p_x + p_y) * 0.5;
		return Vector2(trans_x, trans_y);
	}
	return Vector2(p_x, p_y);
}

int TiledTilesetCreator::get_special_property(const Array &p_properties, const String &p_property_name) {
	for (int i = 0; i < p_properties.size(); i++) {
		Ref<TiledProperty> prop = p_properties[i];
		if (String(prop->get_name()).to_lower() == p_property_name.to_lower()) {
			return (int)prop->get_value();
		}
	}
	return -1;
}

void TiledTilesetCreator::ensure_layer_existing(int p_tp, int p_layer) {
	switch (p_tp) {
		case 0: // PHYSICS
			while (_physics_layer_counter < p_layer) {
				_tileset->add_physics_layer();
				_physics_layer_counter += 1;
			}
			break;
		case 1: // NAVIGATION
			while (_navigation_layer_counter < p_layer) {
				_tileset->add_navigation_layer();
				_navigation_layer_counter += 1;
			}
			break;
		case 2: // OCCLUSION
			while (_occlusion_layer_counter < p_layer) {
				_tileset->add_occlusion_layer();
				_occlusion_layer_counter += 1;
			}
			break;
	}
}
