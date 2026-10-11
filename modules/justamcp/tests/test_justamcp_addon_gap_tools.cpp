/**************************************************************************/
/*  test_justamcp_addon_gap_tools.cpp                                     */
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

#include "test_justamcp_addon_gap_tools.h"

#include "../tools/justamcp_addon_gap_tools.h"
#include "../tools/justamcp_agent_policy.h"
#include "../tools/justamcp_tool_executor.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "editor/editor_undo_redo_manager.h"
#include "scene/main/node.h"
#include "tests/test_macros.h"

void test_justamcp_addon_gap_tools() {
	CHECK(JustAMCPAddonGapTools::handles("add_sprite"));
	CHECK(JustAMCPAddonGapTools::handles("create_visual_shader"));
	CHECK_FALSE(JustAMCPAddonGapTools::handles("not_a_gap_tool"));

	Dictionary missing = JustAMCPAddonGapTools::execute("add_sprite", Dictionary());
	CHECK_FALSE(bool(missing.get("ok", true)));
	CHECK(String(missing.get("error", "")).contains("No scene"));

	Dictionary bad_branch;
	bad_branch["dest_path"] = "user://not_a_scene.txt";
	Dictionary branch = JustAMCPAddonGapTools::execute("save_branch_as_scene", bad_branch);
	CHECK_FALSE(bool(branch.get("ok", true)));

	Dictionary image = JustAMCPAddonGapTools::execute("create_placeholder_texture", Dictionary());
	CHECK(bool(image.get("ok", false)));
	CHECK_EQ(int(image.get("width", 0)), 8);

	Dictionary info_args;
	info_args["path"] = image.get("path", "");
	Dictionary info = JustAMCPAddonGapTools::execute("get_image_info", info_args);
	CHECK(bool(info.get("ok", false)));
	CHECK_EQ(int(info.get("width", 0)), 8);

	Dictionary missing_image;
	missing_image["path"] = "user://justamcp_gap_missing.png";
	Dictionary bad_image = JustAMCPAddonGapTools::execute("get_image_info", missing_image);
	CHECK_FALSE(bool(bad_image.get("ok", true)));

	Dictionary diff = JustAMCPAddonGapTools::execute("diff_images", Dictionary());
	CHECK(bool(diff.get("ok", false)));
	CHECK(bool(diff.get("match", false)));

	Dictionary preset = JustAMCPAddonGapTools::execute("add_shader_preset", Dictionary());
	CHECK(bool(preset.get("ok", false)));
	CHECK(FileAccess::exists(preset["path"]));

	Dictionary visual = JustAMCPAddonGapTools::execute("create_visual_shader", Dictionary());
	if (bool(visual.get("ok", false))) {
		Dictionary added = JustAMCPAddonGapTools::execute("add_visual_shader_node", Dictionary());
		CHECK(bool(added.get("ok", false)));
		Dictionary graph = JustAMCPAddonGapTools::execute("get_visual_shader_info", Dictionary());
		CHECK(bool(graph.get("ok", false)));
		CHECK_GT(int(graph.get("node_count", 0)), 0);
		DirAccess::remove_absolute(visual.get("path", ""));
	} else {
		CHECK(String(visual.get("error", "")).contains("RenderingServer"));
	}

	DirAccess::remove_absolute(image.get("path", ""));
	DirAccess::remove_absolute(preset.get("path", ""));

	Node *root = memnew(Node);
	root->set_name("GapUndoRoot");
	JustAMCPToolExecutor::set_test_scene_root(root);
	Dictionary spawn_args;
	spawn_args["parent_path"] = ".";
	spawn_args["node_name"] = "GapTimer";
	Dictionary spawned = JustAMCPAddonGapTools::execute("add_timer", spawn_args);
	CHECK(bool(spawned.get("ok", false)));
	CHECK(root->get_node_or_null(NodePath("GapTimer")) != nullptr);
	if (EditorUndoRedoManager *undo = EditorUndoRedoManager::get_singleton()) {
		CHECK(undo->undo());
		CHECK(undo->undo());
		CHECK(undo->undo());
	} else {
		CHECK_EQ(JustAMCPAgentPolicy::undo_snapshots(3), 3);
	}
	CHECK(root->get_node_or_null(NodePath("GapTimer")) == nullptr);

	spawned = JustAMCPAddonGapTools::execute("add_timer", spawn_args);
	CHECK(bool(spawned.get("ok", false)));
	Node *timer = root->get_node_or_null(NodePath("GapTimer"));
	CHECK(timer != nullptr);
	const ObjectID timer_id = timer->get_instance_id();
	Dictionary replace_args;
	replace_args["node_path"] = "GapTimer";
	replace_args["node_type"] = "Node";
	Dictionary replaced = JustAMCPAddonGapTools::execute("replace_node_type", replace_args);
	CHECK(bool(replaced.get("ok", false)));
	Node *replacement = root->get_node_or_null(NodePath("GapTimer"));
	CHECK(replacement != nullptr);
	CHECK(replacement->get_instance_id() != timer_id);
	if (EditorUndoRedoManager::get_singleton()) {
		CHECK(ObjectDB::get_instance(timer_id) != nullptr);
	} else {
		CHECK(ObjectDB::get_instance(timer_id) == nullptr);
	}

	Dictionary branch_args;
	branch_args["node_path"] = ".";
	branch_args["dest_path"] = "res://justamcp_gap_branch_undo.tscn";
	Dictionary saved = JustAMCPAddonGapTools::execute("save_branch_as_scene", branch_args);
	CHECK(bool(saved.get("ok", false)));
	CHECK_GT(FileAccess::get_file_as_bytes("res://justamcp_gap_branch_undo.tscn").size(), 0);
	CHECK_EQ(JustAMCPAgentPolicy::undo_snapshots(1), 1);
	CHECK_EQ(FileAccess::get_file_as_bytes("res://justamcp_gap_branch_undo.tscn").size(), 0);
	DirAccess::remove_absolute("res://justamcp_gap_branch_undo.tscn");

	JustAMCPToolExecutor::set_test_scene_root(nullptr);
	memdelete(root);

	for (int i = 0; i < 40; i++) {
		const String id = "gap_cap_" + itos(i);
		JustAMCPAgentPolicy::open_session(id, id, false);
		JustAMCPAgentPolicy::close_session(id);
	}
	CHECK_EQ(JustAMCPAgentPolicy::closed_session_count(), 32);

	JustAMCPAgentPolicy::open_session("gap_plan_cap", "gap_plan_cap", false);
	for (int i = 0; i < 40; i++) {
		Dictionary args;
		args["dry_run"] = true;
		args["session_id"] = "gap_plan_cap";
		Dictionary early;
		JustAMCPAgentPolicy::before_execute("add_sprite", args, early);
		JustAMCPAgentPolicy::after_execute("add_sprite", args, early);
	}
	CHECK_EQ(JustAMCPAgentPolicy::plan_count(), 32);

	CHECK(bool(JustAMCPAgentPolicy::claim_path("res://gap_queue_cap.tscn", false).get("ok", false)));
	JustAMCPAgentPolicy::open_session("gap_queue_writer", "gap_queue_writer", false);
	for (int i = 0; i < 40; i++) {
		Dictionary args;
		args["session_id"] = "gap_queue_writer";
		args["path"] = "res://gap_queue_cap.tscn";
		Dictionary early;
		JustAMCPAgentPolicy::before_execute("add_sprite", args, early);
		JustAMCPAgentPolicy::after_execute("add_sprite", args, early);
	}
	CHECK_EQ(JustAMCPAgentPolicy::queued_write_count(), 32);
	JustAMCPAgentPolicy::close_session("gap_queue_writer");
	JustAMCPAgentPolicy::release_claim("res://gap_queue_cap.tscn");
}

#endif
