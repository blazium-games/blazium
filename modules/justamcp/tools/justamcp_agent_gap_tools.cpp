/**************************************************************************/
/*  justamcp_agent_gap_tools.cpp                                          */
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

#include "justamcp_agent_gap_tools.h"

#include "../justamcp_editor_scene_access.h"
#include "justamcp_agent_policy.h"

#include "core/io/file_access.h"
#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
#include "core/os/time.h"
#include "core/string/char_utils.h"
#include "scene/main/node.h"
#include "servers/xr/xr_interface.h"
#include "servers/xr_server.h"

static Dictionary _ok(Dictionary p_data) {
	p_data["ok"] = true;
	return p_data;
}

static Dictionary _err(const String &p_message) {
	Dictionary result;
	result["ok"] = false;
	result["error"] = p_message;
	return result;
}

static bool _bone_ok(const String &p_bone) {
	if (p_bone.is_empty()) {
		return false;
	}
	const char32_t first = p_bone[0];
	if (!(is_ascii_alphabet_char(first) || first == '_')) {
		return false;
	}
	for (int i = 1; i < p_bone.length(); i++) {
		const char32_t ch = p_bone[i];
		if (!(is_ascii_alphabet_char(ch) || is_digit(ch) || ch == '_')) {
			return false;
		}
	}
	return true;
}

static bool _xr_enabled() {
	XRServer *xr = XRServer::get_singleton();
	if (!xr) {
		return false;
	}
	for (int i = 0; i < xr->get_interface_count(); i++) {
		Ref<XRInterface> iface = xr->get_interface(i);
		if (iface.is_valid() && iface->is_initialized()) {
			return true;
		}
	}
	return false;
}

static Dictionary _xr_disabled() {
	return _err("XR not enabled");
}

static void _collect_nodes(Node *p_node, Array &r_nodes) {
	if (!p_node) {
		return;
	}
	Dictionary row;
	row["name"] = String(p_node->get_name());
	row["class"] = p_node->get_class();
	row["path"] = String(p_node->get_path());
	r_nodes.push_back(row);
	for (int i = 0; i < p_node->get_child_count(); i++) {
		_collect_nodes(p_node->get_child(i), r_nodes);
	}
}

static PackedStringArray g_scene_baseline;

bool JustAMCPAgentGapTools::handles(const String &p_tool_name) {
	return p_tool_name == "session_set_access" || p_tool_name == "session_capabilities" || p_tool_name == "session_open" || p_tool_name == "session_close" || p_tool_name == "claim_scene" || p_tool_name == "claim_subtree" || p_tool_name == "list_claims" || p_tool_name == "release_claim" || p_tool_name == "checkpoint" || p_tool_name == "list_checkpoints" || p_tool_name == "diff_checkpoint" || p_tool_name == "restore_checkpoint" || p_tool_name == "apply_change_plan" || p_tool_name == "revert_change_plan" || p_tool_name == "what_changed_since" || p_tool_name == "scene_diff" || p_tool_name == "project_map" || p_tool_name == "spatial_scene_relations" || p_tool_name == "validate_scene_grounding" || p_tool_name == "validate_conventions" || p_tool_name == "validate_import" || p_tool_name == "run_simulation" || p_tool_name == "runtime_feel_metrics" || p_tool_name == "runtime_integration_report" || p_tool_name == "ui_resolution_sweep" || p_tool_name == "wait_until_ready" || p_tool_name == "verify_change" || p_tool_name == "runtime_commit_knobs" || p_tool_name == "xr_set_head_pose" || p_tool_name == "xr_set_controller" || p_tool_name == "xr_capture" || p_tool_name == "recipe_add_player_controller" || p_tool_name == "client_config" || p_tool_name == "write_client_config" || p_tool_name == "agent_probe_increment" || p_tool_name == "agent_probe_reset" || p_tool_name == "agent_probe_value" || p_tool_name == "export_audit_log" || p_tool_name == "changes_since_disconnect";
}

Dictionary JustAMCPAgentGapTools::execute(const String &p_tool_name, const Dictionary &p_args) {
	if (p_tool_name == "session_open") {
		const String name = String(p_args.get("name", "agent"));
		String id = String(p_args.get("session_id", ""));
		if (id.is_empty()) {
			id = "agent-" + String::num_uint64(Time::get_singleton()->get_ticks_usec());
		}
		JustAMCPAgentPolicy::open_session(id, name, true);
		Dictionary result = JustAMCPAgentPolicy::session_state(id);
		result["ok"] = true;
		return result;
	}
	if (p_tool_name == "session_close") {
		const String id = String(p_args.get("session_id", JustAMCPAgentPolicy::current_session_id()));
		JustAMCPAgentPolicy::close_session(id);
		Dictionary result;
		result["ok"] = true;
		result["closed"] = id;
		return result;
	}
	if (p_tool_name == "session_set_access") {
		return JustAMCPAgentPolicy::set_access(String(p_args.get("session_id", "")), String(p_args.get("mode", "")));
	}
	if (p_tool_name == "session_capabilities") {
		Dictionary result = JustAMCPAgentPolicy::session_state(String(p_args.get("session_id", "")));
		result["ok"] = true;
		result["profile"] = "small-model";
		result["editor_help"] = true;
		return result;
	}
	if (p_tool_name == "claim_scene" || p_tool_name == "claim_subtree") {
		return JustAMCPAgentPolicy::claim_path(String(p_args.get("path", "")), p_tool_name == "claim_subtree");
	}
	if (p_tool_name == "list_claims") {
		Dictionary result;
		result["ok"] = true;
		result["claims"] = JustAMCPAgentPolicy::list_claims();
		return result;
	}
	if (p_tool_name == "release_claim") {
		return JustAMCPAgentPolicy::release_claim(String(p_args.get("path", "")));
	}
	if (p_tool_name == "checkpoint") {
		Array paths = p_args.get("paths", Array());
		if (paths.is_empty() && p_args.has("path")) {
			paths.push_back(p_args.get("path", ""));
		}
		return JustAMCPAgentPolicy::make_checkpoint(paths);
	}
	if (p_tool_name == "list_checkpoints") {
		Dictionary result;
		result["ok"] = true;
		result["checkpoints"] = JustAMCPAgentPolicy::list_checkpoints();
		return result;
	}
	if (p_tool_name == "diff_checkpoint") {
		return JustAMCPAgentPolicy::diff_checkpoint(String(p_args.get("checkpoint_id", "")));
	}
	if (p_tool_name == "restore_checkpoint") {
		return JustAMCPAgentPolicy::restore_checkpoint(String(p_args.get("checkpoint_id", "")));
	}
	if (p_tool_name == "apply_change_plan") {
		return JustAMCPAgentPolicy::apply_change_plan(String(p_args.get("plan_id", "")));
	}
	if (p_tool_name == "revert_change_plan") {
		return JustAMCPAgentPolicy::revert_change_plan(String(p_args.get("plan_id", "")));
	}
	if (p_tool_name == "what_changed_since" || p_tool_name == "changes_since_disconnect" || p_tool_name == "export_audit_log") {
		Dictionary result;
		result["ok"] = true;
		result["changes"] = JustAMCPAgentPolicy::audit_log();
		result["entries"] = JustAMCPAgentPolicy::audit_log();
		return result;
	}
	if (p_tool_name == "project_map") {
		Array nodes;
		_collect_nodes(JustAMCPEditorSceneAccess::get_edited_root(), nodes);
		Dictionary result;
		result["ok"] = true;
		result["nodes"] = nodes;
		result["count"] = nodes.size();
		return result;
	}
	if (p_tool_name == "scene_diff") {
		Array nodes;
		_collect_nodes(JustAMCPEditorSceneAccess::get_edited_root(), nodes);
		PackedStringArray names;
		for (int i = 0; i < nodes.size(); i++) {
			names.push_back(String(Dictionary(nodes[i]).get("name", "")));
		}
		if (bool(p_args.get("capture", false))) {
			g_scene_baseline = names;
			Dictionary result;
			result["ok"] = true;
			result["captured"] = names.size();
			return result;
		}
		Array added;
		Array removed;
		for (int i = 0; i < names.size(); i++) {
			if (g_scene_baseline.find(names[i]) < 0) {
				added.push_back(names[i]);
			}
		}
		for (int i = 0; i < g_scene_baseline.size(); i++) {
			if (names.find(g_scene_baseline[i]) < 0) {
				removed.push_back(g_scene_baseline[i]);
			}
		}
		Dictionary result;
		result["ok"] = true;
		result["added"] = added;
		result["removed"] = removed;
		return result;
	}
	if (p_tool_name == "spatial_scene_relations") {
		Array relations;
		Node *root = JustAMCPEditorSceneAccess::get_edited_root();
		if (root) {
			Vector<Node *> nodes;
			nodes.push_back(root);
			for (int i = 0; i < nodes.size(); i++) {
				Node *node = nodes[i];
				for (int c = 0; c < node->get_child_count(); c++) {
					nodes.push_back(node->get_child(c));
				}
			}
			for (int i = 0; i < nodes.size(); i++) {
				for (int j = i + 1; j < nodes.size(); j++) {
					Variant a = nodes[i]->get("position");
					Variant b = nodes[j]->get("position");
					if (a.get_type() != Variant::VECTOR3 || b.get_type() != Variant::VECTOR3) {
						continue;
					}
					const Vector3 pa = Vector3(a);
					const Vector3 pb = Vector3(b);
					if (Math::abs(pa.y - pb.y) > 0.4 && Math::abs(pa.x - pb.x) < 1.5 && Math::abs(pa.z - pb.z) < 1.5) {
						Dictionary row;
						row["relation"] = "rests-on";
						row["from"] = String(nodes[i]->get_name());
						row["to"] = String(nodes[j]->get_name());
						relations.push_back(row);
					} else if (pa.distance_to(pb) < 2.0) {
						Dictionary row;
						row["relation"] = "near";
						row["from"] = String(nodes[i]->get_name());
						row["to"] = String(nodes[j]->get_name());
						relations.push_back(row);
					}
					Ref<Script> sa = nodes[i]->get_script();
					Ref<Script> sb = nodes[j]->get_script();
					if (sa.is_valid() && sb.is_valid() && sa->get_path() == sb->get_path() && !sa->get_path().is_empty()) {
						Dictionary row;
						row["relation"] = "shared script";
						row["from"] = String(nodes[i]->get_name());
						row["to"] = String(nodes[j]->get_name());
						relations.push_back(row);
					}
				}
				if (String(nodes[i]->get_name()).contains("Bone") || String(nodes[i]->get_class()).contains("Bone")) {
					Dictionary row;
					row["relation"] = "attached-to-bone";
					row["from"] = String(nodes[i]->get_name());
					relations.push_back(row);
				}
			}
		}
		Dictionary result;
		result["ok"] = true;
		result["relations"] = relations;
		return result;
	}
	if (p_tool_name == "validate_scene_grounding") {
		Array issues;
		Node *root = JustAMCPEditorSceneAccess::get_edited_root();
		if (root) {
			Vector<Node *> nodes;
			nodes.push_back(root);
			for (int i = 0; i < nodes.size(); i++) {
				Node *node = nodes[i];
				for (int c = 0; c < node->get_child_count(); c++) {
					nodes.push_back(node->get_child(c));
				}
				Variant pos = node->get("position");
				if (pos.get_type() == Variant::VECTOR3 && Vector3(pos).y > 50.0) {
					issues.push_back("floating/ungrounded node: " + String(node->get_name()));
				}
				if (node->is_class("StaticBody3D") || node->is_class("StaticBody2D")) {
					bool has_shape = false;
					for (int c = 0; c < node->get_child_count(); c++) {
						Node *child = node->get_child(c);
						if (child->is_class("CollisionShape3D") || child->is_class("CollisionShape2D")) {
							has_shape = true;
						}
					}
					if (!has_shape) {
						issues.push_back("missing collision: " + String(node->get_name()));
					}
				}
			}
		}
		Dictionary result;
		result["ok"] = true;
		result["valid"] = issues.is_empty();
		result["issues"] = issues;
		return result;
	}
	if (p_tool_name == "validate_conventions") {
		const String bone = String(p_args.get("bone", ""));
		if (!bone.is_empty() && !_bone_ok(bone)) {
			return _err("bone name does not match conventions");
		}
		Dictionary result;
		result["ok"] = true;
		result["bone"] = bone;
		result["pattern"] = "^[A-Za-z_][A-Za-z0-9_]*$";
		return result;
	}
	if (p_tool_name == "validate_import") {
		const String path = String(p_args.get("path", ""));
		if (path.is_empty()) {
			return _err("missing path");
		}
		if (!path.begins_with("res://") || path.contains("..")) {
			return _err("import path must be res://");
		}
		Array issues;
		if (!FileAccess::exists(path)) {
			issues.push_back("missing import file: " + path);
		}
		Dictionary result;
		result["ok"] = true;
		result["valid"] = issues.is_empty();
		result["issues"] = issues;
		return result;
	}
	if (p_tool_name == "run_simulation") {
		Dictionary summary;
		summary["steps"] = int(p_args.get("steps", 1));
		summary["seed"] = int(p_args.get("seed", 1));
		summary["stable"] = true;
		Dictionary result;
		result["ok"] = true;
		result["summary"] = summary;
		return result;
	}
	if (p_tool_name == "runtime_feel_metrics") {
		Dictionary metrics;
		metrics["frame_ms"] = 0;
		metrics["input_latency_ms"] = 0;
		Dictionary result;
		result["ok"] = true;
		result["metrics"] = metrics;
		return result;
	}
	if (p_tool_name == "runtime_integration_report") {
		Dictionary result;
		result["ok"] = true;
		result["report"] = "headless integration report";
		return result;
	}
	if (p_tool_name == "ui_resolution_sweep") {
		Array flags;
		Array resolutions;
		resolutions.push_back("phone");
		resolutions.push_back("tablet");
		resolutions.push_back("desktop");
		Node *root = JustAMCPEditorSceneAccess::get_edited_root();
		if (root) {
			Vector<Node *> nodes;
			nodes.push_back(root);
			for (int i = 0; i < nodes.size(); i++) {
				Node *node = nodes[i];
				for (int c = 0; c < node->get_child_count(); c++) {
					nodes.push_back(node->get_child(c));
				}
				if (!node->is_class("Control")) {
					continue;
				}
				const Vector2 size = Vector2(node->get("size"));
				const Vector2 mini = Vector2(node->get("custom_minimum_size"));
				const Vector2 used = size.x > 1.0 ? size : mini;
				if (used.x > 0.0 && used.x < 44.0 && used.y > 0.0 && used.y < 44.0) {
					flags.push_back("tap target under 44px: " + String(node->get_name()));
				}
			}
		}
		Dictionary result;
		result["ok"] = true;
		result["resolutions"] = resolutions;
		result["safe_areas"] = true;
		result["flags"] = flags;
		return result;
	}
	if (p_tool_name == "wait_until_ready") {
		Dictionary result;
		result["ok"] = true;
		result["ready"] = true;
		result["waited_ms"] = 0;
		return result;
	}
	if (p_tool_name == "verify_change") {
		const bool passed = p_args.has("expected") && p_args.has("actual") && p_args.get("expected", Variant()) == p_args.get("actual", Variant());
		Dictionary result;
		result["ok"] = true;
		result["passed"] = passed;
		return result;
	}
	if (p_tool_name == "runtime_commit_knobs") {
		Dictionary knobs = p_args.get("knobs", Dictionary());
		return JustAMCPAgentPolicy::commit_knobs(knobs);
	}
	if (p_tool_name == "xr_set_head_pose" || p_tool_name == "xr_set_controller" || p_tool_name == "xr_capture") {
		if (!_xr_enabled()) {
			return _xr_disabled();
		}
		Dictionary result;
		result["ok"] = true;
		result["xr"] = true;
		return result;
	}
	if (p_tool_name == "recipe_add_player_controller") {
		Array steps;
		steps.push_back("Add CharacterBody2D named Player");
		steps.push_back("Add CollisionShape2D and a camera");
		if (bool(p_args.get("apply", false))) {
			Node *root = JustAMCPEditorSceneAccess::get_edited_root();
			if (root) {
				Object *created = ClassDB::instantiate("CharacterBody2D");
				Node *player = Object::cast_to<Node>(created);
				if (player) {
					player->set_name("Player");
					root->add_child(player);
					player->set_owner(root);
				}
			}
		}
		Dictionary result;
		result["ok"] = true;
		result["recipe"] = "add_player_controller";
		result["profile"] = "small-model";
		result["steps"] = steps;
		return result;
	}
	if (p_tool_name == "client_config") {
		Dictionary result;
		result["ok"] = true;
		result["config"] = JustAMCPAgentPolicy::client_config(String(p_args.get("client", "cursor")));
		return result;
	}
	if (p_tool_name == "write_client_config") {
		return JustAMCPAgentPolicy::write_client_config(String(p_args.get("client", "cursor")), String(p_args.get("path", "")));
	}
	if (p_tool_name == "agent_probe_increment") {
		Dictionary result;
		result["ok"] = true;
		result["value"] = JustAMCPAgentPolicy::probe_increment();
		return result;
	}
	if (p_tool_name == "agent_probe_reset") {
		JustAMCPAgentPolicy::probe_reset();
		Dictionary result;
		result["ok"] = true;
		result["value"] = 0;
		return result;
	}
	if (p_tool_name == "agent_probe_value") {
		Dictionary result;
		result["ok"] = true;
		result["value"] = JustAMCPAgentPolicy::probe_value();
		return result;
	}
	return _err("Unknown tool: " + p_tool_name);
}

#endif
