/**************************************************************************/
/*  anticheat_editor_plugin.h                                             */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#pragma once

#ifdef TOOLS_ENABLED

#include "editor/plugins/editor_plugin.h"

class LineEdit;
class RichTextLabel;
class TextEdit;
class VBoxContainer;

class AnticheatEditorPlugin : public EditorPlugin {
	GDCLASS(AnticheatEditorPlugin, EditorPlugin);

private:
	VBoxContainer *dock_root = nullptr;
	RichTextLabel *log = nullptr;
	LineEdit *player_id_edit = nullptr;
	LineEdit *client_index_edit = nullptr;
	TextEdit *ops_edit = nullptr;

	void _append_log(const String &p_line);
	void _on_status_pressed();
	void _on_init_pressed();
	void _on_sv_init_pressed();
	void _on_ops_connect_pressed();
	void _on_tick_pressed();
	void _on_shutdown_pressed();
	void _on_bind_pressed();
	void _on_unbind_pressed();
	void _on_apply_ops_pressed();
	void _setup_dock();
	void _teardown_dock();

protected:
	static void _bind_methods() {}
	void _notification(int p_what);

public:
	virtual String get_plugin_name() const override { return "Anticheat"; }

	AnticheatEditorPlugin();
	~AnticheatEditorPlugin();
};

#endif
