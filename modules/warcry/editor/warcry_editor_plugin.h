/**************************************************************************/
/*  warcry_editor_plugin.h                                                */
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

#include "editor/plugins/editor_plugin.h"

class Button;
class CheckBox;
class HSlider;
class ItemList;
class Label;
class LineEdit;
class VBoxContainer;

class WarcryEditorPlugin : public EditorPlugin {
	GDCLASS(WarcryEditorPlugin, EditorPlugin);

	VBoxContainer *dock_root = nullptr;
	LineEdit *host_edit = nullptr;
	LineEdit *port_edit = nullptr;
	LineEdit *user_edit = nullptr;
	LineEdit *pass_edit = nullptr;
	Label *status_label = nullptr;
	ItemList *channel_list = nullptr;
	ItemList *user_list = nullptr;
	CheckBox *mute_box = nullptr;
	CheckBox *deaf_box = nullptr;
	CheckBox *listen_box = nullptr;
	CheckBox *pref_mute_box = nullptr;
	HSlider *master_slider = nullptr;
	HSlider *user_volume_slider = nullptr;
	Button *ptt_button = nullptr;

	void _setup_dock();
	void _teardown_dock();
	void _load_settings();
	void _save_settings();
	void _refresh_lists();
	void _on_connect_pressed();
	void _on_disconnect_pressed();
	void _on_auth_pressed();
	void _on_join_pressed();
	void _on_leave_pressed();
	void _on_apply_pref_pressed();
	void _on_mute_toggled(bool p_pressed);
	void _on_deaf_toggled(bool p_pressed);
	void _on_listen_toggled(bool p_pressed);
	void _on_master_changed(float p_value);
	void _on_ptt_down();
	void _on_ptt_up();
	void _on_connected();
	void _on_disconnected();
	void _on_server_state();
	void _on_authenticated(const String &p_role);
	void _on_error(const String &p_message);

protected:
	static void _bind_methods() {}
	void _notification(int p_what);

public:
	virtual String get_plugin_name() const override { return "Warcry"; }

	WarcryEditorPlugin();
	~WarcryEditorPlugin();
};

#endif // TOOLS_ENABLED
