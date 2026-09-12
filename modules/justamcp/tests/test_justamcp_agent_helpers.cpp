/**************************************************************************/
/*  test_justamcp_agent_helpers.cpp                                       */
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

#ifdef TESTS_ENABLED

#include "test_justamcp_agent_helpers.h"

#include "../justamcp_mcp_tool_macros.h"
#include "../tools/justamcp_agent_helpers.h"
#include "../tools/justamcp_category_dispatch.h"
#include "../tools/justamcp_settings_resolver.h"
#include "../tools/justamcp_tool_executor.h"
#include "../tools/justamcp_tool_schema_cache.h"
#include "core/config/project_settings.h"
#include "modules/modules_enabled.gen.h"
#include "tests/test_macros.h"

#ifdef MODULE_AUTOWORK_ENABLED
#include "modules/autowork/autowork_main.h"
#endif

void test_justamcp_agent_helpers() {
	CHECK(justamcp_path_is_sandboxed("res://foo.txt"));
	CHECK(justamcp_path_is_sandboxed("user://bar.txt"));
	CHECK(!justamcp_path_is_sandboxed("C:/Windows/system32"));
	CHECK(justamcp_ensure_res_path("scenes/main.tscn") == "res://scenes/main.tscn");

	Dictionary width_args;
	width_args["x"] = 5;
	width_args["y"] = 7;
	width_args["width"] = 3;
	width_args["height"] = 2;
	int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
	String err;
	CHECK(justamcp_fill_rect_from_args(width_args, x1, y1, x2, y2, err));
	CHECK(x1 == 5);
	CHECK(y1 == 7);
	CHECK(x2 == 7);
	CHECK(y2 == 8);

	Dictionary bad;
	bad["width"] = 0;
	bad["height"] = 2;
	CHECK(!justamcp_fill_rect_from_args(bad, x1, y1, x2, y2, err));

	Vector<Vector2i> hline = justamcp_horizontal_line_cells(1, 2, 3);
	CHECK(hline.size() == 3);
	CHECK(hline[2] == Vector2i(3, 2));
	Vector<Vector2i> vline = justamcp_vertical_line_cells(1, 2, 3);
	CHECK(vline[2] == Vector2i(1, 4));
	Vector<Vector2i> stairs = justamcp_stairs_cells(0, 0, 3, "up");
	CHECK(stairs[2] == Vector2i(2, -2));

	CHECK(justamcp_atlas_grid_count(64, 32, 16, 16, 0, 0) == 8);
	CHECK(justamcp_atlas_grid_count(0, 32, 16, 16, 0, 0) == 0);

	Array inputs;
	Dictionary first;
	first["at_ms"] = 100;
	first["hold_ms"] = 50;
	inputs.push_back(first);
	Dictionary second;
	second["at_ms"] = 400;
	second["hold_ms"] = 80;
	inputs.push_back(second);
	CHECK(justamcp_playtest_required_duration_ms(inputs) == 480);

	Vector<Vector2i> continuous;
	continuous.push_back(Vector2i(0, 0));
	continuous.push_back(Vector2i(1, 0));
	continuous.push_back(Vector2i(2, 0));
	CHECK(justamcp_tile_structure_continuous(continuous, true));
	Vector<Vector2i> gapped;
	gapped.push_back(Vector2i(0, 0));
	gapped.push_back(Vector2i(2, 0));
	CHECK(!justamcp_tile_structure_continuous(gapped, true));

	CHECK(justamcp_remap_tool_name("run_scene") == "editor_run_scene");
	CHECK(justamcp_remap_tool_name("get_input_map") == "project_get_input_actions");
	CHECK(justamcp_remap_tool_name("add_resource") == "node_add_resource");
	CHECK(justamcp_remap_tool_name("get_tilemap_state") == "tilemap_get_info");
	CHECK(justamcp_remap_tool_name("read") == "read_file");
	CHECK(justamcp_remap_tool_name("execute_script") == "execute_gdscript_snippet");
	CHECK(justamcp_remap_tool_name("open_in_godot") == "open_in_blazium");
	CHECK(justamcp_remap_tool_name("game_start") == "qa_start");
	CHECK(justamcp_remap_tool_name("game_stop") == "qa_stop");
	CHECK(justamcp_remap_tool_name("set_editor_setting") == "editor_set_settings");
	CHECK(justamcp_remap_tool_name("update_project_setting") == "set_project_setting");
	CHECK(justamcp_remap_tool_name("run_tests") == "runtime_run_autowork_tests");
	CHECK(justamcp_remap_tool_name("runtime_run_gut_tests") == "runtime_run_autowork_tests");
	CHECK(justamcp_remap_tool_name("asset_to_uid") == "project_path_to_uid");
	CHECK(justamcp_remap_tool_name("uid_to_asset") == "uid_to_project_path");
	CHECK(justamcp_remap_tool_name("assign_uid") == "asset_assign_uid");
	CHECK(justamcp_remap_tool_name("play_scene") == "editor_play_scene");
	CHECK(justamcp_remap_tool_name("glob") == "search_files");
	CHECK(justamcp_remap_tool_name("write_file") == "create_file");
	CHECK(justamcp_remap_tool_name("move_node") == "reparent_node");
	CHECK(justamcp_remap_tool_name("cp") == "copy_file");
	CHECK(justamcp_remap_tool_name("stop_play") == "editor_stop_play");
	CHECK(justamcp_remap_tool_name("eval") == "eval_expression");

	Dictionary screen_args;
	screen_args["screen_name"] = "2D";
	screen_args["script"] = "1 + 1";
	screen_args["project_path"] = "res://icon.svg";
	screen_args["uid_string"] = "uid://abc";
	Dictionary screen_norm = justamcp_normalize_tool_args(screen_args, "editor_set_main_screen");
	CHECK(String(screen_norm.get("screen", "")) == "2D");
	CHECK(String(screen_norm.get("code", "")) == "1 + 1");
	CHECK(String(screen_norm.get("path", "")) == "res://icon.svg");
	CHECK(String(screen_norm.get("uid", "")) == "uid://abc");

	Dictionary move_args;
	move_args["path"] = "res://a.txt";
	Dictionary move_norm = justamcp_normalize_tool_args(move_args, "move_file");
	CHECK(String(move_norm.get("from", "")) == "res://a.txt");

	Dictionary dest_args;
	dest_args["from"] = "res://a.txt";
	dest_args["destination"] = "res://b.txt";
	dest_args["dx"] = 4;
	dest_args["scenePath"] = "res://level.tscn";
	Dictionary dest_norm = justamcp_normalize_tool_args(dest_args, "copy_file");
	CHECK(String(dest_norm.get("to", "")) == "res://b.txt");
	CHECK(int(dest_norm.get("relative_x", 0)) == 4);
	CHECK(String(dest_norm.get("scene_path", "")) == "res://level.tscn");

	String canon;
	String canon_err;
	CHECK(justamcp_canonical_sandbox_path("res://foo/bar.tscn", canon, canon_err));
	CHECK(canon == "res://foo/bar.tscn");
	CHECK(!justamcp_canonical_sandbox_path("res://foo/../../outside", canon, canon_err));
	CHECK(!justamcp_canonical_sandbox_path("C:/tmp/out.png", canon, canon_err));
	CHECK(justamcp_ensure_res_path("res://foo/../../outside").is_empty());

	Dictionary nested;
	nested["ok"] = true;
	nested["count"] = 3;
	Dictionary flat_success = justamcp_mcp_success(nested);
	CHECK(bool(flat_success.get("ok", false)));
	CHECK(int(flat_success.get("count", 0)) == 3);
	CHECK(!flat_success.has("result"));

	Dictionary raw;
	raw["tilemap_node"] = "Ground";
	raw["path"] = "res://level.tscn";
	raw["property_path"] = "shape";
	Dictionary norm = justamcp_normalize_tool_args(raw);
	CHECK(String(norm.get("node_path", "")) == "Ground");
	CHECK(String(norm.get("file_path", "")) == "res://level.tscn");
	CHECK(String(norm.get("property", "")) == "shape");

	Vector<Vector2i> rect_cells;
	rect_cells.push_back(Vector2i(2, 4));
	rect_cells.push_back(Vector2i(3, 5));
	Rect2i used = justamcp_used_rect_from_cells(rect_cells);
	CHECK(used.position == Vector2i(2, 4));
	CHECK(used.size == Vector2i(2, 2));

	Vector<Vector2i> ascii_cells;
	ascii_cells.push_back(Vector2i(0, 0));
	ascii_cells.push_back(Vector2i(1, 0));
	ascii_cells.push_back(Vector2i(0, 1));
	Vector<int> ascii_ids;
	ascii_ids.push_back(1);
	ascii_ids.push_back(2);
	ascii_ids.push_back(-1);
	CHECK(justamcp_tile_ascii_grid(ascii_cells, ascii_ids) == "1 2\n. .");

	Dictionary flat = justamcp_ok(Dictionary());
	CHECK(bool(flat.get("ok", false)));
	CHECK(!flat.has("result"));
}

static bool _schema_has(const Array &p_schemas, const String &p_name) {
	for (int i = 0; i < p_schemas.size(); i++) {
		Dictionary item = p_schemas[i];
		if (String(item.get("name", "")) == p_name) {
			return true;
		}
	}
	return false;
}

static Dictionary _schema_named(const Array &p_schemas, const String &p_name) {
	for (int i = 0; i < p_schemas.size(); i++) {
		Dictionary item = p_schemas[i];
		if (String(item.get("name", "")) == p_name) {
			return item;
		}
	}
	return Dictionary();
}

void test_justamcp_agent_gap_schemas() {
	JustAMCPToolSchemaCache::invalidate();
	const Array schemas = JustAMCPToolSchemaCache::get_schemas(false, false, false, true);
	CHECK(_schema_has(schemas, "blazium_editor_run_scene"));
	CHECK(_schema_has(schemas, "blazium_tilemap_configure_atlas"));
	CHECK(_schema_has(schemas, "blazium_tilemap_draw_h_line"));
	CHECK(_schema_has(schemas, "blazium_configure_sprite_frames"));
	CHECK(_schema_has(schemas, "blazium_read_file"));
	CHECK(_schema_has(schemas, "blazium_create_file"));
	CHECK(_schema_has(schemas, "blazium_qa_start"));
	CHECK(_schema_has(schemas, "blazium_get_node_warnings"));
	CHECK(_schema_has(schemas, "blazium_validate_physics_setup"));
	CHECK(_schema_has(schemas, "blazium_validate_tilemap_structure"));
	CHECK(_schema_has(schemas, "blazium_qa_act"));
	CHECK(_schema_has(schemas, "blazium_qa_observe"));
	CHECK(_schema_has(schemas, "blazium_qa_watch"));
	CHECK(_schema_has(schemas, "blazium_qa_drive"));
	CHECK(_schema_has(schemas, "blazium_qa_stop"));
	CHECK(_schema_has(schemas, "blazium_edit_file"));
	CHECK(_schema_has(schemas, "blazium_move_file"));
	CHECK(_schema_has(schemas, "blazium_delete_file"));
	CHECK(_schema_has(schemas, "blazium_tilemap_draw_v_line"));
	CHECK(_schema_has(schemas, "blazium_tilemap_draw_stairs"));
	CHECK(_schema_has(schemas, "blazium_tilemap_erase_rect"));
	CHECK(!_schema_has(schemas, "blazium_run_scene"));
	CHECK(_schema_has(schemas, "blazium_open_in_blazium"));
	CHECK(!_schema_has(schemas, "blazium_open_in_godot"));
	CHECK(_schema_has(schemas, "blazium_runtime_run_autowork_tests"));
	CHECK(!_schema_has(schemas, "blazium_runtime_run_gut_tests"));
	CHECK(_schema_has(schemas, "blazium_autowork_list_tests"));
	CHECK(_schema_has(schemas, "blazium_autowork_is_running"));
	const Dictionary run_aw_schema = _schema_named(schemas, "blazium_runtime_run_autowork_tests");
	Dictionary run_aw_props = Dictionary(Dictionary(run_aw_schema.get("inputSchema", Dictionary())).get("properties", Dictionary()));
	CHECK(run_aw_props.has("include_subdirs"));
	CHECK(run_aw_props.has("prefix"));
	CHECK(run_aw_props.has("suffix"));
	CHECK(run_aw_props.has("select"));
	CHECK(run_aw_props.has("junit_xml"));
#ifdef MODULE_REMOTE_CONTROL_ENABLED
	JustAMCPToolSchemaCache::invalidate();
	const Array disabled_included = JustAMCPToolSchemaCache::get_schemas(false, true, false, true);
	const Dictionary exec_schema = _schema_named(disabled_included, "blazium_remote_control_exec");
	CHECK(!exec_schema.is_empty());
	CHECK(String(exec_schema.get("description", "")).findn("Disabled by default") != -1);
	const Dictionary eval_schema = _schema_named(disabled_included, "blazium_remote_control_eval");
	CHECK(!eval_schema.is_empty());
	CHECK(String(eval_schema.get("description", "")).findn("Disabled by default") != -1);
	CHECK(_schema_has(disabled_included, "blazium_debugger_summary"));
	CHECK(_schema_has(disabled_included, "blazium_focus_window"));
#endif
	CHECK(_schema_has(schemas, "blazium_asset_assign_uid"));
	CHECK(_schema_has(schemas, "blazium_asset_update_uid"));
	CHECK(_schema_has(schemas, "blazium_asset_remove_uid"));
	CHECK(_schema_has(schemas, "blazium_copy_file"));
	CHECK(_schema_has(schemas, "blazium_read_directory"));
	CHECK(_schema_has(schemas, "blazium_save_pixel_art"));
	const Dictionary settings_schema = _schema_named(schemas, "blazium_editor_set_settings");
	Dictionary settings_props = Dictionary(Dictionary(settings_schema.get("inputSchema", Dictionary())).get("properties", Dictionary()));
	Dictionary value_prop = settings_props.get("value", Dictionary());
	const bool value_accepts_any = !value_prop.has("type") || String(value_prop.get("type", "")) != "string";
	CHECK(value_accepts_any);
	const Dictionary compare_schema = _schema_named(schemas, "blazium_runtime_compare_screenshots");
	Dictionary compare_props = Dictionary(Dictionary(compare_schema.get("inputSchema", Dictionary())).get("properties", Dictionary()));
	CHECK(compare_props.has("image_a"));
	CHECK(compare_props.has("image_b"));
	const Dictionary record_schema = _schema_named(schemas, "blazium_runtime_record_video");
	Dictionary record_props = Dictionary(Dictionary(record_schema.get("inputSchema", Dictionary())).get("properties", Dictionary()));
	CHECK(record_props.has("action"));
	const Dictionary run_schema = JustAMCPToolSchemaCache::find_tool_schema("blazium_" + justamcp_remap_tool_name("run_scene"), true);
	CHECK(!run_schema.is_empty());
	const Dictionary input_schema = JustAMCPToolSchemaCache::find_tool_schema("blazium_" + justamcp_remap_tool_name("get_input_map"), true);
	CHECK(!input_schema.is_empty());
}

void test_justamcp_agent_gap_dispatch() {
	JustAMCPToolExecutor executor;
	struct Route {
		const char *category;
		const char *internal_name;
	};
	const Route routes[] = {
		{ "editor_tools", "qa_start" },
		{ "editor_tools", "editor_run_scene" },
		{ "scene_tools", "get_node_warnings" },
		{ "tilemap_tools", "tilemap_configure_atlas" },
		{ "animation_tools", "configure_sprite_frames" },
		{ "project_tools", "read_file" },
		{ "physics_tools", "validate_physics_setup" },
		{ "editor_tools", "scene_tree_dump" },
		{ "project_tools", "project_get_input_actions" },
		{ "tilemap_tools", "tilemap_get_info" },
		{ "analysis_tools", "project_state" },
		{ "runtime_tools", "wait" },
		{ "editor_tools", "logs_read" },
		{ "documentation_tools", "classdb_query" },
		{ "project_tools", "asset_assign_uid" },
		{ "scene_tools", "find_nodes" },
		{ "scene_tools", "get_node_property" },
		{ "scene_tools", "press_button" },
		{ "project_tools", "copy_file" },
		{ "asset_tools", "save_pixel_art" },
#ifdef MODULE_AUTOWORK_ENABLED
		{ "autowork_tools", "autowork_list_tests" },
		{ "autowork_tools", "autowork_is_running" },
#endif
	};
	for (const Route &route : routes) {
		Dictionary routed = JustAMCPToolCategoryDispatch::dispatch_module_tools(
				&executor, route.category, route.internal_name, Dictionary());
		CHECK(bool(routed.get("handled", false)));
		CHECK(routed.has("ok"));
	}

	Dictionary via_alias = executor.execute_tool("run_scene", Dictionary());
	CHECK(via_alias.has("ok"));
	{
		const Variant err = via_alias.get("error", Variant());
		const String err_text = err.get_type() == Variant::DICTIONARY ? String(Dictionary(err).get("message", "")) : String(err);
		CHECK(err_text.findn("not found") == -1);
	}

	Dictionary input_map = executor.execute_tool("get_input_map", Dictionary());
	CHECK(input_map.has("ok"));
	{
		const Variant err = input_map.get("error", Variant());
		const String err_text = err.get_type() == Variant::DICTIONARY ? String(Dictionary(err).get("message", "")) : String(err);
		CHECK(err_text.findn("not found") == -1);
	}

	Dictionary open_alias = executor.execute_tool("open_in_godot", Dictionary());
	CHECK(open_alias.has("ok"));
	{
		const Variant err = open_alias.get("error", Variant());
		const String err_text = err.get_type() == Variant::DICTIONARY ? String(Dictionary(err).get("message", "")) : String(err);
		CHECK(err_text.findn("not found") == -1);
	}

	Dictionary empty_required = executor.execute_tool("read_file", Dictionary());
	CHECK(!bool(empty_required.get("ok", true)));
	Dictionary empty_path;
	empty_path["file_path"] = "";
	Dictionary empty_string = executor.execute_tool("read_file", empty_path);
	CHECK(!bool(empty_string.get("ok", true)));

	Dictionary empty_array_args;
	empty_array_args["node_paths"] = Array();
	Dictionary empty_array = executor.execute_tool("editor_select_node", empty_array_args);
	CHECK(!bool(empty_array.get("ok", true)));

	Dictionary playtest_inputs;
	Array play_inputs;
	Dictionary play_input;
	play_input["kind"] = "action";
	play_input["action"] = "ui_accept";
	play_inputs.push_back(play_input);
	playtest_inputs["inputs"] = play_inputs;
	Dictionary playtest_no_duration = executor.execute_tool("editor_play_scene", playtest_inputs);
	CHECK(!bool(playtest_no_duration.get("ok", true)));

	Dictionary shot_args;
	shot_args["path"] = "C:/tmp/out.png";
	Dictionary shot = executor.execute_tool("editor_take_screenshot", shot_args);
	CHECK(!bool(shot.get("ok", true)));
	Dictionary shot_escape;
	shot_escape["path"] = "res://foo/../..";
	Dictionary shot_bad = executor.execute_tool("editor_take_screenshot", shot_escape);
	CHECK(!bool(shot_bad.get("ok", true)));

	Dictionary uid_alias = executor.execute_tool("asset_to_uid", Dictionary());
	CHECK(uid_alias.has("ok"));
	{
		const Variant err = uid_alias.get("error", Variant());
		const String err_text = err.get_type() == Variant::DICTIONARY ? String(Dictionary(err).get("message", "")) : String(err);
		CHECK(err_text.findn("not found") == -1);
	}

	Dictionary run_tests_args;
	run_tests_args["timeout"] = 1;
	run_tests_args["path"] = "res://__justamcp_missing_autowork_suite";
	Dictionary run_tests = executor.execute_tool("run_tests", run_tests_args);
	CHECK(run_tests.has("ok"));
	{
		const Variant err = run_tests.get("error", Variant());
		const String err_text = err.get_type() == Variant::DICTIONARY ? String(Dictionary(err).get("message", "")) : String(err);
		CHECK(err_text.findn("not found") == -1);
		CHECK(err_text.findn("GUT") == -1);
	}

#ifdef MODULE_AUTOWORK_ENABLED
	Dictionary list_escape;
	list_escape["path"] = "C:/tmp/tests";
	Dictionary listed_bad = executor.execute_tool("autowork_list_tests", list_escape);
	CHECK(!bool(listed_bad.get("ok", true)));

	Dictionary list_ok = executor.execute_tool("autowork_list_tests", Dictionary());
	CHECK(bool(list_ok.get("ok", false)));
	CHECK(list_ok.has("scripts"));

	Dictionary running = executor.execute_tool("autowork_is_running", Dictionary());
	CHECK(bool(running.get("ok", false)));
	CHECK(running.has("running"));

	Dictionary junit_args;
	junit_args["timeout"] = 1;
	junit_args["path"] = "res://__justamcp_missing_autowork_suite";
	junit_args["junit_xml"] = "res://out.xml";
	Dictionary junit_bad = executor.execute_tool("runtime_run_autowork_tests", junit_args);
	CHECK(!bool(junit_bad.get("ok", true)));
	{
		const Variant err = junit_bad.get("error", Variant());
		const String err_text = err.get_type() == Variant::DICTIONARY ? String(Dictionary(err).get("message", "")) : String(err);
		CHECK(err_text.findn("user://") != -1);
	}

	Autowork *aw = memnew(Autowork);
	CHECK(!aw->is_finished());
	CHECK(!aw->is_aborted());
	aw->abort();
	CHECK(aw->is_finished());
	CHECK(aw->is_aborted());
	memdelete(aw);
#endif

#ifdef MODULE_REMOTE_CONTROL_ENABLED
	JustAMCPToolExecutor::register_tool_settings();
	CHECK(!JustAMCPSettingsResolver::is_tool_executable("remote_control_tools", "blazium_remote_control_exec"));
	CHECK(!JustAMCPSettingsResolver::is_tool_executable("remote_control_tools", "blazium_remote_control_eval"));

	ProjectSettings *ps = ProjectSettings::get_singleton();
	const String exec_path = "blazium/justamcp/tools/remote_control_tools/blazium_remote_control_exec";
	const bool had_exec = ps && ps->has_setting(exec_path);
	const Variant prev_exec = had_exec ? ps->get_setting(exec_path) : Variant();
	if (ps) {
		ps->set_setting(exec_path, false);
	}
	Dictionary exec_args;
	exec_args["command"] = "ping";
	Dictionary exec_disabled = executor.execute_tool("remote_control_exec", exec_args);
	CHECK(!bool(exec_disabled.get("ok", true)));
	{
		const Variant err = exec_disabled.get("error", Variant());
		const String err_text = err.get_type() == Variant::DICTIONARY ? String(Dictionary(err).get("message", "")) : String(err);
		CHECK(err_text.findn("disabled") != -1);
	}
	if (ps && had_exec) {
		ps->set_setting(exec_path, prev_exec);
	}

	Dictionary summary = executor.execute_tool("debugger_summary", Dictionary());
	CHECK(bool(summary.get("ok", false)));
	CHECK(summary.has("error_count"));
	CHECK(summary.has("stack_tops"));
	CHECK(!summary.has("files"));
	CHECK(!summary.has("logs"));
#endif

	Dictionary flags;
	justamcp_apply_alias_query_flags("get_tilemap_state", flags);
	CHECK(bool(flags.get("ascii", false)));
	justamcp_apply_alias_query_flags("get_tileset_info", flags);
	CHECK(bool(flags.get("tileset_only", false)));
}

#endif
