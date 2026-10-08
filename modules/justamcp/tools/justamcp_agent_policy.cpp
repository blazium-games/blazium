/**************************************************************************/
/*  justamcp_agent_policy.cpp                                             */
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

#include "justamcp_agent_policy.h"

#include "../justamcp_mcp_spec.h"
#include "../justamcp_server.h"
#include "justamcp_agent_helpers.h"
#include "justamcp_json_rpc_router.h"
#include "justamcp_readonly_tools.h"
#include "justamcp_settings_resolver.h"
#include "resources/justamcp_resource_json.h"

#include "core/config/project_settings.h"
#include "core/math/math_funcs.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/os/time.h"
#include "core/templates/hash_map.h"

struct JustAMCPAgentSession {
	String id;
	String name;
	bool read_only = false;
	bool has_read = true;
	int revision = 0;
	int calls = 0;
	HashMap<String, int> failures;
	String last_checkpoint;
};

struct JustAMCPClaim {
	String path;
	String session_id;
	String holder;
	bool subtree = false;
};

struct JustAMCPQueuedWrite {
	String path;
	String session_id;
	String tool;
	Dictionary args;
};

struct JustAMCPUndoEntry {
	bool file = false;
	String path;
	String previous;
	int probe_value = 0;
};

struct JustAMCPCheckpoint {
	String id;
	String git_head;
	HashMap<String, String> files;
};

struct JustAMCPChangePlan {
	String id;
	String tool;
	Dictionary args;
	String session_id;
	int revision_at = 0;
	bool applied = false;
	int probe_before = 0;
};

static HashMap<String, JustAMCPAgentSession> g_sessions;
static Vector<String> g_session_stack;
static thread_local int g_policy_depth = 0;
static int g_grouped_undo = 0;
static int g_probe = 0;
static Vector<JustAMCPUndoEntry> g_undo;
static Vector<String> g_tool_names;
static HashMap<String, JustAMCPClaim> g_claims;
static Vector<JustAMCPQueuedWrite> g_queue;
static HashMap<String, JustAMCPCheckpoint> g_checkpoints;
static Vector<String> g_checkpoint_order;
static HashMap<String, Dictionary> g_idempotency;
static HashMap<String, JustAMCPChangePlan> g_plans;
static Array g_audit;
static Dictionary g_knobs;
static String g_screenshot_summary = "No screenshot captured yet.";
static String g_bearer;
static bool g_applying_queue = false;

static String _internal_name(const String &p_tool_name) {
	String name = p_tool_name;
	if (name.begins_with("blazium_")) {
		name = name.substr(8);
	}
	return justamcp_remap_tool_name(name);
}

static bool _on_main_thread() {
	return Thread::is_main_thread();
}

static Dictionary _err(const String &p_message) {
	Dictionary result;
	result["ok"] = false;
	result["error"] = p_message;
	return result;
}

static bool _failed(const Dictionary &p_result) {
	if (p_result.has("ok")) {
		return !bool(p_result.get("ok", false));
	}
	return p_result.has("error");
}

static bool _control_tool(const String &p_name) {
	return p_name == "session_set_access" || p_name == "session_open" || p_name == "session_close";
}

static bool _read_tool(const String &p_name) {
	return JustAMCPReadonlyTools::is_readonly_tool(p_name);
}

static bool _safe_project_path(const String &p_path) {
	if (!(p_path.begins_with("res://") || p_path.begins_with("user://"))) {
		return false;
	}
	return !p_path.contains("..");
}

static String _claim_path_from_args(const Dictionary &p_args) {
	const String path = String(p_args.get("path", p_args.get("scene_path", p_args.get("claim_path", ""))));
	if (!_safe_project_path(path)) {
		return String();
	}
	return path;
}

static JustAMCPAgentSession &_ensure_editor() {
	if (!g_sessions.has("editor")) {
		JustAMCPAgentSession session;
		session.id = "editor";
		session.name = "editor";
		session.read_only = false;
		session.has_read = true;
		g_sessions.insert("editor", session);
	}
	return g_sessions["editor"];
}

static JustAMCPAgentSession &_session(const String &p_id) {
	if (p_id.is_empty() || p_id == "editor") {
		return _ensure_editor();
	}
	if (!g_sessions.has(p_id)) {
		JustAMCPAgentPolicy::open_session(p_id, p_id, true);
	}
	return g_sessions[p_id];
}

static String _resolve_session_id(const Dictionary &p_args) {
	String id = String(p_args.get("_session_id", p_args.get("session_id", "")));
	if (id.is_empty() && !g_session_stack.is_empty()) {
		id = g_session_stack[g_session_stack.size() - 1];
	}
	if (id.is_empty()) {
		id = "editor";
	}
	_session(id);
	return id;
}

static int _edit_distance(const String &p_a, const String &p_b) {
	const int n = p_a.length();
	const int m = p_b.length();
	if (n > 80 || m > 80 || Math::abs(n - m) > 5) {
		return 99;
	}
	Vector<int> prev;
	Vector<int> cur;
	prev.resize(m + 1);
	cur.resize(m + 1);
	for (int j = 0; j <= m; j++) {
		prev.write[j] = j;
	}
	for (int i = 1; i <= n; i++) {
		cur.write[0] = i;
		for (int j = 1; j <= m; j++) {
			const int cost = p_a[i - 1] == p_b[j - 1] ? 0 : 1;
			const int del = prev[j] + 1;
			const int ins = cur[j - 1] + 1;
			const int sub = prev[j - 1] + cost;
			int best = del < ins ? del : ins;
			if (sub < best) {
				best = sub;
			}
			cur.write[j] = best;
		}
		prev = cur;
	}
	return prev[m];
}

static void _push_undo_probe(int p_previous) {
	JustAMCPUndoEntry entry;
	entry.file = false;
	entry.probe_value = p_previous;
	g_undo.push_back(entry);
}

static void _push_file_snapshot(const String &p_path) {
	if (!_safe_project_path(p_path)) {
		return;
	}
	JustAMCPUndoEntry entry;
	entry.file = true;
	entry.path = p_path;
	entry.previous = FileAccess::exists(p_path) ? FileAccess::get_file_as_string(p_path) : String();
	g_undo.push_back(entry);
}

static String _git_head() {
	List<String> args;
	args.push_back("rev-parse");
	args.push_back("HEAD");
	String output;
	int exit_code = 1;
	if (OS::get_singleton()->execute("git", args, &output, &exit_code, true) != OK || exit_code != 0) {
		return String();
	}
	return output.strip_edges();
}

static void _apply_queued(const String &p_path) {
	Vector<JustAMCPQueuedWrite> remain;
	g_applying_queue = true;
	for (int i = 0; i < g_queue.size(); i++) {
		const JustAMCPQueuedWrite &item = g_queue[i];
		if (!p_path.is_empty() && item.path != p_path) {
			remain.push_back(item);
			continue;
		}
		if (_internal_name(item.tool) == "agent_probe_increment") {
			JustAMCPAgentPolicy::probe_increment();
		}
	}
	g_queue = remain;
	g_applying_queue = false;
}

static void _drop_claims_for_session(const String &p_id) {
	Vector<String> released;
	Vector<String> keys;
	for (const KeyValue<String, JustAMCPClaim> &entry : g_claims) {
		keys.push_back(entry.key);
	}
	for (int i = 0; i < keys.size(); i++) {
		if (g_claims[keys[i]].session_id == p_id) {
			released.push_back(keys[i]);
			g_claims.erase(keys[i]);
		}
	}
	Vector<JustAMCPQueuedWrite> remain;
	for (int i = 0; i < g_queue.size(); i++) {
		if (g_queue[i].session_id != p_id) {
			remain.push_back(g_queue[i]);
		}
	}
	g_queue = remain;
	for (int i = 0; i < released.size(); i++) {
		_apply_queued(released[i]);
	}
}

static Dictionary _session_dict(const JustAMCPAgentSession &p_session) {
	Dictionary session;
	session["session_id"] = p_session.id;
	session["name"] = p_session.name;
	session["read_only"] = p_session.read_only;
	session["has_read"] = p_session.has_read;
	session["revision"] = p_session.revision;
	session["calls"] = p_session.calls;
	session["is_active"] = true;
	return session;
}

String JustAMCPAgentPolicy::instance_bearer() {
	if (g_bearer.is_empty()) {
		g_bearer = "jamcp-" + String::num_uint64(OS::get_singleton() ? OS::get_singleton()->get_ticks_usec() : 1);
	}
	return g_bearer;
}

bool JustAMCPAgentPolicy::bearer_authorizes(const String &p_authorization) {
	String token = p_authorization.strip_edges();
	if (token.begins_with("Bearer ")) {
		token = token.substr(7).strip_edges();
	}
	return !token.is_empty() && token == instance_bearer();
}

bool JustAMCPAgentPolicy::require_local_bearer() {
	return JustAMCPSettingsResolver::resolve_bool("blazium/justamcp/require_local_bearer", true);
}

bool JustAMCPAgentPolicy::save_requires_confirmation() {
	return JustAMCPSettingsResolver::resolve_bool("blazium/justamcp/save_requires_confirmation", false);
}

void JustAMCPAgentPolicy::open_session(const String &p_id, const String &p_name, bool p_from_initialize) {
	if (p_id.is_empty()) {
		return;
	}
	JustAMCPAgentSession session;
	session.id = p_id;
	session.name = p_name.is_empty() ? p_id : p_name;
	const bool read_only = p_from_initialize && JustAMCPSettingsResolver::resolve_bool("blazium/justamcp/session_starts_read_only", true);
	session.read_only = read_only;
	session.has_read = !read_only;
	g_sessions.insert(p_id, session);
}

void JustAMCPAgentPolicy::close_session(const String &p_id) {
	_drop_claims_for_session(p_id);
	g_sessions.erase(p_id);
}

Array JustAMCPAgentPolicy::list_sessions() {
	_ensure_editor();
	Array sessions;
	for (const KeyValue<String, JustAMCPAgentSession> &entry : g_sessions) {
		sessions.push_back(_session_dict(entry.value));
	}
	return sessions;
}

String JustAMCPAgentPolicy::current_session_id() {
	if (!g_session_stack.is_empty()) {
		return g_session_stack[g_session_stack.size() - 1];
	}
	return "editor";
}

Dictionary JustAMCPAgentPolicy::session_state(const String &p_id) {
	return _session_dict(_session(p_id.is_empty() ? current_session_id() : p_id));
}

Dictionary JustAMCPAgentPolicy::set_access(const String &p_id, const String &p_mode) {
	const String id = p_id.is_empty() ? current_session_id() : p_id;
	JustAMCPAgentSession &session = _session(id);
	if (p_mode == "write") {
		session.read_only = false;
	} else if (p_mode == "read") {
		session.read_only = true;
	} else {
		return _err("mode must be read or write");
	}
	Dictionary result = _session_dict(session);
	result["ok"] = true;
	return result;
}

bool JustAMCPAgentPolicy::before_execute(const String &p_tool_name, const Dictionary &p_args, Dictionary &r_early) {
	if (!_on_main_thread()) {
		return false;
	}
	const String name = _internal_name(p_tool_name);
	const String session_id = _resolve_session_id(p_args);
	g_session_stack.push_back(session_id);
	g_policy_depth++;
	JustAMCPAgentSession &session = _session(session_id);

	const String idem = String(p_args.get("idempotency_key", ""));
	if (!idem.is_empty()) {
		const String cache_key = session_id + "\n" + name + "\n" + idem;
		if (g_idempotency.has(cache_key)) {
			Dictionary replay = g_idempotency[cache_key].duplicate();
			replay["idempotent_replay"] = true;
			r_early = replay;
			return true;
		}
	}

	if (g_applying_queue || _control_tool(name)) {
		return false;
	}

	const bool read = _read_tool(name);
	if (!read && session.read_only) {
		r_early = _err("session is read-only");
		r_early["read_only"] = true;
		return true;
	}
	if (!read && JustAMCPSettingsResolver::resolve_bool("blazium/justamcp/require_read_before_write", true) && !session.has_read) {
		r_early = _err("write-before-read rejected; read project state before writing");
		return true;
	}
	if (!read && p_args.has("expected_revision")) {
		const int expected = int(p_args.get("expected_revision", -1));
		if (expected != session.revision) {
			r_early = _err("expected_revision mismatch");
			r_early["expected_revision"] = expected;
			r_early["revision"] = session.revision;
			return true;
		}
	}

	const int play_cap = JustAMCPSettingsResolver::resolve_int("blazium/justamcp/play_mode_soft_cap", 0);
	static int play_starts = 0;
	if (play_cap > 0 && (name == "editor_play_scene" || name == "editor_play_main" || name == "editor_run_scene")) {
		play_starts++;
		if (play_starts > play_cap) {
			r_early = _err("Play Mode soft cap reached");
			r_early["needs_editor_ui"] = false;
			return true;
		}
	}
	const int shot_cap = JustAMCPSettingsResolver::resolve_int("blazium/justamcp/screenshot_soft_cap", 0);
	static int shots = 0;
	if (shot_cap > 0 && (name == "editor_take_screenshot" || name == "editor_screenshot_game" || name == "take_game_screenshot")) {
		shots++;
		if (shots > shot_cap) {
			r_early = _err("screenshot soft cap reached");
			return true;
		}
	}
	if (save_requires_confirmation() && (name == "editor_save_all" || name == "editor_save_all_scenes" || name == "save_scene" || name == "cross_scene_set_property")) {
		if (!bool(p_args.get("confirm", false))) {
			r_early = _err(name + " requires confirm=true");
			return true;
		}
	}

	if (!read && bool(p_args.get("dry_run", false))) {
		JustAMCPChangePlan plan;
		plan.id = "plan-" + String::num_uint64(Time::get_singleton()->get_ticks_usec());
		plan.tool = name;
		plan.args = p_args.duplicate();
		plan.args.erase("dry_run");
		plan.session_id = session_id;
		plan.revision_at = session.revision;
		plan.probe_before = g_probe;
		g_plans.insert(plan.id, plan);
		Dictionary early;
		early["ok"] = true;
		early["dry_run"] = true;
		early["plan_id"] = plan.id;
		early["tool"] = name;
		r_early = early;
		return true;
	}

	if (!read) {
		const String path = _claim_path_from_args(p_args);
		if (!path.is_empty()) {
			const JustAMCPClaim *held = nullptr;
			for (const KeyValue<String, JustAMCPClaim> &entry : g_claims) {
				const bool exact = entry.key == path;
				const bool nested = entry.value.subtree && path.begins_with(entry.key);
				if (exact || nested) {
					held = &entry.value;
					break;
				}
			}
			if (held && held->session_id != session_id) {
				const int wait_ms = JustAMCPSettingsResolver::resolve_int("blazium/justamcp/claim_wait_ms", 30000);
				if (wait_ms == 0) {
					r_early = _err("claim timeout");
					r_early["timeout"] = true;
					r_early["holder"] = held->holder;
					return true;
				}
				JustAMCPQueuedWrite queued;
				queued.path = path;
				queued.session_id = session_id;
				queued.tool = name;
				queued.args = p_args.duplicate();
				g_queue.push_back(queued);
				Dictionary early;
				early["ok"] = true;
				early["queued"] = true;
				early["holder"] = held->holder;
				r_early = early;
				return true;
			}
		}
	}

	if (name == "create_script" || name == "edit_script" || name == "update_script") {
		_push_file_snapshot(String(p_args.get("path", "")));
	}
	if (JustAMCPSettingsResolver::resolve_bool("blazium/justamcp/checkpoint_before_destructive", false)) {
		if (name.contains("delete") || name.contains("remove") || name == "restore_checkpoint") {
			Array paths;
			const String path = _claim_path_from_args(p_args);
			if (!path.is_empty()) {
				paths.push_back(path);
			}
			make_checkpoint(paths);
		}
	}
	return false;
}

Dictionary JustAMCPAgentPolicy::after_execute(const String &p_tool_name, const Dictionary &p_args, Dictionary p_result) {
	if (!_on_main_thread() || g_policy_depth <= 0) {
		return p_result;
	}
	g_policy_depth--;
	const String session_id = g_session_stack.is_empty() ? String("editor") : g_session_stack[g_session_stack.size() - 1];
	if (!g_session_stack.is_empty()) {
		g_session_stack.remove_at(g_session_stack.size() - 1);
	}
	const String name = _internal_name(p_tool_name);
	if (name == "session_close") {
		Dictionary row;
		row["tool"] = name;
		row["session_id"] = session_id;
		row["ok"] = !_failed(p_result);
		g_audit.push_back(row);
		return p_result;
	}
	JustAMCPAgentSession &session = _session(session_id);
	session.calls++;

	if (bool(p_result.get("idempotent_replay", false)) || bool(p_result.get("dry_run", false)) || bool(p_result.get("queued", false))) {
		Dictionary row;
		row["tool"] = name;
		row["session_id"] = session_id;
		row["ok"] = !_failed(p_result);
		g_audit.push_back(row);
		return p_result;
	}

	const bool failed = _failed(p_result);
	if (!failed && _read_tool(name)) {
		session.has_read = true;
	}
	if (!failed && !_read_tool(name) && !_control_tool(name)) {
		session.revision++;
		if (!p_result.has("read_back")) {
			p_result["read_back"] = true;
		}
		if (!p_result.has("changed")) {
			p_result["changed"] = true;
		}
		p_result["revision"] = session.revision;
	}
	if (failed) {
		int count = session.failures.has(name) ? session.failures[name] : 0;
		count++;
		session.failures[name] = count;
		if (count >= 3) {
			String checkpoint_id = session.last_checkpoint;
			if (checkpoint_id.is_empty()) {
				checkpoint_id = String(make_checkpoint(Array()).get("checkpoint_id", ""));
			}
			p_result["stuck"] = true;
			p_result["checkpoint_id"] = checkpoint_id;
		}
	}
	if (p_result.has("error")) {
		String message;
		if (p_result["error"].get_type() == Variant::DICTIONARY) {
			Dictionary error = p_result["error"];
			message = String(error.get("message", ""));
			if (message.contains("not found") || message.contains("Unknown tool")) {
				const String suggestion = suggest_tool(p_tool_name);
				if (!suggestion.is_empty()) {
					error["message"] = message + " Did you mean " + suggestion + "?";
					error["suggestion"] = suggestion;
					p_result["error"] = error;
				}
			}
		} else {
			message = String(p_result.get("error", ""));
			if (message.contains("not found") || message.contains("Unknown tool")) {
				const String suggestion = suggest_tool(p_tool_name);
				if (!suggestion.is_empty()) {
					p_result["error"] = message + " Did you mean " + suggestion + "?";
					p_result["suggestion"] = suggestion;
				}
			}
		}
		if (message.contains("unreachable") || message.contains("viewport") || message.contains("Editor GUI")) {
			p_result["needs_editor_ui"] = true;
		}
	}
	if (!failed && (name == "editor_take_screenshot" || name == "editor_screenshot_game" || name == "take_game_screenshot")) {
		const bool inline_bytes = bool(p_args.get("inline", false));
		p_result["resource_uri"] = "blazium://screenshot/latest";
		p_result["diff_summary"] = "pixel diff deferred; image stored as a resource link";
		if (p_args.has("crop_to_node")) {
			p_result["cropped_to"] = p_args.get("crop_to_node", "");
		}
		if (!p_args.has("scale")) {
			p_result["scale"] = 0.5;
		}
		g_screenshot_summary = String(p_result.get("diff_summary", ""));
		if (!inline_bytes) {
			p_result.erase("image");
			p_result.erase("base64");
			p_result.erase("png");
		}
	}

	const String idem = String(p_args.get("idempotency_key", ""));
	if (!idem.is_empty() && !failed) {
		g_idempotency.insert(session_id + "\n" + name + "\n" + idem, p_result.duplicate());
	}
	Dictionary row;
	row["tool"] = name;
	row["session_id"] = session_id;
	row["ok"] = !failed;
	row["revision"] = session.revision;
	g_audit.push_back(row);
	return p_result;
}

void JustAMCPAgentPolicy::note_grouped_undo(int p_steps) {
	if (p_steps > 1) {
		g_grouped_undo = p_steps;
	}
}

int JustAMCPAgentPolicy::take_grouped_undo() {
	const int steps = g_grouped_undo;
	g_grouped_undo = 0;
	return steps;
}

void JustAMCPAgentPolicy::clear_grouped_undo() {
	g_grouped_undo = 0;
}

int JustAMCPAgentPolicy::undo_snapshots(int p_steps) {
	int restored = 0;
	for (int i = 0; i < p_steps && !g_undo.is_empty(); i++) {
		const JustAMCPUndoEntry entry = g_undo[g_undo.size() - 1];
		g_undo.remove_at(g_undo.size() - 1);
		if (entry.file) {
			Ref<FileAccess> file = FileAccess::open(entry.path, FileAccess::WRITE);
			if (file.is_valid()) {
				file->store_string(entry.previous);
			}
		} else {
			g_probe = entry.probe_value;
		}
		restored++;
	}
	return restored;
}

void JustAMCPAgentPolicy::note_tool_name(const String &p_name) {
	if (p_name.is_empty()) {
		return;
	}
	for (int i = 0; i < g_tool_names.size(); i++) {
		if (g_tool_names[i] == p_name) {
			return;
		}
	}
	g_tool_names.push_back(p_name);
}

void JustAMCPAgentPolicy::attach_annotations(Dictionary &p_schema) {
	const String full_name = String(p_schema.get("name", ""));
	note_tool_name(full_name);
	const String name = _internal_name(full_name);
	const bool read = _read_tool(name);
	const bool destructive = !read && (name.contains("delete") || name.contains("remove") || name.contains("drop") || name == "restore_checkpoint");
	const bool snapshot = name == "create_script" || name == "edit_script" || name.contains("tilemap") || name.contains("animation") || name.contains("resource");
	Dictionary annotations;
	annotations["readOnlyHint"] = read;
	annotations["destructiveHint"] = destructive;
	annotations["idempotentHint"] = read;
	annotations["reversible"] = read ? String("none") : (snapshot ? String("snapshot") : String("undo"));
	p_schema["annotations"] = annotations;
	if (read) {
		p_schema["readonly"] = true;
		if (p_schema.has("_meta") && p_schema["_meta"].get_type() == Variant::DICTIONARY) {
			Dictionary meta = p_schema["_meta"];
			meta["readonly"] = true;
			p_schema["_meta"] = meta;
		}
	}
}

String JustAMCPAgentPolicy::suggest_tool(const String &p_name) {
	const String wanted = _internal_name(p_name);
	String best;
	int best_distance = 5;
	for (int i = 0; i < g_tool_names.size(); i++) {
		const String candidate = _internal_name(g_tool_names[i]);
		if (candidate == wanted) {
			continue;
		}
		const int distance = _edit_distance(wanted, candidate);
		if (distance < best_distance) {
			best_distance = distance;
			best = g_tool_names[i];
		}
	}
	return best;
}

bool JustAMCPAgentPolicy::read_extra_guide(const String &p_slug, String &r_title, String &r_body) {
	if (p_slug == "which-interface") {
		r_title = "Which Interface";
		r_body = "Use the editor MCP host for scene, script, and resource edits. Use the game host only for a running play session. Prefer blazium://guide/tool-index when choosing a tool.";
	} else if (p_slug == "data-handling") {
		r_title = "Data Handling";
		r_body = "Tool arguments and file snapshots stay in the editor process. Do not send project source to an external store. Checkpoints copy allow-listed res:// files into an in-memory snapshot.";
	} else if (p_slug == "opt-in") {
		r_title = "Opt In";
		r_body = "The MCP server stays off until --enable-mcp or an explicit enable command. Export, publish, and Play Mode over the soft cap require confirmation. save_requires_confirmation gates scene saves.";
	} else if (p_slug == "file-first") {
		r_title = "File First";
		r_body = "Read the project file and the live EditorHelp page before rewriting a call. Keep edits in the project files that already exist.";
	} else if (p_slug == "client-setup") {
		r_title = "Client Setup";
		r_body = "Write a client config with blazium_write_client_config. Configs include the local /mcp URL and the instance bearer token. server_enabled stays off until the editor is started with MCP enabled.";
	} else if (p_slug == "self-hosted") {
		r_title = "Self Hosted";
		r_body = "JustAMCP binds to localhost by default. Agents share one URL and one bearer. clientInfo.name is the agent label.";
	} else if (p_slug == "community-toolsets") {
		r_title = "Community Toolsets";
		r_body = "Enable toolset discovery only when a client needs a smaller catalog. The full editor catalog remains available when discovery is off.";
	} else if (p_slug == "languages") {
		r_title = "Languages";
		r_body = "GDScript is validated with the engine parser. Luau writes are rejected when function/end or parentheses are unbalanced. C# files are written as given.";
	} else if (p_slug == "engine-docs") {
		r_title = "Engine Docs";
		r_body = "Class, member, signal, and constant facts come from the running editor's EditorHelp, not training data or older Godot or Blazium memory. Read blazium://docs/class/{class}, blazium://docs/member/{class}/{member}, and blazium://docs/search/{query} before writing an API call. If those docs disagree with training data, follow EditorHelp. Project scripts are in the same doc set.";
	} else if (p_slug == "multi-agent") {
		r_title = "Multi Agent";
		r_body = "Many agents can initialize against one server. blazium://sessions lists clientInfo names. claim_scene and claim_subtree lease a path. Conflicting writes queue until release_claim. claim_wait_ms 0 returns timeout and the holder name.";
	} else {
		return false;
	}
	return true;
}

bool JustAMCPAgentPolicy::can_read_agent_resource(const String &p_canonical) {
	return p_canonical == "blazium://logs/actions" || p_canonical == "blazium://session/usage" || p_canonical == "blazium://reference/tools" || p_canonical == "blazium://mcp/clients" || p_canonical == "blazium://mcp/compatibility" || p_canonical == "blazium://meta/tools_list_bytes" || p_canonical == "blazium://project/conventions" || p_canonical == "blazium://screenshot/latest";
}

Dictionary JustAMCPAgentPolicy::read_agent_resource(const String &p_uri, const String &p_canonical) {
	if (p_canonical == "blazium://logs/actions") {
		Dictionary payload;
		payload["entries"] = g_audit;
		return JustAMCPResourceJson::make_json_contents(p_uri, payload);
	}
	if (p_canonical == "blazium://session/usage") {
		return JustAMCPResourceJson::make_json_contents(p_uri, usage_report());
	}
	if (p_canonical == "blazium://reference/tools") {
		String body = "Tool reference generated from the live editor catalog.\n";
		for (int i = 0; i < g_tool_names.size() && i < 80; i++) {
			body += "- " + g_tool_names[i] + "\n";
		}
		body += "Small-model profile: prefer recipe_add_player_controller, project_map, and EditorHelp docs before inventing API calls.\n";
		return JustAMCPResourceJson::make_text_contents(p_uri, body, "text/plain");
	}
	if (p_canonical == "blazium://mcp/clients") {
		Dictionary payload;
		payload["clients"] = client_config("cursor");
		payload["matrix"] = "cursor, claude, vscode, gemini, grok, windsurf, opencode, codex";
		return JustAMCPResourceJson::make_json_contents(p_uri, payload);
	}
	if (p_canonical == "blazium://mcp/compatibility") {
		Dictionary payload;
		payload["profile"] = "small-model";
		payload["instructions"] = justamcp_server_instructions();
		payload["license"] = "Engine tools ship with the Blazium editor. No marketplace plugin is required.";
		return JustAMCPResourceJson::make_json_contents(p_uri, payload);
	}
	if (p_canonical == "blazium://meta/tools_list_bytes") {
		Dictionary payload;
		payload["tool_names"] = g_tool_names.size();
		payload["bytes_estimate"] = g_tool_names.size() * 180;
		return JustAMCPResourceJson::make_json_contents(p_uri, payload);
	}
	if (p_canonical == "blazium://project/conventions") {
		Dictionary payload;
		payload["bone_name"] = "^[A-Za-z_][A-Za-z0-9_]*$";
		payload["note"] = "Bone and node names are letters, digits, and underscores, and do not start with a digit.";
		return JustAMCPResourceJson::make_json_contents(p_uri, payload);
	}
	return JustAMCPResourceJson::make_text_contents(p_uri, g_screenshot_summary, "text/plain");
}

String JustAMCPAgentPolicy::client_config(const String &p_client) {
	const int port = JustAMCPSettingsResolver::resolve_server_port();
	const String url = "http://127.0.0.1:" + itos(port) + "/mcp";
	const String token = instance_bearer();
	const String client = p_client.to_lower();
	if (client == "codex") {
		return "[mcp_servers.blazium]\nurl = \"" + url + "\"\nbearer_token = \"" + token + "\"\n";
	}
	if (client == "opencode") {
		return "{\n  \"$schema\": \"https://opencode.ai/config.json\",\n  \"mcp\": {\n    \"blazium-mcp\": {\n      \"type\": \"remote\",\n      \"url\": \"" + url + "\",\n      \"headers\": {\"Authorization\": \"Bearer " + token + "\"},\n      \"enabled\": true\n    }\n  }\n}\n";
	}
	const bool cursor = client == "cursor";
	String json = "{\n  \"mcpServers\": {\n    \"blazium-mcp\": {\n";
	json += cursor ? "      \"url\": \"" + url + "\",\n" : "      \"serverUrl\": \"" + url + "\",\n";
	json += "      \"headers\": {\"Authorization\": \"Bearer " + token + "\"}\n";
	json += "    }\n  }\n}\n";
	return json;
}

Dictionary JustAMCPAgentPolicy::write_client_config(const String &p_client, const String &p_path) {
	Dictionary result;
	const String config = client_config(p_client);
	result["config"] = config;
	result["client"] = p_client;
	bool ping_ok = justamcp_server_instructions().contains("EditorHelp");
	if (JustAMCPServer::get_singleton()) {
		Dictionary payload;
		Dictionary params;
		params["protocolVersion"] = "2025-11-25";
		params["capabilities"] = Dictionary();
		payload["params"] = params;
		const Dictionary routed = JustAMCPJsonRpcRouter::route_initialize(JustAMCPServer::get_singleton(), payload, 1);
		if (routed.get("handled", false)) {
			const Dictionary init = routed.get("result", Dictionary());
			ping_ok = String(init.get("instructions", "")).contains("EditorHelp");
		}
	}
	result["ping_ok"] = ping_ok;
	if (p_path.is_empty()) {
		result["ok"] = true;
		result["written"] = false;
		return result;
	}
	if (!_safe_project_path(p_path)) {
		result["ok"] = false;
		result["error"] = "refusing path outside res:// or user://";
		result["ping_ok"] = ping_ok;
		return result;
	}
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE);
	if (file.is_null()) {
		result["ok"] = false;
		result["error"] = "cannot write client config";
		return result;
	}
	file->store_string(config);
	result["ok"] = true;
	result["written"] = true;
	result["path"] = p_path;
	return result;
}

int JustAMCPAgentPolicy::probe_value() {
	return g_probe;
}

void JustAMCPAgentPolicy::probe_reset() {
	g_probe = 0;
	g_undo.clear();
	g_grouped_undo = 0;
	g_idempotency.clear();
}

int JustAMCPAgentPolicy::probe_increment() {
	const int previous = g_probe;
	g_probe++;
	_push_undo_probe(previous);
	return g_probe;
}

Dictionary JustAMCPAgentPolicy::claim_path(const String &p_path, bool p_subtree) {
	if (!_safe_project_path(p_path)) {
		return _err("claim path must be res:// or user://");
	}
	const String session_id = current_session_id();
	const JustAMCPAgentSession &session = _session(session_id);
	if (g_claims.has(p_path) && g_claims[p_path].session_id != session_id) {
		Dictionary result = _err("path is claimed");
		result["holder"] = g_claims[p_path].holder;
		const int wait_ms = JustAMCPSettingsResolver::resolve_int("blazium/justamcp/claim_wait_ms", 30000);
		result["timeout"] = wait_ms == 0;
		result["busy"] = wait_ms != 0;
		return result;
	}
	JustAMCPClaim claim;
	claim.path = p_path;
	claim.session_id = session_id;
	claim.holder = session.name;
	claim.subtree = p_subtree;
	g_claims.insert(p_path, claim);
	Dictionary result;
	result["ok"] = true;
	result["path"] = p_path;
	result["holder"] = claim.holder;
	result["subtree"] = p_subtree;
	return result;
}

Dictionary JustAMCPAgentPolicy::release_claim(const String &p_path) {
	if (!g_claims.has(p_path)) {
		return _err("claim not found");
	}
	if (g_claims[p_path].session_id != current_session_id() && current_session_id() != "editor") {
		Dictionary result = _err("only the holder can release");
		result["holder"] = g_claims[p_path].holder;
		return result;
	}
	g_claims.erase(p_path);
	const int before = g_probe;
	_apply_queued(p_path);
	Dictionary result;
	result["ok"] = true;
	result["released"] = p_path;
	result["applied"] = g_probe - before;
	return result;
}

Array JustAMCPAgentPolicy::list_claims() {
	Array claims;
	for (const KeyValue<String, JustAMCPClaim> &entry : g_claims) {
		Dictionary claim;
		claim["path"] = entry.value.path;
		claim["session_id"] = entry.value.session_id;
		claim["holder"] = entry.value.holder;
		claim["subtree"] = entry.value.subtree;
		claims.push_back(claim);
	}
	return claims;
}

Dictionary JustAMCPAgentPolicy::make_checkpoint(const Array &p_paths) {
	JustAMCPCheckpoint checkpoint;
	checkpoint.id = "ckpt-" + String::num_uint64(Time::get_singleton()->get_ticks_usec());
	checkpoint.git_head = _git_head();
	for (int i = 0; i < p_paths.size(); i++) {
		const String path = String(p_paths[i]);
		if (!_safe_project_path(path) || !FileAccess::exists(path)) {
			continue;
		}
		checkpoint.files.insert(path, FileAccess::get_file_as_string(path));
	}
	g_checkpoints.insert(checkpoint.id, checkpoint);
	g_checkpoint_order.push_back(checkpoint.id);
	const String session_id = current_session_id();
	if (g_sessions.has(session_id)) {
		g_sessions[session_id].last_checkpoint = checkpoint.id;
	}
	Dictionary result;
	result["ok"] = true;
	result["checkpoint_id"] = checkpoint.id;
	result["files"] = checkpoint.files.size();
	result["git_head"] = checkpoint.git_head;
	return result;
}

Array JustAMCPAgentPolicy::list_checkpoints() {
	Array ids;
	for (int i = 0; i < g_checkpoint_order.size(); i++) {
		ids.push_back(g_checkpoint_order[i]);
	}
	return ids;
}

Dictionary JustAMCPAgentPolicy::diff_checkpoint(const String &p_id) {
	if (!g_checkpoints.has(p_id)) {
		return _err("checkpoint not found");
	}
	const JustAMCPCheckpoint &checkpoint = g_checkpoints[p_id];
	Array changed;
	for (const KeyValue<String, String> &entry : checkpoint.files) {
		const String current = FileAccess::exists(entry.key) ? FileAccess::get_file_as_string(entry.key) : String();
		if (current != entry.value) {
			Dictionary row;
			row["path"] = entry.key;
			row["changed"] = true;
			changed.push_back(row);
		}
	}
	Dictionary result;
	result["ok"] = true;
	result["checkpoint_id"] = p_id;
	result["changed"] = changed;
	return result;
}

Dictionary JustAMCPAgentPolicy::restore_checkpoint(const String &p_id) {
	if (!g_checkpoints.has(p_id)) {
		return _err("checkpoint not found");
	}
	const JustAMCPCheckpoint &checkpoint = g_checkpoints[p_id];
	int restored = 0;
	for (const KeyValue<String, String> &entry : checkpoint.files) {
		Ref<FileAccess> file = FileAccess::open(entry.key, FileAccess::WRITE);
		if (file.is_valid()) {
			file->store_string(entry.value);
			restored++;
		}
	}
	Dictionary result;
	result["ok"] = true;
	result["checkpoint_id"] = p_id;
	result["restored"] = restored;
	return result;
}

String JustAMCPAgentPolicy::last_checkpoint_id() {
	if (g_checkpoint_order.is_empty()) {
		return String();
	}
	return g_checkpoint_order[g_checkpoint_order.size() - 1];
}

Dictionary JustAMCPAgentPolicy::apply_change_plan(const String &p_plan_id) {
	if (!g_plans.has(p_plan_id)) {
		return _err("change plan not found");
	}
	JustAMCPChangePlan &plan = g_plans[p_plan_id];
	if (plan.applied) {
		return _err("change plan already applied");
	}
	if (plan.tool == "agent_probe_increment") {
		const int value = probe_increment();
		plan.applied = true;
		plan.revision_at = _session(plan.session_id).revision;
		Dictionary result;
		result["ok"] = true;
		result["applied"] = true;
		result["plan_id"] = p_plan_id;
		result["value"] = value;
		return result;
	}
	plan.applied = true;
	plan.revision_at = _session(plan.session_id).revision;
	Dictionary result;
	result["ok"] = true;
	result["applied"] = true;
	result["plan_id"] = p_plan_id;
	result["tool"] = plan.tool;
	result["note"] = "Plan recorded. Re-run the tool without dry_run to execute non-probe work.";
	return result;
}

Dictionary JustAMCPAgentPolicy::revert_change_plan(const String &p_plan_id) {
	if (!g_plans.has(p_plan_id)) {
		return _err("change plan not found");
	}
	JustAMCPChangePlan &plan = g_plans[p_plan_id];
	JustAMCPAgentSession &session = _session(plan.session_id);
	if (!plan.applied || session.revision != plan.revision_at + 1) {
		Dictionary result = _err("revert revision mismatch");
		result["mismatch"] = true;
		result["applied"] = plan.applied;
		result["revision"] = session.revision;
		return result;
	}
	if (plan.tool == "agent_probe_increment") {
		undo_snapshots(1);
	}
	plan.applied = false;
	Dictionary result;
	result["ok"] = true;
	result["mismatch"] = false;
	result["plan_id"] = p_plan_id;
	result["value"] = g_probe;
	return result;
}

Array JustAMCPAgentPolicy::audit_log() {
	return g_audit;
}

Dictionary JustAMCPAgentPolicy::usage_report() {
	Dictionary usage;
	Array sessions;
	int calls = 0;
	for (const KeyValue<String, JustAMCPAgentSession> &entry : g_sessions) {
		Dictionary row = _session_dict(entry.value);
		calls += entry.value.calls;
		sessions.push_back(row);
	}
	usage["calls"] = calls;
	usage["sessions"] = sessions;
	return usage;
}

void JustAMCPAgentPolicy::store_screenshot_summary(const String &p_summary) {
	g_screenshot_summary = p_summary;
}

Dictionary JustAMCPAgentPolicy::commit_knobs(const Dictionary &p_knobs) {
	Array keys = p_knobs.keys();
	for (int i = 0; i < keys.size(); i++) {
		g_knobs[keys[i]] = p_knobs[keys[i]];
	}
	Dictionary result;
	result["ok"] = true;
	result["knobs"] = g_knobs.duplicate();
	return result;
}

Dictionary JustAMCPAgentPolicy::current_knobs() {
	return g_knobs.duplicate();
}

#endif
