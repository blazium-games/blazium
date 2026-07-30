/**************************************************************************/
/*  cold_storage_editor_plugin.cpp                                        */
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

#include "cold_storage_editor_plugin.h"

#include "cold_storage_async.h"
#include "cold_storage_settings.h"
#include "cold_storage_settings_ui.h"
#include "cold_storage_vcs.h"
#include "core/config/project_settings.h"
#include "core/os/os.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_vcs_interface.h"
#include "editor/plugins/version_control_editor_plugin.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/label.h"

ColdStorageEditorPlugin *ColdStorageEditorPlugin::singleton = nullptr;

void ColdStorageEditorPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_show_configuration_dialog"), &ColdStorageEditorPlugin::_show_configuration_dialog);
	ClassDB::bind_method(D_METHOD("_poll_runtime"), &ColdStorageEditorPlugin::_poll_runtime);
	ClassDB::bind_method(D_METHOD("_on_async_complete", "ok", "error", "kind"), &ColdStorageEditorPlugin::_on_async_complete);
}

ColdStorageEditorPlugin::ColdStorageEditorPlugin() {
	singleton = this;
	cli_ = cold_storage_parse_cli();
}

ColdStorageEditorPlugin::~ColdStorageEditorPlugin() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

void ColdStorageEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			cold_storage_register_editor_settings();
			_setup_status_indicator();
			add_tool_menu_item("ColdStorage Configuration", callable_mp(this, &ColdStorageEditorPlugin::_show_configuration_dialog));

			inspector_plugin.instantiate();
			add_inspector_plugin(inspector_plugin);

			set_process(true);
		} break;
		case NOTIFICATION_EXIT_TREE: {
			remove_tool_menu_item("ColdStorage Configuration");
			if (inspector_plugin.is_valid()) {
				remove_inspector_plugin(inspector_plugin);
				inspector_plugin.unref();
			}
			_teardown_status_indicator();
			set_process(false);
		} break;
		case NOTIFICATION_PROCESS: {
			_poll_runtime();
		} break;
		default:
			break;
	}
}

void ColdStorageEditorPlugin::_setup_status_indicator() {
	if (status_label) {
		return;
	}
	status_label = memnew(Label);
	status_label->set_text("CS: —");
	status_label->set_tooltip_text("ColdStorage connection status");
	add_control_to_container(CONTAINER_TOOLBAR, status_label);
	_update_status_label("CS: Idle", Color(0.7, 0.7, 0.7));
}

void ColdStorageEditorPlugin::_teardown_status_indicator() {
	if (!status_label) {
		return;
	}
	remove_control_from_container(CONTAINER_TOOLBAR, status_label);
	memdelete(status_label);
	status_label = nullptr;
}

void ColdStorageEditorPlugin::_update_status_label(const String &p_text, const Color &p_color) {
	if (!status_label) {
		return;
	}
	status_label->set_text(p_text);
	status_label->add_theme_color_override("font_color", p_color);
}

void ColdStorageEditorPlugin::_finish_startup() {
	startup_done_ = true;
	set_process(false);
}

void ColdStorageEditorPlugin::refresh_status_from_vcs() {
	if (startup_busy_) {
		_update_status_label("CS: Connecting…", Color(0.85, 0.75, 0.3));
		return;
	}
	ColdStorageVCS *vcs = Object::cast_to<ColdStorageVCS>(EditorVCSInterface::get_singleton());
	if (vcs && vcs->is_connected_to_server()) {
		_update_status_label("CS: Connected", Color(0.3, 0.85, 0.4));
	} else if (vcs && !vcs->get_last_error().is_empty()) {
		_update_status_label("CS: Error", Color(0.95, 0.35, 0.3));
	} else {
		_update_status_label("CS: Disconnected", Color(0.75, 0.75, 0.75));
	}
}

void ColdStorageEditorPlugin::_show_configuration_dialog() {
	if (!config_dialog) {
		config_dialog = memnew(AcceptDialog);
		config_dialog->set_title("ColdStorage Configuration");
		config_dialog->set_min_size(Size2(480, 520));
		settings_ui = memnew(ColdStorageSettingsUI);
		config_dialog->add_child(settings_ui);
		EditorNode::get_singleton()->get_gui_base()->add_child(config_dialog);
	}
	config_dialog->popup_centered();
}

void ColdStorageEditorPlugin::_poll_runtime() {
	_try_deferred_startup();
}

void ColdStorageEditorPlugin::_try_deferred_startup() {
	if (startup_done_ || startup_busy_) {
		return;
	}
	if (!EditorSettings::get_singleton()) {
		return;
	}
	EditorNode *editor_node = EditorNode::get_singleton();
	if (!editor_node || !editor_node->is_editor_ready()) {
		editor_ready_since_usec_ = 0;
		return;
	}
	if (editor_ready_since_usec_ == 0) {
		editor_ready_since_usec_ = OS::get_singleton()->get_ticks_usec();
		return;
	}

	if (OS::get_singleton()->get_ticks_usec() - editor_ready_since_usec_ < 500'000) {
		return;
	}

	ColdStorageConnectionConfig cfg = cold_storage_load_config();
	cold_storage_apply_cli_to_config(cfg, cli_);

	if (!cold_storage_should_autoconnect(cfg, cli_)) {
		_finish_startup();
		refresh_status_from_vcs();
		return;
	}

	const bool validate = cfg.validate_on_startup && !cli_.no_validate;
	const bool auto_pull = cfg.auto_pull || (cli_.has_auto_pull && cli_.auto_pull);

	if (EditorVCSInterface::get_singleton()) {
		ColdStorageVCS *existing = Object::cast_to<ColdStorageVCS>(EditorVCSInterface::get_singleton());
		if (existing) {
			existing->apply_connection_config(cfg);
			if (existing->is_connected_to_server()) {
				if (validate && !existing->validate_connection()) {
					_update_status_label("CS: Validate failed", Color(0.95, 0.35, 0.3));
					WARN_PRINT("ColdStorage validate failed: " + existing->get_last_error());
				} else if (auto_pull) {
					// Pull still blocks briefly; full reconnect path below is async.
					existing->auto_pull_sync();
				}
				_finish_startup();
				refresh_status_from_vcs();
				return;
			}
			if (!_begin_vcs_connect_async(cfg, validate, auto_pull)) {
				_finish_startup();
				_update_status_label("CS: Busy", Color(0.95, 0.35, 0.3));
			}
			return;
		}
		// Another VCS is already active.
		_finish_startup();
		_update_status_label("CS: Other VCS active", Color(0.9, 0.75, 0.3));
		return;
	}

	if (!_begin_vcs_connect_async(cfg, validate, auto_pull)) {
		_finish_startup();
		_update_status_label("CS: Busy", Color(0.95, 0.35, 0.3));
	}
}

bool ColdStorageEditorPlugin::_begin_vcs_connect_async(const ColdStorageConnectionConfig &p_cfg, bool p_validate, bool p_auto_pull) {
	ColdStorageConnectionConfig cfg = p_cfg;
	cfg.enabled = true;
	cold_storage_save_config(cfg);
	pending_startup_cfg_ = cfg;

	ColdStorageConnectRequest req;
	req.kind = ColdStorageConnectRequest::Kind::STARTUP_JOB;
	req.cfg = cfg;
	req.project_path = OS::get_singleton()->get_resource_dir();
	req.validate = p_validate;
	req.auto_pull = p_auto_pull;
	req.caller_id = get_instance_id();
	req.complete_method = "_on_async_complete";

	if (!cold_storage_begin_connect_async(req)) {
		return false;
	}
	startup_busy_ = true;
	_update_status_label("CS: Connecting…", Color(0.85, 0.75, 0.3));
	return true;
}

void ColdStorageEditorPlugin::_on_async_complete(bool p_ok, const String &p_error, int p_kind) {
	(void)p_kind;
	startup_busy_ = false;
	_finish_startup();

	if (!p_ok) {
		cold_storage_discard_connected_client();
		_update_status_label("CS: Connect failed", Color(0.95, 0.35, 0.3));
		WARN_PRINT("ColdStorage connect failed: " + p_error);
		return;
	}

	if (!cold_storage_has_pending_client()) {
		// Existing VCS path with validate/pull-only shouldn't happen with STARTUP keeping client.
		refresh_status_from_vcs();
		return;
	}

	if (EditorVCSInterface::get_singleton()) {
		ColdStorageVCS *existing = Object::cast_to<ColdStorageVCS>(EditorVCSInterface::get_singleton());
		if (existing) {
			if (cold_storage_adopt_connected_client_into(existing, pending_startup_cfg_, OS::get_singleton()->get_resource_dir())) {
				_update_status_label("CS: Connected", Color(0.3, 0.85, 0.4));
			}
			return;
		}
		// Drop client if another VCS took over.
		cold_storage_discard_connected_client();
		return;
	}

	ColdStorageVCS *vcs = memnew(ColdStorageVCS);
	if (!cold_storage_adopt_connected_client_into(vcs, pending_startup_cfg_, OS::get_singleton()->get_resource_dir())) {
		_update_status_label("CS: Connect failed", Color(0.95, 0.35, 0.3));
		WARN_PRINT("ColdStorage connect failed: " + vcs->get_last_error());
		memdelete(vcs);
		return;
	}

	EditorVCSInterface::set_singleton(vcs);
	if (ProjectSettings::get_singleton()) {
		ProjectSettings::get_singleton()->set("editor/version_control/plugin_name", "ColdStorageVCS");
		ProjectSettings::get_singleton()->set("editor/version_control/autoload_on_startup", true);
	}
	if (VersionControlEditorPlugin::get_singleton()) {
		VersionControlEditorPlugin::get_singleton()->register_editor();
	}
	_update_status_label("CS: Connected", Color(0.3, 0.85, 0.4));
}

#endif // TOOLS_ENABLED
