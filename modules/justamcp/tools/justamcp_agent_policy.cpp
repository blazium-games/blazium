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
#include "../justamcp_read_limits.h"
#include "../justamcp_server.h"
#include "justamcp_agent_helpers.h"
#include "justamcp_json_rpc_router.h"
#include "justamcp_readonly_tools.h"
#include "justamcp_settings_resolver.h"
#include "justamcp_tool_executor.h"
#include "resources/justamcp_resource_json.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/math/math_funcs.h"
#include "core/object/object.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/os/time.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "scene/2d/tile_map.h"
#include "scene/2d/tile_map_layer.h"
#include "scene/main/node.h"

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
	bool binary = false;
	bool moved = false;
	bool copied = false;
	bool dest_existed = false;
	String path;
	String dest;
	String previous;
	PackedByteArray previous_bytes;
	PackedByteArray dest_previous_bytes;
	int probe_value = 0;
	Array property_restores;
	Array added_node_ids;
	Array tile_restores;
};

struct JustAMCPCheckpoint {
	String id;
	String git_head;
	Dictionary files;
	Dictionary binary_files;
};

struct JustAMCPChangePlan {
	String id;
	String tool;
	Dictionary args;
	String session_id;
	int revision_at = 0;
	bool applied = false;
	bool snapshot = false;
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
static Array g_audit_slots;
static int g_audit_count = 0;
static int g_audit_next = 0;
static Dictionary g_knobs;
static String g_screenshot_summary = "No screenshot captured yet.";
static Array g_scene_baseline;
static String g_bearer;
static thread_local bool g_applying_queue = false;
static thread_local bool g_snapshot_skipped = false;
static HashSet<String> g_closed_sessions;
static Mutex g_policy_mutex;
static Vector<String> g_idempotency_order;
static HashMap<int, Vector<int>> g_tools_by_length;
static String g_cached_git_head;
static bool g_git_head_valid = false;
static int g_result_bytes = 0;

static constexpr int k_audit_cap = 256;
static constexpr int k_checkpoint_cap = 32;
static constexpr int k_idempotency_cap = 128;
static constexpr int k_undo_cap = 64;
static constexpr int k_session_call_cap = 4096;

static String _claim_key(const String &p_path) {
	String key = p_path;
	while (key.length() > 6 && key.ends_with("/")) {
		key = key.substr(0, key.length() - 1);
	}
	return key;
}

static bool _claim_covers(const String &p_claim_path, bool p_subtree, const String &p_path) {
	const String key = _claim_key(p_claim_path);
	const String path = _claim_key(p_path);
	if (key == path) {
		return true;
	}
	return p_subtree && path.begins_with(key + "/");
}

static bool _session_closed(const String &p_id) {
	return !p_id.is_empty() && p_id != "editor" && g_closed_sessions.has(p_id);
}

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

static bool _safe_project_path(const String &p_path);

static bool _file_snapshot_tool(const String &p_name) {
	return p_name == "create_script" || p_name == "edit_script" || p_name == "update_script" || p_name == "patch_script" || p_name == "delete_script" || p_name == "create_shader" || p_name == "edit_shader" || p_name == "create_shader_template" || p_name == "edit_resource_file" || p_name == "create_file" || p_name == "edit_file" || p_name == "delete_file" || p_name == "create_resource" || p_name == "modify_resource" || p_name == "create_material" || p_name == "create_tileset" || p_name == "create_theme" || p_name == "save_resource_as" || p_name == "set_theme_resource_color" || p_name == "set_theme_resource_font_size";
}

static String _snapshot_path_from_args(const String &p_name, const Dictionary &p_args) {
	Vector<String> keys;
	if (p_name == "save_resource_as") {
		keys.push_back("dest_path");
		keys.push_back("save_path");
	}
	keys.push_back("path");
	keys.push_back("resource_path");
	keys.push_back("resourcePath");
	keys.push_back("file_path");
	keys.push_back("shaderPath");
	keys.push_back("materialPath");
	keys.push_back("tilesetPath");
	keys.push_back("themePath");
	keys.push_back("scene_path");
	keys.push_back("scenePath");
	if (p_name != "save_resource_as") {
		keys.push_back("dest_path");
		keys.push_back("save_path");
	}
	for (const String &key : keys) {
		const String path = String(p_args.get(key, ""));
		if (_safe_project_path(path)) {
			return path;
		}
	}
	return String();
}

static bool _safe_project_path(const String &p_path) {
	if (!(p_path.begins_with("res://") || p_path.begins_with("user://"))) {
		return false;
	}
	return !p_path.contains("..");
}

static bool _binary_extension(const String &p_path) {
	const String ext = p_path.get_extension().to_lower();
	return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp" || ext == "wav" || ext == "ogg" || ext == "mp3" || ext == "res" || ext == "scn" || ext == "ctex" || ext == "ttf" || ext == "otf" || ext == "glb";
}

static bool _bytes_are_text(const PackedByteArray &p_bytes) {
	if (p_bytes.is_empty()) {
		return true;
	}
	String text;
	return text.parse_utf8((const char *)p_bytes.ptr(), p_bytes.size()) == OK;
}

static bool _file_over_snapshot_cap(const String &p_path) {
	if (!FileAccess::exists(p_path)) {
		return false;
	}
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	if (file.is_null()) {
		return false;
	}
	return file->get_length() > uint64_t(JUSTAMCP_MAX_SYNC_READ_BYTES);
}

static bool _capture_file_bytes(const String &p_path, PackedByteArray &r_bytes, bool &r_binary) {
	r_bytes = PackedByteArray();
	r_binary = false;
	if (!FileAccess::exists(p_path)) {
		return true;
	}
	if (_file_over_snapshot_cap(p_path)) {
		g_snapshot_skipped = true;
		return false;
	}
	r_bytes = FileAccess::get_file_as_bytes(p_path);
	r_binary = _binary_extension(p_path) || !_bytes_are_text(r_bytes);
	return true;
}

static void _write_bytes(const String &p_path, const PackedByteArray &p_bytes) {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE);
	if (file.is_valid()) {
		file->store_buffer(p_bytes);
	}
}

static void _remove_snapshot_file(const String &p_path) {
	Ref<DirAccess> dir = DirAccess::create_for_path(p_path.get_base_dir());
	if (dir.is_valid()) {
		dir->remove(p_path);
	}
}

static void _trim_undo();

static void _note_transfer_undo(const String &p_from, const String &p_to, bool p_move) {
	if (!_safe_project_path(p_from) || !_safe_project_path(p_to)) {
		return;
	}
	PackedByteArray source_bytes;
	PackedByteArray dest_bytes;
	bool source_binary = false;
	bool dest_binary = false;
	const bool dest_existed = FileAccess::exists(p_to);
	if (!_capture_file_bytes(p_from, source_bytes, source_binary) || (dest_existed && !_capture_file_bytes(p_to, dest_bytes, dest_binary))) {
		return;
	}
	MutexLock lock(g_policy_mutex);
	JustAMCPUndoEntry entry;
	entry.moved = p_move;
	entry.copied = !p_move;
	entry.path = p_from;
	entry.dest = p_to;
	entry.dest_existed = dest_existed;
	entry.previous_bytes = source_bytes;
	entry.dest_previous_bytes = dest_bytes;
	g_undo.push_back(entry);
	_trim_undo();
}

static void _restore_transfer(const JustAMCPUndoEntry &p_entry) {
	if (p_entry.moved) {
		_write_bytes(p_entry.path, p_entry.previous_bytes);
	}
	if (p_entry.dest_existed) {
		_write_bytes(p_entry.dest, p_entry.dest_previous_bytes);
	} else {
		_remove_snapshot_file(p_entry.dest);
	}
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
	if (!g_sessions.has(p_id) && !g_closed_sessions.has(p_id)) {
		JustAMCPAgentPolicy::open_session(p_id, p_id, true);
	}
	if (!g_sessions.has(p_id)) {
		static JustAMCPAgentSession missing;
		missing = JustAMCPAgentSession();
		missing.id = p_id;
		missing.name = p_id;
		missing.read_only = true;
		return missing;
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

static void _trim_undo() {
	while (g_undo.size() > k_undo_cap) {
		g_undo.remove_at(0);
	}
}

static void _push_audit(const Dictionary &p_row) {
	if (g_audit_slots.size() != k_audit_cap) {
		g_audit_slots.resize(k_audit_cap);
	}
	g_audit_slots[g_audit_next] = p_row;
	g_audit_next = (g_audit_next + 1) % k_audit_cap;
	if (g_audit_count < k_audit_cap) {
		g_audit_count++;
	}
}

static void _store_idempotent(const String &p_key, const Dictionary &p_value) {
	for (int i = 0; i < g_idempotency_order.size(); i++) {
		if (g_idempotency_order[i] == p_key) {
			g_idempotency_order.remove_at(i);
			break;
		}
	}
	g_idempotency.erase(p_key);
	g_idempotency.insert(p_key, p_value);
	g_idempotency_order.push_back(p_key);
	while (g_idempotency_order.size() > k_idempotency_cap) {
		const String old_key = g_idempotency_order[0];
		g_idempotency_order.remove_at(0);
		g_idempotency.erase(old_key);
	}
}

static int _measured_result_bytes(const Dictionary &p_result) {
	Dictionary measured = p_result.duplicate();
	measured.erase("image");
	measured.erase("base64");
	measured.erase("png");
	return JSON::stringify(measured).utf8().length();
}

static void _count_result_bytes(const Dictionary &p_result) {
	const int bytes = _measured_result_bytes(p_result);
	MutexLock lock(g_policy_mutex);
	g_result_bytes += bytes;
}

static Vector<String> _copy_suggestion_names(const String &p_wanted) {
	Vector<String> names;
	const int wanted_length = p_wanted.length();
	for (int length = MAX(0, wanted_length - 5); length <= wanted_length + 5; length++) {
		if (!g_tools_by_length.has(length)) {
			continue;
		}
		const Vector<int> bucket = g_tools_by_length[length];
		for (int i = 0; i < bucket.size(); i++) {
			const int index = bucket[i];
			if (index < 0 || index >= g_tool_names.size()) {
				continue;
			}
			names.push_back(g_tool_names[index]);
		}
	}
	return names;
}

static String _suggest_from_names(const String &p_wanted, const Vector<String> &p_names) {
	String best;
	int best_distance = 5;
	for (int i = 0; i < p_names.size(); i++) {
		const String candidate = _internal_name(p_names[i]);
		if (candidate == p_wanted) {
			continue;
		}
		const int distance = _edit_distance(p_wanted, candidate);
		if (distance < best_distance) {
			best_distance = distance;
			best = p_names[i];
		}
	}
	return best;
}

static String _catalog_text() {
	Vector<String> names;
	{
		MutexLock lock(g_policy_mutex);
		names = g_tool_names;
	}
	String body = "Tool reference generated from the live editor catalog.\n";
	for (int i = 0; i < names.size(); i++) {
		const String tool_name = names[i];
		const bool readonly = JustAMCPReadonlyTools::is_readonly_tool(tool_name);
		body += "- " + tool_name + " readonly=" + String(readonly ? "true" : "false") + "\n";
	}
	body += "Small-model profile: prefer recipe_add_player_controller, project_map, and EditorHelp docs before inventing API calls.\n";
	return body;
}

static void _push_undo_probe(int p_previous) {
	JustAMCPUndoEntry entry;
	entry.file = false;
	entry.probe_value = p_previous;
	g_undo.push_back(entry);
	_trim_undo();
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

static String _git_head_for_checkpoint() {
	bool refresh = false;
	String cached;
	{
		MutexLock lock(g_policy_mutex);
		refresh = !g_git_head_valid;
		cached = g_cached_git_head;
	}
	if (!refresh) {
		return cached;
	}
	const String head = _git_head();
	{
		MutexLock lock(g_policy_mutex);
		g_cached_git_head = head;
		g_git_head_valid = true;
	}
	return head;
}

static Array _apply_queued(const String &p_path, bool p_subtree) {
	Vector<String> tools;
	Vector<String> sessions;
	Vector<Dictionary> arg_copies;
	Vector<JustAMCPQueuedWrite> remain;
	g_policy_mutex.lock();
	for (int i = 0; i < g_queue.size(); i++) {
		const String path = g_queue[i].path;
		const String tool = g_queue[i].tool;
		const String session_id = g_queue[i].session_id;
		const Dictionary args = g_queue[i].args;
		if (!p_path.is_empty() && !_claim_covers(p_path, p_subtree, path)) {
			JustAMCPQueuedWrite kept;
			kept.path = path;
			kept.tool = tool;
			kept.session_id = session_id;
			kept.args = args;
			remain.push_back(kept);
			continue;
		}
		tools.push_back(tool);
		sessions.push_back(session_id);
		arg_copies.push_back(args);
	}
	g_queue = remain;
	g_applying_queue = true;
	JustAMCPToolExecutor *executor = JustAMCPToolExecutor::get_active_instance();
	g_policy_mutex.unlock();
	Array results;
	for (int i = 0; i < tools.size(); i++) {
		if (executor) {
			Dictionary args = arg_copies[i].duplicate(true);
			if (!sessions[i].is_empty()) {
				args["_session_id"] = sessions[i];
			}
			const String tool = tools[i].begins_with("blazium_") ? tools[i] : String("blazium_") + tools[i];
			results.push_back(executor->execute_tool(tool, args));
		} else if (_internal_name(tools[i]) == "agent_probe_increment") {
			Dictionary fallback;
			fallback["ok"] = true;
			fallback["value"] = JustAMCPAgentPolicy::probe_increment();
			results.push_back(fallback);
		}
	}
	g_policy_mutex.lock();
	g_applying_queue = false;
	g_policy_mutex.unlock();
	return results;
}

static Vector<JustAMCPClaim> _drop_claims_for_session(const String &p_id) {
	Vector<JustAMCPClaim> released;
	Vector<String> keys;
	for (const KeyValue<String, JustAMCPClaim> &entry : g_claims) {
		keys.push_back(entry.key);
	}
	for (int i = 0; i < keys.size(); i++) {
		if (g_claims[keys[i]].session_id == p_id) {
			released.push_back(g_claims[keys[i]]);
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
	return released;
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
	MutexLock lock(g_policy_mutex);
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
	if (token.is_empty()) {
		return false;
	}
	const PackedByteArray trusted = instance_bearer().to_utf8_buffer();
	const PackedByteArray received = token.to_utf8_buffer();
	if (trusted.size() != received.size()) {
		return false;
	}
	uint8_t diff = 0;
	for (int i = 0; i < trusted.size(); i++) {
		diff |= trusted[i] ^ received[i];
	}
	return diff == 0;
}

bool JustAMCPAgentPolicy::require_local_bearer() {
	return JustAMCPSettingsResolver::resolve_bool("blazium/justamcp/require_local_bearer", true);
}

bool JustAMCPAgentPolicy::save_requires_confirmation() {
	return JustAMCPSettingsResolver::resolve_bool("blazium/justamcp/save_requires_confirmation", false);
}

void JustAMCPAgentPolicy::open_session(const String &p_id, const String &p_name, bool p_from_initialize) {
	const bool read_only = p_from_initialize && JustAMCPSettingsResolver::resolve_bool("blazium/justamcp/session_starts_read_only", true);
	MutexLock lock(g_policy_mutex);
	if (p_id.is_empty()) {
		return;
	}
	const bool resurrect = g_closed_sessions.has(p_id);
	g_closed_sessions.erase(p_id);
	if (g_sessions.has(p_id) && !resurrect) {
		return;
	}
	JustAMCPAgentSession session;
	session.id = p_id;
	session.name = p_name.is_empty() ? p_id : p_name;
	session.read_only = read_only;
	session.has_read = !read_only;
	g_sessions.erase(p_id);
	g_sessions.insert(p_id, session);
}

void JustAMCPAgentPolicy::close_session(const String &p_id) {
	Vector<JustAMCPClaim> released;
	{
		MutexLock lock(g_policy_mutex);
		if (p_id.is_empty()) {
			return;
		}
		released = _drop_claims_for_session(p_id);
		g_sessions.erase(p_id);
		if (p_id != "editor") {
			g_closed_sessions.insert(p_id);
		}
	}
	for (int i = 0; i < released.size(); i++) {
		_apply_queued(released[i].path, released[i].subtree);
	}
}

Array JustAMCPAgentPolicy::list_sessions() {
	Vector<JustAMCPAgentSession> copied;
	{
		MutexLock lock(g_policy_mutex);
		_ensure_editor();
		for (const KeyValue<String, JustAMCPAgentSession> &entry : g_sessions) {
			copied.push_back(entry.value);
		}
	}
	Array sessions;
	for (int i = 0; i < copied.size(); i++) {
		sessions.push_back(_session_dict(copied[i]));
	}
	return sessions;
}

String JustAMCPAgentPolicy::current_session_id() {
	MutexLock lock(g_policy_mutex);
	if (!g_session_stack.is_empty()) {
		return g_session_stack[g_session_stack.size() - 1];
	}
	return "editor";
}

Dictionary JustAMCPAgentPolicy::session_state(const String &p_id) {
	MutexLock lock(g_policy_mutex);
	const String id = p_id.is_empty() ? current_session_id() : p_id;
	if (_session_closed(id)) {
		return _err("session is closed");
	}
	return _session_dict(_session(id));
}

Dictionary JustAMCPAgentPolicy::set_access(const String &p_id, const String &p_mode) {
	MutexLock lock(g_policy_mutex);
	const String id = p_id.is_empty() ? current_session_id() : p_id;
	if (_session_closed(id)) {
		return _err("session is closed");
	}
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
	const String name = _internal_name(p_tool_name);
	if (!_on_main_thread()) {
		MutexLock lock(g_policy_mutex);
		if (_read_tool(name)) {
			const String session_id = _resolve_session_id(p_args);
			if (!_session_closed(session_id)) {
				_session(session_id).has_read = true;
			}
			return false;
		}
		r_early = _err("write tools must run on the main thread");
		return true;
	}
	const bool require_read = JustAMCPSettingsResolver::resolve_bool("blazium/justamcp/require_read_before_write", true);
	const int play_cap = JustAMCPSettingsResolver::resolve_int("blazium/justamcp/play_mode_soft_cap", 0);
	const int shot_cap = JustAMCPSettingsResolver::resolve_int("blazium/justamcp/screenshot_soft_cap", 0);
	const bool save_confirm = save_requires_confirmation();
	const int claim_wait_ms = JustAMCPSettingsResolver::resolve_int("blazium/justamcp/claim_wait_ms", 30000);
	const bool checkpoint_destructive = JustAMCPSettingsResolver::resolve_bool("blazium/justamcp/checkpoint_before_destructive", false);
	Array checkpoint_paths;
	bool checkpoint_now = false;
	String snapshot_path;
	bool replay_now = false;
	Dictionary replay_shared;
	bool dry_run_now = false;
	String dry_plan_id;
	String dry_tool;
	String dry_session_id;
	int dry_revision = 0;
	int dry_probe = 0;
	Dictionary dry_args_shared;
	bool queue_now = false;
	String queue_path;
	String queue_session_id;
	String queue_tool;
	String queue_holder;
	Dictionary queue_args_shared;
	String transfer_from;
	String transfer_to;
	bool transfer_move = false;
	g_snapshot_skipped = false;
	{
		MutexLock lock(g_policy_mutex);
		const String session_id = _resolve_session_id(p_args);
		g_session_stack.push_back(session_id);
		g_policy_depth++;
		JustAMCPAgentSession &session = _session(session_id);

		const String idem = String(p_args.get("idempotency_key", ""));
		if (!idem.is_empty()) {
			const String cache_key = session_id + "\n" + name + "\n" + idem;
			if (g_idempotency.has(cache_key)) {
				replay_shared = g_idempotency[cache_key];
				replay_now = true;
			}
		}

		if (!replay_now) {
			if (g_applying_queue || _control_tool(name)) {
				return false;
			}
			if (_session_closed(session_id)) {
				r_early = _err("session is closed");
				return true;
			}

			const bool read = _read_tool(name);
			if (!read && session.read_only) {
				r_early = _err("session is read-only");
				r_early["read_only"] = true;
				return true;
			}
			if (!read && require_read && !session.has_read) {
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

			static int play_starts = 0;
			if (play_cap > 0 && (name == "editor_play_scene" || name == "editor_play_main" || name == "editor_run_scene")) {
				play_starts++;
				if (play_starts > play_cap) {
					r_early = _err("Play Mode soft cap reached");
					r_early["needs_editor_ui"] = false;
					return true;
				}
			}
			static int shots = 0;
			if (shot_cap > 0 && (name == "editor_take_screenshot" || name == "editor_screenshot_game" || name == "take_game_screenshot")) {
				shots++;
				if (shots > shot_cap) {
					r_early = _err("screenshot soft cap reached");
					return true;
				}
			}
			if (save_confirm && (name == "editor_save_all" || name == "editor_save_all_scenes" || name == "save_scene" || name == "cross_scene_set_property")) {
				if (!bool(p_args.get("confirm", false))) {
					r_early = _err(name + " requires confirm=true");
					return true;
				}
			}

			if (!read && bool(p_args.get("dry_run", false))) {
				dry_plan_id = "plan-" + String::num_uint64(Time::get_singleton()->get_ticks_usec());
				dry_tool = name;
				dry_args_shared = p_args;
				dry_session_id = session_id;
				dry_revision = session.revision;
				dry_probe = g_probe;
				dry_run_now = true;
			} else if (!read && name != "claim_scene" && name != "claim_subtree" && name != "release_claim") {
				const String path = _claim_path_from_args(p_args);
				if (!path.is_empty()) {
					const JustAMCPClaim *held = nullptr;
					for (const KeyValue<String, JustAMCPClaim> &entry : g_claims) {
						if (_claim_covers(entry.key, entry.value.subtree, path)) {
							held = &entry.value;
							break;
						}
					}
					if (held && held->session_id != session_id) {
						if (claim_wait_ms == 0) {
							r_early = _err("claim timeout");
							r_early["timeout"] = true;
							r_early["holder"] = held->holder;
							return true;
						}
						queue_path = path;
						queue_session_id = session_id;
						queue_tool = name;
						queue_holder = held->holder;
						queue_args_shared = p_args;
						queue_now = true;
					}
				}
			}

			if (!dry_run_now && !queue_now) {
				if (_file_snapshot_tool(name)) {
					snapshot_path = _snapshot_path_from_args(name, p_args);
				}
				if (name == "move_file" || name == "copy_file" || name == "scene_duplicate_file" || name == "import_asset_copy") {
					String from;
					String to;
					if (name == "scene_duplicate_file") {
						from = String(p_args.get("source_path", p_args.get("sourcePath", "")));
						to = String(p_args.get("dest_path", p_args.get("destPath", "")));
					} else if (name == "import_asset_copy") {
						from = String(p_args.get("source_path", ""));
						to = String(p_args.get("dest_path", ""));
					} else {
						from = String(p_args.get("from", p_args.get("file_path", "")));
						to = String(p_args.get("to", p_args.get("destination", "")));
					}
					if (_safe_project_path(from) && _safe_project_path(to)) {
						transfer_from = from;
						transfer_to = to;
						transfer_move = name == "move_file";
					}
				}
				if (checkpoint_destructive) {
					if (name.contains("delete") || name.contains("remove") || name == "restore_checkpoint") {
						checkpoint_now = true;
						const String path = _claim_path_from_args(p_args);
						if (!path.is_empty()) {
							checkpoint_paths.push_back(path);
						}
					}
				}
			}
		}
	}
	if (replay_now) {
		Dictionary replay = replay_shared.duplicate();
		replay["idempotent_replay"] = true;
		r_early = replay;
		return true;
	}
	if (dry_run_now) {
		JustAMCPChangePlan plan;
		plan.id = dry_plan_id;
		plan.tool = dry_tool;
		plan.args = dry_args_shared.duplicate();
		plan.args.erase("dry_run");
		plan.session_id = dry_session_id;
		plan.revision_at = dry_revision;
		plan.probe_before = dry_probe;
		{
			MutexLock lock(g_policy_mutex);
			g_plans.insert(plan.id, plan);
		}
		Dictionary early;
		early["ok"] = true;
		early["dry_run"] = true;
		early["plan_id"] = plan.id;
		early["tool"] = dry_tool;
		r_early = early;
		return true;
	}
	if (queue_now) {
		JustAMCPQueuedWrite queued;
		queued.path = queue_path;
		queued.session_id = queue_session_id;
		queued.tool = queue_tool;
		queued.args = queue_args_shared.duplicate(true);
		{
			MutexLock lock(g_policy_mutex);
			g_queue.push_back(queued);
		}
		Dictionary early;
		early["ok"] = true;
		early["queued"] = true;
		early["holder"] = queue_holder;
		r_early = early;
		return true;
	}
	if (!snapshot_path.is_empty()) {
		note_file_undo(snapshot_path);
	}
	if (!transfer_from.is_empty()) {
		_note_transfer_undo(transfer_from, transfer_to, transfer_move);
	}
	if (checkpoint_now) {
		make_checkpoint(checkpoint_paths);
	}
	return false;
}

Dictionary JustAMCPAgentPolicy::after_execute(const String &p_tool_name, const Dictionary &p_args, Dictionary p_result) {
	if (!_on_main_thread()) {
		_count_result_bytes(p_result);
		return p_result;
	}
	if (g_policy_depth <= 0) {
		return p_result;
	}
	bool need_stuck_checkpoint = false;
	String stuck_session;
	bool count_bytes = false;
	bool want_suggestion = false;
	Vector<String> suggestion_names;
	String suggestion_wanted;
	bool store_idem = false;
	String idem_key;
	{
		MutexLock lock(g_policy_mutex);
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
			_push_audit(row);
			count_bytes = true;
		} else {
			JustAMCPAgentSession &session = _session(session_id);
			session.calls++;

			if (bool(p_result.get("idempotent_replay", false)) || bool(p_result.get("dry_run", false)) || bool(p_result.get("queued", false)) || bool(p_result.get("nested_write", false))) {
				Dictionary row;
				row["tool"] = name;
				row["session_id"] = session_id;
				row["ok"] = !_failed(p_result);
				_push_audit(row);
				count_bytes = true;
			} else {
				const bool failed = _failed(p_result);
				if (!failed && _read_tool(name)) {
					session.has_read = true;
				}
				if (!failed && !_read_tool(name) && !_control_tool(name)) {
					if (name != "checkpoint") {
						g_git_head_valid = false;
					}
					session.revision++;
					if (p_result.has("before") && p_result.has("after")) {
						Dictionary read_back;
						read_back["before"] = p_result["before"];
						read_back["after"] = p_result["after"];
						p_result["read_back"] = read_back;
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
							need_stuck_checkpoint = true;
							stuck_session = session_id;
						} else {
							p_result["stuck"] = true;
							p_result["checkpoint_id"] = checkpoint_id;
						}
					}
				}
				if (p_result.has("error")) {
					String message;
					if (p_result["error"].get_type() == Variant::DICTIONARY) {
						Dictionary error = p_result["error"];
						message = String(error.get("message", ""));
					} else {
						message = String(p_result.get("error", ""));
					}
					if (message.contains("not found") || message.contains("Unknown tool")) {
						suggestion_wanted = _internal_name(p_tool_name);
						suggestion_names = _copy_suggestion_names(suggestion_wanted);
						want_suggestion = true;
					}
					if (message.contains("unreachable") || message.contains("viewport") || message.contains("Editor GUI")) {
						p_result["needs_editor_ui"] = true;
					}
				}
				if (!failed && (name == "editor_take_screenshot" || name == "editor_screenshot_game" || name == "take_game_screenshot")) {
					const bool inline_bytes = bool(p_args.get("inline", false));
					p_result["resource_uri"] = "blazium://screenshot/latest";
					if (p_result.has("cropped_to")) {
						p_result["diff_summary"] = "cropped to " + String(p_result.get("cropped_to", ""));
					} else {
						p_result["diff_summary"] = "image stored as a resource link";
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
					store_idem = true;
					idem_key = session_id + "\n" + name + "\n" + idem;
				}
				Dictionary row;
				row["tool"] = name;
				row["session_id"] = session_id;
				row["ok"] = !failed;
				row["revision"] = _session(session_id).revision;
				_push_audit(row);
				count_bytes = true;
			}
		}
	}
	if (want_suggestion) {
		const String suggestion = _suggest_from_names(suggestion_wanted, suggestion_names);
		if (!suggestion.is_empty()) {
			if (p_result["error"].get_type() == Variant::DICTIONARY) {
				Dictionary error = p_result["error"];
				error["message"] = String(error.get("message", "")) + " Did you mean " + suggestion + "?";
				error["suggestion"] = suggestion;
				p_result["error"] = error;
			} else {
				p_result["error"] = String(p_result.get("error", "")) + " Did you mean " + suggestion + "?";
				p_result["suggestion"] = suggestion;
			}
		}
	}
	if (store_idem) {
		const Dictionary stored = p_result.duplicate();
		MutexLock lock(g_policy_mutex);
		_store_idempotent(idem_key, stored);
	}
	if (count_bytes) {
		_count_result_bytes(p_result);
	}
	if (need_stuck_checkpoint) {
		const String checkpoint_id = String(make_checkpoint(Array()).get("checkpoint_id", ""));
		{
			MutexLock lock(g_policy_mutex);
			if (g_sessions.has(stuck_session)) {
				g_sessions[stuck_session].last_checkpoint = checkpoint_id;
			}
		}
		p_result["stuck"] = true;
		p_result["checkpoint_id"] = checkpoint_id;
	}
	if (g_snapshot_skipped) {
		g_snapshot_skipped = false;
		p_result["snapshot_skipped"] = true;
		if (!p_result.has("message")) {
			p_result["message"] = "snapshot skipped";
		}
	}
	return p_result;
}

void JustAMCPAgentPolicy::note_grouped_undo(int p_steps) {
	MutexLock lock(g_policy_mutex);
	if (p_steps > 1) {
		g_grouped_undo = p_steps;
	}
}

int JustAMCPAgentPolicy::take_grouped_undo() {
	MutexLock lock(g_policy_mutex);
	const int steps = g_grouped_undo;
	g_grouped_undo = 0;
	return steps;
}

void JustAMCPAgentPolicy::clear_grouped_undo() {
	MutexLock lock(g_policy_mutex);
	g_grouped_undo = 0;
}

void JustAMCPAgentPolicy::note_batch_undo(const Array &p_property_restores, const Array &p_added_node_ids) {
	if (p_property_restores.is_empty() && p_added_node_ids.is_empty()) {
		return;
	}
	MutexLock lock(g_policy_mutex);
	JustAMCPUndoEntry entry;
	entry.property_restores = p_property_restores.duplicate();
	entry.added_node_ids = p_added_node_ids.duplicate();
	g_undo.push_back(entry);
	_trim_undo();
}

void JustAMCPAgentPolicy::note_file_undo(const String &p_path) {
	if (!_safe_project_path(p_path)) {
		return;
	}
	PackedByteArray bytes;
	bool binary = false;
	if (!_capture_file_bytes(p_path, bytes, binary)) {
		return;
	}
	String previous;
	if (!binary) {
		if (bytes.is_empty()) {
			previous = String();
		} else {
			previous = String::utf8((const char *)bytes.ptr(), bytes.size());
		}
		bytes = PackedByteArray();
	}
	MutexLock lock(g_policy_mutex);
	JustAMCPUndoEntry entry;
	entry.file = true;
	entry.binary = binary;
	entry.path = p_path;
	entry.previous = previous;
	entry.previous_bytes = bytes;
	g_undo.push_back(entry);
	_trim_undo();
}

void JustAMCPAgentPolicy::note_tile_undo(const Array &p_cells) {
	if (p_cells.is_empty()) {
		return;
	}
	MutexLock lock(g_policy_mutex);
	JustAMCPUndoEntry entry;
	entry.tile_restores = p_cells.duplicate();
	g_undo.push_back(entry);
	_trim_undo();
}

int JustAMCPAgentPolicy::undo_snapshots(int p_steps) {
	MutexLock lock(g_policy_mutex);
	int restored = 0;
	for (int i = 0; i < p_steps && !g_undo.is_empty(); i++) {
		const JustAMCPUndoEntry entry = g_undo[g_undo.size() - 1];
		g_undo.remove_at(g_undo.size() - 1);
		if (entry.moved || entry.copied) {
			const JustAMCPUndoEntry transfer = entry;
			g_policy_mutex.unlock();
			_restore_transfer(transfer);
			g_policy_mutex.lock();
		} else if (entry.file) {
			const String path = entry.path;
			const String previous = entry.previous;
			const bool binary = entry.binary;
			const PackedByteArray previous_bytes = entry.previous_bytes;
			g_policy_mutex.unlock();
			if (binary) {
				_write_bytes(path, previous_bytes);
			} else {
				Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
				if (file.is_valid()) {
					file->store_string(previous);
				}
			}
			g_policy_mutex.lock();
		} else if (!entry.tile_restores.is_empty()) {
			const Array cells = entry.tile_restores;
			g_policy_mutex.unlock();
			for (int cell_index = 0; cell_index < cells.size(); cell_index++) {
				const Dictionary row = cells[cell_index];
				Object *object = ObjectDB::get_instance(ObjectID(uint64_t(row.get("id", 0))));
				const Vector2i coords(int(row.get("x", 0)), int(row.get("y", 0)));
				const Vector2i atlas(int(row.get("atlas_x", -1)), int(row.get("atlas_y", -1)));
				const int source_id = int(row.get("source_id", -1));
				const int alternative = int(row.get("alternative", 0));
				if (bool(row.get("layer_node", false))) {
					if (TileMapLayer *layer = Object::cast_to<TileMapLayer>(object)) {
						layer->set_cell(coords, source_id, atlas, alternative);
					}
				} else if (TileMap *map = Object::cast_to<TileMap>(object)) {
					map->set_cell(int(row.get("layer_index", 0)), coords, source_id, atlas, alternative);
				}
			}
			g_policy_mutex.lock();
		} else if (!entry.property_restores.is_empty() || !entry.added_node_ids.is_empty()) {
			const Array properties = entry.property_restores;
			const Array added = entry.added_node_ids;
			g_policy_mutex.unlock();
			for (int property_index = 0; property_index < properties.size(); property_index++) {
				const Dictionary row = properties[property_index];
				Object *object = ObjectDB::get_instance(ObjectID(uint64_t(row.get("id", 0))));
				if (object) {
					object->set(row.get("property", StringName()), row.get("value", Variant()));
				}
			}
			for (int node_index = added.size() - 1; node_index >= 0; node_index--) {
				Node *node = Object::cast_to<Node>(ObjectDB::get_instance(ObjectID(uint64_t(added[node_index]))));
				if (node && node->get_parent()) {
					node->get_parent()->remove_child(node);
					memdelete(node);
				}
			}
			g_policy_mutex.lock();
		} else {
			g_probe = entry.probe_value;
		}
		restored++;
	}
	return restored;
}

void JustAMCPAgentPolicy::note_tool_name(const String &p_name) {
	MutexLock lock(g_policy_mutex);
	if (p_name.is_empty()) {
		return;
	}
	for (int i = 0; i < g_tool_names.size(); i++) {
		if (g_tool_names[i] == p_name) {
			return;
		}
	}
	g_tool_names.push_back(p_name);
	const int length = _internal_name(p_name).length();
	Vector<int> bucket;
	if (g_tools_by_length.has(length)) {
		bucket = g_tools_by_length[length];
	}
	bucket.push_back(g_tool_names.size() - 1);
	g_tools_by_length.erase(length);
	g_tools_by_length.insert(length, bucket);
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
	Vector<String> names;
	{
		MutexLock lock(g_policy_mutex);
		names = _copy_suggestion_names(wanted);
	}
	return _suggest_from_names(wanted, names);
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
		r_body = "GDScript is validated with the engine parser. Luau writes are compiled with the Luau parser and are not executed. C# files are written as given.";
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
	return p_canonical == "blazium://logs/actions" || p_canonical == "blazium://session/usage" || p_canonical == "blazium://reference/tools" || p_canonical == "blazium://mcp/clients" || p_canonical == "blazium://mcp/compatibility" || p_canonical == "blazium://mcp/benchmark_tasks" || p_canonical == "blazium://meta/tools_list_bytes" || p_canonical == "blazium://project/conventions" || p_canonical == "blazium://screenshot/latest";
}

Dictionary JustAMCPAgentPolicy::read_agent_resource(const String &p_uri, const String &p_canonical) {
	if (p_canonical == "blazium://logs/actions") {
		Dictionary payload;
		payload["entries"] = audit_log();
		return JustAMCPResourceJson::make_json_contents(p_uri, payload);
	}
	if (p_canonical == "blazium://session/usage") {
		return JustAMCPResourceJson::make_json_contents(p_uri, usage_report());
	}
	if (p_canonical == "blazium://reference/tools") {
		return JustAMCPResourceJson::make_text_contents(p_uri, _catalog_text(), "text/plain");
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
		String protocol = "2025-11-25";
		if (ProjectSettings::get_singleton()) {
			protocol = String(ProjectSettings::get_singleton()->get_setting("blazium/justamcp/protocol_version", protocol));
		}
		payload["protocol_version"] = protocol;
		payload["license"] = "The editor MCP ships with the engine at no extra license.";
		payload["engine_version"] = Engine::get_singleton() ? String(Engine::get_singleton()->get_version_info().get("string", "")) : String();
		return JustAMCPResourceJson::make_json_contents(p_uri, payload);
	}
	if (p_canonical == "blazium://mcp/benchmark_tasks") {
		Dictionary payload;
		Array tasks;
		tasks.push_back("project_map");
		tasks.push_back("validate_script");
		tasks.push_back("editor_take_screenshot");
		tasks.push_back("verify_game_change");
		payload["tasks"] = tasks;
		return JustAMCPResourceJson::make_json_contents(p_uri, payload);
	}
	if (p_canonical == "blazium://meta/tools_list_bytes") {
		Dictionary payload;
		const String catalog = _catalog_text();
		int tool_count = 0;
		{
			MutexLock lock(g_policy_mutex);
			tool_count = g_tool_names.size();
		}
		payload["tool_names"] = tool_count;
		payload["bytes"] = catalog.utf8().length();
		return JustAMCPResourceJson::make_json_contents(p_uri, payload);
	}
	if (p_canonical == "blazium://project/conventions") {
		Dictionary payload;
		payload["bone_name"] = "^[A-Za-z_][A-Za-z0-9_]*$";
		payload["note"] = "Bone and node names are letters, digits, and underscores, and do not start with a digit.";
		return JustAMCPResourceJson::make_json_contents(p_uri, payload);
	}
	String summary;
	{
		MutexLock lock(g_policy_mutex);
		summary = g_screenshot_summary;
	}
	return JustAMCPResourceJson::make_text_contents(p_uri, summary, "text/plain");
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
	note_file_undo(p_path);
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
	MutexLock lock(g_policy_mutex);
	return g_probe;
}

void JustAMCPAgentPolicy::probe_reset() {
	MutexLock lock(g_policy_mutex);
	g_probe = 0;
	g_undo.clear();
	g_grouped_undo = 0;
	g_idempotency.clear();
	g_idempotency_order.clear();
}

int JustAMCPAgentPolicy::probe_increment() {
	MutexLock lock(g_policy_mutex);
	const int previous = g_probe;
	g_probe++;
	_push_undo_probe(previous);
	return g_probe;
}

Dictionary JustAMCPAgentPolicy::claim_path(const String &p_path, bool p_subtree) {
	const int wait_ms = JustAMCPSettingsResolver::resolve_int("blazium/justamcp/claim_wait_ms", 30000);
	MutexLock lock(g_policy_mutex);
	if (!_safe_project_path(p_path)) {
		return _err("claim path must be res:// or user://");
	}
	const String session_id = current_session_id();
	if (_session_closed(session_id)) {
		return _err("session is closed");
	}
	const JustAMCPAgentSession &session = _session(session_id);
	for (const KeyValue<String, JustAMCPClaim> &entry : g_claims) {
		if (entry.value.session_id == session_id) {
			continue;
		}
		if (_claim_covers(entry.key, entry.value.subtree, p_path) || _claim_covers(p_path, p_subtree, entry.key)) {
			Dictionary result = _err("path is claimed");
			result["holder"] = entry.value.holder;
			result["timeout"] = wait_ms == 0;
			result["busy"] = wait_ms != 0;
			return result;
		}
	}
	JustAMCPClaim claim;
	claim.path = p_path;
	claim.session_id = session_id;
	claim.holder = session.name;
	claim.subtree = p_subtree;
	g_claims.erase(p_path);
	g_claims.insert(p_path, claim);
	Dictionary result;
	result["ok"] = true;
	result["path"] = p_path;
	result["holder"] = claim.holder;
	result["subtree"] = p_subtree;
	return result;
}

Dictionary JustAMCPAgentPolicy::release_claim(const String &p_path) {
	bool subtree = false;
	int before = 0;
	{
		MutexLock lock(g_policy_mutex);
		if (!g_claims.has(p_path)) {
			return _err("claim not found");
		}
		if (g_claims[p_path].session_id != current_session_id() && current_session_id() != "editor") {
			Dictionary result = _err("only the holder can release");
			result["holder"] = g_claims[p_path].holder;
			return result;
		}
		subtree = g_claims[p_path].subtree;
		g_claims.erase(p_path);
		before = g_probe;
	}
	const Array replay = _apply_queued(p_path, subtree);
	int after = before;
	{
		MutexLock lock(g_policy_mutex);
		after = g_probe;
	}
	Dictionary result;
	result["ok"] = true;
	result["released"] = p_path;
	result["applied"] = after - before;
	result["replay"] = replay;
	return result;
}

Array JustAMCPAgentPolicy::list_claims() {
	Vector<JustAMCPClaim> copied;
	{
		MutexLock lock(g_policy_mutex);
		for (const KeyValue<String, JustAMCPClaim> &entry : g_claims) {
			copied.push_back(entry.value);
		}
	}
	Array claims;
	for (int i = 0; i < copied.size(); i++) {
		Dictionary claim;
		claim["path"] = copied[i].path;
		claim["session_id"] = copied[i].session_id;
		claim["holder"] = copied[i].holder;
		claim["subtree"] = copied[i].subtree;
		claims.push_back(claim);
	}
	return claims;
}

Dictionary JustAMCPAgentPolicy::make_checkpoint(const Array &p_paths) {
	const String head = _git_head_for_checkpoint();
	Dictionary files;
	Dictionary binary_files;
	for (int i = 0; i < p_paths.size(); i++) {
		const String path = String(p_paths[i]);
		if (!_safe_project_path(path) || !FileAccess::exists(path)) {
			continue;
		}
		PackedByteArray bytes;
		bool binary = false;
		if (!_capture_file_bytes(path, bytes, binary)) {
			continue;
		}
		if (binary) {
			binary_files[path] = bytes;
		} else if (bytes.is_empty()) {
			files[path] = String();
		} else {
			files[path] = String::utf8((const char *)bytes.ptr(), bytes.size());
		}
	}
	JustAMCPCheckpoint checkpoint;
	checkpoint.id = "ckpt-" + String::num_uint64(Time::get_singleton()->get_ticks_usec());
	checkpoint.git_head = head;
	checkpoint.files = files;
	checkpoint.binary_files = binary_files;
	const int file_count = files.size() + binary_files.size();
	{
		MutexLock lock(g_policy_mutex);
		g_checkpoints.insert(checkpoint.id, checkpoint);
		g_checkpoint_order.push_back(checkpoint.id);
		while (g_checkpoint_order.size() > k_checkpoint_cap) {
			const String old_id = g_checkpoint_order[0];
			g_checkpoint_order.remove_at(0);
			g_checkpoints.erase(old_id);
		}
		const String session_id = g_session_stack.is_empty() ? String("editor") : g_session_stack[g_session_stack.size() - 1];
		if (g_sessions.has(session_id)) {
			g_sessions[session_id].last_checkpoint = checkpoint.id;
		}
	}
	Dictionary result;
	result["ok"] = true;
	result["checkpoint_id"] = checkpoint.id;
	result["files"] = file_count;
	result["git_head"] = checkpoint.git_head;
	return result;
}

Array JustAMCPAgentPolicy::list_checkpoints() {
	MutexLock lock(g_policy_mutex);
	Array ids;
	for (int i = 0; i < g_checkpoint_order.size(); i++) {
		ids.push_back(g_checkpoint_order[i]);
	}
	return ids;
}

Dictionary JustAMCPAgentPolicy::diff_checkpoint(const String &p_id) {
	Dictionary files;
	Dictionary binary_files;
	{
		MutexLock lock(g_policy_mutex);
		if (!g_checkpoints.has(p_id)) {
			return _err("checkpoint not found");
		}
		files = g_checkpoints[p_id].files;
		binary_files = g_checkpoints[p_id].binary_files;
	}
	const bool skipped_before = g_snapshot_skipped;
	Array changed;
	Array text_paths = files.keys();
	for (int i = 0; i < text_paths.size(); i++) {
		const String path = text_paths[i];
		const String previous = files[path];
		const String current = FileAccess::exists(path) ? FileAccess::get_file_as_string(path) : String();
		if (current != previous) {
			Dictionary row;
			row["path"] = path;
			row["changed"] = true;
			changed.push_back(row);
		}
	}
	Array binary_paths = binary_files.keys();
	for (int i = 0; i < binary_paths.size(); i++) {
		const String path = binary_paths[i];
		const PackedByteArray previous = binary_files[path];
		PackedByteArray current;
		bool binary = false;
		const bool readable = _capture_file_bytes(path, current, binary);
		if (!readable || !binary || current != previous) {
			Dictionary row;
			row["path"] = path;
			row["changed"] = true;
			changed.push_back(row);
		}
	}
	g_snapshot_skipped = skipped_before;
	Dictionary result;
	result["ok"] = true;
	result["checkpoint_id"] = p_id;
	result["changed"] = changed;
	return result;
}

Dictionary JustAMCPAgentPolicy::restore_checkpoint(const String &p_id) {
	Dictionary files;
	Dictionary binary_files;
	{
		MutexLock lock(g_policy_mutex);
		if (!g_checkpoints.has(p_id)) {
			return _err("checkpoint not found");
		}
		files = g_checkpoints[p_id].files;
		binary_files = g_checkpoints[p_id].binary_files;
	}
	int restored = 0;
	Array text_paths = files.keys();
	for (int i = 0; i < text_paths.size(); i++) {
		const String path = text_paths[i];
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
		if (file.is_valid()) {
			file->store_string(String(files[path]));
			restored++;
		}
	}
	Array binary_paths = binary_files.keys();
	for (int i = 0; i < binary_paths.size(); i++) {
		const String path = binary_paths[i];
		_write_bytes(path, binary_files[path]);
		restored++;
	}
	Dictionary result;
	result["ok"] = true;
	result["checkpoint_id"] = p_id;
	result["restored"] = restored;
	return result;
}

String JustAMCPAgentPolicy::last_checkpoint_id() {
	MutexLock lock(g_policy_mutex);
	if (g_checkpoint_order.is_empty()) {
		return String();
	}
	return g_checkpoint_order[g_checkpoint_order.size() - 1];
}

Dictionary JustAMCPAgentPolicy::apply_change_plan(const String &p_plan_id) {
	String tool;
	Dictionary args;
	{
		MutexLock lock(g_policy_mutex);
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
		if (plan.tool == "apply_change_plan" || plan.tool == "revert_change_plan") {
			return _err("change plan cannot dispatch itself");
		}
		tool = plan.tool;
		args = plan.args;
	}
	args = args.duplicate(true);
	JustAMCPToolExecutor *executor = JustAMCPToolExecutor::get_active_instance();
	if (!executor) {
		return _err("tool executor is not active");
	}
	const Dictionary executed = executor->execute_tool_direct(tool, args);
	if (_failed(executed) || bool(executed.get("queued", false))) {
		Dictionary result = executed.duplicate();
		result["applied"] = false;
		result["plan_id"] = p_plan_id;
		return result;
	}
	{
		MutexLock lock(g_policy_mutex);
		if (!g_plans.has(p_plan_id)) {
			return _err("change plan not found");
		}
		JustAMCPChangePlan &plan = g_plans[p_plan_id];
		plan.applied = true;
		plan.snapshot = _file_snapshot_tool(tool);
		plan.revision_at = _session(plan.session_id).revision - 1;
	}
	Dictionary result;
	result["ok"] = true;
	result["applied"] = true;
	result["plan_id"] = p_plan_id;
	result["tool"] = tool;
	result["nested_write"] = true;
	return result;
}

Dictionary JustAMCPAgentPolicy::revert_change_plan(const String &p_plan_id) {
	bool undo = false;
	Dictionary result;
	{
		MutexLock lock(g_policy_mutex);
		if (!g_plans.has(p_plan_id)) {
			return _err("change plan not found");
		}
		JustAMCPChangePlan &plan = g_plans[p_plan_id];
		JustAMCPAgentSession &session = _session(plan.session_id);
		if (!plan.applied || session.revision != plan.revision_at + 1) {
			result = _err("revert revision mismatch");
			result["mismatch"] = true;
			result["applied"] = plan.applied;
			result["revision"] = session.revision;
			return result;
		}
		undo = plan.tool == "agent_probe_increment" || plan.snapshot;
		plan.applied = false;
		result["ok"] = true;
		result["mismatch"] = false;
		result["plan_id"] = p_plan_id;
		result["value"] = g_probe;
	}
	if (undo) {
		undo_snapshots(1);
		MutexLock lock(g_policy_mutex);
		result["value"] = g_probe;
	}
	return result;
}

Array JustAMCPAgentPolicy::audit_log() {
	Array slots;
	int count = 0;
	int next = 0;
	{
		MutexLock lock(g_policy_mutex);
		slots = g_audit_slots.duplicate();
		count = g_audit_count;
		next = g_audit_next;
	}
	Array ordered;
	if (count <= 0 || slots.is_empty()) {
		return ordered;
	}
	const int start = count < k_audit_cap ? 0 : next;
	for (int i = 0; i < count; i++) {
		ordered.push_back(slots[(start + i) % k_audit_cap]);
	}
	return ordered;
}

Dictionary JustAMCPAgentPolicy::usage_report() {
	Vector<JustAMCPAgentSession> copied;
	int bytes = 0;
	{
		MutexLock lock(g_policy_mutex);
		for (const KeyValue<String, JustAMCPAgentSession> &entry : g_sessions) {
			copied.push_back(entry.value);
		}
		bytes = g_result_bytes;
	}
	Dictionary usage;
	Array sessions;
	int calls = 0;
	for (int i = 0; i < copied.size(); i++) {
		Dictionary row = _session_dict(copied[i]);
		calls += copied[i].calls;
		sessions.push_back(row);
	}
	usage["calls"] = calls;
	usage["bytes"] = bytes;
	usage["call_cap"] = k_session_call_cap;
	if (calls > k_session_call_cap) {
		usage["warning"] = "session call cap exceeded";
	}
	usage["sessions"] = sessions;
	return usage;
}

void JustAMCPAgentPolicy::store_screenshot_summary(const String &p_summary) {
	MutexLock lock(g_policy_mutex);
	g_screenshot_summary = p_summary;
}

void JustAMCPAgentPolicy::store_scene_baseline(const Array &p_rows) {
	const Array copy = p_rows.duplicate();
	MutexLock lock(g_policy_mutex);
	g_scene_baseline = copy;
}

Array JustAMCPAgentPolicy::copy_scene_baseline() {
	Array shared;
	{
		MutexLock lock(g_policy_mutex);
		shared = g_scene_baseline;
	}
	return shared.duplicate();
}

Dictionary JustAMCPAgentPolicy::commit_knobs(const Dictionary &p_knobs) {
	String path;
	bool set_scale = false;
	double scale = 1.0;
	bool write_file = false;
	Dictionary stored = p_knobs.duplicate();
	path = String(stored.get("path", ""));
	stored.erase("path");
	{
		MutexLock lock(g_policy_mutex);
		Array keys = stored.keys();
		for (int i = 0; i < keys.size(); i++) {
			g_knobs[keys[i]] = stored[keys[i]];
		}
		set_scale = stored.has("time_scale");
		scale = set_scale ? double(stored["time_scale"]) : 1.0;
		write_file = path.ends_with(".json") && _safe_project_path(path);
	}
	const String json = write_file ? JSON::stringify(stored) : String();
	Dictionary result;
	result["ok"] = true;
	if (set_scale && Engine::get_singleton()) {
		Engine::get_singleton()->set_time_scale(scale);
		result["time_scale"] = Engine::get_singleton()->get_time_scale();
	}
	if (write_file) {
		note_file_undo(path);
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
		if (file.is_valid()) {
			file->store_string(json);
			result["written"] = path;
		}
	}
	Dictionary knobs_copy;
	{
		MutexLock lock(g_policy_mutex);
		knobs_copy = g_knobs;
	}
	result["knobs"] = knobs_copy.duplicate();
	return result;
}

Dictionary JustAMCPAgentPolicy::current_knobs() {
	Dictionary shared;
	{
		MutexLock lock(g_policy_mutex);
		shared = g_knobs;
	}
	return shared.duplicate();
}

#endif
