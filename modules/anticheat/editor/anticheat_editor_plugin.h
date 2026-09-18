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
#include "core/variant/variant.h"

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
	void _on_ops_message(const String &p_player_id, const String &p_text);
	void _on_ops_kill(const String &p_player_id);
	void _on_ops_screenshot_request(const String &p_player_id, const String &p_side);
	void _on_server_drop_client(int p_client_index, const String &p_reason);
	void _on_screenshot_ready(const PackedByteArray &p_png, int p_width, int p_height);
	void _on_ops_teleport(const String &p_player_id, const String &p_region, float p_x, float p_y, float p_z);
	void _on_ops_global_message(const String &p_text);
	void _on_ops_action(const String &p_json_line);
	void _on_ops_warn(const String &p_player_id, const String &p_text);
	void _on_ops_mute(const String &p_player_id, const String &p_text, int64_t p_until);
	void _on_ops_unmute(const String &p_player_id);
	void _on_ops_spectate(const String &p_player_id, const String &p_text, int64_t p_until);
	void _setup_dock();
	void _teardown_dock();
	void _connect_signals();
	bool signals_connected = false;

protected:
	static void _bind_methods() {}
	void _notification(int p_what);

public:
	virtual String get_plugin_name() const override { return "Anticheat"; }

	AnticheatEditorPlugin();
	~AnticheatEditorPlugin();
};

#endif
