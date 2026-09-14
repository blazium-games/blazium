/**************************************************************************/
/*  warcry_editor_plugin.cpp                                              */
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

#include "warcry_editor_plugin.h"

#include "core/config/project_settings.h"
#include "core/input/shortcut.h"
#include "core/object/class_db.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/item_list.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/slider.h"
#include "scene/scene_string_names.h"
#include "servers/text_server.h"

#include "warcry_client.h"

void WarcryEditorPlugin::_load_settings() {
	if (!host_edit) {
		return;
	}
	host_edit->set_text(GLOBAL_GET("blazium/warcry/host"));
	port_edit->set_text(itos((int)GLOBAL_GET("blazium/warcry/port")));
	user_edit->set_text(GLOBAL_GET("blazium/warcry/username"));
	pass_edit->set_text(GLOBAL_GET("blazium/warcry/password"));
}

void WarcryEditorPlugin::_save_settings() {
	if (!host_edit) {
		return;
	}
	ProjectSettings::get_singleton()->set("blazium/warcry/host", host_edit->get_text());
	ProjectSettings::get_singleton()->set("blazium/warcry/port", port_edit->get_text().to_int());
	ProjectSettings::get_singleton()->set("blazium/warcry/username", user_edit->get_text());
	ProjectSettings::get_singleton()->set("blazium/warcry/password", pass_edit->get_text());
}

void WarcryEditorPlugin::_refresh_lists() {
	WarcryClient *client = WarcryClient::get_singleton();
	if (!client || !channel_list || !user_list) {
		return;
	}

	const int keep_channel = channel_list->get_selected_items().is_empty() ? -1 : (int)channel_list->get_item_metadata(channel_list->get_selected_items()[0]);
	const int keep_user = user_list->get_selected_items().is_empty() ? -1 : (int)user_list->get_item_metadata(user_list->get_selected_items()[0]);

	channel_list->clear();
	const TypedArray<Dictionary> channels = client->get_channels();
	for (int i = 0; i < channels.size(); i++) {
		const Dictionary d = channels[i];
		const int id = d.get("id", 0);
		const String name = d.get("name", String());
		const int members = d.get("member_count", 0);
		const int idx = channel_list->add_item(vformat("%s (%d) [%d]", name, id, members));
		channel_list->set_item_metadata(idx, id);
		if (id == keep_channel || id == client->get_current_channel()) {
			channel_list->select(idx);
		}
	}

	user_list->clear();
	const TypedArray<Dictionary> users = client->get_users();
	for (int i = 0; i < users.size(); i++) {
		const Dictionary d = users[i];
		const int id = d.get("id", 0);
		const String name = d.get("username", String());
		const int ch = d.get("channel_id", 0);
		const int idx = user_list->add_item(vformat("%s #%d ch=%d", name, id, ch));
		user_list->set_item_metadata(idx, id);
		if (id == keep_user) {
			user_list->select(idx);
		}
	}

	if (status_label) {
		status_label->set_text(vformat("user=%d channel=%d %s", client->get_local_user_id(), client->get_current_channel(), client->is_connected() ? "connected" : "offline"));
	}
}

void WarcryEditorPlugin::_on_connect_pressed() {
	WarcryClient *client = WarcryClient::get_singleton();
	if (!client) {
		return;
	}
	_save_settings();
	if (!client->connect_to_server(host_edit->get_text(), port_edit->get_text().to_int(), user_edit->get_text())) {
		status_label->set_text("Connect failed");
	} else {
		status_label->set_text("Connecting...");
	}
}

void WarcryEditorPlugin::_on_disconnect_pressed() {
	WarcryClient *client = WarcryClient::get_singleton();
	if (client) {
		client->disconnect_from_server();
	}
}

void WarcryEditorPlugin::_on_auth_pressed() {
	WarcryClient *client = WarcryClient::get_singleton();
	if (!client) {
		return;
	}
	_save_settings();
	if (!client->authenticate(user_edit->get_text(), pass_edit->get_text())) {
		status_label->set_text("AUTH failed (not connected)");
	}
}

void WarcryEditorPlugin::_on_join_pressed() {
	WarcryClient *client = WarcryClient::get_singleton();
	if (!client || !channel_list) {
		return;
	}
	const Vector<int> selected = channel_list->get_selected_items();
	if (selected.is_empty()) {
		return;
	}
	client->join_channel((int)channel_list->get_item_metadata(selected[0]));
}

void WarcryEditorPlugin::_on_leave_pressed() {
	WarcryClient *client = WarcryClient::get_singleton();
	if (!client || !channel_list) {
		return;
	}
	const Vector<int> selected = channel_list->get_selected_items();
	const int channel_id = selected.is_empty() ? client->get_current_channel() : (int)channel_list->get_item_metadata(selected[0]);
	client->leave_channel(channel_id);
}

void WarcryEditorPlugin::_on_apply_pref_pressed() {
	WarcryClient *client = WarcryClient::get_singleton();
	if (!client || !user_list) {
		return;
	}
	const Vector<int> selected = user_list->get_selected_items();
	if (selected.is_empty()) {
		return;
	}
	client->set_user_preference((int)user_list->get_item_metadata(selected[0]), (float)user_volume_slider->get_value(), pref_mute_box->is_pressed());
}

void WarcryEditorPlugin::_on_mute_toggled(bool p_pressed) {
	WarcryClient *client = WarcryClient::get_singleton();
	if (client) {
		client->set_muted(p_pressed);
	}
}

void WarcryEditorPlugin::_on_deaf_toggled(bool p_pressed) {
	WarcryClient *client = WarcryClient::get_singleton();
	if (client) {
		client->set_deaf(p_pressed);
	}
}

void WarcryEditorPlugin::_on_listen_toggled(bool p_pressed) {
	WarcryClient *client = WarcryClient::get_singleton();
	if (!client || !dock_root) {
		return;
	}
	if (p_pressed) {
		client->setup_audio_io(dock_root);
	}
}

void WarcryEditorPlugin::_on_master_changed(float p_value) {
	WarcryClient *client = WarcryClient::get_singleton();
	if (client) {
		client->set_master_volume(p_value);
	}
}

void WarcryEditorPlugin::_on_ptt_down() {
	WarcryClient *client = WarcryClient::get_singleton();
	if (!client) {
		return;
	}
	if (listen_box && listen_box->is_pressed()) {
		client->setup_audio_io(dock_root);
	}
	client->start_speaking();
}

void WarcryEditorPlugin::_on_ptt_up() {
	WarcryClient *client = WarcryClient::get_singleton();
	if (client) {
		client->stop_speaking();
	}
}

void WarcryEditorPlugin::_on_connected() {
	if (status_label) {
		status_label->set_text("HELLO_ACK");
	}
	_refresh_lists();
}

void WarcryEditorPlugin::_on_disconnected() {
	if (status_label) {
		status_label->set_text("Disconnected");
	}
	if (channel_list) {
		channel_list->clear();
	}
	if (user_list) {
		user_list->clear();
	}
}

void WarcryEditorPlugin::_on_server_state() {
	_refresh_lists();
}

void WarcryEditorPlugin::_on_authenticated(const String &p_role) {
	if (status_label) {
		status_label->set_text(vformat("Authenticated: %s", p_role));
	}
}

void WarcryEditorPlugin::_on_error(const String &p_message) {
	if (status_label) {
		status_label->set_text(p_message);
	}
}

void WarcryEditorPlugin::_setup_dock() {
	GridContainer *fields = memnew(GridContainer);
	fields->set_columns(2);

	Label *host_label = memnew(Label);
	host_label->set_text("Host");
	fields->add_child(host_label);
	host_edit = memnew(LineEdit);
	host_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	fields->add_child(host_edit);

	Label *port_label = memnew(Label);
	port_label->set_text("Port");
	fields->add_child(port_label);
	port_edit = memnew(LineEdit);
	port_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	fields->add_child(port_edit);

	Label *user_label = memnew(Label);
	user_label->set_text("Username");
	fields->add_child(user_label);
	user_edit = memnew(LineEdit);
	user_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	fields->add_child(user_edit);

	Label *pass_label = memnew(Label);
	pass_label->set_text("Password");
	fields->add_child(pass_label);
	pass_edit = memnew(LineEdit);
	pass_edit->set_secret(true);
	pass_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	fields->add_child(pass_edit);

	GridContainer *buttons = memnew(GridContainer);
	buttons->set_columns(2);

	Button *connect_btn = memnew(Button);
	connect_btn->set_text("Connect");
	connect_btn->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	connect_btn->connect(SceneStringName(pressed), callable_mp(this, &WarcryEditorPlugin::_on_connect_pressed));
	buttons->add_child(connect_btn);

	Button *disconnect_btn = memnew(Button);
	disconnect_btn->set_text("Disconnect");
	disconnect_btn->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	disconnect_btn->connect(SceneStringName(pressed), callable_mp(this, &WarcryEditorPlugin::_on_disconnect_pressed));
	buttons->add_child(disconnect_btn);

	Button *auth_btn = memnew(Button);
	auth_btn->set_text("Authenticate");
	auth_btn->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	auth_btn->connect(SceneStringName(pressed), callable_mp(this, &WarcryEditorPlugin::_on_auth_pressed));
	buttons->add_child(auth_btn);

	Button *join_btn = memnew(Button);
	join_btn->set_text("Join channel");
	join_btn->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	join_btn->connect(SceneStringName(pressed), callable_mp(this, &WarcryEditorPlugin::_on_join_pressed));
	buttons->add_child(join_btn);

	Button *leave_btn = memnew(Button);
	leave_btn->set_text("Leave channel");
	leave_btn->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	leave_btn->connect(SceneStringName(pressed), callable_mp(this, &WarcryEditorPlugin::_on_leave_pressed));
	buttons->add_child(leave_btn);

	Button *pref_btn = memnew(Button);
	pref_btn->set_text("Apply user pref");
	pref_btn->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	pref_btn->connect(SceneStringName(pressed), callable_mp(this, &WarcryEditorPlugin::_on_apply_pref_pressed));
	buttons->add_child(pref_btn);

	status_label = memnew(Label);
	status_label->set_text("Offline");
	status_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);

	Label *ch_label = memnew(Label);
	ch_label->set_text("Channels");
	channel_list = memnew(ItemList);
	channel_list->set_custom_minimum_size(Size2(0, 72 * EDSCALE));
	channel_list->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	channel_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	Label *user_list_label = memnew(Label);
	user_list_label->set_text("Users");
	user_list = memnew(ItemList);
	user_list->set_custom_minimum_size(Size2(0, 72 * EDSCALE));
	user_list->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	user_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	mute_box = memnew(CheckBox);
	mute_box->set_text("Mute self");
	mute_box->connect(SceneStringName(toggled), callable_mp(this, &WarcryEditorPlugin::_on_mute_toggled));

	deaf_box = memnew(CheckBox);
	deaf_box->set_text("Deaf");
	deaf_box->connect(SceneStringName(toggled), callable_mp(this, &WarcryEditorPlugin::_on_deaf_toggled));

	listen_box = memnew(CheckBox);
	listen_box->set_text("Listen");
	listen_box->connect(SceneStringName(toggled), callable_mp(this, &WarcryEditorPlugin::_on_listen_toggled));

	pref_mute_box = memnew(CheckBox);
	pref_mute_box->set_text("Mute selected user");

	Label *master_label = memnew(Label);
	master_label->set_text("Master volume");
	master_slider = memnew(HSlider);
	master_slider->set_min(0);
	master_slider->set_max(2);
	master_slider->set_step(0.05);
	master_slider->set_value(1);
	master_slider->connect(SNAME("value_changed"), callable_mp(this, &WarcryEditorPlugin::_on_master_changed));

	Label *user_vol_label = memnew(Label);
	user_vol_label->set_text("Selected user volume");
	user_volume_slider = memnew(HSlider);
	user_volume_slider->set_min(0);
	user_volume_slider->set_max(2);
	user_volume_slider->set_step(0.05);
	user_volume_slider->set_value(1);

	ptt_button = memnew(Button);
	ptt_button->set_text("Hold to talk");
	ptt_button->connect(SNAME("button_down"), callable_mp(this, &WarcryEditorPlugin::_on_ptt_down));
	ptt_button->connect(SNAME("button_up"), callable_mp(this, &WarcryEditorPlugin::_on_ptt_up));

	dock_root = memnew(VBoxContainer);
	dock_root->set_name("Warcry");
	dock_root->add_child(fields);
	dock_root->add_child(buttons);
	dock_root->add_child(status_label);
	dock_root->add_child(ch_label);
	dock_root->add_child(channel_list);
	dock_root->add_child(user_list_label);
	dock_root->add_child(user_list);
	dock_root->add_child(mute_box);
	dock_root->add_child(deaf_box);
	dock_root->add_child(listen_box);
	dock_root->add_child(master_label);
	dock_root->add_child(master_slider);
	dock_root->add_child(pref_mute_box);
	dock_root->add_child(user_vol_label);
	dock_root->add_child(user_volume_slider);
	dock_root->add_child(ptt_button);
	add_control_to_dock(DOCK_SLOT_RIGHT_UL, dock_root);

	_load_settings();

	WarcryClient *client = WarcryClient::get_singleton();
	if (client) {
		client->connect(SNAME("connected"), callable_mp(this, &WarcryEditorPlugin::_on_connected));
		client->connect(SNAME("disconnected"), callable_mp(this, &WarcryEditorPlugin::_on_disconnected));
		client->connect(SNAME("server_state"), callable_mp(this, &WarcryEditorPlugin::_on_server_state));
		client->connect(SNAME("authenticated"), callable_mp(this, &WarcryEditorPlugin::_on_authenticated));
		client->connect(SNAME("error"), callable_mp(this, &WarcryEditorPlugin::_on_error));
	}
}

void WarcryEditorPlugin::_teardown_dock() {
	WarcryClient *client = WarcryClient::get_singleton();
	if (client) {
		if (client->Object::is_connected(SNAME("connected"), callable_mp(this, &WarcryEditorPlugin::_on_connected))) {
			client->disconnect(SNAME("connected"), callable_mp(this, &WarcryEditorPlugin::_on_connected));
		}
		if (client->Object::is_connected(SNAME("disconnected"), callable_mp(this, &WarcryEditorPlugin::_on_disconnected))) {
			client->disconnect(SNAME("disconnected"), callable_mp(this, &WarcryEditorPlugin::_on_disconnected));
		}
		if (client->Object::is_connected(SNAME("server_state"), callable_mp(this, &WarcryEditorPlugin::_on_server_state))) {
			client->disconnect(SNAME("server_state"), callable_mp(this, &WarcryEditorPlugin::_on_server_state));
		}
		if (client->Object::is_connected(SNAME("authenticated"), callable_mp(this, &WarcryEditorPlugin::_on_authenticated))) {
			client->disconnect(SNAME("authenticated"), callable_mp(this, &WarcryEditorPlugin::_on_authenticated));
		}
		if (client->Object::is_connected(SNAME("error"), callable_mp(this, &WarcryEditorPlugin::_on_error))) {
			client->disconnect(SNAME("error"), callable_mp(this, &WarcryEditorPlugin::_on_error));
		}
		client->set_playback(Ref<AudioStreamGeneratorPlayback>());
		client->set_capture_effect(Ref<AudioEffectCapture>());
	}

	if (dock_root) {
		remove_control_from_docks(dock_root);
		dock_root->queue_free();
		dock_root = nullptr;
	}
	host_edit = nullptr;
	port_edit = nullptr;
	user_edit = nullptr;
	pass_edit = nullptr;
	status_label = nullptr;
	channel_list = nullptr;
	user_list = nullptr;
	mute_box = nullptr;
	deaf_box = nullptr;
	listen_box = nullptr;
	pref_mute_box = nullptr;
	master_slider = nullptr;
	user_volume_slider = nullptr;
	ptt_button = nullptr;
}

WarcryEditorPlugin::WarcryEditorPlugin() {
}

WarcryEditorPlugin::~WarcryEditorPlugin() {
}

void WarcryEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_setup_dock();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			WarcryClient *client = WarcryClient::get_singleton();
			if (client) {
				client->poll();
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			_teardown_dock();
		} break;
	}
}

#endif // TOOLS_ENABLED
