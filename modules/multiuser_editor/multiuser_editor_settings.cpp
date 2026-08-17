/**************************************************************************/
/*  multiuser_editor_settings.cpp                                         */
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

#include "multiuser_editor_settings.h"

#include "multiuser_editor_constants.h"

#include "editor/settings/editor_settings.h"

static const double MULTIUSER_POLL_INTERVAL_SEC = 0.05;
static const double MULTIUSER_CURSOR_INTERVAL_SEC = 0.08;
static const double MULTIUSER_SYNC_PENDING_TIMEOUT_SEC = 3.0;

static bool s_editor_settings_registered = false;
static bool s_registering_editor_settings = false;

bool multiuser_editor_are_editor_settings_registered() {
	return s_editor_settings_registered;
}

bool multiuser_editor_is_registering_editor_settings() {
	return s_registering_editor_settings;
}

void multiuser_editor_register_editor_settings() {
	if (s_editor_settings_registered) {
		return;
	}
	if (!EditorSettings::get_singleton()) {
		return;
	}

	s_registering_editor_settings = true;

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
		fs_exc.push_back(".blazium/*");
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

	s_registering_editor_settings = false;
	s_editor_settings_registered = true;
}

#endif // TOOLS_ENABLED
