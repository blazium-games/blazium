/**************************************************************************/
/*  justamcp_tilemap_access.h                                             */
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

#pragma once

#include "core/math/vector2i.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/dictionary.h"
#include "scene/resources/2d/tile_set.h"

class Node;
class TileMap;
class TileMapLayer;

struct JustAMCPTileTarget {
	Node *node = nullptr;
	TileMapLayer *layer = nullptr;
	TileMap *map = nullptr;
	int layer_index = 0;
	String node_class;
	bool valid() const { return layer != nullptr || map != nullptr; }
};

JustAMCPTileTarget justamcp_tile_target_from_node(Node *p_node, int p_layer_index = 0);
void justamcp_tile_set_cell(const JustAMCPTileTarget &p_target, const Vector2i &p_coords, int p_source_id, const Vector2i &p_atlas, int p_alternative);
void justamcp_tile_erase_cell(const JustAMCPTileTarget &p_target, const Vector2i &p_coords);
void justamcp_tile_clear(const JustAMCPTileTarget &p_target);
int justamcp_tile_get_source_id(const JustAMCPTileTarget &p_target, const Vector2i &p_coords);
Vector2i justamcp_tile_get_atlas(const JustAMCPTileTarget &p_target, const Vector2i &p_coords);
int justamcp_tile_get_alternative(const JustAMCPTileTarget &p_target, const Vector2i &p_coords);
TypedArray<Vector2i> justamcp_tile_get_used_cells(const JustAMCPTileTarget &p_target);
Ref<TileSet> justamcp_tile_get_tileset(const JustAMCPTileTarget &p_target);
void justamcp_tile_set_tileset(const JustAMCPTileTarget &p_target, const Ref<TileSet> &p_tileset);
int justamcp_tile_set_cells(const JustAMCPTileTarget &p_target, const Vector<Vector2i> &p_cells, int p_source_id, const Vector2i &p_atlas, int p_alternative);
int justamcp_tile_erase_cells(const JustAMCPTileTarget &p_target, const Vector<Vector2i> &p_cells);
