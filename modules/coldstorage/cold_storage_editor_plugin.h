/**************************************************************************/
/*  cold_storage_editor_plugin.h                                          */
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

#include "cold_storage_cli.h"
#include "cold_storage_settings.h"
#include "editor/plugins/editor_plugin.h"

class AcceptDialog;
class Label;
class ColdStorageSettingsInspectorPlugin;
class ColdStorageSettingsUI;
class ColdStorageVCS;

class ColdStorageEditorPlugin : public EditorPlugin {
	GDCLASS(ColdStorageEditorPlugin, EditorPlugin);

	static ColdStorageEditorPlugin *singleton;

	Label *status_label = nullptr;
	AcceptDialog *config_dialog = nullptr;
	ColdStorageSettingsUI *settings_ui = nullptr;
	Ref<ColdStorageSettingsInspectorPlugin> inspector_plugin;

	ColdStorageCliOverrides cli_;
	bool startup_done_ = false;
	bool startup_busy_ = false;
	uint64_t editor_ready_since_usec_ = 0;
	ColdStorageConnectionConfig pending_startup_cfg_;

	void _setup_status_indicator();
	void _teardown_status_indicator();
	void _update_status_label(const String &p_text, const Color &p_color);
	void _finish_startup();
	void _show_configuration_dialog();
	void _try_deferred_startup();
	void _poll_runtime();
	bool _begin_vcs_connect_async(const ColdStorageConnectionConfig &p_cfg, bool p_validate, bool p_auto_pull);
	void _on_async_complete(bool p_ok, const String &p_error, int p_kind);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	static ColdStorageEditorPlugin *get_singleton() { return singleton; }

	virtual String get_plugin_name() const override { return "ColdStorage"; }
	bool has_main_screen() const override { return false; }

	void refresh_status_from_vcs();

	ColdStorageEditorPlugin();
	~ColdStorageEditorPlugin() override;
};

#endif
