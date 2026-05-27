/**************************************************************************/
/*  multiuser_editor_plugin.h                                             */
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

#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "editor/plugins/editor_plugin.h"
#include "multiuser_editor_access_list.h"
#include "multiuser_editor_action_interceptor.h"
#include "multiuser_editor_filesystem_sync.h"
#include "multiuser_editor_ghost_cursor_overlay.h"
#include "multiuser_editor_network.h"
#include "multiuser_editor_permissions.h"
#include "multiuser_editor_script_sync.h"
#include "multiuser_editor_settings_inspector_plugin.h"

#ifndef MULTIUSER_GET
#define MULTIUSER_GET(m_var, m_default) \
	((EditorSettings::get_singleton() && EditorSettings::get_singleton()->has_setting(m_var)) ? EditorSettings::get_singleton()->get(m_var) : Variant(m_default))
#endif

class EditorSelection;
class EditorUndoRedoManager;
class Timer;

class MultiuserEditorPlugin;

class Label;
class MultiuserEditorDock;

class MultiuserEditorPlugin : public EditorPlugin {
	GDCLASS(MultiuserEditorPlugin, EditorPlugin);

public:
	struct ChallengeRec {
		String challenge;
		uint64_t issued_msec = 0;
	};

private:
	static MultiuserEditorPlugin *singleton;
	static bool s_cli_join_pending;
	static uint64_t s_cli_join_after_usec;
	static uint64_t s_cli_editor_ready_since_usec;
	static bool s_server_host_pending;
	static int s_server_host_port;
	static String s_server_host_password;
	static uint64_t s_server_host_after_usec;
	static uint64_t s_server_host_ready_since_usec;

	MultiuserEditorNetwork network;
	MultiuserEditorLockManager lock_manager;
	MultiuserEditorActionInterceptor action_interceptor;
	MultiuserEditorScriptSync script_sync;
	MultiuserEditorFilesystemSync filesystem_sync;
	Ref<MultiuserEditorPermissions> permissions;
	MultiuserEditorGhostCursorOverlay *ghost_overlay = nullptr;
	Timer *poll_timer = nullptr;
	MultiuserEditorDock *dock = nullptr;
	Label *status_label = nullptr;
	Ref<MultiuserEditorSettingsInspectorPlugin> inspector_plugin;
	EditorSelection *selection = nullptr;
	EditorUndoRedoManager *undo_redo = nullptr;
	String local_peer_id;
	String local_role = "Editor";
	String followed_peer_id;
	String current_session_branch;

	HashMap<int, ChallengeRec> _pending_challenges;
	HashMap<String, Variant> settings_cache;

	Vector<Dictionary> checkpoints;
	Vector<Dictionary> global_history;
	bool session_authenticated = false;
	bool suppress_scene_events = false;
	bool suppress_fs_broadcast = false;
	Timer *fs_debounce_timer = nullptr;
	double last_telemetry_broadcast = 0.0;
	double last_script_poll = 0.0;
	double last_cursor_broadcast = 0.0;
	bool was_playing = false;

	void _emit_action(Dictionary p_action);
	void _emit_action_to_server(const Dictionary &p_action);
	void _emit_file_sync_actions(const Vector<Dictionary> &p_actions);
	void _update_filesystem_sync_policy();
	void _schedule_filesystem_diff();
	void _process_filesystem_diff();
	void _send_project_snapshot_to_peer(int p_target_net_id);
	void _route_action(int p_sender_net_id, const Dictionary &p_action);
	void _send_initial_state(int p_target_net_id);
	void _poll_runtime();

	void _issue_pending_challenge(int p_net_id);
	void _expire_stale_pending_challenges();
	bool _consume_pending_challenge(int p_net_id, String &r_challenge);
	void _update_context();
	void _update_ui();
	void _setup_dock();
	void _teardown_dock();
	void _setup_status_indicator();
	void _connect_network_signals();
	void _disconnect_network_signals();

public:
	enum LogLevel {
		LOG_ERROR = 0,
		LOG_WARN = 1,
		LOG_INFO = 2,
		LOG_DEBUG = 3,
	};
	enum LogCategory {
		LOG_GENERAL,
		LOG_REPLICATION,
		LOG_FILESYSTEM,
		LOG_CRDT,
		LOG_NETWORK,
		LOG_PERMISSIONS,
	};

private:
	void _log(const String &p_message) const;
	void _log_connection(const String &p_msg) const;
	void _log_cat(LogLevel p_level, LogCategory p_cat, const String &p_msg) const;
	void _record_security_event(LogLevel p_level, LogCategory p_cat, const String &p_msg) const;

	void _record_security_event_kind(int p_kind, LogLevel p_level, LogCategory p_cat, const String &p_msg) const;
	static void _security_sink_thunk(void *p_user, int p_kind, int p_level, int p_category, const String &p_message);
	void _bump_drop_counter(int p_by = 1) const;
	void _bump_throttle_counter(int p_by = 1) const;
	void _refresh_permissions_from_settings();

	void _run_known_action_self_check() const;
	bool _settings_path_replicated(const String &p_path) const;
	void _send_project_settings_snapshot(int p_target_net_id);
	void _apply_project_setting_value(const String &p_name, const Variant &p_value);
	String _format_action_summary(const Dictionary &p_action) const;
	bool _is_inside_loaded_project(String *r_reason = nullptr) const;
	bool _is_valid_peer_id_field(const String &p_value) const;
	bool _is_valid_role_field(const String &p_value) const;
	bool _is_valid_chat_message(const String &p_value) const;
	bool _is_valid_hex_token(const String &p_value, int p_expect_len) const;
	bool _is_valid_jwt_token(const String &p_value) const;

	static bool _const_time_eq(const String &p_a, const String &p_b);

	bool _packet_is_sane(const Dictionary &p_action, String *r_reason = nullptr) const;
	bool _is_safe_simple_value(const Variant &p_value, int p_remaining_depth) const;

	bool _is_access_list_path(const String &p_path) const;

	String _extract_target_path_for_protection(const String &p_type, const Dictionary &p_action, const Dictionary &p_data, const String &p_field) const;

	bool _is_path_protected_for_action(const String &p_path) const;

public:
	struct JWTValidationConfig {
		String algorithms_csv = "HS256";
		String expected_audience;
		String expected_issuer;
		double leeway_sec = 30.0;
		int max_token_age_sec = 3600;
		bool require_jti = false;
		int jti_cache_max = 4096;
	};

	struct JWTValidationResult {
		bool valid = false;
		String reason;
		String role;
		String jti;
	};

	static JWTValidationResult validate_jwt_static(const String &p_jwt, const String &p_secret, const JWTValidationConfig &p_cfg);

	JWTValidationResult _validate_jwt_full(const String &p_jwt, const String &p_secret) const;
	void _jwt_remember_jti(const String &p_jti);
	bool _jwt_jti_seen(const String &p_jti) const;
	void _jwt_jti_evict_if_needed();
	String _jwt_jti_fingerprint(const String &p_jti) const;

private:
	JWTValidationConfig _read_jwt_validation_config() const;
	CodeEdit *_find_active_code_edit() const;
	String _find_script_path_for_code_edit(CodeEdit *p_code_edit) const;
	void _on_selection_changed();
	void _on_node_added(Node *p_node);
	void _on_node_removed(Node *p_node);
	void _on_network_peer_connected(int p_net_id);
	void _on_network_peer_disconnected(int p_net_id);
	void _on_inspector_property_changed(Object *p_undo_redo, Object *p_modified_object, const String &p_property, const Variant &p_new_value);
	void _on_project_settings_changed();
	void _on_editor_settings_changed();
	void _refresh_hot_settings_cache();
	void _on_scene_saved(const String &p_filepath);
	void _on_filesystem_changed();
	void _on_resources_reimported(const PackedStringArray &p_paths);
	void _on_sources_changed(bool p_exist);
	void _on_fs_debounce_timeout();
	void _wipe_edited_scene();
	void _handle_uri_join();
	void _handle_cli_multiuser_join();
	void _poll_cli_auto_join();
	void _poll_server_auto_host();
	void _stop();
	void _async_git_execute(const List<String> &p_args);

	enum GitAvailability {
		GIT_UNCHECKED,
		GIT_OK,
		GIT_UNAVAILABLE,
	};
	GitAvailability git_availability = GIT_UNCHECKED;
	String git_unavailable_reason;
	bool _can_use_git();

	String _resolve_session_branch_name();
	String _resolve_merge_target_branch();
	bool last_enabled_state = false;

	void _refresh_settings_inspector();
	String _last_role_for_inspector;
	bool _last_session_connected_for_inspector = false;

	mutable Vector<String> _replicated_prefixes_cache;
	mutable String _replicated_prefixes_cache_source;
	int _cached_max_packet_size_mb = -1;
	int _cached_packets_per_poll_max = -1;
	int _cached_max_clients = -1;
	int _cached_crdt_atoms_max = -1;
	int _cached_script_attach_max_bytes = -1;
	int _cached_security_events_max = -1;
	int _cached_jwt_jti_sweep_ms = -1;

	Ref<MultiuserEditorAccessList> _access_list;
	String _cached_access_list_canonical_path;
	void _reload_access_list();
	String _resolve_access_list_path() const;
	String _resolve_implicit_role_for_dock_password() const;
	void _refresh_access_list_protection();

public:
	void reload_access_list();

private:
	struct InboundBucket {
		double tokens = 0.0;
		uint64_t last_refill_msec = 0;
	};
	mutable HashMap<int, InboundBucket> _inbound_buckets;

public:
	struct SecurityEventSnapshot {
		uint64_t when_msec = 0;
		int severity = 0;
		int category = 0;

		int kind = 0;
		String message;
	};
	Vector<SecurityEventSnapshot> get_recent_security_events_snapshot(int p_max = 32) const;

private:
	HashMap<String, uint64_t> _unknown_relay_log_dedupe;
	HashMap<int, uint64_t> _project_setting_last_msec;
	HashMap<int, uint64_t> _cursor_update_last_msec;
	HashMap<int, uint64_t> _telemetry_last_msec;
	HashMap<int, uint64_t> _chat_last_msec;

	struct RelayBucket {
		double tokens = 0.0;
		uint64_t last_refill_msec = 0;
	};
	mutable HashMap<int, RelayBucket> _relay_buckets;

	mutable HashMap<String, uint64_t> _jwt_jti_cache;
	mutable List<String> _jwt_jti_lru;
	mutable uint64_t _jwt_last_jti_sweep_msec = 0;

	bool _hot_settings_dirty = true;
	bool _hot_enabled = false;
	bool _hot_cursor_sync_enabled = true;
	bool _hot_show_remote_cursors = true;
	bool _hot_sync_scene_changes = true;
	bool _hot_sync_scripts = true;
	double _hot_telemetry_interval = 2.0;
	double _hot_script_poll_interval = 0.25;
	double _hot_lock_timeout_sec = 30.0;

public:
	struct GitOpRequest {
		String op;
		String branch;
		String remote;
		String message;
		String requester_peer_id;
		int requester_net_id = 0;
	};

	struct GitOpResult {
		GitOpRequest req;
		bool success = false;
		int exit_code = 0;
		String output;
		String reason;
	};

	struct GitOpArgv {
		List<String> args1;
		List<String> args2;
		bool has_second = false;
		bool prepend_current_branch = false;
		bool unknown_op = false;
	};

	static GitOpArgv _gitops_build_argv(const GitOpRequest &p_req);

	class GitSharedState : public RefCounted {
	public:
		Mutex mutex;
		List<GitOpResult> pending_results;
		int inflight = 0;
		int queue_cap = 64;
		int output_cap = 8192;
	};

private:
	HashMap<int, uint64_t> _git_throttle_last_msec;
	Ref<GitSharedState> _git_shared;
	bool _gitops_parse(const Dictionary &p_action, int p_sender_net_id, const String &p_peer_id, GitOpRequest &r_out, String &r_reason) const;
	bool _gitops_validate(const GitOpRequest &p_req, String &r_reason) const;
	bool _gitops_throttle_ok(int p_sender_net_id);
	void _gitops_run_async(const GitOpRequest &p_req);
	void _gitops_broadcast_result(const GitOpRequest &p_req, bool p_success, int p_exit_code, const String &p_output, const String &p_reason);
	String _gitops_resolve_current_branch_blocking();
	void _gitops_drain_pending_results();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	static MultiuserEditorPlugin *get_singleton();

	virtual String get_plugin_name() const override { return "MultiuserEditor"; }
	virtual bool has_main_screen() const override { return false; }

	void host_from_settings();
	void join_from_settings();
	void stop_session();
	bool is_session_connected() const;
	bool can_host_sessions() const;
	void host_session(int p_port, const String &p_password = "");
	void join_session(const String &p_host, int p_port, const String &p_password = "");
	String get_status_text() const;
	String get_local_peer_id() const { return local_peer_id; }
	void jump_to_peer(const String &p_peer_id);

	void request_git_op(const String &p_op);
	void request_git_op_with_branch(const String &p_op, const String &p_branch);
	void request_git_op_commit(const String &p_message);

	bool is_local_admin() const;
	bool is_connected_as_client() const;

	void toggle_follow_peer(const String &p_peer_id);
	void send_chat(const String &p_message);
	void kick_peer(const String &p_peer_id);
	void create_checkpoint(const String &p_name);
	void load_checkpoint(int p_index);
	void save_session();
	void load_session();
	void trigger_autowork();
	void request_magic_repair();

	static Dictionary validate_jwt_static_d(const String &p_jwt, const String &p_secret, const Dictionary &p_cfg);
	Array get_recent_security_events_snapshot_array(int p_max = 32) const;

	virtual void forward_3d_draw_over_viewport(Control *p_overlay) override;

	virtual void forward_canvas_draw_over_viewport(Control *p_overlay) override;

	MultiuserEditorPlugin();

	~MultiuserEditorPlugin();
};

#endif
