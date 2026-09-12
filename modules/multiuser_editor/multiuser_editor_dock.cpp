/**************************************************************************/
/*  multiuser_editor_dock.cpp                                             */
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

#include "multiuser_editor_dock.h"

#include <climits>

#include "core/io/file_access.h"
#include "core/os/time.h"
#include "editor/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "multiuser_editor_constants.h"
#include "multiuser_editor_plugin.h"
#include "scene/gui/button.h"
#include "scene/gui/file_dialog.h"
#include "scene/gui/flow_container.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/popup_menu.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/spin_box.h"
#include "scene/gui/tree.h"

void MultiuserChatDock::set_module_enabled(bool p_enabled) {
	chat_input->set_editable(p_enabled);
}

void MultiuserChatDock::add_chat_message(const String &p_peer_id, const String &p_message) {
	ChatEntry entry;
	entry.peer_id = p_peer_id;
	entry.message = p_message;
	chat_ring.push_back(entry);
	const int cap = MAX(1, _chat_ring_max);
	while (chat_ring.size() > cap) {
		chat_ring.pop_front();
	}
	chat_history->add_text("[" + p_peer_id + "]: ");
	chat_history->add_text(p_message);
	chat_history->add_newline();
}

void MultiuserChatDock::set_chat_history_max(int p_max) {
	_chat_ring_max = MAX(1, p_max);
	while (chat_ring.size() > _chat_ring_max) {
		chat_ring.pop_front();
	}
}

void MultiuserChatDock::_chat_submitted(const String &p_text) {
	if (p_text.strip_edges().is_empty()) {
		return;
	}
	if (MultiuserEditorPlugin::get_singleton()) {
		MultiuserEditorPlugin::get_singleton()->call("send_chat", p_text);
	}
	chat_input->clear();
}

void MultiuserChatDock::_on_chat_clear_pressed() {
	chat_ring.clear();
	chat_history->clear();
}

void MultiuserChatDock::_on_chat_export_pressed() {
	if (chat_export_dialog) {
		chat_export_dialog->popup_centered_ratio(0.6f);
	}
}

void MultiuserChatDock::_on_chat_export_file_selected(const String &p_path) {
	if (p_path.is_empty()) {
		return;
	}
	Ref<FileAccess> fa = FileAccess::open(p_path, FileAccess::WRITE);
	if (fa.is_null()) {
		update_info(vformat(TTR("Could not open %s for writing"), p_path));
		return;
	}
	for (const ChatEntry &e : chat_ring) {
		String time_str;
		if (Time::get_singleton()) {
			time_str = Time::get_singleton()->get_time_string_from_system();
		}
		fa->store_line(vformat("[%s] %s: %s", time_str, e.peer_id, e.message));
	}
	fa->close();
	update_info(vformat(TTR("Chat exported to %s"), p_path));
}

void MultiuserChatDock::update_info(const String &p_text) {
	info_label->set_text(p_text);
}

MultiuserChatDock::MultiuserChatDock() {
	set_name("Chat");

	chat_history = memnew(RichTextLabel);
	chat_history->set_custom_minimum_size(Size2(0, 80 * EDSCALE));
	chat_history->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	chat_history->set_scroll_follow(true);

	chat_history->set_use_bbcode(false);
	add_child(chat_history);

	chat_input = memnew(LineEdit);
	chat_input->set_placeholder(TTR("Type a message..."));
	chat_input->set_keep_editing_on_text_submit(true);
	chat_input->connect("text_submitted", callable_mp(this, &MultiuserChatDock::_chat_submitted));
	add_child(chat_input);

	HBoxContainer *chat_actions_row = memnew(HBoxContainer);
	chat_clear_btn = memnew(Button);
	chat_clear_btn->set_h_size_flags(SIZE_EXPAND_FILL);
	chat_clear_btn->set_text(TTR("Clear Chat"));
	chat_clear_btn->connect(SceneStringName(pressed), callable_mp(this, &MultiuserChatDock::_on_chat_clear_pressed));
	chat_actions_row->add_child(chat_clear_btn);

	chat_export_btn = memnew(Button);
	chat_export_btn->set_h_size_flags(SIZE_EXPAND_FILL);
	chat_export_btn->set_text(TTR("Export Chat..."));
	chat_export_btn->connect(SceneStringName(pressed), callable_mp(this, &MultiuserChatDock::_on_chat_export_pressed));
	chat_actions_row->add_child(chat_export_btn);
	add_child(chat_actions_row);

	info_label = memnew(Label);
	add_child(info_label);

	chat_export_dialog = memnew(FileDialog);
	chat_export_dialog->set_file_mode(FileDialog::FILE_MODE_SAVE_FILE);
	chat_export_dialog->set_access(FileDialog::ACCESS_FILESYSTEM);
	chat_export_dialog->set_title(TTR("Export Chat To File"));
	chat_export_dialog->add_filter("*.txt", TTR("Text File"));
	chat_export_dialog->set_current_file("chat.txt");
	chat_export_dialog->connect("file_selected", callable_mp(this, &MultiuserChatDock::_on_chat_export_file_selected));
	add_child(chat_export_dialog);
}

MultiuserEditorDock::MultiuserEditorDock() {
	set_name("Multiuser");

	status_label = memnew(Label);
	status_label->set_custom_minimum_size(Vector2(64 * EDSCALE, 0));
	status_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD);
	add_child(status_label);

	HFlowContainer *status_header = memnew(HFlowContainer);
	auth_mode_label = memnew(Label);
	auth_mode_label->set_text(TTR("Auth: Open / not connected"));
	auth_mode_label->add_theme_color_override("font_color", Color(0.85, 0.85, 0.85));
	status_header->add_child(auth_mode_label);
	HBoxContainer *counter_row = memnew(HBoxContainer);
	drop_counter_label = memnew(Label);
	drop_counter_label->set_text(TTR("Dropped: 0"));
	counter_row->add_child(drop_counter_label);
	throttle_counter_label = memnew(Label);
	throttle_counter_label->set_text(TTR("Throttled: 0"));
	counter_row->add_child(throttle_counter_label);
	status_header->add_child(counter_row);
	add_child(status_header);

	ScrollContainer *scroll = memnew(ScrollContainer);
	scroll->set_follow_focus(true);
	scroll->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	add_child(scroll);

	VBoxContainer *content_vb = memnew(VBoxContainer);
	content_vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content_vb->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	scroll->add_child(content_vb);

	HBoxContainer *host_row = memnew(HBoxContainer);
	Label *host_label = memnew(Label);
	host_label->set_text(TTR("Host:"));
	host_row->add_child(host_label);
	host_input = memnew(LineEdit);
	if (EditorSettings::get_singleton() && EditorSettings::get_singleton()->has_setting("blazium/multiuser_editor/default_host")) {
		host_input->set_text(EditorSettings::get_singleton()->get("blazium/multiuser_editor/default_host"));
	} else {
		host_input->set_text("127.0.0.1");
	}
	host_input->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	host_row->add_child(host_input);
	content_vb->add_child(host_row);

	HBoxContainer *port_row = memnew(HBoxContainer);
	Label *port_label = memnew(Label);
	port_label->set_text(TTR("Port:"));
	port_row->add_child(port_label);
	port_input = memnew(SpinBox);
	port_input->set_min(multiuser_editor::kPortMin);
	port_input->set_max(multiuser_editor::kPortMax);
	if (EditorSettings::get_singleton() && EditorSettings::get_singleton()->has_setting("blazium/multiuser_editor/default_port")) {
		port_input->set_value(int(EditorSettings::get_singleton()->get("blazium/multiuser_editor/default_port")));
	} else {
		port_input->set_value(multiuser_editor::kDefaultPort);
	}
	port_input->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	port_row->add_child(port_input);
	content_vb->add_child(port_row);

	HBoxContainer *pass_row = memnew(HBoxContainer);
	Label *pass_label = memnew(Label);
	pass_label->set_text(TTR("Password:"));
	pass_row->add_child(pass_label);
	password_input = memnew(LineEdit);
	password_input->set_secret(true);
	password_input->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	pass_row->add_child(password_input);
	content_vb->add_child(pass_row);

	HBoxContainer *branch_row = memnew(HBoxContainer);
	Label *branch_label = memnew(Label);
	branch_label->set_text(TTR("Branch:"));
	branch_row->add_child(branch_label);
	session_branch_input = memnew(LineEdit);
	session_branch_input->set_placeholder("multiuser_session_{timestamp}");
	session_branch_input->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	branch_row->add_child(session_branch_input);
	content_vb->add_child(branch_row);

	HBoxContainer *merge_row = memnew(HBoxContainer);
	Label *merge_label = memnew(Label);
	merge_label->set_text(TTR("Merge Into:"));
	merge_row->add_child(merge_label);
	merge_target_input = memnew(LineEdit);
	merge_target_input->set_placeholder("main");
	merge_target_input->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	merge_row->add_child(merge_target_input);
	content_vb->add_child(merge_row);

	HBoxContainer *button_row = memnew(HBoxContainer);
	host_button = memnew(Button);
	host_button->set_text(TTR("Host"));
	host_button->set_h_size_flags(SIZE_EXPAND_FILL);
	host_button->connect(SceneStringName(pressed), callable_mp(this, &MultiuserEditorDock::_host_pressed));
	button_row->add_child(host_button);
	join_button = memnew(Button);
	join_button->set_h_size_flags(SIZE_EXPAND_FILL);
	join_button->set_text(TTR("Join"));
	join_button->connect(SceneStringName(pressed), callable_mp(this, &MultiuserEditorDock::_join_pressed));
	button_row->add_child(join_button);
	stop_button = memnew(Button);
	stop_button->set_h_size_flags(SIZE_EXPAND_FILL);
	stop_button->set_text(TTR("Stop"));
	stop_button->connect(SceneStringName(pressed), callable_mp(this, &MultiuserEditorDock::_stop_pressed));
	button_row->add_child(stop_button);
	content_vb->add_child(button_row);

	Label *peers_label = memnew(Label);
	peers_label->set_text(TTR("Connected Peers:"));
	content_vb->add_child(peers_label);

	peer_tree = memnew(Tree);
	peer_tree->set_columns(4);
	peer_tree->set_column_title(0, "Peer ID");
	peer_tree->set_column_title(1, "Role");
	peer_tree->set_column_title(2, "FPS");
	peer_tree->set_column_title(3, "Memory");
	peer_tree->set_column_titles_visible(true);
	peer_tree->set_hide_root(true);
	peer_tree->create_item();
	peer_tree->set_custom_minimum_size(Size2(0, 100 * EDSCALE));
	peer_tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	peer_tree->connect("cell_selected", callable_mp(this, &MultiuserEditorDock::_on_peer_tree_cell_selected));
	peer_tree->connect("item_mouse_selected", callable_mp(this, &MultiuserEditorDock::_on_peer_tree_item_mouse_selected));
	content_vb->add_child(peer_tree);

	HBoxContainer *peer_buttons_row = memnew(HBoxContainer);
	content_vb->add_child(peer_buttons_row);

	jump_button = memnew(Button);
	jump_button->set_h_size_flags(SIZE_EXPAND_FILL);
	jump_button->set_text(TTR("Jump to Peer"));
	jump_button->set_disabled(true);
	jump_button->connect(SceneStringName(pressed), callable_mp(this, &MultiuserEditorDock::_jump_selected_peer));
	peer_buttons_row->add_child(jump_button);

	follow_button = memnew(Button);
	follow_button->set_h_size_flags(SIZE_EXPAND_FILL);
	follow_button->set_text(TTR("Follow Peer"));
	follow_button->set_disabled(true);
	follow_button->connect(SceneStringName(pressed), callable_mp(this, &MultiuserEditorDock::_follow_selected_peer));
	peer_buttons_row->add_child(follow_button);

	test_button = memnew(Button);
	test_button->set_text(TTR("Network Test (Autowork)"));
	test_button->connect(SceneStringName(pressed), callable_mp(this, &MultiuserEditorDock::_test_pressed));
	content_vb->add_child(test_button);

	security_toggle_btn = memnew(Button);
	security_toggle_btn->set_text(TTR("Security Events"));
	security_toggle_btn->set_toggle_mode(true);
	security_toggle_btn->set_pressed(false);
	security_toggle_btn->connect(SceneStringName(pressed), callable_mp(this, &MultiuserEditorDock::_on_security_toggle_pressed));
	content_vb->add_child(security_toggle_btn);

	security_events_log = memnew(RichTextLabel);
	security_events_log->set_custom_minimum_size(Size2(0, 80 * EDSCALE));
	security_events_log->set_scroll_follow(true);
	security_events_log->set_visible(false);
	content_vb->add_child(security_events_log);

	git_panel = memnew(VBoxContainer);
	content_vb->add_child(git_panel);
	{
		HBoxContainer *git_row = memnew(HBoxContainer);
		git_panel->add_child(git_row);

		Label *git_label = memnew(Label);
		git_label->set_h_size_flags(SIZE_EXPAND_FILL);
		git_label->set_text(TTR("Git Workflow:"));
		// git_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
		git_row->add_child(git_label);

		git_status_btn = memnew(Button);
		git_status_btn->set_text(TTR("Status"));
		git_status_btn->connect(SceneStringName(pressed), callable_mp(this, &MultiuserEditorDock::_git_status_pressed));
		git_row->add_child(git_status_btn);

		HBoxContainer *branch_row_g = memnew(HBoxContainer);
		git_panel->add_child(branch_row_g);
		Label *branch_lbl = memnew(Label);
		branch_lbl->set_text(TTR("Branch:"));
		branch_row_g->add_child(branch_lbl);
		git_branch_input = memnew(LineEdit);
		git_branch_input->set_placeholder("branch-name");
		git_branch_input->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		branch_row_g->add_child(git_branch_input);
		git_switch_btn = memnew(Button);
		git_switch_btn->set_text(TTR("Switch"));
		git_switch_btn->connect(SceneStringName(pressed), callable_mp(this, &MultiuserEditorDock::_git_switch_pressed));
		branch_row_g->add_child(git_switch_btn);
		git_create_btn = memnew(Button);
		git_create_btn->set_text(TTR("Create"));
		git_create_btn->connect(SceneStringName(pressed), callable_mp(this, &MultiuserEditorDock::_git_create_pressed));
		branch_row_g->add_child(git_create_btn);

		HBoxContainer *commit_row = memnew(HBoxContainer);
		git_panel->add_child(commit_row);
		Label *commit_lbl = memnew(Label);
		commit_lbl->set_text(TTR("Message:"));
		commit_row->add_child(commit_lbl);
		git_commit_input = memnew(LineEdit);
		git_commit_input->set_placeholder(TTR("Commit message..."));
		git_commit_input->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		commit_row->add_child(git_commit_input);
		git_commit_btn = memnew(Button);
		git_commit_btn->set_text(TTR("Commit"));
		git_commit_btn->connect(SceneStringName(pressed), callable_mp(this, &MultiuserEditorDock::_git_commit_pressed));
		commit_row->add_child(git_commit_btn);

		GridContainer *git_buttons_gc = memnew(GridContainer);
		git_buttons_gc->set_columns(2);
		git_panel->add_child(git_buttons_gc);

		git_pull_btn = memnew(Button);
		git_pull_btn->set_h_size_flags(SIZE_EXPAND_FILL);
		git_pull_btn->set_text(TTR("Pull"));
		git_pull_btn->connect(SceneStringName(pressed), callable_mp(this, &MultiuserEditorDock::_git_pull_pressed));
		git_buttons_gc->add_child(git_pull_btn);
		git_pull_rebase_btn = memnew(Button);
		git_pull_rebase_btn->set_h_size_flags(SIZE_EXPAND_FILL);
		git_pull_rebase_btn->set_text(TTR("Pull (rebase)"));
		git_pull_rebase_btn->connect(SceneStringName(pressed), callable_mp(this, &MultiuserEditorDock::_git_pull_rebase_pressed));
		git_buttons_gc->add_child(git_pull_rebase_btn);
		git_push_btn = memnew(Button);
		git_push_btn->set_h_size_flags(SIZE_EXPAND_FILL);
		git_push_btn->set_text(TTR("Push"));
		git_push_btn->connect(SceneStringName(pressed), callable_mp(this, &MultiuserEditorDock::_git_push_pressed));
		git_buttons_gc->add_child(git_push_btn);
		git_force_push_btn = memnew(Button);
		git_force_push_btn->set_h_size_flags(SIZE_EXPAND_FILL);
		git_force_push_btn->set_text(TTR("Force Push"));
		git_force_push_btn->connect(SceneStringName(pressed), callable_mp(this, &MultiuserEditorDock::_git_force_push_pressed));
		git_buttons_gc->add_child(git_force_push_btn);

		git_output = memnew(RichTextLabel);
		git_output->set_custom_minimum_size(Size2(0, 80 * EDSCALE));
		git_output->set_scroll_follow(true);
		git_panel->add_child(git_output);
	}
	git_panel->hide();

	info_label = memnew(Label);
	info_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	add_child(info_label);

	set_disconnected();
}

void MultiuserEditorDock::_host_pressed() {
	if (MultiuserEditorPlugin::get_singleton()) {
		MultiuserEditorPlugin::get_singleton()->host_session(int(port_input->get_value()), password_input->get_text());
	}
}

void MultiuserEditorDock::_join_pressed() {
	if (MultiuserEditorPlugin::get_singleton()) {
		MultiuserEditorPlugin::get_singleton()->join_session(host_input->get_text().strip_edges(), int(port_input->get_value()), password_input->get_text());
	}
}

void MultiuserEditorDock::_stop_pressed() {
	if (MultiuserEditorPlugin::get_singleton()) {
		MultiuserEditorPlugin::get_singleton()->stop_session();
	}
}

void MultiuserEditorDock::_jump_selected_peer() {
	if (!peer_tree || !MultiuserEditorPlugin::get_singleton()) {
		return;
	}
	TreeItem *selected = peer_tree->get_selected();
	if (selected && selected != peer_tree->get_root()) {
		String peer_id = selected->get_text(0);
		MultiuserEditorPlugin::get_singleton()->jump_to_peer(peer_id);
	}
}

void MultiuserEditorDock::_on_peer_tree_cell_selected() {
	if (jump_button) {
		jump_button->set_disabled(false);
	}
	if (follow_button) {
		follow_button->set_disabled(false);
	}
}

void MultiuserEditorDock::_on_peer_tree_item_mouse_selected(const Vector2 &p_pos, MouseButton p_button) {
	if (p_button != MouseButton::RIGHT) {
		return;
	}
	TreeItem *selected = peer_tree->get_selected();
	if (!selected || selected == peer_tree->get_root()) {
		return;
	}

	String peer_id = selected->get_text(0);

	if (!peer_context_menu) {
		peer_context_menu = memnew(PopupMenu);
		add_child(peer_context_menu);
		peer_context_menu->connect("id_pressed", callable_mp(this, &MultiuserEditorDock::_handle_context_menu_id));
	}
	peer_context_menu->clear();
	peer_context_menu->add_item(TTR("Jump to Peer"), 0);
	peer_context_menu->add_item(TTR("Copy Peer ID"), 1);

	MultiuserEditorPlugin *plugin = MultiuserEditorPlugin::get_singleton();
	if (plugin && plugin->can_host_sessions() && plugin->is_session_connected() && peer_id != plugin->get_local_peer_id()) {
		peer_context_menu->add_separator();
		peer_context_menu->add_item(TTR("Kick Peer"), 2);
	}

	peer_context_target_id = peer_id;
	peer_context_menu->set_position(get_screen_position() + p_pos);
	peer_context_menu->popup();
}

void MultiuserEditorDock::_handle_context_menu_id(int p_id) {
	_handle_context_menu(p_id, peer_context_target_id);
}

void MultiuserEditorDock::_handle_context_menu(int p_id, const String &p_peer_id) {
	MultiuserEditorPlugin *plugin = MultiuserEditorPlugin::get_singleton();
	if (!plugin) {
		return;
	}

	if (p_id == 0) {
		plugin->jump_to_peer(p_peer_id);
	} else if (p_id == 1) {
		DisplayServer::get_singleton()->clipboard_set(p_peer_id);
	} else if (p_id == 2) {
		plugin->kick_peer(p_peer_id);
	}
}

void MultiuserEditorDock::_follow_selected_peer() {
	if (!peer_tree || !MultiuserEditorPlugin::get_singleton()) {
		return;
	}
	TreeItem *selected = peer_tree->get_selected();
	if (selected && selected != peer_tree->get_root()) {
		String peer_id = selected->get_text(0);
		MultiuserEditorPlugin::get_singleton()->call("toggle_follow_peer", peer_id);
	}
}

void MultiuserEditorDock::_test_pressed() {
	if (MultiuserEditorPlugin::get_singleton()) {
		MultiuserEditorPlugin::get_singleton()->trigger_autowork();
	}
}

void MultiuserEditorDock::set_connected(const String &p_status) {
	session_active = true;
	status_label->set_text(vformat(TTR("Status: %s"), p_status));
	if (!module_enabled) {
		return;
	}
	host_button->set_disabled(true);
	join_button->set_disabled(true);
	stop_button->set_disabled(false);
	host_input->set_editable(false);
	port_input->set_editable(false);
	if (session_branch_input) {
		session_branch_input->set_editable(false);
	}
	if (merge_target_input) {
		merge_target_input->set_editable(false);
	}
}

void MultiuserEditorDock::set_disconnected() {
	session_active = false;
	status_label->set_text(TTR("Status: Offline"));
	if (peer_tree && peer_tree->get_root()) {
		TreeItem *child = peer_tree->get_root()->get_first_child();
		while (child) {
			TreeItem *next = child->get_next();
			memdelete(child);
			child = next;
		}
		peer_items.clear();
	}
	if (!module_enabled) {
		return;
	}
	MultiuserEditorPlugin *plugin = MultiuserEditorPlugin::get_singleton();
	host_button->set_disabled(!plugin || !plugin->can_host_sessions());
	join_button->set_disabled(false);
	stop_button->set_disabled(true);
	host_input->set_editable(true);
	port_input->set_editable(true);
	if (session_branch_input) {
		session_branch_input->set_editable(true);
	}
	if (merge_target_input) {
		merge_target_input->set_editable(true);
	}
}

void MultiuserEditorDock::set_module_enabled(bool p_enabled) {
	module_enabled = p_enabled;
	if (p_enabled) {
		set_modulate(Color(1, 1, 1, 1));
		if (session_active) {
			set_connected(status_label ? status_label->get_text() : String("Connected"));
		} else {
			set_disconnected();
		}
		return;
	}
	set_modulate(Color(1, 1, 1, 0.6));
	if (status_label) {
		status_label->set_text(TTR("Multiuser Editor is disabled in Editor Settings (Blazium > Multiuser Editor > Enabled)"));
	}
	if (host_button) {
		host_button->set_disabled(true);
	}
	if (join_button) {
		join_button->set_disabled(true);
	}
	if (stop_button) {
		stop_button->set_disabled(true);
	}
	if (host_input) {
		host_input->set_editable(false);
	}
	if (port_input) {
		port_input->set_editable(false);
	}
	if (password_input) {
		password_input->set_editable(false);
	}
	if (session_branch_input) {
		session_branch_input->set_editable(false);
	}
	if (merge_target_input) {
		merge_target_input->set_editable(false);
	}
	if (jump_button) {
		jump_button->set_disabled(true);
	}
	if (follow_button) {
		follow_button->set_disabled(true);
	}
	if (test_button) {
		test_button->set_disabled(true);
	}
	if (peer_tree) {
		peer_tree->set_focus_mode(Control::FOCUS_NONE);
	}
}

void MultiuserEditorDock::set_session_branch_default(const String &p_value) {
	if (session_branch_input) {
		if (session_branch_input->get_text().strip_edges().is_empty()) {
			session_branch_input->set_text(p_value);
		}
		session_branch_input->set_placeholder(p_value);
	}
}

void MultiuserEditorDock::set_merge_target_default(const String &p_value) {
	if (merge_target_input) {
		if (merge_target_input->get_text().strip_edges().is_empty()) {
			merge_target_input->set_text(p_value);
		}
		merge_target_input->set_placeholder(p_value);
	}
}

String MultiuserEditorDock::get_session_branch_override() const {
	if (!session_branch_input) {
		return String();
	}
	return session_branch_input->get_text().strip_edges();
}

String MultiuserEditorDock::get_merge_target_override() const {
	if (!merge_target_input) {
		return String();
	}
	return merge_target_input->get_text().strip_edges();
}

void MultiuserEditorDock::add_peer(const String &p_peer_id) {
	if (!peer_tree || p_peer_id.is_empty() || !peer_tree->get_root()) {
		return;
	}
	if (peer_items.has(p_peer_id)) {
		return;
	}
	TreeItem *item = peer_tree->create_item(peer_tree->get_root());
	item->set_text(0, p_peer_id);
	item->set_text(1, multiuser_editor::kRoleViewer);
	item->set_text(2, "0 FPS");
	item->set_text(3, "0 MB");
	peer_items[p_peer_id] = item;
}

void MultiuserEditorDock::remove_peer(const String &p_peer_id) {
	peer_telemetry.erase(p_peer_id);
	if (peer_items.has(p_peer_id)) {
		TreeItem *item = peer_items[p_peer_id];
		if (item) {
			memdelete(item);
		}
		peer_items.erase(p_peer_id);
	}
}

void MultiuserEditorDock::update_peer_telemetry(const String &p_peer_id, const Dictionary &p_telemetry) {
	if (!peer_items.has(p_peer_id)) {
		return;
	}
	peer_telemetry[p_peer_id] = p_telemetry;
	if (peer_items.has(p_peer_id)) {
		TreeItem *item = peer_items[p_peer_id];
		if (item) {
			float fps = p_telemetry.get("fps", 0.0);
			int mem = int(p_telemetry.get("memory", 0)) / 1024 / 1024;
			String role = p_telemetry.get("role", String());
			if (role.length() > 64) {
				role = role.substr(0, 61) + "...";
			}

			bool mcp_active = p_telemetry.get("mcp_active", false);

			if (mcp_active) {
				role += " (MCP Agent)";
			}

			if (!role.is_empty()) {
				item->set_text(1, role);
				if (mcp_active) {
					item->set_custom_color(1, Color(0.2, 0.8, 1.0));
				} else {
					item->clear_custom_color(1);
				}
			}

			item->set_text(2, vformat("%.0f FPS", fps));
			item->set_text(3, vformat("%d MB", mem));

			if (fps < 30) {
				item->set_custom_color(2, Color(1.0, 0.5, 0.0));
			} else {
				item->clear_custom_color(2);
			}

			if (mem > 2000) {
				item->set_custom_color(3, Color(1.0, 0.2, 0.2));
			} else {
				item->clear_custom_color(3);
			}
		}
	}
}

void MultiuserEditorDock::update_info(const String &p_text) {
	info_label->set_text(p_text);
}

void MultiuserEditorDock::_refresh_git_visibility() {
	if (!git_panel) {
		return;
	}
	const bool can_show = session_active && module_enabled && local_role_cached != multiuser_editor::kRoleViewer;
	git_panel->set_visible(can_show);
	if (git_force_push_btn) {
		const bool admin_or_allowed = (local_role_cached == multiuser_editor::kRoleAdmin) || force_push_visible_cached;
		git_force_push_btn->set_visible(admin_or_allowed);
	}
}

void MultiuserEditorDock::set_git_panel_enabled(bool p_enabled, const String &p_local_role, bool p_force_push_enabled) {
	local_role_cached = p_local_role.is_empty() ? String(multiuser_editor::kRoleEditor) : p_local_role;
	force_push_visible_cached = p_force_push_enabled;
	if (!p_enabled && git_panel) {
		git_panel->hide();
		return;
	}
	_refresh_git_visibility();
}

void MultiuserEditorDock::show_git_response(const Dictionary &p_data) {
	if (!git_output) {
		return;
	}
	if (p_data.has("force_push_enabled")) {
		const bool fp = bool(p_data.get("force_push_enabled", false));
		if (fp != force_push_visible_cached) {
			force_push_visible_cached = fp;
			_refresh_git_visibility();
		}
	}
	const String op = String(p_data.get("op", ""));
	const bool success = bool(p_data.get("success", false));
	const int exit_code = int(p_data.get("exit_code", -1));
	const String reason = String(p_data.get("reason", ""));
	const String requester = String(p_data.get("requester_peer_id", ""));
	String output = String(p_data.get("output", ""));
	int output_max = multiuser_editor::kGitOutputDefaultMax;
	if (EditorSettings::get_singleton() && EditorSettings::get_singleton()->has_setting("blazium/multiuser_editor/limits/git_output_max_chars")) {
		output_max = MAX(64, int(EditorSettings::get_singleton()->get("blazium/multiuser_editor/limits/git_output_max_chars")));
	}
	if (output.length() > output_max) {
		output = output.substr(0, output_max) + "\n...[truncated]";
	}

	git_output->push_color(success ? Color(0.6, 1.0, 0.6) : Color(1.0, 0.6, 0.6));
	git_output->add_text(vformat("[%s] op=%s exit=%d", success ? String("OK") : String("FAIL"), op, exit_code));
	git_output->pop();
	git_output->add_newline();
	if (!requester.is_empty()) {
		git_output->add_text(vformat("requester: %s", requester));
		git_output->add_newline();
	}
	if (!reason.is_empty()) {
		git_output->add_text(vformat("reason: %s", reason));
		git_output->add_newline();
	}
	if (!output.is_empty()) {
		git_output->add_text(output);
		git_output->add_newline();
	}
	git_output->add_text("---");
	git_output->add_newline();
}

void MultiuserEditorDock::_git_switch_pressed() {
	if (!git_branch_input || !MultiuserEditorPlugin::get_singleton()) {
		return;
	}
	const String b = git_branch_input->get_text().strip_edges();
	MultiuserEditorPlugin::get_singleton()->request_git_op_with_branch("branch_switch", b);
}

void MultiuserEditorDock::_git_create_pressed() {
	if (!git_branch_input || !MultiuserEditorPlugin::get_singleton()) {
		return;
	}
	const String b = git_branch_input->get_text().strip_edges();
	MultiuserEditorPlugin::get_singleton()->request_git_op_with_branch("branch_create", b);
}

void MultiuserEditorDock::_git_commit_pressed() {
	if (!git_commit_input || !MultiuserEditorPlugin::get_singleton()) {
		return;
	}
	String m = git_commit_input->get_text().strip_edges();

	if (m.is_empty() || m.length() > multiuser_editor::kCommitMessageMax) {
		update_info(vformat(TTR("Commit message must be 1-%d chars"), multiuser_editor::kCommitMessageMax));
		return;
	}
	MultiuserEditorPlugin::get_singleton()->request_git_op_commit(m);
}

void MultiuserEditorDock::_git_status_pressed() {
	if (MultiuserEditorPlugin::get_singleton()) {
		MultiuserEditorPlugin::get_singleton()->request_git_op("status");
	}
}

void MultiuserEditorDock::_git_pull_pressed() {
	if (MultiuserEditorPlugin::get_singleton()) {
		MultiuserEditorPlugin::get_singleton()->request_git_op("pull");
	}
}

void MultiuserEditorDock::_git_pull_rebase_pressed() {
	if (MultiuserEditorPlugin::get_singleton()) {
		MultiuserEditorPlugin::get_singleton()->request_git_op("pull_rebase");
	}
}

void MultiuserEditorDock::_git_push_pressed() {
	if (MultiuserEditorPlugin::get_singleton()) {
		MultiuserEditorPlugin::get_singleton()->request_git_op("push");
	}
}

void MultiuserEditorDock::_git_force_push_pressed() {
	if (MultiuserEditorPlugin::get_singleton()) {
		MultiuserEditorPlugin::get_singleton()->request_git_op("force_push");
	}
}

void MultiuserEditorDock::set_auth_mode(AuthMode p_mode) {
	if (!auth_mode_label) {
		return;
	}
	switch (p_mode) {
		case AUTH_JWT:
			auth_mode_label->set_text(TTR("Auth: JWT"));
			auth_mode_label->add_theme_color_override("font_color", Color(0.4, 0.95, 0.4));
			break;
		case AUTH_HMAC:
			auth_mode_label->set_text(TTR("Auth: HMAC"));
			auth_mode_label->add_theme_color_override("font_color", Color(1.0, 0.85, 0.3));
			break;
		case AUTH_NONE:
		default:
			auth_mode_label->set_text(TTR("Auth: Open / not connected"));
			auth_mode_label->add_theme_color_override("font_color", Color(0.95, 0.4, 0.4));
			break;
	}
}

void MultiuserEditorDock::bump_drop_counter(int p_by) {
	const int delta = MAX(0, p_by);

	if (delta > 0 && drop_counter > INT_MAX - delta) {
		drop_counter = INT_MAX;
	} else {
		drop_counter += delta;
	}
	if (drop_counter_label) {
		drop_counter_label->set_text(vformat(TTR("Dropped: %d"), drop_counter));
	}
}

void MultiuserEditorDock::bump_throttle_counter(int p_by) {
	const int delta = MAX(0, p_by);

	if (delta > 0 && throttle_counter > INT_MAX - delta) {
		throttle_counter = INT_MAX;
	} else {
		throttle_counter += delta;
	}
	if (throttle_counter_label) {
		throttle_counter_label->set_text(vformat(TTR("Throttled: %d"), throttle_counter));
	}
}

void MultiuserEditorDock::reset_counters() {
	drop_counter = 0;
	throttle_counter = 0;
	if (drop_counter_label) {
		drop_counter_label->set_text(TTR("Dropped: 0"));
	}
	if (throttle_counter_label) {
		throttle_counter_label->set_text(TTR("Throttled: 0"));
	}
}

void MultiuserEditorDock::record_security_event(int p_severity, int p_category, const String &p_message) {
	record_security_event_kind(KIND_OTHER, p_severity, p_category, p_message);
}

void MultiuserEditorDock::record_security_event_kind(int p_kind, int p_severity, int p_category, const String &p_message) {
	SecurityEvent ev;
	ev.when_msec = OS::get_singleton() ? OS::get_singleton()->get_ticks_msec() : 0;
	ev.severity = p_severity;
	ev.category = p_category;
	ev.kind = p_kind;
	String msg = p_message;
	if (msg.length() > 256) {
		msg = msg.substr(0, 253) + "...";
	}
	ev.message = msg;
	security_ring.push_back(ev);
	const int cap = MAX(1, _security_ring_max);
	while (security_ring.size() > cap) {
		security_ring.pop_front();
	}
	_refresh_security_events_log();
}

void MultiuserEditorDock::set_security_ring_max(int p_max) {
	_security_ring_max = MAX(1, p_max);
	while (security_ring.size() > _security_ring_max) {
		security_ring.pop_front();
	}
	_refresh_security_events_log();
}

void MultiuserEditorDock::_on_security_toggle_pressed() {
	security_events_visible = security_toggle_btn ? security_toggle_btn->is_pressed() : !security_events_visible;
	if (security_events_log) {
		security_events_log->set_visible(security_events_visible);
	}
	if (security_events_visible) {
		_refresh_security_events_log();
	}
}

void MultiuserEditorDock::_refresh_security_events_log() {
	if (!security_events_log || !security_events_visible) {
		return;
	}
	security_events_log->clear();
	for (const SecurityEvent &ev : security_ring) {
		Color c(0.85, 0.85, 0.85);
		String prefix = "INFO";

		switch (ev.severity) {
			case 0:
				c = Color(0.95, 0.4, 0.4);
				prefix = "ERROR";
				break;
			case 1:
				c = Color(1.0, 0.85, 0.3);
				prefix = "WARN";
				break;
			case 2:
				c = Color(0.6, 0.85, 1.0);
				prefix = "INFO";
				break;
			default:
				c = Color(0.7, 0.7, 0.7);
				prefix = "DBG";
				break;
		}
		security_events_log->push_color(c);
		security_events_log->add_text(vformat("[%s] ", prefix));
		security_events_log->pop();
		security_events_log->add_text(ev.message);
		security_events_log->add_newline();
	}
}

#endif
