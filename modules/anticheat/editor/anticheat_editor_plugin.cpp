/**************************************************************************/
/*  anticheat_editor_plugin.cpp                                           */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#ifdef TOOLS_ENABLED

#include "anticheat_editor_plugin.h"

#include "core/object/callable_mp.h"
#include "modules/anticheat/anticheat.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/text_edit.h"

void AnticheatEditorPlugin::_append_log(const String &p_line) {
	if (!log) {
		return;
	}
	log->add_text(p_line);
	log->add_newline();
}

void AnticheatEditorPlugin::_on_status_pressed() {
	Anticheat *ac = Anticheat::get_singleton();
	if (!ac) {
		_append_log("Anticheat singleton unavailable");
		return;
	}
	_append_log(vformat("available=%s initialized=%s enabled=%s server_initialized=%s ops_connected=%s",
			ac->is_available() ? "true" : "false",
			ac->is_initialized() ? "true" : "false",
			ac->is_enabled() ? "true" : "false",
			ac->is_server_initialized() ? "true" : "false",
			ac->is_ops_connected() ? "true" : "false"));
}

void AnticheatEditorPlugin::_on_init_pressed() {
	Anticheat *ac = Anticheat::get_singleton();
	if (!ac) {
		_append_log("Anticheat singleton unavailable");
		return;
	}
	_append_log(vformat("initialize() -> %d", ac->initialize()));
}

void AnticheatEditorPlugin::_on_sv_init_pressed() {
	Anticheat *ac = Anticheat::get_singleton();
	if (!ac) {
		_append_log("Anticheat singleton unavailable");
		return;
	}
	_append_log(vformat("sv_initialize() -> %d", ac->sv_initialize()));
}

void AnticheatEditorPlugin::_on_ops_connect_pressed() {
	Anticheat *ac = Anticheat::get_singleton();
	if (!ac) {
		_append_log("Anticheat singleton unavailable");
		return;
	}
	_append_log(vformat("ops_connect() -> %d", ac->ops_connect()));
}

void AnticheatEditorPlugin::_on_tick_pressed() {
	Anticheat *ac = Anticheat::get_singleton();
	if (!ac) {
		_append_log("Anticheat singleton unavailable");
		return;
	}
	ac->tick(0);
	_append_log("tick()");
}

void AnticheatEditorPlugin::_on_shutdown_pressed() {
	Anticheat *ac = Anticheat::get_singleton();
	if (!ac) {
		_append_log("Anticheat singleton unavailable");
		return;
	}
	ac->shutdown();
	_append_log("shutdown()");
}

void AnticheatEditorPlugin::_on_bind_pressed() {
	Anticheat *ac = Anticheat::get_singleton();
	if (!ac || !player_id_edit || !client_index_edit) {
		return;
	}
	const int idx = client_index_edit->get_text().to_int();
	ac->bind_player(idx, player_id_edit->get_text());
	_append_log(vformat("bind_player(%d, %s)", idx, player_id_edit->get_text()));
}

void AnticheatEditorPlugin::_on_unbind_pressed() {
	Anticheat *ac = Anticheat::get_singleton();
	if (!ac || !client_index_edit) {
		return;
	}
	const int idx = client_index_edit->get_text().to_int();
	ac->unbind_player(idx);
	_append_log(vformat("unbind_player(%d)", idx));
}

void AnticheatEditorPlugin::_on_apply_ops_pressed() {
	Anticheat *ac = Anticheat::get_singleton();
	if (!ac || !ops_edit) {
		return;
	}
	const String line = ops_edit->get_text();
	ac->apply_ops_line(line);
	_append_log("apply_ops_line()");
}

void AnticheatEditorPlugin::_on_ops_message(const String &p_player_id, const String &p_text) {
	_append_log(vformat("ops_message player=%s text=%s", p_player_id, p_text));
}

void AnticheatEditorPlugin::_on_ops_kill(const String &p_player_id) {
	_append_log(vformat("ops_kill player=%s", p_player_id));
}

void AnticheatEditorPlugin::_on_ops_screenshot_request(const String &p_player_id, const String &p_side) {
	_append_log(vformat("ops_screenshot_request player=%s side=%s", p_player_id, p_side));
}

void AnticheatEditorPlugin::_on_server_drop_client(int p_client_index, const String &p_reason) {
	_append_log(vformat("server_drop_client index=%d reason=%s", p_client_index, p_reason));
}

void AnticheatEditorPlugin::_on_screenshot_ready(const PackedByteArray &p_png, int p_width, int p_height) {
	_append_log(vformat("screenshot_ready %dx%d bytes=%d", p_width, p_height, p_png.size()));
}

void AnticheatEditorPlugin::_on_ops_teleport(const String &p_player_id, const String &p_region, float p_x, float p_y, float p_z) {
	_append_log(vformat("ops_teleport player=%s region=%s x=%s y=%s z=%s", p_player_id, p_region, rtos(p_x), rtos(p_y), rtos(p_z)));
}

void AnticheatEditorPlugin::_on_ops_global_message(const String &p_text) {
	_append_log(vformat("ops_global_message text=%s", p_text));
}

void AnticheatEditorPlugin::_on_ops_action(const String &p_json_line) {
	_append_log(vformat("ops_action %s", p_json_line));
}

void AnticheatEditorPlugin::_on_ops_warn(const String &p_player_id, const String &p_text) {
	_append_log(vformat("ops_warn player=%s text=%s", p_player_id, p_text));
}

void AnticheatEditorPlugin::_on_ops_mute(const String &p_player_id, const String &p_text, int64_t p_until) {
	_append_log(vformat("ops_mute player=%s text=%s until=%d", p_player_id, p_text, (int)p_until));
}

void AnticheatEditorPlugin::_on_ops_unmute(const String &p_player_id) {
	_append_log(vformat("ops_unmute player=%s", p_player_id));
}

void AnticheatEditorPlugin::_on_ops_spectate(const String &p_player_id, const String &p_text, int64_t p_until) {
	_append_log(vformat("ops_spectate player=%s text=%s until=%d", p_player_id, p_text, (int)p_until));
}

void AnticheatEditorPlugin::_connect_signals() {
	Anticheat *ac = Anticheat::get_singleton();
	if (!ac || signals_connected) {
		return;
	}
	ac->connect("ops_message", callable_mp(this, &AnticheatEditorPlugin::_on_ops_message));
	ac->connect("ops_kill", callable_mp(this, &AnticheatEditorPlugin::_on_ops_kill));
	ac->connect("ops_screenshot_request", callable_mp(this, &AnticheatEditorPlugin::_on_ops_screenshot_request));
	ac->connect("server_drop_client", callable_mp(this, &AnticheatEditorPlugin::_on_server_drop_client));
	ac->connect("screenshot_ready", callable_mp(this, &AnticheatEditorPlugin::_on_screenshot_ready));
	ac->connect("ops_teleport", callable_mp(this, &AnticheatEditorPlugin::_on_ops_teleport));
	ac->connect("ops_global_message", callable_mp(this, &AnticheatEditorPlugin::_on_ops_global_message));
	ac->connect("ops_action", callable_mp(this, &AnticheatEditorPlugin::_on_ops_action));
	ac->connect("ops_warn", callable_mp(this, &AnticheatEditorPlugin::_on_ops_warn));
	ac->connect("ops_mute", callable_mp(this, &AnticheatEditorPlugin::_on_ops_mute));
	ac->connect("ops_unmute", callable_mp(this, &AnticheatEditorPlugin::_on_ops_unmute));
	ac->connect("ops_spectate", callable_mp(this, &AnticheatEditorPlugin::_on_ops_spectate));
	signals_connected = true;
}

void AnticheatEditorPlugin::_setup_dock() {
	dock_root = memnew(VBoxContainer);
	add_control_to_dock(DOCK_SLOT_RIGHT_UL, dock_root);

	Button *status = memnew(Button);
	status->set_text("Status");
	status->connect("pressed", callable_mp(this, &AnticheatEditorPlugin::_on_status_pressed));
	dock_root->add_child(status);

	Button *init = memnew(Button);
	init->set_text("Initialize");
	init->connect("pressed", callable_mp(this, &AnticheatEditorPlugin::_on_init_pressed));
	dock_root->add_child(init);

	Button *sv = memnew(Button);
	sv->set_text("sv_initialize");
	sv->connect("pressed", callable_mp(this, &AnticheatEditorPlugin::_on_sv_init_pressed));
	dock_root->add_child(sv);

	Button *ops = memnew(Button);
	ops->set_text("ops_connect");
	ops->connect("pressed", callable_mp(this, &AnticheatEditorPlugin::_on_ops_connect_pressed));
	dock_root->add_child(ops);

	Button *tick = memnew(Button);
	tick->set_text("Tick once");
	tick->connect("pressed", callable_mp(this, &AnticheatEditorPlugin::_on_tick_pressed));
	dock_root->add_child(tick);

	Button *shut = memnew(Button);
	shut->set_text("Shutdown");
	shut->connect("pressed", callable_mp(this, &AnticheatEditorPlugin::_on_shutdown_pressed));
	dock_root->add_child(shut);

	player_id_edit = memnew(LineEdit);
	player_id_edit->set_placeholder("player_id");
	dock_root->add_child(player_id_edit);

	client_index_edit = memnew(LineEdit);
	client_index_edit->set_placeholder("client_index");
	dock_root->add_child(client_index_edit);

	Button *bind = memnew(Button);
	bind->set_text("Bind player");
	bind->connect("pressed", callable_mp(this, &AnticheatEditorPlugin::_on_bind_pressed));
	dock_root->add_child(bind);

	Button *unbind = memnew(Button);
	unbind->set_text("Unbind player");
	unbind->connect("pressed", callable_mp(this, &AnticheatEditorPlugin::_on_unbind_pressed));
	dock_root->add_child(unbind);

	ops_edit = memnew(TextEdit);
	ops_edit->set_custom_minimum_size(Size2(0, 72));
	ops_edit->set_placeholder("{\"event\":\"Kick\",\"player_id\":\"\"}");
	dock_root->add_child(ops_edit);

	Button *apply = memnew(Button);
	apply->set_text("Apply ops line");
	apply->connect("pressed", callable_mp(this, &AnticheatEditorPlugin::_on_apply_ops_pressed));
	dock_root->add_child(apply);

	log = memnew(RichTextLabel);
	log->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	log->set_custom_minimum_size(Size2(0, 120));
	dock_root->add_child(log);
	_connect_signals();
}

void AnticheatEditorPlugin::_teardown_dock() {
	if (signals_connected) {
		Anticheat *ac = Anticheat::get_singleton();
		if (ac) {
			if (ac->is_connected("ops_message", callable_mp(this, &AnticheatEditorPlugin::_on_ops_message))) {
				ac->disconnect("ops_message", callable_mp(this, &AnticheatEditorPlugin::_on_ops_message));
			}
			if (ac->is_connected("ops_kill", callable_mp(this, &AnticheatEditorPlugin::_on_ops_kill))) {
				ac->disconnect("ops_kill", callable_mp(this, &AnticheatEditorPlugin::_on_ops_kill));
			}
			if (ac->is_connected("ops_screenshot_request", callable_mp(this, &AnticheatEditorPlugin::_on_ops_screenshot_request))) {
				ac->disconnect("ops_screenshot_request", callable_mp(this, &AnticheatEditorPlugin::_on_ops_screenshot_request));
			}
			if (ac->is_connected("server_drop_client", callable_mp(this, &AnticheatEditorPlugin::_on_server_drop_client))) {
				ac->disconnect("server_drop_client", callable_mp(this, &AnticheatEditorPlugin::_on_server_drop_client));
			}
			if (ac->is_connected("screenshot_ready", callable_mp(this, &AnticheatEditorPlugin::_on_screenshot_ready))) {
				ac->disconnect("screenshot_ready", callable_mp(this, &AnticheatEditorPlugin::_on_screenshot_ready));
			}
			if (ac->is_connected("ops_teleport", callable_mp(this, &AnticheatEditorPlugin::_on_ops_teleport))) {
				ac->disconnect("ops_teleport", callable_mp(this, &AnticheatEditorPlugin::_on_ops_teleport));
			}
			if (ac->is_connected("ops_global_message", callable_mp(this, &AnticheatEditorPlugin::_on_ops_global_message))) {
				ac->disconnect("ops_global_message", callable_mp(this, &AnticheatEditorPlugin::_on_ops_global_message));
			}
			if (ac->is_connected("ops_action", callable_mp(this, &AnticheatEditorPlugin::_on_ops_action))) {
				ac->disconnect("ops_action", callable_mp(this, &AnticheatEditorPlugin::_on_ops_action));
			}
			if (ac->is_connected("ops_warn", callable_mp(this, &AnticheatEditorPlugin::_on_ops_warn))) {
				ac->disconnect("ops_warn", callable_mp(this, &AnticheatEditorPlugin::_on_ops_warn));
			}
			if (ac->is_connected("ops_mute", callable_mp(this, &AnticheatEditorPlugin::_on_ops_mute))) {
				ac->disconnect("ops_mute", callable_mp(this, &AnticheatEditorPlugin::_on_ops_mute));
			}
			if (ac->is_connected("ops_unmute", callable_mp(this, &AnticheatEditorPlugin::_on_ops_unmute))) {
				ac->disconnect("ops_unmute", callable_mp(this, &AnticheatEditorPlugin::_on_ops_unmute));
			}
			if (ac->is_connected("ops_spectate", callable_mp(this, &AnticheatEditorPlugin::_on_ops_spectate))) {
				ac->disconnect("ops_spectate", callable_mp(this, &AnticheatEditorPlugin::_on_ops_spectate));
			}
		}
		signals_connected = false;
	}
	if (dock_root) {
		remove_control_from_docks(dock_root);
		dock_root->queue_free();
		dock_root = nullptr;
		log = nullptr;
		player_id_edit = nullptr;
		client_index_edit = nullptr;
		ops_edit = nullptr;
	}
}

void AnticheatEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
			_setup_dock();
			break;
		case NOTIFICATION_EXIT_TREE:
			_teardown_dock();
			break;
		default:
			break;
	}
}

AnticheatEditorPlugin::AnticheatEditorPlugin() = default;

AnticheatEditorPlugin::~AnticheatEditorPlugin() {
	_teardown_dock();
}

#endif
