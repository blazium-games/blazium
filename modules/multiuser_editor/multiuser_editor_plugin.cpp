/**************************************************************************/
/*  multiuser_editor_plugin.cpp                                           */
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

#include "multiuser_editor_plugin.h"

#include "multiuser_editor_constants.h"
#include "multiuser_editor_dock.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/marshalls.h"
#include "core/io/resource.h"
#include "core/object/worker_thread_pool.h"
#include "core/os/os.h"
#include "editor/editor_file_system.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/plugins/script_editor_plugin.h"
#include "modules/autowork/autowork_main.h"
#include "modules/dotenv/env.h"
#include "modules/jwttool/jwt.h"
#include "scene/gui/label.h"
#include "scene/main/timer.h"
#include "scene/resources/material.h"
#include "scene/resources/shader.h"
#include "scene/resources/surface_tool.h"
#include "scene/resources/visual_shader.h"

#ifdef MODULE_JUSTAMCP_ENABLED
#include "modules/justamcp/justamcp_server.h"
#endif

static const double MULTIUSER_POLL_INTERVAL_SEC = 0.05;
static const double MULTIUSER_CURSOR_INTERVAL_SEC = 0.08;
static const double MULTIUSER_SYNC_PENDING_TIMEOUT_SEC = 3.0;

static const char *MULTIUSER_SENSITIVE_SETTING_PREFIXES[] = {
	"editor/",
	"application/run/main_run_args",
	"dotnet/",
	"application/config/use_custom_user_dir",
	"application/config/custom_user_dir_name",
	"application/config/project_settings_override",
	"blazium/multiuser_editor/",
	"network/ssl/",
	"network/tls/",
	"network/limits/tcp/",
	"network/limits/packet_peer_stream/",
	"debug/file_logging/",
	"debug/settings/crash_handler/",
	"debug/shapes/",
	"filesystem/import/",
	"filesystem/on_save/",
	"editor_plugins/",
	nullptr,
};

static bool _mu_path_has_sensitive_prefix(const String &p_path) {
	for (int i = 0; MULTIUSER_SENSITIVE_SETTING_PREFIXES[i] != nullptr; i++) {
		if (p_path.begins_with(MULTIUSER_SENSITIVE_SETTING_PREFIXES[i])) {
			return true;
		}
	}
	return false;
}

static Dictionary _make_action(const String &p_type, const Dictionary &p_data) {
	Dictionary action;
	action["type"] = p_type;
	action["data"] = p_data;
	return action;
}

static PackedStringArray _read_packed_string_array_setting(const String &p_key, const Vector<String> &p_defaults) {
	PackedStringArray out;
	if (EditorSettings::get_singleton() && EditorSettings::get_singleton()->has_setting(p_key)) {
		const Variant v = EditorSettings::get_singleton()->get(p_key);
		if (v.get_type() == Variant::PACKED_STRING_ARRAY) {
			return v;
		}
		if (v.get_type() == Variant::ARRAY) {
			const Array a = v;
			for (int i = 0; i < a.size(); i++) {
				out.push_back(String(a[i]));
			}
			return out;
		}
	}
	for (int i = 0; i < p_defaults.size(); i++) {
		out.push_back(p_defaults[i]);
	}
	return out;
}

static CodeEdit *multiuser_find_code_edit(Node *p_node) {
	if (!p_node) {
		return nullptr;
	}
	if (CodeEdit *code_edit = Object::cast_to<CodeEdit>(p_node)) {
		return code_edit;
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		if (CodeEdit *found = multiuser_find_code_edit(p_node->get_child(i))) {
			return found;
		}
	}
	return nullptr;
}

MultiuserEditorPlugin *MultiuserEditorPlugin::singleton = nullptr;
bool MultiuserEditorPlugin::s_cli_join_pending = false;
uint64_t MultiuserEditorPlugin::s_cli_join_after_usec = 0;
uint64_t MultiuserEditorPlugin::s_cli_editor_ready_since_usec = 0;
bool MultiuserEditorPlugin::s_server_host_pending = false;
int MultiuserEditorPlugin::s_server_host_port = 0;
String MultiuserEditorPlugin::s_server_host_password;
uint64_t MultiuserEditorPlugin::s_server_host_after_usec = 0;
uint64_t MultiuserEditorPlugin::s_server_host_ready_since_usec = 0;

MultiuserEditorPlugin *MultiuserEditorPlugin::get_singleton() {
	return singleton;
}

void MultiuserEditorPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_poll_runtime"), &MultiuserEditorPlugin::_poll_runtime);
	ClassDB::bind_method(D_METHOD("_on_selection_changed"), &MultiuserEditorPlugin::_on_selection_changed);
	ClassDB::bind_method(D_METHOD("_on_node_added", "node"), &MultiuserEditorPlugin::_on_node_added);
	ClassDB::bind_method(D_METHOD("_on_node_removed", "node"), &MultiuserEditorPlugin::_on_node_removed);
	ClassDB::bind_method(D_METHOD("_on_network_peer_disconnected", "net_id"), &MultiuserEditorPlugin::_on_network_peer_disconnected);
	ClassDB::bind_method(D_METHOD("_on_inspector_property_changed", "undo_redo", "modified_object", "property", "new_value"), &MultiuserEditorPlugin::_on_inspector_property_changed);
	ClassDB::bind_method(D_METHOD("_on_project_settings_changed"), &MultiuserEditorPlugin::_on_project_settings_changed);
	ClassDB::bind_method(D_METHOD("_on_editor_settings_changed"), &MultiuserEditorPlugin::_on_editor_settings_changed);
	ClassDB::bind_method(D_METHOD("_on_scene_saved", "filepath"), &MultiuserEditorPlugin::_on_scene_saved);
	ClassDB::bind_method(D_METHOD("_on_filesystem_changed"), &MultiuserEditorPlugin::_on_filesystem_changed);
	ClassDB::bind_method(D_METHOD("_on_resources_reimported", "resources"), &MultiuserEditorPlugin::_on_resources_reimported);
	ClassDB::bind_method(D_METHOD("_on_sources_changed", "exist"), &MultiuserEditorPlugin::_on_sources_changed);
	ClassDB::bind_method(D_METHOD("_on_fs_debounce_timeout"), &MultiuserEditorPlugin::_on_fs_debounce_timeout);
	ClassDB::bind_method(D_METHOD("send_chat", "message"), &MultiuserEditorPlugin::send_chat);
	ClassDB::bind_method(D_METHOD("toggle_follow_peer", "peer_id"), &MultiuserEditorPlugin::toggle_follow_peer);
	ClassDB::bind_method(D_METHOD("trigger_autowork"), &MultiuserEditorPlugin::trigger_autowork);
	ClassDB::bind_method(D_METHOD("host_session", "port", "password"), &MultiuserEditorPlugin::host_session, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("join_session", "host", "port", "password"), &MultiuserEditorPlugin::join_session, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("stop_session"), &MultiuserEditorPlugin::stop_session);
	ClassDB::bind_method(D_METHOD("request_git_op", "op"), &MultiuserEditorPlugin::request_git_op);
	ClassDB::bind_method(D_METHOD("request_git_op_with_branch", "op", "branch"), &MultiuserEditorPlugin::request_git_op_with_branch);
	ClassDB::bind_method(D_METHOD("request_git_op_commit", "message"), &MultiuserEditorPlugin::request_git_op_commit);
	ClassDB::bind_method(D_METHOD("kick_peer", "peer_id"), &MultiuserEditorPlugin::kick_peer);
	ClassDB::bind_method(D_METHOD("create_checkpoint", "name"), &MultiuserEditorPlugin::create_checkpoint);
	ClassDB::bind_method(D_METHOD("reload_access_list"), &MultiuserEditorPlugin::reload_access_list);
	ClassDB::bind_method(D_METHOD("is_session_connected"), &MultiuserEditorPlugin::is_session_connected);
	ClassDB::bind_method(D_METHOD("is_local_admin"), &MultiuserEditorPlugin::is_local_admin);
	ClassDB::bind_method(D_METHOD("get_local_peer_id"), &MultiuserEditorPlugin::get_local_peer_id);

	ClassDB::bind_method(D_METHOD("host_from_settings"), &MultiuserEditorPlugin::host_from_settings);
	ClassDB::bind_method(D_METHOD("join_from_settings"), &MultiuserEditorPlugin::join_from_settings);
	ClassDB::bind_method(D_METHOD("can_host_sessions"), &MultiuserEditorPlugin::can_host_sessions);
	ClassDB::bind_method(D_METHOD("get_status_text"), &MultiuserEditorPlugin::get_status_text);
	ClassDB::bind_method(D_METHOD("jump_to_peer", "peer_id"), &MultiuserEditorPlugin::jump_to_peer);
	ClassDB::bind_method(D_METHOD("load_checkpoint", "index"), &MultiuserEditorPlugin::load_checkpoint);
	ClassDB::bind_method(D_METHOD("save_session"), &MultiuserEditorPlugin::save_session);
	ClassDB::bind_method(D_METHOD("load_session"), &MultiuserEditorPlugin::load_session);
	ClassDB::bind_method(D_METHOD("request_magic_repair"), &MultiuserEditorPlugin::request_magic_repair);
	ClassDB::bind_static_method("MultiuserEditorPlugin", D_METHOD("validate_jwt_static", "jwt", "secret", "config"), &MultiuserEditorPlugin::validate_jwt_static_d);
	ClassDB::bind_method(D_METHOD("get_recent_security_events_snapshot", "max"), &MultiuserEditorPlugin::get_recent_security_events_snapshot_array, DEFVAL(32));
}

MultiuserEditorPlugin::MultiuserEditorPlugin() {
	print_line("[Multiuser Editor] Native Plugin Instantiated Successfully in C++ Engine Module.");
	uint64_t seed = uint64_t(OS::get_singleton()->get_unix_time()) ^ OS::get_singleton()->get_ticks_usec();
	local_peer_id = "peer_" + String::num_uint64(seed, 16).sha256_text().substr(0, 8);
	script_sync.set_local_peer_id(local_peer_id);
	permissions.instantiate();
	permissions->load_defaults();
	_run_known_action_self_check();
	_git_shared.instantiate();
}

MultiuserEditorPlugin::~MultiuserEditorPlugin() {
	print_line("[Multiuser Editor] Native Plugin Destructor Executed!");
	if (singleton == this) {
		singleton = nullptr;
	}
}

void MultiuserEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			singleton = this;
			{
				multiuser_editor::SecuritySink sink;
				sink.fn = &MultiuserEditorPlugin::_security_sink_thunk;
				sink.user = this;
				network.set_security_sink(sink);
				action_interceptor.set_security_sink(sink);
				filesystem_sync.set_security_sink(sink);
				lock_manager.set_security_sink(sink);
				script_sync.set_security_sink(sink);
			}
			selection = EditorInterface::get_singleton()->get_selection();
			undo_redo = EditorUndoRedoManager::get_singleton();
			if (selection && !selection->is_connected("selection_changed", callable_mp(this, &MultiuserEditorPlugin::_on_selection_changed))) {
				selection->connect("selection_changed", callable_mp(this, &MultiuserEditorPlugin::_on_selection_changed));
			}
			if (get_tree()) {
				get_tree()->connect("node_added", callable_mp(this, &MultiuserEditorPlugin::_on_node_added));
				get_tree()->connect("node_removed", callable_mp(this, &MultiuserEditorPlugin::_on_node_removed));
			}
			poll_timer = memnew(Timer);
			poll_timer->set_wait_time(double(MULTIUSER_GET("blazium/multiuser_editor/intervals/poll_sec", MULTIUSER_POLL_INTERVAL_SEC)));
			poll_timer->set_one_shot(false);
			poll_timer->connect("timeout", callable_mp(this, &MultiuserEditorPlugin::_poll_runtime));
			add_child(poll_timer);
			poll_timer->start();
			ghost_overlay = memnew(MultiuserEditorGhostCursorOverlay);
			add_child(ghost_overlay);
			_setup_dock();
			_setup_status_indicator();
			inspector_plugin.instantiate();
			add_inspector_plugin(inspector_plugin);
			add_undo_redo_inspector_hook_callback(callable_mp(this, &MultiuserEditorPlugin::_on_inspector_property_changed));
			ProjectSettings::get_singleton()->connect("settings_changed", callable_mp(this, &MultiuserEditorPlugin::_on_project_settings_changed));
			if (EditorSettings::get_singleton() && !EditorSettings::get_singleton()->is_connected("settings_changed", callable_mp(this, &MultiuserEditorPlugin::_on_editor_settings_changed))) {
				EditorSettings::get_singleton()->connect("settings_changed", callable_mp(this, &MultiuserEditorPlugin::_on_editor_settings_changed));
			}
			if (!is_connected("scene_saved", callable_mp(this, &MultiuserEditorPlugin::_on_scene_saved))) {
				connect("scene_saved", callable_mp(this, &MultiuserEditorPlugin::_on_scene_saved));
			}
			EditorFileSystem *efs = EditorInterface::get_singleton()->get_resource_file_system();
			if (efs) {
				efs->connect("filesystem_changed", callable_mp(this, &MultiuserEditorPlugin::_on_filesystem_changed));
				efs->connect("resources_reimported", callable_mp(this, &MultiuserEditorPlugin::_on_resources_reimported));
				efs->connect("sources_changed", callable_mp(this, &MultiuserEditorPlugin::_on_sources_changed));
			}
			fs_debounce_timer = memnew(Timer);
			fs_debounce_timer->set_wait_time(0.15);
			fs_debounce_timer->set_one_shot(true);
			fs_debounce_timer->connect("timeout", callable_mp(this, &MultiuserEditorPlugin::_on_fs_debounce_timeout));
			add_child(fs_debounce_timer);
			_update_context();
			_handle_uri_join();

			EDITOR_DEF_BASIC("blazium/multiuser_editor/enabled", false);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/role", 0);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/auto_host", false);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/default_host", "127.0.0.1");
			EDITOR_DEF_BASIC("blazium/multiuser_editor/default_port", multiuser_editor::kDefaultPort);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/locks/timeout_sec", 30.0);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/intervals/poll_sec", MULTIUSER_POLL_INTERVAL_SEC);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/intervals/cursor_sec", MULTIUSER_CURSOR_INTERVAL_SEC);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/intervals/sync_pending_timeout_sec", MULTIUSER_SYNC_PENDING_TIMEOUT_SEC);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/intervals/jwt_jti_sweep_ms", 30000);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/git/shutdown_wait_ms", 5000);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/git/poll_delay_usec", 10000);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/unknown_relay_log_dedupe_max", 1024);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/sync_scripts", true);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/sync_scene_changes", true);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/show_remote_cursors", true);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/cursor_sync_enabled", true);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/telemetry_broadcast_interval_sec", 2.0);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/script_poll_interval_sec", 0.5);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/max_packet_size_mb", 8);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/enable_debug_logging", false);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/server_log_chat", true);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/server_log_connections", true);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/logging/log_level", "info");
			EDITOR_DEF_BASIC("blazium/multiuser_editor/logging/log_replication", false);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/logging/log_filesystem", false);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/logging/log_crdt", false);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/logging/log_network", false);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/logging/log_permissions", true);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/project_settings/replicated_prefixes", "rendering/,physics/,application/config/,input/,layer_names/,internationalization/,audio/,debug/,network/");
			EDITOR_DEF_BASIC("blazium/multiuser_editor/permissions/overrides", "");

			EDITOR_DEF_BASIC("blazium/multiuser_editor/permissions/allow_widen_host_only", false);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/git_auto_commit", false);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/git_auto_branching", false);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/git_session_branch_name", "multiuser_session_{timestamp}");
			EDITOR_DEF_BASIC("blazium/multiuser_editor/git_merge_target_branch", "main");
			EDITOR_DEF_BASIC("blazium/multiuser_editor/git_remote_actions_enabled", false);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/allow_editor_force_push", false);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/git_default_remote", "origin");
			EDITOR_DEF_BASIC("blazium/multiuser_editor/git_op_throttle_ms", 2000);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/crdt_sync_max_scripts_per_message", 4096);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/project_settings_snapshot_max_keys", 4096);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/crdt_import_max_atoms", 512000);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/concurrent_transfers_per_peer", 8);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/git_pending_results_max", 64);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/git_output_max_chars", multiuser_editor::kGitOutputDefaultMax);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/transfer_id_max_chars", 128);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/property_value_max_bytes", 4 * 1024 * 1024);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/scene_sync_max_bytes", 8 * 1024 * 1024);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/script_buffers_max", 256);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/locks_per_peer_max", 256);

			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/locks_total_max", 4096);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/tile_data_max_bytes", 4 * 1024 * 1024);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/crdt_export_max_atoms", 512000);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/project_setting_array_max_elements", 65536);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/throttle/project_setting_min_interval_ms", 50);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/enable_team_play", true);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/require_jwt", false);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/jwt_secret_key", "");
			EDITOR_DEF_BASIC("blazium/multiuser_editor/client_jwt", "");
			EDITOR_DEF_BASIC("blazium/multiuser_editor/jwt/algorithms_allowed", "HS256");
			EDITOR_DEF_BASIC("blazium/multiuser_editor/jwt/expected_audience", "");
			EDITOR_DEF_BASIC("blazium/multiuser_editor/jwt/expected_issuer", "");
			EDITOR_DEF_BASIC("blazium/multiuser_editor/jwt/leeway_sec", 30);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/jwt/max_token_age_sec", 3600);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/jwt/require_jti", false);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/jwt/jti_cache_max", 4096);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/cursor_update_min_interval_ms", 50);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/telemetry_min_interval_ms", 250);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/chat_min_interval_ms", 250);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/chat_history_max", 256);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/global_history_max", 100);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/checkpoint_state_max", 4096);

			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/script_attach_max_bytes", 4 * 1024 * 1024);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/crdt_live_atoms_max", 1000000);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/packets_per_poll_max", 256);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/relay_packets_per_sec", 200);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/visual_shader_node_id_max", 1048576);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/security_events_max", 16);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/network/max_clients", 32);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/allow_remote_autowork", false);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/z_connection_controls", "");

			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/process_packets_per_sec", 500);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/select_paths_max", 256);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/project_setting_name_max_chars", 256);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/tile_coord_max", 1 << 24);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/tile_source_max", 1 << 16);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/resource_sync_max_bytes", 5 * 1024 * 1024);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/multiuser_uri_password_max_chars", 1024);

			EDITOR_DEF_BASIC("blazium/multiuser_editor/access_list/enabled", true);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/access_list/path", multiuser_editor::kDefaultAccessListPath);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/access_list/max_entries", 256);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/access_list/auto_load_on_startup", true);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/access_list/implicit_default_role", "Editor");
			EDITOR_DEF_BASIC("blazium/multiuser_editor/access_list/auto_gitignore", true);

			EDITOR_DEF_BASIC("blazium/multiuser_editor/allow_empty_secret_handshake", false);

			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/pending_challenges_max", 256);

			EDITOR_DEF_BASIC("blazium/multiuser_editor/limits/pending_challenge_ttl_sec", 30);

			{
				PackedStringArray fs_inc;
				fs_inc.push_back("res://*");
				EDITOR_DEF("blazium/multiuser_editor/file_sync/include_patterns", fs_inc);
			}
			{
				PackedStringArray fs_exc;
				fs_exc.push_back(".godot/*");
				fs_exc.push_back(".git/*");
				fs_exc.push_back("*.tmp");
				fs_exc.push_back("*.~lock");
				fs_exc.push_back(".vscode/*");
				fs_exc.push_back(".idea/*");
				EDITOR_DEF("blazium/multiuser_editor/file_sync/exclude_patterns", fs_exc);
			}
			EDITOR_DEF_BASIC("blazium/multiuser_editor/file_sync/enabled", true);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/file_sync/max_file_bytes", 67108864);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/file_sync/chunk_bytes", 262144);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/file_sync/include_imports", true);
			EDITOR_DEF_BASIC("blazium/multiuser_editor/file_sync/initial_snapshot_on_join", true);

			if (EditorSettings::get_singleton()) {
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/multiuser_editor/enabled"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/role", PROPERTY_HINT_ENUM, "Client,Host"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/multiuser_editor/auto_host"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "blazium/multiuser_editor/default_host"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/default_port", PROPERTY_HINT_RANGE, "1024,65535,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::FLOAT, "blazium/multiuser_editor/locks/timeout_sec", PROPERTY_HINT_RANGE, "1,600,1,suffix:s"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::FLOAT, "blazium/multiuser_editor/intervals/poll_sec", PROPERTY_HINT_RANGE, "0.001,5.0,0.001,suffix:s"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::FLOAT, "blazium/multiuser_editor/intervals/cursor_sec", PROPERTY_HINT_RANGE, "0.01,5.0,0.01,suffix:s"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::FLOAT, "blazium/multiuser_editor/intervals/sync_pending_timeout_sec", PROPERTY_HINT_RANGE, "0.5,60.0,0.1,suffix:s"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/intervals/jwt_jti_sweep_ms", PROPERTY_HINT_RANGE, "1000,3600000,1000,suffix:ms"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/git/shutdown_wait_ms", PROPERTY_HINT_RANGE, "100,60000,100,suffix:ms"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/git/poll_delay_usec", PROPERTY_HINT_RANGE, "100,1000000,100,suffix:us"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/unknown_relay_log_dedupe_max", PROPERTY_HINT_RANGE, "16,1048576,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/multiuser_editor/sync_scripts"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/multiuser_editor/sync_scene_changes"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/multiuser_editor/show_remote_cursors"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/multiuser_editor/cursor_sync_enabled"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::FLOAT, "blazium/multiuser_editor/telemetry_broadcast_interval_sec", PROPERTY_HINT_RANGE, "0.1,10.0,0.1,suffix:s"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::FLOAT, "blazium/multiuser_editor/script_poll_interval_sec", PROPERTY_HINT_RANGE, "0.05,5.0,0.01,suffix:s"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/max_packet_size_mb", PROPERTY_HINT_RANGE, "1,128,1,suffix:MB"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/multiuser_editor/enable_debug_logging"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/multiuser_editor/server_log_chat"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/multiuser_editor/server_log_connections"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "blazium/multiuser_editor/logging/log_level", PROPERTY_HINT_ENUM, "error,warn,info,debug"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/multiuser_editor/logging/log_replication"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/multiuser_editor/logging/log_filesystem"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/multiuser_editor/logging/log_crdt"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/multiuser_editor/logging/log_network"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/multiuser_editor/logging/log_permissions"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "blazium/multiuser_editor/project_settings/replicated_prefixes"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "blazium/multiuser_editor/permissions/overrides"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/multiuser_editor/permissions/allow_widen_host_only"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/multiuser_editor/git_auto_commit"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/multiuser_editor/git_auto_branching"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "blazium/multiuser_editor/git_session_branch_name"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "blazium/multiuser_editor/git_merge_target_branch"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/multiuser_editor/git_remote_actions_enabled"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/multiuser_editor/allow_editor_force_push"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "blazium/multiuser_editor/git_default_remote"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/git_op_throttle_ms", PROPERTY_HINT_RANGE, "0,60000,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/crdt_sync_max_scripts_per_message", PROPERTY_HINT_RANGE, "64,65536,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/project_settings_snapshot_max_keys", PROPERTY_HINT_RANGE, "64,65536,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/crdt_import_max_atoms", PROPERTY_HINT_RANGE, "10000,2000000,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/concurrent_transfers_per_peer", PROPERTY_HINT_RANGE, "1,64,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/git_pending_results_max", PROPERTY_HINT_RANGE, "8,1024,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/git_output_max_chars", PROPERTY_HINT_RANGE, "1024,262144,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/transfer_id_max_chars", PROPERTY_HINT_RANGE, "16,1024,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/property_value_max_bytes", PROPERTY_HINT_RANGE, "65536,67108864,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/scene_sync_max_bytes", PROPERTY_HINT_RANGE, "65536,134217728,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/script_buffers_max", PROPERTY_HINT_RANGE, "8,4096,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/locks_per_peer_max", PROPERTY_HINT_RANGE, "8,8192,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/locks_total_max", PROPERTY_HINT_RANGE, "64,1048576,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/tile_data_max_bytes", PROPERTY_HINT_RANGE, "65536,67108864,1024"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/crdt_export_max_atoms", PROPERTY_HINT_RANGE, "10000,2000000,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/project_setting_array_max_elements", PROPERTY_HINT_RANGE, "16,1048576,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/throttle/project_setting_min_interval_ms", PROPERTY_HINT_RANGE, "0,60000,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/multiuser_editor/enable_team_play"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/multiuser_editor/require_jwt"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "blazium/multiuser_editor/jwt_secret_key", PROPERTY_HINT_PASSWORD));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "blazium/multiuser_editor/client_jwt", PROPERTY_HINT_PASSWORD));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "blazium/multiuser_editor/jwt/algorithms_allowed"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "blazium/multiuser_editor/jwt/expected_audience"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "blazium/multiuser_editor/jwt/expected_issuer"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/jwt/leeway_sec", PROPERTY_HINT_RANGE, "0,3600,1,suffix:s"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/jwt/max_token_age_sec", PROPERTY_HINT_RANGE, "60,604800,1,suffix:s"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/multiuser_editor/jwt/require_jti"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/jwt/jti_cache_max", PROPERTY_HINT_RANGE, "16,1048576,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/cursor_update_min_interval_ms", PROPERTY_HINT_RANGE, "0,5000,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/telemetry_min_interval_ms", PROPERTY_HINT_RANGE, "0,60000,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/chat_min_interval_ms", PROPERTY_HINT_RANGE, "0,60000,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/chat_history_max", PROPERTY_HINT_RANGE, "16,65536,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/global_history_max", PROPERTY_HINT_RANGE, "16,65536,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/checkpoint_state_max", PROPERTY_HINT_RANGE, "32,65536,1"));

				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/script_attach_max_bytes", PROPERTY_HINT_RANGE, "1024,67108864,1,suffix:B"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/crdt_live_atoms_max", PROPERTY_HINT_RANGE, "1024,10000000,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/packets_per_poll_max", PROPERTY_HINT_RANGE, "16,65536,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/relay_packets_per_sec", PROPERTY_HINT_RANGE, "1,100000,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/visual_shader_node_id_max", PROPERTY_HINT_RANGE, "1024,16777216,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/security_events_max", PROPERTY_HINT_RANGE, "4,1024,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/network/max_clients", PROPERTY_HINT_RANGE, "1,256,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/multiuser_editor/allow_remote_autowork"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "blazium/multiuser_editor/z_connection_controls", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_READ_ONLY));

				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/process_packets_per_sec", PROPERTY_HINT_RANGE, "1,100000,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/select_paths_max", PROPERTY_HINT_RANGE, "1,8192,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/project_setting_name_max_chars", PROPERTY_HINT_RANGE, "16,1024,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/tile_coord_max", PROPERTY_HINT_RANGE, "1024,16777216,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/tile_source_max", PROPERTY_HINT_RANGE, "16,65536,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/resource_sync_max_bytes", PROPERTY_HINT_RANGE, "65536,67108864,1024"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/multiuser_uri_password_max_chars", PROPERTY_HINT_RANGE, "16,4096,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/multiuser_editor/access_list/enabled"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "blazium/multiuser_editor/access_list/path", PROPERTY_HINT_FILE, "*.json"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/access_list/max_entries", PROPERTY_HINT_RANGE, "1,4096,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/multiuser_editor/access_list/auto_load_on_startup"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "blazium/multiuser_editor/access_list/implicit_default_role", PROPERTY_HINT_ENUM, "Viewer,Editor,Admin"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/multiuser_editor/access_list/auto_gitignore"));

				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/multiuser_editor/allow_empty_secret_handshake"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/pending_challenges_max", PROPERTY_HINT_RANGE, "1,4096,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/limits/pending_challenge_ttl_sec", PROPERTY_HINT_RANGE, "1,3600,1,suffix:s"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/multiuser_editor/file_sync/enabled"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::PACKED_STRING_ARRAY, "blazium/multiuser_editor/file_sync/include_patterns"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::PACKED_STRING_ARRAY, "blazium/multiuser_editor/file_sync/exclude_patterns"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/file_sync/max_file_bytes", PROPERTY_HINT_RANGE, "4096,268435456,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/multiuser_editor/file_sync/chunk_bytes", PROPERTY_HINT_RANGE, "4096,4194304,1"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/multiuser_editor/file_sync/include_imports"));
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/multiuser_editor/file_sync/initial_snapshot_on_join"));
			}

			_refresh_permissions_from_settings();

			if (bool(MULTIUSER_GET("blazium/multiuser_editor/access_list/auto_load_on_startup", true))) {
				_reload_access_list();
			} else {
				_refresh_access_list_protection();
			}

			if (EditorSettings::get_singleton() && EditorSettings::get_singleton()->has_setting("blazium/multiuser_editor/enabled")) {
				if (bool(EditorSettings::get_singleton()->get("blazium/multiuser_editor/enabled"))) {
					int role = int(EditorSettings::get_singleton()->get("blazium/multiuser_editor/role"));
					bool auto_host = bool(EditorSettings::get_singleton()->get("blazium/multiuser_editor/auto_host"));
					if (role == 1 && auto_host) {
						host_from_settings();
					}
				}
			}

			List<String> raw_args = OS::get_singleton()->get_cmdline_args();
			Vector<String> args;
			for (const String &arg : raw_args) {
				args.push_back(arg);
			}
			if (args.has("--multiuser-server")) {
				if (DisplayServer::get_singleton()->get_name() != "headless" && !args.has("--headless")) {
					ERR_PRINT("MultiuserEditor: Cannot launch dedicated server in an active Display Server. You MUST pass `--headless` to the engine binary to auto-host.");
				} else {
					String reason;
					if (!_is_inside_loaded_project(&reason)) {
						ERR_PRINT("MultiuserEditor: --multiuser-server requires a loaded Godot project: " + reason);
						OS::get_singleton()->set_exit_code(1);
						break;
					}
					int port = multiuser_editor::kDefaultPort;

					int port_idx = args.find("--multiuser-port");
					if (port_idx != -1 && port_idx + 1 < args.size()) {
						port = args[port_idx + 1].to_int();
					}
					if (port < multiuser_editor::kPortMin || port > multiuser_editor::kPortMax) {
						ERR_PRINT(vformat("MultiuserEditor: --multiuser-port out of range (%d). Must be %d-%d.", port, multiuser_editor::kPortMin, multiuser_editor::kPortMax));
						OS::get_singleton()->set_exit_code(1);
						break;
					}
					String password = "";
					int pass_idx = args.find("--multiuser-password");
					if (pass_idx != -1 && pass_idx + 1 < args.size()) {
						password = args[pass_idx + 1];
					}

					if (ENV *env = ENV::get_singleton()) {
						if (env->has_env("MULTIUSER_JWT_AUTH")) {
							EditorSettings::get_singleton()->set("blazium/multiuser_editor/require_jwt", env->get_env_bool("MULTIUSER_JWT_AUTH"));
						}
						if (env->has_env("MULTIUSER_JWT_SECRET")) {
							EditorSettings::get_singleton()->set("blazium/multiuser_editor/jwt_secret_key", String(env->get_env("MULTIUSER_JWT_SECRET")));
						}
						if (env->has_env("MULTIUSER_JWT_ISSUER")) {
							EditorSettings::get_singleton()->set("blazium/multiuser_editor/jwt/expected_issuer", String(env->get_env("MULTIUSER_JWT_ISSUER")));
						}
						if (env->has_env("MULTIUSER_JWT_AUDIENCE")) {
							EditorSettings::get_singleton()->set("blazium/multiuser_editor/jwt/expected_audience", String(env->get_env("MULTIUSER_JWT_AUDIENCE")));
						}
						if (env->has_env("MULTIUSER_JWT_ALGORITHMS")) {
							EditorSettings::get_singleton()->set("blazium/multiuser_editor/jwt/algorithms_allowed", String(env->get_env("MULTIUSER_JWT_ALGORITHMS")));
						}
					}

					if (args.has("--multiuser-jwt-auth")) {
						EditorSettings::get_singleton()->set("blazium/multiuser_editor/require_jwt", true);
					}
					int sec_idx = args.find("--multiuser-jwt-secret");
					if (sec_idx != -1 && sec_idx + 1 < args.size()) {
						EditorSettings::get_singleton()->set("blazium/multiuser_editor/jwt_secret_key", args[sec_idx + 1]);
					}
					int iss_idx = args.find("--multiuser-jwt-issuer");
					if (iss_idx != -1 && iss_idx + 1 < args.size()) {
						EditorSettings::get_singleton()->set("blazium/multiuser_editor/jwt/expected_issuer", args[iss_idx + 1]);
					}
					int aud_idx = args.find("--multiuser-jwt-audience");
					if (aud_idx != -1 && aud_idx + 1 < args.size()) {
						EditorSettings::get_singleton()->set("blazium/multiuser_editor/jwt/expected_audience", args[aud_idx + 1]);
					}
					int alg_idx = args.find("--multiuser-jwt-algorithms");
					if (alg_idx != -1 && alg_idx + 1 < args.size()) {
						EditorSettings::get_singleton()->set("blazium/multiuser_editor/jwt/algorithms_allowed", args[alg_idx + 1]);
					}
					int leeway_idx = args.find("--multiuser-jwt-leeway");
					if (leeway_idx != -1 && leeway_idx + 1 < args.size()) {
						EditorSettings::get_singleton()->set("blazium/multiuser_editor/jwt/leeway_sec", args[leeway_idx + 1].to_int());
					}
					int max_age_idx = args.find("--multiuser-jwt-max-age");
					if (max_age_idx != -1 && max_age_idx + 1 < args.size()) {
						EditorSettings::get_singleton()->set("blazium/multiuser_editor/jwt/max_token_age_sec", args[max_age_idx + 1].to_int());
					}
					if (args.has("--multiuser-jwt-require-jti")) {
						EditorSettings::get_singleton()->set("blazium/multiuser_editor/jwt/require_jti", true);
					}

					EditorSettings::get_singleton()->set("blazium/multiuser_editor/enabled", true);
					if (!s_server_host_pending) {
						s_server_host_pending = true;
						s_server_host_port = port;
						s_server_host_password = password;
						s_server_host_after_usec = OS::get_singleton()->get_ticks_usec() + 3'000'000;
						s_server_host_ready_since_usec = 0;
						_log_connection(vformat("Server auto-host scheduled on port %d (poll deferred)", port));
					}
				}
			}

			_handle_cli_multiuser_join();

		} break;
		case NOTIFICATION_EXIT_TREE: {
			_stop();
			if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->is_connected("settings_changed", callable_mp(this, &MultiuserEditorPlugin::_on_project_settings_changed))) {
				ProjectSettings::get_singleton()->disconnect("settings_changed", callable_mp(this, &MultiuserEditorPlugin::_on_project_settings_changed));
			}
			if (EditorSettings::get_singleton() && EditorSettings::get_singleton()->is_connected("settings_changed", callable_mp(this, &MultiuserEditorPlugin::_on_editor_settings_changed))) {
				EditorSettings::get_singleton()->disconnect("settings_changed", callable_mp(this, &MultiuserEditorPlugin::_on_editor_settings_changed));
			}
			if (selection && selection->is_connected("selection_changed", callable_mp(this, &MultiuserEditorPlugin::_on_selection_changed))) {
				selection->disconnect("selection_changed", callable_mp(this, &MultiuserEditorPlugin::_on_selection_changed));
			}
			if (get_tree()) {
				if (get_tree()->is_connected("node_added", callable_mp(this, &MultiuserEditorPlugin::_on_node_added))) {
					get_tree()->disconnect("node_added", callable_mp(this, &MultiuserEditorPlugin::_on_node_added));
				}
				if (get_tree()->is_connected("node_removed", callable_mp(this, &MultiuserEditorPlugin::_on_node_removed))) {
					get_tree()->disconnect("node_removed", callable_mp(this, &MultiuserEditorPlugin::_on_node_removed));
				}
			}
			if (poll_timer) {
				poll_timer->queue_free();
				poll_timer = nullptr;
			}
			if (ghost_overlay) {
				ghost_overlay->queue_free();
				ghost_overlay = nullptr;
			}
			if (EditorFileSystem *efs = EditorInterface::get_singleton()->get_resource_file_system()) {
				if (efs->is_connected("filesystem_changed", callable_mp(this, &MultiuserEditorPlugin::_on_filesystem_changed))) {
					efs->disconnect("filesystem_changed", callable_mp(this, &MultiuserEditorPlugin::_on_filesystem_changed));
				}
				if (efs->is_connected("resources_reimported", callable_mp(this, &MultiuserEditorPlugin::_on_resources_reimported))) {
					efs->disconnect("resources_reimported", callable_mp(this, &MultiuserEditorPlugin::_on_resources_reimported));
				}
				if (efs->is_connected("sources_changed", callable_mp(this, &MultiuserEditorPlugin::_on_sources_changed))) {
					efs->disconnect("sources_changed", callable_mp(this, &MultiuserEditorPlugin::_on_sources_changed));
				}
			}
			if (fs_debounce_timer) {
				fs_debounce_timer->queue_free();
				fs_debounce_timer = nullptr;
			}
			if (is_connected("scene_saved", callable_mp(this, &MultiuserEditorPlugin::_on_scene_saved))) {
				disconnect("scene_saved", callable_mp(this, &MultiuserEditorPlugin::_on_scene_saved));
			}
			remove_undo_redo_inspector_hook_callback(callable_mp(this, &MultiuserEditorPlugin::_on_inspector_property_changed));
			if (inspector_plugin.is_valid()) {
				remove_inspector_plugin(inspector_plugin);
				inspector_plugin.unref();
			}
			_teardown_dock();
			if (status_label) {
				remove_control_from_container(CONTAINER_TOOLBAR, status_label);
				status_label->queue_free();
				status_label = nullptr;
			}
		} break;
	}
}

void MultiuserEditorPlugin::_emit_action(Dictionary p_action) {
	if (!is_session_connected()) {
		return;
	}
	const String type = String(p_action.get("type", ""));
	if (permissions.is_valid() && permissions->is_known_action(type)) {
		if (!permissions->can_perform(type, local_role)) {
			_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("Local emit denied: action='%s' role='%s' (configure blazium/multiuser_editor/permissions/overrides to relax)", type, local_role));
			return;
		}
		if (permissions->is_host_only(type) && network.get_mode() != MultiuserEditorNetwork::MODE_HOST) {
			_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("Local emit denied: host-only action='%s' attempted from non-host", type));
			return;
		}
	}
	p_action["peer_id"] = local_peer_id;
	p_action["timestamp"] = OS::get_singleton()->get_unix_time();
	_log_cat(LOG_DEBUG, LOG_REPLICATION, "emit -> all: " + _format_action_summary(p_action));
	network.send_action(p_action);
}

void MultiuserEditorPlugin::_emit_action_to_server(const Dictionary &p_action) {
	if (!is_session_connected()) {
		return;
	}
	const String type = String(p_action.get("type", ""));
	if (permissions.is_valid() && permissions->is_known_action(type)) {
		if (!permissions->can_perform(type, local_role)) {
			_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("Local emit-to-server denied: action='%s' role='%s'", type, local_role));
			return;
		}
		if (permissions->is_host_only(type) && network.get_mode() != MultiuserEditorNetwork::MODE_HOST) {
			_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("Local emit-to-server denied: host-only action='%s' attempted from non-host", type));
			return;
		}
	}
	Dictionary a = p_action.duplicate(true);
	a["peer_id"] = local_peer_id;
	a["timestamp"] = OS::get_singleton()->get_unix_time();
	if (network.get_mode() == MultiuserEditorNetwork::MODE_JOIN) {
		_log_cat(LOG_DEBUG, LOG_REPLICATION, "emit -> host: " + _format_action_summary(a));
		network.send_action_to(MultiuserEditorFilesystemSync::get_server_peer_id(), a);
	}
}

void MultiuserEditorPlugin::_emit_file_sync_actions(const Vector<Dictionary> &p_actions) {
	for (int i = 0; i < p_actions.size(); i++) {
		Dictionary a = p_actions[i];
		if (network.get_mode() == MultiuserEditorNetwork::MODE_HOST) {
			_emit_action(a);
		} else if (network.get_mode() == MultiuserEditorNetwork::MODE_JOIN) {
			_emit_action_to_server(a);
		}
	}
}

void MultiuserEditorPlugin::_update_filesystem_sync_policy() {
	const PackedStringArray inc = _read_packed_string_array_setting("blazium/multiuser_editor/file_sync/include_patterns", Vector<String>{ "res://*" });
	const PackedStringArray exc = _read_packed_string_array_setting("blazium/multiuser_editor/file_sync/exclude_patterns", Vector<String>{ ".godot/*", ".git/*", "*.tmp", "*.~lock", ".vscode/*", ".idea/*" });
	const bool imp = bool(MULTIUSER_GET("blazium/multiuser_editor/file_sync/include_imports", true));
	const int64_t max_b = int64_t(MULTIUSER_GET("blazium/multiuser_editor/file_sync/max_file_bytes", 67108864));
	const int64_t chunk_b = int64_t(MULTIUSER_GET("blazium/multiuser_editor/file_sync/chunk_bytes", 262144));
	filesystem_sync.set_sync_policy(inc, exc, imp, max_b, chunk_b);
	const int concurrent_cap = MAX(1, int(MULTIUSER_GET("blazium/multiuser_editor/limits/concurrent_transfers_per_peer", 8)));
	filesystem_sync.set_concurrent_transfer_cap(concurrent_cap);
	const int tid_max = MAX(16, int(MULTIUSER_GET("blazium/multiuser_editor/limits/transfer_id_max_chars", 128)));
	filesystem_sync.set_transfer_id_max_chars(tid_max);
	const int script_buf_max = MAX(8, int(MULTIUSER_GET("blazium/multiuser_editor/limits/script_buffers_max", 256)));
	script_sync.set_max_tracked_buffers(script_buf_max);
	const int locks_max = MAX(8, int(MULTIUSER_GET("blazium/multiuser_editor/limits/locks_per_peer_max", 256)));
	lock_manager.set_max_locks_per_peer(locks_max);

	const int locks_global_max = MAX(64, int(MULTIUSER_GET("blazium/multiuser_editor/limits/locks_total_max", 4096)));
	lock_manager.set_max_total_locks(locks_global_max);
}

void MultiuserEditorPlugin::_schedule_filesystem_diff() {
	if (!fs_debounce_timer) {
		return;
	}
	fs_debounce_timer->start();
}

void MultiuserEditorPlugin::_on_fs_debounce_timeout() {
	_process_filesystem_diff();
}

void MultiuserEditorPlugin::_on_resources_reimported(const PackedStringArray &p_paths) {
	(void)p_paths;
	_schedule_filesystem_diff();
}

void MultiuserEditorPlugin::_on_sources_changed(bool p_exist) {
	(void)p_exist;
	_schedule_filesystem_diff();
}

void MultiuserEditorPlugin::_process_filesystem_diff() {
	if (!network.is_connected() || !bool(MULTIUSER_GET("blazium/multiuser_editor/file_sync/enabled", true))) {
		return;
	}
	if (!is_session_connected()) {
		return;
	}
	if (suppress_fs_broadcast) {
		return;
	}
	_update_filesystem_sync_policy();
	const bool inc_imp = bool(MULTIUSER_GET("blazium/multiuser_editor/file_sync/include_imports", true));
	const PackedStringArray inc = _read_packed_string_array_setting("blazium/multiuser_editor/file_sync/include_patterns", Vector<String>{ "res://*" });
	const PackedStringArray exc = _read_packed_string_array_setting("blazium/multiuser_editor/file_sync/exclude_patterns", Vector<String>{ ".godot/*", ".git/*", "*.tmp", "*.~lock", ".vscode/*", ".idea/*" });
	const int64_t max_b = int64_t(MULTIUSER_GET("blazium/multiuser_editor/file_sync/max_file_bytes", 67108864));
	const int64_t chunk_b = int64_t(MULTIUSER_GET("blazium/multiuser_editor/file_sync/chunk_bytes", 262144));

	Vector<MultiuserEditorFilesystemSync::Delta> deltas = filesystem_sync.diff_and_update_snapshot(inc_imp, inc, exc, max_b);
	for (int i = 0; i < deltas.size(); i++) {
		const MultiuserEditorFilesystemSync::Delta &d = deltas[i];
		if ((d.kind == MultiuserEditorFilesystemSync::DELTA_UPDATE || d.kind == MultiuserEditorFilesystemSync::DELTA_CREATE) &&
				filesystem_sync.should_skip_path_hash(d.path, d.hash_hex)) {
			continue;
		}
		String tid;
		Vector<Dictionary> actions = MultiuserEditorFilesystemSync::build_transfer_actions(
				network.get_mode() == MultiuserEditorNetwork::MODE_HOST ? String("file_apply") : String("file_propose"),
				d, max_b, chunk_b, inc_imp, tid);
		_emit_file_sync_actions(actions);
	}
}

void MultiuserEditorPlugin::_send_project_snapshot_to_peer(int p_target_net_id) {
	if (network.get_mode() != MultiuserEditorNetwork::MODE_HOST) {
		return;
	}
	if (!bool(MULTIUSER_GET("blazium/multiuser_editor/file_sync/initial_snapshot_on_join", true))) {
		return;
	}
	if (!bool(MULTIUSER_GET("blazium/multiuser_editor/file_sync/enabled", true))) {
		return;
	}
	_update_filesystem_sync_policy();
	const bool inc_imp = bool(MULTIUSER_GET("blazium/multiuser_editor/file_sync/include_imports", true));
	const PackedStringArray inc = _read_packed_string_array_setting("blazium/multiuser_editor/file_sync/include_patterns", Vector<String>{ "res://*" });
	const PackedStringArray exc = _read_packed_string_array_setting("blazium/multiuser_editor/file_sync/exclude_patterns", Vector<String>{ ".godot/*", ".git/*", "*.tmp", "*.~lock", ".vscode/*", ".idea/*" });
	const int64_t max_b = int64_t(MULTIUSER_GET("blazium/multiuser_editor/file_sync/max_file_bytes", 67108864));
	const int64_t chunk_b = int64_t(MULTIUSER_GET("blazium/multiuser_editor/file_sync/chunk_bytes", 262144));

	filesystem_sync.clear_snapshot();
	filesystem_sync.capture_snapshot_from_res(inc_imp, inc, exc);
	const Vector<String> paths = filesystem_sync.get_snapshot_paths_sorted();
	for (int i = 0; i < paths.size(); i++) {
		MultiuserEditorFilesystemSync::Delta d;
		d.kind = MultiuserEditorFilesystemSync::DELTA_UPDATE;
		d.path = paths[i];
		d.hash_hex = MultiuserEditorFilesystemSync::hash_file_hex(paths[i]);
		{
			Ref<FileAccess> fsz = FileAccess::open(paths[i], FileAccess::READ);
			if (!fsz.is_null()) {
				d.size = uint64_t(fsz->get_length());
			}
		}
		String tid;
		Vector<Dictionary> actions = MultiuserEditorFilesystemSync::build_transfer_actions(String("file_apply"), d, max_b, chunk_b, inc_imp, tid);
		for (int j = 0; j < actions.size(); j++) {
			Dictionary a = actions[j];
			a["peer_id"] = local_peer_id;
			a["timestamp"] = OS::get_singleton()->get_unix_time();
			network.send_action_to(p_target_net_id, a);
		}
	}
	Dictionary done_data;
	done_data["ok"] = true;
	Dictionary done;
	done["type"] = multiuser_editor::kActionFsSnapshotDone;
	done["data"] = done_data;
	done["peer_id"] = local_peer_id;
	done["timestamp"] = OS::get_singleton()->get_unix_time();
	network.send_action_to(p_target_net_id, done);
}

void MultiuserEditorPlugin::_route_action(int p_sender_net_id, const Dictionary &p_action) {
	if (p_action.is_empty()) {
		return;
	}
	{
		String sane_reason;
		if (!_packet_is_sane(p_action, &sane_reason)) {
			_log_cat(LOG_WARN, LOG_NETWORK, vformat("Dropped malformed packet from net_id=%d reason=%s", p_sender_net_id, sane_reason));
			_record_security_event_kind(MultiuserEditorDock::KIND_MALFORMED, LOG_WARN, LOG_NETWORK, vformat("malformed packet (%s) from %d", sane_reason, p_sender_net_id));
			_bump_drop_counter();
			return;
		}
	}
	String type = String(p_action.get("type", ""));

	{
		const Dictionary maybe_data = p_action.get("data", Dictionary());
		const char *fields[] = { "src", "dst", "path", "old_path", "new_path", "script_path", "filepath", "node_path", nullptr };
		for (int fi = 0; fields[fi] != nullptr; fi++) {
			const String candidate = _extract_target_path_for_protection(type, p_action, maybe_data, String(fields[fi]));
			if (!candidate.is_empty() && _is_path_protected_for_action(candidate)) {
				_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("blocked protected path: type=%s field=%s sender=%d", type, fields[fi], p_sender_net_id));
				_record_security_event_kind(MultiuserEditorDock::KIND_PROTECTED_PATH, LOG_WARN, LOG_PERMISSIONS, vformat("blocked protected path: type=%s sender=%d", type, p_sender_net_id));
				_bump_drop_counter();
				return;
			}
		}

		if (type == multiuser_editor::kActionCrdtSync) {
			const Variant bv = maybe_data.get("buffers", Variant());
			if (bv.get_type() == Variant::DICTIONARY) {
				const Dictionary buffers = bv;
				const Array bkeys = buffers.keys();
				const int crdt_bulk_limit = MAX(1, int(MULTIUSER_GET("blazium/multiuser_editor/limits/crdt_sync_max_scripts_per_message", 4096)));
				const int n = MIN(bkeys.size(), crdt_bulk_limit);
				for (int i = 0; i < n; i++) {
					const String k = String(bkeys[i]);
					if (!k.is_empty() && _is_path_protected_for_action(k)) {
						_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("blocked protected path in crdt_sync bulk buffers: %s sender=%d", k, p_sender_net_id));
						_record_security_event_kind(MultiuserEditorDock::KIND_PROTECTED_PATH, LOG_WARN, LOG_PERMISSIONS, vformat("blocked crdt_sync bulk path sender=%d", p_sender_net_id));
						_bump_drop_counter();
						return;
					}
				}
			}
		}
	}

	{
		const bool client_passive_or_handshake =
				type == multiuser_editor::kActionHandshake || type == multiuser_editor::kActionHandshakeAck || type == multiuser_editor::kActionAuthChallenge ||
				type == multiuser_editor::kActionChat || type == multiuser_editor::kActionTelemetry || type == multiuser_editor::kActionCursorUpdate;

		const bool host_handshake_only =
				type == multiuser_editor::kActionHandshake || type == multiuser_editor::kActionHandshakeAck || type == multiuser_editor::kActionAuthChallenge;

		if (network.get_mode() == MultiuserEditorNetwork::MODE_JOIN) {
			if (!session_authenticated && !client_passive_or_handshake) {
				_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("pre-auth drop (client): action='%s' from net_id=%d", type, p_sender_net_id));
				_record_security_event_kind(MultiuserEditorDock::KIND_PRE_AUTH_DROP, LOG_WARN, LOG_PERMISSIONS, vformat("pre-auth drop (client): %s from %d", type, p_sender_net_id));
				return;
			}
		} else if (network.get_mode() == MultiuserEditorNetwork::MODE_HOST) {
			if (p_sender_net_id != 1 && !network.is_peer_authenticated(p_sender_net_id)) {
				if (!host_handshake_only) {
					_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("pre-auth drop (host): action='%s' from unauthenticated peer net_id=%d", type, p_sender_net_id));
					_record_security_event_kind(MultiuserEditorDock::KIND_PRE_AUTH_DROP, LOG_WARN, LOG_PERMISSIONS, vformat("pre-auth drop: %s from %d", type, p_sender_net_id));
					return;
				}
			}
		}
	}

	String peer_id = String(p_action.get("peer_id", ""));
	if (peer_id == local_peer_id) {
		return;
	}
	if (!peer_id.is_empty() && !_is_valid_peer_id_field(peer_id)) {
		_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("Dropped action with invalid peer_id (len=%d) from net_id=%d type=%s", peer_id.length(), p_sender_net_id, type));
		_record_security_event_kind(MultiuserEditorDock::KIND_INVALID_PACKET, LOG_WARN, LOG_PERMISSIONS, vformat("invalid peer_id len=%d sender=%d type=%s", peer_id.length(), p_sender_net_id, type));
		return;
	}
	{
		Dictionary inner = p_action.get("data", Dictionary());
		if (p_action.has("role")) {
			const String role_str = String(p_action.get("role", ""));
			if (!role_str.is_empty() && !_is_valid_role_field(role_str)) {
				_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("Dropped action with invalid role field len=%d type=%s", role_str.length(), type));
				_record_security_event_kind(MultiuserEditorDock::KIND_INVALID_PACKET, LOG_WARN, LOG_PERMISSIONS, vformat("invalid role len=%d type=%s sender=%d", role_str.length(), type, p_sender_net_id));
				return;
			}
		}
		if (inner.has("role")) {
			const String role_str = String(inner.get("role", ""));
			if (!role_str.is_empty() && !_is_valid_role_field(role_str)) {
				_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("Dropped action with invalid data.role field len=%d type=%s", role_str.length(), type));
				_record_security_event_kind(MultiuserEditorDock::KIND_INVALID_PACKET, LOG_WARN, LOG_PERMISSIONS, vformat("invalid data.role len=%d type=%s sender=%d", role_str.length(), type, p_sender_net_id));
				return;
			}
		}
		if (type == multiuser_editor::kActionChat) {
			const String message = String(inner.get("message", ""));
			if (!_is_valid_chat_message(message)) {
				_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("Dropped chat with invalid message len=%d", message.length()));
				_record_security_event_kind(MultiuserEditorDock::KIND_INVALID_PACKET, LOG_WARN, LOG_PERMISSIONS, vformat("invalid chat message len=%d sender=%d", message.length(), p_sender_net_id));
				return;
			}
		}
		if (type == multiuser_editor::kActionAuthChallenge) {
			const String challenge = String(inner.get("challenge", ""));
			if (!_is_valid_hex_token(challenge, multiuser_editor::kSHA256HexLength)) {
				_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("Dropped auth_challenge with invalid challenge len=%d", challenge.length()));
				_record_security_event_kind(MultiuserEditorDock::KIND_AUTH_FAILED, LOG_WARN, LOG_PERMISSIONS, vformat("invalid auth_challenge len=%d sender=%d", challenge.length(), p_sender_net_id));
				return;
			}
		}
		if (type == "handshake") {
			if (inner.has("hmac")) {
				const String hmac = String(inner.get("hmac", ""));
				if (!_is_valid_hex_token(hmac, multiuser_editor::kSHA256HexLength)) {
					_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("Dropped handshake with invalid hmac len=%d", hmac.length()));
					_record_security_event_kind(MultiuserEditorDock::KIND_AUTH_FAILED, LOG_WARN, LOG_PERMISSIONS, vformat("invalid handshake hmac len=%d sender=%d", hmac.length(), p_sender_net_id));
					return;
				}
			}
			if (inner.has("jwt")) {
				const String jwt = String(inner.get("jwt", ""));
				if (!jwt.is_empty() && !_is_valid_jwt_token(jwt)) {
					_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("Dropped handshake with invalid jwt len=%d", jwt.length()));
					_record_security_event_kind(MultiuserEditorDock::KIND_AUTH_FAILED, LOG_WARN, LOG_PERMISSIONS, vformat("invalid handshake jwt len=%d sender=%d", jwt.length(), p_sender_net_id));
					return;
				}
			}
		}
	}
	if (!peer_id.is_empty()) {
		const bool is_host = network.get_mode() == MultiuserEditorNetwork::MODE_HOST;
		const bool is_authenticated_or_self = !is_host || p_sender_net_id == 1 || network.is_peer_authenticated(p_sender_net_id);
		if (is_authenticated_or_self) {
			bool first_seen = network.get_peer_id(p_sender_net_id).is_empty();
			network.remember_peer(p_sender_net_id, peer_id);
			if (dock) {
				dock->add_peer(peer_id);
			}
			if (first_seen && is_host) {
				_send_initial_state(p_sender_net_id);
			}
		}
	}

	String remote_role = network.get_peer_role(p_sender_net_id);
	Node *scene_root = EditorInterface::get_singleton()->get_edited_scene_root();

	_log_cat(LOG_DEBUG, LOG_REPLICATION, vformat("recv from net_id=%d peer=%s role=%s : %s", p_sender_net_id, peer_id, remote_role.is_empty() ? String("?") : remote_role, _format_action_summary(p_action)));

	if (permissions.is_valid()) {
		if (!permissions->is_known_action(type)) {
			_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("Receive denied: unknown action type='%s' from net_id=%d peer=%s", type, p_sender_net_id, peer_id));
			_record_security_event_kind(MultiuserEditorDock::KIND_UNKNOWN_ACTION, LOG_WARN, LOG_PERMISSIONS, vformat("unknown action='%s' sender=%d", type, p_sender_net_id));
			return;
		}
		const String role_for_check = remote_role.is_empty() ? String("Viewer") : remote_role;
		if (!permissions->can_perform(type, role_for_check)) {
			_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("Receive denied: action='%s' sender_role='%s' peer=%s", type, role_for_check, peer_id));
			_record_security_event_kind(MultiuserEditorDock::KIND_PERMISSION_DENIED, LOG_WARN, LOG_PERMISSIONS, vformat("permission denied action='%s' role='%s' sender=%d", type, role_for_check, p_sender_net_id));
			return;
		}
		if (permissions->is_host_only(type) && p_sender_net_id != 1) {
			_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("Receive denied: host-only action='%s' from non-host net_id=%d (peer=%s)", type, p_sender_net_id, peer_id));
			_record_security_event_kind(MultiuserEditorDock::KIND_PERMISSION_DENIED, LOG_WARN, LOG_PERMISSIONS, vformat("host-only action='%s' from non-host sender=%d", type, p_sender_net_id));
			return;
		}
	}

	if (network.get_mode() == MultiuserEditorNetwork::MODE_HOST) {
		if (type == multiuser_editor::kActionProperty || type == multiuser_editor::kActionNodeAdd || type == multiuser_editor::kActionNodeDelete) {
			global_history.push_back(p_action);
			const int gh_max = MAX(16, int(MULTIUSER_GET("blazium/multiuser_editor/limits/global_history_max", 100)));
			while (global_history.size() > gh_max) {
				global_history.remove_at(0);
			}
		}
	}

	if (type == multiuser_editor::kActionSelect) {
		Dictionary action_data = p_action.get("data", Dictionary());

		Array raw_paths = action_data.get("paths", Array());
		const int select_paths_cap = MAX(1, int(MULTIUSER_GET("blazium/multiuser_editor/limits/select_paths_max", 256)));
		Array safe_paths;
		const int n = MIN(raw_paths.size(), select_paths_cap);
		if (raw_paths.size() > select_paths_cap) {
			const String _msg = vformat("select.paths truncated: %d > %d", raw_paths.size(), select_paths_cap);
			_log_cat(LOG_WARN, LOG_REPLICATION, _msg);
			_record_security_event_kind(MultiuserEditorDock::KIND_INVALID_PACKET, LOG_WARN, LOG_REPLICATION, _msg);
		}
		for (int i = 0; i < n; i++) {
			const Variant &raw = raw_paths[i];
			if (raw.get_type() != Variant::STRING && raw.get_type() != Variant::STRING_NAME && raw.get_type() != Variant::NODE_PATH) {
				continue;
			}
			const String s = String(raw);
			if (s.is_empty() || !MultiuserEditorActionInterceptor::is_safe_node_path(s)) {
				continue;
			}
			safe_paths.append(s);
		}
		lock_manager.update_peer_selection(peer_id, safe_paths);
	} else if (type == multiuser_editor::kActionCrdt) {
		Dictionary action_data = p_action.get("data", Dictionary());
		Variant wrapped_op = action_data.get("op", Variant());
		Dictionary op_data = action_data;
		if (wrapped_op.get_type() == Variant::DICTIONARY) {
			op_data = wrapped_op;
		}
		String script_path = String(p_action.get("node_path", action_data.get("script", "")));
		script_sync.apply_remote_crdt(op_data, script_path);
	} else if (type == multiuser_editor::kActionCrdtSync) {
		Dictionary action_data = p_action.get("data", Dictionary());
		String script_path = String(p_action.get("node_path", ""));
		if (!script_path.is_empty()) {
			if (network.get_mode() == MultiuserEditorNetwork::MODE_HOST && script_sync.has_buffer(script_path)) {
				Dictionary reply;
				reply["type"] = multiuser_editor::kActionCrdtSync;
				reply["peer_id"] = local_peer_id;
				reply["timestamp"] = OS::get_singleton()->get_unix_time();
				reply["node_path"] = script_path;
				reply["data"] = script_sync.export_buffer(script_path);
				network.send_action_to(p_sender_net_id, reply);
			} else {
				bool clear_sync_pending = bool(action_data.get("sync_complete", true));
				script_sync.import_buffer_state(script_path, action_data, clear_sync_pending);
			}
		} else {
			Dictionary buffers = action_data.get("buffers", Dictionary());
			Array keys = buffers.keys();
			const int crdt_bulk_limit = MAX(1, int(MULTIUSER_GET("blazium/multiuser_editor/limits/crdt_sync_max_scripts_per_message", 4096)));
			if (keys.size() > crdt_bulk_limit) {
				_log_cat(LOG_WARN, LOG_CRDT, vformat("crdt_sync bulk dropped: %d entries exceeds cap %d", keys.size(), crdt_bulk_limit));
				_record_security_event_kind(MultiuserEditorDock::KIND_REPLICATION_FAILED, LOG_WARN, LOG_CRDT, vformat("crdt_sync bulk dropped keys=%d cap=%d sender=%d", keys.size(), crdt_bulk_limit, p_sender_net_id));
			} else {
				bool all_ok = true;
				int failed_count = 0;
				for (int i = 0; i < keys.size(); i++) {
					script_path = String(keys[i]);
					const bool ok = script_sync.import_buffer_state(script_path, buffers[script_path], false);
					if (!ok) {
						all_ok = false;
						failed_count++;
					}
				}
				if (bool(action_data.get("sync_complete", true))) {
					if (all_ok) {
						script_sync.set_sync_pending(false);
					} else {
						_log_cat(LOG_WARN, LOG_CRDT, vformat("crdt_sync bulk: %d/%d buffers refused; keeping sync_pending true to force resync.", failed_count, keys.size()));
						_record_security_event_kind(MultiuserEditorDock::KIND_CRDT_REFUSED, LOG_WARN, LOG_CRDT, vformat("crdt_sync bulk partial sender=%d failed=%d/%d", p_sender_net_id, failed_count, keys.size()));
					}
				}
			}
		}
	} else if (type == multiuser_editor::kActionProjectSetting) {
		const int min_interval_ms = MAX(0, int(MULTIUSER_GET("blazium/multiuser_editor/throttle/project_setting_min_interval_ms", 50)));
		if (min_interval_ms > 0) {
			const uint64_t now_msec = OS::get_singleton()->get_ticks_msec();
			HashMap<int, uint64_t>::Iterator it = _project_setting_last_msec.find(p_sender_net_id);
			if (it && now_msec - it->value < uint64_t(min_interval_ms)) {
				_log_cat(LOG_WARN, LOG_REPLICATION, vformat("project_setting throttled from net_id=%d (interval=%d ms)", p_sender_net_id, min_interval_ms));
				_record_security_event_kind(MultiuserEditorDock::KIND_RATE_LIMITED, LOG_WARN, LOG_REPLICATION, vformat("project_setting throttled sender=%d interval=%d ms", p_sender_net_id, min_interval_ms));
				return;
			}
			_project_setting_last_msec[p_sender_net_id] = now_msec;
		}
		Dictionary action_data = p_action.get("data", Dictionary());
		String setting_name = String(action_data.get("name", ""));

		const int setting_name_max_chars = MAX(1, int(MULTIUSER_GET("blazium/multiuser_editor/limits/project_setting_name_max_chars", 256)));
		if (setting_name.length() > setting_name_max_chars) {
			_log_cat(LOG_WARN, LOG_REPLICATION, vformat("project_setting dropped: name too long (len=%d, cap=%d)", setting_name.length(), setting_name_max_chars));
			_record_security_event_kind(MultiuserEditorDock::KIND_INVALID_PACKET, LOG_WARN, LOG_REPLICATION, vformat("project_setting name too long len=%d cap=%d sender=%d", setting_name.length(), setting_name_max_chars, p_sender_net_id));
			return;
		}
		Variant setting_value = action_data.get("value", Variant());
		_apply_project_setting_value(setting_name, setting_value);
		ProjectSettings::get_singleton()->save();
	} else if (type == multiuser_editor::kActionProjectSettingsSnapshot) {
		Dictionary action_data = p_action.get("data", Dictionary());
		Dictionary settings_map = action_data.get("settings", Dictionary());
		Array keys = settings_map.keys();
		const int snapshot_key_limit = MAX(1, int(MULTIUSER_GET("blazium/multiuser_editor/limits/project_settings_snapshot_max_keys", 4096)));
		if (keys.size() > snapshot_key_limit) {
			_log_cat(LOG_WARN, LOG_REPLICATION, vformat("project_settings_snapshot dropped: %d keys exceeds cap %d", keys.size(), snapshot_key_limit));
			_record_security_event_kind(MultiuserEditorDock::KIND_INVALID_PACKET, LOG_WARN, LOG_REPLICATION, vformat("project_settings_snapshot keys=%d cap=%d sender=%d", keys.size(), snapshot_key_limit, p_sender_net_id));
			return;
		}
		int applied = 0;
		int dropped = 0;
		for (int i = 0; i < keys.size(); i++) {
			const String key = String(keys[i]);
			if (!_settings_path_replicated(key)) {
				dropped++;
				continue;
			}
			_apply_project_setting_value(key, settings_map[key]);
			applied++;
		}
		if (applied > 0) {
			ProjectSettings::get_singleton()->save();
		}
		_log_cat(LOG_INFO, LOG_REPLICATION, vformat("Project settings snapshot applied: %d ok, %d dropped (filter)", applied, dropped));
	} else if (type == multiuser_editor::kActionFsOp) {
		Dictionary action_data = p_action.get("data", Dictionary());
		String op = String(action_data.get("op", ""));
		if (op == "rename") {
			String raw_old = String(action_data.get("old_path", ""));
			String raw_new = String(action_data.get("new_path", ""));
			String old_path;
			String new_path;
			if (!MultiuserEditorActionInterceptor::canonicalize_res_path(raw_old, old_path) ||
					!MultiuserEditorActionInterceptor::canonicalize_res_path(raw_new, new_path)) {
				_log_cat(LOG_WARN, LOG_FILESYSTEM, "fs_op rename: rejected unsafe path");
			} else if (_is_path_protected_for_action(old_path) || _is_path_protected_for_action(new_path)) {
				_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("fs_op rename: refusing protected path '%s' -> '%s'", old_path, new_path));
				_record_security_event_kind(MultiuserEditorDock::KIND_PROTECTED_PATH, LOG_WARN, LOG_PERMISSIONS, vformat("fs_op rename blocked on protected path sender=%d", p_sender_net_id));
				_bump_drop_counter();
			} else {
				Ref<DirAccess> dir = DirAccess::open("res://");
				if (dir.is_valid()) {
					String rel_old = old_path.substr(6, old_path.length() - 6);
					String rel_new = new_path.substr(6, new_path.length() - 6);
					dir->rename(rel_old, rel_new);
					if (EditorFileSystem::get_singleton()) {
						EditorFileSystem::get_singleton()->scan();
					}
				}
			}
		} else if (op == "remove") {
			String raw_path = String(action_data.get("path", ""));
			String path;
			if (!MultiuserEditorActionInterceptor::canonicalize_res_path(raw_path, path)) {
				_log_cat(LOG_WARN, LOG_FILESYSTEM, "fs_op remove: rejected unsafe path " + raw_path);
			} else if (_is_path_protected_for_action(path)) {
				_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("fs_op remove: refusing protected path '%s'", path));
				_record_security_event_kind(MultiuserEditorDock::KIND_PROTECTED_PATH, LOG_WARN, LOG_PERMISSIONS, vformat("fs_op remove blocked on protected path sender=%d", p_sender_net_id));
				_bump_drop_counter();
			} else {
				Ref<DirAccess> dir = DirAccess::open("res://");
				if (dir.is_valid()) {
					String rel_path = path.substr(6, path.length() - 6);
					dir->remove(rel_path);
					if (EditorFileSystem::get_singleton()) {
						EditorFileSystem::get_singleton()->scan();
					}
				}
			}
		} else {
			_log_cat(LOG_WARN, LOG_NETWORK, vformat("fs_op dropped: unknown op='%s' from net_id=%d", op, p_sender_net_id));
			_record_security_event_kind(MultiuserEditorDock::KIND_UNKNOWN_ACTION, LOG_WARN, LOG_NETWORK, vformat("fs_op unknown op='%s' sender=%d", op, p_sender_net_id));
			_bump_drop_counter();
		}

	} else if (type == multiuser_editor::kActionTileSync) {
		Dictionary action_data = p_action.get("data", Dictionary());
		String path = String(action_data.get("node_path", ""));
		String clean = lock_manager.clean_path(path);
		if (!scene_root) {
			_log_cat(LOG_WARN, LOG_REPLICATION, "tile_sync dropped: no edited scene root");
			_record_security_event_kind(MultiuserEditorDock::KIND_REPLICATION_FAILED, LOG_WARN, LOG_REPLICATION, vformat("tile_sync rejected: no scene root sender=%d", p_sender_net_id));
			return;
		}
		Node *node = MultiuserEditorActionInterceptor::is_safe_node_path(clean) ? scene_root->get_node_or_null(NodePath(clean)) : nullptr;
		if (node && node->is_class("TileMapLayer") && action_data.has("tile_data")) {
			PackedByteArray blob = action_data.get("tile_data", PackedByteArray());
			const int tile_data_max = MAX(0, int(MULTIUSER_GET("blazium/multiuser_editor/limits/tile_data_max_bytes", 4 * 1024 * 1024)));
			if (tile_data_max > 0 && blob.size() > tile_data_max) {
				_log_cat(LOG_WARN, LOG_REPLICATION, vformat("tile_sync(tile_data) recv dropped: size %d exceeds cap %d", blob.size(), tile_data_max));
				_record_security_event_kind(MultiuserEditorDock::KIND_REPLICATION_FAILED, LOG_WARN, LOG_REPLICATION, vformat("tile_sync tile_data oversize %d > %d sender=%d", blob.size(), tile_data_max, p_sender_net_id));
				return;
			}
			node->set("tile_data", blob);
			return;
		}
		if (node && node->is_class("TileMapLayer")) {
			Vector2i coords = action_data.get("coords", Vector2i());
			int source_id = action_data.get("source_id", -1);
			Vector2i atlas_coords = action_data.get("atlas_coords", Vector2i());
			int alternative_tile = action_data.get("alternative_tile", 0);
			const int tile_coord_max = MAX(1, int(MULTIUSER_GET("blazium/multiuser_editor/limits/tile_coord_max", 1 << 24)));
			const int tile_source_max = MAX(1, int(MULTIUSER_GET("blazium/multiuser_editor/limits/tile_source_max", 1 << 16)));
			const int kCoordMin = -tile_coord_max;
			const int kCoordMax = tile_coord_max;
			const int kAltMax = tile_source_max;
			if (coords.x < kCoordMin || coords.x > kCoordMax || coords.y < kCoordMin || coords.y > kCoordMax) {
				_log_cat(LOG_WARN, LOG_REPLICATION, vformat("tile_sync dropped: coords out of range (%d,%d)", coords.x, coords.y));
				_record_security_event_kind(MultiuserEditorDock::KIND_REPLICATION_FAILED, LOG_WARN, LOG_REPLICATION, vformat("tile_sync coords out-of-range (%d,%d) sender=%d", coords.x, coords.y, p_sender_net_id));
				return;
			}
			if (source_id < -1 || source_id > tile_coord_max) {
				_log_cat(LOG_WARN, LOG_REPLICATION, vformat("tile_sync dropped: source_id=%d out of range", source_id));
				_record_security_event_kind(MultiuserEditorDock::KIND_REPLICATION_FAILED, LOG_WARN, LOG_REPLICATION, vformat("tile_sync source_id=%d out-of-range sender=%d", source_id, p_sender_net_id));
				return;
			}
			if (atlas_coords.x < -1 || atlas_coords.x > kAltMax || atlas_coords.y < -1 || atlas_coords.y > kAltMax) {
				_log_cat(LOG_WARN, LOG_REPLICATION, vformat("tile_sync dropped: atlas_coords out of range (%d,%d)", atlas_coords.x, atlas_coords.y));
				_record_security_event_kind(MultiuserEditorDock::KIND_REPLICATION_FAILED, LOG_WARN, LOG_REPLICATION, vformat("tile_sync atlas_coords out-of-range (%d,%d) sender=%d", atlas_coords.x, atlas_coords.y, p_sender_net_id));
				return;
			}
			if (alternative_tile < 0 || alternative_tile > kAltMax) {
				_log_cat(LOG_WARN, LOG_REPLICATION, vformat("tile_sync dropped: alternative_tile=%d out of range", alternative_tile));
				_record_security_event_kind(MultiuserEditorDock::KIND_REPLICATION_FAILED, LOG_WARN, LOG_REPLICATION, vformat("tile_sync alternative_tile=%d out-of-range sender=%d", alternative_tile, p_sender_net_id));
				return;
			}
			node->call("set_cell", coords, source_id, atlas_coords, alternative_tile);
		} else {
			_log_cat(LOG_WARN, LOG_REPLICATION, vformat("tile_sync dropped: node missing or wrong class for path '%s' (sender=%d)", clean, p_sender_net_id));
			_record_security_event_kind(MultiuserEditorDock::KIND_REPLICATION_FAILED, LOG_WARN, LOG_REPLICATION, vformat("tile_sync target invalid path='%s' sender=%d", clean, p_sender_net_id));
		}
	} else if (type == multiuser_editor::kActionResourceSync) {
		Dictionary action_data = p_action.get("data", Dictionary());
		String node_path = action_data.get("node_path", "");

		if (node_path.is_empty()) {
			_log_cat(LOG_WARN, LOG_REPLICATION, "resource_sync dropped: empty node_path");
			_record_security_event_kind(MultiuserEditorDock::KIND_REPLICATION_FAILED, LOG_WARN, LOG_REPLICATION, vformat("resource_sync empty node_path sender=%d", p_sender_net_id));
			return;
		}
		String res_name = action_data.get("name", "");
		PackedByteArray res_data = action_data.get("data", PackedByteArray());
		if (res_data.is_empty()) {
			_log_cat(LOG_WARN, LOG_REPLICATION, vformat("resource_sync dropped: empty payload for '%s' (sender=%d)", node_path, p_sender_net_id));
			_record_security_event_kind(MultiuserEditorDock::KIND_REPLICATION_FAILED, LOG_WARN, LOG_REPLICATION, vformat("resource_sync empty payload path='%s' sender=%d", node_path, p_sender_net_id));
			return;
		}
		{
			const int resource_sync_max = MAX(0, int(MULTIUSER_GET("blazium/multiuser_editor/limits/resource_sync_max_bytes", 5 * 1024 * 1024)));
			if (resource_sync_max > 0 && res_data.size() > resource_sync_max) {
				_log_cat(LOG_WARN, LOG_REPLICATION, vformat("resource_sync: dropped payload %d > cap %d", res_data.size(), resource_sync_max));
				_record_security_event_kind(MultiuserEditorDock::KIND_REPLICATION_FAILED, LOG_WARN, LOG_REPLICATION, vformat("resource_sync oversize payload %d > %d sender=%d", res_data.size(), resource_sync_max, p_sender_net_id));
			} else if (!MultiuserEditorActionInterceptor::is_safe_property_name(res_name)) {
				_log_cat(LOG_WARN, LOG_PERMISSIONS, "resource_sync: dropped unsafe resource property name: " + res_name);
				_record_security_event_kind(MultiuserEditorDock::KIND_REPLICATION_FAILED, LOG_WARN, LOG_PERMISSIONS, vformat("resource_sync unsafe property name='%s' sender=%d", res_name, p_sender_net_id));
			} else {
				Variant res_var;
				if (decode_variant(res_var, res_data.ptr(), res_data.size(), nullptr, false) == OK) {
					const int64_t value_cap = int64_t(MULTIUSER_GET("blazium/multiuser_editor/limits/property_value_max_bytes", 4 * 1024 * 1024));
					if (!MultiuserEditorActionInterceptor::is_safe_remote_value(res_var, value_cap)) {
						_log_cat(LOG_WARN, LOG_REPLICATION, vformat("resource_sync: decoded variant exceeds cap %d bytes", value_cap));
						_record_security_event_kind(MultiuserEditorDock::KIND_REPLICATION_FAILED, LOG_WARN, LOG_REPLICATION, vformat("resource_sync decoded variant exceeds cap %d sender=%d", value_cap, p_sender_net_id));
					} else {
						Node *node = action_interceptor.is_safe_node_path(node_path) ? scene_root->get_node_or_null(NodePath(node_path)) : nullptr;
						if (node) {
							node->set(res_name, res_var);
						}
					}
				}
			}
		}

	} else if (type == multiuser_editor::kActionFsMove) {
		Dictionary action_data = p_action.get("data", Dictionary());
		String raw_old = String(action_data.get("old_path", ""));
		String raw_new = String(action_data.get("new_path", ""));
		String old_path;
		String new_path;
		if (raw_old.is_empty() || raw_new.is_empty()) {
			_log_cat(LOG_WARN, LOG_FILESYSTEM, "fs_move: empty path");
			_record_security_event_kind(MultiuserEditorDock::KIND_INVALID_PACKET, LOG_WARN, LOG_FILESYSTEM, "fs_move: empty path");
		} else if (!MultiuserEditorActionInterceptor::canonicalize_res_path(raw_old, old_path) ||
				!MultiuserEditorActionInterceptor::canonicalize_res_path(raw_new, new_path)) {
			_log_cat(LOG_WARN, LOG_FILESYSTEM, "fs_move: rejected unsafe path");
			_record_security_event_kind(MultiuserEditorDock::KIND_REPLICATION_FAILED, LOG_WARN, LOG_FILESYSTEM, "fs_move: rejected unsafe path");
		} else {
			suppress_scene_events = true;
			Ref<DirAccess> dir = DirAccess::open("res://");
			if (dir.is_valid()) {
				String rel_old = old_path.substr(6, old_path.length() - 6);
				String rel_new = new_path.substr(6, new_path.length() - 6);
				dir->rename(rel_old, rel_new);
			}
			if (EditorInterface::get_singleton() && EditorInterface::get_singleton()->get_resource_file_system()) {
				EditorInterface::get_singleton()->get_resource_file_system()->update_file(new_path);
				EditorInterface::get_singleton()->get_resource_file_system()->update_file(old_path);
			}
			suppress_scene_events = false;
		}

	} else if (type == multiuser_editor::kActionFsRemove) {
		Dictionary action_data = p_action.get("data", Dictionary());
		String raw_path = String(action_data.get("path", ""));
		String path;
		if (raw_path.is_empty()) {
			_log_cat(LOG_WARN, LOG_FILESYSTEM, "fs_remove: empty path");
			_record_security_event_kind(MultiuserEditorDock::KIND_INVALID_PACKET, LOG_WARN, LOG_FILESYSTEM, "fs_remove: empty path");
		} else if (!MultiuserEditorActionInterceptor::canonicalize_res_path(raw_path, path)) {
			const String _msg = "fs_remove: rejected unsafe path " + raw_path;
			_log_cat(LOG_WARN, LOG_FILESYSTEM, _msg);
			_record_security_event_kind(MultiuserEditorDock::KIND_REPLICATION_FAILED, LOG_WARN, LOG_FILESYSTEM, _msg);
		} else {
			suppress_scene_events = true;
			Ref<DirAccess> dir = DirAccess::open("res://");
			if (dir.is_valid()) {
				String rel_path = path.substr(6, path.length() - 6);
				dir->remove(rel_path);
			}
			if (EditorInterface::get_singleton() && EditorInterface::get_singleton()->get_resource_file_system()) {
				EditorInterface::get_singleton()->get_resource_file_system()->update_file(path);
			}
			suppress_scene_events = false;
		}
	} else if (type == multiuser_editor::kActionFsRefresh) {
		if (EditorInterface::get_singleton() && EditorInterface::get_singleton()->get_resource_file_system()) {
			EditorInterface::get_singleton()->get_resource_file_system()->scan();
		}
	} else if (type == multiuser_editor::kActionFileApplyDelete || type == multiuser_editor::kActionFileApplyMove ||
			type == multiuser_editor::kActionFileApplyBegin || type == multiuser_editor::kActionFileApplyChunk || type == multiuser_editor::kActionFileApplyEnd) {
		suppress_fs_broadcast = true;
		const bool reimport = bool(MULTIUSER_GET("blazium/multiuser_editor/file_sync/include_imports", true));
		const bool inc_imp = bool(MULTIUSER_GET("blazium/multiuser_editor/file_sync/include_imports", true));
		const Error fs_err = filesystem_sync.apply_incoming_transfer(type, p_action, true, reimport, inc_imp);
		suppress_fs_broadcast = false;
		_log_cat(LOG_DEBUG, LOG_FILESYSTEM, vformat("Applied transfer '%s' result=%d", type, int(fs_err)));
	} else if (type == multiuser_editor::kActionFsSnapshotDone) {
		if (network.get_mode() == MultiuserEditorNetwork::MODE_JOIN && bool(MULTIUSER_GET("blazium/multiuser_editor/file_sync/enabled", true))) {
			_update_filesystem_sync_policy();
			const bool inc_imp = bool(MULTIUSER_GET("blazium/multiuser_editor/file_sync/include_imports", true));
			const PackedStringArray inc = _read_packed_string_array_setting("blazium/multiuser_editor/file_sync/include_patterns", Vector<String>{ "res://*" });
			const PackedStringArray exc = _read_packed_string_array_setting("blazium/multiuser_editor/file_sync/exclude_patterns", Vector<String>{ ".godot/*", ".git/*", "*.tmp", "*.~lock", ".vscode/*", ".idea/*" });
			filesystem_sync.clear_snapshot();
			filesystem_sync.capture_snapshot_from_res(inc_imp, inc, exc);
		}
		_log_cat(LOG_INFO, LOG_FILESYSTEM, "Filesystem snapshot from host completed.");
	} else if (type == multiuser_editor::kActionFileReject) {
		Dictionary rd = p_action.get("data", Dictionary());
		_log_cat(LOG_WARN, LOG_FILESYSTEM, vformat("File sync rejected: %s (path=%s)", String(rd.get("reason", "?")), String(rd.get("path", ""))));
	} else if (network.get_mode() == MultiuserEditorNetwork::MODE_HOST &&
			(type == multiuser_editor::kActionFileProposeDelete || type == multiuser_editor::kActionFileProposeMove ||
					type == multiuser_editor::kActionFileProposeBegin || type == multiuser_editor::kActionFileProposeChunk || type == multiuser_editor::kActionFileProposeEnd)) {
		Vector<Dictionary> broadcast;
		String rej;
		const Error acc = filesystem_sync.host_accumulate_propose(p_sender_net_id, type, p_action, broadcast, rej);
		if (acc == ERR_BUSY) {
			return;
		}
		if (acc != OK) {
			Dictionary rdata;
			rdata["reason"] = rej;
			const Dictionary pdata = p_action.get("data", Dictionary());
			rdata["path"] = String(pdata.get("path", ""));
			network.send_action_to(p_sender_net_id, _make_action(multiuser_editor::kActionFileReject, rdata));
			return;
		}
		for (int i = 0; i < broadcast.size(); i++) {
			_emit_action(broadcast[i]);
		}
	} else if (type == multiuser_editor::kActionTeamPlayStart) {
		if (bool(MULTIUSER_GET("blazium/multiuser_editor/enable_team_play", true))) {
			EditorInterface::get_singleton()->play_main_scene();
		}
	} else if (type == multiuser_editor::kActionTeamPlayStop) {
		if (bool(MULTIUSER_GET("blazium/multiuser_editor/enable_team_play", true))) {
			EditorInterface::get_singleton()->stop_playing_scene();
		}
	} else if (type == multiuser_editor::kActionVfxRestart) {
		Dictionary act_data = p_action.get("data", Dictionary());
		String path = act_data.get("path", "");

		if (path.is_empty()) {
			_log_cat(LOG_WARN, LOG_REPLICATION, "vfx_restart dropped: empty path");
			_record_security_event_kind(MultiuserEditorDock::KIND_INVALID_PACKET, LOG_WARN, LOG_REPLICATION, "vfx_restart dropped: empty path");
			return;
		}
		Node *node = MultiuserEditorActionInterceptor::is_safe_node_path(path) ? scene_root->get_node_or_null(NodePath(path)) : nullptr;

		if (node) {
			node->call("restart");
		} else {
			_log_cat(LOG_WARN, LOG_REPLICATION, vformat("vfx_restart dropped: node missing or unsafe path '%s' (sender=%d)", path, p_sender_net_id));
			_record_security_event_kind(MultiuserEditorDock::KIND_REPLICATION_FAILED, LOG_WARN, LOG_REPLICATION, vformat("vfx_restart target missing path='%s' sender=%d", path, p_sender_net_id));
		}
	} else if (type == multiuser_editor::kActionShaderAction) {
		Dictionary act_data = p_action.get("data", Dictionary());
		String path = act_data.get("path", "");
		Node *node = MultiuserEditorActionInterceptor::is_safe_node_path(path) ? scene_root->get_node_or_null(NodePath(path)) : nullptr;

		if (!node) {
			_log_cat(LOG_WARN, LOG_REPLICATION, vformat("shader_action dropped: node missing or unsafe path '%s' (sender=%d)", path, p_sender_net_id));
			_record_security_event_kind(MultiuserEditorDock::KIND_REPLICATION_FAILED, LOG_WARN, LOG_REPLICATION, vformat("shader_action target missing path='%s' sender=%d", path, p_sender_net_id));
			return;
		}
		{
			Variant mat = node->get("material");
			if (mat.get_type() != Variant::OBJECT) {
				_log_cat(LOG_WARN, LOG_REPLICATION, vformat("shader_action dropped: node '%s' has no material object (sender=%d)", path, p_sender_net_id));
				_record_security_event_kind(MultiuserEditorDock::KIND_REPLICATION_FAILED, LOG_WARN, LOG_REPLICATION, vformat("shader_action no material path='%s' sender=%d", path, p_sender_net_id));
				return;
			}
			{
				Ref<ShaderMaterial> smat = mat;
				if (smat.is_null() || smat->get_shader().is_null()) {
					_log_cat(LOG_WARN, LOG_REPLICATION, vformat("shader_action dropped: node '%s' material is not a ShaderMaterial with a shader (sender=%d)", path, p_sender_net_id));
					_record_security_event_kind(MultiuserEditorDock::KIND_REPLICATION_FAILED, LOG_WARN, LOG_REPLICATION, vformat("shader_action non-shader material path='%s' sender=%d", path, p_sender_net_id));
					return;
				}
				{
					Ref<VisualShader> vs = smat->get_shader();
					if (vs.is_null()) {
						_log_cat(LOG_WARN, LOG_REPLICATION, vformat("shader_action dropped: shader on '%s' is not a VisualShader (sender=%d)", path, p_sender_net_id));
						_record_security_event_kind(MultiuserEditorDock::KIND_REPLICATION_FAILED, LOG_WARN, LOG_REPLICATION, vformat("shader_action not VisualShader path='%s' sender=%d", path, p_sender_net_id));
						return;
					}
					{
						const int mode_int = int(act_data.get("mode", -1));
						if (mode_int < 0 || mode_int >= int(VisualShader::TYPE_MAX)) {
							_log_cat(LOG_WARN, LOG_REPLICATION, vformat("shader_action dropped: mode=%d out of range", mode_int));
							_record_security_event_kind(MultiuserEditorDock::KIND_REPLICATION_FAILED, LOG_WARN, LOG_REPLICATION, vformat("shader_action mode=%d out-of-range sender=%d", mode_int, p_sender_net_id));
							return;
						}
						const VisualShader::Type vs_mode = (VisualShader::Type)mode_int;
						String vs_type = act_data.get("vs_type", "");

						const int node_id_max = MAX(1, int(MULTIUSER_GET("blazium/multiuser_editor/limits/visual_shader_node_id_max", 1048576)));
						if (vs_type == "connect" || vs_type == "disconnect") {
							const int from_node = int(act_data.get("from_node", -1));
							const int from_port = int(act_data.get("from_port", -1));
							const int to_node = int(act_data.get("to_node", -1));
							const int to_port = int(act_data.get("to_port", -1));
							const int port_max = multiuser_editor::kPortMax;
							if (from_node < 0 || to_node < 0 || from_port < 0 || to_port < 0 ||
									from_port > port_max || to_port > port_max ||
									from_node > node_id_max || to_node > node_id_max) {
								_log_cat(LOG_WARN, LOG_REPLICATION, "shader_action dropped: negative or out-of-range node/port id");
								_record_security_event_kind(MultiuserEditorDock::KIND_REPLICATION_FAILED, LOG_WARN, LOG_REPLICATION, vformat("shader_action node/port out-of-range sender=%d", p_sender_net_id));
								return;
							}
							if (vs_type == "connect") {
								vs->connect_nodes(vs_mode, from_node, from_port, to_node, to_port);
							} else {
								vs->disconnect_nodes(vs_mode, from_node, from_port, to_node, to_port);
							}
						} else if (vs_type == "node_move") {
							const int node_id = int(act_data.get("node_id", -1));
							if (node_id < 0 || node_id > node_id_max) {
								_log_cat(LOG_WARN, LOG_REPLICATION, vformat("shader_action dropped: node_id=%d out of range [0,%d]", node_id, node_id_max));
								_record_security_event_kind(MultiuserEditorDock::KIND_REPLICATION_FAILED, LOG_WARN, LOG_REPLICATION, vformat("shader_action node_id=%d out-of-range sender=%d", node_id, p_sender_net_id));
								return;
							}
							vs->set_node_position(vs_mode, node_id, (Vector2)act_data.get("position", Vector2()));
						} else {
							_log_cat(LOG_WARN, LOG_NETWORK, vformat("shader_action dropped: unknown vs_type='%s' from net_id=%d", vs_type, p_sender_net_id));
							_record_security_event_kind(MultiuserEditorDock::KIND_UNKNOWN_ACTION, LOG_WARN, LOG_NETWORK, vformat("shader_action unknown vs_type='%s' sender=%d", vs_type, p_sender_net_id));
							_bump_drop_counter();
							return;
						}
					}
				}
			}
		}
	} else if (type == multiuser_editor::kActionMagicRepairRequest) {
		if (network.get_mode() == MultiuserEditorNetwork::MODE_HOST) {
			Dictionary act_data = p_action.get("data", Dictionary());
			uint64_t remote_hash = uint64_t(act_data.get("hash", (uint64_t)0));
			uint64_t local_hash = action_interceptor.generate_scene_hash(get_tree() ? get_tree()->get_edited_scene_root() : nullptr);
			if (remote_hash != local_hash) {
				network.send_action_to(p_sender_net_id, _make_action(multiuser_editor::kActionMagicRepairStart, Dictionary()));
				_send_initial_state(p_sender_net_id);
			}
		} else {
			_log_cat(LOG_WARN, LOG_REPLICATION, vformat("magic_repair_request dropped: not host (sender=%d)", p_sender_net_id));
			_record_security_event_kind(MultiuserEditorDock::KIND_REPLICATION_FAILED, LOG_WARN, LOG_REPLICATION, vformat("magic_repair_request received as non-host sender=%d", p_sender_net_id));
		}
	} else if (type == multiuser_editor::kActionMagicRepairStart) {
		if (p_sender_net_id != 1) {
			_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("magic_repair_start dropped: sender net_id=%d is not host (1)", p_sender_net_id));
			_record_security_event_kind(MultiuserEditorDock::KIND_PERMISSION_DENIED, LOG_WARN, LOG_PERMISSIONS, vformat("magic_repair_start from non-host sender=%d", p_sender_net_id));
			return;
		}
		_wipe_edited_scene();
	} else if (type == multiuser_editor::kActionTelemetry) {
		const int min_telem_ms = MAX(0, int(MULTIUSER_GET("blazium/multiuser_editor/limits/telemetry_min_interval_ms", 250)));
		if (min_telem_ms > 0) {
			const uint64_t now_msec = OS::get_singleton()->get_ticks_msec();
			HashMap<int, uint64_t>::Iterator it = _telemetry_last_msec.find(p_sender_net_id);
			if (it != _telemetry_last_msec.end() && (now_msec - it->value) < (uint64_t)min_telem_ms) {
				return;
			}
			_telemetry_last_msec[p_sender_net_id] = now_msec;
		}
		Dictionary action_data = p_action.get("data", Dictionary());
		if (action_data.has("role")) {
			const String inner_role = String(action_data.get("role", ""));
			if (inner_role.length() > multiuser_editor::kRoleFieldTelemetryMax) {
				const String _msg = vformat("Dropped telemetry: data.role len=%d exceeds %d", inner_role.length(), multiuser_editor::kRoleFieldTelemetryMax);
				_log_cat(LOG_WARN, LOG_PERMISSIONS, _msg);
				_record_security_event_kind(MultiuserEditorDock::KIND_INVALID_PACKET, LOG_WARN, LOG_PERMISSIONS, _msg);
				return;
			}
		}
		if (action_data.has("fps")) {
			const double fps = double(action_data.get("fps", 0.0));
			if (!(fps >= 0.0 && fps <= 1e6)) {
				const String _msg = vformat("Dropped telemetry: fps=%f out of range", fps);
				_log_cat(LOG_WARN, LOG_PERMISSIONS, _msg);
				_record_security_event_kind(MultiuserEditorDock::KIND_INVALID_PACKET, LOG_WARN, LOG_PERMISSIONS, _msg);
				return;
			}
		}
		if (action_data.has("memory")) {
			const int64_t mem = int64_t(action_data.get("memory", 0));
			if (mem < 0 || mem > (int64_t(1) << 48)) {
				const String _msg = vformat("Dropped telemetry: memory=%d out of range", mem);
				_log_cat(LOG_WARN, LOG_PERMISSIONS, _msg);
				_record_security_event_kind(MultiuserEditorDock::KIND_INVALID_PACKET, LOG_WARN, LOG_PERMISSIONS, _msg);
				return;
			}
		}

		Dictionary safe_telem;
		if (action_data.has("role")) {
			safe_telem["role"] = String(action_data["role"]);
		}
		if (action_data.has("fps")) {
			safe_telem["fps"] = double(action_data["fps"]);
		}
		if (action_data.has("memory")) {
			safe_telem["memory"] = int64_t(action_data["memory"]);
		}
		if (action_data.has("mcp_active")) {
			safe_telem["mcp_active"] = bool(action_data["mcp_active"]);
		}
		if (dock) {
			dock->update_peer_telemetry(peer_id, safe_telem);
		}
	} else if (type == multiuser_editor::kActionGitRequest) {
		if (network.get_mode() != MultiuserEditorNetwork::MODE_HOST) {
			_log_cat(LOG_WARN, LOG_NETWORK, vformat("git_request dropped: not host (sender=%d)", p_sender_net_id));
			_record_security_event_kind(MultiuserEditorDock::KIND_REPLICATION_FAILED, LOG_WARN, LOG_NETWORK, vformat("git_request received as non-host sender=%d", p_sender_net_id));
			return;
		}
		GitOpRequest req;
		String reason;
		if (!_gitops_parse(p_action, p_sender_net_id, peer_id, req, reason)) {
			_gitops_broadcast_result(req, false, -1, String(), reason);
			return;
		}
		if (!bool(MULTIUSER_GET("blazium/multiuser_editor/git_remote_actions_enabled", false))) {
			_gitops_broadcast_result(req, false, -1, String(), "remote_actions_disabled");
			return;
		}
		if (!_gitops_validate(req, reason)) {
			_gitops_broadcast_result(req, false, -1, String(), reason);
			return;
		}
		if (req.op == "force_push") {
			const bool allow = bool(MULTIUSER_GET("blazium/multiuser_editor/allow_editor_force_push", false));
			if (!allow && remote_role != "Admin") {
				_gitops_broadcast_result(req, false, -1, String(), "force_push_admin_only");
				return;
			}
		}
		if (!_can_use_git()) {
			_gitops_broadcast_result(req, false, -1, String(), "git_unavailable");
			return;
		}
		if (!_gitops_throttle_ok(p_sender_net_id)) {
			_gitops_broadcast_result(req, false, -1, String(), "throttled");
			return;
		}
		_gitops_run_async(req);
	} else if (type == multiuser_editor::kActionGitResponse) {
		if (dock) {
			Dictionary action_data = p_action.get("data", Dictionary());

			auto cap_str = [](const Variant &p_v, int p_max) -> String {
				if (p_v.get_type() != Variant::STRING && p_v.get_type() != Variant::STRING_NAME) {
					return String();
				}
				String s = String(p_v);
				if (s.length() > p_max) {
					s = s.substr(0, MAX(1, p_max - 3)) + "...";
				}
				return s;
			};
			if (action_data.has("op")) {
				action_data["op"] = cap_str(action_data["op"], 32);
			}
			if (action_data.has("reason")) {
				action_data["reason"] = cap_str(action_data["reason"], 256);
			}
			if (action_data.has("requester_peer_id")) {
				action_data["requester_peer_id"] = cap_str(action_data["requester_peer_id"], 64);
			}
			dock->show_git_response(action_data);
		}
	} else if (type == multiuser_editor::kActionGlobalUndo) {
		if (network.get_mode() == MultiuserEditorNetwork::MODE_HOST) {
			if (global_history.size() > 0) {
				Dictionary last_action = global_history[global_history.size() - 1];
				global_history.remove_at(global_history.size() - 1);

				String prev_type = last_action.get("type", "");
				Dictionary prev_data = last_action.get("data", Dictionary());
				if (prev_type == multiuser_editor::kActionProperty) {
					Dictionary inverse_data = prev_data.duplicate();
					inverse_data["value"] = prev_data.get("old_value", Variant());
					inverse_data["old_value"] = prev_data.get("value", Variant());

					Dictionary inverse_action = _make_action(multiuser_editor::kActionProperty, inverse_data);
					inverse_action["node_path"] = last_action.get("node_path", "");
					_emit_action(inverse_action);
					action_interceptor.apply_remote_action(inverse_action);
				} else if (prev_type == multiuser_editor::kActionNodeAdd) {
					Dictionary inverse_action = _make_action(multiuser_editor::kActionNodeDelete, prev_data);
					inverse_action["node_path"] = last_action.get("node_path", "");
					_emit_action(inverse_action);
					action_interceptor.apply_remote_action(inverse_action);
				} else if (prev_type == multiuser_editor::kActionNodeDelete) {
					Dictionary inverse_action = _make_action(multiuser_editor::kActionNodeAdd, prev_data);
					_emit_action(inverse_action);
					action_interceptor.apply_remote_action(inverse_action);
				} else {
					_log_cat(LOG_INFO, LOG_REPLICATION, vformat("global_undo: dropping non-invertible history entry type='%s' (sender=%d)", prev_type, p_sender_net_id));
				}
			}
		}

	} else if (type == multiuser_editor::kActionAuthChallenge) {
		Dictionary act_data = p_action.get("data", Dictionary());
		String challenge = act_data.get("challenge", "");
		String hmac = (challenge + network.get_session_password()).sha256_text();

		Dictionary hs_data;
		hs_data["peer_id"] = local_peer_id;
		hs_data["role"] = local_role;
		hs_data["hmac"] = hmac;

		String client_jwt = String(MULTIUSER_GET("blazium/multiuser_editor/client_jwt", ""));
		if (!client_jwt.is_empty()) {
			hs_data["jwt"] = client_jwt;
		}

		_log_connection(vformat("Received auth_challenge, sending handshake (jwt=%s)", client_jwt.is_empty() ? "no" : "yes"));
		network.send_action_to(p_sender_net_id, _make_action(multiuser_editor::kActionHandshake, hs_data));

	} else if (type == "handshake") {
		Dictionary action_data = p_action.get("data", Dictionary());
		String remote_peer_id = action_data.get("peer_id", "");

		String incoming_role = "Editor";
		String remote_hmac = action_data.get("hmac", "");
		String remote_jwt = action_data.get("jwt", "");

		if (!remote_peer_id.is_empty() && !_is_valid_peer_id_field(remote_peer_id)) {
			_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("handshake dropped: invalid peer_id len=%d from net_id=%d", remote_peer_id.length(), p_sender_net_id));
			_record_security_event_kind(MultiuserEditorDock::KIND_AUTH_FAILED, LOG_WARN, LOG_PERMISSIONS, vformat("handshake invalid peer_id len=%d sender=%d", remote_peer_id.length(), p_sender_net_id));
			if (network.get_mode() == MultiuserEditorNetwork::MODE_HOST) {
				network.disconnect_peer(p_sender_net_id);
			}
			return;
		}

		if (network.get_mode() == MultiuserEditorNetwork::MODE_HOST) {
			bool auth_success = false;

			String per_peer_challenge;
			const bool have_challenge = _consume_pending_challenge(p_sender_net_id, per_peer_challenge);
			if (!have_challenge) {
				_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("handshake rejected: no pending challenge for net_id=%d", p_sender_net_id));
				_log_connection(vformat("Auth failed: no pending challenge for net_id=%d", p_sender_net_id));
				_record_security_event_kind(MultiuserEditorDock::KIND_AUTH_FAIL, LOG_WARN, LOG_PERMISSIONS, vformat("auth fail: no challenge sender=%d", p_sender_net_id));
			} else if (bool(MULTIUSER_GET("blazium/multiuser_editor/require_jwt", false))) {
				String jwt_secret = String(MULTIUSER_GET("blazium/multiuser_editor/jwt_secret_key", ""));
				if (remote_jwt.is_empty()) {
					_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("JWT auth failed: reason=missing_token sender_net_id=%d", p_sender_net_id));
					_log_connection(vformat("JWT auth failed: missing token from net_id=%d", p_sender_net_id));

					_record_security_event_kind(MultiuserEditorDock::KIND_AUTH_FAIL, LOG_WARN, LOG_PERMISSIONS, vformat("auth fail: jwt missing sender=%d", p_sender_net_id));
				} else {
					JWTValidationResult vr = _validate_jwt_full(remote_jwt, jwt_secret);
					if (vr.valid) {
						auth_success = true;
						incoming_role = vr.role;
						_log_connection(vformat("JWT auth ok for net_id=%d role=%s", p_sender_net_id, incoming_role));
					} else {
						_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("JWT auth failed: reason=%s jti_fp=%s sender_net_id=%d", vr.reason, _jwt_jti_fingerprint(vr.jti), p_sender_net_id));
						_log_connection(vformat("JWT auth failed: %s from net_id=%d", vr.reason, p_sender_net_id));
						_record_security_event_kind(MultiuserEditorDock::KIND_AUTH_FAIL, LOG_WARN, LOG_PERMISSIONS, vformat("auth fail: jwt %s sender=%d", vr.reason, p_sender_net_id));
					}
				}
			} else {
				Ref<MultiuserEditorAccessList> auth_list;
				auth_list.instantiate();
				auth_list->set_max_entries(int(MULTIUSER_GET("blazium/multiuser_editor/access_list/max_entries", 256)));
				const String dock_pw = network.get_session_password();
				const bool al_enabled = bool(MULTIUSER_GET("blazium/multiuser_editor/access_list/enabled", true));
				bool file_has_default = false;
				if (al_enabled && _access_list.is_valid()) {
					const Vector<MultiuserEditorAccessList::Entry> file_entries = _access_list->get_entries();
					for (int i = 0; i < file_entries.size(); i++) {
						MultiuserEditorAccessList::Entry e = file_entries[i];
						auth_list->add_or_update(e);

						if (e.codename.to_lower() == "default") {
							file_has_default = true;
						}
					}
				}
				if (!dock_pw.is_empty() && !file_has_default) {
					MultiuserEditorAccessList::Entry implicit;
					implicit.codename = "default";
					implicit.password = dock_pw;
					implicit.role = _resolve_implicit_role_for_dock_password();
					implicit.source = "dock";
					auth_list->add_or_update(implicit);
				}

				if (auth_list->get_entry_count() == 0) {
					const bool allow_legacy = bool(MULTIUSER_GET("blazium/multiuser_editor/allow_empty_secret_handshake", false));
					if (!allow_legacy) {
						_log_cat(LOG_ERROR, LOG_PERMISSIONS, vformat("handshake rejected: no verifier material configured (sender=%d). Configure access list, set a session password, or enable JWT.", p_sender_net_id));
						_record_security_event_kind(MultiuserEditorDock::KIND_AUTH_FAIL, LOG_ERROR, LOG_PERMISSIONS, vformat("auth fail: no verifier material sender=%d", p_sender_net_id));
					} else {
						const String expected_hmac = (per_peer_challenge + dock_pw).sha256_text();
						auth_success = _const_time_eq(remote_hmac, expected_hmac);
						if (!auth_success) {
							_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("handshake auth failed: legacy HMAC mismatch sender=%d", p_sender_net_id));
							_record_security_event_kind(MultiuserEditorDock::KIND_AUTH_FAIL, LOG_WARN, LOG_PERMISSIONS, vformat("auth fail: legacy hmac mismatch sender=%d", p_sender_net_id));
						} else {
							_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("handshake auth via legacy empty-secret path sender=%d", p_sender_net_id));
							_record_security_event_kind(MultiuserEditorDock::KIND_AUTH_OK, LOG_WARN, LOG_PERMISSIONS, vformat("auth ok via legacy empty-secret path sender=%d (allow_empty_secret_handshake=true)", p_sender_net_id));
						}
					}
				} else {
					MultiuserEditorAccessList::Entry matched;
					if (auth_list->find_match_for_hmac(per_peer_challenge, remote_hmac, matched)) {
						auth_success = true;
						incoming_role = matched.role;
						_log_cat(LOG_INFO, LOG_PERMISSIONS, vformat("handshake auth ok: codename=%s src=%s role=%s sender=%d", matched.codename, matched.source, matched.role, p_sender_net_id));
						_record_security_event_kind(MultiuserEditorDock::KIND_AUTH_OK, LOG_INFO, LOG_PERMISSIONS, vformat("auth ok: codename=%s src=%s role=%s sender=%d", matched.codename, matched.source, matched.role, p_sender_net_id));
					} else {
						_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("handshake auth failed: access-list HMAC mismatch sender=%d", p_sender_net_id));
						_record_security_event_kind(MultiuserEditorDock::KIND_AUTH_FAIL, LOG_WARN, LOG_PERMISSIONS, vformat("auth fail: no matching codename sender=%d", p_sender_net_id));
					}
				}
			}

			Dictionary ack_data;
			ack_data["peer_id"] = local_peer_id;
			ack_data["host_role"] = local_role;
			ack_data["success"] = auth_success;

			if (auth_success) {
				network.mark_peer_authenticated(p_sender_net_id);
				network.remember_peer(p_sender_net_id, remote_peer_id);
				network.remember_peer_role(p_sender_net_id, incoming_role);
				ack_data["role"] = incoming_role;

				network.send_action_to(p_sender_net_id, _make_action(multiuser_editor::kActionHandshakeAck, ack_data));
				_send_initial_state(p_sender_net_id);
				if (dock) {
					dock->add_peer(remote_peer_id);
					Dictionary remote_role_dict;
					remote_role_dict["role"] = incoming_role;
					dock->update_peer_telemetry(remote_peer_id, remote_role_dict);
				}

				if (bool(MULTIUSER_GET("blazium/multiuser_editor/server_log_connections", true))) {
					print_line(vformat("[Multiuser Authenticated] Peer fully connected: %s (Role: %s)", remote_peer_id, incoming_role));
				}
			} else {
				if (bool(MULTIUSER_GET("blazium/multiuser_editor/server_log_connections", true))) {
					print_line(vformat("[Multiuser Error] Peer failed authentication (Role Drop)"));
				}
				network.send_action_to(p_sender_net_id, _make_action(multiuser_editor::kActionHandshakeAck, ack_data));
				network.disconnect_peer(p_sender_net_id);
			}
		}
	} else if (type == multiuser_editor::kActionHandshakeAck) {
		if (p_sender_net_id != 1) {
			const String _msg = vformat("handshake_ack dropped: sender net_id=%d is not host (1)", p_sender_net_id);
			_log_cat(LOG_WARN, LOG_PERMISSIONS, _msg);
			_record_security_event_kind(MultiuserEditorDock::KIND_AUTH_FAILED, LOG_WARN, LOG_PERMISSIONS, _msg);
			return;
		}
		Dictionary action_data = p_action.get("data", Dictionary());
		if (!bool(action_data.get("success", false))) {
			if (bool(MULTIUSER_GET("blazium/multiuser_editor/server_log_connections", true))) {
				print_line("[Multiuser Authenticated] Handshake failed: Invalid password or unauthorized role.");
			}
			_log(TTR("Handshake failed: Invalid password."));
			_stop();
			return;
		}
		String remote_peer_id = action_data.get("peer_id", "");
		String host_role = action_data.get("host_role", "Editor");

		if (!remote_peer_id.is_empty() && !_is_valid_peer_id_field(remote_peer_id)) {
			const String _msg = vformat("handshake_ack dropped: invalid host peer_id len=%d", remote_peer_id.length());
			_log_cat(LOG_WARN, LOG_PERMISSIONS, _msg);
			_record_security_event_kind(MultiuserEditorDock::KIND_INVALID_PACKET, LOG_WARN, LOG_PERMISSIONS, _msg);
			_stop();
			return;
		}

		network.remember_peer(p_sender_net_id, remote_peer_id);
		network.remember_peer_role(p_sender_net_id, host_role);
		session_authenticated = true;
		if (bool(MULTIUSER_GET("blazium/multiuser_editor/file_sync/enabled", true))) {
			_update_filesystem_sync_policy();
			const bool inc_imp = bool(MULTIUSER_GET("blazium/multiuser_editor/file_sync/include_imports", true));
			const PackedStringArray inc = _read_packed_string_array_setting("blazium/multiuser_editor/file_sync/include_patterns", Vector<String>{ "res://*" });
			const PackedStringArray exc = _read_packed_string_array_setting("blazium/multiuser_editor/file_sync/exclude_patterns", Vector<String>{ ".godot/*", ".git/*", "*.tmp", "*.~lock", ".vscode/*", ".idea/*" });
			filesystem_sync.clear_snapshot();
			filesystem_sync.capture_snapshot_from_res(inc_imp, inc, exc);
		}
		if (dock) {
			dock->add_peer(remote_peer_id);
			dock->add_peer(local_peer_id);
			Dictionary self_role;
			self_role["role"] = local_role;
			dock->update_peer_telemetry(local_peer_id, self_role);
			Dictionary host_role_dict;
			host_role_dict["role"] = host_role;
			dock->update_peer_telemetry(remote_peer_id, host_role_dict);
			dock->set_connected(vformat("Connected! Role: %s", local_role));

			const bool client_used_jwt = !String(MULTIUSER_GET("blazium/multiuser_editor/client_jwt", "")).is_empty();
			dock->set_auth_mode(client_used_jwt ? MultiuserEditorDock::AUTH_JWT : MultiuserEditorDock::AUTH_HMAC);
			const bool fp_enabled = bool(MULTIUSER_GET("blazium/multiuser_editor/allow_editor_force_push", false));
			dock->set_git_panel_enabled(true, local_role, fp_enabled);
		}
		if (bool(MULTIUSER_GET("blazium/multiuser_editor/server_log_connections", true))) {
			print_line(vformat("[Multiuser Authenticated] Successfully connected to Host as %s", local_role));
			print_line(vformat("[Multiuser Connect] Session open: authenticated as %s", local_role));
		}
		if (s_cli_join_pending) {
			s_cli_join_pending = false;
		}
	} else if (type == multiuser_editor::kActionChat) {
		const int min_chat_ms = MAX(0, int(MULTIUSER_GET("blazium/multiuser_editor/limits/chat_min_interval_ms", 250)));
		if (min_chat_ms > 0) {
			const uint64_t now_msec = OS::get_singleton()->get_ticks_msec();
			HashMap<int, uint64_t>::Iterator it = _chat_last_msec.find(p_sender_net_id);
			if (it != _chat_last_msec.end() && (now_msec - it->value) < (uint64_t)min_chat_ms) {
				_log_cat(LOG_DEBUG, LOG_PERMISSIONS, vformat("Throttled chat from net_id=%d", p_sender_net_id));
				return;
			}
			_chat_last_msec[p_sender_net_id] = now_msec;
		}
		Dictionary action_data = p_action.get("data", Dictionary());
		String message = String(action_data.get("message", ""));
		if (dock) {
			dock->add_chat_message(peer_id, message);
		}
		if (bool(MULTIUSER_GET("blazium/multiuser_editor/server_log_chat", true))) {
			print_line(vformat("[Multiuser Chat] %s: %s", peer_id, message));
		}
	} else if (type == multiuser_editor::kActionAutoworkTrigger) {
		if (!bool(MULTIUSER_GET("blazium/multiuser_editor/allow_remote_autowork", false))) {
			send_chat(vformat("Peer %s blocked autowork execution (Disabled natively).", local_peer_id));
			return;
		}
		send_chat(vformat("Peer %s has started remote Automated Testing...", local_peer_id));

		Autowork *aw = memnew(Autowork);
		EditorInterface::get_singleton()->get_base_control()->add_child(aw);
		aw->add_directory("res://");
		aw->run_tests();

		int passes = aw->get_pass_count();
		int fails = aw->get_fail_count();
		int pendings = aw->get_pending_count();

		String results = vformat("Autowork Results [%s]: Passed: %d | Failed: %d | Pending: %d", local_peer_id, passes, fails, pendings);
		send_chat(results);

		aw->queue_free();
	} else if (type == multiuser_editor::kActionCursorUpdate) {
		const int min_cur_ms = MAX(0, int(MULTIUSER_GET("blazium/multiuser_editor/limits/cursor_update_min_interval_ms", 50)));
		if (min_cur_ms > 0) {
			const uint64_t now_msec = OS::get_singleton()->get_ticks_msec();
			HashMap<int, uint64_t>::Iterator it = _cursor_update_last_msec.find(p_sender_net_id);
			if (it != _cursor_update_last_msec.end() && (now_msec - it->value) < (uint64_t)min_cur_ms) {
				return;
			}
			_cursor_update_last_msec[p_sender_net_id] = now_msec;
		}
		if (ghost_overlay && bool(MULTIUSER_GET("blazium/multiuser_editor/show_remote_cursors", true))) {
			Dictionary action_data = p_action.get("data", Dictionary());
			const String raw_script = String(action_data.has("script") ? action_data.get("script", "") : (action_data.has("script_path") ? action_data.get("script_path", "") : p_action.get("node_path", "")));
			String safe_script;
			if (raw_script.is_empty()) {
				safe_script = String();
			} else if (raw_script.length() > multiuser_editor::kPathLengthMax) {
				const String _msg = vformat("cursor_update dropped: script path too long len=%d", raw_script.length());
				_log_cat(LOG_WARN, LOG_PERMISSIONS, _msg);
				_record_security_event_kind(MultiuserEditorDock::KIND_INVALID_PACKET, LOG_WARN, LOG_PERMISSIONS, _msg);
				return;
			} else if (!MultiuserEditorActionInterceptor::canonicalize_res_path(raw_script, safe_script)) {
				_log_cat(LOG_WARN, LOG_PERMISSIONS, "cursor_update dropped: unsafe script path");
				_record_security_event_kind(MultiuserEditorDock::KIND_REPLICATION_FAILED, LOG_WARN, LOG_PERMISSIONS, "cursor_update dropped: unsafe script path");
				return;
			}
			action_data["script"] = safe_script;
			action_data["script_path"] = safe_script;
			ghost_overlay->update_peer_cursor(peer_id, action_data);
		}
	} else if (type == multiuser_editor::kActionUnlockAll) {
		lock_manager.release_peer(peer_id);
	} else if (type == multiuser_editor::kActionSceneSync) {
		Dictionary action_data = p_action.get("data", Dictionary());
		String raw_filepath = String(action_data.get("filepath", ""));
		String content = String(action_data.get("content", ""));
		String filepath;
		const int64_t scene_sync_cap = int64_t(MULTIUSER_GET("blazium/multiuser_editor/limits/scene_sync_max_bytes", 8 * 1024 * 1024));
		if (raw_filepath.is_empty() || content.is_empty()) {
			_log_cat(LOG_WARN, LOG_FILESYSTEM, "scene_sync: empty path or content");
			_record_security_event_kind(MultiuserEditorDock::KIND_INVALID_PACKET, LOG_WARN, LOG_FILESYSTEM, "scene_sync: empty path or content");
		} else if (int64_t(content.length()) > scene_sync_cap) {
			const String _msg = vformat("scene_sync: content len=%d exceeds cap %d", content.length(), scene_sync_cap);
			_log_cat(LOG_WARN, LOG_FILESYSTEM, _msg);
			_record_security_event_kind(MultiuserEditorDock::KIND_INVALID_PACKET, LOG_WARN, LOG_FILESYSTEM, _msg);
		} else if (!MultiuserEditorActionInterceptor::canonicalize_res_path(raw_filepath, filepath)) {
			const String _msg = "scene_sync: rejected unsafe path " + raw_filepath;
			_log_cat(LOG_WARN, LOG_FILESYSTEM, _msg);
			_record_security_event_kind(MultiuserEditorDock::KIND_REPLICATION_FAILED, LOG_WARN, LOG_FILESYSTEM, _msg);
		} else if (_is_path_protected_for_action(filepath)) {
			_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("scene_sync: refusing protected path '%s'", filepath));
			_record_security_event_kind(MultiuserEditorDock::KIND_PROTECTED_PATH, LOG_WARN, LOG_PERMISSIONS, vformat("scene_sync blocked on protected path sender=%d", p_sender_net_id));
			_bump_drop_counter();
		} else {
			const bool inc_imp = bool(MULTIUSER_GET("blazium/multiuser_editor/file_sync/include_imports", true));
			const PackedStringArray inc = _read_packed_string_array_setting("blazium/multiuser_editor/file_sync/include_patterns", Vector<String>{ "res://*" });
			const PackedStringArray exc = _read_packed_string_array_setting("blazium/multiuser_editor/file_sync/exclude_patterns", Vector<String>{ ".godot/*", ".git/*", "*.tmp", "*.~lock", ".vscode/*", ".idea/*" });
			if (!MultiuserEditorFilesystemSync::test_path_matches_policy(filepath, inc, exc, inc_imp)) {
				_log_cat(LOG_WARN, LOG_FILESYSTEM, "scene_sync: path outside file_sync policy " + filepath);
			} else {
				Ref<FileAccess> file = FileAccess::open(filepath, FileAccess::WRITE);
				if (file.is_valid()) {
					file->store_string(content);
					file->close();
					suppress_scene_events = true;
					if (EditorInterface::get_singleton()) {
						EditorInterface::get_singleton()->reload_scene_from_path(filepath);
					}
					suppress_scene_events = false;
					_log_cat(LOG_INFO, LOG_REPLICATION, vformat("Scene sync applied: %s (%d bytes)", filepath, content.length()));
				} else {
					_log_cat(LOG_WARN, LOG_FILESYSTEM, vformat("Scene sync failed: cannot open '%s' for write", filepath));
				}
			}
		}
	} else {
		if (type == multiuser_editor::kActionProperty) {
			Dictionary action_data = p_action.get("data", Dictionary());
			lock_manager.add_peer_lock(peer_id, String(p_action.get("node_path", action_data.get("path", ""))));
		} else if (type == multiuser_editor::kActionNodeAdd || type == multiuser_editor::kActionNodeDelete) {
			Dictionary action_data = p_action.get("data", Dictionary());
			lock_manager.add_peer_lock(peer_id, String(action_data.get("parent_path", action_data.get("parent", ""))));
		}

		if (type == multiuser_editor::kActionScriptAttach) {
			Dictionary action_data = p_action.get("data", Dictionary());
			script_sync.initialize_buffer_from_content(String(action_data.get("script_path", action_data.get("script", ""))), String(action_data.get("script_content", "")));
		} else if (type == multiuser_editor::kActionScriptDetach) {
			Dictionary action_data = p_action.get("data", Dictionary());
			const String raw_detach_path = String(action_data.get("script_path", action_data.get("script", "")));
			String canonical_detach_path;
			if (MultiuserEditorActionInterceptor::canonicalize_res_path(raw_detach_path, canonical_detach_path)) {
				script_sync.remove_buffer(canonical_detach_path);
			} else if (!raw_detach_path.is_empty()) {
				_log_cat(LOG_WARN, LOG_FILESYSTEM, "script_detach rejected unsafe path: " + raw_detach_path);
			}
		}
		suppress_scene_events = true;
		action_interceptor.apply_remote_action(p_action);
		suppress_scene_events = false;
	}
}

void MultiuserEditorPlugin::_send_initial_state(int p_target_net_id) {
	Dictionary buffers = script_sync.export_all_buffers();
	Array keys = buffers.keys();
	if (keys.size() == 0) {
		Dictionary action;
		Dictionary sync_data;
		sync_data["sync_complete"] = true;
		action["type"] = multiuser_editor::kActionCrdtSync;
		action["peer_id"] = local_peer_id;
		action["timestamp"] = OS::get_singleton()->get_unix_time();
		action["data"] = sync_data;
		network.send_action_to(p_target_net_id, action);
	}
	for (int i = 0; i < keys.size(); i++) {
		String script_path = String(keys[i]);
		Dictionary state = buffers[script_path];
		state["sync_complete"] = i == keys.size() - 1;
		Dictionary action;
		action["type"] = multiuser_editor::kActionCrdtSync;
		action["peer_id"] = local_peer_id;
		action["timestamp"] = OS::get_singleton()->get_unix_time();
		action["node_path"] = script_path;
		action["data"] = state;
		network.send_action_to(p_target_net_id, action);
	}
	_send_project_settings_snapshot(p_target_net_id);
	_send_project_snapshot_to_peer(p_target_net_id);
}

void MultiuserEditorPlugin::_poll_runtime() {
	_update_context();
	_gitops_drain_pending_results();
	if (_hot_settings_dirty) {
		_refresh_hot_settings_cache();
	}
	const bool current_enabled = _hot_enabled;
	if (current_enabled != last_enabled_state) {
		last_enabled_state = current_enabled;
		if (dock) {
			dock->set_module_enabled(current_enabled);
		}
	}
	if (!current_enabled) {
		if (network.is_connected()) {
			_stop();
		}
		return;
	}
	_poll_cli_auto_join();
	_poll_server_auto_host();
	if (!followed_peer_id.is_empty()) {
		jump_to_peer(followed_peer_id);
	}
	const int desired_max_packet_size_mb = int(MULTIUSER_GET("blazium/multiuser_editor/max_packet_size_mb", 8));
	if (desired_max_packet_size_mb != _cached_max_packet_size_mb) {
		_cached_max_packet_size_mb = desired_max_packet_size_mb;
		network.set_max_packet_size_mb(desired_max_packet_size_mb);
	}

	const int desired_packets_per_poll_max = int(MULTIUSER_GET("blazium/multiuser_editor/limits/packets_per_poll_max", 256));
	if (desired_packets_per_poll_max != _cached_packets_per_poll_max) {
		_cached_packets_per_poll_max = desired_packets_per_poll_max;
		network.set_max_packets_per_poll(desired_packets_per_poll_max);
	}

	const int desired_max_clients = int(MULTIUSER_GET("blazium/multiuser_editor/network/max_clients", 32));
	if (desired_max_clients != _cached_max_clients) {
		_cached_max_clients = desired_max_clients;
		network.set_max_clients(desired_max_clients);
	}

	const int desired_crdt_atoms_max = int(MULTIUSER_GET("blazium/multiuser_editor/limits/crdt_live_atoms_max", 1000000));
	if (desired_crdt_atoms_max != _cached_crdt_atoms_max) {
		_cached_crdt_atoms_max = desired_crdt_atoms_max;
		script_sync.set_crdt_atoms_max_per_buffer(desired_crdt_atoms_max);
	}

	const int desired_script_attach_max_bytes = int(MULTIUSER_GET("blazium/multiuser_editor/limits/script_attach_max_bytes", 4 * 1024 * 1024));
	if (desired_script_attach_max_bytes != _cached_script_attach_max_bytes) {
		_cached_script_attach_max_bytes = desired_script_attach_max_bytes;
		script_sync.set_script_attach_max_bytes(desired_script_attach_max_bytes);
	}

	if (dock) {
		const int desired_security_events_max = int(MULTIUSER_GET("blazium/multiuser_editor/limits/security_events_max", 16));
		if (desired_security_events_max != _cached_security_events_max) {
			_cached_security_events_max = desired_security_events_max;
			dock->set_security_ring_max(desired_security_events_max);
		}
	}

	{
		const uint64_t now_msec = OS::get_singleton()->get_ticks_msec();
		if (_cached_jwt_jti_sweep_ms < 0) {
			_cached_jwt_jti_sweep_ms = MAX(1000, int(MULTIUSER_GET("blazium/multiuser_editor/intervals/jwt_jti_sweep_ms", 30000)));
		}
		if (now_msec - _jwt_last_jti_sweep_msec > (uint64_t)_cached_jwt_jti_sweep_ms) {
			_jwt_last_jti_sweep_msec = now_msec;
			JWTValidationConfig cfg = _read_jwt_validation_config();
			const uint64_t cutoff_msec = uint64_t(cfg.max_token_age_sec) * 2 * 1000;
			Vector<String> to_drop;
			for (const KeyValue<String, uint64_t> &E : _jwt_jti_cache) {
				if (now_msec > E.value && now_msec - E.value > cutoff_msec) {
					to_drop.push_back(E.key);
				}
			}
			for (const String &j : to_drop) {
				_jwt_jti_cache.erase(j);
				for (List<String>::Element *e = _jwt_jti_lru.front(); e; e = e->next()) {
					if (e->get() == j) {
						_jwt_jti_lru.erase(e);
						break;
					}
				}
			}
			if (to_drop.size() > 0) {
				_log_cat(LOG_DEBUG, LOG_PERMISSIONS, vformat("JWT JTI age prune: removed %d entries", to_drop.size()));
			}
		}
	}

	{
		const int polled_drops = network.consume_poll_truncated_count();
		const int send_drops = network.consume_send_truncated_count();
		if (polled_drops > 0 || send_drops > 0) {
			_bump_drop_counter(polled_drops + send_drops);
		}
	}
	Vector<MultiuserEditorNetwork::Packet> packets;
	network.poll(packets);
	for (const MultiuserEditorNetwork::Packet &packet : packets) {
		bool routable = true;

		if (packet.sender_net_id != 1) {
			const double rate = double(MULTIUSER_GET("blazium/multiuser_editor/limits/process_packets_per_sec", 500));
			if (rate > 0.0) {
				const uint64_t now_msec_in = OS::get_singleton()->get_ticks_msec();
				InboundBucket &bucket = _inbound_buckets[packet.sender_net_id];
				if (bucket.last_refill_msec == 0) {
					bucket.last_refill_msec = now_msec_in;
					bucket.tokens = MAX(1.0, rate);
				}
				const double elapsed_sec = double(now_msec_in - bucket.last_refill_msec) / 1000.0;
				bucket.tokens = MIN(MAX(1.0, rate), bucket.tokens + elapsed_sec * MAX(1.0, rate));
				bucket.last_refill_msec = now_msec_in;
				if (bucket.tokens >= 1.0) {
					bucket.tokens -= 1.0;
				} else {
					_log_cat(LOG_WARN, LOG_NETWORK, vformat("Inbound process throttled: net_id=%d rate=%.0f", packet.sender_net_id, rate));
					_record_security_event_kind(MultiuserEditorDock::KIND_THROTTLED, LOG_WARN, LOG_NETWORK, vformat("inbound throttled: %d", packet.sender_net_id));
					_bump_throttle_counter();
					routable = false;
				}
			}
		}

		if (routable && network.get_mode() == MultiuserEditorNetwork::MODE_HOST) {
			const String pkt_type = String(packet.action.get("type", ""));
			const String sender_role = network.get_peer_role(packet.sender_net_id);
			bool relay_allowed = true;
			if (!permissions.is_valid() || !permissions->is_known_action(pkt_type)) {
				const String dedupe_key = String::num_int64(packet.sender_net_id) + ":" + pkt_type;
				const uint64_t now_msec = OS::get_singleton()->get_ticks_msec();
				HashMap<String, uint64_t>::Iterator it = _unknown_relay_log_dedupe.find(dedupe_key);
				if (!it || now_msec - it->value > 1000) {
					_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("Host relay denied: unknown action type='%s' from net_id=%d", pkt_type, packet.sender_net_id));
					_record_security_event_kind(MultiuserEditorDock::KIND_PERMISSION_DENIED, LOG_WARN, LOG_PERMISSIONS, vformat("relay denied: unknown action='%s' sender=%d", pkt_type, packet.sender_net_id));
					_unknown_relay_log_dedupe[dedupe_key] = now_msec;
				}
				const int unknown_relay_dedupe_cap = MAX(16, int(MULTIUSER_GET("blazium/multiuser_editor/limits/unknown_relay_log_dedupe_max", 1024)));
				if (int(_unknown_relay_log_dedupe.size()) > unknown_relay_dedupe_cap) {
					_unknown_relay_log_dedupe.clear();
				}
				relay_allowed = false;
				routable = false;
			} else if (!sender_role.is_empty()) {
				if (!permissions->can_perform(pkt_type, sender_role)) {
					_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("Host relay denied: action='%s' sender_role='%s' net_id=%d", pkt_type, sender_role, packet.sender_net_id));
					_record_security_event_kind(MultiuserEditorDock::KIND_PERMISSION_DENIED, LOG_WARN, LOG_PERMISSIONS, vformat("relay denied: action='%s' role='%s' sender=%d", pkt_type, sender_role, packet.sender_net_id));
					relay_allowed = false;
					routable = false;
				} else if (permissions->is_host_only(pkt_type) && packet.sender_net_id != 1) {
					_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("Host relay denied: host-only action='%s' from net_id=%d", pkt_type, packet.sender_net_id));
					_record_security_event_kind(MultiuserEditorDock::KIND_PERMISSION_DENIED, LOG_WARN, LOG_PERMISSIONS, vformat("relay denied: host-only action='%s' sender=%d", pkt_type, packet.sender_net_id));
					relay_allowed = false;
					routable = false;
				}
			}
			if (relay_allowed && !MultiuserEditorFilesystemSync::should_skip_host_relay(packet.action)) {
				if (packet.sender_net_id == 1) {
					network.relay_packet(packet.sender_net_id, packet.raw_packet);
				} else {
					const double rate = double(MULTIUSER_GET("blazium/multiuser_editor/limits/relay_packets_per_sec", 200));
					const uint64_t now_msec_relay = OS::get_singleton()->get_ticks_msec();
					RelayBucket &bucket = _relay_buckets[packet.sender_net_id];
					if (bucket.last_refill_msec == 0) {
						bucket.last_refill_msec = now_msec_relay;
						bucket.tokens = MAX(1.0, rate);
					}
					const double elapsed_sec = double(now_msec_relay - bucket.last_refill_msec) / 1000.0;
					bucket.tokens = MIN(MAX(1.0, rate), bucket.tokens + elapsed_sec * MAX(1.0, rate));
					bucket.last_refill_msec = now_msec_relay;
					if (bucket.tokens >= 1.0) {
						bucket.tokens -= 1.0;
						network.relay_packet(packet.sender_net_id, packet.raw_packet);
					} else {
						_log_cat(LOG_WARN, LOG_NETWORK, vformat("Host relay throttled: net_id=%d type=%s rate=%.0f", packet.sender_net_id, pkt_type, rate));
						_record_security_event_kind(MultiuserEditorDock::KIND_THROTTLED, LOG_WARN, LOG_NETWORK, vformat("relay throttled: %d/%s", packet.sender_net_id, pkt_type));
						_bump_throttle_counter();
						routable = false;
					}
				}
			}
		}

		if (routable) {
			_route_action(packet.sender_net_id, packet.action);
		}
	}

	{
		const double now_sec = OS::get_singleton()->get_ticks_msec() / 1000.0;
		Vector<MultiuserEditorLockManager::EvictedLock> evicted = lock_manager.check_timeouts(now_sec);
		for (int i = 0; i < evicted.size(); i++) {
			const MultiuserEditorLockManager::EvictedLock &ev = evicted[i];
			_log_cat(LOG_INFO, LOG_PERMISSIONS, vformat("Lock evicted (TTL): peer=%s path=%s", ev.peer_id, ev.path));
			_record_security_event_kind(MultiuserEditorDock::KIND_LOCK_EVICTED, LOG_INFO, LOG_PERMISSIONS, vformat("lock TTL expired peer=%s path=%s", ev.peer_id, ev.path));
		}
	}
	lock_manager.refresh_overlay(get_tree() ? get_tree()->get_edited_scene_root() : nullptr);

	if (network.is_connected() && _hot_sync_scene_changes && !suppress_scene_events) {
		Vector<Dictionary> actions;
		action_interceptor.poll_scene_changes(actions);
		for (Dictionary action : actions) {
			_emit_action(action);
		}
	}

	double time = OS::get_singleton()->get_ticks_msec() / 1000.0;
	if (time - last_telemetry_broadcast > _hot_telemetry_interval) {
		last_telemetry_broadcast = time;
		if (network.is_connected()) {
			Dictionary act_data;
			act_data["fps"] = Engine::get_singleton()->get_frames_per_second();
			act_data["memory"] = OS::get_singleton()->get_static_memory_usage();

#ifdef MODULE_JUSTAMCP_ENABLED
			act_data["mcp_active"] = JustAMCPServer::get_singleton() && JustAMCPServer::get_singleton()->is_server_started();
#else
			act_data["mcp_active"] = false;
#endif

			act_data["role"] = local_role;
			_emit_action(_make_action(multiuser_editor::kActionTelemetry, act_data));
			if (dock) {
				dock->update_peer_telemetry(local_peer_id, act_data);
			}
		}
	}

	if (network.is_connected() && _hot_sync_scripts) {
		double now = OS::get_singleton()->get_ticks_msec() / 1000.0;
		if (script_sync.is_sync_pending_expired(now, double(MULTIUSER_GET("blazium/multiuser_editor/intervals/sync_pending_timeout_sec", MULTIUSER_SYNC_PENDING_TIMEOUT_SEC)))) {
			script_sync.set_sync_pending(false);
		}
		if (now - last_script_poll >= _hot_script_poll_interval) {
			last_script_poll = now;
			Vector<Dictionary> actions;
			CodeEdit *code_edit = _find_active_code_edit();
			String script_path = _find_script_path_for_code_edit(code_edit);
			script_sync.attach_code_edit(code_edit, script_path, actions);
			script_sync.poll_text_changes(actions);
			for (Dictionary action : actions) {
				_emit_action(action);
			}
			if (ghost_overlay && code_edit && _hot_show_remote_cursors) {
				ghost_overlay->attach_to(code_edit, script_path);
			}
		}
		if (_hot_cursor_sync_enabled && now - last_cursor_broadcast >= double(MULTIUSER_GET("blazium/multiuser_editor/intervals/cursor_sec", MULTIUSER_CURSOR_INTERVAL_SEC))) {
			last_cursor_broadcast = now;
			Dictionary cursor_action = script_sync.make_cursor_action();
			if (!cursor_action.is_empty()) {
				_emit_action(cursor_action);
			}
		} else if (!_hot_show_remote_cursors && ghost_overlay) {
			ghost_overlay->detach();
		}
	}

	bool is_playing = EditorInterface::get_singleton()->is_playing_scene();
	if (is_playing != was_playing) {
		was_playing = is_playing;
		if (network.get_mode() == MultiuserEditorNetwork::MODE_HOST && bool(MULTIUSER_GET("blazium/multiuser_editor/enable_team_play", true))) {
			if (is_playing) {
				_emit_action(_make_action(multiuser_editor::kActionTeamPlayStart, Dictionary()));
			} else {
				_emit_action(_make_action(multiuser_editor::kActionTeamPlayStop, Dictionary()));
			}
		}
	}
}

void MultiuserEditorPlugin::_update_context() {
	action_interceptor.set_context(get_tree() ? get_tree()->get_edited_scene_root() : nullptr, selection, undo_redo, &lock_manager);
}

void MultiuserEditorPlugin::_update_ui() {
	if (dock) {
		if (network.get_mode() == MultiuserEditorNetwork::MODE_HOST) {
			dock->set_connected(TTR("Hosting"));
			const bool fp_enabled = bool(MULTIUSER_GET("blazium/multiuser_editor/allow_editor_force_push", false));
			dock->set_git_panel_enabled(true, local_role, fp_enabled);
		} else if (network.get_mode() == MultiuserEditorNetwork::MODE_JOIN) {
			dock->set_connected(TTR("Joined"));
			const bool fp_enabled = bool(MULTIUSER_GET("blazium/multiuser_editor/allow_editor_force_push", false));
			dock->set_git_panel_enabled(true, local_role, fp_enabled);
		} else {
			dock->set_disconnected();
			dock->set_git_panel_enabled(false, local_role, false);
		}
		dock->update_info(local_peer_id);
	}

	const bool now_connected = network.is_connected();
	if (now_connected != _last_session_connected_for_inspector || local_role != _last_role_for_inspector) {
		_last_session_connected_for_inspector = now_connected;
		_last_role_for_inspector = local_role;
		_refresh_settings_inspector();
	}
	if (status_label) {
		status_label->set_visible(network.get_mode() != MultiuserEditorNetwork::MODE_OFF);
	}
}

void MultiuserEditorPlugin::_setup_dock() {
	dock = memnew(MultiuserEditorDock);
	add_control_to_dock(DOCK_SLOT_RIGHT_UL, dock);
	dock->set_session_branch_default(String(MULTIUSER_GET("blazium/multiuser_editor/git_session_branch_name", "multiuser_session_{timestamp}")));
	dock->set_merge_target_default(String(MULTIUSER_GET("blazium/multiuser_editor/git_merge_target_branch", "main")));
	dock->set_chat_history_max(MAX(16, int(MULTIUSER_GET("blazium/multiuser_editor/limits/chat_history_max", 256))));
	last_enabled_state = bool(MULTIUSER_GET("blazium/multiuser_editor/enabled", false));
	dock->set_module_enabled(last_enabled_state);
}

void MultiuserEditorPlugin::_teardown_dock() {
	if (dock) {
		remove_control_from_docks(dock);
		dock->queue_free();
		dock = nullptr;
	}
}

void MultiuserEditorPlugin::_setup_status_indicator() {
	status_label = memnew(Label);
	status_label->set_text(TTR("Multiuser Editor"));
	status_label->add_theme_color_override("font_color", Color(0.3, 1.0, 0.3));
	status_label->hide();
	add_control_to_container(CONTAINER_TOOLBAR, status_label);
}

void MultiuserEditorPlugin::_connect_network_signals() {
	Ref<MultiplayerPeer> peer = network.get_peer();
	if (peer.is_null()) {
		return;
	}
	Callable connected = callable_mp(this, &MultiuserEditorPlugin::_on_network_peer_connected);
	Callable disconnected = callable_mp(this, &MultiuserEditorPlugin::_on_network_peer_disconnected);
	if (!peer->is_connected("peer_connected", connected)) {
		peer->connect("peer_connected", connected);
	}
	if (!peer->is_connected("peer_disconnected", disconnected)) {
		peer->connect("peer_disconnected", disconnected);
	}
}

void MultiuserEditorPlugin::_disconnect_network_signals() {
	Ref<MultiplayerPeer> peer = network.get_peer();
	if (peer.is_null()) {
		return;
	}
	Callable connected = callable_mp(this, &MultiuserEditorPlugin::_on_network_peer_connected);
	Callable disconnected = callable_mp(this, &MultiuserEditorPlugin::_on_network_peer_disconnected);
	if (peer->is_connected("peer_connected", connected)) {
		peer->disconnect("peer_connected", connected);
	}
	if (peer->is_connected("peer_disconnected", disconnected)) {
		peer->disconnect("peer_disconnected", disconnected);
	}
}

static const char *_mu_level_label(MultiuserEditorPlugin::LogLevel p_level) {
	switch (p_level) {
		case MultiuserEditorPlugin::LOG_ERROR:
			return "ERR";
		case MultiuserEditorPlugin::LOG_WARN:
			return "WRN";
		case MultiuserEditorPlugin::LOG_INFO:
			return "INF";
		case MultiuserEditorPlugin::LOG_DEBUG:
			return "DBG";
	}
	return "INF";
}

static const char *_mu_cat_label(MultiuserEditorPlugin::LogCategory p_cat) {
	switch (p_cat) {
		case MultiuserEditorPlugin::LOG_GENERAL:
			return "general";
		case MultiuserEditorPlugin::LOG_REPLICATION:
			return "replication";
		case MultiuserEditorPlugin::LOG_FILESYSTEM:
			return "filesystem";
		case MultiuserEditorPlugin::LOG_CRDT:
			return "crdt";
		case MultiuserEditorPlugin::LOG_NETWORK:
			return "network";
		case MultiuserEditorPlugin::LOG_PERMISSIONS:
			return "permissions";
	}
	return "general";
}

static int _mu_level_from_string(const String &p_value) {
	const String v = p_value.strip_edges().to_lower();
	if (v == "error" || v == "err") {
		return MultiuserEditorPlugin::LOG_ERROR;
	}
	if (v == "warn" || v == "warning") {
		return MultiuserEditorPlugin::LOG_WARN;
	}
	if (v == "debug" || v == "dbg" || v == "trace") {
		return MultiuserEditorPlugin::LOG_DEBUG;
	}
	return MultiuserEditorPlugin::LOG_INFO;
}

void MultiuserEditorPlugin::_log(const String &p_message) const {
	_log_cat(LOG_INFO, LOG_GENERAL, p_message);
}

void MultiuserEditorPlugin::_log_connection(const String &p_msg) const {
	if (bool(MULTIUSER_GET("blazium/multiuser_editor/server_log_connections", true))) {
		print_line(vformat("[Multiuser Connect] %s", p_msg));
	}
}

void MultiuserEditorPlugin::_log_cat(LogLevel p_level, LogCategory p_cat, const String &p_msg) const {
	const bool legacy_debug = bool(MULTIUSER_GET("blazium/multiuser_editor/enable_debug_logging", false));
	int max_level = _mu_level_from_string(String(MULTIUSER_GET("blazium/multiuser_editor/logging/log_level", "info")));
	if (legacy_debug && max_level < int(LOG_DEBUG)) {
		max_level = LOG_DEBUG;
	}
	if (int(p_level) > max_level) {
		return;
	}

	bool category_enabled = true;
	switch (p_cat) {
		case LOG_GENERAL:
			category_enabled = true;
			break;
		case LOG_REPLICATION:
			category_enabled = legacy_debug || bool(MULTIUSER_GET("blazium/multiuser_editor/logging/log_replication", false));
			break;
		case LOG_FILESYSTEM:
			category_enabled = legacy_debug || bool(MULTIUSER_GET("blazium/multiuser_editor/logging/log_filesystem", false));
			break;
		case LOG_CRDT:
			category_enabled = legacy_debug || bool(MULTIUSER_GET("blazium/multiuser_editor/logging/log_crdt", false));
			break;
		case LOG_NETWORK:
			category_enabled = legacy_debug || bool(MULTIUSER_GET("blazium/multiuser_editor/logging/log_network", false));
			break;
		case LOG_PERMISSIONS:
			category_enabled = bool(MULTIUSER_GET("blazium/multiuser_editor/logging/log_permissions", true));
			break;
	}
	if (!category_enabled && p_level >= LOG_INFO) {
		return;
	}

	const double ts = OS::get_singleton() ? double(OS::get_singleton()->get_ticks_msec()) / 1000.0 : 0.0;
	const String ts_str = String::num(ts, 3);
	const String header = vformat("[MultiuserEditor][%s][%s][peer=%s role=%s][%ss]",
			_mu_cat_label(p_cat), _mu_level_label(p_level), local_peer_id, local_role, ts_str);

	const String line = header + " " + p_msg;
	if (p_level == LOG_ERROR) {
		print_error(line);
	} else if (p_level == LOG_WARN) {
		print_verbose(line);
	} else {
		print_line(line);
	}
}

void MultiuserEditorPlugin::_record_security_event(LogLevel p_level, LogCategory p_cat, const String &p_msg) const {
	if (dock) {
		dock->record_security_event(int(p_level), int(p_cat), p_msg);
	}
}

void MultiuserEditorPlugin::_record_security_event_kind(int p_kind, LogLevel p_level, LogCategory p_cat, const String &p_msg) const {
	if (dock) {
		dock->record_security_event_kind(p_kind, int(p_level), int(p_cat), p_msg);
	}
}

void MultiuserEditorPlugin::_security_sink_thunk(void *p_user, int p_kind, int p_level, int p_category, const String &p_message) {
	MultiuserEditorPlugin *plugin = static_cast<MultiuserEditorPlugin *>(p_user);
	if (!plugin) {
		return;
	}
	const LogLevel level = LogLevel(CLAMP(p_level, int(LOG_ERROR), int(LOG_DEBUG)));
	const LogCategory category = LogCategory(CLAMP(p_category, int(LOG_GENERAL), int(LOG_PERMISSIONS)));
	plugin->_log_cat(level, category, p_message);
	plugin->_record_security_event_kind(p_kind, level, category, p_message);
}

void MultiuserEditorPlugin::_bump_drop_counter(int p_by) const {
	if (dock) {
		dock->bump_drop_counter(p_by);
	}
}

void MultiuserEditorPlugin::_bump_throttle_counter(int p_by) const {
	if (dock) {
		dock->bump_throttle_counter(p_by);
	}
}

bool MultiuserEditorPlugin::_const_time_eq(const String &p_a, const String &p_b) {
	const CharString a = p_a.utf8();
	const CharString b = p_b.utf8();
	const int la = a.length();
	const int lb = b.length();
	const int n = MAX(la, lb);
	uint32_t diff = uint32_t(la ^ lb);
	for (int i = 0; i < n; i++) {
		const uint8_t ca = i < la ? uint8_t(a[i]) : 0;
		const uint8_t cb = i < lb ? uint8_t(b[i]) : 0;
		diff |= uint32_t(ca ^ cb);
	}
	return diff == 0;
}

bool MultiuserEditorPlugin::_is_safe_simple_value(const Variant &p_value, int p_remaining_depth) const {
	if (p_remaining_depth <= 0) {
		return false;
	}

	const int kStringCharCap = multiuser_editor::kSimpleValueStringMax;
	const int kPackedByteCap = multiuser_editor::kSimpleValuePackedByteMax;
	const int kPackedElemCap = multiuser_editor::kSimpleValuePackedElemMax;
	switch (p_value.get_type()) {
		case Variant::NIL:
		case Variant::OBJECT:
		case Variant::RID:
		case Variant::SIGNAL:
		case Variant::CALLABLE:
			return false;

		case Variant::BOOL:
		case Variant::INT:
		case Variant::FLOAT:
		case Variant::VECTOR2:
		case Variant::VECTOR2I:
		case Variant::RECT2:
		case Variant::RECT2I:
		case Variant::VECTOR3:
		case Variant::VECTOR3I:
		case Variant::VECTOR4:
		case Variant::VECTOR4I:
		case Variant::TRANSFORM2D:
		case Variant::TRANSFORM3D:
		case Variant::PLANE:
		case Variant::QUATERNION:
		case Variant::AABB:
		case Variant::BASIS:
		case Variant::PROJECTION:
		case Variant::COLOR:
			return true;

		case Variant::STRING:
		case Variant::STRING_NAME:
		case Variant::NODE_PATH: {
			const String s = String(p_value);
			return s.length() <= kStringCharCap;
		}

		case Variant::PACKED_BYTE_ARRAY: {
			const PackedByteArray a = p_value;
			return a.size() <= kPackedByteCap;
		}
		case Variant::PACKED_INT32_ARRAY: {
			const PackedInt32Array a = p_value;
			return a.size() <= kPackedElemCap;
		}
		case Variant::PACKED_INT64_ARRAY: {
			const PackedInt64Array a = p_value;
			return a.size() <= kPackedElemCap;
		}
		case Variant::PACKED_FLOAT32_ARRAY: {
			const PackedFloat32Array a = p_value;
			return a.size() <= kPackedElemCap;
		}
		case Variant::PACKED_FLOAT64_ARRAY: {
			const PackedFloat64Array a = p_value;
			return a.size() <= kPackedElemCap;
		}
		case Variant::PACKED_STRING_ARRAY: {
			const PackedStringArray a = p_value;
			if (a.size() > kPackedElemCap) {
				return false;
			}
			for (int i = 0; i < a.size(); i++) {
				if (a[i].length() > kStringCharCap) {
					return false;
				}
			}
			return true;
		}

		case Variant::PACKED_VECTOR2_ARRAY: {
			const PackedVector2Array a = p_value;
			return a.size() <= kPackedElemCap;
		}
		case Variant::PACKED_VECTOR3_ARRAY: {
			const PackedVector3Array a = p_value;
			return a.size() <= kPackedElemCap;
		}
		case Variant::PACKED_VECTOR4_ARRAY: {
			const PackedVector4Array a = p_value;
			return a.size() <= kPackedElemCap;
		}
		case Variant::PACKED_COLOR_ARRAY: {
			const PackedColorArray a = p_value;
			return a.size() <= kPackedElemCap;
		}

		case Variant::DICTIONARY: {
			const Dictionary &d = (const Dictionary &)p_value;
			if (d.size() > multiuser_editor::kSimpleValueDictMaxKeys) {
				return false;
			}
			Array keys = d.keys();
			for (int i = 0; i < keys.size(); i++) {
				if (!_is_safe_simple_value(d[keys[i]], p_remaining_depth - 1)) {
					return false;
				}
			}
			return true;
		}
		case Variant::ARRAY: {
			const Array &a = (const Array &)p_value;
			if (a.size() > multiuser_editor::kSimpleValueArrayMaxLen) {
				return false;
			}
			for (int i = 0; i < a.size(); i++) {
				if (!_is_safe_simple_value(a[i], p_remaining_depth - 1)) {
					return false;
				}
			}
			return true;
		}
		default:

			return false;
	}
}

bool MultiuserEditorPlugin::_packet_is_sane(const Dictionary &p_action, String *r_reason) const {
	if (p_action.is_empty()) {
		if (r_reason) {
			*r_reason = "empty";
		}
		return false;
	}
	if (p_action.size() > multiuser_editor::kPacketTopLevelKeysMax) {
		if (r_reason) {
			*r_reason = "too_many_keys";
		}
		return false;
	}
	if (!p_action.has("type")) {
		if (r_reason) {
			*r_reason = "missing_type";
		}
		return false;
	}
	const Variant tv = p_action["type"];
	if (tv.get_type() != Variant::STRING && tv.get_type() != Variant::STRING_NAME) {
		if (r_reason) {
			*r_reason = "type_not_string";
		}
		return false;
	}
	const String type = String(tv);
	if (type.is_empty() || type.length() > multiuser_editor::kPacketTypeStringMax) {
		if (r_reason) {
			*r_reason = "bad_type_len";
		}
		return false;
	}
	for (int i = 0; i < type.length(); i++) {
		const char32_t c = type[i];
		const bool ok = (c >= 'a' && c <= 'z') || c == '_';
		if (!ok) {
			if (r_reason) {
				*r_reason = "bad_type_chars";
			}
			return false;
		}
	}
	if (p_action.has("data")) {
		const Variant dv = p_action["data"];
		if (dv.get_type() != Variant::DICTIONARY) {
			if (r_reason) {
				*r_reason = "data_not_dict";
			}
			return false;
		}

		if (!_is_safe_simple_value(dv, multiuser_editor::kPacketRecursionDepthMax)) {
			if (r_reason) {
				*r_reason = "data_depth_or_size";
			}
			return false;
		}
	}
	return true;
}

String MultiuserEditorPlugin::_format_action_summary(const Dictionary &p_action) const {
	String type = String(p_action.get("type", ""));
	String origin = String(p_action.get("peer_id", ""));
	int data_size = 0;
	if (p_action.has("data")) {
		const Variant d = p_action["data"];
		if (d.get_type() == Variant::DICTIONARY) {
			data_size = Dictionary(d).size();
		}
	}
	return vformat("type=%s origin=%s data_keys=%d", type, origin, data_size);
}

bool MultiuserEditorPlugin::_is_access_list_path(const String &p_path) const {
	if (p_path.is_empty() || _cached_access_list_canonical_path.is_empty()) {
		return false;
	}
	return MultiuserEditorAccessList::path_equals(p_path, _cached_access_list_canonical_path);
}

String MultiuserEditorPlugin::_resolve_access_list_path() const {
	String path = String(MULTIUSER_GET("blazium/multiuser_editor/access_list/path", MultiuserEditorAccessList::default_path())).strip_edges();
	if (path.is_empty()) {
		path = MultiuserEditorAccessList::default_path();
	}
	return path;
}

String MultiuserEditorPlugin::_resolve_implicit_role_for_dock_password() const {
	String role = String(MULTIUSER_GET("blazium/multiuser_editor/access_list/implicit_default_role", multiuser_editor::kRoleEditor));
	if (!MultiuserEditorAccessList::is_valid_role(role)) {
		role = multiuser_editor::kRoleEditor;
	}
	return role;
}

void MultiuserEditorPlugin::reload_access_list() {
	_reload_access_list();
}

void MultiuserEditorPlugin::_reload_access_list() {
	if (_access_list.is_null()) {
		_access_list.instantiate();
	}
	_access_list->set_max_entries(int(MULTIUSER_GET("blazium/multiuser_editor/access_list/max_entries", 256)));
	const bool enabled = bool(MULTIUSER_GET("blazium/multiuser_editor/access_list/enabled", true));
	if (!enabled) {
		_access_list->clear();
	} else {
		const String path = _resolve_access_list_path();
		const Error le = _access_list->load_from_file(path);
		if (le != OK) {
			_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("AccessList load_from_file('%s') failed: error=%d", path, int(le)));
		}
	}
	_refresh_access_list_protection();
}

void MultiuserEditorPlugin::_refresh_access_list_protection() {
	const String path = _resolve_access_list_path();
	_cached_access_list_canonical_path = MultiuserEditorAccessList::canonicalize_path(path);

	PackedStringArray protect;
	if (!_cached_access_list_canonical_path.is_empty()) {
		protect.push_back(_cached_access_list_canonical_path);
	}
	filesystem_sync.set_protected_paths(protect);

	const bool auto_gitignore = bool(MULTIUSER_GET("blazium/multiuser_editor/access_list/auto_gitignore", true));
	if (auto_gitignore && !_cached_access_list_canonical_path.is_empty() && _cached_access_list_canonical_path.begins_with("res://")) {
		const Error ge = MultiuserEditorAccessList::ensure_in_gitignore(_cached_access_list_canonical_path, "res://");
		if (ge != OK) {
			_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("AccessList ensure_in_gitignore failed: error=%d", int(ge)));
		}
	}
}

Vector<MultiuserEditorPlugin::SecurityEventSnapshot> MultiuserEditorPlugin::get_recent_security_events_snapshot(int p_max) const {
	Vector<SecurityEventSnapshot> out;
	if (!dock) {
		return out;
	}

	const int requested = MAX(0, p_max);
	if (requested == 0) {
		return out;
	}
	const int count = MIN(requested, dock->get_security_event_count());
	if (count == 0) {
		return out;
	}
	dock->snapshot_recent_security_events(count, out);
	return out;
}

Array MultiuserEditorPlugin::get_recent_security_events_snapshot_array(int p_max) const {
	Array result;
	const Vector<SecurityEventSnapshot> events = get_recent_security_events_snapshot(p_max);
	result.resize(events.size());
	for (int i = 0; i < events.size(); i++) {
		Dictionary d;
		d["when_msec"] = uint64_t(events[i].when_msec);
		d["severity"] = events[i].severity;
		d["level"] = events[i].severity;
		d["category"] = events[i].category;
		d["kind"] = events[i].kind;
		d["message"] = events[i].message;
		result[i] = d;
	}
	return result;
}

Dictionary MultiuserEditorPlugin::validate_jwt_static_d(const String &p_jwt, const String &p_secret, const Dictionary &p_cfg) {
	JWTValidationConfig cfg;
	if (p_cfg.has("algorithms_csv")) {
		cfg.algorithms_csv = String(p_cfg["algorithms_csv"]);
	}
	if (p_cfg.has("expected_audience")) {
		cfg.expected_audience = String(p_cfg["expected_audience"]);
	}
	if (p_cfg.has("expected_issuer")) {
		cfg.expected_issuer = String(p_cfg["expected_issuer"]);
	}
	if (p_cfg.has("leeway_sec")) {
		cfg.leeway_sec = double(p_cfg["leeway_sec"]);
	}
	if (p_cfg.has("max_token_age_sec")) {
		cfg.max_token_age_sec = int(p_cfg["max_token_age_sec"]);
	}
	if (p_cfg.has("require_jti")) {
		cfg.require_jti = bool(p_cfg["require_jti"]);
	}
	if (p_cfg.has("jti_cache_max")) {
		cfg.jti_cache_max = int(p_cfg["jti_cache_max"]);
	}

	const JWTValidationResult res = validate_jwt_static(p_jwt, p_secret, cfg);
	Dictionary out;
	out["valid"] = res.valid;
	out["reason"] = res.reason;
	out["role"] = res.role;
	out["jti"] = res.jti;
	return out;
}

String MultiuserEditorPlugin::_extract_target_path_for_protection(const String &p_type, const Dictionary &p_action, const Dictionary &p_data, const String &p_field) const {
	if (p_type == multiuser_editor::kActionFsOp) {
		if (p_field == "src" || p_field == "old_path") {
			return String(p_data.get("old_path", p_data.get("src", "")));
		}
		if (p_field == "dst" || p_field == "new_path" || p_field == "path") {
			return String(p_data.get("new_path", p_data.get("path", p_data.get("dst", ""))));
		}
	}
	if (p_type == multiuser_editor::kActionFsMove) {
		if (p_field == "old_path") {
			return String(p_data.get("old_path", ""));
		}
		if (p_field == "new_path") {
			return String(p_data.get("new_path", ""));
		}
	}
	if (p_type == multiuser_editor::kActionFsRemove) {
		if (p_field == "path") {
			return String(p_data.get("path", ""));
		}
	}
	if (p_type == multiuser_editor::kActionFileProposeBegin || p_type == multiuser_editor::kActionFileProposeChunk || p_type == multiuser_editor::kActionFileProposeEnd ||
			p_type == multiuser_editor::kActionFileApplyBegin || p_type == multiuser_editor::kActionFileApplyChunk || p_type == multiuser_editor::kActionFileApplyEnd) {
		if (p_field == "path") {
			return String(p_data.get("path", ""));
		}
		if (p_field == "old_path") {
			return String(p_data.get("old_path", ""));
		}
	}
	if (p_type == multiuser_editor::kActionFileProposeDelete || p_type == multiuser_editor::kActionFileApplyDelete) {
		if (p_field == "path") {
			return String(p_data.get("path", ""));
		}
	}
	if (p_type == multiuser_editor::kActionFileProposeMove || p_type == multiuser_editor::kActionFileApplyMove) {
		if (p_field == "old_path") {
			return String(p_data.get("old_path", ""));
		}
		if (p_field == "new_path") {
			return String(p_data.get("new_path", ""));
		}
	}
	if (p_type == multiuser_editor::kActionResourceSync) {
		if (p_field == "path") {
			return String(p_data.get("path", ""));
		}
	}
	if (p_type == multiuser_editor::kActionScriptAttach || p_type == multiuser_editor::kActionCrdt || p_type == multiuser_editor::kActionCrdtSync) {
		if (p_field == "script_path") {
			return String(p_data.get("script_path", ""));
		}

		if (p_field == "node_path") {
			const String np = String(p_action.get("node_path", ""));
			if (!np.is_empty()) {
				return np;
			}
			return String(p_data.get("script", ""));
		}
	}
	if (p_type == multiuser_editor::kActionSceneSync) {
		if (p_field == "filepath") {
			return String(p_data.get("filepath", ""));
		}
	}
	return String();
}

bool MultiuserEditorPlugin::_is_path_protected_for_action(const String &p_path) const {
	if (p_path.is_empty()) {
		return false;
	}
	if (_is_access_list_path(p_path)) {
		return true;
	}
	if (filesystem_sync.is_path_protected(p_path)) {
		return true;
	}

	const String canon = MultiuserEditorAccessList::canonicalize_path(p_path);
	if (canon.is_empty()) {
		_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("J.2 path protection: canonicalize rejected non-empty input '%s'; treating as protected.", p_path));
		return true;
	}
	if (canon != p_path) {
		if (_is_access_list_path(canon)) {
			return true;
		}
		if (filesystem_sync.is_path_protected(canon)) {
			return true;
		}
	}
	return false;
}

bool MultiuserEditorPlugin::_is_inside_loaded_project(String *r_reason) const {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	if (!ps) {
		if (r_reason) {
			*r_reason = "ProjectSettings singleton unavailable";
		}
		return false;
	}
	if (!ps->is_project_loaded()) {
		if (r_reason) {
			*r_reason = "no project loaded (ProjectSettings::is_project_loaded() == false)";
		}
		return false;
	}
	if (ps->get_resource_path().is_empty()) {
		if (r_reason) {
			*r_reason = "project resource path is empty";
		}
		return false;
	}
	if (!FileAccess::exists("res://project.godot") && !FileAccess::exists("res://project.binary")) {
		if (r_reason) {
			*r_reason = "no project.godot or project.binary at res://";
		}
		return false;
	}
	const String data_dir = ps->globalize_path(ps->get_project_data_path());
	if (!data_dir.is_empty() && !DirAccess::dir_exists_absolute(data_dir)) {
		if (r_reason) {
			*r_reason = "project data folder missing: " + data_dir;
		}
		return false;
	}
	return true;
}

bool MultiuserEditorPlugin::_is_valid_peer_id_field(const String &p_value) const {
	if (p_value.is_empty() || p_value.length() > multiuser_editor::kPeerIdMax) {
		return false;
	}
	for (int i = 0; i < p_value.length(); i++) {
		const char32_t c = p_value[i];
		const bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '-';
		if (!ok) {
			return false;
		}
	}
	return true;
}

bool MultiuserEditorPlugin::_is_valid_role_field(const String &p_value) const {
	if (p_value.is_empty() || p_value.length() > multiuser_editor::kRoleFieldMax) {
		return false;
	}
	return MultiuserEditorPermissions::role_from_string(p_value) != MultiuserEditorPermissions::ROLE_NONE;
}

bool MultiuserEditorPlugin::_is_valid_chat_message(const String &p_value) const {
	if (p_value.is_empty() || p_value.length() > multiuser_editor::kChatMessageMax) {
		return false;
	}
	for (int i = 0; i < p_value.length(); i++) {
		const char32_t c = p_value[i];
		if (c == '\0' || c == '\r') {
			return false;
		}
		if (c < 0x20 && c != '\n' && c != '\t') {
			return false;
		}
	}
	return true;
}

bool MultiuserEditorPlugin::_is_valid_hex_token(const String &p_value, int p_expect_len) const {
	if (p_value.length() != p_expect_len) {
		return false;
	}
	for (int i = 0; i < p_value.length(); i++) {
		const char32_t c = p_value[i];
		const bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
		if (!ok) {
			return false;
		}
	}
	return true;
}

bool MultiuserEditorPlugin::_is_valid_jwt_token(const String &p_value) const {
	if (p_value.length() < multiuser_editor::kJWTLengthMin || p_value.length() > multiuser_editor::kJWTLengthMax) {
		return false;
	}
	int dot_count = 0;
	for (int i = 0; i < p_value.length(); i++) {
		const char32_t c = p_value[i];
		if (c == '.') {
			dot_count++;
			continue;
		}
		const bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c == '_';
		if (!ok) {
			return false;
		}
	}
	return dot_count == 2;
}

String MultiuserEditorPlugin::_jwt_jti_fingerprint(const String &p_jti) const {
	if (p_jti.is_empty()) {
		return String("?");
	}
	const String h = p_jti.sha256_text();
	return h.substr(0, multiuser_editor::kJTIFingerprintHexLength);
}

bool MultiuserEditorPlugin::_jwt_jti_seen(const String &p_jti) const {
	return _jwt_jti_cache.has(p_jti);
}

void MultiuserEditorPlugin::_jwt_remember_jti(const String &p_jti) {
	if (p_jti.is_empty()) {
		return;
	}
	_jwt_jti_cache[p_jti] = OS::get_singleton() ? OS::get_singleton()->get_ticks_msec() : 0;
	_jwt_jti_lru.push_back(p_jti);
	_jwt_jti_evict_if_needed();
}

void MultiuserEditorPlugin::_jwt_jti_evict_if_needed() {
	const int cap = MAX(16, int(MULTIUSER_GET("blazium/multiuser_editor/jwt/jti_cache_max", 4096)));
	while (_jwt_jti_cache.size() > (uint32_t)cap && !_jwt_jti_lru.is_empty()) {
		const String oldest = _jwt_jti_lru.front()->get();
		_jwt_jti_lru.pop_front();
		_jwt_jti_cache.erase(oldest);
	}
}

MultiuserEditorPlugin::JWTValidationConfig MultiuserEditorPlugin::_read_jwt_validation_config() const {
	JWTValidationConfig cfg;
	cfg.algorithms_csv = String(MULTIUSER_GET("blazium/multiuser_editor/jwt/algorithms_allowed", "HS256"));
	cfg.expected_audience = String(MULTIUSER_GET("blazium/multiuser_editor/jwt/expected_audience", ""));
	cfg.expected_issuer = String(MULTIUSER_GET("blazium/multiuser_editor/jwt/expected_issuer", ""));
	cfg.leeway_sec = MAX(0.0, double(MULTIUSER_GET("blazium/multiuser_editor/jwt/leeway_sec", 30)));
	cfg.max_token_age_sec = int(MULTIUSER_GET("blazium/multiuser_editor/jwt/max_token_age_sec", 3600));
	cfg.require_jti = bool(MULTIUSER_GET("blazium/multiuser_editor/jwt/require_jti", false));
	cfg.jti_cache_max = MAX(16, int(MULTIUSER_GET("blazium/multiuser_editor/jwt/jti_cache_max", 4096)));
	return cfg;
}

MultiuserEditorPlugin::JWTValidationResult MultiuserEditorPlugin::validate_jwt_static(const String &p_jwt, const String &p_secret, const JWTValidationConfig &p_cfg) {
	JWTValidationResult res;

	if (p_jwt.length() < multiuser_editor::kJWTLengthMin || p_jwt.length() > multiuser_editor::kJWTLengthMax) {
		res.reason = "bad_format";
		return res;
	}
	int dot_count = 0;
	for (int i = 0; i < p_jwt.length(); i++) {
		const char32_t c = p_jwt[i];
		if (c == '.') {
			dot_count++;
			continue;
		}
		const bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c == '_';
		if (!ok) {
			res.reason = "bad_format";
			return res;
		}
	}
	if (dot_count != 2) {
		res.reason = "bad_format";
		return res;
	}

	JWT *jwt_singleton = JWT::get_singleton();
	if (!jwt_singleton) {
		res.reason = "no_jwt_singleton";
		return res;
	}

	const String alg = jwt_singleton->get_algorithm(p_jwt);
	bool alg_ok = false;
	{
		Vector<String> parts = p_cfg.algorithms_csv.split(",", false);
		for (int i = 0; i < parts.size(); i++) {
			const String p = parts[i].strip_edges();
			if (!p.is_empty() && p == alg) {
				alg_ok = true;
				break;
			}
		}
	}
	if (!alg_ok) {
		res.reason = "bad_alg:" + alg;
		return res;
	}

	if (alg == "HS256") {
		if (!jwt_singleton->validate_signature_hs256(p_jwt, p_secret)) {
			res.reason = "bad_sig";
			return res;
		}
	} else {
		if (!jwt_singleton->validate(p_jwt, p_secret)) {
			res.reason = "bad_sig";
			return res;
		}
	}

	const double leeway = MAX(0.0, p_cfg.leeway_sec);
	if (!jwt_singleton->validate_timing(p_jwt, leeway)) {
		Dictionary payload = jwt_singleton->get_payload(p_jwt);
		const double now = OS::get_singleton() ? OS::get_singleton()->get_unix_time() : 0.0;
		if (payload.has("nbf") && double(payload["nbf"]) > now + leeway) {
			res.reason = "not_yet_valid";
		} else if (payload.has("exp") && double(payload["exp"]) + leeway < now) {
			res.reason = "expired";
		} else {
			res.reason = "bad_timing";
		}
		return res;
	}

	Dictionary payload = jwt_singleton->get_payload(p_jwt);
	const double now = OS::get_singleton() ? OS::get_singleton()->get_unix_time() : 0.0;

	if (!payload.has("iat")) {
		res.reason = "missing_iat";
		return res;
	}
	const double iat = double(payload["iat"]);
	if (iat - leeway > now) {
		res.reason = "future_iat";
		return res;
	}
	if (p_cfg.max_token_age_sec > 0 && (now - iat) > (double(p_cfg.max_token_age_sec) + leeway)) {
		res.reason = "too_old";
		return res;
	}

	if (!p_cfg.expected_audience.is_empty()) {
		if (!payload.has("aud")) {
			res.reason = "bad_aud:missing";
			return res;
		}
		const Variant aud_var = payload["aud"];
		bool match = false;
		if (aud_var.get_type() == Variant::STRING) {
			match = (String(aud_var) == p_cfg.expected_audience);
		} else if (aud_var.get_type() == Variant::ARRAY) {
			Array aud_arr = aud_var;
			for (int i = 0; i < aud_arr.size(); i++) {
				if (String(aud_arr[i]) == p_cfg.expected_audience) {
					match = true;
					break;
				}
			}
		}
		if (!match) {
			res.reason = "bad_aud";
			return res;
		}
	}

	if (!p_cfg.expected_issuer.is_empty()) {
		if (!payload.has("iss")) {
			res.reason = "bad_iss:missing";
			return res;
		}
		if (String(payload["iss"]) != p_cfg.expected_issuer) {
			res.reason = "bad_iss";
			return res;
		}
	}

	String jti;
	if (payload.has("jti")) {
		jti = String(payload["jti"]);
		if (jti.length() > multiuser_editor::kJWTJTIMax) {
			res.reason = "bad_jti:too_long";
			return res;
		}
	}
	if (p_cfg.require_jti && jti.is_empty()) {
		res.reason = "missing_jti";
		return res;
	}

	if (!payload.has("role")) {
		res.reason = "missing_role";
		return res;
	}
	const String role = String(payload["role"]);
	if (!(role == multiuser_editor::kRoleViewer || role == multiuser_editor::kRoleEditor || role == multiuser_editor::kRoleAdmin)) {
		res.reason = "bad_role";
		return res;
	}
	res.role = role;
	res.jti = jti;
	res.valid = true;
	return res;
}

MultiuserEditorPlugin::JWTValidationResult MultiuserEditorPlugin::_validate_jwt_full(const String &p_jwt, const String &p_secret) const {
	const JWTValidationConfig cfg = _read_jwt_validation_config();
	JWTValidationResult res = validate_jwt_static(p_jwt, p_secret, cfg);
	if (!res.valid) {
		return res;
	}

	if (!res.jti.is_empty()) {
		if (_jwt_jti_seen(res.jti)) {
			res.valid = false;
			res.reason = "replayed";
			return res;
		}
		const_cast<MultiuserEditorPlugin *>(this)->_jwt_remember_jti(res.jti);
	}
	return res;
}

void MultiuserEditorPlugin::_refresh_permissions_from_settings() {
	if (permissions.is_null()) {
		permissions.instantiate();
	}
	permissions->load_defaults();

	permissions->set_allow_widen_host_only(bool(MULTIUSER_GET("blazium/multiuser_editor/permissions/allow_widen_host_only", false)));
	const String overrides = String(MULTIUSER_GET("blazium/multiuser_editor/permissions/overrides", ""));
	if (!overrides.is_empty()) {
		permissions->apply_overrides(overrides);
	}
	_run_known_action_self_check();
}

void MultiuserEditorPlugin::_run_known_action_self_check() const {
	if (permissions.is_null()) {
		return;
	}
	static const char *kKnownActionTypes[] = {
		multiuser_editor::kActionHandshake,
		multiuser_editor::kActionHandshakeAck,
		multiuser_editor::kActionAuthChallenge,
		multiuser_editor::kActionChat,
		multiuser_editor::kActionCursorUpdate,
		multiuser_editor::kActionSelect,
		multiuser_editor::kActionTelemetry,
		multiuser_editor::kActionFsSnapshotDone,
		multiuser_editor::kActionFileReject,
		multiuser_editor::kActionProjectSettingsSnapshot,
		multiuser_editor::kActionProperty,
		multiuser_editor::kActionNodeAdd,
		multiuser_editor::kActionNodeDelete,
		multiuser_editor::kActionCrdt,
		multiuser_editor::kActionCrdtSync,
		multiuser_editor::kActionScriptAttach,
		multiuser_editor::kActionScriptDetach,
		multiuser_editor::kActionFileProposeBegin,
		multiuser_editor::kActionFileProposeChunk,
		multiuser_editor::kActionFileProposeEnd,
		multiuser_editor::kActionFileProposeDelete,
		multiuser_editor::kActionFileProposeMove,
		multiuser_editor::kActionFileApplyBegin,
		multiuser_editor::kActionFileApplyChunk,
		multiuser_editor::kActionFileApplyEnd,
		multiuser_editor::kActionFileApplyDelete,
		multiuser_editor::kActionFileApplyMove,
		multiuser_editor::kActionResourceSync,
		multiuser_editor::kActionTileSync,
		multiuser_editor::kActionVfxRestart,
		multiuser_editor::kActionShaderAction,
		multiuser_editor::kActionUnlockAll,
		multiuser_editor::kActionMagicRepairRequest,
		multiuser_editor::kActionGitRequest,
		multiuser_editor::kActionGitResponse,
		multiuser_editor::kActionProjectSetting,
		multiuser_editor::kActionSceneSync,
		multiuser_editor::kActionFsOp,
		multiuser_editor::kActionFsMove,
		multiuser_editor::kActionFsRemove,
		multiuser_editor::kActionFsRefresh,
		multiuser_editor::kActionTeamPlayStart,
		multiuser_editor::kActionTeamPlayStop,
		multiuser_editor::kActionMagicRepairStart,
		multiuser_editor::kActionAutoworkTrigger,
		multiuser_editor::kActionGlobalUndo,
		nullptr,
	};
	for (int i = 0; kKnownActionTypes[i] != nullptr; i++) {
		const String action = String(kKnownActionTypes[i]);
		if (!permissions->is_known_action(action)) {
			ERR_PRINT(vformat("Multiuser Editor: BUG - action type '%s' handled by _route_action is NOT registered in permissions->load_defaults; default-deny will drop it at runtime.", action));
		}
	}
}

bool MultiuserEditorPlugin::_settings_path_replicated(const String &p_path) const {
	if (p_path.is_empty()) {
		return false;
	}
	if (_mu_path_has_sensitive_prefix(p_path)) {
		return false;
	}
	const String csv = String(MULTIUSER_GET("blazium/multiuser_editor/project_settings/replicated_prefixes",
			"rendering/,physics/,application/config/,input/,layer_names/,internationalization/,audio/,debug/,network/"));
	if (csv != _replicated_prefixes_cache_source) {
		_replicated_prefixes_cache.clear();
		const Vector<String> raw = csv.split(",", false);
		for (int i = 0; i < raw.size(); i++) {
			const String prefix = raw[i].strip_edges();
			if (!prefix.is_empty()) {
				_replicated_prefixes_cache.push_back(prefix);
			}
		}
		_replicated_prefixes_cache_source = csv;
	}
	for (int i = 0; i < _replicated_prefixes_cache.size(); i++) {
		if (p_path.begins_with(_replicated_prefixes_cache[i])) {
			return true;
		}
	}
	return false;
}

void MultiuserEditorPlugin::_apply_project_setting_value(const String &p_name, const Variant &p_value) {
	if (p_name.is_empty()) {
		return;
	}
	if (!_settings_path_replicated(p_name)) {
		_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("Project setting filtered (not in allowlist or sensitive): '%s'", p_name));
		return;
	}

	const int array_max = MAX(16, int(MULTIUSER_GET("blazium/multiuser_editor/limits/project_setting_array_max_elements", 65536)));
	switch (p_value.get_type()) {
		case Variant::ARRAY: {
			const Array a = p_value;
			if (a.size() > array_max) {
				_log_cat(LOG_WARN, LOG_REPLICATION, vformat("project_setting '%s' rejected: Array size %d exceeds cap %d", p_name, a.size(), array_max));
				return;
			}
		} break;
		case Variant::DICTIONARY: {
			const Dictionary d = p_value;
			if (d.size() > array_max) {
				_log_cat(LOG_WARN, LOG_REPLICATION, vformat("project_setting '%s' rejected: Dictionary size %d exceeds cap %d", p_name, d.size(), array_max));
				return;
			}
		} break;
		case Variant::PACKED_BYTE_ARRAY: {
			const PackedByteArray a = p_value;
			if (a.size() > array_max) {
				_log_cat(LOG_WARN, LOG_REPLICATION, vformat("project_setting '%s' rejected: PackedByteArray size %d exceeds cap %d", p_name, a.size(), array_max));
				return;
			}
		} break;
		case Variant::PACKED_STRING_ARRAY: {
			const PackedStringArray a = p_value;
			if (a.size() > array_max) {
				_log_cat(LOG_WARN, LOG_REPLICATION, vformat("project_setting '%s' rejected: PackedStringArray size %d exceeds cap %d", p_name, a.size(), array_max));
				return;
			}
		} break;
		case Variant::PACKED_INT32_ARRAY: {
			const PackedInt32Array a = p_value;
			if (a.size() > array_max) {
				_log_cat(LOG_WARN, LOG_REPLICATION, vformat("project_setting '%s' rejected: PackedInt32Array size %d exceeds cap %d", p_name, a.size(), array_max));
				return;
			}
		} break;
		case Variant::PACKED_INT64_ARRAY: {
			const PackedInt64Array a = p_value;
			if (a.size() > array_max) {
				_log_cat(LOG_WARN, LOG_REPLICATION, vformat("project_setting '%s' rejected: PackedInt64Array size %d exceeds cap %d", p_name, a.size(), array_max));
				return;
			}
		} break;
		case Variant::PACKED_FLOAT32_ARRAY: {
			const PackedFloat32Array a = p_value;
			if (a.size() > array_max) {
				_log_cat(LOG_WARN, LOG_REPLICATION, vformat("project_setting '%s' rejected: PackedFloat32Array size %d exceeds cap %d", p_name, a.size(), array_max));
				return;
			}
		} break;
		case Variant::PACKED_FLOAT64_ARRAY: {
			const PackedFloat64Array a = p_value;
			if (a.size() > array_max) {
				_log_cat(LOG_WARN, LOG_REPLICATION, vformat("project_setting '%s' rejected: PackedFloat64Array size %d exceeds cap %d", p_name, a.size(), array_max));
				return;
			}
		} break;
		case Variant::PACKED_VECTOR2_ARRAY: {
			const PackedVector2Array a = p_value;
			if (a.size() > array_max) {
				_log_cat(LOG_WARN, LOG_REPLICATION, vformat("project_setting '%s' rejected: PackedVector2Array size %d exceeds cap %d", p_name, a.size(), array_max));
				return;
			}
		} break;
		case Variant::PACKED_VECTOR3_ARRAY: {
			const PackedVector3Array a = p_value;
			if (a.size() > array_max) {
				_log_cat(LOG_WARN, LOG_REPLICATION, vformat("project_setting '%s' rejected: PackedVector3Array size %d exceeds cap %d", p_name, a.size(), array_max));
				return;
			}
		} break;
		case Variant::PACKED_COLOR_ARRAY: {
			const PackedColorArray a = p_value;
			if (a.size() > array_max) {
				_log_cat(LOG_WARN, LOG_REPLICATION, vformat("project_setting '%s' rejected: PackedColorArray size %d exceeds cap %d", p_name, a.size(), array_max));
				return;
			}
		} break;
		default:
			break;
	}

	if (ProjectSettings::get_singleton()->has_setting(p_name)) {
		const Variant existing = ProjectSettings::get_singleton()->get(p_name);
		if (existing.get_type() != Variant::NIL && existing.get_type() != p_value.get_type()) {
			_log_cat(LOG_WARN, LOG_REPLICATION, vformat("project_setting '%s' rejected: type mismatch (existing=%d, incoming=%d)", p_name, int(existing.get_type()), int(p_value.get_type())));
			return;
		}
	}
	suppress_scene_events = true;
	ProjectSettings::get_singleton()->set_setting(p_name, p_value);
	settings_cache[p_name] = p_value;
	suppress_scene_events = false;
	_log_cat(LOG_DEBUG, LOG_REPLICATION, vformat("Project setting applied: '%s'", p_name));
}

void MultiuserEditorPlugin::_send_project_settings_snapshot(int p_target_net_id) {
	if (network.get_mode() != MultiuserEditorNetwork::MODE_HOST) {
		return;
	}
	Dictionary snapshot;
	List<PropertyInfo> props;
	ProjectSettings::get_singleton()->get_property_list(&props);
	for (const PropertyInfo &pi : props) {
		if (!_settings_path_replicated(pi.name)) {
			continue;
		}
		snapshot[pi.name] = ProjectSettings::get_singleton()->get(pi.name);
	}
	if (snapshot.is_empty()) {
		_log_cat(LOG_INFO, LOG_REPLICATION, "Project settings snapshot is empty (no allowlisted keys).");
		return;
	}
	Dictionary act_data;
	act_data["settings"] = snapshot;
	Dictionary action;
	action["type"] = multiuser_editor::kActionProjectSettingsSnapshot;
	action["data"] = act_data;
	action["peer_id"] = local_peer_id;
	action["timestamp"] = OS::get_singleton()->get_unix_time();
	network.send_action_to(p_target_net_id, action);
	_log_cat(LOG_INFO, LOG_REPLICATION, vformat("Sent project settings snapshot (%d keys) to net_id=%d", snapshot.size(), p_target_net_id));
}

CodeEdit *MultiuserEditorPlugin::_find_active_code_edit() const {
	ScriptEditor *script_editor = EditorInterface::get_singleton() ? EditorInterface::get_singleton()->get_script_editor() : nullptr;
	if (script_editor) {
		Variant editor_variant = script_editor->call("get_current_editor");
		if (editor_variant.get_type() == Variant::OBJECT) {
			Object *editor_obj = editor_variant;
			if (CodeEdit *code_edit = multiuser_find_code_edit(Object::cast_to<Node>(editor_obj))) {
				return code_edit;
			}
		}
	}
	Control *focus = get_viewport() ? get_viewport()->gui_get_focus_owner() : nullptr;
	return Object::cast_to<CodeEdit>(focus);
}

String MultiuserEditorPlugin::_find_script_path_for_code_edit(CodeEdit *p_code_edit) const {
	if (!p_code_edit) {
		return String();
	}
	ScriptEditor *script_editor = EditorInterface::get_singleton() ? EditorInterface::get_singleton()->get_script_editor() : nullptr;
	if (script_editor) {
		Variant script_variant = script_editor->call("get_current_script");
		if (script_variant.get_type() == Variant::OBJECT) {
			Object *script_obj = script_variant;
			Resource *res = Object::cast_to<Resource>(script_obj);
			if (res && !res->get_path().is_empty()) {
				return res->get_path();
			}
		}
	}
	if (p_code_edit->has_meta("multiuser_script_path")) {
		return p_code_edit->get_meta("multiuser_script_path");
	}
	return "unsaved://" + String::num_uint64(uint64_t(reinterpret_cast<uintptr_t>(p_code_edit)), 16);
}

void MultiuserEditorPlugin::_on_selection_changed() {
	if (!network.is_connected()) {
		return;
	}
	_emit_action(action_interceptor.capture_selection());
}

void MultiuserEditorPlugin::_on_node_added(Node *p_node) {
	if (!undo_redo || !undo_redo->is_committing_action() || suppress_scene_events) {
		return;
	}
	if (network.is_connected() && bool(MULTIUSER_GET("blazium/multiuser_editor/sync_scene_changes", true))) {
		Dictionary action = action_interceptor.capture_node_add(p_node);
		if (!action.is_empty()) {
			_emit_action(action);
		} else if (action_interceptor.consume_locked_add_blocked()) {
			suppress_scene_events = true;
			action_interceptor.undo_last_scene_action();
			suppress_scene_events = false;
		}
	}
}

void MultiuserEditorPlugin::_on_node_removed(Node *p_node) {
	if (!undo_redo || !undo_redo->is_committing_action() || suppress_scene_events) {
		return;
	}
	if (network.is_connected() && bool(MULTIUSER_GET("blazium/multiuser_editor/sync_scene_changes", true))) {
		Dictionary action = action_interceptor.capture_node_delete(p_node);
		if (!action.is_empty()) {
			_emit_action(action);
		} else if (action_interceptor.consume_locked_delete_blocked()) {
			suppress_scene_events = true;
			action_interceptor.undo_last_scene_action();
			suppress_scene_events = false;
		}
	}
}

void MultiuserEditorPlugin::_on_network_peer_connected(int p_net_id) {
	if (p_net_id == 1) {
		if (network.get_mode() == MultiuserEditorNetwork::MODE_JOIN) {
			if (bool(MULTIUSER_GET("blazium/multiuser_editor/server_log_connections", true))) {
				print_line("[Multiuser Socket] Connected to host! Negotiating role...");
			}
			return;
		} else if (network.get_mode() == MultiuserEditorNetwork::MODE_HOST) {
			network.remember_peer(1, local_peer_id);
			if (dock) {
				dock->add_peer(local_peer_id);
				Dictionary self_role;
				self_role["role"] = local_role;
				dock->update_peer_telemetry(local_peer_id, self_role);
				dock->set_connected(get_status_text());
			}
			if (bool(MULTIUSER_GET("blazium/multiuser_editor/server_log_connections", true))) {
				print_line(vformat("[Multiuser Connect] Local Session Host Started as Peer %d", p_net_id));
			}
			return;
		}
	}
	if (network.get_mode() == MultiuserEditorNetwork::MODE_HOST) {
		if (bool(MULTIUSER_GET("blazium/multiuser_editor/server_log_connections", true))) {
			print_line(vformat("[Multiuser Socket] Inbound connection (%d). Awaiting authentication...", p_net_id));
		}

		_issue_pending_challenge(p_net_id);
	}
}

void MultiuserEditorPlugin::_issue_pending_challenge(int p_net_id) {
	const int cap = MAX(1, int(MULTIUSER_GET("blazium/multiuser_editor/limits/pending_challenges_max", 256)));
	_expire_stale_pending_challenges();
	if (int(_pending_challenges.size()) >= cap) {
		_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("Pending challenges at cap (%d); dropping new join net_id=%d", cap, p_net_id));
		_record_security_event(LOG_WARN, LOG_PERMISSIONS, vformat("pending challenge cap hit; drop %d", p_net_id));
		network.disconnect_peer(p_net_id);
		return;
	}
	const uint64_t ticks = OS::get_singleton()->get_ticks_usec();
	const uint64_t rand_salt = (uint64_t)Math::rand();
	ChallengeRec rec;
	rec.challenge = String::num_int64(int64_t(ticks ^ (rand_salt << 32 | (uint64_t)Math::rand()))).sha256_text();
	rec.issued_msec = OS::get_singleton()->get_ticks_msec();
	_pending_challenges[p_net_id] = rec;

	Dictionary act_data;
	act_data["challenge"] = rec.challenge;
	network.send_action_to(p_net_id, _make_action(multiuser_editor::kActionAuthChallenge, act_data));
	_log_connection(vformat("Issued auth_challenge to net_id=%d", p_net_id));
}

void MultiuserEditorPlugin::_expire_stale_pending_challenges() {
	const int ttl_sec = MAX(1, int(MULTIUSER_GET("blazium/multiuser_editor/limits/pending_challenge_ttl_sec", 30)));
	const uint64_t ttl_msec = uint64_t(ttl_sec) * 1000ULL;
	const uint64_t now = OS::get_singleton()->get_ticks_msec();
	Vector<int> to_drop;
	for (const KeyValue<int, ChallengeRec> &E : _pending_challenges) {
		if (now < E.value.issued_msec) {
			to_drop.push_back(E.key);
			continue;
		}
		if ((now - E.value.issued_msec) > ttl_msec) {
			to_drop.push_back(E.key);
		}
	}
	for (int net_id : to_drop) {
		_pending_challenges.erase(net_id);
	}
}

bool MultiuserEditorPlugin::_consume_pending_challenge(int p_net_id, String &r_challenge) {
	_expire_stale_pending_challenges();
	HashMap<int, ChallengeRec>::Iterator it = _pending_challenges.find(p_net_id);
	if (!it) {
		return false;
	}
	r_challenge = it->value.challenge;
	_pending_challenges.remove(it);
	return true;
}

void MultiuserEditorPlugin::_on_network_peer_disconnected(int p_net_id) {
	String peer_id = network.get_peer_id(p_net_id);
	if (!peer_id.is_empty()) {
		if (bool(MULTIUSER_GET("blazium/multiuser_editor/server_log_connections", true))) {
			print_line(vformat("[Multiuser Connect] Connection closed: peer %s (%d)", peer_id, p_net_id));
		}
		lock_manager.release_peer(peer_id);
		if (ghost_overlay) {
			ghost_overlay->remove_peer(peer_id);
		}
		if (dock) {
			dock->remove_peer(peer_id);
		}
	}
	_git_throttle_last_msec.erase(p_net_id);
	_project_setting_last_msec.erase(p_net_id);
	_cursor_update_last_msec.erase(p_net_id);
	_telemetry_last_msec.erase(p_net_id);
	_chat_last_msec.erase(p_net_id);
	_relay_buckets.erase(p_net_id);
	_inbound_buckets.erase(p_net_id);

	_pending_challenges.erase(p_net_id);
	{
		const String prefix = String::num_int64(p_net_id) + ":";
		Vector<String> to_erase;
		for (const KeyValue<String, uint64_t> &E : _unknown_relay_log_dedupe) {
			if (E.key.begins_with(prefix)) {
				to_erase.push_back(E.key);
			}
		}
		for (const String &k : to_erase) {
			_unknown_relay_log_dedupe.erase(k);
		}
	}

	filesystem_sync.forget_peer(p_net_id);
	network.forget_peer(p_net_id);
	_update_ui();
}

void MultiuserEditorPlugin::_on_inspector_property_changed(Object *p_undo_redo, Object *p_modified_object, const String &p_property, const Variant &p_new_value) {
	if (!network.is_connected() || suppress_scene_events) {
		return;
	}
	action_interceptor.apply_inspector_property_lock(p_undo_redo, p_modified_object, p_property, p_new_value);

	Node *node = Object::cast_to<Node>(p_modified_object);
	if (node && node->is_class("TileMapLayer") && p_property == "tile_data") {
		PackedByteArray blob;
		if (p_new_value.get_type() == Variant::PACKED_BYTE_ARRAY) {
			blob = p_new_value;
		} else {
			int len = 0;
			encode_variant(p_new_value, nullptr, len, false);
			if (len > 0) {
				blob.resize(len);
				encode_variant(p_new_value, blob.ptrw(), len, false);
			}
		}
		const int tile_data_max = MAX(0, int(MULTIUSER_GET("blazium/multiuser_editor/limits/tile_data_max_bytes", 4 * 1024 * 1024)));
		if (tile_data_max > 0 && blob.size() > tile_data_max) {
			_log_cat(LOG_WARN, LOG_REPLICATION, vformat("tile_sync(tile_data) dropped: size %d exceeds cap %d", blob.size(), tile_data_max));
		} else {
			Node *root = EditorInterface::get_singleton()->get_edited_scene_root();
			if (root) {
				Dictionary act_data;
				act_data["node_path"] = lock_manager.clean_path(String(root->get_path_to(node)));
				act_data["tile_data"] = blob;
				act_data["version"] = 1;
				_emit_action(_make_action(multiuser_editor::kActionTileSync, act_data));
			}
		}
		return;
	}

	if (p_new_value.get_type() == Variant::OBJECT) {
		Resource *res = Object::cast_to<Resource>(p_new_value);
		if (res && res->get_path().is_empty()) {
			Node *root = EditorInterface::get_singleton()->get_edited_scene_root();
			Dictionary act_data;
			act_data["node_path"] = lock_manager.clean_path(String(root->get_path_to(node)));
			act_data["name"] = p_property;
			int len = 0;
			encode_variant(p_new_value, nullptr, len, false);
			PackedByteArray res_data;
			res_data.resize(len);
			encode_variant(p_new_value, res_data.ptrw(), len, false);
			act_data["data"] = res_data;
			_emit_action(_make_action(multiuser_editor::kActionResourceSync, act_data));
		}
	}
}

void MultiuserEditorPlugin::_refresh_hot_settings_cache() {
	_hot_enabled = EditorSettings::get_singleton() && EditorSettings::get_singleton()->has_setting("blazium/multiuser_editor/enabled") && bool(MULTIUSER_GET("blazium/multiuser_editor/enabled", false));
	_hot_cursor_sync_enabled = bool(MULTIUSER_GET("blazium/multiuser_editor/cursor_sync_enabled", true));
	_hot_show_remote_cursors = bool(MULTIUSER_GET("blazium/multiuser_editor/show_remote_cursors", true));
	_hot_sync_scene_changes = bool(MULTIUSER_GET("blazium/multiuser_editor/sync_scene_changes", true));
	_hot_sync_scripts = bool(MULTIUSER_GET("blazium/multiuser_editor/sync_scripts", true));
	_hot_telemetry_interval = double(MULTIUSER_GET("blazium/multiuser_editor/telemetry_broadcast_interval_sec", 2.0));
	_hot_script_poll_interval = double(MULTIUSER_GET("blazium/multiuser_editor/script_poll_interval_sec", 0.5));
	_hot_lock_timeout_sec = double(MULTIUSER_GET("blazium/multiuser_editor/locks/timeout_sec", 30.0));
	lock_manager.set_lock_ttl_msec((uint64_t)MAX(0.0, _hot_lock_timeout_sec * 1000.0));
	_hot_settings_dirty = false;
}

void MultiuserEditorPlugin::_on_editor_settings_changed() {
	_hot_settings_dirty = true;

	_cached_max_packet_size_mb = -1;
	_cached_packets_per_poll_max = -1;
	_cached_max_clients = -1;
	_cached_crdt_atoms_max = -1;
	_cached_script_attach_max_bytes = -1;
	_cached_security_events_max = -1;
	_cached_jwt_jti_sweep_ms = -1;

	_reload_access_list();
}

void MultiuserEditorPlugin::_on_project_settings_changed() {
	if (!network.is_connected() || suppress_scene_events) {
		return;
	}
	List<PropertyInfo> props;
	ProjectSettings::get_singleton()->get_property_list(&props);
	for (const PropertyInfo &pi : props) {
		if (!_settings_path_replicated(pi.name)) {
			continue;
		}
		Variant current = ProjectSettings::get_singleton()->get(pi.name);
		if (!settings_cache.has(pi.name) || settings_cache[pi.name] != current) {
			settings_cache[pi.name] = current;
			Dictionary act_data;
			act_data["name"] = pi.name;
			act_data["value"] = current;
			_emit_action(_make_action(multiuser_editor::kActionProjectSetting, act_data));
			_log_cat(LOG_DEBUG, LOG_REPLICATION, vformat("Local project setting changed: '%s'", String(pi.name)));
		}
	}
}

void MultiuserEditorPlugin::_on_scene_saved(const String &p_filepath) {
	if (!network.is_connected() || suppress_scene_events) {
		return;
	}
	_emit_action(_make_action(multiuser_editor::kActionUnlockAll, Dictionary()));

	Ref<FileAccess> file = FileAccess::open(p_filepath, FileAccess::READ);
	if (file.is_valid()) {
		Dictionary act_data;
		act_data["filepath"] = p_filepath;
		act_data["content"] = file->get_as_text();
		_emit_action(_make_action(multiuser_editor::kActionSceneSync, act_data));
	}
}

void MultiuserEditorPlugin::_on_filesystem_changed() {
	if (!network.is_connected() || suppress_scene_events || suppress_fs_broadcast) {
		return;
	}
	if (!bool(MULTIUSER_GET("blazium/multiuser_editor/file_sync/enabled", true))) {
		_emit_action(_make_action(multiuser_editor::kActionFsRefresh, Dictionary()));
		return;
	}
	_schedule_filesystem_diff();
}

bool MultiuserEditorPlugin::_can_use_git() {
	if (git_availability != GIT_UNCHECKED) {
		return git_availability == GIT_OK;
	}

	String out;
	int code = 0;

	List<String> ver;
	ver.push_back("--version");
	Error err = OS::get_singleton()->execute("git", ver, &out, &code, true);
	if (err != OK || code != 0) {
		git_availability = GIT_UNAVAILABLE;
		git_unavailable_reason = "git binary not found";
		WARN_PRINT("Multiuser Editor: git auto-commit/branching disabled (" + git_unavailable_reason + "). Set blazium/multiuser_editor/git_auto_commit = false to silence.");
		return false;
	}

	out = String();
	code = 0;
	List<String> args;
	args.push_back("rev-parse");
	args.push_back("--is-inside-work-tree");
	err = OS::get_singleton()->execute("git", args, &out, &code, true);
	if (err != OK || code != 0 || out.strip_edges() != "true") {
		git_availability = GIT_UNAVAILABLE;
		git_unavailable_reason = "project directory is not a git repository";
		WARN_PRINT("Multiuser Editor: git auto-commit/branching disabled (" + git_unavailable_reason + "). Set blazium/multiuser_editor/git_auto_commit = false to silence.");
		return false;
	}

	git_availability = GIT_OK;
	return true;
}

String MultiuserEditorPlugin::_resolve_session_branch_name() {
	String name;
	if (dock) {
		name = dock->get_session_branch_override();
	}
	if (name.is_empty()) {
		name = String(MULTIUSER_GET("blazium/multiuser_editor/git_session_branch_name", "multiuser_session_{timestamp}"));
	}
	name = name.strip_edges();
	if (name.is_empty()) {
		name = "multiuser_session_{timestamp}";
	}
	name = name.replace("{timestamp}", String::num_int64((int64_t)OS::get_singleton()->get_unix_time()));
	return name;
}

String MultiuserEditorPlugin::_resolve_merge_target_branch() {
	String name;
	if (dock) {
		name = dock->get_merge_target_override();
	}
	if (name.is_empty()) {
		name = String(MULTIUSER_GET("blazium/multiuser_editor/git_merge_target_branch", "main"));
	}
	name = name.strip_edges();
	if (name.is_empty()) {
		name = "main";
	}
	return name;
}

void MultiuserEditorPlugin::_async_git_execute(const List<String> &p_args) {
	struct GitTaskPayload {
		List<String> args;
		bool debug_logging = false;
	};
	struct AsyncRunner {
		static void _run(void *p_userdata) {
			GitTaskPayload *payload = static_cast<GitTaskPayload *>(p_userdata);
			String output;
			int exitcode = 0;
			OS::get_singleton()->execute("git", payload->args, &output, &exitcode, true);
			if (!output.is_empty() && exitcode == 0) {
				print_line("MultiuserEditor Git: " + output.strip_edges());
			} else if (exitcode != 0 && payload->debug_logging) {
				print_line("MultiuserEditor Git failed (exit " + itos(exitcode) + "): " + output.strip_edges());
			}
			memdelete(payload);
		}
	};
	GitTaskPayload *payload = memnew(GitTaskPayload);
	payload->args = p_args;
	payload->debug_logging = bool(MULTIUSER_GET("blazium/multiuser_editor/enable_debug_logging", false));
	WorkerThreadPool::get_singleton()->add_native_task(&AsyncRunner::_run, payload, true, "Multiuser Git Task");
}

bool MultiuserEditorPlugin::_gitops_parse(const Dictionary &p_action, int p_sender_net_id, const String &p_peer_id, GitOpRequest &r_out, String &r_reason) const {
	const Dictionary data_dict = p_action.get("data", Dictionary());
	r_out.op = String(data_dict.get("op", "")).strip_edges();
	r_out.branch = String(data_dict.get("branch", "")).strip_edges();
	r_out.remote = String(data_dict.get("remote", "")).strip_edges();
	r_out.message = String(data_dict.get("message", ""));
	r_out.requester_peer_id = p_peer_id;
	r_out.requester_net_id = p_sender_net_id;
	if (r_out.remote.is_empty()) {
		r_out.remote = String(MULTIUSER_GET("blazium/multiuser_editor/git_default_remote", "origin"));
	}
	if (r_out.op.is_empty()) {
		r_reason = "missing_op";
		return false;
	}
	return true;
}

bool MultiuserEditorPlugin::_gitops_validate(const GitOpRequest &p_req, String &r_reason) const {
	static const char *kAllowed[] = {
		"pull", "pull_rebase", "push", "force_push", "commit",
		"branch_switch", "branch_create", "status", "current_branch", nullptr
	};
	bool op_ok = false;
	for (int i = 0; kAllowed[i] != nullptr; i++) {
		if (p_req.op == kAllowed[i]) {
			op_ok = true;
			break;
		}
	}
	if (!op_ok) {
		r_reason = "unknown_op";
		return false;
	}
	if (!MultiuserEditorActionInterceptor::is_safe_remote_name(p_req.remote)) {
		r_reason = "invalid_remote";
		return false;
	}
	if (p_req.op == "branch_switch" || p_req.op == "branch_create") {
		if (!MultiuserEditorActionInterceptor::is_safe_branch_name(p_req.branch)) {
			r_reason = "invalid_branch";
			return false;
		}
	}
	if (p_req.op == "commit") {
		if (!MultiuserEditorActionInterceptor::is_safe_commit_message(p_req.message)) {
			r_reason = "invalid_commit_message";
			return false;
		}
	}
	return true;
}

bool MultiuserEditorPlugin::_gitops_throttle_ok(int p_sender_net_id) {
	const int throttle_ms = MAX(0, int(MULTIUSER_GET("blazium/multiuser_editor/git_op_throttle_ms", 2000)));
	if (throttle_ms <= 0) {
		return true;
	}
	const uint64_t now_msec = OS::get_singleton()->get_ticks_msec();
	if (_git_throttle_last_msec.has(p_sender_net_id)) {
		const uint64_t prev = _git_throttle_last_msec[p_sender_net_id];
		if (now_msec - prev < uint64_t(throttle_ms)) {
			return false;
		}
	}
	_git_throttle_last_msec[p_sender_net_id] = now_msec;
	return true;
}

String MultiuserEditorPlugin::_gitops_resolve_current_branch_blocking() {
	String out;
	int code = 0;
	List<String> args;
	args.push_back("rev-parse");
	args.push_back("--abbrev-ref");
	args.push_back("HEAD");
	OS::get_singleton()->execute("git", args, &out, &code, true);
	if (code != 0) {
		return String();
	}
	String branch = out.strip_edges();
	if (!MultiuserEditorActionInterceptor::is_safe_branch_name(branch)) {
		return String();
	}
	return branch;
}

void MultiuserEditorPlugin::_gitops_drain_pending_results() {
	if (_git_shared.is_null()) {
		return;
	}
	List<GitOpResult> drained;
	{
		MutexLock lock(_git_shared->mutex);
		drained = _git_shared->pending_results;
		_git_shared->pending_results.clear();
	}
	for (const GitOpResult &res : drained) {
		_gitops_broadcast_result(res.req, res.success, res.exit_code, res.output, res.reason);
	}
}

MultiuserEditorPlugin::GitOpArgv MultiuserEditorPlugin::_gitops_build_argv(const GitOpRequest &p_req) {
	GitOpArgv out;
	if (p_req.op == "pull") {
		out.args1.push_back("pull");
		out.args1.push_back(p_req.remote);
		out.prepend_current_branch = true;
	} else if (p_req.op == "pull_rebase") {
		out.args1.push_back("pull");
		out.args1.push_back("--rebase");
		out.args1.push_back(p_req.remote);
		out.prepend_current_branch = true;
	} else if (p_req.op == "push") {
		out.args1.push_back("push");
		out.args1.push_back(p_req.remote);
		out.prepend_current_branch = true;
	} else if (p_req.op == "force_push") {
		out.args1.push_back("push");
		out.args1.push_back("--force-with-lease");
		out.args1.push_back(p_req.remote);
		out.prepend_current_branch = true;
	} else if (p_req.op == "commit") {
		out.args1.push_back("add");
		out.args1.push_back(".");
		out.args2.push_back("commit");
		out.args2.push_back("-m");
		out.args2.push_back(p_req.message);
		out.has_second = true;
	} else if (p_req.op == "branch_switch") {
		out.args1.push_back("checkout");
		out.args1.push_back(p_req.branch);
	} else if (p_req.op == "branch_create") {
		out.args1.push_back("checkout");
		out.args1.push_back("-b");
		out.args1.push_back(p_req.branch);
	} else if (p_req.op == "status") {
		out.args1.push_back("status");
		out.args1.push_back("--porcelain=v1");
	} else if (p_req.op == "current_branch") {
		out.args1.push_back("rev-parse");
		out.args1.push_back("--abbrev-ref");
		out.args1.push_back("HEAD");
	} else {
		out.unknown_op = true;
	}
	return out;
}

void MultiuserEditorPlugin::_gitops_run_async(const GitOpRequest &p_req) {
	struct GitOpsTaskPayload {
		GitOpRequest req;
		List<String> args1;
		List<String> args2;
		bool has_second = false;

		bool prepend_current_branch = false;
		Ref<GitSharedState> shared;
		int output_cap = multiuser_editor::kGitOutputDefaultMax;
	};
	struct AsyncRunner {
		static void _run(void *p_userdata) {
			GitOpsTaskPayload *payload = static_cast<GitOpsTaskPayload *>(p_userdata);
			String combined_output;
			int exitcode = 0;

			if (payload->prepend_current_branch) {
				List<String> rev_args;
				rev_args.push_back("rev-parse");
				rev_args.push_back("--abbrev-ref");
				rev_args.push_back("HEAD");
				String rev_out;
				int rev_code = 0;
				OS::get_singleton()->execute("git", rev_args, &rev_out, &rev_code, true);
				if (rev_code == 0) {
					String branch = rev_out.strip_edges();
					if (MultiuserEditorActionInterceptor::is_safe_branch_name(branch)) {
						payload->args1.push_back(branch);
					}
				}
			}

			String out1;
			OS::get_singleton()->execute("git", payload->args1, &out1, &exitcode, true);
			combined_output += out1;
			if (payload->has_second && exitcode == 0) {
				String out2;
				OS::get_singleton()->execute("git", payload->args2, &out2, &exitcode, true);
				combined_output += out2;
			}

			GitOpResult res;
			res.req = payload->req;
			res.success = (exitcode == 0);
			res.exit_code = exitcode;
			const int git_output_cap = MAX(64, payload->output_cap);
			if (combined_output.length() > git_output_cap) {
				res.output = combined_output.substr(0, git_output_cap) + "\n...[truncated]";
			} else {
				res.output = combined_output;
			}
			res.reason = String();

			if (payload->shared.is_valid()) {
				MutexLock lock(payload->shared->mutex);
				const int queue_cap = MAX(1, payload->shared->queue_cap);
				if (payload->shared->pending_results.size() < queue_cap) {
					payload->shared->pending_results.push_back(res);
				} else {
					WARN_PRINT("Multiuser editor: git pending-results queue full; dropping result.");
				}
				if (payload->shared->inflight > 0) {
					payload->shared->inflight--;
				}
			}
			memdelete(payload);
		}
	};

	if (_git_shared.is_null()) {
		_git_shared.instantiate();
	}

	const int snap_queue_cap = MAX(1, int(MULTIUSER_GET("blazium/multiuser_editor/limits/git_pending_results_max", 64)));
	const int snap_output_cap = MAX(64, int(MULTIUSER_GET("blazium/multiuser_editor/limits/git_output_max_chars", multiuser_editor::kGitOutputDefaultMax)));
	{
		MutexLock lock(_git_shared->mutex);
		_git_shared->queue_cap = snap_queue_cap;
		_git_shared->output_cap = snap_output_cap;
	}

	GitOpsTaskPayload *payload = memnew(GitOpsTaskPayload);
	payload->req = p_req;
	payload->shared = _git_shared;
	payload->output_cap = snap_output_cap;
	{
		MutexLock lock(_git_shared->mutex);
		_git_shared->inflight++;
	}

	GitOpArgv argv = _gitops_build_argv(p_req);
	if (argv.unknown_op) {
		{
			MutexLock lock(_git_shared->mutex);
			if (_git_shared->inflight > 0) {
				_git_shared->inflight--;
			}
		}
		memdelete(payload);
		_gitops_broadcast_result(p_req, false, -1, String(), "unknown_op");
		return;
	}
	payload->args1 = argv.args1;
	payload->args2 = argv.args2;
	payload->has_second = argv.has_second;
	payload->prepend_current_branch = argv.prepend_current_branch;

	WorkerThreadPool::get_singleton()->add_native_task(&AsyncRunner::_run, payload, true, "Multiuser Git Op");
}

void MultiuserEditorPlugin::_gitops_broadcast_result(const GitOpRequest &p_req, bool p_success, int p_exit_code, const String &p_output, const String &p_reason) {
	const bool force_push_enabled = bool(MULTIUSER_GET("blazium/multiuser_editor/allow_editor_force_push", false)) ||
			bool(MULTIUSER_GET("blazium/multiuser_editor/git_remote_actions_enabled", false));

	Dictionary act_data;
	act_data["op"] = p_req.op;
	act_data["branch"] = p_req.branch;
	act_data["remote"] = p_req.remote;
	act_data["requester_peer_id"] = p_req.requester_peer_id;
	act_data["success"] = p_success;
	act_data["exit_code"] = p_exit_code;
	act_data["output"] = p_output;
	act_data["reason"] = p_reason;
	act_data["force_push_enabled"] = bool(MULTIUSER_GET("blazium/multiuser_editor/allow_editor_force_push", false));
	act_data["remote_actions_enabled"] = bool(MULTIUSER_GET("blazium/multiuser_editor/git_remote_actions_enabled", false));

	(void)force_push_enabled;

	Dictionary action = _make_action(multiuser_editor::kActionGitResponse, act_data);
	action["peer_id"] = local_peer_id;
	action["timestamp"] = OS::get_singleton()->get_unix_time();

	if (network.get_mode() == MultiuserEditorNetwork::MODE_HOST && network.is_connected()) {
		network.send_action(action);
	}
	if (dock) {
		dock->show_git_response(act_data);
	}
	_log_cat(p_success ? LOG_INFO : LOG_WARN, LOG_REPLICATION,
			vformat("git_response op='%s' success=%s exit=%d reason='%s' requester=%s",
					p_req.op, p_success ? String("true") : String("false"), p_exit_code, p_reason, p_req.requester_peer_id));
}

void MultiuserEditorPlugin::_stop() {
	if (network.get_mode() == MultiuserEditorNetwork::MODE_HOST && !current_session_branch.is_empty()) {
		if (_can_use_git()) {
			List<String> git_add_args;
			git_add_args.push_back("add");
			git_add_args.push_back(".");
			_async_git_execute(git_add_args);

			List<String> git_commit_args;
			git_commit_args.push_back("commit");
			git_commit_args.push_back("-m");
			git_commit_args.push_back("Multiuser Session Sync");
			_async_git_execute(git_commit_args);

			List<String> git_checkout_args;
			git_checkout_args.push_back("checkout");
			git_checkout_args.push_back(_resolve_merge_target_branch());
			_async_git_execute(git_checkout_args);

			List<String> git_merge_args;
			git_merge_args.push_back("merge");
			git_merge_args.push_back(current_session_branch);
			_async_git_execute(git_merge_args);
		}
		current_session_branch = "";
	} else if (network.get_mode() == MultiuserEditorNetwork::MODE_HOST && bool(MULTIUSER_GET("blazium/multiuser_editor/git_auto_commit", false))) {
		if (_can_use_git()) {
			List<String> git_add_args;
			git_add_args.push_back("add");
			git_add_args.push_back(".");
			_async_git_execute(git_add_args);

			List<String> git_commit_args;
			git_commit_args.push_back("commit");
			git_commit_args.push_back("-m");
			git_commit_args.push_back("Multiuser Session Sync");
			_async_git_execute(git_commit_args);
		}
	}

	Vector<String> peer_ids = network.get_peer_ids();
	for (const String &peer_id : peer_ids) {
		lock_manager.release_peer(peer_id);
		if (ghost_overlay) {
			ghost_overlay->remove_peer(peer_id);
		}
	}
	_disconnect_network_signals();
	network.stop();

	if (_git_shared.is_valid()) {
		const uint64_t shutdown_wait_ms = (uint64_t)MAX(100, int(MULTIUSER_GET("blazium/multiuser_editor/git/shutdown_wait_ms", 5000)));
		const uint32_t poll_delay_usec = (uint32_t)MAX(100, int(MULTIUSER_GET("blazium/multiuser_editor/git/poll_delay_usec", 10000)));
		const uint64_t deadline = OS::get_singleton()->get_ticks_msec() + shutdown_wait_ms;
		while (true) {
			int inflight = 0;
			{
				MutexLock lock(_git_shared->mutex);
				inflight = _git_shared->inflight;
			}
			if (inflight == 0 || OS::get_singleton()->get_ticks_msec() >= deadline) {
				break;
			}
			OS::get_singleton()->delay_usec(poll_delay_usec);
		}
		MutexLock lock(_git_shared->mutex);
		_git_shared->pending_results.clear();
	}

	lock_manager.clear();
	action_interceptor.clear_caches();
	script_sync.clear_all_buffers();
	filesystem_sync.clear_snapshot();
	filesystem_sync.clear_pending();
	if (ghost_overlay) {
		ghost_overlay->detach();
	}

	_chat_last_msec.clear();
	_cursor_update_last_msec.clear();
	_telemetry_last_msec.clear();
	_git_throttle_last_msec.clear();
	_project_setting_last_msec.clear();
	_unknown_relay_log_dedupe.clear();
	_jwt_jti_cache.clear();
	_jwt_jti_lru.clear();
	_jwt_last_jti_sweep_msec = 0;
	_relay_buckets.clear();
	_inbound_buckets.clear();

	_pending_challenges.clear();
	session_authenticated = false;
	if (dock) {
		dock->set_auth_mode(MultiuserEditorDock::AUTH_NONE);
		dock->reset_counters();
	}
	_update_ui();
}

void MultiuserEditorPlugin::host_from_settings() {
	host_session(int(MULTIUSER_GET("blazium/multiuser_editor/default_port", 7654)), "");
}

void MultiuserEditorPlugin::join_from_settings() {
	join_session(String(MULTIUSER_GET("blazium/multiuser_editor/default_host", "127.0.0.1")), int(MULTIUSER_GET("blazium/multiuser_editor/default_port", 7654)), "");
}

void MultiuserEditorPlugin::stop_session() {
	_stop();
}

void MultiuserEditorPlugin::host_session(int p_port, const String &p_password) {
	if (!can_host_sessions()) {
		WARN_PRINT("Multiuser Editor host mode is unavailable in web client-only builds.");
		_update_ui();
		return;
	}
	{
		String reason;
		if (!_is_inside_loaded_project(&reason)) {
			ERR_PRINT("Multiuser Editor: refusing to host outside a loaded Godot project: " + reason);
			_log_cat(LOG_ERROR, LOG_PERMISSIONS, "host refused: " + reason);
			_update_ui();
			return;
		}
	}
	if (p_port < multiuser_editor::kPortMin || p_port > multiuser_editor::kPortMax) {
		ERR_PRINT(vformat("Multiuser Editor: refusing to host on out-of-range port %d (must be %d-%d).", p_port, multiuser_editor::kPortMin, multiuser_editor::kPortMax));
		_update_ui();
		return;
	}
	if (bool(MULTIUSER_GET("blazium/multiuser_editor/git_auto_branching", false))) {
		const String session_branch = _resolve_session_branch_name();
		const String merge_target = _resolve_merge_target_branch();
		if (!MultiuserEditorActionInterceptor::is_safe_branch_name(session_branch)) {
			_log_cat(LOG_ERROR, LOG_PERMISSIONS, vformat("Refusing to host: configured session branch is unsafe (len=%d).", session_branch.length()));
			if (dock) {
				dock->update_info("Multiuser: Refused to host - unsafe session branch name. Fix git_session_branch_name.");
			}
			_update_ui();
			return;
		}
		if (!MultiuserEditorActionInterceptor::is_safe_branch_name(merge_target)) {
			_log_cat(LOG_ERROR, LOG_PERMISSIONS, vformat("Refusing to host: configured merge target branch is unsafe (len=%d).", merge_target.length()));
			if (dock) {
				dock->update_info("Multiuser: Refused to host - unsafe merge target branch name. Fix git_merge_target_branch.");
			}
			_update_ui();
			return;
		}
	}
	git_availability = GIT_UNCHECKED;
	_stop();
	_refresh_permissions_from_settings();
	settings_cache.clear();
	{
		List<PropertyInfo> seed_props;
		ProjectSettings::get_singleton()->get_property_list(&seed_props);
		for (const PropertyInfo &pi : seed_props) {
			if (_settings_path_replicated(pi.name)) {
				settings_cache[pi.name] = ProjectSettings::get_singleton()->get(pi.name);
			}
		}
	}
	if (network.host(p_port, p_password) == OK) {
		local_role = "Admin";
		session_authenticated = true;
		_log_connection(vformat("Host session started on port %d", p_port));
		if (s_server_host_pending && s_server_host_port == p_port) {
			s_server_host_pending = false;
		}

		if (dock) {
			const bool require_jwt = bool(MULTIUSER_GET("blazium/multiuser_editor/require_jwt", false));
			dock->set_auth_mode(require_jwt ? MultiuserEditorDock::AUTH_JWT : MultiuserEditorDock::AUTH_HMAC);
		}
		print_line(vformat("[Multiuser Editor] Successfully hosted session on port %d...", p_port));
		if (bool(MULTIUSER_GET("blazium/multiuser_editor/git_auto_branching", false))) {
			if (_can_use_git()) {
				current_session_branch = _resolve_session_branch_name();
				List<String> git_args;
				git_args.push_back("checkout");
				git_args.push_back("-b");
				git_args.push_back(current_session_branch);
				_async_git_execute(git_args);
			}
		}

		_on_network_peer_connected(1);
		_connect_network_signals();
		if (bool(MULTIUSER_GET("blazium/multiuser_editor/file_sync/enabled", true))) {
			_update_filesystem_sync_policy();
			const bool inc_imp = bool(MULTIUSER_GET("blazium/multiuser_editor/file_sync/include_imports", true));
			const PackedStringArray inc = _read_packed_string_array_setting("blazium/multiuser_editor/file_sync/include_patterns", Vector<String>{ "res://*" });
			const PackedStringArray exc = _read_packed_string_array_setting("blazium/multiuser_editor/file_sync/exclude_patterns", Vector<String>{ ".godot/*", ".git/*", "*.tmp", "*.~lock", ".vscode/*", ".idea/*" });
			filesystem_sync.clear_snapshot();
			filesystem_sync.capture_snapshot_from_res(inc_imp, inc, exc);
		}
	} else {
		_log_connection(vformat("Failed to host session on port %d", p_port));
	}

	_update_ui();
}

void MultiuserEditorPlugin::join_session(const String &p_host, int p_port, const String &p_password) {
	{
		String reason;
		if (!_is_inside_loaded_project(&reason)) {
			ERR_PRINT("Multiuser Editor: refusing to join outside a loaded Godot project: " + reason);
			_log_cat(LOG_ERROR, LOG_PERMISSIONS, "join refused: " + reason);
			_update_ui();
			return;
		}
	}
	if (p_port < multiuser_editor::kPortMin || p_port > multiuser_editor::kPortMax) {
		ERR_PRINT(vformat("Multiuser Editor: refusing to join on out-of-range port %d (must be %d-%d).", p_port, multiuser_editor::kPortMin, multiuser_editor::kPortMax));
		_update_ui();
		return;
	}
	if (p_host.is_empty() || p_host.length() > 253) {
		ERR_PRINT("Multiuser Editor: refusing to join: host string is empty or too long.");
		_update_ui();
		return;
	}
	const int password_cap = MAX(16, int(MULTIUSER_GET("blazium/multiuser_editor/limits/multiuser_uri_password_max_chars", 1024)));
	if (p_password.length() > password_cap) {
		ERR_PRINT(vformat("Multiuser Editor: refusing to join: password length %d exceeds cap %d.", p_password.length(), password_cap));
		_update_ui();
		return;
	}
	_stop();
	_refresh_permissions_from_settings();
	settings_cache.clear();
	{
		List<PropertyInfo> seed_props;
		ProjectSettings::get_singleton()->get_property_list(&seed_props);
		for (const PropertyInfo &pi : seed_props) {
			if (_settings_path_replicated(pi.name)) {
				settings_cache[pi.name] = ProjectSettings::get_singleton()->get(pi.name);
			}
		}
	}
	if (network.join(p_host, p_port, p_password) == OK) {
		local_role = "Editor";
		session_authenticated = false;
		_log_connection(vformat("Joining session at %s:%d", p_host, p_port));
		print_line(vformat("[Multiuser Editor] Successfully joining session at %s:%d...", p_host, p_port));
		_connect_network_signals();
		script_sync.clear_all_buffers();
		script_sync.set_sync_pending(true);
	} else {
		_log_connection(vformat("Failed to join session at %s:%d", p_host, p_port));
	}
	_update_ui();
}

String MultiuserEditorPlugin::get_status_text() const {
	switch (network.get_mode()) {
		case MultiuserEditorNetwork::MODE_HOST:
			return TTR("Hosting");
		case MultiuserEditorNetwork::MODE_JOIN:
			return TTR("Joined");
		default:
			return TTR("Offline");
	}
}

bool MultiuserEditorPlugin::is_session_connected() const {
	return network.is_connected() && session_authenticated;
}

bool MultiuserEditorPlugin::is_local_admin() const {
	return local_role == "Admin";
}

bool MultiuserEditorPlugin::is_connected_as_client() const {
	return network.is_connected() && network.get_mode() != MultiuserEditorNetwork::MODE_HOST;
}

void MultiuserEditorPlugin::_refresh_settings_inspector() {
	if (!EditorInterface::get_singleton()) {
		return;
	}
	EditorInspector *inspector = EditorInterface::get_singleton()->get_inspector();
	if (inspector) {
		inspector->update_tree();
	}
}

bool MultiuserEditorPlugin::can_host_sessions() const {
#ifdef MULTIUSER_EDITOR_CLIENT_ONLY
	return false;
#else
	return true;
#endif
}

void MultiuserEditorPlugin::jump_to_peer(const String &p_peer_id) {
	Vector<String> paths = lock_manager.get_peer_selection(p_peer_id);
	if (paths.is_empty()) {
		return;
	}
	Node *root = get_tree() ? get_tree()->get_edited_scene_root() : nullptr;
	if (!root) {
		return;
	}

	Node *target = nullptr;
	for (const String &path : paths) {
		const String clean = lock_manager.clean_path(path);
		if (!MultiuserEditorActionInterceptor::is_safe_node_path(clean)) {
			_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("jump_to_peer skipped unsafe path for peer=%s", p_peer_id));
			continue;
		}
		target = root->get_node_or_null(NodePath(clean));
		if (target) {
			break;
		}
	}

	if (target) {
		EditorInterface::get_singleton()->edit_node(target);
	}
}

void MultiuserEditorPlugin::toggle_follow_peer(const String &p_peer_id) {
	if (followed_peer_id == p_peer_id) {
		followed_peer_id = "";
	} else {
		followed_peer_id = p_peer_id;
	}
}

void MultiuserEditorPlugin::send_chat(const String &p_message) {
	if (!_is_valid_chat_message(p_message)) {
		_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("send_chat rejected: invalid local chat message len=%d", p_message.length()));
		return;
	}
	Dictionary action;
	action["type"] = multiuser_editor::kActionChat;
	Dictionary act_data;
	act_data["message"] = p_message;
	action["data"] = act_data;
	_emit_action(action);
	if (dock) {
		dock->add_chat_message(local_peer_id, p_message);
	}
}

void MultiuserEditorPlugin::request_git_op(const String &p_op) {
	request_git_op_with_branch(p_op, String());
}

void MultiuserEditorPlugin::request_git_op_with_branch(const String &p_op, const String &p_branch) {
	Dictionary act_data;
	act_data["op"] = p_op;
	if (!p_branch.is_empty()) {
		act_data["branch"] = p_branch;
	}
	act_data["remote"] = String(MULTIUSER_GET("blazium/multiuser_editor/git_default_remote", "origin"));

	if (network.get_mode() == MultiuserEditorNetwork::MODE_HOST) {
		GitOpRequest req;
		req.op = p_op;
		req.branch = p_branch;
		req.remote = String(act_data["remote"]);
		req.requester_peer_id = local_peer_id;
		req.requester_net_id = 1;

		String reason;
		if (!bool(MULTIUSER_GET("blazium/multiuser_editor/git_remote_actions_enabled", false))) {
			_gitops_broadcast_result(req, false, -1, String(), "remote_actions_disabled");
			return;
		}
		if (!_gitops_validate(req, reason)) {
			_gitops_broadcast_result(req, false, -1, String(), reason);
			return;
		}
		if (req.op == "force_push") {
			const bool allow = bool(MULTIUSER_GET("blazium/multiuser_editor/allow_editor_force_push", false));
			if (!allow && local_role != "Admin") {
				_gitops_broadcast_result(req, false, -1, String(), "force_push_admin_only");
				return;
			}
		}
		if (!_can_use_git()) {
			_gitops_broadcast_result(req, false, -1, String(), "git_unavailable");
			return;
		}
		if (!_gitops_throttle_ok(1)) {
			_gitops_broadcast_result(req, false, -1, String(), "throttled");
			return;
		}
		_gitops_run_async(req);
	} else {
		Dictionary action = _make_action(multiuser_editor::kActionGitRequest, act_data);
		_emit_action(action);
	}
}

void MultiuserEditorPlugin::request_git_op_commit(const String &p_message) {
	Dictionary act_data;
	act_data["op"] = "commit";
	act_data["message"] = p_message;
	act_data["remote"] = String(MULTIUSER_GET("blazium/multiuser_editor/git_default_remote", "origin"));

	if (network.get_mode() == MultiuserEditorNetwork::MODE_HOST) {
		GitOpRequest req;
		req.op = "commit";
		req.message = p_message;
		req.remote = String(act_data["remote"]);
		req.requester_peer_id = local_peer_id;
		req.requester_net_id = 1;

		String reason;
		if (!bool(MULTIUSER_GET("blazium/multiuser_editor/git_remote_actions_enabled", false))) {
			_gitops_broadcast_result(req, false, -1, String(), "remote_actions_disabled");
			return;
		}
		if (!_gitops_validate(req, reason)) {
			_gitops_broadcast_result(req, false, -1, String(), reason);
			return;
		}
		if (!_can_use_git()) {
			_gitops_broadcast_result(req, false, -1, String(), "git_unavailable");
			return;
		}
		if (!_gitops_throttle_ok(1)) {
			_gitops_broadcast_result(req, false, -1, String(), "throttled");
			return;
		}
		_gitops_run_async(req);
	} else {
		Dictionary action = _make_action(multiuser_editor::kActionGitRequest, act_data);
		_emit_action(action);
	}
}

void MultiuserEditorPlugin::save_session() {
	Dictionary state;
	state["peer_id"] = local_peer_id;
	state["timestamp"] = OS::get_singleton()->get_unix_time();
	Array check_array;
	for (int i = 0; i < checkpoints.size(); i++) {
		check_array.push_back(checkpoints[i]);
	}
	state["checkpoints"] = check_array;

	Array history_array;
	for (int i = 0; i < global_history.size(); i++) {
		history_array.push_back(global_history[i]);
	}
	state["global_history"] = history_array;

	Ref<FileAccess> file = FileAccess::open("res://.multiuser_session", FileAccess::WRITE);
	if (file.is_valid()) {
		file->store_string(JSON::stringify(state, "\t"));
	}
}

void MultiuserEditorPlugin::load_session() {
	if (!FileAccess::exists("res://.multiuser_session")) {
		return;
	}
	Ref<FileAccess> file = FileAccess::open("res://.multiuser_session", FileAccess::READ);
	if (file.is_valid()) {
		String content = file->get_as_text();
		Dictionary state = JSON::parse_string(content);
		if (!state.is_empty()) {
			local_peer_id = state.get("peer_id", local_peer_id);
			const int chk_state_cap = MAX(32, int(MULTIUSER_GET("blazium/multiuser_editor/limits/checkpoint_state_max", 4096)));
			const int gh_cap = MAX(16, int(MULTIUSER_GET("blazium/multiuser_editor/limits/global_history_max", 100)));

			Array check_array = state.get("checkpoints", Array());
			checkpoints.clear();
			int check_truncated = 0;
			for (int i = 0; i < check_array.size(); i++) {
				Variant v = check_array[i];
				if (v.get_type() != Variant::DICTIONARY) {
					continue;
				}
				Dictionary d = v;
				Variant sv = d.get("state", Array());
				if (sv.get_type() == Variant::ARRAY) {
					Array sa = sv;
					if (sa.size() > chk_state_cap) {
						sa.resize(chk_state_cap);
						d["state"] = sa;
						check_truncated++;
					}
				}
				checkpoints.push_back(d);
			}
			if (check_truncated > 0) {
				_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("Restored session: truncated state arrays in %d checkpoint(s) to %d entries.", check_truncated, chk_state_cap));
			}

			Array history_array = state.get("global_history", Array());
			global_history.clear();
			const int hist_take = MIN(history_array.size(), gh_cap);
			const int hist_drop = history_array.size() - hist_take;
			for (int i = history_array.size() - hist_take; i < history_array.size(); i++) {
				global_history.push_back(history_array[i]);
			}
			if (hist_drop > 0) {
				_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("Restored session: global_history truncated by %d entries (cap=%d).", hist_drop, gh_cap));
			}
			_update_ui();
			_log(TTR("Multiuser session state loaded."));
		}
	}
}

void MultiuserEditorPlugin::_handle_uri_join() {
	List<String> args_list = OS::get_singleton()->get_cmdline_args();
	Vector<String> args;
	for (const String &s : args_list) {
		args.push_back(s);
	}

	for (int i = 0; i < args.size(); i++) {
		if (args[i] == "--multiuser-jwt" && i + 1 < args.size()) {
			EditorSettings::get_singleton()->set("blazium/multiuser_editor/client_jwt", args[i + 1]);
			break;
		}
	}

	for (int i = 0; i < args.size(); i++) {
		String arg = args[i];
		if (arg.begins_with("godot://multiuser")) {
			String uri = arg;
			if (uri.find("?") != -1) {
				String query = uri.split("?")[1];
				Vector<String> pairs = query.split("&");
				String host = "127.0.0.1";
				int port = multiuser_editor::kDefaultPort;
				String password = "";
				bool valid = true;
				for (int j = 0; j < pairs.size(); j++) {
					Vector<String> kv = pairs[j].split("=");
					if (kv.size() != 2) {
						continue;
					}
					String key = kv[0];
					String value = kv[1].uri_decode();
					if (key == "host") {
						if (value.is_empty() || value.length() > multiuser_editor::kURIHostMax) {
							_log_cat(LOG_WARN, LOG_NETWORK, "Multiuser URI: invalid host (empty or too long).");
							_record_security_event_kind(MultiuserEditorDock::KIND_AUTH_FAILED, LOG_WARN, LOG_NETWORK, vformat("URI join: invalid host len=%d", value.length()));
							valid = false;
							break;
						}
						bool host_ok = true;
						const char *bad_chars = ";|&<>\"\\";
						for (int k = 0; k < value.length(); k++) {
							const char32_t c = value[k];
							if (c <= 0x20) {
								host_ok = false;
								break;
							}
							for (int b = 0; bad_chars[b] != 0; b++) {
								if (c == (char32_t)bad_chars[b]) {
									host_ok = false;
									break;
								}
							}
							if (!host_ok) {
								break;
							}
						}
						if (!host_ok) {
							_log_cat(LOG_WARN, LOG_NETWORK, "Multiuser URI: host contains invalid characters.");
							_record_security_event_kind(MultiuserEditorDock::KIND_AUTH_FAILED, LOG_WARN, LOG_NETWORK, "URI join: host contains invalid characters");
							valid = false;
							break;
						}
						host = value;
					} else if (key == "port") {
						int parsed = value.to_int();
						if (parsed < multiuser_editor::kPortMin || parsed > multiuser_editor::kPortMax) {
							_log_cat(LOG_WARN, LOG_NETWORK, vformat("Multiuser URI: port out of range (%d). Defaulting to %d.", parsed, multiuser_editor::kDefaultPort));
							_record_security_event_kind(MultiuserEditorDock::KIND_AUTH_FAILED, LOG_WARN, LOG_NETWORK, vformat("URI join: port out of range %d (defaulting)", parsed));
							port = multiuser_editor::kDefaultPort;
						} else {
							port = parsed;
						}
					} else if (key == "password") {
						if (value.length() > multiuser_editor::kAccessListPasswordMax) {
							_log_cat(LOG_WARN, LOG_NETWORK, vformat("Multiuser URI: password too long (>%d chars).", multiuser_editor::kAccessListPasswordMax));
							_record_security_event_kind(MultiuserEditorDock::KIND_AUTH_FAILED, LOG_WARN, LOG_NETWORK, vformat("URI join: password too long len=%d", value.length()));
							valid = false;
							break;
						}
						password = value;
					} else if (key == "jwt") {
						if (!_is_valid_jwt_token(value)) {
							_log_cat(LOG_WARN, LOG_NETWORK, vformat("Multiuser URI: jwt rejected (len=%d).", value.length()));
							_record_security_event_kind(MultiuserEditorDock::KIND_AUTH_FAILED, LOG_WARN, LOG_NETWORK, vformat("URI join: jwt rejected len=%d", value.length()));
							valid = false;
							break;
						}
						EditorSettings::get_singleton()->set("blazium/multiuser_editor/client_jwt", value);
					}
				}
				if (!valid) {
					break;
				}
				join_session(host, port, password);
				_log(vformat(TTR("Auto-joining session from URI: %s:%d"), host, port));
			}
			break;
		}
	}
}

void MultiuserEditorPlugin::_handle_cli_multiuser_join() {
	List<String> args_list = OS::get_singleton()->get_cmdline_args();
	Vector<String> args;
	for (const String &s : args_list) {
		args.push_back(s);
	}

	if (args.has("--multiuser-server")) {
		return;
	}

	if (args.has("--multiuser-debug")) {
		EditorSettings::get_singleton()->set("blazium/multiuser_editor/enable_debug_logging", true);
		EditorSettings::get_singleton()->set("blazium/multiuser_editor/logging/log_network", true);
		_log_connection("Debug logging enabled via --multiuser-debug");
	}

	for (int i = 0; i < args.size(); i++) {
		if (args[i] == "--multiuser-jwt" && i + 1 < args.size()) {
			EditorSettings::get_singleton()->set("blazium/multiuser_editor/client_jwt", args[i + 1]);
		}
	}

	int host_idx = args.find("--multiuser-host");
	if (host_idx != -1 && host_idx + 1 < args.size()) {
		EditorSettings::get_singleton()->set("blazium/multiuser_editor/default_host", args[host_idx + 1]);
	}

	int port_idx = args.find("--multiuser-port");
	if (port_idx != -1 && port_idx + 1 < args.size()) {
		const int port = args[port_idx + 1].to_int();
		if (port >= multiuser_editor::kPortMin && port <= multiuser_editor::kPortMax) {
			EditorSettings::get_singleton()->set("blazium/multiuser_editor/default_port", port);
		}
	}

	if (args.has("--multiuser-join")) {
		EditorSettings::get_singleton()->set("blazium/multiuser_editor/enabled", true);
		if (!s_cli_join_pending) {
			s_cli_join_pending = true;
			s_cli_join_after_usec = OS::get_singleton()->get_ticks_usec() + 5'000'000;
			const String host = String(MULTIUSER_GET("blazium/multiuser_editor/default_host", "127.0.0.1"));
			const int port = int(MULTIUSER_GET("blazium/multiuser_editor/default_port", multiuser_editor::kDefaultPort));
			_log_connection(vformat("CLI auto-join scheduled for %s:%d (poll deferred, 5s)", host, port));
		}
	}
}

void MultiuserEditorPlugin::_poll_cli_auto_join() {
	if (singleton != this) {
		return;
	}
	if (!s_cli_join_pending) {
		return;
	}
	if (OS::get_singleton()->get_ticks_usec() < s_cli_join_after_usec) {
		return;
	}
	String reason;
	if (!_is_inside_loaded_project(&reason)) {
		return;
	}
	EditorNode *editor_node = EditorNode::get_singleton();
	if (!editor_node || !editor_node->is_editor_ready()) {
		s_cli_editor_ready_since_usec = 0;
		return;
	}
	if (s_cli_editor_ready_since_usec == 0) {
		s_cli_editor_ready_since_usec = OS::get_singleton()->get_ticks_usec();
		_log_connection("CLI auto-join waiting for editor settle (10s after ready)");
		return;
	}
	if (OS::get_singleton()->get_ticks_usec() - s_cli_editor_ready_since_usec < 10'000'000) {
		return;
	}
	if (network.get_mode() != MultiuserEditorNetwork::MODE_OFF) {
		return;
	}
	const String host = String(MULTIUSER_GET("blazium/multiuser_editor/default_host", "127.0.0.1"));
	const int port = int(MULTIUSER_GET("blazium/multiuser_editor/default_port", multiuser_editor::kDefaultPort));
	_log_connection(vformat("CLI auto-join executing for %s:%d", host, port));
	join_session(host, port, "");
}

void MultiuserEditorPlugin::_poll_server_auto_host() {
	if (singleton != this) {
		return;
	}
	if (!s_server_host_pending) {
		return;
	}
	if (OS::get_singleton()->get_ticks_usec() < s_server_host_after_usec) {
		return;
	}
	String reason;
	if (!_is_inside_loaded_project(&reason)) {
		return;
	}
	EditorNode *editor_node = EditorNode::get_singleton();
	if (!editor_node || !editor_node->is_editor_ready()) {
		s_server_host_ready_since_usec = 0;
		return;
	}
	if (s_server_host_ready_since_usec == 0) {
		s_server_host_ready_since_usec = OS::get_singleton()->get_ticks_usec();
		_log_connection("Server auto-host waiting for editor settle (5s after ready)");
		return;
	}
	if (OS::get_singleton()->get_ticks_usec() - s_server_host_ready_since_usec < 5'000'000) {
		return;
	}
	if (network.get_mode() != MultiuserEditorNetwork::MODE_OFF) {
		return;
	}
	_log_connection(vformat("Server auto-host executing on port %d", s_server_host_port));
	host_session(s_server_host_port, s_server_host_password);
}

void MultiuserEditorPlugin::request_magic_repair() {
	if (network.get_mode() == MultiuserEditorNetwork::MODE_JOIN) {
		uint64_t local_hash = action_interceptor.generate_scene_hash(get_tree() ? get_tree()->get_edited_scene_root() : nullptr);
		Dictionary act_data;
		act_data["hash"] = local_hash;
		_emit_action(_make_action(multiuser_editor::kActionMagicRepairRequest, act_data));
	}
}

void MultiuserEditorPlugin::_wipe_edited_scene() {
	Node *root = get_tree() ? get_tree()->get_edited_scene_root() : nullptr;
	if (root) {
		suppress_scene_events = true;
		TypedArray<Node> children = root->get_children();
		for (int i = 0; i < children.size(); i++) {
			Node *c = Object::cast_to<Node>(children[i]);
			if (c) {
				root->remove_child(c);
				memdelete(c);
			}
		}
		suppress_scene_events = false;
		_log(TTR("Magic Repair: Scene tree wiped immediately for collision-free reconstruction."));
	}
}

void MultiuserEditorPlugin::trigger_autowork() {
	if (network.is_connected()) {
		_emit_action(_make_action(multiuser_editor::kActionAutoworkTrigger, Dictionary()));
	}

	if (bool(MULTIUSER_GET("blazium/multiuser_editor/allow_remote_autowork", false))) {
		send_chat(vformat("Peer %s has started remote Automated Testing...", local_peer_id));

		Autowork *aw = memnew(Autowork);
		EditorInterface::get_singleton()->get_base_control()->add_child(aw);
		aw->add_directory("res://");
		aw->run_tests();

		int passes = aw->get_pass_count();
		int fails = aw->get_fail_count();
		int pendings = aw->get_pending_count();

		String results = vformat("Autowork Results [%s]: Passed: %d | Failed: %d | Pending: %d", local_peer_id, passes, fails, pendings);
		send_chat(results);

		aw->queue_free();
	}
}

void MultiuserEditorPlugin::kick_peer(const String &p_peer_id) {
	if (network.get_mode() != MultiuserEditorNetwork::MODE_HOST) {
		_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("kick_peer denied: not host (peer='%s')", p_peer_id));
		_record_security_event_kind(MultiuserEditorDock::KIND_ADMIN_KICK, LOG_WARN, LOG_PERMISSIONS, vformat("kick_peer denied (not host): peer='%s'", p_peer_id));
		return;
	}
	int net_id = network.get_net_id(p_peer_id);
	if (net_id != 0 && net_id != 1) {
		_log_cat(LOG_INFO, LOG_PERMISSIONS, vformat("kick_peer: disconnecting peer='%s' net_id=%d", p_peer_id, net_id));
		_record_security_event_kind(MultiuserEditorDock::KIND_ADMIN_KICK, LOG_INFO, LOG_PERMISSIONS, vformat("kick_peer: peer='%s' net_id=%d", p_peer_id, net_id));
		network.disconnect_peer(net_id);
	}
}

void MultiuserEditorPlugin::create_checkpoint(const String &p_name) {
	Dictionary checkpoint;
	checkpoint["name"] = p_name;
	checkpoint["timestamp"] = OS::get_singleton()->get_unix_time();

	Vector<Dictionary> actions;
	action_interceptor.build_initial_state_actions(actions);
	const int chk_state_cap = MAX(32, int(MULTIUSER_GET("blazium/multiuser_editor/limits/checkpoint_state_max", 4096)));
	const int take = MIN(actions.size(), chk_state_cap);
	Array actions_array;
	for (int i = 0; i < take; i++) {
		actions_array.push_back(actions[i]);
	}
	if (actions.size() > chk_state_cap) {
		_log_cat(LOG_WARN, LOG_PERMISSIONS, vformat("create_checkpoint: state truncated from %d to %d entries.", actions.size(), chk_state_cap));
	}
	checkpoint["state"] = actions_array;

	checkpoints.push_back(checkpoint);
}

void MultiuserEditorPlugin::load_checkpoint(int p_index) {
	if (p_index < 0 || p_index >= checkpoints.size()) {
		return;
	}
	Dictionary checkpoint = checkpoints[p_index];
	Array actions_array = checkpoint["state"];
	Vector<Dictionary> actions;
	for (int i = 0; i < actions_array.size(); i++) {
		actions.push_back(actions_array[i]);
	}

	for (const Dictionary &action : actions) {
		_emit_action(action);
		action_interceptor.apply_remote_action(action);
	}
}

void MultiuserEditorPlugin::forward_3d_draw_over_viewport(Control *p_overlay) {
	if (!is_session_connected()) {
		return;
	}

	const HashMap<String, HashSet<String>> &peer_locks = lock_manager.get_peer_locks();
	Node *root = EditorInterface::get_singleton()->get_edited_scene_root();
	if (!root) {
		return;
	}

	for (const KeyValue<String, HashSet<String>> &E : peer_locks) {
		String peer_id = E.key;
		Color color = Color::from_hsv(float(peer_id.hash() % 360) / 360.0, 0.75, 0.95, 0.5);

		for (const String &path : E.value) {
			Node *node = root->get_node_or_null(NodePath(lock_manager.clean_path(path)));
			if (node && node->is_class("Node3D")) {
				p_overlay->draw_string(p_overlay->get_theme_font(SNAME("font")), Point2(20, 20), "Remote 3D Editor Active: " + peer_id, HORIZONTAL_ALIGNMENT_LEFT, -1, 14, color);
			}
		}
	}
}

void MultiuserEditorPlugin::forward_canvas_draw_over_viewport(Control *p_overlay) {
	if (!is_session_connected()) {
		return;
	}

	const HashMap<String, HashSet<String>> &peer_locks = lock_manager.get_peer_locks();
	Node *root = EditorInterface::get_singleton()->get_edited_scene_root();
	if (!root) {
		return;
	}

	for (const KeyValue<String, HashSet<String>> &E : peer_locks) {
		String peer_id = E.key;
		Color color = Color::from_hsv(float(peer_id.hash() % 360) / 360.0, 0.75, 0.95, 0.4);

		for (const String &path : E.value) {
			Node *node = root->get_node_or_null(NodePath(lock_manager.clean_path(path)));

			if (node && (node->is_class("Node2D") || node->is_class("Control"))) {
				p_overlay->draw_string(p_overlay->get_theme_font(SNAME("font")), Point2(20, 40), "Remote 2D Editor Active: " + peer_id, HORIZONTAL_ALIGNMENT_LEFT, -1, 14, color);
			}
		}
	}
}

#endif
