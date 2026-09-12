/**************************************************************************/
/*  justamcp_tilemap_tools.h                                              */
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

#ifdef TOOLS_ENABLED

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "justamcp_tilemap_access.h"
#include "scene/main/node.h"

class JustAMCPEditorPlugin;

class JustAMCPTileMapTools : public Object {
	GDCLASS(JustAMCPTileMapTools, Object);

private:
	JustAMCPEditorPlugin *editor_plugin = nullptr;

	struct ResolvedTiles {
		JustAMCPTileTarget target;
		Node *owned_root = nullptr;
		String file_path;
		Dictionary error;
		bool ok() const { return error.is_empty() && target.valid(); }
	};

	ResolvedTiles _resolve(const Dictionary &p_params);
	void _commit_if_file(ResolvedTiles &p_resolved);
	Dictionary _paint_cells(const Dictionary &p_params, const Vector<Vector2i> &p_cells, const String &p_label);

public:
	void set_editor_plugin(JustAMCPEditorPlugin *p_plugin) { editor_plugin = p_plugin; }

	Dictionary execute_tool(const String &p_tool_name, const Dictionary &p_args);

	Dictionary tilemap_set_cell(const Dictionary &p_params);
	Dictionary tilemap_fill_rect(const Dictionary &p_params);
	Dictionary tilemap_get_cell(const Dictionary &p_params);
	Dictionary tilemap_clear(const Dictionary &p_params);
	Dictionary tilemap_get_info(const Dictionary &p_params);
	Dictionary tilemap_get_used_cells(const Dictionary &p_params);
	Dictionary tilemap_draw_h_line(const Dictionary &p_params);
	Dictionary tilemap_draw_v_line(const Dictionary &p_params);
	Dictionary tilemap_draw_stairs(const Dictionary &p_params);
	Dictionary tilemap_erase_rect(const Dictionary &p_params);
	Dictionary tilemap_configure_atlas(const Dictionary &p_params);
	Dictionary validate_tilemap_structure(const Dictionary &p_params);

	JustAMCPTileMapTools();
	~JustAMCPTileMapTools();
};

#endif
