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
#include "justamcp_tool_executor.h"

#include "core/config/project_settings.h"
#include "core/input/input_map.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
#include "core/object/message_queue.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/os/time.h"
#include "core/string/char_utils.h"
#include "core/templates/hash_map.h"
#include "editor/editor_interface.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/file_system/editor_file_system.h"
#include "main/performance.h"
#include "scene/3d/skeleton_3d.h"
#include "scene/gui/control.h"
#include "scene/main/node.h"
#include "scene/main/viewport.h"
#include "scene/resources/mesh.h"
#include "scene/resources/packed_scene.h"
#include "servers/display/display_server.h"
#include "servers/xr/xr_interface.h"
#include "servers/xr/xr_positional_tracker.h"
#include "servers/xr/xr_server.h"

static Dictionary _err(const String &p_message) {
	Dictionary result;
	result["ok"] = false;
	result["error"] = p_message;
	return result;
}

static constexpr int k_scene_walk_cap = 4096;

static bool _note_empty_skeletons(Node *p_node, Array &r_issues, int &r_visited) {
	if (!p_node) {
		return false;
	}
	if (r_visited >= k_scene_walk_cap) {
		return true;
	}
	r_visited++;
	if (Skeleton3D *skeleton = Object::cast_to<Skeleton3D>(p_node)) {
		if (skeleton->get_bone_count() <= 0) {
			r_issues.push_back("skeleton has no bones");
		}
	}
	bool truncated = r_visited >= k_scene_walk_cap;
	for (int i = 0; i < p_node->get_child_count() && !truncated; i++) {
		truncated = _note_empty_skeletons(p_node->get_child(i), r_issues, r_visited);
	}
	return truncated;
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
	Dictionary result = _err("XR not enabled");
	result["available"] = false;
	return result;
}

static Dictionary _xr_unavailable(const String &p_reason) {
	Dictionary result = _err(p_reason);
	result["available"] = false;
	return result;
}

static Ref<XRPositionalTracker> _first_tracker(int p_mask) {
	XRServer *xr = XRServer::get_singleton();
	if (!xr) {
		return Ref<XRPositionalTracker>();
	}
	const Dictionary trackers = xr->get_trackers(p_mask);
	const Array keys = trackers.keys();
	for (int i = 0; i < keys.size(); i++) {
		Ref<XRPositionalTracker> tracker = trackers[keys[i]];
		if (tracker.is_valid()) {
			return tracker;
		}
	}
	return Ref<XRPositionalTracker>();
}

static bool _set_tracker_pose(const Ref<XRPositionalTracker> &p_tracker, const Dictionary &p_args) {
	if (p_tracker.is_null() || !XRServer::get_singleton()) {
		return false;
	}
	Vector3 origin;
	if (p_args.get("position", Variant()).get_type() == Variant::VECTOR3) {
		origin = p_args["position"];
	}
	Transform3D pose;
	pose.origin = origin;
	StringName pose_name = "default";
	const PackedStringArray suggested = XRServer::get_singleton()->get_suggested_pose_names(p_tracker->get_tracker_name());
	if (!suggested.is_empty()) {
		pose_name = suggested[0];
	}
	p_tracker->set_pose(pose_name, pose, Vector3(), Vector3());
	return true;
}

static bool _step_processing_nodes(Node *p_node, double p_delta, int &r_visited) {
	if (!p_node || !p_node->is_inside_tree()) {
		return false;
	}
	if (r_visited >= k_scene_walk_cap) {
		return true;
	}
	r_visited++;
	if (p_node->get_script_instance()) {
		if (p_node->is_processing() && p_node->get_script_instance()->has_method("_process")) {
			p_node->get_script_instance()->call("_process", p_delta);
		}
		if (p_node->is_physics_processing() && p_node->get_script_instance()->has_method("_physics_process")) {
			p_node->get_script_instance()->call("_physics_process", p_delta);
		}
	}
	bool truncated = r_visited >= k_scene_walk_cap;
	for (int i = 0; i < p_node->get_child_count() && !truncated; i++) {
		truncated = _step_processing_nodes(p_node->get_child(i), p_delta, r_visited);
	}
	return truncated;
}

static bool _collect_scene_nodes(Node *p_root, Vector<Node *> &r_nodes) {
	r_nodes.clear();
	if (!p_root) {
		return false;
	}
	r_nodes.push_back(p_root);
	bool truncated = false;
	for (int i = 0; i < r_nodes.size(); i++) {
		Node *node = r_nodes[i];
		for (int c = 0; c < node->get_child_count(); c++) {
			if (r_nodes.size() >= k_scene_walk_cap) {
				truncated = true;
				break;
			}
			r_nodes.push_back(node->get_child(c));
		}
	}
	return truncated;
}

static int _collect_map_nodes(Node *p_root, int p_budget, Array &r_nodes) {
	r_nodes.clear();
	if (!p_root) {
		return 0;
	}
	Vector<Node *> nodes;
	nodes.push_back(p_root);
	for (int i = 0; i < nodes.size(); i++) {
		Node *node = nodes[i];
		if (r_nodes.size() < p_budget) {
			Dictionary row;
			row["name"] = String(node->get_name());
			row["class"] = node->get_class();
			row["path"] = String(node->get_path());
			r_nodes.push_back(row);
		}
		for (int c = 0; c < node->get_child_count(); c++) {
			if (nodes.size() >= k_scene_walk_cap) {
				break;
			}
			nodes.push_back(node->get_child(c));
		}
	}
	return nodes.size();
}

static void _note_shape_overlaps(const Vector<Node *> &p_shapes, Array &r_issues) {
	struct ShapePoint {
		Vector3 position;
		bool is_3d = false;
		bool valid = false;
	};
	Vector<ShapePoint> points;
	HashMap<String, Vector<int>> bins;
	for (int i = 0; i < p_shapes.size(); i++) {
		Variant pos = p_shapes[i]->get("global_position");
		if (pos.get_type() != Variant::VECTOR3 && pos.get_type() != Variant::VECTOR2) {
			pos = p_shapes[i]->get("position");
		}
		ShapePoint point;
		if (pos.get_type() == Variant::VECTOR3) {
			point.position = Vector3(pos);
			point.is_3d = true;
			point.valid = true;
		} else if (pos.get_type() == Variant::VECTOR2) {
			const Vector2 flat = Vector2(pos);
			point.position = Vector3(flat.x, flat.y, 0.0);
			point.valid = true;
		}
		points.push_back(point);
		if (!point.valid) {
			continue;
		}
		const String key = String::num_int64(int(Math::floor(point.position.x / 0.25))) + "," + String::num_int64(int(Math::floor(point.position.y / 0.25))) + "," + String::num_int64(int(Math::floor(point.position.z / 0.25)));
		if (!bins.has(key)) {
			bins.insert(key, Vector<int>());
		}
		bins[key].push_back(i);
	}
	for (int i = 0; i < points.size(); i++) {
		if (!points[i].valid) {
			continue;
		}
		const Vector3 pa = points[i].position;
		const int cx = int(Math::floor(pa.x / 0.25));
		const int cy = int(Math::floor(pa.y / 0.25));
		const int cz = int(Math::floor(pa.z / 0.25));
		for (int ox = -1; ox <= 1; ox++) {
			for (int oy = -1; oy <= 1; oy++) {
				for (int oz = -1; oz <= 1; oz++) {
					const String key = String::num_int64(cx + ox) + "," + String::num_int64(cy + oy) + "," + String::num_int64(cz + oz);
					if (!bins.has(key)) {
						continue;
					}
					const Vector<int> &there = bins[key];
					int compared = 0;
					for (int b = 0; b < there.size() && compared < 8; b++) {
						const int other_index = there[b];
						if (other_index <= i || !points[other_index].valid || points[other_index].is_3d != points[i].is_3d) {
							continue;
						}
						compared++;
						const Vector3 pb = points[other_index].position;
						const bool overlap = points[i].is_3d ? pa.distance_to(pb) < 0.25f : Vector2(pa.x, pa.y).distance_to(Vector2(pb.x, pb.y)) < 0.25f;
						if (overlap) {
							r_issues.push_back("overlapping collision: " + String(p_shapes[i]->get_name()) + " " + String(p_shapes[other_index]->get_name()));
						}
					}
				}
			}
		}
	}
}

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

static Dictionary _feel_metrics() {
	Dictionary metrics;
	Performance *performance = Performance::get_singleton();
	if (!performance) {
		metrics["available"] = false;
		metrics["frame_ms"] = Variant();
		metrics["physics_frame_ms"] = Variant();
		metrics["input_latency_available"] = false;
		return metrics;
	}
	metrics["available"] = true;
	metrics["frame_ms"] = performance->get_monitor(Performance::TIME_PROCESS) * 1000.0;
	metrics["physics_frame_ms"] = performance->get_monitor(Performance::TIME_PHYSICS_PROCESS) * 1000.0;
	metrics["input_latency_available"] = false;
	return metrics;
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

static Array _scene_rows(Node *p_root, bool &r_truncated) {
	Array rows;
	Vector<Node *> nodes;
	r_truncated = _collect_scene_nodes(p_root, nodes);
	for (int i = 0; i < nodes.size(); i++) {
		rows.push_back(_scene_row(nodes[i]));
	}
	return rows;
}

static bool _extreme_scale(const Vector3 &p_scale) {
	const real_t axes[3] = { p_scale.x, p_scale.y, p_scale.z };
	for (int i = 0; i < 3; i++) {
		const real_t axis = Math::abs(axes[i]);
		if (axis > 100.0 || (axis > 0.0 && axis < 0.01)) {
			return true;
		}
	}
	return false;
}

bool JustAMCPAgentGapTools::handles(const String &p_tool_name) {
	return p_tool_name == "session_set_access" || p_tool_name == "session_capabilities" || p_tool_name == "session_open" || p_tool_name == "session_close" || p_tool_name == "claim_scene" || p_tool_name == "claim_subtree" || p_tool_name == "list_claims" || p_tool_name == "release_claim" || p_tool_name == "checkpoint" || p_tool_name == "list_checkpoints" || p_tool_name == "diff_checkpoint" || p_tool_name == "restore_checkpoint" || p_tool_name == "apply_change_plan" || p_tool_name == "revert_change_plan" || p_tool_name == "what_changed_since" || p_tool_name == "scene_diff" || p_tool_name == "project_map" || p_tool_name == "spatial_scene_relations" || p_tool_name == "validate_scene_grounding" || p_tool_name == "validate_conventions" || p_tool_name == "validate_import" || p_tool_name == "run_simulation" || p_tool_name == "runtime_feel_metrics" || p_tool_name == "runtime_integration_report" || p_tool_name == "ui_resolution_sweep" || p_tool_name == "wait_until_ready" || p_tool_name == "verify_change" || p_tool_name == "verify_game_change" || p_tool_name == "runtime_commit_knobs" || p_tool_name == "xr_set_head_pose" || p_tool_name == "xr_set_controller" || p_tool_name == "xr_capture" || p_tool_name == "recipe_add_player_controller" || p_tool_name == "client_config" || p_tool_name == "write_client_config" || p_tool_name == "agent_probe_increment" || p_tool_name == "agent_probe_reset" || p_tool_name == "agent_probe_value" || p_tool_name == "export_audit_log" || p_tool_name == "changes_since_disconnect" || p_tool_name == "playtest_handoff";
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
		int budget = int(p_args.get("budget", 40));
		if (budget < 1) {
			budget = 1;
		}
		if (budget > 400) {
			budget = 400;
		}
		Array nodes;
		const int total = _collect_map_nodes(JustAMCPEditorSceneAccess::get_edited_root(), budget, nodes);
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
			const TypedArray<StringName> actions = InputMap::get_singleton()->get_actions();
			for (int action_index = 0; action_index < actions.size(); action_index++) {
				const StringName action = actions[action_index];
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
		bool truncated = false;
		Array rows = _scene_rows(JustAMCPEditorSceneAccess::get_edited_root(), truncated);
		if (bool(p_args.get("capture", false))) {
			JustAMCPAgentPolicy::store_scene_baseline(rows);
			Dictionary result;
			result["ok"] = true;
			result["captured"] = rows.size();
			result["truncated"] = truncated;
			return result;
		}
		const Array baseline = JustAMCPAgentPolicy::copy_scene_baseline();
		HashMap<String, String> baseline_parents;
		HashMap<String, bool> baseline_names;
		for (int i = 0; i < baseline.size(); i++) {
			Dictionary row = baseline[i];
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
		for (int i = 0; i < baseline.size(); i++) {
			const String name = String(Dictionary(baseline[i]).get("name", ""));
			if (!current_names.has(name)) {
				removed.push_back(name);
			}
		}
		Dictionary result;
		result["ok"] = true;
		result["added"] = added;
		result["removed"] = removed;
		result["reparented"] = reparented;
		result["truncated"] = truncated;
		return result;
	}
	if (p_tool_name == "spatial_scene_relations") {
		Array relations;
		bool truncated = false;
		Node *root = JustAMCPEditorSceneAccess::get_edited_root();
		if (root) {
			Vector<Node *> nodes;
			truncated = _collect_scene_nodes(root, nodes);
			HashMap<String, Vector<int>> bins;
			HashMap<String, Vector<String>> scripts;
			for (int i = 0; i < nodes.size(); i++) {
				Variant pos = nodes[i]->get("position");
				if (pos.get_type() == Variant::VECTOR3) {
					const Vector3 point = Vector3(pos);
					const String key = String::num_int64(int(Math::floor(point.x / 2.0))) + "," + String::num_int64(int(Math::floor(point.y / 2.0))) + "," + String::num_int64(int(Math::floor(point.z / 2.0)));
					if (!bins.has(key)) {
						bins.insert(key, Vector<int>());
					}
					bins[key].push_back(i);
				}
				Ref<Script> script = nodes[i]->get_script();
				if (script.is_valid() && !script->get_path().is_empty()) {
					const String path = script->get_path();
					if (!scripts.has(path)) {
						scripts.insert(path, Vector<String>());
					}
					scripts[path].push_back(String(nodes[i]->get_name()));
				}
				if (String(nodes[i]->get_name()).contains("Bone") || String(nodes[i]->get_class()).contains("Bone")) {
					Dictionary row;
					row["relation"] = "attached-to-bone";
					row["from"] = String(nodes[i]->get_name());
					relations.push_back(row);
				}
			}
			for (const KeyValue<String, Vector<int>> &bin : bins) {
				const Vector<int> &here = bin.value;
				for (int a = 0; a < here.size(); a++) {
					const int ia = here[a];
					const Vector3 pa = Vector3(nodes[ia]->get("position"));
					const int cx = int(Math::floor(pa.x / 2.0));
					const int cy = int(Math::floor(pa.y / 2.0));
					const int cz = int(Math::floor(pa.z / 2.0));
					for (int ox = -1; ox <= 1; ox++) {
						for (int oy = -1; oy <= 1; oy++) {
							for (int oz = -1; oz <= 1; oz++) {
								const String key = String::num_int64(cx + ox) + "," + String::num_int64(cy + oy) + "," + String::num_int64(cz + oz);
								if (!bins.has(key)) {
									continue;
								}
								const Vector<int> &there = bins[key];
								const bool same_cell = ox == 0 && oy == 0 && oz == 0;
								int compared = 0;
								for (int b = same_cell ? a + 1 : 0; b < there.size() && compared < 8; b++) {
									const int ib = there[b];
									if (ib <= ia) {
										continue;
									}
									compared++;
									const Vector3 pb = Vector3(nodes[ib]->get("position"));
									if (Math::abs(pa.y - pb.y) > 0.4 && Math::abs(pa.x - pb.x) < 1.5 && Math::abs(pa.z - pb.z) < 1.5) {
										Dictionary row;
										row["relation"] = "rests-on";
										row["from"] = String(nodes[ia]->get_name());
										row["to"] = String(nodes[ib]->get_name());
										relations.push_back(row);
									} else if (pa.distance_to(pb) < 2.0) {
										Dictionary row;
										row["relation"] = "near";
										row["from"] = String(nodes[ia]->get_name());
										row["to"] = String(nodes[ib]->get_name());
										relations.push_back(row);
									}
								}
							}
						}
					}
				}
			}
			for (const KeyValue<String, Vector<String>> &group : scripts) {
				const Vector<String> &names = group.value;
				for (int i = 1; i < names.size(); i++) {
					Dictionary row;
					row["relation"] = "shared script";
					row["from"] = names[i];
					row["to"] = names[0];
					relations.push_back(row);
				}
			}
		}
		Dictionary result;
		result["ok"] = true;
		result["relations"] = relations;
		result["truncated"] = truncated;
		return result;
	}
	if (p_tool_name == "validate_scene_grounding") {
		Array issues;
		bool truncated = false;
		Node *root = JustAMCPEditorSceneAccess::get_edited_root();
		if (root) {
			Vector<Node *> nodes;
			truncated = _collect_scene_nodes(root, nodes);
			for (int i = 0; i < nodes.size(); i++) {
				Node *node = nodes[i];
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
			_note_shape_overlaps(shapes, issues);
		}
		Dictionary result;
		result["ok"] = true;
		result["valid"] = issues.is_empty();
		result["issues"] = issues;
		result["truncated"] = truncated;
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
		Ref<Resource> loaded = ResourceLoader::exists(path) ? ResourceLoader::load(path) : Ref<Resource>();
		Ref<Mesh> mesh = loaded.is_valid() ? Ref<Mesh>(Object::cast_to<Mesh>(loaded.ptr())) : Ref<Mesh>();
		if (mesh.is_valid()) {
			if (mesh->get_surface_count() <= 0) {
				issues.push_back("mesh has no surfaces");
			} else {
				const Array arrays = mesh->surface_get_arrays(0);
				const bool missing_uv = arrays.size() <= Mesh::ARRAY_TEX_UV || arrays[Mesh::ARRAY_TEX_UV].get_type() == Variant::NIL || (arrays[Mesh::ARRAY_TEX_UV].get_type() == Variant::PACKED_VECTOR2_ARRAY && PackedVector2Array(arrays[Mesh::ARRAY_TEX_UV]).is_empty());
				if (missing_uv) {
					issues.push_back("mesh missing UV");
				}
			}
			const Vector3 size = mesh->get_aabb().size;
			const real_t longest = MAX(size.x, MAX(size.y, size.z));
			if (longest <= 0.0 || longest > 1000.0) {
				issues.push_back("mesh scale outside convention");
			}
		}
		bool truncated = false;
		Ref<PackedScene> packed = loaded;
		if (packed.is_valid()) {
			Node *instance = packed->instantiate();
			int visited = 0;
			truncated = _note_empty_skeletons(instance, issues, visited);
			if (instance) {
				memdelete(instance);
			}
		}
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
		result["truncated"] = truncated;
		return result;
	}
	if (p_tool_name == "run_simulation") {
		Array values;
		if (p_args.has("values") && p_args.get("values", Variant()).get_type() == Variant::ARRAY) {
			values = p_args["values"];
		}
		int runs = int(p_args.get("runs", 1));
		if (!values.is_empty()) {
			runs = values.size();
		}
		if (runs < 1) {
			runs = 1;
		}
		if (runs > 8) {
			runs = 8;
		}
		int steps = int(p_args.get("steps", 1));
		if (steps < 1) {
			steps = 1;
		}
		if (steps > 32) {
			steps = 32;
		}
		const int seed = int(p_args.get("seed", 1));
		const bool on_main = Thread::is_main_thread();
		Array rows;
		for (int i = 0; i < runs; i++) {
			const int row_seed = seed + i;
			bool inside = false;
			bool stepped = false;
			bool step_truncated = false;
			if (on_main) {
				Math::seed(uint64_t(row_seed < 0 ? 0 : row_seed));
				Node *root = JustAMCPEditorSceneAccess::get_edited_root();
				if (root) {
					const uint64_t started_usec = Time::get_singleton()->get_ticks_usec();
					const uint64_t budget_usec = 50000;
					for (int s = 0; s < steps; s++) {
						if (Time::get_singleton()->get_ticks_usec() - started_usec >= budget_usec) {
							break;
						}
						if (root->is_inside_tree()) {
							int visited = 0;
							if (_step_processing_nodes(root, 1.0 / 60.0, visited)) {
								step_truncated = true;
							}
						}
					}
					inside = root->is_inside_tree();
					stepped = true;
				}
			}
			Dictionary row;
			row["run"] = i;
			row["steps"] = steps;
			row["seed"] = row_seed;
			row["stepped"] = stepped;
			row["inside_tree"] = inside;
			row["stable"] = inside;
			row["truncated"] = step_truncated;
			if (i < values.size()) {
				row["value"] = values[i];
			}
			rows.push_back(row);
		}
		Dictionary result;
		result["ok"] = true;
		result["summary"] = rows[0];
		result["rows"] = rows;
		return result;
	}
	if (p_tool_name == "runtime_feel_metrics") {
		Dictionary result;
		result["ok"] = true;
		result["metrics"] = _feel_metrics();
		return result;
	}
	if (p_tool_name == "runtime_integration_report") {
		Node *root = JustAMCPEditorSceneAccess::get_edited_root();
		const String scene_name = root ? String(root->get_name()) : String();
		const int errors = _editor_error_count();
		const Dictionary metrics = _feel_metrics();
		Dictionary result;
		result["ok"] = true;
		result["scene"] = scene_name;
		result["error_count"] = errors;
		result["metrics"] = metrics;
		result["report"] = "scene=" + scene_name + " errors=" + String::num_int64(errors);
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
		bool truncated = false;
		Node *root = JustAMCPEditorSceneAccess::get_edited_root();
		if (root) {
			Vector<Node *> nodes;
			truncated = _collect_scene_nodes(root, nodes);
			for (int i = 0; i < nodes.size(); i++) {
				Node *node = nodes[i];
				Control *control = Object::cast_to<Control>(node);
				if (!control) {
					continue;
				}
				const Rect2 global_rect = control->get_global_rect();
				Vector2 position = global_rect.position;
				Vector2 used = global_rect.size;
				if (used.x <= 1.0 || used.y <= 1.0) {
					position = control->get_global_position();
					const Vector2 size = control->get_size();
					const Vector2 mini = control->get_custom_minimum_size();
					used = size.x > 1.0 ? size : mini;
				}
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
		result["truncated"] = truncated;
		return result;
	}
	if (p_tool_name == "wait_until_ready") {
		const uint64_t started_msec = OS::get_singleton()->get_ticks_msec();
		bool scanning = EditorFileSystem::get_singleton() && EditorFileSystem::get_singleton()->is_scanning();
		while (scanning && OS::get_singleton()->get_ticks_msec() - started_msec < 2000) {
			if (MessageQueue::get_singleton()) {
				MessageQueue::get_singleton()->flush();
			}
			OS::get_singleton()->delay_usec(10000);
			scanning = EditorFileSystem::get_singleton() && EditorFileSystem::get_singleton()->is_scanning();
		}
		Dictionary result;
		result["ok"] = true;
		result["ready"] = !scanning;
		result["scanning"] = scanning;
		result["waited_ms"] = OS::get_singleton()->get_ticks_msec() - started_msec;
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
	if (p_tool_name == "verify_game_change") {
		int duration_ms = int(p_args.get("duration_ms", 500));
		duration_ms = CLAMP(duration_ms, 1, 2000);
		Dictionary play_args;
		play_args["duration_ms"] = duration_ms;
		const String scene_path = String(p_args.get("scene_path", p_args.get("scene", "")));
		if (!scene_path.is_empty()) {
			play_args["scene_path"] = scene_path;
		}
		const int errors_before = _editor_error_count();
		JustAMCPToolExecutor *executor = JustAMCPToolExecutor::get_active_instance();
		if (!executor) {
			Dictionary result;
			result["ok"] = false;
			result["played"] = false;
			result["passed"] = false;
			result["error"] = "Failed to evaluate play request.";
			return result;
		}
		const Dictionary played_result = executor->execute_tool("blazium_editor_play_scene", play_args);
		const bool played = bool(played_result.get("ok", false));
		executor->execute_tool("blazium_editor_stop_play", Dictionary());
		const int errors_after = _editor_error_count();
		bool expected_ok = true;
		if (p_args.has("expected") && p_args.has("actual")) {
			expected_ok = p_args.get("expected", Variant()) == p_args.get("actual", Variant());
		}
		Dictionary result;
		result["ok"] = played;
		result["played"] = played;
		result["passed"] = played && errors_after <= errors_before && expected_ok;
		result["error_count"] = errors_after;
		if (!played) {
			result["error"] = played_result.get("error", "Failed to evaluate play request.");
		}
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
	if (p_tool_name == "xr_set_head_pose" || p_tool_name == "xr_set_controller") {
		if (!_xr_enabled()) {
			return _xr_disabled();
		}
		const int mask = p_tool_name == "xr_set_head_pose" ? XRServer::TRACKER_HEAD : XRServer::TRACKER_CONTROLLER;
		const Ref<XRPositionalTracker> tracker = _first_tracker(mask);
		if (tracker.is_null() || !_set_tracker_pose(tracker, p_args)) {
			return _xr_unavailable(p_tool_name == "xr_set_head_pose" ? String("XR head tracker unavailable") : String("XR controller tracker unavailable"));
		}
		Dictionary result;
		result["ok"] = true;
		result["available"] = true;
		result["xr"] = true;
		result["tracker"] = String(tracker->get_tracker_name());
		return result;
	}
	if (p_tool_name == "xr_capture") {
		if (!_xr_enabled()) {
			return _xr_disabled();
		}
		Ref<Image> image;
		if (EditorInterface::get_singleton()) {
			SubViewport *viewport = EditorInterface::get_singleton()->get_editor_viewport_3d(0);
			if (viewport && viewport->get_texture().is_valid()) {
				image = viewport->get_texture()->get_image();
			}
		}
		if ((image.is_null() || image->is_empty()) && DisplayServer::get_singleton()) {
			image = DisplayServer::get_singleton()->screen_get_image(DisplayServer::get_singleton()->get_primary_screen());
		}
		if (image.is_null() || image->is_empty()) {
			return _xr_unavailable("XR capture unavailable");
		}
		Dictionary result;
		result["ok"] = true;
		result["available"] = true;
		result["xr"] = true;
		result["width"] = image->get_width();
		result["height"] = image->get_height();
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
					Object *shape_created = ClassDB::instantiate("CollisionShape2D");
					Object *camera_created = ClassDB::instantiate("Camera2D");
					Node *shape = Object::cast_to<Node>(shape_created);
					Node *camera = Object::cast_to<Node>(camera_created);
					if (shape) {
						shape->set_name("CollisionShape2D");
					} else if (shape_created) {
						memdelete(shape_created);
					}
					if (camera) {
						camera->set_name("Camera2D");
					} else if (camera_created) {
						memdelete(camera_created);
					}
					EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
					if (undo_redo) {
						undo_redo->create_action("AI Local: Add Player [" + JustAMCPAgentPolicy::current_session_id() + "]", UndoRedo::MERGE_DISABLE);
						undo_redo->add_do_method(root, "add_child", player, true);
						undo_redo->add_do_method(player, "set_owner", root);
						undo_redo->add_do_reference(player);
						if (shape) {
							undo_redo->add_do_method(player, "add_child", shape, true);
							undo_redo->add_do_method(shape, "set_owner", root);
							undo_redo->add_do_reference(shape);
						}
						if (camera) {
							undo_redo->add_do_method(player, "add_child", camera, true);
							undo_redo->add_do_method(camera, "set_owner", root);
							undo_redo->add_do_reference(camera);
						}
						undo_redo->add_undo_method(root, "remove_child", player);
						undo_redo->commit_action(true);
					} else {
						root->add_child(player);
						player->set_owner(root);
						if (shape) {
							player->add_child(shape);
							shape->set_owner(root);
						}
						if (camera) {
							player->add_child(camera);
							camera->set_owner(root);
						}
						Array added_ids;
						added_ids.push_back(int64_t(player->get_instance_id()));
						JustAMCPAgentPolicy::note_batch_undo(Array(), added_ids);
					}
				} else if (created) {
					memdelete(created);
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
