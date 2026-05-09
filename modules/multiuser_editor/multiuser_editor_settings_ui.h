/**************************************************************************/
/*  multiuser_editor_settings_ui.h                                        */
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

#include "core/templates/hash_map.h"
#include "scene/gui/margin_container.h"

class Button;
class Label;
class LineEdit;
class SpinBox;
class CheckBox;
class TabContainer;
class AcceptDialog;
class RichTextLabel;
class VBoxContainer;
class GridContainer;
class FileDialog;
class OptionButton;
class Tree;
class TreeItem;

class MultiuserEditorSettingsUI : public MarginContainer {
	GDCLASS(MultiuserEditorSettingsUI, MarginContainer);

	Label *status_label = nullptr;
	Button *start_button = nullptr;
	Button *stop_button = nullptr;
	SpinBox *port_input = nullptr;
	LineEdit *host_input = nullptr;

	LineEdit *connection_password_input = nullptr;

	TabContainer *tabs = nullptr;
	Label *diag_auth_mode_label = nullptr;
	Label *diag_drop_label = nullptr;
	Label *diag_throttle_label = nullptr;
	RichTextLabel *diag_events_log = nullptr;

	LineEdit *al_path_input = nullptr;
	Button *al_browse_btn = nullptr;
	Button *al_load_btn = nullptr;
	Button *al_save_btn = nullptr;
	Button *al_reload_btn = nullptr;
	CheckBox *al_auto_load_cb = nullptr;
	CheckBox *al_auto_gitignore_cb = nullptr;
	Tree *al_entries_tree = nullptr;
	LineEdit *al_codename_input = nullptr;
	LineEdit *al_password_input = nullptr;
	OptionButton *al_role_input = nullptr;
	Button *al_add_btn = nullptr;
	Button *al_remove_btn = nullptr;
	CheckBox *al_reveal_cb = nullptr;
	Label *al_warning_label = nullptr;
	Label *al_status_label = nullptr;
	FileDialog *al_browse_dialog = nullptr;

	Button *mint_test_token_btn = nullptr;
	AcceptDialog *mint_dialog = nullptr;
	LineEdit *mint_role_input = nullptr;
	SpinBox *mint_expiry_input = nullptr;

	struct Field {
		String setting_path;
		Variant default_value;
	};
	HashMap<String, LineEdit *> _line_inputs;
	HashMap<String, SpinBox *> _spin_inputs;
	HashMap<String, CheckBox *> _check_inputs;
	HashMap<String, OptionButton *> _option_inputs;

	uint64_t _last_diag_refresh_msec = 0;
	static constexpr uint64_t DIAG_REFRESH_MIN_INTERVAL_MS = 250;

	void _start_pressed();
	void _stop_pressed();
	void _on_mint_test_token_pressed();
	void _on_mint_dialog_confirmed();
	void _on_line_edit_changed(const String &p_setting, const String &p_text);
	void _on_spin_changed(const String &p_setting, double p_value);
	void _on_check_toggled(const String &p_setting, bool p_pressed);
	void _refresh();
	void _refresh_diagnostics();

	void _build_connection_tab(VBoxContainer *p_root);
	void _build_general_tab(VBoxContainer *p_root);
	void _build_sync_tab(VBoxContainer *p_root);
	void _build_network_tab(VBoxContainer *p_root);
	void _build_security_tab(VBoxContainer *p_root);
	void _build_throttling_tab(VBoxContainer *p_root);
	void _build_filesync_tab(VBoxContainer *p_root);
	void _build_git_tab(VBoxContainer *p_root);
	void _build_permissions_tab(VBoxContainer *p_root);
	void _build_logging_tab(VBoxContainer *p_root);
	void _build_intervals_tab(VBoxContainer *p_root);
	void _build_diagnostics_tab(VBoxContainer *p_root);
	void _build_access_list_tab(VBoxContainer *p_root);

	void _add_line_field(GridContainer *p_grid, const String &p_label, const String &p_setting, const String &p_default, bool p_secret = false);
	void _add_spin_field(GridContainer *p_grid, const String &p_label, const String &p_setting, double p_default, double p_min, double p_max, double p_step = 1.0);
	void _add_check_field(GridContainer *p_grid, const String &p_label, const String &p_setting, bool p_default);
	void _add_option_field(GridContainer *p_grid, const String &p_label, const String &p_setting, const PackedStringArray &p_options, const String &p_default);
	void _on_option_selected(const String &p_setting, const PackedStringArray &p_options, int p_idx);

	void _al_refresh_tree();
	void _al_browse_pressed();
	void _al_browse_file_selected(const String &p_path);
	void _al_load_pressed();
	void _al_save_pressed();
	void _al_reload_pressed();
	void _al_add_pressed();
	void _al_remove_pressed();
	void _al_reveal_toggled(bool p_pressed);
	void _al_path_text_changed(const String &p_text);
	void _al_auto_load_toggled(bool p_pressed);
	void _al_auto_gitignore_toggled(bool p_pressed);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	MultiuserEditorSettingsUI();
};

#endif
