/**************************************************************************/
/*  dddbrowser_editor_plugin.h                                            */
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

#include "../dddbrowser_exporter.h"
#include "../dddbrowser_preview_server.h"

#include "editor/plugins/editor_context_menu_plugin.h"
#include "editor/plugins/editor_plugin.h"

class EditorFileDialog;

class DDDBrowserFilesystemContextPlugin : public EditorContextMenuPlugin {
	GDCLASS(DDDBrowserFilesystemContextPlugin, EditorContextMenuPlugin);

	Callable export_cb;
	Callable test_cb;
	Callable create_cb;
	Callable check_luau_cb;

protected:
	static void _bind_methods() {}

public:
	void set_callbacks(const Callable &p_export, const Callable &p_test, const Callable &p_create, const Callable &p_check_luau = Callable());
	virtual void get_options(const Vector<String> &p_paths) override;
};

class DDDBrowserSceneTreeContextPlugin : public EditorContextMenuPlugin {
	GDCLASS(DDDBrowserSceneTreeContextPlugin, EditorContextMenuPlugin);

	Callable export_cb;
	Callable test_cb;
	Callable check_luau_cb;

protected:
	static void _bind_methods() {}

public:
	void set_callbacks(const Callable &p_export, const Callable &p_test, const Callable &p_check_luau = Callable());
	virtual void get_options(const Vector<String> &p_paths) override;
};

class DDDBrowserEditorPlugin : public EditorPlugin {
	GDCLASS(DDDBrowserEditorPlugin, EditorPlugin);

	EditorFileDialog *export_dialog = nullptr;
	Ref<DDDBrowserExporter> exporter;
	Ref<DDDBrowserPreviewServer> preview_server;
	Ref<DDDBrowserFilesystemContextPlugin> fs_plugin;
	Ref<DDDBrowserFilesystemContextPlugin> fs_create_plugin;
	Ref<DDDBrowserSceneTreeContextPlugin> tree_plugin;

	void _popup_export_dialog();
	void _export_to_path(const String &p_path);
	void _export_paths(const Variant &p_paths);
	void _test_paths(const Variant &p_paths);
	void _create_level_paths(const Variant &p_paths);
	void _check_luau_paths(const Variant &p_paths);
	void _test_scene_root(Node *p_root);
	Node *_load_scene_root(const String &p_path);
	String _default_export_dir(const String &p_scene_path) const;
	Error _write_template_file(const String &p_res_path, const String &p_contents) const;

protected:
	static void _bind_methods();

public:
	virtual String get_plugin_name() const override;
	bool has_main_screen() const override { return false; }

	DDDBrowserEditorPlugin();
	~DDDBrowserEditorPlugin();
};

#endif
