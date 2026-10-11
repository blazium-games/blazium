/**************************************************************************/
/*  editor_split_view.h                                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
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

#include "scene/gui/box_container.h"
#include "scene/gui/split_container.h"

class Button;
class CodeEdit;
class Camera3D;
class EditorInspector;
class Label;
class MenuButton;
class Node;
class PanelContainer;
class SceneTreeEditor;
class Script;
class SubViewport;
class SubViewportContainer;
class Window;

// One side of the split view: a header naming the scene, the scene's own
// tree and inspector strips, and the content between them.
class EditorScenePane : public VBoxContainer {
	GDCLASS(EditorScenePane, VBoxContainer);

	friend class EditorSplitView;

	bool focused = false;

	PanelContainer *header = nullptr;
	Control *dot = nullptr;
	Label *file_label = nullptr;
	// Second pane only: the file name, as a menu to pick what it shows.
	MenuButton *pick_button = nullptr;
	Label *type_label = nullptr;
	Button *code_button = nullptr;
	Button *swap_button = nullptr;
	Button *float_button = nullptr;
	Button *close_button = nullptr;

	VBoxContainer *tree_strip = nullptr;
	Label *tree_title = nullptr;
	SceneTreeEditor *tree = nullptr;

	VBoxContainer *content = nullptr;

	VBoxContainer *inspector_strip = nullptr;
	Label *inspector_title = nullptr;
	EditorInspector *inspector = nullptr;

	// Preview pane only.
	SubViewportContainer *viewport_container = nullptr;
	SubViewport *viewport = nullptr;
	Control *overlay = nullptr;
	Camera3D *camera_3d = nullptr;
	Node *fallback_lighting = nullptr;

	// Beside the script editor: a live view of the edited scene itself.
	SubViewportContainer *live_container = nullptr;
	SubViewport *live_viewport = nullptr;
	Camera3D *live_camera = nullptr;

	// Beside the script editor: another open script, to compare.
	VBoxContainer *code_container = nullptr;
	CodeEdit *code_view = nullptr;
	Label *code_placeholder = nullptr;

	void _draw_dot();

protected:
	void _notification(int p_what);

public:
	void set_title(const String &p_title);

	EditorScenePane(bool p_focused);
};

// Shows a second open scene beside the one being edited. Both scenes stay in
// the tree and render live; the focused pane hosts the editor's main screen
// (2D/3D editors) and its scene is the edited scene. Clicking the other pane
// makes its scene the edited one, and the panes swap places so each scene
// stays on its side.
class EditorSplitView : public HSplitContainer {
	GDCLASS(EditorSplitView, HSplitContainer);

	static EditorSplitView *singleton;

	EditorScenePane *focus_pane = nullptr;
	EditorScenePane *preview_pane = nullptr;
	Control *main_screen = nullptr;

	// The preview pane can move into a window of its own.
	Window *float_window = nullptr;
	bool floating = false;
	int docked_index = 1;
	Rect2i float_rect;

	ObjectID preview_scene;
	ObjectID pane_tree_root;
	// On the script screen; the pane shows the edited scene or, with
	// showing_code, another open script.
	bool showing_live = false;
	bool showing_code = false;
	bool compare_scripts = false;
	ObjectID code_editor_id; // TextEditorBase shown in the pane.
	ObjectID current_code_editor_id; // TextEditorBase being edited.
	ObjectID code_source_id; // Its CodeEdit, while connected.
	// The divider is kept apart for the script editor, which needs more room.
	int scene_split_offset = 0;
	int script_split_offset = 0;
	bool has_script_split_offset = false;
	ObjectID last_focus_selected;
	ObjectID closing_scene;
	bool enabled = false;
	bool show_pane_docks = true;
	bool swap_on_change = false;
	NodePath pending_select;
	ObjectID pending_select_scene;
	bool update_queued = false;

	bool _is_scene_screen() const;
	bool _is_script_screen() const;
	int _find_scene_index(ObjectID p_root) const;
	void _take_into_preview(Node *p_root);
	void _pick_default_preview();
	void _release_preview();
	void _update_preview_camera();
	void _update_live_camera();
	void _frame_view(Node *p_root, const Dictionary &p_state_3d, Camera3D *p_camera, SubViewport *p_viewport, const Size2 &p_size);
	void _set_pane_tree_root(Node *p_root);
	void _live_gui_input(const Ref<InputEvent> &p_event);
	void _open_live_scene_editor();
	void _track_current_script();
	void _script_changed(const Ref<Script> &p_script);
	void _set_code_source(CodeEdit *p_source);
	void _code_source_changed();
	void _update_code_view();
	void _code_gui_input(const Ref<InputEvent> &p_event);
	void _edit_code_at(int p_line, int p_column);
	void _code_toggled(bool p_pressed);
	void _fill_pick_menu();
	void _pick_menu_id_pressed(int p_id);
	static void _collect_bounds(Node *p_node, AABB &r_bounds, bool &r_has_bounds);
	void _setup_preview_lighting(Node *p_root);
	void _focus_preview();
	void _swap_sides();
	void _apply_pending_select();
	void _preview_gui_input(const Ref<InputEvent> &p_event);
	void _preview_tree_selected();
	void _focus_tree_selected();
	void _close_preview();
	void _draw_preview_overlay();
	void _inspected_object_changed();
	void _queue_update();
	void _update_strips();
	void _update();
	void _update_pane_header(EditorScenePane *p_pane, Node *p_root);
	void _set_floating(bool p_floating);
	void _apply_float_setting();
	void _float_toggled(bool p_pressed);
	void _float_window_close_requested();
	void _show_float_window(bool p_show);
	Rect2i _default_float_rect() const;

protected:
	void _notification(int p_what);

public:
	static EditorSplitView *get_singleton() { return singleton; }

	void set_main_screen(Control *p_main_screen);
	void set_split_enabled(bool p_enabled);
	bool is_split_enabled() const { return enabled; }
	void set_show_pane_docks(bool p_show);

	// Called by EditorNode while switching the edited scene, after the new
	// scene was added to the editor's scene root.
	void edited_scene_changed(Node *p_old_root, Node *p_new_root);
	// Called before an open scene is closed or replaced.
	void release_scene(Node *p_root);
	// The edited scene is about to be closed: when the editor switches away
	// from it, don't move it into the preview.
	void set_closing_scene(Node *p_root);
	void queue_update();
	// Shows the scene at p_idx in the preview pane.
	void show_scene_in_preview(int p_idx);
	Node *get_preview_scene() const;

	EditorSplitView();
	~EditorSplitView();
};
