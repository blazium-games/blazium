/**************************************************************************/
/*  crowd_control_game_pack_editor_plugin.h                               */
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

#include "editor/editor_inspector.h"
#include "editor/plugins/editor_plugin.h"
#include "modules/crowdcontrol/crowd_control_game_pack.h"
#include "scene/gui/button.h"
#include "scene/gui/dialogs.h"

class EditorFileDialog;

class CrowdControlGamePackInspectorPlugin : public EditorInspectorPlugin {
	GDCLASS(CrowdControlGamePackInspectorPlugin, EditorInspectorPlugin);

private:
	Button *export_button = nullptr;
	EditorFileDialog *file_dialog = nullptr;
	CrowdControlGamePack *current_game_pack = nullptr;

	void _on_export_button_pressed();
	void _on_file_selected(const String &p_path);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;

	CrowdControlGamePackInspectorPlugin();
};

class CrowdControlGamePackEditorPlugin : public EditorPlugin {
	GDCLASS(CrowdControlGamePackEditorPlugin, EditorPlugin);

private:
	Ref<CrowdControlGamePackInspectorPlugin> inspector_plugin;

public:
	virtual String get_plugin_name() const override { return "CrowdControlGamePack"; }

	CrowdControlGamePackEditorPlugin();
	~CrowdControlGamePackEditorPlugin();
};

#endif // TOOLS_ENABLED
