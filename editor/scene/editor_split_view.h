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
class CheckBox;
class CodeEdit;
class Camera3D;
class CreateDialog;
class LineEdit;
class ScriptCreateDialog;
class EditorInspector;
class EditorPaneRotationControl;
class EditorPaneTools;
class HFlowContainer;
class Label;
class MenuButton;
class MeshInstance3D;
class PopupMenu;
class Node;
class PanelContainer;
class PopupPanel;
class SceneTreeEditor;
class ShaderMaterial;
class Script;
class ScriptEditorBase;
class SubViewport;
class SubViewportContainer;
class Window;
class World2D;

// One side of the split view: a header naming the scene, the scene's own
// tree and inspector strips, and the content between them.
class EditorScenePane : public VBoxContainer {
	GDCLASS(EditorScenePane, VBoxContainer);

	friend class EditorSplitView;

	bool focused = false;
	// Where shortcuts and undo go: the side last clicked.
	bool active = false;
	bool editable = true;

	PanelContainer *header = nullptr;
	Control *dot = nullptr;
	Label *file_label = nullptr;
	// Second pane only: the file name, as a menu to pick what it shows.
	MenuButton *pick_button = nullptr;
	Label *type_label = nullptr;
	Label *state_label = nullptr; // "Editing", "Editable" or "View only".
	Button *code_button = nullptr;
	Button *swap_button = nullptr;
	Button *float_button = nullptr;
	Button *close_button = nullptr;

	VBoxContainer *tree_strip = nullptr;
	Label *tree_title = nullptr;
	// Preview pane only: the Scene dock's toolbar, for the pane's scene.
	HBoxContainer *tree_toolbar = nullptr;
	Button *add_node_button = nullptr;
	Button *instantiate_button = nullptr;
	LineEdit *tree_filter = nullptr;
	Button *attach_script_button = nullptr;
	Button *detach_script_button = nullptr;
	MenuButton *tree_menu = nullptr;
	PopupMenu *subresource_menu = nullptr;
	SceneTreeEditor *tree = nullptr;
	bool tree_fit_queued = false;

	VBoxContainer *content = nullptr;

	VBoxContainer *inspector_strip = nullptr;
	Label *inspector_title = nullptr;
	EditorInspector *inspector = nullptr;

	// Preview pane only.
	EditorPaneTools *tools = nullptr;
	SubViewportContainer *viewport_container = nullptr;
	SubViewport *viewport = nullptr;
	Control *overlay = nullptr;
	Camera3D *camera_3d = nullptr;
	Node *fallback_lighting = nullptr;
	// For 3D scenes, as the 3D editor's viewports have: the view menu, the
	// information and frame time panels, and the grid.
	MenuButton *view_menu = nullptr;
	// For 2D scenes, the 2D editor's View menu, in the toolbar.
	MenuButton *view_menu_2d = nullptr;
	PopupMenu *view_grid_menu_2d = nullptr;
	PopupMenu *view_gizmos_menu_2d = nullptr;
	EditorPaneRotationControl *rotation_control = nullptr;
	PopupMenu *view_advanced_menu = nullptr;
	PanelContainer *info_panel = nullptr;
	Label *info_label = nullptr;
	PanelContainer *frame_time_panel = nullptr;
	Label *cpu_time_label = nullptr;
	Label *gpu_time_label = nullptr;
	Label *fps_label = nullptr;
	MeshInstance3D *grid = nullptr;

	// Beside the script editor: another open script, to compare.
	VBoxContainer *code_container = nullptr;
	enum CodeMenu {
		CODE_FIND,
		CODE_FIND_NEXT,
		CODE_FIND_PREVIOUS,
		CODE_GOTO_LINE,
		CODE_GOTO_FUNCTION,
	};
	// Read-only tools for the code shown: Search, Go To and its history.
	HBoxContainer *code_toolbar = nullptr;
	MenuButton *code_search_menu = nullptr;
	MenuButton *code_goto_menu = nullptr;
	MenuButton *code_function_menu = nullptr;
	Button *code_both_button = nullptr;
	Button *code_back_button = nullptr;
	Button *code_forward_button = nullptr;
	CodeEdit *code_view = nullptr;
	Label *code_placeholder = nullptr;
	HFlowContainer *code_find_bar = nullptr;
	LineEdit *code_find_text = nullptr;
	Label *code_find_matches = nullptr;
	Button *code_find_prev = nullptr;
	Button *code_find_next = nullptr;
	CheckBox *code_find_case = nullptr;
	CheckBox *code_find_words = nullptr;
	Button *code_find_close = nullptr;
	PopupPanel *code_line_popup = nullptr;
	LineEdit *code_line_text = nullptr;

	void _draw_dot();
	void _update_style();

protected:
	void _queue_fit_tree_strip();
	void _fit_tree_strip();
	void _notification(int p_what);

public:
	void set_title(const String &p_title);
	void set_active(bool p_active, bool p_editable = true);

	EditorScenePane(bool p_focused);
};

// Shows a second open scene beside the one being edited. Both scenes stay in
// the tree and render live; the focused pane hosts the editor's main screen
// (2D/3D editors) and its scene is the edited scene. The other pane has tools
// of its own to edit its scene where it is; the swap button trades the two
// scenes, and the panes stay where they are.
class EditorSplitView : public HSplitContainer {
	GDCLASS(EditorSplitView, HSplitContainer);

	// A camera orbiting a point, as the 3D editor's.
	struct OrbitView {
		Vector3 target;
		real_t x_rot = 0.5;
		real_t y_rot = -0.5;
		real_t distance = 4.0;
		real_t fov = 70.0;
		bool orthogonal = false;
	};

	static EditorSplitView *singleton;

	// The second pane's Scene toolbar menu, as the Scene dock's.
	enum PaneTreeMenu {
		PANE_TREE_AUTO_EXPAND,
		PANE_TREE_HIDE_FILTERED_OUT_PARENTS,
		PANE_TREE_ACCESSIBILITY_WARNINGS,
		PANE_TREE_FILTER_BY_TYPE,
		PANE_TREE_FILTER_BY_GROUP,
	};
	CreateDialog *pane_create_dialog = nullptr;
	ScriptCreateDialog *pane_script_dialog = nullptr;
	ObjectID pane_new_node_parent;
	ObjectID pane_script_target;

	// The second pane's view menu, with the 3D editor's viewport menu items.
	enum PaneViewMenu {
		PANE_VIEW_TOP,
		PANE_VIEW_BOTTOM,
		PANE_VIEW_LEFT,
		PANE_VIEW_RIGHT,
		PANE_VIEW_FRONT,
		PANE_VIEW_REAR,
		PANE_VIEW_SWITCH_PERSPECTIVE_ORTHOGONAL,
		PANE_VIEW_PERSPECTIVE,
		PANE_VIEW_ORTHOGONAL,
		PANE_VIEW_AUTO_ORTHOGONAL,
		PANE_VIEW_LOCK_ROTATION,
		PANE_VIEW_ENVIRONMENT,
		PANE_VIEW_GIZMOS,
		PANE_VIEW_TRANSFORM_GIZMO,
		PANE_VIEW_GRID,
		PANE_VIEW_INFORMATION,
		PANE_VIEW_FRAME_TIME,
		PANE_VIEW_HALF_RESOLUTION,
		PANE_VIEW_AUDIO_LISTENER,
		PANE_VIEW_AUDIO_DOPPLER,
		PANE_VIEW_CINEMATIC_PREVIEW,
		PANE_VIEW_CENTER_TO_ORIGIN,
		PANE_VIEW_CENTER_TO_SELECTION,
		PANE_VIEW_CENTER_TO_AABB,
		PANE_VIEW_ALIGN_TRANSFORM_WITH_VIEW,
		PANE_VIEW_ALIGN_ROTATION_WITH_VIEW,
		PANE_VIEW_DISPLAY_ADVANCED,
		// Display modes: this plus a Viewport::DebugDraw value.
		PANE_VIEW_DISPLAY_BASE = 100,
	};

	// Which way the second pane looks, as named in its view menu.
	// The 2D editor's View menu, for the pane's 2D scenes.
	enum PaneView2DMenu {
		PANE_2D_GRID_SHOW,
		PANE_2D_GRID_SHOW_WHEN_SNAPPING,
		PANE_2D_GRID_HIDE,
		PANE_2D_GRID_TOGGLE,
		PANE_2D_SHOW_RULERS,
		PANE_2D_SHOW_GUIDES,
		PANE_2D_SHOW_ORIGIN,
		PANE_2D_SHOW_VIEWPORT,
		PANE_2D_SHOW_INFORMATION,
		PANE_2D_SHOW_FRAME_TIME,
		PANE_2D_GIZMO_POSITION,
		PANE_2D_GIZMO_LOCK,
		PANE_2D_GIZMO_GROUP,
		PANE_2D_GIZMO_TRANSFORMATION,
		PANE_2D_CENTER_SELECTION,
		PANE_2D_FRAME_SELECTION,
		PANE_2D_CLEAR_GUIDES,
		// The submenus themselves, apart from their items.
		PANE_2D_GRID_MENU,
		PANE_2D_GIZMOS_MENU,
	};

	enum PaneViewType {
		PANE_VIEW_TYPE_USER,
		PANE_VIEW_TYPE_TOP,
		PANE_VIEW_TYPE_BOTTOM,
		PANE_VIEW_TYPE_LEFT,
		PANE_VIEW_TYPE_RIGHT,
		PANE_VIEW_TYPE_FRONT,
		PANE_VIEW_TYPE_REAR,
	};

	EditorScenePane *focus_pane = nullptr;
	EditorScenePane *preview_pane = nullptr;
	Control *main_screen = nullptr;

	// The preview pane can move into a window of its own.
	Window *float_window = nullptr;
	bool floating = false;
	Rect2i float_rect;

	ObjectID preview_scene;
	// The second pane's own view once it was zoomed, orbited or panned;
	// dropped when another scene is shown there.
	bool preview_view_custom = false;
	OrbitView preview_orbit;
	Transform2D preview_view_2d;
	PaneViewType preview_view_type = PANE_VIEW_TYPE_USER;
	// Switched to orthogonal by picking a side view, until orbiting again.
	bool preview_auto_orthogonal = false;
	bool preview_cinematic = false;
	ObjectID preview_cinematic_camera;
	static constexpr int FRAME_TIME_HISTORY = 20;
	double cpu_time_history[FRAME_TIME_HISTORY] = {};
	double gpu_time_history[FRAME_TIME_HISTORY] = {};
	int frame_time_history_index = 0;
	// Seconds left showing the zoom bar, as the 3D editor shows after zooming.
	double zoom_indicator_delay = 0.0;
	// Freelook, while the right mouse button is held over a 3D scene.
	bool preview_freelook = false;
	real_t freelook_speed = 5.0;
	Point2 freelook_mouse_start;
	ObjectID pane_tree_root;
	// On the script screen; the pane shows the edited scene or, with
	// showing_code, another open script.
	bool showing_live = false;
	// Beside the script editor the pane edits the edited scene itself: its
	// viewport looks into the editor's world, where that scene is, and the
	// scene it shows elsewhere waits out of the tree.
	bool pane_live = false;
	ObjectID pane_live_root;
	// The 2D scene last fitted into the pane, and the size it was fitted to.
	ObjectID preview_fit_root;
	Size2 preview_fit_size;
	bool syncing_live_selection = false;
	Ref<World2D> preview_world_2d; // The pane's own 2D world, kept meanwhile.
	bool showing_code = false;
	bool compare_scripts = false;
	ObjectID code_editor_id; // TextEditorBase shown in the pane.
	ObjectID current_code_editor_id; // TextEditorBase being edited.
	ObjectID code_source_id; // Its CodeEdit, while connected.
	// On the 2D and 3D screens, the pane can show a script instead of a
	// scene: the selected node's, or one picked or dropped there.
	bool code_beside_scene = false;
	bool showing_scene_code = false;
	Ref<Script> scene_code_script;
	Ref<Script> code_static_script; // Shown without an open editor.

	// The scripts view 2 has shown, by path, to step back and forth through.
	static constexpr int CODE_HISTORY_MAX = 50;
	Vector<String> code_history;
	int code_history_pos = -1;
	// The script editor given view 2's search by "Both Scripts", so it can
	// be taken off again.
	ObjectID code_other_search_id;

	// View 2's grid, one material per plane, rebuilt as the 3D editor
	// rebuilds its own: when the camera moves 10 m or its projection changes.
	Ref<ShaderMaterial> preview_grid_materials[3];
	bool preview_grid_drawn = false;
	bool preview_grid_orthogonal = false;
	real_t preview_grid_ortho_size = 0.0;
	Vector3 preview_grid_position;
	// The divider is kept apart for the script editor, which needs more room.
	int scene_split_offset = 0;
	int script_split_offset = 0;
	bool has_script_split_offset = false;
	ObjectID last_focus_selected;
	ObjectID closing_scene;
	bool enabled = false;
	bool show_pane_docks = true;
	NodePath pending_select;
	ObjectID pending_select_scene;
	bool update_queued = false;
	// The second pane was clicked last: undo and redo go to its scene.
	bool preview_active = false;
	// Whether the second pane's scene had unsaved changes, to update the
	// scene tabs when that changes.
	bool preview_unsaved = false;
	// The Inspector dock shows the second pane's selection while that side
	// is being edited.
	bool pane_in_main_inspector = false;
	// Scenes saved since the last frame, to rebuild the second pane's scene
	// if it uses them.
	Vector<String> saved_scenes;

	bool _is_scene_screen() const;
	bool _is_script_screen() const;
	int _find_scene_index(ObjectID p_root) const;
	void _take_into_preview(Node *p_root);
	void _pick_default_preview();
	void _release_preview();
	void _update_preview_camera();
	static OrbitView _orbit_from_state(Node *p_root, const Dictionary &p_state_3d);
	OrbitView _preview_orbit_from_state(Node *p_root) const;
	static void _apply_orbit(const OrbitView &p_orbit, Camera3D *p_camera, SubViewport *p_viewport);
	static Transform2D _fit_project_window(const Size2 &p_size);
	void _begin_custom_view();
	void _set_pane_tree_root(Node *p_root);
	void _track_current_script();
	void _script_changed(const Ref<Script> &p_script);
	void _set_code_source(CodeEdit *p_source);
	void _code_source_changed();
	void _update_code_view();
	void _code_toggled(bool p_pressed);
	void _style_code_view(CodeEdit *p_style_from);
	void _set_code_static(const Ref<Script> &p_script);
	Ref<Script> _selected_node_script() const;
	void _selection_changed();
	void _update_scene_code_view();

	Ref<Script> _shown_code_script() const;
	CodeEdit *_edited_code_edit() const;
	bool _code_both_scripts() const;
	void _update_code_toolbar();
	void _fill_code_menus();
	void _code_menu_id_pressed(int p_id);
	bool _code_shortcut_input(const Ref<InputEvent> &p_event);
	void _code_find_show();
	void _code_find_hide();
	uint32_t _code_find_flags() const;
	void _code_find_text_changed(const String &p_text);
	void _code_find_submitted(const String &p_text);
	void _code_find_options_changed(bool p_pressed);
	void _code_find_input(const Ref<InputEvent> &p_event);
	void _code_find(bool p_backwards);
	static bool _code_search_in(CodeEdit *p_edit, const String &p_text, uint32_t p_flags, bool p_backwards, bool p_from_start);
	static int _code_count_matches(CodeEdit *p_edit, const String &p_text, uint32_t p_flags, int &r_current);
	void _update_code_find_matches();
	void _clear_other_search();
	void _code_line_popup();
	void _code_line_submitted(const String &p_text);
	void _fill_code_function_menu();
	void _code_function_pressed(int p_index);
	void _record_code_history(const String &p_path);
	void _code_history_step(int p_step);
	void _update_code_history_buttons();
	void _set_setting(const String &p_name, const Variant &p_value);
	bool _can_drop(const Point2 &p_point, const Variant &p_data) const;
	void _drop(const Point2 &p_point, const Variant &p_data);
	void _fill_pick_menu();
	void _pick_menu_id_pressed(int p_id);
	static void _collect_bounds(Node *p_node, AABB &r_bounds, bool &r_has_bounds);
	void _setup_preview_lighting(Node *p_root);
	Node *_pane_target_node() const;
	void _pane_add_node();
	void _pane_create_node();
	void _pane_instantiate_scene();
	void _pane_scene_picked(const String &p_path);
	void _pane_add_child(Node *p_parent, Node *p_child, const String &p_action);
	void _pane_attach_script();
	void _pane_script_created(const Ref<Script> &p_script);
	void _pane_detach_script();
	void _pane_open_scene(const String &p_path);
	void _pane_open_script(const Ref<Script> &p_script);
	void _pane_filter_changed(const String &p_filter);
	void _pane_tree_menu_about_to_popup();
	void _pane_tree_menu_option(int p_option);
	void _pane_list_subresources();
	void _pane_edit_subresource(int p_index);
	void _update_pane_toolbar();
	void _build_view_menu();
	void _build_view_menu_2d();
	void _view_menu_2d_option(int p_option);
	bool _view_menu_2d_checked(int p_option) const;
	void _set_view_menu_2d_checked(int p_option, bool p_checked);
	bool _view_menu_2d_shortcut(const Ref<InputEvent> &p_event);
	bool _is_grid_2d_visible() const;
	void _focus_preview_2d(bool p_frame);
	void _draw_preview_2d_under();
	void _draw_preview_2d_over();
	void _draw_locks_and_positions_2d(Node *p_node, const Transform2D &p_xform, Node *p_root);
	void _view_menu_option(int p_option);
	bool _view_menu_checked(int p_option) const;
	void _set_view_menu_checked(int p_option, bool p_checked);
	bool _view_menu_shortcut(const Ref<InputEvent> &p_event);
	void _update_view_menu();
	void _end_side_view();
	void _rotation_axis_clicked(int p_axis);
	void _orbit_preview(const Vector2 &p_relative);
	static Basis _orbit_basis(const OrbitView &p_orbit);
	void _set_freelook(bool p_enabled, const Point2 &p_mouse_position = Point2());
	void _freelook_look(const Vector2 &p_relative);
	void _freelook_move(double p_delta);
	static bool _is_shortcut_held(const String &p_name);
	void _set_cinematic_preview(bool p_enabled);
	Camera3D *_find_scene_camera(Node *p_node) const;
	void _focus_preview_bounds(bool p_frame);
	void _align_selection_with_view(bool p_rotation_only);
	void _build_preview_grid();
	Node *_pane_scene() const;
	void _set_pane_live(bool p_live);
	void _sync_pane_live_selection();
	void _update_preview_grid(bool p_force = false);
	void _update_view_panels();
	void _update_process();
	void _set_preview_active(bool p_active);
	void _update_active_pane();
	void _pane_selection_changed();
	void _sync_main_inspector();
	void _restore_main_inspector();
	void _float_window_input(const Ref<InputEvent> &p_event);
	void _trade_sides();
	void _apply_pending_select();
	void _preview_gui_input(const Ref<InputEvent> &p_event);
	void _preview_tree_selected();
	void _focus_tree_selected();
	void _close_preview();
	void _draw_preview_overlay();
	void _inspected_object_changed();
	void _scene_saved(const String &p_path);
	void _refresh_preview_instances();
	static bool _uses_scene(const Node *p_node, const String &p_path);
	static bool _uses_any_scene(int p_idx, const Vector<String> &p_paths);
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
	virtual void input(const Ref<InputEvent> &p_event) override;

public:
	static EditorSplitView *get_singleton() { return singleton; }

	// The undo history of the second pane's scene while edits go to it, or
	// EditorUndoRedoManager::INVALID_HISTORY.
	int get_active_history_id() const;

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
	// Shows an open script in the second pane on the Script screen, and goes
	// back to the script that was edited before it.
	void show_script_in_preview(ScriptEditorBase *p_editor);
	Node *get_preview_scene() const;

	EditorSplitView();
	~EditorSplitView();
};
