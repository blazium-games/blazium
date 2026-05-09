/**************************************************************************/
/*  multiuser_editor_dock.h                                               */
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

#include "core/templates/list.h"

#include "scene/gui/box_container.h"

class Label;
class Button;
class LineEdit;
class SpinBox;
class Tree;
class TreeItem;
class RichTextLabel;
class FileDialog;
class PopupMenu;

class MultiuserEditorDock : public VBoxContainer {
	GDCLASS(MultiuserEditorDock, VBoxContainer);

	Label *status_label = nullptr;
	Label *info_label = nullptr;
	Tree *peer_tree = nullptr;
	RichTextLabel *chat_history = nullptr;
	LineEdit *chat_input = nullptr;
	Button *host_button = nullptr;
	Button *join_button = nullptr;
	Button *stop_button = nullptr;
	Button *jump_button = nullptr;
	Button *kick_button = nullptr;
	Button *follow_button = nullptr;
	Button *test_button = nullptr;

	Label *auth_mode_label = nullptr;
	Label *drop_counter_label = nullptr;
	Label *throttle_counter_label = nullptr;
	int drop_counter = 0;
	int throttle_counter = 0;

public:
	enum SecurityEventKind {
		KIND_OTHER = 0,
		KIND_AUTH_FAIL = 1,
		KIND_AUTH_OK = 2,
		KIND_THROTTLED = 3,
		KIND_DROPPED = 4,
		KIND_PROTECTED_PATH = 5,
		KIND_MALFORMED = 6,
		KIND_PRE_AUTH_DROP = 7,
		KIND_UNKNOWN_ACTION = 8,
		KIND_CRDT_REFUSED = 9,
		KIND_PERMISSION_OVERRIDE = 10,
		KIND_LOCK_EVICTED = 11,
		KIND_REPLICATION_FAILED = 12,
		KIND_PERMISSION_DENIED = 13,
		KIND_INVALID_PACKET = 14,
		KIND_RATE_LIMITED = 15,
		KIND_AUTH_FAILED = 16,
		KIND_ADMIN_KICK = 17,
	};

private:
	struct SecurityEvent {
		uint64_t when_msec = 0;
		int severity = 0;
		int category = 0;
		int kind = KIND_OTHER;
		String message;
	};
	List<SecurityEvent> security_ring;
	int _security_ring_max = 16;
	Button *security_toggle_btn = nullptr;
	RichTextLabel *security_events_log = nullptr;
	bool security_events_visible = false;

	Button *chat_clear_btn = nullptr;
	Button *chat_export_btn = nullptr;
	FileDialog *chat_export_dialog = nullptr;

	SpinBox *port_input = nullptr;
	LineEdit *host_input = nullptr;
	LineEdit *password_input = nullptr;
	LineEdit *session_branch_input = nullptr;
	LineEdit *merge_target_input = nullptr;
	bool module_enabled = true;
	bool session_active = false;

	VBoxContainer *git_panel = nullptr;
	LineEdit *git_branch_input = nullptr;
	Button *git_switch_btn = nullptr;
	Button *git_create_btn = nullptr;
	LineEdit *git_commit_input = nullptr;
	Button *git_commit_btn = nullptr;
	Button *git_status_btn = nullptr;
	Button *git_pull_btn = nullptr;
	Button *git_pull_rebase_btn = nullptr;
	Button *git_push_btn = nullptr;
	Button *git_force_push_btn = nullptr;
	RichTextLabel *git_output = nullptr;
	String local_role_cached = "Editor";
	bool force_push_visible_cached = false;

	void _host_pressed();
	void _join_pressed();
	void _stop_pressed();
	void _jump_selected_peer();
	void _follow_selected_peer();
	void _test_pressed();
	void _chat_submitted(const String &p_text);

	void _git_switch_pressed();
	void _git_create_pressed();
	void _git_commit_pressed();
	void _git_status_pressed();
	void _git_pull_pressed();
	void _git_pull_rebase_pressed();
	void _git_push_pressed();
	void _git_force_push_pressed();
	void _refresh_git_visibility();

	void _on_peer_tree_cell_selected();
	void _on_peer_tree_item_mouse_selected(const Vector2 &p_pos, MouseButton p_button);
	void _handle_context_menu(int p_id, const String &p_peer_id);

	void _handle_context_menu_id(int p_id);
	void _on_kick_pressed();
	void _on_copy_id_pressed();
	void _on_security_toggle_pressed();
	void _on_chat_clear_pressed();
	void _on_chat_export_pressed();
	void _on_chat_export_file_selected(const String &p_path);
	void _refresh_security_events_log();

	HashMap<String, Dictionary> peer_telemetry;
	HashMap<String, TreeItem *> peer_items;

	PopupMenu *peer_context_menu = nullptr;
	String peer_context_target_id;

	struct ChatEntry {
		String peer_id;
		String message;
	};
	List<ChatEntry> chat_ring;
	int _chat_ring_max = 256;

protected:
	static void _bind_methods();

public:
	void set_connected(const String &p_status);
	void set_disconnected();
	void add_peer(const String &p_peer_id);
	void remove_peer(const String &p_peer_id);
	void update_peer_telemetry(const String &p_peer_id, const Dictionary &p_telemetry);

	void update_info(const String &p_text);
	void add_chat_message(const String &p_peer_id, const String &p_message);
	void set_chat_history_max(int p_max);
	int get_chat_history_count() const { return chat_ring.size(); }

	enum AuthMode {
		AUTH_NONE = 0,
		AUTH_HMAC = 1,
		AUTH_JWT = 2,
	};
	void set_auth_mode(AuthMode p_mode);
	void bump_drop_counter(int p_by = 1);
	void bump_throttle_counter(int p_by = 1);
	void reset_counters();
	int get_drop_counter() const { return drop_counter; }
	int get_throttle_counter() const { return throttle_counter; }

	void record_security_event(int p_severity, int p_category, const String &p_message);

	void record_security_event_kind(int p_kind, int p_severity, int p_category, const String &p_message);
	int get_security_event_count() const { return security_ring.size(); }
	void set_security_ring_max(int p_max);

	struct SecurityEventSnapshotOut {
		uint64_t when_msec = 0;
		int severity = 0;
		int category = 0;
		int kind = KIND_OTHER;
		String message;
	};
	template <typename T>
	void snapshot_recent_security_events(int p_max, Vector<T> &r_out) const {
		r_out.clear();
		if (p_max <= 0) {
			return;
		}
		int copied = 0;
		for (const SecurityEvent &E : security_ring) {
			if (copied >= p_max) {
				break;
			}
			T s;
			s.when_msec = E.when_msec;
			s.severity = E.severity;
			s.category = E.category;
			s.kind = E.kind;
			s.message = E.message;
			r_out.push_back(s);
			copied++;
		}
	}

	void set_module_enabled(bool p_enabled);
	void set_session_branch_default(const String &p_value);
	void set_merge_target_default(const String &p_value);
	String get_session_branch_override() const;
	String get_merge_target_override() const;

	void set_git_panel_enabled(bool p_enabled, const String &p_local_role, bool p_force_push_enabled);
	void show_git_response(const Dictionary &p_data);

	MultiuserEditorDock();
};

#endif
