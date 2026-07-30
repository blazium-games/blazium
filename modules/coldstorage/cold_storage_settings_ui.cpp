/**************************************************************************/
/*  cold_storage_settings_ui.cpp                                          */
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

#include "cold_storage_settings_ui.h"

#include "cold_storage_async.h"
#include "cold_storage_editor_plugin.h"
#include "cold_storage_settings.h"
#include "cold_storage_vcs.h"
#include "core/config/project_settings.h"
#include "core/os/os.h"
#include "editor/editor_vcs_interface.h"
#include "editor/plugins/version_control_editor_plugin.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/spin_box.h"

void ColdStorageSettingsUI::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_connect"), &ColdStorageSettingsUI::_on_connect);
	ClassDB::bind_method(D_METHOD("_on_disconnect"), &ColdStorageSettingsUI::_on_disconnect);
	ClassDB::bind_method(D_METHOD("_on_test"), &ColdStorageSettingsUI::_on_test);
	ClassDB::bind_method(D_METHOD("_on_save"), &ColdStorageSettingsUI::_on_save);
	ClassDB::bind_method(D_METHOD("_on_async_complete", "ok", "error", "kind"), &ColdStorageSettingsUI::_on_async_complete);
}

void ColdStorageSettingsUI::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		_load_from_settings();
		_refresh_status();
	}
}

ColdStorageSettingsUI::ColdStorageSettingsUI() {
	set_custom_minimum_size(Size2(0, 220));

	VBoxContainer *vb = memnew(VBoxContainer);
	add_child(vb);

	status_label = memnew(Label);
	status_label->set_text("ColdStorage: Disconnected");
	vb->add_child(status_label);

	host_edit = memnew(LineEdit);
	host_edit->set_placeholder("Host");
	vb->add_child(host_edit);

	port_spin = memnew(SpinBox);
	port_spin->set_min(1);
	port_spin->set_max(65535);
	port_spin->set_step(1);
	port_spin->set_value(1666);
	vb->add_child(port_spin);

	user_edit = memnew(LineEdit);
	user_edit->set_placeholder("User");
	vb->add_child(user_edit);

	password_edit = memnew(LineEdit);
	password_edit->set_placeholder("Password (stored in Editor Settings only)");
	password_edit->set_secret(true);
	vb->add_child(password_edit);

	workspace_edit = memnew(LineEdit);
	workspace_edit->set_placeholder("Workspace");
	vb->add_child(workspace_edit);

	repo_edit = memnew(LineEdit);
	repo_edit->set_placeholder("Repo");
	vb->add_child(repo_edit);

	jwt_edit = memnew(LineEdit);
	jwt_edit->set_placeholder("JWT (optional, Editor Settings only)");
	jwt_edit->set_secret(true);
	vb->add_child(jwt_edit);

	tls_cb = memnew(CheckBox);
	tls_cb->set_text("Use TLS");
	vb->add_child(tls_cb);

	tls_insecure_cb = memnew(CheckBox);
	tls_insecure_cb->set_text("TLS insecure (skip verify)");
	vb->add_child(tls_insecure_cb);

	auto_pull_cb = memnew(CheckBox);
	auto_pull_cb->set_text("Auto-pull on connect");
	vb->add_child(auto_pull_cb);

	HBoxContainer *hb = memnew(HBoxContainer);
	vb->add_child(hb);

	connect_btn = memnew(Button);
	connect_btn->set_text("Connect");
	connect_btn->connect("pressed", callable_mp(this, &ColdStorageSettingsUI::_on_connect));
	hb->add_child(connect_btn);

	disconnect_btn = memnew(Button);
	disconnect_btn->set_text("Disconnect");
	disconnect_btn->connect("pressed", callable_mp(this, &ColdStorageSettingsUI::_on_disconnect));
	hb->add_child(disconnect_btn);

	test_btn = memnew(Button);
	test_btn->set_text("Test");
	test_btn->connect("pressed", callable_mp(this, &ColdStorageSettingsUI::_on_test));
	hb->add_child(test_btn);

	save_btn = memnew(Button);
	save_btn->set_text("Save");
	save_btn->connect("pressed", callable_mp(this, &ColdStorageSettingsUI::_on_save));
	hb->add_child(save_btn);
}

void ColdStorageSettingsUI::_load_from_settings() {
	ColdStorageConnectionConfig c = cold_storage_load_config();
	host_edit->set_text(c.host);
	port_spin->set_value(c.port);
	user_edit->set_text(c.user);
	password_edit->set_text(c.password);
	workspace_edit->set_text(c.workspace);
	repo_edit->set_text(c.repo);
	jwt_edit->set_text(c.jwt);
	tls_cb->set_pressed(c.use_tls);
	tls_insecure_cb->set_pressed(c.tls_insecure);
	auto_pull_cb->set_pressed(c.auto_pull);
}

void ColdStorageSettingsUI::_apply_to_config() {
	ColdStorageConnectionConfig c = cold_storage_load_config();
	c.enabled = true;
	c.host = host_edit->get_text();
	c.port = (int)port_spin->get_value();
	c.user = user_edit->get_text();
	c.password = password_edit->get_text();
	c.workspace = workspace_edit->get_text();
	c.repo = repo_edit->get_text();
	c.jwt = jwt_edit->get_text();
	c.use_tls = tls_cb->is_pressed();
	c.tls_insecure = tls_insecure_cb->is_pressed();
	c.auto_pull = auto_pull_cb->is_pressed();
	cold_storage_save_config(c);
	pending_cfg_ = c;
}

void ColdStorageSettingsUI::_set_busy(bool p_busy) {
	ui_busy_ = p_busy;
	if (connect_btn) {
		connect_btn->set_disabled(p_busy);
	}
	if (test_btn) {
		test_btn->set_disabled(p_busy);
	}
	if (save_btn) {
		save_btn->set_disabled(p_busy);
	}
}

void ColdStorageSettingsUI::_refresh_status() {
	ColdStorageVCS *vcs = Object::cast_to<ColdStorageVCS>(EditorVCSInterface::get_singleton());
	if (ui_busy_) {
		status_label->set_text("ColdStorage: Working…");
		status_label->add_theme_color_override("font_color", Color(0.85, 0.75, 0.3));
	} else if (vcs && vcs->is_connected_to_server()) {
		status_label->set_text("ColdStorage: Connected");
		status_label->add_theme_color_override("font_color", Color(0.3, 0.85, 0.4));
	} else if (vcs && !vcs->get_last_error().is_empty()) {
		status_label->set_text("ColdStorage: Error — " + vcs->get_last_error());
		status_label->add_theme_color_override("font_color", Color(0.95, 0.35, 0.3));
	} else {
		status_label->set_text("ColdStorage: Disconnected");
		status_label->add_theme_color_override("font_color", Color(0.75, 0.75, 0.75));
	}
	if (ColdStorageEditorPlugin::get_singleton()) {
		ColdStorageEditorPlugin::get_singleton()->refresh_status_from_vcs();
	}
}

void ColdStorageSettingsUI::_on_save() {
	_apply_to_config();
	_refresh_status();
}

void ColdStorageSettingsUI::_on_connect() {
	if (ui_busy_ || cold_storage_connect_busy()) {
		return;
	}
	_apply_to_config();
	pending_cfg_.autoload_on_startup = true;
	cold_storage_save_config(pending_cfg_);

	if (VersionControlEditorPlugin::get_singleton() && EditorVCSInterface::get_singleton()) {
		VersionControlEditorPlugin::get_singleton()->shut_down();
	} else if (EditorVCSInterface::get_singleton()) {
		EditorVCSInterface::get_singleton()->shut_down();
		memdelete(EditorVCSInterface::get_singleton());
		EditorVCSInterface::set_singleton(nullptr);
	}

	ColdStorageConnectRequest req;
	req.kind = ColdStorageConnectRequest::Kind::CONNECT_JOB;
	req.cfg = pending_cfg_;
	req.project_path = OS::get_singleton()->get_resource_dir();
	req.validate = pending_cfg_.validate_on_startup;
	req.auto_pull = pending_cfg_.auto_pull;
	req.caller_id = get_instance_id();
	req.complete_method = "_on_async_complete";

	if (!cold_storage_begin_connect_async(req)) {
		status_label->set_text("ColdStorage: Busy");
		status_label->add_theme_color_override("font_color", Color(0.95, 0.35, 0.3));
		return;
	}
	_set_busy(true);
	_refresh_status();
}

void ColdStorageSettingsUI::_on_disconnect() {
	if (ui_busy_) {
		return;
	}
	if (VersionControlEditorPlugin::get_singleton()) {
		VersionControlEditorPlugin::get_singleton()->shut_down();
	} else if (EditorVCSInterface::get_singleton()) {
		EditorVCSInterface::get_singleton()->shut_down();
		memdelete(EditorVCSInterface::get_singleton());
		EditorVCSInterface::set_singleton(nullptr);
	}
	_refresh_status();
}

void ColdStorageSettingsUI::_on_test() {
	if (ui_busy_ || cold_storage_connect_busy()) {
		return;
	}
	_apply_to_config();

	ColdStorageConnectRequest req;
	req.kind = ColdStorageConnectRequest::Kind::TEST_JOB;
	req.cfg = pending_cfg_;
	req.project_path = OS::get_singleton()->get_resource_dir();
	req.validate = true;
	req.auto_pull = false;
	req.caller_id = get_instance_id();
	req.complete_method = "_on_async_complete";

	if (!cold_storage_begin_connect_async(req)) {
		status_label->set_text("ColdStorage: Busy");
		status_label->add_theme_color_override("font_color", Color(0.95, 0.35, 0.3));
		return;
	}
	_set_busy(true);
	_refresh_status();
}

void ColdStorageSettingsUI::_on_async_complete(bool p_ok, const String &p_error, int p_kind) {
	_set_busy(false);
	const auto kind = (ColdStorageConnectRequest::Kind)p_kind;

	if (kind == ColdStorageConnectRequest::Kind::TEST_JOB) {
		cold_storage_discard_connected_client();
		if (p_ok) {
			status_label->set_text("ColdStorage: Test OK");
			status_label->add_theme_color_override("font_color", Color(0.3, 0.85, 0.4));
		} else {
			status_label->set_text("ColdStorage: Test failed — " + p_error);
			status_label->add_theme_color_override("font_color", Color(0.95, 0.35, 0.3));
		}
		return;
	}

	if (!p_ok) {
		cold_storage_discard_connected_client();
		status_label->set_text("ColdStorage: Connect failed — " + p_error);
		status_label->add_theme_color_override("font_color", Color(0.95, 0.35, 0.3));
		_refresh_status();
		return;
	}

	ColdStorageVCS *vcs = memnew(ColdStorageVCS);
	if (!cold_storage_adopt_connected_client_into(vcs, pending_cfg_, OS::get_singleton()->get_resource_dir())) {
		status_label->set_text("ColdStorage: Connect failed — " + vcs->get_last_error());
		status_label->add_theme_color_override("font_color", Color(0.95, 0.35, 0.3));
		memdelete(vcs);
		_refresh_status();
		return;
	}

	EditorVCSInterface::set_singleton(vcs);
	ProjectSettings::get_singleton()->set("editor/version_control/plugin_name", "ColdStorageVCS");
	ProjectSettings::get_singleton()->set("editor/version_control/autoload_on_startup", true);
	if (VersionControlEditorPlugin::get_singleton()) {
		VersionControlEditorPlugin::get_singleton()->register_editor();
	}
	_refresh_status();
}

bool ColdStorageSettingsInspectorPlugin::can_handle(Object *p_object) {
	const String cname = p_object->get_class();
	return cname == "ProjectSettings" || cname == "EditorSettings" || cname == "SectionedInspectorFilter";
}

bool ColdStorageSettingsInspectorPlugin::parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
	(void)p_object;
	(void)p_type;
	(void)p_hint;
	(void)p_hint_text;
	(void)p_usage;
	(void)p_wide;
	if (p_path == "blazium/coldstorage/z_connection_controls" || p_path == "z_connection_controls") {
		add_custom_control(memnew(ColdStorageSettingsUI));
		return true;
	}
	return false;
}

#endif
