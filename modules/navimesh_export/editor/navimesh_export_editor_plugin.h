/**************************************************************************/
/*  navimesh_export_editor_plugin.h                                       */
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

#include "core/object/object.h"
#include "editor/editor_inspector.h"
#include "editor/plugins/editor_plugin.h"

class Button;
class EditorFileDialog;
class HBoxContainer;
class Node;

class NavimeshExportInspectorPlugin : public EditorInspectorPlugin {
	GDCLASS(NavimeshExportInspectorPlugin, EditorInspectorPlugin);

	Node *current_node = nullptr;
	EditorFileDialog *file_dialog = nullptr;

	void _on_export_pressed();
	void _on_file_selected(const String &p_path);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
};

class NavimeshExportEditorPlugin : public EditorPlugin {
	GDCLASS(NavimeshExportEditorPlugin, EditorPlugin);

	Ref<NavimeshExportInspectorPlugin> inspector_plugin;
	HBoxContainer *spatial_hbox = nullptr;
	HBoxContainer *canvas_hbox = nullptr;
	Button *spatial_export = nullptr;
	Button *canvas_export = nullptr;
	EditorFileDialog *file_dialog = nullptr;
	ObjectID current_object;

	void _ensure_file_dialog();
	void _on_toolbar_export();
	void _on_file_selected(const String &p_path);

protected:
	void _notification(int p_what);

public:
	virtual String get_plugin_name() const override { return "NavimeshExport"; }
	virtual bool handles(Object *p_object) const override;
	virtual void edit(Object *p_object) override;
	virtual void make_visible(bool p_visible) override;

	NavimeshExportEditorPlugin();
	~NavimeshExportEditorPlugin();
};

#endif
