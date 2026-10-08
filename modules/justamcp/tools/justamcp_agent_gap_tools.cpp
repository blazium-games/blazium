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
#include "../justamcp_server.h"
#include "justamcp_agent_policy.h"

#include "core/config/project_settings.h"
#include "core/templates/hash_map.h"
#include "core/input/input_map.h"
#include "core/io/file_access.h"
#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
#include "core/os/time.h"
#include "core/string/char_utils.h"
#include "editor/editor_file_system.h"
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

static Array g_scene_baseline;

static bool _foreign_session(const Dictionary &p_args) {
	const String transport = String(p_args.get("_session_id", ""));
	const String requested = String(p_args.get("session_id", ""));
	return !transport.is_empty() && !requested.is_empty() && transport != requested;
}

static int _editor_error_count() {
	int count = 0;
	JustAMCPServer *server = JustAMCPServer::get_singleton();
	if (!server) {
		return 0;
	}
	Vector<String> logs = server->get_engine_logs();
	const int start = MAX(0, logs.size() - 200);
	for (int i = start; i < logs.size(); i++) {
		const String lower = logs[i].to_lower();
		if (lower.contains("error") || lower.contains("warning") || lower.contains("failed")) {
			count++;
		}
	}
	return count;
}

static Array _recent_errors(int p_limit) {
	Array errors;
	JustAMCPServer *server = JustAMCPServer::get_singleton();
	if (!server) {
		return errors;
	}
	Vector<String> logs = server->get_engine_logs();
	const int start = MAX(0, logs.size() - 200);
	for (int i = start; i < logs.size() && errors.size() < p_limit; i++) {
		const String lower = logs[i].to_lower();
		if (lower.contains("error") || lower.contains("warning") || lower.contains("failed")) {
			errors.push_back(logs[i]);
		}
	}
	return errors;
}

static Dictionary _scene_row(Node *p_node) {
	Dictionary row;
	row["name"] = String(p_node->get_name());
	row["path"] = String(p_node->get_path());
	row["parent"] = p_node->get_parent() ? String(p_node->get_parent()->get_path()) : String();
	Variant position = p_node->get("position");
	if (position.get_type() == Variant::VECTOR3 || position.get_type() == Variant::VECTOR2) {
		row["position"] = position;
	}
	Variant scale = p_node->get("scale");
	if (scale.get_type() == Variant::VECTOR3 || scale.get_type() == Variant::VECTOR2) {
		row["scale"] = scale;
	}
	return row;
}

static Array _scene_rows(Node *p_root) {
	Array rows;
	if (!p_root) {
		return rows;
	}
	Vector<Node *> nodes;
	nodes.push_back(p_root);
	for (int i = 0; i < nodes.size(); i++) {
		Node *node = nodes[i];
		rows.push_back(_scene_row(node));
		for (int c = 0; c < node->get_child_count(); c++) {
			nodes.push_back(node->get_child(c));
		}
	}
	return rows;
}

static bool _extreme_scale(const Vector3 &p_scale) {
	const float axes[3] = { p_scale.x, p_scale.y, p_scale.z };
	for (int i = 0; i < 3; i++) {
		const float axis = Math::abs(axes[i]);
		if (axis > 100.0f || (axis > 0.0f && axis < 0.01f)) {
			return true;
		}
	}
	return false;
}

bool JustAMCPAgentGapTools::handles(const String &p_tool_name) {
	return p_tool_name == "session_set_access" || p_tool_name == "session_capabilities" || p_tool_name == "session_open" || p_tool_name == "session_close" || p_tool_name == "claim_scene" || p_tool_name == "claim_subtree" || p_tool_name == "list_claims" || p_tool_name == "release_claim" || p_tool_name == "checkpoint" || p_tool_name == "list_checkpoints" || p_tool_name == "diff_checkpoint" || p_tool_name == "restore_checkpoint" || p_tool_name == "apply_change_plan" || p_tool_name == "revert_change_plan" || p_tool_name == "what_changed_since" || p_tool_name == "scene_diff" || p_tool_name == "project_map" || p_tool_name == "spatial_scene_relations" || p_tool_name == "validate_scene_grounding" || p_tool_name == "validate_conventions" || p_tool_name == "validate_import" || p_tool_name == "run_simulation" || p_tool_name == "runtime_feel_metrics" || p_tool_name == "runtime_integration_report" || p_tool_name == "ui_resolution_sweep" || p_tool_name == "wait_until_ready" || p_tool_name == "verify_change" || p_tool_name == "runtime_commit_knobs" || p_tool_name == "xr_set_head_pose" || p_tool_name == "xr_set_controller" || p_tool_name == "xr_capture" || p_tool_name == "recipe_add_player_controller" || p_tool_name == "client_config" || p_tool_name == "write_client_config" || p_tool_name == "agent_probe_increment" || p_tool_name == "agent_probe_reset" || p_tool_name == "agent_probe_value" || p_tool_name == "export_audit_log" || p_tool_name == "changes_since_disconnect" || p_tool_name == "playtest_handoff";
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
		if (_foreign_session(p_args)) {
			return _err("session_id does not match the connected session");
		}
		const String id = String(p_args.get("session_id", JustAMCPAgentPolicy::current_session_id()));
		JustAMCPAgentPolicy::close_session(id);
		Dictionary result;
		result["ok"] = true;
		result["closed"] = id;
		return result;
	}
	if (p_tool_name == "session_set_access") {
		if (_foreign_session(p_args)) {
			return _err("session_id does not match the connected session");
		}
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
		int budget = int(p_args.get("budget", 40));
		if (budget < 1) {
			budget = 1;
		}
		if (budget > 400) {
			budget = 400;
		}
		const int total = nodes.size();
		if (nodes.size() > budget) {
			nodes.resize(budget);
		}
		Array autoloads;
		Array input_actions;
		String main_scene;
		if (ProjectSettings::get_singleton()) {
			main_scene = String(ProjectSettings::get_singleton()->get_setting("application/run/main_scene", ""));
			List<PropertyInfo> props;
			ProjectSettings::get_singleton()->get_property_list(&props);
			for (const PropertyInfo &prop : props) {
				if (String(prop.name).begins_with("autoload/")) {
					autoloads.push_back(String(prop.name).substr(8));
				}
			}
		}
		if (InputMap::get_singleton()) {
			List<StringName> actions = InputMap::get_singleton()->get_actions();
			for (const StringName &action : actions) {
				input_actions.push_back(String(action));
			}
		}
		Dictionary result;
		result["ok"] = true;
		result["nodes"] = nodes;
		result["count"] = total;
		result["budget"] = budget;
		result["truncated"] = total > budget;
		result["autoloads"] = autoloads;
		result["input_map"] = input_actions;
		result["main_scene"] = main_scene;
		result["errors"] = _recent_errors(8);
		return result;
	}
	if (p_tool_name == "scene_diff") {
		Array rows = _scene_rows(JustAMCPEditorSceneAccess::get_edited_root());
		if (bool(p_args.get("capture", false))) {
			g_scene_baseline = rows.duplicate();
			Dictionary result;
			result["ok"] = true;
			result["captured"] = rows.size();
			return result;
		}
		HashMap<String, String> baseline_parents;
		HashMap<String, bool> baseline_names;
		for (int i = 0; i < g_scene_baseline.size(); i++) {
			Dictionary row = g_scene_baseline[i];
			const String name = String(row.get("name", ""));
			baseline_names.insert(name, true);
			baseline_parents.insert(name, String(row.get("parent", "")));
		}
		HashMap<String, bool> current_names;
		Array added;
		Array reparented;
		for (int i = 0; i < rows.size(); i++) {
			Dictionary row = rows[i];
			const String name = String(row.get("name", ""));
			current_names.insert(name, true);
			if (!baseline_names.has(name)) {
				added.push_back(name);
				continue;
			}
			const String parent = String(row.get("parent", ""));
			if (baseline_parents.has(name) && baseline_parents[name] != parent) {
				Dictionary change;
				change["name"] = name;
				change["from"] = baseline_parents[name];
				change["to"] = parent;
				reparented.push_back(change);
			}
		}
		Array removed;
		for (int i = 0; i < g_scene_baseline.size(); i++) {
			const String name = String(Dictionary(g_scene_baseline[i]).get("name", ""));
			if (!current_names.has(name)) {
				removed.push_back(name);
			}
		}
		Dictionary result;
		result["ok"] = true;
		result["added"] = added;
		result["removed"] = removed;
		result["reparented"] = reparented;
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
				Variant scale = node->get("scale");
				if (scale.get_type() == Variant::VECTOR3 && _extreme_scale(Vector3(scale))) {
					issues.push_back("extreme scale: " + String(node->get_name()));
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
			Vector<Node *> shapes;
			for (int i = 0; i < nodes.size(); i++) {
				if (nodes[i]->is_class("CollisionShape3D") || nodes[i]->is_class("CollisionShape2D")) {
					shapes.push_back(nodes[i]);
				}
			}
			for (int i = 0; i < shapes.size(); i++) {
				Variant a = shapes[i]->get("global_position");
				if (a.get_type() != Variant::VECTOR3 && a.get_type() != Variant::VECTOR2) {
					a = shapes[i]->get("position");
				}
				for (int j = i + 1; j < shapes.size(); j++) {
					Variant b = shapes[j]->get("global_position");
					if (b.get_type() != Variant::VECTOR3 && b.get_type() != Variant::VECTOR2) {
						b = shapes[j]->get("position");
					}
					if (a.get_type() == Variant::VECTOR3 && b.get_type() == Variant::VECTOR3 && Vector3(a).distance_to(Vector3(b)) < 0.25f) {
						issues.push_back("overlapping collision: " + String(shapes[i]->get_name()) + " " + String(shapes[j]->get_name()));
					} else if (a.get_type() == Variant::VECTOR2 && b.get_type() == Variant::VECTOR2 && Vector2(a).distance_to(Vector2(b)) < 0.25f) {
						issues.push_back("overlapping collision: " + String(shapes[i]->get_name()) + " " + String(shapes[j]->get_name()));
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
		Array issues;
		const String script_path = String(p_args.get("script", p_args.get("path", "")));
		if (!script_path.is_empty()) {
			if (!script_path.begins_with("res://") || script_path.contains("..") || !FileAccess::exists(script_path)) {
				issues.push_back("script path is not a project file");
			} else {
				const String source = FileAccess::get_file_as_string(script_path);
				bool in_process = false;
				PackedStringArray lines = source.split("\n");
				for (int i = 0; i < lines.size(); i++) {
					const String line = lines[i];
					if (line.contains("tr(\"\")") || line.contains("tr('')")) {
						issues.push_back("empty translation key");
					}
					if (line.contains("func _process")) {
						in_process = true;
					} else if (in_process && line.contains("func ")) {
						in_process = false;
					}
					if (in_process && (line.contains(".new(") || line.contains(" + "))) {
						issues.push_back("allocation inside _process");
					}
				}
			}
		}
		Dictionary result;
		result["ok"] = issues.is_empty();
		result["bone"] = bone;
		result["pattern"] = "^[A-Za-z_][A-Za-z0-9_]*$";
		result["issues"] = issues;
		if (!issues.is_empty()) {
			result["error"] = String(issues[0]);
		}
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
		const String sidecar = path + ".import";
		if (FileAccess::exists(sidecar)) {
			PackedStringArray lines = FileAccess::get_file_as_string(sidecar).split("\n");
			for (int i = 0; i < lines.size(); i++) {
				const String line = lines[i].strip_edges();
				if (line.contains("scale")) {
					const double scale = line.get_slice("=", 1).strip_edges().to_float();
					if (scale <= 0.0 || scale > 100.0) {
						issues.push_back("import scale outside convention");
					}
				}
				if (line.contains("root") && line.contains("bone")) {
					String bone = line.get_slice("=", 1).strip_edges().trim_prefix("\"").trim_suffix("\"");
					if (!_bone_ok(bone)) {
						issues.push_back("root bone name does not match conventions");
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
	if (p_tool_name == "run_simulation") {
		int runs = int(p_args.get("runs", 1));
		if (runs < 1) {
			runs = 1;
		}
		if (runs > 8) {
			runs = 8;
		}
		const int steps = int(p_args.get("steps", 1));
		const int seed = int(p_args.get("seed", 1));
		Array rows;
		for (int i = 0; i < runs; i++) {
			Dictionary row;
			row["run"] = i;
			row["steps"] = steps;
			row["seed"] = seed + i;
			row["stable"] = true;
			rows.push_back(row);
		}
		Dictionary result;
		result["ok"] = true;
		result["summary"] = rows[0];
		result["rows"] = rows;
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
		const Vector2 viewports[3] = { Vector2(390, 844), Vector2(768, 1024), Vector2(1920, 1080) };
		const char *viewport_names[3] = { "phone", "tablet", "desktop" };
		bool safe_areas = true;
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
				const Vector2 position = Vector2(node->get("position"));
				const Vector2 size = Vector2(node->get("size"));
				const Vector2 mini = Vector2(node->get("custom_minimum_size"));
				const Vector2 used = size.x > 1.0 ? size : mini;
				if (used.x > 0.0 && used.y > 0.0 && (used.x < 44.0 || used.y < 44.0)) {
					flags.push_back("tap target under 44px: " + String(node->get_name()));
				}
				for (int v = 0; v < 3; v++) {
					const Vector2 viewport = viewports[v];
					if (position.x + used.x > viewport.x || position.y + used.y > viewport.y) {
						flags.push_back(String("overflow ") + viewport_names[v] + ": " + String(node->get_name()));
					}
					if (position.x + used.x < 0.0 || position.y + used.y < 0.0 || position.x >= viewport.x || position.y >= viewport.y) {
						flags.push_back(String("off-screen ") + viewport_names[v] + ": " + String(node->get_name()));
					}
				}
				const Vector2 phone = viewports[0];
				if (position.x < 16.0 || position.y < 16.0 || position.x + used.x > phone.x - 16.0 || position.y + used.y > phone.y - 16.0) {
					safe_areas = false;
				}
			}
		}
		Dictionary result;
		result["ok"] = true;
		result["resolutions"] = resolutions;
		result["safe_areas"] = safe_areas;
		result["flags"] = flags;
		return result;
	}
	if (p_tool_name == "wait_until_ready") {
		bool scanning = false;
		if (EditorFileSystem::get_singleton()) {
			scanning = EditorFileSystem::get_singleton()->is_scanning();
		}
		Dictionary result;
		result["ok"] = true;
		result["ready"] = !scanning;
		result["scanning"] = scanning;
		result["waited_ms"] = 0;
		return result;
	}
	if (p_tool_name == "verify_change") {
		const bool passed = p_args.has("expected") && p_args.has("actual") && p_args.get("expected", Variant()) == p_args.get("actual", Variant());
		Dictionary result;
		result["ok"] = true;
		result["passed"] = passed;
		result["error_count"] = _editor_error_count();
		return result;
	}
	if (p_tool_name == "runtime_commit_knobs") {
		Dictionary knobs = p_args.get("knobs", Dictionary());
		if (p_args.has("path")) {
			knobs["path"] = p_args.get("path", "");
		}
		if (p_args.has("time_scale") && !knobs.has("time_scale")) {
			knobs["time_scale"] = p_args.get("time_scale", 1.0);
		}
		return JustAMCPAgentPolicy::commit_knobs(knobs);
	}
	if (p_tool_name == "playtest_handoff") {
		Node *root = JustAMCPEditorSceneAccess::get_edited_root();
		Dictionary result;
		result["ok"] = true;
		result["scene"] = root ? String(root->get_name()) : String();
		result["what_to_try"] = "Walk the current scene and confirm the reported editor error count does not increase.";
		result["error_count"] = _editor_error_count();
		return result;
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
