/**************************************************************************/
/*  justamcp_tilemap_access.cpp                                           */
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

#include "justamcp_tilemap_access.h"

#ifdef TOOLS_ENABLED
#include "justamcp_agent_policy.h"

#include "editor/editor_undo_redo_manager.h"
#endif
#include "scene/main/node.h"

#include "modules/tilemap/tile_map.h"
#include "modules/tilemap/tile_map_layer.h"

struct JustAMCPTileCell {
	Vector2i coords;
	int source_id = -1;
	Vector2i atlas = Vector2i(-1, -1);
	int alternative = 0;
};

static void _raw_set_cell(const JustAMCPTileTarget &p_target, const JustAMCPTileCell &p_cell) {
	if (p_target.layer) {
		p_target.layer->set_cell(p_cell.coords, p_cell.source_id, p_cell.atlas, p_cell.alternative);
	} else if (p_target.map) {
		p_target.map->set_cell(p_target.layer_index, p_cell.coords, p_cell.source_id, p_cell.atlas, p_cell.alternative);
	}
}

static JustAMCPTileCell _capture_cell(const JustAMCPTileTarget &p_target, const Vector2i &p_coords) {
	JustAMCPTileCell cell;
	cell.coords = p_coords;
	cell.source_id = justamcp_tile_get_source_id(p_target, p_coords);
	cell.atlas = justamcp_tile_get_atlas(p_target, p_coords);
	cell.alternative = justamcp_tile_get_alternative(p_target, p_coords);
	return cell;
}

static void _commit_tile_edit(const JustAMCPTileTarget &p_target, const Vector<JustAMCPTileCell> &p_previous, const Vector<JustAMCPTileCell> &p_next) {
	if (p_previous.size() != p_next.size() || p_previous.is_empty()) {
		return;
	}
	Object *object = p_target.layer ? static_cast<Object *>(p_target.layer) : static_cast<Object *>(p_target.map);
	if (!object) {
		return;
	}
#ifdef TOOLS_ENABLED
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	if (undo_redo) {
		undo_redo->create_action("AI Local: Set Tiles [" + JustAMCPAgentPolicy::current_session_id() + "]", UndoRedo::MERGE_DISABLE);
		for (int i = 0; i < p_next.size(); i++) {
			if (p_target.layer) {
				undo_redo->add_do_method(p_target.layer, "set_cell", p_next[i].coords, p_next[i].source_id, p_next[i].atlas, p_next[i].alternative);
				undo_redo->add_undo_method(p_target.layer, "set_cell", p_previous[i].coords, p_previous[i].source_id, p_previous[i].atlas, p_previous[i].alternative);
			} else {
				undo_redo->add_do_method(p_target.map, "set_cell", p_target.layer_index, p_next[i].coords, p_next[i].source_id, p_next[i].atlas, p_next[i].alternative);
				undo_redo->add_undo_method(p_target.map, "set_cell", p_target.layer_index, p_previous[i].coords, p_previous[i].source_id, p_previous[i].atlas, p_previous[i].alternative);
			}
		}
		undo_redo->commit_action(true);
		return;
	}
	Array restores;
	for (int i = 0; i < p_previous.size(); i++) {
		_raw_set_cell(p_target, p_next[i]);
		Dictionary row;
		row["id"] = int64_t(object->get_instance_id());
		row["layer_node"] = p_target.layer != nullptr;
		row["layer_index"] = p_target.layer_index;
		row["x"] = p_previous[i].coords.x;
		row["y"] = p_previous[i].coords.y;
		row["source_id"] = p_previous[i].source_id;
		row["atlas_x"] = p_previous[i].atlas.x;
		row["atlas_y"] = p_previous[i].atlas.y;
		row["alternative"] = p_previous[i].alternative;
		restores.push_back(row);
	}
	JustAMCPAgentPolicy::note_tile_undo(restores);
#else
	for (int i = 0; i < p_next.size(); i++) {
		_raw_set_cell(p_target, p_next[i]);
	}
#endif
}

JustAMCPTileTarget justamcp_tile_target_from_node(Node *p_node, int p_layer_index) {
	JustAMCPTileTarget target;
	target.node = p_node;
	target.layer_index = p_layer_index;
	if (!p_node) {
		return target;
	}
	target.layer = Object::cast_to<TileMapLayer>(p_node);
	if (target.layer) {
		target.node_class = "TileMapLayer";
		return target;
	}
	target.map = Object::cast_to<TileMap>(p_node);
	if (target.map) {
		target.node_class = "TileMap";
	}
	return target;
}

void justamcp_tile_set_cell(const JustAMCPTileTarget &p_target, const Vector2i &p_coords, int p_source_id, const Vector2i &p_atlas, int p_alternative) {
	if (!p_target.valid()) {
		return;
	}
	Vector<JustAMCPTileCell> previous;
	previous.push_back(_capture_cell(p_target, p_coords));
	JustAMCPTileCell next;
	next.coords = p_coords;
	next.source_id = p_source_id;
	next.atlas = p_atlas;
	next.alternative = p_alternative;
	Vector<JustAMCPTileCell> next_cells;
	next_cells.push_back(next);
	_commit_tile_edit(p_target, previous, next_cells);
}

void justamcp_tile_erase_cell(const JustAMCPTileTarget &p_target, const Vector2i &p_coords) {
	justamcp_tile_set_cell(p_target, p_coords, -1, Vector2i(-1, -1), 0);
}

void justamcp_tile_clear(const JustAMCPTileTarget &p_target) {
	if (!p_target.valid()) {
		return;
	}
	const TypedArray<Vector2i> used = justamcp_tile_get_used_cells(p_target);
	Vector<JustAMCPTileCell> previous;
	Vector<JustAMCPTileCell> next_cells;
	for (int i = 0; i < used.size(); i++) {
		const Vector2i coords = used[i];
		previous.push_back(_capture_cell(p_target, coords));
		JustAMCPTileCell next;
		next.coords = coords;
		next_cells.push_back(next);
	}
	_commit_tile_edit(p_target, previous, next_cells);
}

int justamcp_tile_get_source_id(const JustAMCPTileTarget &p_target, const Vector2i &p_coords) {
	if (p_target.layer) {
		return p_target.layer->get_cell_source_id(p_coords);
	}
	if (p_target.map) {
		return p_target.map->get_cell_source_id(p_target.layer_index, p_coords);
	}
	return -1;
}

Vector2i justamcp_tile_get_atlas(const JustAMCPTileTarget &p_target, const Vector2i &p_coords) {
	if (p_target.layer) {
		return p_target.layer->get_cell_atlas_coords(p_coords);
	}
	if (p_target.map) {
		return p_target.map->get_cell_atlas_coords(p_target.layer_index, p_coords);
	}
	return Vector2i(-1, -1);
}

int justamcp_tile_get_alternative(const JustAMCPTileTarget &p_target, const Vector2i &p_coords) {
	if (p_target.layer) {
		return p_target.layer->get_cell_alternative_tile(p_coords);
	}
	if (p_target.map) {
		return p_target.map->get_cell_alternative_tile(p_target.layer_index, p_coords);
	}
	return 0;
}

TypedArray<Vector2i> justamcp_tile_get_used_cells(const JustAMCPTileTarget &p_target) {
	if (p_target.layer) {
		return p_target.layer->get_used_cells();
	}
	if (p_target.map) {
		return p_target.map->get_used_cells(p_target.layer_index);
	}
	return TypedArray<Vector2i>();
}

Ref<TileSet> justamcp_tile_get_tileset(const JustAMCPTileTarget &p_target) {
	if (p_target.layer) {
		return p_target.layer->get_tile_set();
	}
	if (p_target.map) {
		return p_target.map->get_tileset();
	}
	return Ref<TileSet>();
}

void justamcp_tile_set_tileset(const JustAMCPTileTarget &p_target, const Ref<TileSet> &p_tileset) {
	if (p_target.layer) {
		p_target.layer->set_tile_set(p_tileset);
	} else if (p_target.map) {
		p_target.map->set_tileset(p_tileset);
	}
}

int justamcp_tile_set_cells(const JustAMCPTileTarget &p_target, const Vector<Vector2i> &p_cells, int p_source_id, const Vector2i &p_atlas, int p_alternative) {
	Vector<JustAMCPTileCell> previous;
	Vector<JustAMCPTileCell> next_cells;
	for (int i = 0; i < p_cells.size(); i++) {
		previous.push_back(_capture_cell(p_target, p_cells[i]));
		JustAMCPTileCell next;
		next.coords = p_cells[i];
		next.source_id = p_source_id;
		next.atlas = p_atlas;
		next.alternative = p_alternative;
		next_cells.push_back(next);
	}
	_commit_tile_edit(p_target, previous, next_cells);
	return p_cells.size();
}

int justamcp_tile_erase_cells(const JustAMCPTileTarget &p_target, const Vector<Vector2i> &p_cells) {
	Vector<JustAMCPTileCell> previous;
	Vector<JustAMCPTileCell> next_cells;
	for (int i = 0; i < p_cells.size(); i++) {
		previous.push_back(_capture_cell(p_target, p_cells[i]));
		JustAMCPTileCell next;
		next.coords = p_cells[i];
		next_cells.push_back(next);
	}
	_commit_tile_edit(p_target, previous, next_cells);
	return p_cells.size();
}
