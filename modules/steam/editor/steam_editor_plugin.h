/**************************************************************************/
/*  steam_editor_plugin.h                                                 */
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

class LineEdit;
class RichTextLabel;
class VBoxContainer;

class SteamEditorPlugin : public EditorPlugin {
	GDCLASS(SteamEditorPlugin, EditorPlugin);

private:
	VBoxContainer *dock_root;
	LineEdit *app_id_edit;
	LineEdit *identity_edit;
	LineEdit *server_url_edit;
	LineEdit *achievement_id_edit;
	LineEdit *item_def_id_edit;
	RichTextLabel *log;
	String last_hex_ticket;

	void _append_log(const String &p_line);
	void _on_init_pressed();
	void _on_request_ticket_pressed();
	void _on_authenticate_pressed();
	void _on_clear_log_pressed();
	void _on_refresh_stats_pressed();
	void _on_unlock_achievement_pressed();
	void _on_clear_achievement_pressed();
	void _on_load_definitions_pressed();
	void _on_refresh_inventory_pressed();
	void _on_add_promo_pressed();
	void _on_ticket_ready(const String &p_hex_ticket, int p_handle);
	void _on_ticket_failed(const String &p_error);
	void _setup_dock();
	void _teardown_dock();

protected:
	static void _bind_methods() {}
	void _notification(int p_what);

public:
	virtual String get_plugin_name() const override { return "Steam"; }

	SteamEditorPlugin();
	~SteamEditorPlugin();
};

#endif // TOOLS_ENABLED
