/**************************************************************************/
/*  steam_editor_plugin.cpp                                               */
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

#include "steam_editor_plugin.h"

#include "core/input/shortcut.h"
#include "editor/themes/editor_scale.h"
#include "modules/steam/steam.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/rich_text_label.h"

void SteamEditorPlugin::_append_log(const String &p_line) {
	if (!log) {
		return;
	}
	log->push_bold();
	log->add_text(p_line);
	log->pop();
	log->add_newline();
}

void SteamEditorPlugin::_on_init_pressed() {
	Steam *steam = Steam::get_singleton();
	if (!steam) {
		_append_log("Steam singleton unavailable");
		return;
	}
	if (!steam->is_available()) {
		_append_log("Steam API library not found");
		return;
	}
	int app_id = app_id_edit ? app_id_edit->get_text().to_int() : 0;
	Error err = steam->initialize(app_id);
	_append_log(vformat("initialize(app_id=%d) -> %d, logged_on=%s, steam_id=%s",
			app_id, (int)err,
			steam->is_logged_on() ? "true" : "false",
			String::num_uint64(steam->get_local_steam_id())));
}

void SteamEditorPlugin::_on_request_ticket_pressed() {
	Steam *steam = Steam::get_singleton();
	if (!steam) {
		_append_log("Steam singleton unavailable");
		return;
	}
	String identity = identity_edit ? identity_edit->get_text() : "blazium";
	steam->request_web_api_ticket(identity);
	for (int i = 0; i < 300 && steam->get_ticket_state() == Steam::TICKET_STATE_PENDING; i++) {
		steam->poll_callbacks();
		OS::get_singleton()->delay_usec(10000);
	}
	if (steam->get_ticket_state() == Steam::TICKET_STATE_READY) {
		last_hex_ticket = steam->get_pending_hex_ticket();
		_append_log(vformat("Ticket ready (%d chars)", last_hex_ticket.length()));
	} else {
		_append_log(vformat("Ticket failed: %s", steam->get_pending_ticket_error()));
	}
}

void SteamEditorPlugin::_on_authenticate_pressed() {
	Steam *steam = Steam::get_singleton();
	if (!steam) {
		_append_log("Steam singleton unavailable");
		return;
	}
	if (last_hex_ticket.is_empty()) {
		last_hex_ticket = steam->get_pending_hex_ticket();
	}
	if (last_hex_ticket.is_empty()) {
		_append_log("No ticket available; request a ticket first");
		return;
	}
	String url = server_url_edit ? server_url_edit->get_text() : "http://127.0.0.1:8080/v1/auth/steam";
	int app_id = app_id_edit ? app_id_edit->get_text().to_int() : 0;
	Ref<SteamAuthResult> result = steam->authenticate_with_server(url, last_hex_ticket, app_id);
	if (result.is_null()) {
		_append_log("Auth result was null");
		return;
	}
	if (result->is_success()) {
		_append_log(vformat("Auth OK persona=%s steam_id=%s jwt_len=%d",
				result->get_persona(), result->get_steam_id(), result->get_jwt().length()));
	} else {
		_append_log(vformat("Auth failed HTTP %d: %s", result->get_http_status(), result->get_error_message()));
	}
}

void SteamEditorPlugin::_on_clear_log_pressed() {
	if (log) {
		log->clear();
	}
	Steam *steam = Steam::get_singleton();
	if (steam) {
		steam->clear_debug_log();
	}
}

void SteamEditorPlugin::_on_refresh_stats_pressed() {
	Steam *steam = Steam::get_singleton();
	if (!steam) {
		_append_log("Steam singleton unavailable");
		return;
	}
	const bool ok = steam->request_current_stats();
	_append_log(vformat("request_current_stats -> %s", ok ? "true" : "false"));
}

void SteamEditorPlugin::_on_unlock_achievement_pressed() {
	Steam *steam = Steam::get_singleton();
	if (!steam) {
		_append_log("Steam singleton unavailable");
		return;
	}
	String achievement_id = achievement_id_edit ? achievement_id_edit->get_text() : String();
	if (achievement_id.is_empty()) {
		_append_log("Achievement ID is empty");
		return;
	}
	const bool set_ok = steam->set_achievement(achievement_id);
	const bool store_ok = steam->store_stats();
	_append_log(vformat("unlock %s set=%s store=%s achieved=%s",
			achievement_id,
			set_ok ? "true" : "false",
			store_ok ? "true" : "false",
			steam->get_achievement(achievement_id) ? "true" : "false"));
}

void SteamEditorPlugin::_on_clear_achievement_pressed() {
	Steam *steam = Steam::get_singleton();
	if (!steam) {
		_append_log("Steam singleton unavailable");
		return;
	}
	String achievement_id = achievement_id_edit ? achievement_id_edit->get_text() : String();
	if (achievement_id.is_empty()) {
		_append_log("Achievement ID is empty");
		return;
	}
	const bool clear_ok = steam->clear_achievement(achievement_id);
	const bool store_ok = steam->store_stats();
	_append_log(vformat("clear %s clear=%s store=%s achieved=%s",
			achievement_id,
			clear_ok ? "true" : "false",
			store_ok ? "true" : "false",
			steam->get_achievement(achievement_id) ? "true" : "false"));
}

void SteamEditorPlugin::_on_load_definitions_pressed() {
	Steam *steam = Steam::get_singleton();
	if (!steam) {
		_append_log("Steam singleton unavailable");
		return;
	}
	const bool ok = steam->request_item_definitions(15.0);
	_append_log(vformat("request_item_definitions -> %s ids=%d", ok ? "true" : "false", steam->get_item_definition_ids().size()));
}

void SteamEditorPlugin::_on_refresh_inventory_pressed() {
	Steam *steam = Steam::get_singleton();
	if (!steam) {
		_append_log("Steam singleton unavailable");
		return;
	}
	const Array items = steam->get_all_items();
	_append_log(vformat("get_all_items -> %d items", items.size()));
}

void SteamEditorPlugin::_on_add_promo_pressed() {
	Steam *steam = Steam::get_singleton();
	if (!steam) {
		_append_log("Steam singleton unavailable");
		return;
	}
	const int def_id = item_def_id_edit ? item_def_id_edit->get_text().to_int() : 0;
	if (def_id <= 0) {
		_append_log("Item def ID is empty");
		return;
	}
	const bool ok = steam->add_promo_item(def_id);
	_append_log(vformat("add_promo_item(%d) -> %s", def_id, ok ? "true" : "false"));
}

void SteamEditorPlugin::_on_ticket_ready(const String &p_hex_ticket, int p_handle) {
	last_hex_ticket = p_hex_ticket;
	_append_log(vformat("Signal ticket ready handle=%d len=%d", p_handle, p_hex_ticket.length()));
}

void SteamEditorPlugin::_on_ticket_failed(const String &p_error) {
	_append_log(vformat("Signal ticket failed: %s", p_error));
}

void SteamEditorPlugin::_setup_dock() {
	GridContainer *fields_grid = memnew(GridContainer);
	fields_grid->set_columns(2);

	Label *app_label = memnew(Label);
	app_label->set_text("App ID");
	fields_grid->add_child(app_label);

	app_id_edit = memnew(LineEdit);
	app_id_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	app_id_edit->set_text("1742110");
	app_id_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	fields_grid->add_child(app_id_edit);

	Label *identity_label = memnew(Label);
	identity_label->set_text("Identity");
	fields_grid->add_child(identity_label);

	identity_edit = memnew(LineEdit);
	identity_edit->set_text("blazium");
	identity_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	fields_grid->add_child(identity_edit);

	Label *url_label = memnew(Label);
	url_label->set_text("Auth URL");
	fields_grid->add_child(url_label);

	server_url_edit = memnew(LineEdit);
	server_url_edit->set_text("http://127.0.0.1:8080/v1/auth/steam");
	server_url_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	fields_grid->add_child(server_url_edit);

	Label *achievement_label = memnew(Label);
	achievement_label->set_text("Achievement");
	fields_grid->add_child(achievement_label);

	achievement_id_edit = memnew(LineEdit);
	achievement_id_edit->set_text("CHEATER_ACHIEVEMENT_2_1");
	achievement_id_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	fields_grid->add_child(achievement_id_edit);

	Label *item_def_label = memnew(Label);
	item_def_label->set_text("Def ID");
	item_def_label->set_tooltip_text("Item definition ID");
	fields_grid->add_child(item_def_label);

	item_def_id_edit = memnew(LineEdit);
	item_def_id_edit->set_placeholder("Item definition ID");
	item_def_id_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	fields_grid->add_child(item_def_id_edit);

	GridContainer *buttons_grid = memnew(GridContainer);
	buttons_grid->set_columns(2);

	Button *init_button = memnew(Button);
	init_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	init_button->set_text("Init Steam");
	init_button->connect(SceneStringName(pressed), callable_mp(this, &SteamEditorPlugin::_on_init_pressed));
	buttons_grid->add_child(init_button);

	Button *auth_button = memnew(Button);
	auth_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	auth_button->set_text("Authenticate");
	auth_button->connect(SceneStringName(pressed), callable_mp(this, &SteamEditorPlugin::_on_authenticate_pressed));
	buttons_grid->add_child(auth_button);

	Button *ticket_button = memnew(Button);
	ticket_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	ticket_button->set_text("Request Ticket");
	ticket_button->connect(SceneStringName(pressed), callable_mp(this, &SteamEditorPlugin::_on_request_ticket_pressed));
	buttons_grid->add_child(ticket_button);

	Button *refresh_stats_button = memnew(Button);
	refresh_stats_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	refresh_stats_button->set_text("Refresh Stats");
	refresh_stats_button->connect(SceneStringName(pressed), callable_mp(this, &SteamEditorPlugin::_on_refresh_stats_pressed));
	buttons_grid->add_child(refresh_stats_button);

	Button *unlock_achievement_button = memnew(Button);
	unlock_achievement_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	unlock_achievement_button->set_text("Unlock Achievement");
	unlock_achievement_button->connect(SceneStringName(pressed), callable_mp(this, &SteamEditorPlugin::_on_unlock_achievement_pressed));
	buttons_grid->add_child(unlock_achievement_button);

	Button *clear_achievement_button = memnew(Button);
	clear_achievement_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	clear_achievement_button->set_text("Clear Achievement");
	clear_achievement_button->connect(SceneStringName(pressed), callable_mp(this, &SteamEditorPlugin::_on_clear_achievement_pressed));
	buttons_grid->add_child(clear_achievement_button);

	Button *load_definitions_button = memnew(Button);
	load_definitions_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	load_definitions_button->set_text("Load Definitions");
	load_definitions_button->connect(SceneStringName(pressed), callable_mp(this, &SteamEditorPlugin::_on_load_definitions_pressed));
	buttons_grid->add_child(load_definitions_button);

	Button *add_promo_button = memnew(Button);
	add_promo_button->set_text("Add Promo");
	add_promo_button->connect(SceneStringName(pressed), callable_mp(this, &SteamEditorPlugin::_on_add_promo_pressed));
	buttons_grid->add_child(add_promo_button);

	Button *refresh_inventory_button = memnew(Button);
	refresh_inventory_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	refresh_inventory_button->set_text("Refresh Inventory");
	refresh_inventory_button->connect(SceneStringName(pressed), callable_mp(this, &SteamEditorPlugin::_on_refresh_inventory_pressed));
	buttons_grid->add_child(refresh_inventory_button);

	Button *clear_button = memnew(Button);
	clear_button->set_text("Clear Log");
	clear_button->connect(SceneStringName(pressed), callable_mp(this, &SteamEditorPlugin::_on_clear_log_pressed));
	buttons_grid->add_child(clear_button);

	log = memnew(RichTextLabel);
	log->set_custom_minimum_size(Vector2(0, 64 * EDSCALE));
	log->set_threaded(true);
	log->set_use_bbcode(true);
	log->set_scroll_follow(true);
	log->set_selection_enabled(true);
	log->set_context_menu_enabled(true);
	log->set_focus_mode(Control::FOCUS_CLICK);
	log->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	log->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	log->set_deselect_on_focus_loss_enabled(false);

	dock_root = memnew(VBoxContainer);
	dock_root->set_name("Steam");
	dock_root->add_child(fields_grid);
	dock_root->add_child(buttons_grid);
	dock_root->add_child(log);
	add_control_to_dock(DOCK_SLOT_RIGHT_UL, dock_root);

	Steam *steam = Steam::get_singleton();
	if (steam) {
		steam->connect("web_api_ticket_ready", callable_mp(this, &SteamEditorPlugin::_on_ticket_ready));
		steam->connect("web_api_ticket_failed", callable_mp(this, &SteamEditorPlugin::_on_ticket_failed));
	}
}

void SteamEditorPlugin::_teardown_dock() {
	if (!dock_root) {
		return;
	}

	Steam *steam = Steam::get_singleton();
	if (steam) {
		steam->disconnect("web_api_ticket_ready", callable_mp(this, &SteamEditorPlugin::_on_ticket_ready));
		steam->disconnect("web_api_ticket_failed", callable_mp(this, &SteamEditorPlugin::_on_ticket_failed));
	}

	remove_control_from_docks(dock_root);
	dock_root->queue_free();
	dock_root = nullptr;
	app_id_edit = nullptr;
	identity_edit = nullptr;
	server_url_edit = nullptr;
	achievement_id_edit = nullptr;
	item_def_id_edit = nullptr;
	log = nullptr;
}

SteamEditorPlugin::SteamEditorPlugin() {
}

SteamEditorPlugin::~SteamEditorPlugin() {
}

void SteamEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_setup_dock();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			_teardown_dock();
		} break;
	}
}

#endif // TOOLS_ENABLED
