/**************************************************************************/
/*  inter_dvd_editor_plugin.h                                             */
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
#include "editor/export/editor_export_platform.h"
#include "editor/plugins/editor_plugin.h"

class InterDVDCell;
class InterDVDButton;

class EditorInspectorPluginInterDVDCell : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginInterDVDCell, EditorInspectorPlugin);

	void _bake_pressed(ObjectID p_id);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
};

class EditorInspectorPluginInterDVDButton : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginInterDVDButton, EditorInspectorPlugin);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual bool parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide = false) override;
};

class EditorInspectorPluginInterDVDProject : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginInterDVDProject, EditorInspectorPlugin);

	void _pick_videos(ObjectID p_id);
	void _videos_selected(const PackedStringArray &p_paths, ObjectID p_id, ObjectID p_dialog_id);
	void _pick_chapter(ObjectID p_id);
	void _chapter_selected(const String &p_path, ObjectID p_id, ObjectID p_dialog_id);
	void _pick_scene(ObjectID p_id);
	void _scene_selected(const String &p_path, ObjectID p_id, ObjectID p_dialog_id);
	void _add_title_menu(ObjectID p_id);
	void _add_menu_title(ObjectID p_id);
	void _ensure_first_play(ObjectID p_id);
	void _use_as_export_project(ObjectID p_id);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
};

class EditorInspectorPluginInterDVDPGC : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginInterDVDPGC, EditorInspectorPlugin);

	void _pick_video_cell(ObjectID p_id);
	void _video_cell_selected(const String &p_path, ObjectID p_id, ObjectID p_dialog_id);
	void _add_jump_title_button(ObjectID p_id);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
};

class EditorInspectorPluginInterDVDDisc : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginInterDVDDisc, EditorInspectorPlugin);

	void _add_title(ObjectID p_id);
	void _add_chapter(ObjectID p_id);
	void _add_title_menu(ObjectID p_id);
	void _add_root_menu(ObjectID p_id);
	void _add_title_set(ObjectID p_id);
	void _add_menu_title(ObjectID p_id);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
};

class EditorInspectorPluginInterDVDTitle : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginInterDVDTitle, EditorInspectorPlugin);

	void _add_chapter(ObjectID p_id);
	void _add_hotspot(ObjectID p_id);
	void _add_menu_hotspot(ObjectID p_id);
	void _add_resume_hotspot(ObjectID p_id);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
};

class EditorInspectorPluginInterDVDTitleSet : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginInterDVDTitleSet, EditorInspectorPlugin);

	void _add_title(ObjectID p_id);
	void _add_root_menu(ObjectID p_id);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
};

class EditorInspectorPluginInterDVDMenuPage : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginInterDVDMenuPage, EditorInspectorPlugin);

	void _add_hotspot(ObjectID p_id);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
};

class InterDVDEditorPlugin : public EditorPlugin {
	GDCLASS(InterDVDEditorPlugin, EditorPlugin);

	Ref<EditorExportPlatform> export_platform;
	Ref<EditorInspectorPluginInterDVDCell> inspector_plugin;
	Ref<EditorInspectorPluginInterDVDButton> button_inspector_plugin;
	Ref<EditorInspectorPluginInterDVDProject> project_inspector_plugin;
	Ref<EditorInspectorPluginInterDVDPGC> pgc_inspector_plugin;
	Ref<EditorInspectorPluginInterDVDDisc> disc_inspector_plugin;
	Ref<EditorInspectorPluginInterDVDTitle> title_inspector_plugin;
	Ref<EditorInspectorPluginInterDVDTitleSet> title_set_inspector_plugin;
	Ref<EditorInspectorPluginInterDVDMenuPage> menu_page_inspector_plugin;

	void _create_dvd_scene();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	virtual String get_plugin_name() const override { return "InteractiveDVD"; }
};

#endif
