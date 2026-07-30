/**************************************************************************/
/*  cold_storage_settings_ui.h                                            */
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

#include "cold_storage_settings.h"
#include "editor/editor_inspector.h"
#include "scene/gui/margin_container.h"

class Button;
class CheckBox;
class Label;
class LineEdit;
class SpinBox;
class VBoxContainer;

class ColdStorageSettingsUI : public MarginContainer {
	GDCLASS(ColdStorageSettingsUI, MarginContainer);

	Label *status_label = nullptr;
	LineEdit *host_edit = nullptr;
	SpinBox *port_spin = nullptr;
	LineEdit *user_edit = nullptr;
	LineEdit *password_edit = nullptr;
	LineEdit *workspace_edit = nullptr;
	LineEdit *repo_edit = nullptr;
	LineEdit *jwt_edit = nullptr;
	CheckBox *tls_cb = nullptr;
	CheckBox *tls_insecure_cb = nullptr;
	CheckBox *auto_pull_cb = nullptr;
	Button *connect_btn = nullptr;
	Button *disconnect_btn = nullptr;
	Button *test_btn = nullptr;
	Button *save_btn = nullptr;

	ColdStorageConnectionConfig pending_cfg_;
	bool ui_busy_ = false;

	void _load_from_settings();
	void _apply_to_config();
	void _on_connect();
	void _on_disconnect();
	void _on_test();
	void _on_save();
	void _refresh_status();
	void _set_busy(bool p_busy);
	void _on_async_complete(bool p_ok, const String &p_error, int p_kind);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	ColdStorageSettingsUI();
};

class ColdStorageSettingsInspectorPlugin : public EditorInspectorPlugin {
	GDCLASS(ColdStorageSettingsInspectorPlugin, EditorInspectorPlugin);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual bool parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) override;
};

#endif
