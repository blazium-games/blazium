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

class RichTextLabel;
class VBoxContainer;

class AnticheatEditorPlugin : public EditorPlugin {
	GDCLASS(AnticheatEditorPlugin, EditorPlugin);

private:
	VBoxContainer *dock_root = nullptr;
	RichTextLabel *log = nullptr;

	void _append_log(const String &p_line);
	void _on_status_pressed();
	void _on_init_pressed();
	void _on_tick_pressed();
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
