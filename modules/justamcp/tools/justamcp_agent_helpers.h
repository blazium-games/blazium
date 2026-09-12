/**************************************************************************/
/*  justamcp_agent_helpers.h                                              */
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

#include "core/io/resource_uid.h"
#include "core/math/rect2i.h"
#include "core/math/vector2i.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/pair.h"
#include "core/templates/vector.h"
#include "core/typedefs.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

inline bool justamcp_path_is_sandboxed(const String &p_path) {
	const String p = p_path.strip_edges().replace("\\", "/");
	return p.begins_with("res://") || p.begins_with("user://") || p.begins_with("uid://");
}

inline bool justamcp_canonical_sandbox_path(const String &p_path, String &r_path, String &r_error) {
	String p = p_path.strip_edges().replace("\\", "/");
	if (p.is_empty()) {
		r_error = "path is required.";
		return false;
	}
	if ((p.length() >= 2 && p[1] == ':' && ((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z'))) ||
			p.begins_with("//") || p.begins_with("\\\\")) {
		r_error = "Path must stay under res:// or user://.";
		return false;
	}
	if (p.begins_with("uid://")) {
		ResourceUID *uid_api = ResourceUID::get_singleton();
		if (!uid_api) {
			r_error = "ResourceUID singleton unavailable.";
			return false;
		}
		const ResourceUID::ID uid = uid_api->text_to_id(p);
		if (uid == ResourceUID::INVALID_ID || !uid_api->has_id(uid)) {
			r_error = "Unknown UID: " + p;
			return false;
		}
		p = uid_api->get_id_path(uid).replace("\\", "/");
	}
	if (!p.begins_with("res://") && !p.begins_with("user://")) {
		if (p.begins_with("/")) {
			p = p.substr(1);
		}
		p = "res://" + p;
	}
	const String prefix = p.begins_with("user://") ? String("user://") : String("res://");
	const String rest = p.substr(prefix.length());
	Vector<String> out;
	const Vector<String> parts = rest.split("/");
	for (int i = 0; i < parts.size(); i++) {
		const String part = parts[i];
		if (part.is_empty() || part == ".") {
			continue;
		}
		if (part == "..") {
			if (out.is_empty()) {
				r_error = "Path escapes the project sandbox: " + p_path;
				return false;
			}
			out.remove_at(out.size() - 1);
			continue;
		}
		out.push_back(part);
	}
	r_path = prefix;
	for (int i = 0; i < out.size(); i++) {
		if (i > 0) {
			r_path += "/";
		}
		r_path += out[i];
	}
	if (!(r_path.begins_with("res://") || r_path.begins_with("user://"))) {
		r_error = "Path must stay under res:// or user://.";
		return false;
	}
	return true;
}

inline String justamcp_ensure_res_path(const String &p_path) {
	String canon;
	String err;
	if (!justamcp_canonical_sandbox_path(p_path, canon, err)) {
		return String();
	}
	return canon;
}

inline bool justamcp_fill_rect_from_args(const Dictionary &p_args, int &r_x1, int &r_y1, int &r_x2, int &r_y2, String &r_error) {
	if (p_args.has("width") || p_args.has("height")) {
		const int x = int(p_args.get("x", p_args.get("x1", 0)));
		const int y = int(p_args.get("y", p_args.get("y1", 0)));
		const int width = int(p_args.get("width", 0));
		const int height = int(p_args.get("height", 0));
		if (width <= 0 || height <= 0) {
			r_error = "width/height must be greater than 0";
			return false;
		}
		r_x1 = x;
		r_y1 = y;
		r_x2 = x + width - 1;
		r_y2 = y + height - 1;
		return true;
	}

	r_x1 = int(p_args.get("x1", p_args.get("x", 0)));
	r_y1 = int(p_args.get("y1", p_args.get("y", 0)));
	r_x2 = int(p_args.get("x2", r_x1));
	r_y2 = int(p_args.get("y2", r_y1));
	if (r_x2 < r_x1) {
		SWAP(r_x1, r_x2);
	}
	if (r_y2 < r_y1) {
		SWAP(r_y1, r_y2);
	}
	return true;
}

inline Vector<Vector2i> justamcp_horizontal_line_cells(int p_x, int p_y, int p_length) {
	Vector<Vector2i> cells;
	if (p_length <= 0) {
		return cells;
	}
	cells.resize(p_length);
	for (int i = 0; i < p_length; i++) {
		cells.write[i] = Vector2i(p_x + i, p_y);
	}
	return cells;
}

inline Vector<Vector2i> justamcp_vertical_line_cells(int p_x, int p_y, int p_length) {
	Vector<Vector2i> cells;
	if (p_length <= 0) {
		return cells;
	}
	cells.resize(p_length);
	for (int i = 0; i < p_length; i++) {
		cells.write[i] = Vector2i(p_x, p_y + i);
	}
	return cells;
}

inline Vector<Vector2i> justamcp_stairs_cells(int p_x, int p_y, int p_length, const String &p_direction) {
	Vector<Vector2i> cells;
	if (p_length <= 0) {
		return cells;
	}
	const String dir = p_direction.to_lower();
	const int y_step = (dir == "up") ? -1 : 1;
	cells.resize(p_length);
	for (int i = 0; i < p_length; i++) {
		cells.write[i] = Vector2i(p_x + i, p_y + (i * y_step));
	}
	return cells;
}

inline int justamcp_atlas_grid_count(int p_tex_w, int p_tex_h, int p_tile_w, int p_tile_h, int p_sep_x, int p_sep_y) {
	if (p_tex_w <= 0 || p_tex_h <= 0 || p_tile_w <= 0 || p_tile_h <= 0) {
		return 0;
	}
	const int stride_x = p_tile_w + MAX(p_sep_x, 0);
	const int stride_y = p_tile_h + MAX(p_sep_y, 0);
	const int cols = p_tex_w / stride_x;
	const int rows = p_tex_h / stride_y;
	return MAX(cols, 0) * MAX(rows, 0);
}

inline int justamcp_playtest_required_duration_ms(const Array &p_inputs) {
	int needed = 0;
	for (int i = 0; i < p_inputs.size(); i++) {
		if (p_inputs[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		const Dictionary input = p_inputs[i];
		const int at_ms = int(input.get("at_ms", 0));
		const int hold_ms = int(input.get("hold_ms", 0));
		needed = MAX(needed, at_ms + hold_ms);
	}
	return needed;
}

inline bool justamcp_glob_match(const String &p_text, const String &p_pattern) {
	if (p_pattern.is_empty() || p_pattern == "*") {
		return true;
	}
	return p_text.matchn(p_pattern);
}

inline bool justamcp_tile_structure_continuous(const Vector<Vector2i> &p_cells, bool p_horizontal) {
	if (p_cells.size() <= 1) {
		return true;
	}
	HashMap<int, Vector<int>> lanes;
	for (int i = 0; i < p_cells.size(); i++) {
		const Vector2i c = p_cells[i];
		if (p_horizontal) {
			lanes[c.y].push_back(c.x);
		} else {
			lanes[c.x].push_back(c.y);
		}
	}
	for (const KeyValue<int, Vector<int>> &E : lanes) {
		Vector<int> vals = E.value;
		vals.sort();
		for (int i = 1; i < vals.size(); i++) {
			if (vals[i] != vals[i - 1] + 1 && vals[i] != vals[i - 1]) {
				return false;
			}
		}
	}
	return true;
}

inline Dictionary justamcp_ok(const Dictionary &p_fields) {
	Dictionary r = p_fields.duplicate();
	r["ok"] = true;
	return r;
}

inline String justamcp_remap_tool_name(const String &p_name) {
	if (p_name == "run_scene") {
		return "editor_run_scene";
	}
	if (p_name == "play_scene") {
		return "editor_play_scene";
	}
	if (p_name == "open_scene") {
		return "editor_open_scene";
	}
	if (p_name == "open_in_godot") {
		return "open_in_blazium";
	}
	if (p_name == "stop_running_scene" || p_name == "stop_play" || p_name == "stop_scene") {
		return "editor_stop_play";
	}
	if (p_name == "clear_output_logs") {
		return "editor_clear_output";
	}
	if (p_name == "get_scene_tree") {
		return "scene_tree_dump";
	}
	if (p_name == "get_screenshot") {
		return "editor_take_screenshot";
	}
	if (p_name == "read") {
		return "read_file";
	}
	if (p_name == "write" || p_name == "write_file") {
		return "create_file";
	}
	if (p_name == "mv") {
		return "move_file";
	}
	if (p_name == "cp") {
		return "copy_file";
	}
	if (p_name == "rm") {
		return "delete_file";
	}
	if (p_name == "glob") {
		return "search_files";
	}
	if (p_name == "grep" || p_name == "grep_code") {
		return "search_in_files";
	}
	if (p_name == "list_dir") {
		return "get_filesystem_tree";
	}
	if (p_name == "move_node") {
		return "reparent_node";
	}
	if (p_name == "eval") {
		return "eval_expression";
	}
	if (p_name == "fill_rectangle") {
		return "tilemap_fill_rect";
	}
	if (p_name == "draw_horizontal_line") {
		return "tilemap_draw_h_line";
	}
	if (p_name == "draw_vertical_line") {
		return "tilemap_draw_v_line";
	}
	if (p_name == "draw_stairs") {
		return "tilemap_draw_stairs";
	}
	if (p_name == "erase_rectangle") {
		return "tilemap_erase_rect";
	}
	if (p_name == "configure_tileset_atlas") {
		return "tilemap_configure_atlas";
	}
	if (p_name == "get_tilemap_layout" || p_name == "get_tilemap_state" || p_name == "get_tileset_info") {
		return "tilemap_get_info";
	}
	if (p_name == "get_input_map") {
		return "project_get_input_actions";
	}
	if (p_name == "configure_input_map") {
		return "project_set_input_action";
	}
	if (p_name == "add_resource") {
		return "node_add_resource";
	}
	if (p_name == "execute_script") {
		return "execute_gdscript_snippet";
	}
	if (p_name == "game_start") {
		return "qa_start";
	}
	if (p_name == "game_stop") {
		return "qa_stop";
	}
	if (p_name == "game_act") {
		return "qa_act";
	}
	if (p_name == "game_observe") {
		return "qa_observe";
	}
	if (p_name == "game_watch") {
		return "qa_watch";
	}
	if (p_name == "game_drive") {
		return "qa_drive";
	}
	if (p_name == "set_editor_setting") {
		return "editor_set_settings";
	}
	if (p_name == "update_project_setting") {
		return "set_project_setting";
	}
	if (p_name == "run_tests" || p_name == "runtime_run_gut_tests" || p_name == "autowork_run_all_tests") {
		return "runtime_run_autowork_tests";
	}
	if (p_name == "asset_to_uid" || p_name == "path_to_uid") {
		return "project_path_to_uid";
	}
	if (p_name == "uid_to_asset" || p_name == "uid_to_path") {
		return "uid_to_project_path";
	}
	if (p_name == "assign_uid" || p_name == "create_uid") {
		return "asset_assign_uid";
	}
	if (p_name == "update_uid" || p_name == "set_uid") {
		return "asset_update_uid";
	}
	if (p_name == "remove_uid" || p_name == "delete_uid") {
		return "asset_remove_uid";
	}
	return p_name;
}

inline Dictionary justamcp_normalize_tool_args(const Dictionary &p_args, const String &p_tool_name = String()) {
	Dictionary args = p_args.duplicate();
	if (!args.has("node_path") && args.has("tilemap_node")) {
		args["node_path"] = args["tilemap_node"];
	}
	if (!args.has("file_path") && args.has("scene_path")) {
		args["file_path"] = args["scene_path"];
	}
	if (!args.has("file_path") && args.has("path")) {
		args["file_path"] = args["path"];
	}
	if (!args.has("property") && args.has("property_path")) {
		args["property"] = args["property_path"];
	}
	if (!args.has("resource_properties") && args.has("properties") && args["properties"].get_type() == Variant::DICTIONARY) {
		args["resource_properties"] = args["properties"];
	}
	if (!args.has("code") && args.has("script")) {
		args["code"] = args["script"];
	}
	if (!args.has("screen") && args.has("screen_name")) {
		args["screen"] = args["screen_name"];
	}
	if (!args.has("screen_name") && args.has("screen")) {
		args["screen_name"] = args["screen"];
	}
	if (!args.has("path") && args.has("project_path")) {
		args["path"] = args["project_path"];
	}
	if (!args.has("path") && args.has("asset_path")) {
		args["path"] = args["asset_path"];
	}
	if (!args.has("scene_path") && args.has("scenePath")) {
		args["scene_path"] = args["scenePath"];
	}
	if (!args.has("node_path") && args.has("nodePath")) {
		args["node_path"] = args["nodePath"];
	}
	if (!args.has("player_node_path") && args.has("playerNodePath")) {
		args["player_node_path"] = args["playerNodePath"];
	}
	if (!args.has("relative_x") && args.has("dx")) {
		args["relative_x"] = args["dx"];
	}
	if (!args.has("relative_y") && args.has("dy")) {
		args["relative_y"] = args["dy"];
	}
	if (!args.has("search_text") && args.has("search")) {
		args["search_text"] = args["search"];
	}
	if (!args.has("replace_text") && args.has("replace")) {
		args["replace_text"] = args["replace"];
	}
	if (!args.has("uid") && args.has("uid_string")) {
		args["uid"] = args["uid_string"];
	}
	if (!args.has("uid") && args.has("asset_uid")) {
		args["uid"] = args["asset_uid"];
	}
	if ((p_tool_name == "move_file" || p_tool_name == "copy_file") && !args.has("from")) {
		if (args.has("file_path")) {
			args["from"] = args["file_path"];
		} else if (args.has("path")) {
			args["from"] = args["path"];
		}
	}
	if ((p_tool_name == "move_file" || p_tool_name == "copy_file") && !args.has("to")) {
		if (args.has("destination")) {
			args["to"] = args["destination"];
		}
	}
	if (p_tool_name == "runtime_run_autowork_tests" || p_tool_name.begins_with("autowork_run_")) {
		if (!args.has("path") && args.has("test_script")) {
			args["path"] = args["test_script"];
		}
		if (!args.has("path") && args.has("directory_path")) {
			args["path"] = args["directory_path"];
		}
		if (!args.has("filter") && args.has("test_name")) {
			args["filter"] = args["test_name"];
		}
	}
	return args;
}

inline void justamcp_apply_alias_query_flags(const String &p_original_name, Dictionary &r_args) {
	if (p_original_name == "get_tilemap_state") {
		r_args["ascii"] = true;
	} else if (p_original_name == "get_tileset_info") {
		r_args["tileset_only"] = true;
	} else if (p_original_name == "get_tilemap_layout" && !r_args.has("include_cells")) {
		r_args["include_cells"] = false;
	}
}

inline Rect2i justamcp_used_rect_from_cells(const Vector<Vector2i> &p_cells) {
	if (p_cells.is_empty()) {
		return Rect2i();
	}
	int min_x = p_cells[0].x;
	int min_y = p_cells[0].y;
	int max_x = p_cells[0].x;
	int max_y = p_cells[0].y;
	for (int i = 1; i < p_cells.size(); i++) {
		min_x = MIN(min_x, p_cells[i].x);
		min_y = MIN(min_y, p_cells[i].y);
		max_x = MAX(max_x, p_cells[i].x);
		max_y = MAX(max_y, p_cells[i].y);
	}
	return Rect2i(min_x, min_y, max_x - min_x + 1, max_y - min_y + 1);
}

inline String justamcp_tile_ascii_grid(const Vector<Vector2i> &p_cells, const Vector<int> &p_source_ids) {
	if (p_cells.is_empty()) {
		return String();
	}
	const Rect2i bounds = justamcp_used_rect_from_cells(p_cells);
	HashMap<Vector2i, int> sources;
	const int count = MIN(p_cells.size(), p_source_ids.size());
	for (int i = 0; i < count; i++) {
		sources[p_cells[i]] = p_source_ids[i];
	}
	String out;
	for (int y = bounds.position.y; y < bounds.position.y + bounds.size.y; y++) {
		if (!out.is_empty()) {
			out += "\n";
		}
		for (int x = bounds.position.x; x < bounds.position.x + bounds.size.x; x++) {
			const Vector2i cell(x, y);
			if (x > bounds.position.x) {
				out += " ";
			}
			if (!sources.has(cell) || sources[cell] < 0) {
				out += ".";
			} else {
				out += itos(sources[cell]);
			}
		}
	}
	return out;
}
