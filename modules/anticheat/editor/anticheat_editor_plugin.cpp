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
#include "scene/gui/rich_text_label.h"

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
	_append_log(vformat("available=%s initialized=%s enabled=%s",
			ac->is_available() ? "true" : "false",
			ac->is_initialized() ? "true" : "false",
			ac->is_enabled() ? "true" : "false"));
}

void AnticheatEditorPlugin::_on_init_pressed() {
	Anticheat *ac = Anticheat::get_singleton();
	if (!ac) {
		_append_log("Anticheat singleton unavailable");
		return;
	}
	_append_log(vformat("initialize() -> %d", ac->initialize()));
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

	Button *tick = memnew(Button);
	tick->set_text("Tick once");
	tick->connect("pressed", callable_mp(this, &AnticheatEditorPlugin::_on_tick_pressed));
	dock_root->add_child(tick);

	log = memnew(RichTextLabel);
	log->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	log->set_custom_minimum_size(Size2(0, 120));
	dock_root->add_child(log);
}

void AnticheatEditorPlugin::_teardown_dock() {
	if (dock_root) {
		remove_control_from_docks(dock_root);
		dock_root->queue_free();
		dock_root = nullptr;
		log = nullptr;
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
