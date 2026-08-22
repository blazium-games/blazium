/**************************************************************************/
/*  justamcp_tilemap_tools.cpp                                            */
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

#include "justamcp_tilemap_tools.h"
#include "../justamcp_editor_scene_access.h"
#include "../justamcp_mcp_tool_macros.h"
#include "justamcp_agent_helpers.h"
#include "justamcp_scene_file_io.h"

#include "core/io/resource_loader.h"
#include "scene/2d/tile_map.h"
#include "scene/2d/tile_map_layer.h"
#include "scene/resources/2d/tile_set.h"
#include "scene/resources/texture.h"

JustAMCPTileMapTools::JustAMCPTileMapTools() {
}

JustAMCPTileMapTools::~JustAMCPTileMapTools() {
}

JustAMCPTileMapTools::ResolvedTiles JustAMCPTileMapTools::_resolve(const Dictionary &p_params) {
	ResolvedTiles resolved;
	const String node_path = p_params.get("node_path", p_params.get("tilemap_node", ""));
	if (node_path.is_empty()) {
		resolved.error = MCP_INVALID_PARAMS("Missing node_path");
		return resolved;
	}
	const int layer_index = int(p_params.get("layer", 0));
	if (layer_index < 0) {
		resolved.error = MCP_INVALID_PARAMS("layer must be >= 0");
		return resolved;
	}
	String file_path = p_params.get("file_path", p_params.get("scene_path", ""));
	if (!file_path.is_empty()) {
		String sandbox_error;
		if (!justamcp_canonical_sandbox_path(file_path, file_path, sandbox_error)) {
			resolved.error = MCP_ERROR(-32602, sandbox_error);
			return resolved;
		}
		resolved.file_path = file_path;
		Node *root = nullptr;
		Dictionary load_err = justamcp_load_scene_root(resolved.file_path, &root);
		if (!load_err.is_empty()) {
			resolved.error = load_err.has("error") ? MCP_ERROR(-32000, String(load_err["error"])) : MCP_ERROR(-32000, "Failed to load scene");
			return resolved;
		}
		resolved.owned_root = root;
		Node *node = justamcp_find_node_in_root(root, node_path);
		resolved.target = justamcp_tile_target_from_node(node, layer_index);
		if (!resolved.target.valid()) {
			memdelete(root);
			resolved.owned_root = nullptr;
			resolved.error = MCP_ERROR(-32000, "Node is not a TileMap or TileMapLayer: " + node_path);
			return resolved;
		}
		return resolved;
	}

	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		resolved.error = MCP_ERROR(-32000, "No scene is currently open.");
		return resolved;
	}
	Node *node = justamcp_find_node_in_root(root, node_path);
	resolved.target = justamcp_tile_target_from_node(node, layer_index);
	if (!resolved.target.valid()) {
		resolved.error = MCP_ERROR(-32000, "Node is not a TileMap or TileMapLayer: " + node_path);
	}
	return resolved;
}

void JustAMCPTileMapTools::_commit_if_file(ResolvedTiles &p_resolved) {
	if (p_resolved.owned_root) {
		justamcp_save_scene_root(p_resolved.owned_root, p_resolved.file_path, true);
		p_resolved.owned_root = nullptr;
	}
}

Dictionary JustAMCPTileMapTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	if (p_tool_name == "tilemap_set_cell") {
		return tilemap_set_cell(p_args);
	}
	if (p_tool_name == "tilemap_fill_rect") {
		return tilemap_fill_rect(p_args);
	}
	if (p_tool_name == "tilemap_get_cell") {
		return tilemap_get_cell(p_args);
	}
	if (p_tool_name == "tilemap_clear") {
		return tilemap_clear(p_args);
	}
	if (p_tool_name == "tilemap_get_info") {
		return tilemap_get_info(p_args);
	}
	if (p_tool_name == "tilemap_get_used_cells") {
		return tilemap_get_used_cells(p_args);
	}
	if (p_tool_name == "tilemap_draw_h_line") {
		return tilemap_draw_h_line(p_args);
	}
	if (p_tool_name == "tilemap_draw_v_line") {
		return tilemap_draw_v_line(p_args);
	}
	if (p_tool_name == "tilemap_draw_stairs") {
		return tilemap_draw_stairs(p_args);
	}
	if (p_tool_name == "tilemap_erase_rect") {
		return tilemap_erase_rect(p_args);
	}
	if (p_tool_name == "tilemap_configure_atlas") {
		return tilemap_configure_atlas(p_args);
	}
	if (p_tool_name == "validate_tilemap_structure") {
		return validate_tilemap_structure(p_args);
	}
	return Dictionary();
}

Dictionary JustAMCPTileMapTools::_paint_cells(const Dictionary &p_params, const Vector<Vector2i> &p_cells, const String &p_label) {
	if (p_cells.size() > 10000) {
		return MCP_INVALID_PARAMS("Tile operation exceeds the 10000 cell cap.");
	}
	ResolvedTiles resolved = _resolve(p_params);
	if (!resolved.ok()) {
		return resolved.error;
	}
	const int source_id = int(p_params.get("source_id", 0));
	const int atlas_x = int(p_params.get("atlas_x", 0));
	const int atlas_y = int(p_params.get("atlas_y", 0));
	const int alternative = int(p_params.get("alternative", 0));
	if (source_id < 0 || atlas_x < 0 || atlas_y < 0 || alternative < 0) {
		return MCP_INVALID_PARAMS("source_id, atlas coords, and alternative must be >= 0.");
	}
	const int count = justamcp_tile_set_cells(resolved.target, p_cells, source_id, Vector2i(atlas_x, atlas_y), alternative);
	Dictionary res;
	res["written"] = count;
	res["label"] = p_label;
	res["node_class"] = resolved.target.node_class;
	_commit_if_file(resolved);
	return justamcp_ok(res);
}

Dictionary JustAMCPTileMapTools::tilemap_set_cell(const Dictionary &p_params) {
	ResolvedTiles resolved = _resolve(p_params);
	if (!resolved.ok()) {
		return resolved.error;
	}
	const int x = int(p_params.get("x", 0));
	const int y = int(p_params.get("y", 0));
	const int source_id = int(p_params.get("source_id", 0));
	const int atlas_x = int(p_params.get("atlas_x", 0));
	const int atlas_y = int(p_params.get("atlas_y", 0));
	const int alternative = int(p_params.get("alternative", 0));
	if (source_id < 0 || atlas_x < 0 || atlas_y < 0 || alternative < 0) {
		return MCP_INVALID_PARAMS("source_id, atlas coords, and alternative must be >= 0.");
	}
	justamcp_tile_set_cell(resolved.target, Vector2i(x, y), source_id, Vector2i(atlas_x, atlas_y), alternative);
	Dictionary res;
	res["x"] = x;
	res["y"] = y;
	res["source_id"] = source_id;
	Array atlas_coords;
	atlas_coords.push_back(atlas_x);
	atlas_coords.push_back(atlas_y);
	res["atlas_coords"] = atlas_coords;
	res["node_class"] = resolved.target.node_class;
	_commit_if_file(resolved);
	return justamcp_ok(res);
}

Dictionary JustAMCPTileMapTools::tilemap_fill_rect(const Dictionary &p_params) {
	int x1 = 0;
	int y1 = 0;
	int x2 = 0;
	int y2 = 0;
	String rect_err;
	if (!justamcp_fill_rect_from_args(p_params, x1, y1, x2, y2, rect_err)) {
		return MCP_INVALID_PARAMS(rect_err);
	}
	const int64_t cell_count = int64_t(x2 - x1 + 1) * int64_t(y2 - y1 + 1);
	if (cell_count > 10000) {
		return MCP_INVALID_PARAMS("Tile operation exceeds the 10000 cell cap.");
	}
	Vector<Vector2i> cells;
	for (int cx = x1; cx <= x2; cx++) {
		for (int cy = y1; cy <= y2; cy++) {
			cells.push_back(Vector2i(cx, cy));
		}
	}
	Dictionary painted = _paint_cells(p_params, cells, "fill_rect");
	if (bool(painted.get("ok", false))) {
		painted["filled"] = painted.get("written", cells.size());
		Array rect;
		rect.push_back(x1);
		rect.push_back(y1);
		rect.push_back(x2);
		rect.push_back(y2);
		painted["rect"] = rect;
	}
	return painted;
}

Dictionary JustAMCPTileMapTools::tilemap_get_cell(const Dictionary &p_params) {
	ResolvedTiles resolved = _resolve(p_params);
	if (!resolved.ok()) {
		return resolved.error;
	}
	const int x = int(p_params.get("x", 0));
	const int y = int(p_params.get("y", 0));
	const Vector2i coords(x, y);
	const int source_id = justamcp_tile_get_source_id(resolved.target, coords);
	const Vector2i atlas_coords = justamcp_tile_get_atlas(resolved.target, coords);
	Dictionary res;
	res["x"] = x;
	res["y"] = y;
	res["source_id"] = source_id;
	Array ac;
	ac.push_back(atlas_coords.x);
	ac.push_back(atlas_coords.y);
	res["atlas_coords"] = ac;
	res["alternative"] = justamcp_tile_get_alternative(resolved.target, coords);
	res["empty"] = (source_id == -1);
	res["node_class"] = resolved.target.node_class;
	if (resolved.owned_root) {
		memdelete(resolved.owned_root);
	}
	return justamcp_ok(res);
}

Dictionary JustAMCPTileMapTools::tilemap_clear(const Dictionary &p_params) {
	ResolvedTiles resolved = _resolve(p_params);
	if (!resolved.ok()) {
		return resolved.error;
	}
	justamcp_tile_clear(resolved.target);
	Dictionary res;
	res["cleared"] = true;
	res["node_class"] = resolved.target.node_class;
	_commit_if_file(resolved);
	return justamcp_ok(res);
}

Dictionary JustAMCPTileMapTools::tilemap_get_info(const Dictionary &p_params) {
	ResolvedTiles resolved = _resolve(p_params);
	if (!resolved.ok()) {
		return resolved.error;
	}
	Ref<TileSet> tile_set = justamcp_tile_get_tileset(resolved.target);
	Array sources;
	int physics_layers = 0;
	int collision_layer = 0;
	int collision_mask = 0;
	if (tile_set.is_valid()) {
		physics_layers = tile_set->get_physics_layers_count();
		if (physics_layers > 0) {
			collision_layer = tile_set->get_physics_layer_collision_layer(0);
			collision_mask = tile_set->get_physics_layer_collision_mask(0);
		}
		for (int i = 0; i < tile_set->get_source_count(); i++) {
			const int source_id = tile_set->get_source_id(i);
			Ref<TileSetSource> source = tile_set->get_source(source_id);
			if (source.is_null()) {
				continue;
			}
			Dictionary info;
			info["id"] = source_id;
			info["type"] = source->get_class();
			Ref<TileSetAtlasSource> atlas = source;
			if (atlas.is_valid()) {
				info["texture"] = atlas->get_texture().is_valid() ? atlas->get_texture()->get_path() : String();
				info["tile_count"] = atlas->get_tiles_count();
				if (atlas->get_texture().is_valid()) {
					info["texture_width"] = atlas->get_texture()->get_width();
					info["texture_height"] = atlas->get_texture()->get_height();
				}
				info["separation_x"] = atlas->get_separation().x;
				info["separation_y"] = atlas->get_separation().y;
				info["tile_size_x"] = atlas->get_texture_region_size().x;
				info["tile_size_y"] = atlas->get_texture_region_size().y;
			}
			sources.push_back(info);
		}
	}

	TypedArray<Vector2i> used = justamcp_tile_get_used_cells(resolved.target);
	Vector<Vector2i> cells;
	Vector<int> source_ids;
	cells.resize(used.size());
	source_ids.resize(used.size());
	for (int i = 0; i < used.size(); i++) {
		cells.write[i] = used[i];
		source_ids.write[i] = justamcp_tile_get_source_id(resolved.target, used[i]);
	}
	const Rect2i used_rect = justamcp_used_rect_from_cells(cells);
	const Vector2i tile_sz = tile_set.is_valid() ? tile_set->get_tile_size() : Vector2i();

	Dictionary res;
	res["node_path"] = p_params.get("node_path", p_params.get("tilemap_node", ""));
	res["used_cells"] = used.size();
	res["total_cells"] = used.size();
	res["tile_set_sources"] = sources;
	res["physics_layers"] = physics_layers;
	res["physics_layers_count"] = physics_layers;
	res["collision_layer"] = collision_layer;
	res["collision_mask"] = collision_mask;
	res["node_class"] = resolved.target.node_class;
	Array tile_size;
	tile_size.push_back(tile_sz.x);
	tile_size.push_back(tile_sz.y);
	res["tile_size"] = tile_size;
	Array used_rect_arr;
	used_rect_arr.push_back(used_rect.position.x);
	used_rect_arr.push_back(used_rect.position.y);
	used_rect_arr.push_back(used_rect.size.x);
	used_rect_arr.push_back(used_rect.size.y);
	res["used_rect"] = used_rect_arr;
	Array pixel_bounds;
	pixel_bounds.push_back(used_rect.position.x * tile_sz.x);
	pixel_bounds.push_back(used_rect.position.y * tile_sz.y);
	pixel_bounds.push_back(used_rect.size.x * tile_sz.x);
	pixel_bounds.push_back(used_rect.size.y * tile_sz.y);
	res["pixel_bounds"] = pixel_bounds;
	if (tile_set.is_null()) {
		res["message"] = "No TileSet assigned.";
	}

	const bool tileset_only = bool(p_params.get("tileset_only", false));
	if (!tileset_only) {
		if (bool(p_params.get("ascii", false))) {
			res["ascii"] = justamcp_tile_ascii_grid(cells, source_ids);
		}
		if (bool(p_params.get("include_cells", false))) {
			Array cell_items;
			Dictionary region = p_params.get("region", Dictionary());
			const bool has_region = !region.is_empty();
			const int min_x = int(region.get("min_x", region.get("x", used_rect.position.x)));
			const int min_y = int(region.get("min_y", region.get("y", used_rect.position.y)));
			const int max_x = int(region.get("max_x", min_x + used_rect.size.x - 1));
			const int max_y = int(region.get("max_y", min_y + used_rect.size.y - 1));
			const int cap = MIN(100, used.size());
			for (int i = 0; i < used.size() && cell_items.size() < cap; i++) {
				const Vector2i pos = cells[i];
				if (has_region && (pos.x < min_x || pos.x > max_x || pos.y < min_y || pos.y > max_y)) {
					continue;
				}
				Dictionary cell;
				cell["x"] = pos.x;
				cell["y"] = pos.y;
				cell["source_id"] = source_ids[i];
				cell_items.push_back(cell);
			}
			res["cells"] = cell_items;
		}
	}

	if (resolved.owned_root) {
		memdelete(resolved.owned_root);
	}
	return justamcp_ok(res);
}

Dictionary JustAMCPTileMapTools::tilemap_get_used_cells(const Dictionary &p_params) {
	ResolvedTiles resolved = _resolve(p_params);
	if (!resolved.ok()) {
		return resolved.error;
	}
	const int max_count = CLAMP(int(p_params.get("max_count", 500)), 1, 5000);
	TypedArray<Vector2i> used = justamcp_tile_get_used_cells(resolved.target);
	Array cells;
	const int iter_max = MIN((int)used.size(), max_count);
	for (int i = 0; i < iter_max; i++) {
		const Vector2i pos = used[i];
		Dictionary cell;
		cell["x"] = pos.x;
		cell["y"] = pos.y;
		cell["source_id"] = justamcp_tile_get_source_id(resolved.target, pos);
		cells.push_back(cell);
	}
	Dictionary res;
	res["cells"] = cells;
	res["total"] = used.size();
	res["returned"] = cells.size();
	res["node_class"] = resolved.target.node_class;
	if (resolved.owned_root) {
		memdelete(resolved.owned_root);
	}
	return justamcp_ok(res);
}

Dictionary JustAMCPTileMapTools::tilemap_draw_h_line(const Dictionary &p_params) {
	const int length = int(p_params.get("length", 0));
	if (length <= 0) {
		return MCP_INVALID_PARAMS("Length must be greater than 0");
	}
	return _paint_cells(p_params, justamcp_horizontal_line_cells(int(p_params.get("x", 0)), int(p_params.get("y", 0)), length), "horizontal_line");
}

Dictionary JustAMCPTileMapTools::tilemap_draw_v_line(const Dictionary &p_params) {
	const int length = int(p_params.get("length", 0));
	if (length <= 0) {
		return MCP_INVALID_PARAMS("Length must be greater than 0");
	}
	return _paint_cells(p_params, justamcp_vertical_line_cells(int(p_params.get("x", 0)), int(p_params.get("y", 0)), length), "vertical_line");
}

Dictionary JustAMCPTileMapTools::tilemap_draw_stairs(const Dictionary &p_params) {
	const int length = int(p_params.get("length", 0));
	if (length <= 0) {
		return MCP_INVALID_PARAMS("Length must be greater than 0");
	}
	const String direction = String(p_params.get("direction", "up")).to_lower();
	if (direction != "up" && direction != "down") {
		return MCP_INVALID_PARAMS("Direction must be 'up' or 'down'");
	}
	return _paint_cells(p_params, justamcp_stairs_cells(int(p_params.get("x", 0)), int(p_params.get("y", 0)), length, direction), "stairs");
}

Dictionary JustAMCPTileMapTools::tilemap_erase_rect(const Dictionary &p_params) {
	int x1 = 0;
	int y1 = 0;
	int x2 = 0;
	int y2 = 0;
	String rect_err;
	if (!justamcp_fill_rect_from_args(p_params, x1, y1, x2, y2, rect_err)) {
		return MCP_INVALID_PARAMS(rect_err);
	}
	const int64_t cell_count = int64_t(x2 - x1 + 1) * int64_t(y2 - y1 + 1);
	if (cell_count > 10000) {
		return MCP_INVALID_PARAMS("Tile operation exceeds the 10000 cell cap.");
	}
	ResolvedTiles resolved = _resolve(p_params);
	if (!resolved.ok()) {
		return resolved.error;
	}
	Vector<Vector2i> cells;
	for (int cx = x1; cx <= x2; cx++) {
		for (int cy = y1; cy <= y2; cy++) {
			cells.push_back(Vector2i(cx, cy));
		}
	}
	const int count = justamcp_tile_erase_cells(resolved.target, cells);
	Dictionary res;
	res["erased"] = count;
	res["node_class"] = resolved.target.node_class;
	_commit_if_file(resolved);
	return justamcp_ok(res);
}

Dictionary JustAMCPTileMapTools::tilemap_configure_atlas(const Dictionary &p_params) {
	ResolvedTiles resolved = _resolve(p_params);
	if (!resolved.ok()) {
		return resolved.error;
	}
	const String texture_path = justamcp_resolve_project_path(p_params.get("texture_path", ""));
	if (texture_path.is_empty()) {
		if (resolved.owned_root) {
			memdelete(resolved.owned_root);
		}
		return MCP_INVALID_PARAMS("texture_path is required");
	}
	Ref<Texture2D> tex = ResourceLoader::load(texture_path);
	if (tex.is_null()) {
		if (resolved.owned_root) {
			memdelete(resolved.owned_root);
		}
		return MCP_ERROR(-32000, "Failed to load texture: " + texture_path);
	}

	Ref<TileSet> tileset = justamcp_tile_get_tileset(resolved.target);
	if (tileset.is_null()) {
		tileset.instantiate();
		justamcp_tile_set_tileset(resolved.target, tileset);
	}

	const int tile_w = int(p_params.get("tile_size_x", 16));
	const int tile_h = int(p_params.get("tile_size_y", 16));
	const int sep_x = int(p_params.get("separation_x", 0));
	const int sep_y = int(p_params.get("separation_y", 0));
	const int source_id = int(p_params.get("source_id", 0));

	Ref<TileSetAtlasSource> atlas;
	if (tileset->has_source(source_id)) {
		atlas = tileset->get_source(source_id);
	}
	if (atlas.is_null()) {
		atlas.instantiate();
		if (tileset->has_source(source_id)) {
			tileset->remove_source(source_id);
		}
		tileset->add_source(atlas, source_id);
	}
	atlas->set_texture(tex);
	atlas->set_texture_region_size(Vector2i(tile_w, tile_h));
	atlas->set_separation(Vector2i(sep_x, sep_y));
	tileset->set_tile_size(Vector2i(tile_w, tile_h));

	if (p_params.has("physics_collision_layer") || p_params.has("physics_collision_mask") || bool(p_params.get("add_collision_shapes", false))) {
		if (tileset->get_physics_layers_count() == 0) {
			tileset->add_physics_layer();
		}
		if (p_params.has("physics_collision_layer")) {
			tileset->set_physics_layer_collision_layer(0, int(p_params.get("physics_collision_layer", 1)));
		}
		if (p_params.has("physics_collision_mask")) {
			tileset->set_physics_layer_collision_mask(0, int(p_params.get("physics_collision_mask", 1)));
		}
	}

	const int cols = tile_w > 0 ? tex->get_width() / (tile_w + MAX(sep_x, 0)) : 0;
	const int rows = tile_h > 0 ? tex->get_height() / (tile_h + MAX(sep_y, 0)) : 0;
	if (int64_t(cols) * int64_t(rows) > 10000) {
		if (resolved.owned_root) {
			memdelete(resolved.owned_root);
		}
		return MCP_INVALID_PARAMS("Atlas grid exceeds the 10000 tile cap.");
	}
	int created = 0;
	const bool add_shapes = bool(p_params.get("add_collision_shapes", false));
	for (int y = 0; y < rows; y++) {
		for (int x = 0; x < cols; x++) {
			const Vector2i coords(x, y);
			if (!atlas->has_tile(coords)) {
				atlas->create_tile(coords);
				created++;
			}
			if (add_shapes) {
				TileData *data = atlas->get_tile_data(coords, 0);
				if (data && data->get_collision_polygons_count(0) == 0) {
					data->add_collision_polygon(0);
					Vector<Vector2> polygon;
					polygon.push_back(Vector2(-tile_w / 2.0, -tile_h / 2.0));
					polygon.push_back(Vector2(tile_w / 2.0, -tile_h / 2.0));
					polygon.push_back(Vector2(tile_w / 2.0, tile_h / 2.0));
					polygon.push_back(Vector2(-tile_w / 2.0, tile_h / 2.0));
					data->set_collision_polygon_points(0, 0, polygon);
				}
			}
		}
	}

	Dictionary res;
	res["source_id"] = source_id;
	res["texture"] = texture_path;
	res["tiles_created"] = created;
	res["columns"] = cols;
	res["rows"] = rows;
	res["node_class"] = resolved.target.node_class;
	_commit_if_file(resolved);
	return justamcp_ok(res);
}

Dictionary JustAMCPTileMapTools::validate_tilemap_structure(const Dictionary &p_params) {
	ResolvedTiles resolved = _resolve(p_params);
	if (!resolved.ok()) {
		return resolved.error;
	}
	Dictionary checks = p_params.get("checks", Dictionary());
	if (checks.is_empty() && p_params.has("tile_count")) {
		checks["tile_count"] = p_params["tile_count"];
	}
	TypedArray<Vector2i> used = justamcp_tile_get_used_cells(resolved.target);
	Vector<Vector2i> cells;
	cells.resize(used.size());
	for (int i = 0; i < used.size(); i++) {
		cells.write[i] = used[i];
	}

	Array results;
	bool all_passed = true;
	auto add_check = [&](const String &p_name, bool p_passed, const Variant &p_expected, const Variant &p_actual) {
		Dictionary item;
		item["name"] = p_name;
		item["passed"] = p_passed;
		item["expected"] = p_expected;
		item["actual"] = p_actual;
		results.push_back(item);
		if (!p_passed) {
			all_passed = false;
		}
	};

	if (checks.has("tile_count")) {
		add_check("tile_count", used.size() == int(checks["tile_count"]), checks["tile_count"], used.size());
	}
	if (checks.has("tile_count_min")) {
		add_check("tile_count_min", used.size() >= int(checks["tile_count_min"]), checks["tile_count_min"], used.size());
	}
	if (bool(checks.get("has_continuous_horizontal", false))) {
		add_check("has_continuous_horizontal", justamcp_tile_structure_continuous(cells, true), true, justamcp_tile_structure_continuous(cells, true));
	}
	if (bool(checks.get("has_continuous_vertical", false))) {
		add_check("has_continuous_vertical", justamcp_tile_structure_continuous(cells, false), true, justamcp_tile_structure_continuous(cells, false));
	}
	if (checks.has("bounds") && checks["bounds"].get_type() == Variant::DICTIONARY) {
		Dictionary bounds = checks["bounds"];
		const int min_x = int(bounds.get("min_x", bounds.get("x", 0)));
		const int min_y = int(bounds.get("min_y", bounds.get("y", 0)));
		const int max_x = int(bounds.get("max_x", min_x));
		const int max_y = int(bounds.get("max_y", min_y));
		bool in_bounds = true;
		for (int i = 0; i < cells.size(); i++) {
			if (cells[i].x < min_x || cells[i].x > max_x || cells[i].y < min_y || cells[i].y > max_y) {
				in_bounds = false;
				break;
			}
		}
		add_check("bounds", in_bounds, bounds, used.size());
	}

	Dictionary res;
	res["all_checks_passed"] = all_passed;
	res["check_results"] = results;
	res["tile_count"] = used.size();
	res["node_class"] = resolved.target.node_class;
	if (resolved.owned_root) {
		memdelete(resolved.owned_root);
	}
	return justamcp_ok(res);
}

#endif
