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

#include "scene/2d/tile_map.h"
#include "scene/2d/tile_map_layer.h"
#include "scene/main/node.h"

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
	if (p_target.layer) {
		p_target.layer->set_cell(p_coords, p_source_id, p_atlas, p_alternative);
	} else if (p_target.map) {
		p_target.map->set_cell(p_target.layer_index, p_coords, p_source_id, p_atlas, p_alternative);
	}
}

void justamcp_tile_erase_cell(const JustAMCPTileTarget &p_target, const Vector2i &p_coords) {
	if (p_target.layer) {
		p_target.layer->erase_cell(p_coords);
	} else if (p_target.map) {
		p_target.map->erase_cell(p_target.layer_index, p_coords);
	}
}

void justamcp_tile_clear(const JustAMCPTileTarget &p_target) {
	if (p_target.layer) {
		p_target.layer->clear();
	} else if (p_target.map) {
		p_target.map->clear_layer(p_target.layer_index);
	}
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
	int count = 0;
	for (int i = 0; i < p_cells.size(); i++) {
		justamcp_tile_set_cell(p_target, p_cells[i], p_source_id, p_atlas, p_alternative);
		count++;
	}
	return count;
}

int justamcp_tile_erase_cells(const JustAMCPTileTarget &p_target, const Vector<Vector2i> &p_cells) {
	int count = 0;
	for (int i = 0; i < p_cells.size(); i++) {
		justamcp_tile_erase_cell(p_target, p_cells[i]);
		count++;
	}
	return count;
}
