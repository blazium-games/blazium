/**************************************************************************/
/*  multiuser_editor_settings_ui.cpp                                      */
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

#include "multiuser_editor_settings_ui.h"

#include "core/io/json.h"
#include "core/io/marshalls.h"
#include "core/os/time.h"
#include "editor/editor_settings.h"
#include "modules/jwttool/jwt.h"
#include "multiuser_editor_access_list.h"
#include "multiuser_editor_dock.h"
#include "multiuser_editor_plugin.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/file_dialog.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/spin_box.h"
#include "scene/gui/tab_container.h"
#include "scene/gui/tree.h"

void MultiuserEditorSettingsUI::_bind_methods() {}

MultiuserEditorSettingsUI::MultiuserEditorSettingsUI() {
	VBoxContainer *root = memnew(VBoxContainer);
	add_child(root);
	status_label = memnew(Label);
	root->add_child(status_label);

	tabs = memnew(TabContainer);
	tabs->set_custom_minimum_size(Size2(0, 360));
	root->add_child(tabs);

	{
		VBoxContainer *conn = memnew(VBoxContainer);
		conn->set_name(TTR("Connection"));
		tabs->add_child(conn);
		_build_connection_tab(conn);
	}

	{
		VBoxContainer *gen = memnew(VBoxContainer);
		gen->set_name(TTR("General"));
		tabs->add_child(gen);
		_build_general_tab(gen);
	}

	{
		VBoxContainer *syn = memnew(VBoxContainer);
		syn->set_name(TTR("Sync"));
		tabs->add_child(syn);
		_build_sync_tab(syn);
	}

	{
		VBoxContainer *net = memnew(VBoxContainer);
		net->set_name(TTR("Network"));
		tabs->add_child(net);
		_build_network_tab(net);
	}

	{
		VBoxContainer *sec = memnew(VBoxContainer);
		sec->set_name(TTR("Security"));
		tabs->add_child(sec);
		_build_security_tab(sec);
	}

	{
		VBoxContainer *thr = memnew(VBoxContainer);
		thr->set_name(TTR("Throttling"));
		tabs->add_child(thr);
		_build_throttling_tab(thr);
	}

	{
		VBoxContainer *fs = memnew(VBoxContainer);
		fs->set_name(TTR("File Sync"));
		tabs->add_child(fs);
		_build_filesync_tab(fs);
	}

	{
		VBoxContainer *git = memnew(VBoxContainer);
		git->set_name(TTR("Git"));
		tabs->add_child(git);
		_build_git_tab(git);
	}

	{
		VBoxContainer *perm = memnew(VBoxContainer);
		perm->set_name(TTR("Permissions"));
		tabs->add_child(perm);
		_build_permissions_tab(perm);
	}

	{
		VBoxContainer *al = memnew(VBoxContainer);
		al->set_name(TTR("Access List"));
		tabs->add_child(al);
		_build_access_list_tab(al);
	}

	{
		VBoxContainer *log_tab = memnew(VBoxContainer);
		log_tab->set_name(TTR("Logging"));
		tabs->add_child(log_tab);
		_build_logging_tab(log_tab);
	}

	{
		VBoxContainer *int_tab = memnew(VBoxContainer);
		int_tab->set_name(TTR("Intervals"));
		tabs->add_child(int_tab);
		_build_intervals_tab(int_tab);
	}

	{
		VBoxContainer *diag = memnew(VBoxContainer);
		diag->set_name(TTR("Diagnostics"));
		tabs->add_child(diag);
		_build_diagnostics_tab(diag);
	}

	_refresh();
}

void MultiuserEditorSettingsUI::_build_connection_tab(VBoxContainer *p_root) {
	GridContainer *grid = memnew(GridContainer);
	grid->set_columns(2);
	p_root->add_child(grid);

	Label *host_label = memnew(Label);
	host_label->set_text(TTR("Host:"));
	grid->add_child(host_label);
	host_input = memnew(LineEdit);
	if (EditorSettings::get_singleton() && EditorSettings::get_singleton()->has_setting("blazium/multiuser_editor/default_host")) {
		host_input->set_text(MULTIUSER_GET("blazium/multiuser_editor/default_host", "127.0.0.1"));
	} else {
		host_input->set_text("127.0.0.1");
	}
	grid->add_child(host_input);

	_add_spin_field(grid, TTR("Default Port"), "blazium/multiuser_editor/default_port", 7654, 1024, 65535);
	if (_spin_inputs.has("blazium/multiuser_editor/default_port")) {
		port_input = _spin_inputs["blazium/multiuser_editor/default_port"];
	}
	_add_check_field(grid, TTR("Auto Host"), "blazium/multiuser_editor/auto_host", false);

	Label *pw_label = memnew(Label);
	pw_label->set_text(TTR("Password:"));
	grid->add_child(pw_label);
	connection_password_input = memnew(LineEdit);
	connection_password_input->set_secret(true);
	connection_password_input->set_h_size_flags(SIZE_EXPAND_FILL);
	connection_password_input->set_placeholder(TTR("Session password (used as 'default' codename)"));
	grid->add_child(connection_password_input);

	HBoxContainer *buttons = memnew(HBoxContainer);
	p_root->add_child(buttons);
	start_button = memnew(Button);
	start_button->set_text(TTR("Start"));
	start_button->connect(SceneStringName(pressed), callable_mp(this, &MultiuserEditorSettingsUI::_start_pressed));
	buttons->add_child(start_button);
	stop_button = memnew(Button);
	stop_button->set_text(TTR("Stop"));
	stop_button->connect(SceneStringName(pressed), callable_mp(this, &MultiuserEditorSettingsUI::_stop_pressed));
	buttons->add_child(stop_button);
}

void MultiuserEditorSettingsUI::_build_security_tab(VBoxContainer *p_root) {
	GridContainer *grid = memnew(GridContainer);
	grid->set_columns(2);
	p_root->add_child(grid);

	_add_check_field(grid, TTR("Require JWT"), "blazium/multiuser_editor/require_jwt", false);
	_add_line_field(grid, TTR("JWT Secret Key"), "blazium/multiuser_editor/jwt_secret_key", "", true);
	_add_line_field(grid, TTR("Client JWT"), "blazium/multiuser_editor/client_jwt", "", true);
	_add_line_field(grid, TTR("Algorithms Allowed"), "blazium/multiuser_editor/jwt/algorithms_allowed", "HS256");
	_add_line_field(grid, TTR("Expected Audience"), "blazium/multiuser_editor/jwt/expected_audience", "");
	_add_line_field(grid, TTR("Expected Issuer"), "blazium/multiuser_editor/jwt/expected_issuer", "");
	_add_spin_field(grid, TTR("Leeway (sec)"), "blazium/multiuser_editor/jwt/leeway_sec", 30.0, 0.0, 3600.0);
	_add_spin_field(grid, TTR("Max Token Age (sec)"), "blazium/multiuser_editor/jwt/max_token_age_sec", 3600.0, 60.0, 604800.0);
	_add_check_field(grid, TTR("Require JTI"), "blazium/multiuser_editor/jwt/require_jti", false);
	_add_spin_field(grid, TTR("JTI Cache Max"), "blazium/multiuser_editor/jwt/jti_cache_max", 4096.0, 16.0, 1048576.0);

	mint_test_token_btn = memnew(Button);
	mint_test_token_btn->set_text(TTR("Mint Test Token (HS256)..."));
	mint_test_token_btn->connect(SceneStringName(pressed), callable_mp(this, &MultiuserEditorSettingsUI::_on_mint_test_token_pressed));
	p_root->add_child(mint_test_token_btn);

	mint_dialog = memnew(AcceptDialog);
	mint_dialog->set_title(TTR("Mint Test JWT (HS256)"));
	{
		VBoxContainer *box = memnew(VBoxContainer);
		mint_dialog->add_child(box);
		Label *info = memnew(Label);
		info->set_text(TTR("Generates a short-lived HS256 JWT signed with the configured secret. Token is copied to the clipboard."));
		info->set_autowrap_mode(TextServer::AUTOWRAP_WORD);
		box->add_child(info);
		HBoxContainer *role_row = memnew(HBoxContainer);
		box->add_child(role_row);
		Label *role_lbl = memnew(Label);
		role_lbl->set_text(TTR("Role:"));
		role_row->add_child(role_lbl);
		mint_role_input = memnew(LineEdit);
		mint_role_input->set_text("Editor");
		role_row->add_child(mint_role_input);
		HBoxContainer *exp_row = memnew(HBoxContainer);
		box->add_child(exp_row);
		Label *exp_lbl = memnew(Label);
		exp_lbl->set_text(TTR("Expiry (sec):"));
		exp_row->add_child(exp_lbl);
		mint_expiry_input = memnew(SpinBox);
		mint_expiry_input->set_min(60);
		mint_expiry_input->set_max(86400);
		mint_expiry_input->set_value(900);
		exp_row->add_child(mint_expiry_input);
	}
	mint_dialog->connect(SceneStringName(confirmed), callable_mp(this, &MultiuserEditorSettingsUI::_on_mint_dialog_confirmed));
	add_child(mint_dialog);
}

void MultiuserEditorSettingsUI::_build_throttling_tab(VBoxContainer *p_root) {
	GridContainer *grid = memnew(GridContainer);
	grid->set_columns(2);
	p_root->add_child(grid);

	_add_spin_field(grid, TTR("Cursor Update Interval (ms)"), "blazium/multiuser_editor/limits/cursor_update_min_interval_ms", 50, 0, 5000);
	_add_spin_field(grid, TTR("Telemetry Interval (ms)"), "blazium/multiuser_editor/limits/telemetry_min_interval_ms", 250, 0, 60000);
	_add_spin_field(grid, TTR("Chat Interval (ms)"), "blazium/multiuser_editor/limits/chat_min_interval_ms", 250, 0, 60000);
	_add_spin_field(grid, TTR("Chat History Max"), "blazium/multiuser_editor/limits/chat_history_max", 256, 16, 65536);
	_add_spin_field(grid, TTR("Global History Max"), "blazium/multiuser_editor/limits/global_history_max", 100, 16, 65536);
	_add_spin_field(grid, TTR("Checkpoint State Max"), "blazium/multiuser_editor/limits/checkpoint_state_max", 4096, 32, 65536);
	_add_spin_field(grid, TTR("script_attach Max Bytes"), "blazium/multiuser_editor/limits/script_attach_max_bytes", 4 * 1024 * 1024, 1024, 64 * 1024 * 1024);
	_add_spin_field(grid, TTR("CRDT Live Atoms Max"), "blazium/multiuser_editor/limits/crdt_live_atoms_max", 1000000, 1024, 10000000);
	_add_spin_field(grid, TTR("Packets / Poll Max"), "blazium/multiuser_editor/limits/packets_per_poll_max", 256, 16, 65536);
	_add_spin_field(grid, TTR("Relay Packets / Sec"), "blazium/multiuser_editor/limits/relay_packets_per_sec", 200, 1, 100000);
	_add_spin_field(grid, TTR("Visual Shader Node ID Max"), "blazium/multiuser_editor/limits/visual_shader_node_id_max", 1048576, 1024, 1 << 24);
	_add_spin_field(grid, TTR("Max Clients"), "blazium/multiuser_editor/network/max_clients", 32, 1, 256);
	_add_spin_field(grid, TTR("Security Events Max"), "blazium/multiuser_editor/limits/security_events_max", 16, 4, 1024);
}

void MultiuserEditorSettingsUI::_build_general_tab(VBoxContainer *p_root) {
	GridContainer *grid = memnew(GridContainer);
	grid->set_columns(2);
	p_root->add_child(grid);

	_add_check_field(grid, TTR("Enabled"), "blazium/multiuser_editor/enabled", false);
	_add_spin_field(grid, TTR("Role (0=Client, 1=Host)"), "blazium/multiuser_editor/role", 0, 0, 1);
	_add_check_field(grid, TTR("Enable Team Play"), "blazium/multiuser_editor/enable_team_play", true);
	_add_check_field(grid, TTR("Enable Debug Logging"), "blazium/multiuser_editor/enable_debug_logging", false);
	_add_check_field(grid, TTR("Server Logs Chat"), "blazium/multiuser_editor/server_log_chat", true);
	_add_check_field(grid, TTR("Server Logs Connections"), "blazium/multiuser_editor/server_log_connections", true);
}

void MultiuserEditorSettingsUI::_build_sync_tab(VBoxContainer *p_root) {
	GridContainer *grid = memnew(GridContainer);
	grid->set_columns(2);
	p_root->add_child(grid);

	_add_check_field(grid, TTR("Sync Scripts"), "blazium/multiuser_editor/sync_scripts", true);
	_add_check_field(grid, TTR("Sync Scene Changes"), "blazium/multiuser_editor/sync_scene_changes", true);
	_add_check_field(grid, TTR("Show Remote Cursors"), "blazium/multiuser_editor/show_remote_cursors", true);
	_add_check_field(grid, TTR("Cursor Sync Enabled (Send)"), "blazium/multiuser_editor/cursor_sync_enabled", true);
	_add_spin_field(grid, TTR("Telemetry Broadcast Interval (s)"), "blazium/multiuser_editor/telemetry_broadcast_interval_sec", 2.0, 0.1, 10.0, 0.1);
	_add_spin_field(grid, TTR("Script Poll Interval (s)"), "blazium/multiuser_editor/script_poll_interval_sec", 0.5, 0.05, 5.0, 0.01);
	_add_spin_field(grid, TTR("Lock Timeout (s)"), "blazium/multiuser_editor/locks/timeout_sec", 30.0, 1.0, 600.0, 1.0);
	_add_spin_field(grid, TTR("Project Setting Min Interval (ms)"), "blazium/multiuser_editor/throttle/project_setting_min_interval_ms", 50, 0, 60000);
}

void MultiuserEditorSettingsUI::_build_network_tab(VBoxContainer *p_root) {
	GridContainer *grid = memnew(GridContainer);
	grid->set_columns(2);
	p_root->add_child(grid);

	_add_spin_field(grid, TTR("Max Packet Size (MB)"), "blazium/multiuser_editor/max_packet_size_mb", 8, 1, 128);
	_add_spin_field(grid, TTR("Max Clients"), "blazium/multiuser_editor/network/max_clients", 32, 1, 256);
	_add_spin_field(grid, TTR("Process Packets per Second"), "blazium/multiuser_editor/limits/process_packets_per_sec", 500, 1, 100000);
	_add_spin_field(grid, TTR("Packets per Poll Max"), "blazium/multiuser_editor/limits/packets_per_poll_max", 256, 16, 65536);
	_add_spin_field(grid, TTR("Relay Packets / Sec"), "blazium/multiuser_editor/limits/relay_packets_per_sec", 200, 1, 100000);
}

void MultiuserEditorSettingsUI::_build_filesync_tab(VBoxContainer *p_root) {
	GridContainer *grid = memnew(GridContainer);
	grid->set_columns(2);
	p_root->add_child(grid);

	_add_check_field(grid, TTR("Enable File Sync"), "blazium/multiuser_editor/file_sync/enabled", true);
	_add_spin_field(grid, TTR("Max File Bytes"), "blazium/multiuser_editor/file_sync/max_file_bytes", 67108864, 4096, 268435456);
	_add_check_field(grid, TTR("Include Import Sidecars (.import)"), "blazium/multiuser_editor/file_sync/include_imports", true);
	_add_check_field(grid, TTR("Initial Snapshot On Join"), "blazium/multiuser_editor/file_sync/initial_snapshot_on_join", true);
	_add_spin_field(grid, TTR("Chunk Bytes"), "blazium/multiuser_editor/file_sync/chunk_bytes", 262144, 4096, 16777216);

	Label *info = memnew(Label);
	info->set_text(TTR("Include/exclude patterns are PackedStringArray; edit them in the Editor Settings (Project > Editor Settings)."));
	info->set_autowrap_mode(TextServer::AUTOWRAP_WORD);
	p_root->add_child(info);
}

void MultiuserEditorSettingsUI::_build_git_tab(VBoxContainer *p_root) {
	GridContainer *grid = memnew(GridContainer);
	grid->set_columns(2);
	p_root->add_child(grid);

	_add_line_field(grid, TTR("Default Remote"), "blazium/multiuser_editor/git_default_remote", "origin");
	_add_check_field(grid, TTR("Allow Editor Force Push"), "blazium/multiuser_editor/allow_editor_force_push", false);
	_add_check_field(grid, TTR("Enable Remote Git Actions"), "blazium/multiuser_editor/git_remote_actions_enabled", true);
	_add_spin_field(grid, TTR("Git Op Throttle (ms)"), "blazium/multiuser_editor/git_op_throttle_ms", 250, 0, 60000);
	_add_check_field(grid, TTR("Auto Commit"), "blazium/multiuser_editor/git_auto_commit", false);
	_add_check_field(grid, TTR("Auto Branching"), "blazium/multiuser_editor/git_auto_branching", false);
}

void MultiuserEditorSettingsUI::_build_permissions_tab(VBoxContainer *p_root) {
	GridContainer *grid = memnew(GridContainer);
	grid->set_columns(2);
	p_root->add_child(grid);

	_add_line_field(grid, TTR("Action Overrides (JSON)"), "blazium/multiuser_editor/permissions/overrides", "");
	_add_check_field(grid, TTR("Allow Widen Host-Only Actions"), "blazium/multiuser_editor/permissions/allow_widen_host_only", false);

	Label *info = memnew(Label);
	info->set_text(TTR("Format: {\"action\":\"role_or_@any\"}. host-only actions cannot be widened unless 'Allow Widen Host-Only Actions' is set."));
	info->set_autowrap_mode(TextServer::AUTOWRAP_WORD);
	p_root->add_child(info);
}

void MultiuserEditorSettingsUI::_build_logging_tab(VBoxContainer *p_root) {
	GridContainer *grid = memnew(GridContainer);
	grid->set_columns(2);
	p_root->add_child(grid);

	_add_spin_field(grid, TTR("Log Level (0=Error..3=Debug)"), "blazium/multiuser_editor/logging/log_level", 1, 0, 3);
	_add_check_field(grid, TTR("Log: General"), "blazium/multiuser_editor/logging/log_general", true);
	_add_check_field(grid, TTR("Log: Replication"), "blazium/multiuser_editor/logging/log_replication", true);
	_add_check_field(grid, TTR("Log: Filesystem"), "blazium/multiuser_editor/logging/log_filesystem", true);
	_add_check_field(grid, TTR("Log: CRDT"), "blazium/multiuser_editor/logging/log_crdt", true);
	_add_check_field(grid, TTR("Log: Network"), "blazium/multiuser_editor/logging/log_network", true);
	_add_check_field(grid, TTR("Log: Permissions"), "blazium/multiuser_editor/logging/log_permissions", true);
}

void MultiuserEditorSettingsUI::_build_intervals_tab(VBoxContainer *p_root) {
	GridContainer *grid = memnew(GridContainer);
	grid->set_columns(2);
	p_root->add_child(grid);

	_add_spin_field(grid, TTR("Poll Interval (sec)"), "blazium/multiuser_editor/intervals/poll_sec", 0.1, 0.01, 5.0, 0.01);
	_add_spin_field(grid, TTR("Cursor Broadcast Interval (sec)"), "blazium/multiuser_editor/intervals/cursor_sec", 0.05, 0.01, 5.0, 0.01);
	_add_spin_field(grid, TTR("Sync Pending Timeout (sec)"), "blazium/multiuser_editor/intervals/sync_pending_timeout_sec", 30.0, 1.0, 600.0, 1.0);
	_add_spin_field(grid, TTR("JWT JTI Sweep Interval (ms)"), "blazium/multiuser_editor/intervals/jwt_jti_sweep_ms", 5000, 100, 600000);
}

void MultiuserEditorSettingsUI::_build_diagnostics_tab(VBoxContainer *p_root) {
	diag_auth_mode_label = memnew(Label);
	diag_auth_mode_label->set_text(TTR("Auth Mode: Not connected"));
	p_root->add_child(diag_auth_mode_label);
	diag_drop_label = memnew(Label);
	diag_drop_label->set_text(TTR("Dropped: 0"));
	p_root->add_child(diag_drop_label);
	diag_throttle_label = memnew(Label);
	diag_throttle_label->set_text(TTR("Throttled: 0"));
	p_root->add_child(diag_throttle_label);
	Label *events_label = memnew(Label);
	events_label->set_text(TTR("Recent Security Events:"));
	p_root->add_child(events_label);
	diag_events_log = memnew(RichTextLabel);
	diag_events_log->set_custom_minimum_size(Size2(0, 160));
	diag_events_log->set_v_size_flags(SIZE_EXPAND_FILL);
	diag_events_log->set_scroll_follow(true);
	p_root->add_child(diag_events_log);
}

void MultiuserEditorSettingsUI::_add_line_field(GridContainer *p_grid, const String &p_label, const String &p_setting, const String &p_default, bool p_secret) {
	Label *lbl = memnew(Label);
	lbl->set_text(p_label);
	p_grid->add_child(lbl);
	LineEdit *edit = memnew(LineEdit);
	edit->set_secret(p_secret);
	edit->set_h_size_flags(SIZE_EXPAND_FILL);
	String current = String(MULTIUSER_GET(p_setting, p_default));
	edit->set_text(current);
	edit->connect("text_changed", callable_mp(this, &MultiuserEditorSettingsUI::_on_line_edit_changed).bind(p_setting));
	p_grid->add_child(edit);
	_line_inputs[p_setting] = edit;
}

void MultiuserEditorSettingsUI::_add_spin_field(GridContainer *p_grid, const String &p_label, const String &p_setting, double p_default, double p_min, double p_max, double p_step) {
	Label *lbl = memnew(Label);
	lbl->set_text(p_label);
	p_grid->add_child(lbl);
	SpinBox *spin = memnew(SpinBox);
	spin->set_min(p_min);
	spin->set_max(p_max);
	spin->set_step(p_step);
	double current = double(MULTIUSER_GET(p_setting, p_default));
	spin->set_value(current);
	spin->connect("value_changed", callable_mp(this, &MultiuserEditorSettingsUI::_on_spin_changed).bind(p_setting));
	p_grid->add_child(spin);
	_spin_inputs[p_setting] = spin;
}

void MultiuserEditorSettingsUI::_add_check_field(GridContainer *p_grid, const String &p_label, const String &p_setting, bool p_default) {
	Label *lbl = memnew(Label);
	lbl->set_text(p_label);
	p_grid->add_child(lbl);
	CheckBox *check = memnew(CheckBox);
	bool current = bool(MULTIUSER_GET(p_setting, p_default));
	check->set_pressed(current);
	check->connect("toggled", callable_mp(this, &MultiuserEditorSettingsUI::_on_check_toggled).bind(p_setting));
	p_grid->add_child(check);
	_check_inputs[p_setting] = check;
}

void MultiuserEditorSettingsUI::_on_line_edit_changed(const String &p_setting, const String &p_text) {
	if (EditorSettings::get_singleton()) {
		EditorSettings::get_singleton()->set(p_setting, p_text);
	}
}

void MultiuserEditorSettingsUI::_on_spin_changed(const String &p_setting, double p_value) {
	if (!EditorSettings::get_singleton()) {
		return;
	}

	if (Math::is_equal_approx(p_value, Math::round(p_value))) {
		EditorSettings::get_singleton()->set(p_setting, int(p_value));
	} else {
		EditorSettings::get_singleton()->set(p_setting, p_value);
	}
}

void MultiuserEditorSettingsUI::_on_check_toggled(const String &p_setting, bool p_pressed) {
	if (EditorSettings::get_singleton()) {
		EditorSettings::get_singleton()->set(p_setting, p_pressed);
	}
}

void MultiuserEditorSettingsUI::_add_option_field(GridContainer *p_grid, const String &p_label, const String &p_setting, const PackedStringArray &p_options, const String &p_default) {
	Label *lbl = memnew(Label);
	lbl->set_text(p_label);
	p_grid->add_child(lbl);
	OptionButton *opt = memnew(OptionButton);
	for (int i = 0; i < p_options.size(); i++) {
		opt->add_item(p_options[i], i);
	}
	const String current = String(MULTIUSER_GET(p_setting, p_default));
	int sel_idx = 0;
	for (int i = 0; i < p_options.size(); i++) {
		if (p_options[i] == current) {
			sel_idx = i;
			break;
		}
	}
	opt->select(sel_idx);
	opt->connect("item_selected", callable_mp(this, &MultiuserEditorSettingsUI::_on_option_selected).bind(p_setting, p_options));
	p_grid->add_child(opt);
	_option_inputs[p_setting] = opt;
}

void MultiuserEditorSettingsUI::_on_option_selected(const String &p_setting, const PackedStringArray &p_options, int p_idx) {
	if (!EditorSettings::get_singleton() || p_idx < 0 || p_idx >= p_options.size()) {
		return;
	}
	EditorSettings::get_singleton()->set(p_setting, p_options[p_idx]);
}

void MultiuserEditorSettingsUI::_on_mint_test_token_pressed() {
	MultiuserEditorPlugin *plugin = MultiuserEditorPlugin::get_singleton();
	if (!plugin || !plugin->is_local_admin()) {
		return;
	}
	if (mint_dialog) {
		mint_dialog->popup_centered();
	}
}

void MultiuserEditorSettingsUI::_on_mint_dialog_confirmed() {
	MultiuserEditorPlugin *plugin = MultiuserEditorPlugin::get_singleton();
	if (!plugin || !plugin->is_local_admin() || !mint_role_input || !mint_expiry_input) {
		return;
	}
	const String secret = String(MULTIUSER_GET("blazium/multiuser_editor/jwt_secret_key", ""));
	if (secret.is_empty()) {
		status_label->set_text(TTR("Cannot mint: jwt_secret_key is empty."));
		return;
	}
	const String role = mint_role_input->get_text().strip_edges().is_empty() ? String("Editor") : mint_role_input->get_text().strip_edges();
	const int64_t expiry_sec = MAX(60, int(mint_expiry_input->get_value()));
	const int64_t now = Time::get_singleton() ? int64_t(Time::get_singleton()->get_unix_time_from_system()) : 0;

	Ref<JWTBuilder> builder;
	builder.instantiate();
	builder->set_algorithm("HS256");
	builder->add_claim("iat", now);
	builder->add_claim("nbf", now);
	builder->add_claim("exp", now + expiry_sec);
	builder->add_claim("role", role);
	const String iss = String(MULTIUSER_GET("blazium/multiuser_editor/jwt/expected_issuer", ""));
	const String aud = String(MULTIUSER_GET("blazium/multiuser_editor/jwt/expected_audience", ""));
	if (!iss.is_empty()) {
		builder->set_issuer(iss);
	}
	if (!aud.is_empty()) {
		builder->set_audience(aud);
	}
	if (bool(MULTIUSER_GET("blazium/multiuser_editor/jwt/require_jti", false))) {
		builder->set_jwt_id(String::num_uint64(uint64_t(now) ^ uint64_t(OS::get_singleton()->get_ticks_usec()), 16));
	}
	const String jwt_str = builder->sign(secret);
	if (jwt_str.is_empty()) {
		status_label->set_text(TTR("Failed to mint test token."));
		return;
	}
	if (DisplayServer::get_singleton()) {
		DisplayServer::get_singleton()->clipboard_set(jwt_str);
	}
	status_label->set_text(vformat(TTR("Minted JWT (role=%s, %ds) - copied to clipboard."), role, int(expiry_sec)));
}

void MultiuserEditorSettingsUI::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE || p_what == NOTIFICATION_VISIBILITY_CHANGED) {
		_refresh();
	}
	if (p_what == NOTIFICATION_PROCESS) {
		_refresh_diagnostics();
	}
	if (p_what == NOTIFICATION_ENTER_TREE) {
		set_process(true);
	}
}

void MultiuserEditorSettingsUI::_start_pressed() {
	if (MultiuserEditorPlugin::get_singleton()) {
		int role = 0;
		if (EditorSettings::get_singleton() && EditorSettings::get_singleton()->has_setting("blazium/multiuser_editor/role")) {
			role = int(EditorSettings::get_singleton()->get("blazium/multiuser_editor/role"));
		}

		const String session_pw = connection_password_input ? connection_password_input->get_text() : String();
		if (role == 1) {
			MultiuserEditorPlugin::get_singleton()->host_session(int(port_input->get_value()), session_pw);
		} else {
			MultiuserEditorPlugin::get_singleton()->join_session(host_input->get_text().strip_edges(), int(port_input->get_value()), session_pw);
		}

		if (connection_password_input) {
			connection_password_input->set_text("");
		}
	}
	_refresh();
}

void MultiuserEditorSettingsUI::_stop_pressed() {
	if (MultiuserEditorPlugin::get_singleton()) {
		MultiuserEditorPlugin::get_singleton()->stop_session();
	}
	_refresh();
}

void MultiuserEditorSettingsUI::_refresh() {
	MultiuserEditorPlugin *plugin = MultiuserEditorPlugin::get_singleton();
	bool connected = plugin && plugin->is_session_connected();
	bool can_host = plugin && plugin->can_host_sessions();
	const bool is_admin = plugin && plugin->is_local_admin();

	int role = 0;
	if (EditorSettings::get_singleton() && EditorSettings::get_singleton()->has_setting("blazium/multiuser_editor/role")) {
		role = int(EditorSettings::get_singleton()->get("blazium/multiuser_editor/role"));
	}

	status_label->set_text(vformat(TTR("Connection: %s"), plugin ? plugin->get_status_text() : String(TTR("Plugin unavailable"))));

	if (start_button && stop_button) {
		if (role == 1) {
			start_button->set_disabled(connected || !can_host);
			if (host_input) {
				host_input->set_editable(false);
			}
		} else {
			start_button->set_disabled(connected);
			if (host_input) {
				host_input->set_editable(true);
			}
		}
		stop_button->set_disabled(!connected);
	}

	if (mint_test_token_btn) {
		mint_test_token_btn->set_disabled(!is_admin);
		mint_test_token_btn->set_tooltip_text(is_admin ? TTR("Mint a short-lived test JWT signed with the configured secret.") : TTR("Mint Test Token is Admin-only."));
	}

	for (KeyValue<String, LineEdit *> &E : _line_inputs) {
		if (E.value) {
			E.value->set_editable(is_admin);
		}
	}
	for (KeyValue<String, SpinBox *> &E : _spin_inputs) {
		if (E.value) {
			E.value->set_editable(is_admin);
		}
	}
	for (KeyValue<String, CheckBox *> &E : _check_inputs) {
		if (E.value) {
			E.value->set_disabled(!is_admin);
		}
	}
	for (KeyValue<String, OptionButton *> &E : _option_inputs) {
		if (E.value) {
			E.value->set_disabled(!is_admin);
		}
	}

	if (al_codename_input) {
		al_codename_input->set_editable(is_admin);
	}
	if (al_password_input) {
		al_password_input->set_editable(is_admin);
	}
	if (al_role_input) {
		al_role_input->set_disabled(!is_admin);
	}
	if (al_add_btn) {
		al_add_btn->set_disabled(!is_admin);
	}
	if (al_remove_btn) {
		al_remove_btn->set_disabled(!is_admin);
	}
	if (al_save_btn) {
		al_save_btn->set_disabled(!is_admin);
	}
	if (al_reveal_cb) {
		al_reveal_cb->set_disabled(!is_admin);
	}
}

void MultiuserEditorSettingsUI::_refresh_diagnostics() {
	MultiuserEditorPlugin *plugin = MultiuserEditorPlugin::get_singleton();
	if (!plugin) {
		return;
	}

	const uint64_t now = OS::get_singleton()->get_ticks_msec();
	if (_last_diag_refresh_msec != 0 && now - _last_diag_refresh_msec < DIAG_REFRESH_MIN_INTERVAL_MS) {
		return;
	}
	_last_diag_refresh_msec = now;

	if (diag_auth_mode_label) {
		const bool require_jwt = bool(MULTIUSER_GET("blazium/multiuser_editor/require_jwt", false));
		const bool connected = plugin->is_session_connected();
		String txt;
		if (!connected) {
			txt = TTR("Auth Mode: Not connected");
		} else if (require_jwt) {
			txt = TTR("Auth Mode: JWT");
		} else {
			txt = TTR("Auth Mode: HMAC");
		}
		diag_auth_mode_label->set_text(txt);
	}

	{
		const Vector<MultiuserEditorPlugin::SecurityEventSnapshot> snap_for_counts = plugin->get_recent_security_events_snapshot(64);
		int drops = 0;
		int thrott = 0;
		for (int i = 0; i < snap_for_counts.size(); i++) {
			const MultiuserEditorPlugin::SecurityEventSnapshot &E = snap_for_counts[i];
			switch (E.kind) {
				case MultiuserEditorDock::KIND_THROTTLED:
					thrott++;
					break;
				case MultiuserEditorDock::KIND_AUTH_FAIL:
				case MultiuserEditorDock::KIND_PROTECTED_PATH:
				case MultiuserEditorDock::KIND_MALFORMED:
				case MultiuserEditorDock::KIND_PRE_AUTH_DROP:
				case MultiuserEditorDock::KIND_UNKNOWN_ACTION:
				case MultiuserEditorDock::KIND_DROPPED:
				case MultiuserEditorDock::KIND_CRDT_REFUSED:
				case MultiuserEditorDock::KIND_LOCK_EVICTED:
				case MultiuserEditorDock::KIND_REPLICATION_FAILED:
					drops++;
					break;
				case MultiuserEditorDock::KIND_OTHER:
				default: {
					const String &m = E.message;
					if (m.contains("throttled")) {
						thrott++;
					} else if (m.begins_with("blocked") || m.contains("malformed") || m.contains("dropped") || m.contains("auth fail") || m.contains("pre-auth drop")) {
						drops++;
					}
					break;
				}
			}
		}
		if (diag_drop_label) {
			diag_drop_label->set_text(vformat(TTR("Dropped (recent ring): %d"), drops));
		}
		if (diag_throttle_label) {
			diag_throttle_label->set_text(vformat(TTR("Throttled (recent ring): %d"), thrott));
		}
	}

	if (diag_events_log) {
		const Vector<MultiuserEditorPlugin::SecurityEventSnapshot> snap = plugin->get_recent_security_events_snapshot(32);
		String all;
		for (int i = 0; i < snap.size(); i++) {
			const MultiuserEditorPlugin::SecurityEventSnapshot &E = snap[i];
			String color;
			switch (E.severity) {
				case 0:
					color = "red";
					break;
				case 1:
					color = "orange";
					break;
				default:
					color = "gray";
					break;
			}
			all += vformat("[color=%s][%d][/color] %s\n", color, int(E.when_msec / 1000), E.message);
		}
		diag_events_log->clear();
		diag_events_log->append_text(all);
	}
}

void MultiuserEditorSettingsUI::_build_access_list_tab(VBoxContainer *p_root) {
	{
		GridContainer *cfg_grid = memnew(GridContainer);
		cfg_grid->set_columns(2);
		p_root->add_child(cfg_grid);
		_add_check_field(cfg_grid, TTR("Access List Enabled"), "blazium/multiuser_editor/access_list/enabled", true);
		PackedStringArray role_opts;
		role_opts.push_back("Viewer");
		role_opts.push_back("Editor");
		role_opts.push_back("Admin");
		_add_option_field(cfg_grid, TTR("Implicit Default Role"), "blazium/multiuser_editor/access_list/implicit_default_role", role_opts, "Editor");
	}

	HBoxContainer *path_row = memnew(HBoxContainer);
	p_root->add_child(path_row);
	Label *path_label = memnew(Label);
	path_label->set_text(TTR("File:"));
	path_row->add_child(path_label);
	al_path_input = memnew(LineEdit);
	al_path_input->set_h_size_flags(SIZE_EXPAND_FILL);
	const String configured_path = String(MULTIUSER_GET("blazium/multiuser_editor/access_list/path", MultiuserEditorAccessList::default_path()));
	al_path_input->set_text(configured_path);
	al_path_input->connect("text_changed", callable_mp(this, &MultiuserEditorSettingsUI::_al_path_text_changed));
	path_row->add_child(al_path_input);
	al_browse_btn = memnew(Button);
	al_browse_btn->set_text(TTR("Browse..."));
	al_browse_btn->connect(SceneStringName(pressed), callable_mp(this, &MultiuserEditorSettingsUI::_al_browse_pressed));
	path_row->add_child(al_browse_btn);
	al_load_btn = memnew(Button);
	al_load_btn->set_text(TTR("Load"));
	al_load_btn->connect(SceneStringName(pressed), callable_mp(this, &MultiuserEditorSettingsUI::_al_load_pressed));
	path_row->add_child(al_load_btn);
	al_save_btn = memnew(Button);
	al_save_btn->set_text(TTR("Save"));
	al_save_btn->connect(SceneStringName(pressed), callable_mp(this, &MultiuserEditorSettingsUI::_al_save_pressed));
	path_row->add_child(al_save_btn);
	al_reload_btn = memnew(Button);
	al_reload_btn->set_text(TTR("Reload"));
	al_reload_btn->connect(SceneStringName(pressed), callable_mp(this, &MultiuserEditorSettingsUI::_al_reload_pressed));
	path_row->add_child(al_reload_btn);

	HBoxContainer *opt_row = memnew(HBoxContainer);
	p_root->add_child(opt_row);
	al_auto_load_cb = memnew(CheckBox);
	al_auto_load_cb->set_text(TTR("Auto-load on startup"));
	al_auto_load_cb->set_pressed(bool(MULTIUSER_GET("blazium/multiuser_editor/access_list/auto_load_on_startup", true)));
	al_auto_load_cb->connect("toggled", callable_mp(this, &MultiuserEditorSettingsUI::_al_auto_load_toggled));
	opt_row->add_child(al_auto_load_cb);
	al_auto_gitignore_cb = memnew(CheckBox);
	al_auto_gitignore_cb->set_text(TTR("Auto-add to .gitignore"));
	al_auto_gitignore_cb->set_pressed(bool(MULTIUSER_GET("blazium/multiuser_editor/access_list/auto_gitignore", true)));
	al_auto_gitignore_cb->connect("toggled", callable_mp(this, &MultiuserEditorSettingsUI::_al_auto_gitignore_toggled));
	opt_row->add_child(al_auto_gitignore_cb);
	al_reveal_cb = memnew(CheckBox);
	al_reveal_cb->set_text(TTR("Reveal passwords"));
	al_reveal_cb->set_pressed(false);
	al_reveal_cb->connect("toggled", callable_mp(this, &MultiuserEditorSettingsUI::_al_reveal_toggled));
	opt_row->add_child(al_reveal_cb);

	al_entries_tree = memnew(Tree);
	al_entries_tree->set_columns(3);
	al_entries_tree->set_column_title(0, TTR("Codename"));
	al_entries_tree->set_column_title(1, TTR("Role"));
	al_entries_tree->set_column_title(2, TTR("Password"));
	al_entries_tree->set_column_titles_visible(true);
	al_entries_tree->set_hide_root(true);
	al_entries_tree->create_item();
	al_entries_tree->set_custom_minimum_size(Size2(0, 200));
	al_entries_tree->set_v_size_flags(SIZE_EXPAND_FILL);
	p_root->add_child(al_entries_tree);

	HBoxContainer *add_row = memnew(HBoxContainer);
	p_root->add_child(add_row);
	al_codename_input = memnew(LineEdit);
	al_codename_input->set_placeholder(TTR("codename"));
	al_codename_input->set_h_size_flags(SIZE_EXPAND_FILL);
	add_row->add_child(al_codename_input);
	al_password_input = memnew(LineEdit);
	al_password_input->set_secret(true);
	al_password_input->set_placeholder(TTR("password"));
	al_password_input->set_h_size_flags(SIZE_EXPAND_FILL);
	add_row->add_child(al_password_input);
	al_role_input = memnew(OptionButton);
	al_role_input->add_item("Viewer");
	al_role_input->add_item("Editor");
	al_role_input->add_item("Admin");
	al_role_input->select(1);
	add_row->add_child(al_role_input);
	al_add_btn = memnew(Button);
	al_add_btn->set_text(TTR("Add/Update"));
	al_add_btn->connect(SceneStringName(pressed), callable_mp(this, &MultiuserEditorSettingsUI::_al_add_pressed));
	add_row->add_child(al_add_btn);
	al_remove_btn = memnew(Button);
	al_remove_btn->set_text(TTR("Remove Selected"));
	al_remove_btn->connect(SceneStringName(pressed), callable_mp(this, &MultiuserEditorSettingsUI::_al_remove_pressed));
	add_row->add_child(al_remove_btn);

	al_warning_label = memnew(Label);
	al_warning_label->set_text(TTR("Passwords are stored in plaintext. The file is excluded from network sync and auto-added to .gitignore. Distribute it to teammates manually."));
	al_warning_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD);
	al_warning_label->add_theme_color_override("font_color", Color(1.0, 0.6, 0.2));
	p_root->add_child(al_warning_label);

	al_status_label = memnew(Label);
	al_status_label->set_text("");
	p_root->add_child(al_status_label);

	al_browse_dialog = memnew(FileDialog);
	al_browse_dialog->set_file_mode(FileDialog::FILE_MODE_SAVE_FILE);
	al_browse_dialog->set_access(FileDialog::ACCESS_RESOURCES);
	al_browse_dialog->add_filter("*.json", TTR("JSON Files"));
	al_browse_dialog->connect("file_selected", callable_mp(this, &MultiuserEditorSettingsUI::_al_browse_file_selected));
	add_child(al_browse_dialog);

	_al_refresh_tree();
}

void MultiuserEditorSettingsUI::_al_refresh_tree() {
	if (!al_entries_tree || !al_entries_tree->get_root()) {
		return;
	}
	TreeItem *root = al_entries_tree->get_root();
	while (root->get_first_child()) {
		memdelete(root->get_first_child());
	}
	MultiuserEditorPlugin *plugin = MultiuserEditorPlugin::get_singleton();
	if (!plugin) {
		return;
	}
	const bool reveal = al_reveal_cb && al_reveal_cb->is_pressed() && plugin->is_local_admin();

	Ref<MultiuserEditorAccessList> snapshot;
	snapshot.instantiate();
	snapshot->set_max_entries(int(MULTIUSER_GET("blazium/multiuser_editor/access_list/max_entries", 256)));
	const String path = al_path_input ? al_path_input->get_text() : String();
	if (!path.is_empty()) {
		snapshot->load_from_file(path);
	}
	const Vector<MultiuserEditorAccessList::Entry> entries = snapshot->get_entries();
	for (int i = 0; i < entries.size(); i++) {
		TreeItem *item = al_entries_tree->create_item(root);
		item->set_text(0, entries[i].codename);
		item->set_text(1, entries[i].role);
		item->set_text(2, reveal ? entries[i].password : String("***"));
		item->set_metadata(0, entries[i].codename);
	}
	if (al_status_label) {
		al_status_label->set_text(vformat(TTR("Loaded %d entries from %s"), int(entries.size()), path));
	}
}

void MultiuserEditorSettingsUI::_al_browse_pressed() {
	if (al_browse_dialog) {
		al_browse_dialog->popup_file_dialog();
	}
}

void MultiuserEditorSettingsUI::_al_browse_file_selected(const String &p_path) {
	if (al_path_input) {
		al_path_input->set_text(p_path);
	}
	if (EditorSettings::get_singleton()) {
		EditorSettings::get_singleton()->set("blazium/multiuser_editor/access_list/path", p_path);
	}
	_al_refresh_tree();
}

void MultiuserEditorSettingsUI::_al_load_pressed() {
	_al_refresh_tree();

	if (MultiuserEditorPlugin *plugin = MultiuserEditorPlugin::get_singleton()) {
		plugin->reload_access_list();
	}
}

void MultiuserEditorSettingsUI::_al_save_pressed() {
	MultiuserEditorPlugin *plugin = MultiuserEditorPlugin::get_singleton();
	if (!plugin || !plugin->is_local_admin()) {
		if (al_status_label) {
			al_status_label->set_text(TTR("Save denied: Admin only."));
		}
		return;
	}

	if (al_status_label) {
		al_status_label->set_text(TTR("All changes are saved immediately on Add/Remove."));
	}
}

void MultiuserEditorSettingsUI::_al_reload_pressed() {
	if (EditorSettings::get_singleton() && al_path_input) {
		EditorSettings::get_singleton()->set("blazium/multiuser_editor/access_list/path", al_path_input->get_text());
	}
	_al_refresh_tree();
}

void MultiuserEditorSettingsUI::_al_add_pressed() {
	MultiuserEditorPlugin *plugin = MultiuserEditorPlugin::get_singleton();
	if (!plugin || !plugin->is_local_admin()) {
		if (al_status_label) {
			al_status_label->set_text(TTR("Add denied: Admin only."));
		}
		return;
	}
	if (!al_codename_input || !al_password_input || !al_role_input || !al_path_input) {
		return;
	}
	MultiuserEditorAccessList::Entry e;
	e.codename = al_codename_input->get_text().strip_edges();
	e.password = al_password_input->get_text();
	const int role_idx = al_role_input->get_selected_id() == -1 ? al_role_input->get_selected() : al_role_input->get_selected_id();
	e.role = al_role_input->get_item_text(MAX(0, role_idx));
	e.source = "file";
	if (!MultiuserEditorAccessList::is_valid_codename(e.codename)) {
		if (al_status_label) {
			al_status_label->set_text(TTR("Invalid codename (1..64 chars, [A-Za-z0-9_.-])."));
		}
		return;
	}
	if (e.password.is_empty() || e.password.length() > 1024) {
		if (al_status_label) {
			al_status_label->set_text(TTR("Invalid password (1..1024 chars)."));
		}
		return;
	}
	const String path = al_path_input->get_text();
	if (path.is_empty()) {
		if (al_status_label) {
			al_status_label->set_text(TTR("Set a file path first."));
		}
		return;
	}
	Ref<MultiuserEditorAccessList> al;
	al.instantiate();
	al->set_max_entries(int(MULTIUSER_GET("blazium/multiuser_editor/access_list/max_entries", 256)));
	al->load_from_file(path);
	const Error ae = al->add_or_update(e);
	if (ae != OK) {
		if (al_status_label) {
			al_status_label->set_text(vformat(TTR("Add failed: error=%d"), int(ae)));
		}
		return;
	}
	const Error se = al->save_to_file(path);
	if (se != OK) {
		if (al_status_label) {
			al_status_label->set_text(vformat(TTR("Save failed: error=%d"), int(se)));
		}
		return;
	}

	if (bool(MULTIUSER_GET("blazium/multiuser_editor/access_list/auto_gitignore", true))) {
		const String canon = MultiuserEditorAccessList::canonicalize_path(path);
		if (canon.begins_with("res://")) {
			MultiuserEditorAccessList::ensure_in_gitignore(canon, "res://");
		}
	}
	al_codename_input->set_text("");
	al_password_input->set_text("");
	_al_refresh_tree();

	plugin->reload_access_list();
}

void MultiuserEditorSettingsUI::_al_remove_pressed() {
	MultiuserEditorPlugin *plugin = MultiuserEditorPlugin::get_singleton();
	if (!plugin || !plugin->is_local_admin()) {
		if (al_status_label) {
			al_status_label->set_text(TTR("Remove denied: Admin only."));
		}
		return;
	}
	if (!al_entries_tree || !al_path_input) {
		return;
	}
	TreeItem *sel = al_entries_tree->get_selected();
	if (!sel) {
		if (al_status_label) {
			al_status_label->set_text(TTR("Select an entry to remove."));
		}
		return;
	}
	const String codename = String(sel->get_metadata(0));
	if (codename.is_empty()) {
		return;
	}
	const String path = al_path_input->get_text();
	Ref<MultiuserEditorAccessList> al;
	al.instantiate();
	al->set_max_entries(int(MULTIUSER_GET("blazium/multiuser_editor/access_list/max_entries", 256)));
	al->load_from_file(path);
	al->remove(codename);
	al->save_to_file(path);
	_al_refresh_tree();

	plugin->reload_access_list();
}

void MultiuserEditorSettingsUI::_al_reveal_toggled(bool p_pressed) {
	(void)p_pressed;
	_al_refresh_tree();
}

void MultiuserEditorSettingsUI::_al_path_text_changed(const String &p_text) {
	if (EditorSettings::get_singleton()) {
		EditorSettings::get_singleton()->set("blazium/multiuser_editor/access_list/path", p_text);
	}
}

void MultiuserEditorSettingsUI::_al_auto_load_toggled(bool p_pressed) {
	if (EditorSettings::get_singleton()) {
		EditorSettings::get_singleton()->set("blazium/multiuser_editor/access_list/auto_load_on_startup", p_pressed);
	}
}

void MultiuserEditorSettingsUI::_al_auto_gitignore_toggled(bool p_pressed) {
	if (EditorSettings::get_singleton()) {
		EditorSettings::get_singleton()->set("blazium/multiuser_editor/access_list/auto_gitignore", p_pressed);
	}
}

#endif
