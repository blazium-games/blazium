/**************************************************************************/
/*  editor_split_view_tools.h                                             */
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

class Button;
class Camera3D;
class CanvasItem;
class SubViewport;

// The tools of the split view's second pane, so the scene shown there can be
// edited without switching to it: its own selection, and moving, rotating and
// scaling with handles drawn over the pane. Edits go to that scene's own
// undo history.
class EditorPaneTools : public HBoxContainer {
	GDCLASS(EditorPaneTools, HBoxContainer);

public:
	enum Tool {
		TOOL_SELECT,
		TOOL_MOVE,
		TOOL_ROTATE,
		TOOL_SCALE,
		TOOL_MAX,
	};

private:
	enum Handle {
		HANDLE_NONE = -1,
		HANDLE_X,
		HANDLE_Y,
		HANDLE_Z,
		HANDLE_CENTER, // Free move, uniform scale.
		HANDLE_BODY, // The selection itself, dragged.
	};

	Button *tool_buttons[TOOL_MAX] = {};
	Button *local_button = nullptr;
	Button *snap_button = nullptr;
	Button *undo_button = nullptr;
	Button *redo_button = nullptr;

	SubViewport *viewport = nullptr;
	Camera3D *camera = nullptr;
	Control *overlay = nullptr;

	Tool tool = TOOL_SELECT;
	// From the pane's view menu: View Gizmos and View Transform Gizmo.
	bool show_selection = true;
	bool show_handles = true;
	ObjectID root_id;
	ObjectID selected_id;
	Handle hover = HANDLE_NONE;

	bool dragging = false;
	bool drag_changed = false;
	Handle drag_handle = HANDLE_NONE;
	Point2 drag_from;
	// 3D: the global transform at the start, the handles' axes and size, and
	// the local transform to undo to.
	Transform3D drag_start_3d;
	Transform3D drag_start_local_3d;
	Basis drag_axes;
	real_t drag_size = 1.0;
	// 2D: the local values to undo to, and the global position.
	Point2 drag_start_position;
	Point2 drag_start_global_position;
	real_t drag_start_rotation = 0.0;
	Size2 drag_start_scale = Size2(1, 1);
	Vector2 drag_axes_2d[2];

	Node *_get_root() const;
	bool _is_3d() const;
	int _get_scene_index() const;
	bool _is_snapping(bool p_ctrl) const;
	Node *_selectable(Node *p_node) const;
	Node *_pick(const Point2 &p_pos) const;
	void _pick_3d(Node *p_node, const Vector3 &p_from, const Vector3 &p_dir, Node *&r_best, real_t &r_best_distance) const;
	void _pick_3d_origin(Node *p_node, const Point2 &p_pos, Node *&r_best, real_t &r_best_distance) const;
	void _collect_canvas_items(Node *p_node, Vector<CanvasItem *> &r_items) const;
	Transform2D _screen_transform_2d(const CanvasItem *p_item) const;

	bool _get_gizmo_3d(Vector3 &r_origin, Basis &r_axes, real_t &r_size) const;
	bool _get_gizmo_2d(Point2 &r_origin, Vector2 *r_axes) const;
	Vector<Point2> _ring_points_3d(const Vector3 &p_origin, const Basis &p_axes, real_t p_size, int p_axis) const;
	Handle _handle_at(const Point2 &p_pos) const;

	void _begin_drag(Handle p_handle, const Point2 &p_pos);
	void _update_drag(const Point2 &p_pos, bool p_ctrl);
	void _update_drag_3d(Node *p_node, const Point2 &p_pos, bool p_snap);
	void _update_drag_2d(CanvasItem *p_item, const Point2 &p_pos, bool p_snap);
	void _end_drag(bool p_commit);

	void _draw_selection_3d(Node *p_node);
	void _draw_selection_2d(CanvasItem *p_item);
	void _draw_gizmo_3d();
	void _draw_gizmo_2d();

	void _set_tool(int p_tool);
	void _undo();
	void _redo();
	void _history_changed();
	void _queue_overlay_redraw();
	void _snap_toggled(bool p_pressed);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void setup(SubViewport *p_viewport, Camera3D *p_camera, Control *p_overlay);
	// The scene shown in the pane; a new scene drops the selection.
	void set_scene(Node *p_root);
	Node *get_selected() const;
	void set_selected(Node *p_node, bool p_emit = true);
	// Input on the pane's viewport. Returns true when used.
	bool handle_input(const Ref<InputEvent> &p_event);
	bool is_dragging() const { return dragging; }
	bool is_snap_enabled() const;
	void set_show_selection(bool p_show);
	void set_show_handles(bool p_show);
	// Draws the selection and handles onto the overlay.
	void draw_overlay();
	// The undo history of the scene shown, or INVALID_HISTORY.
	int get_history_id() const;

	EditorPaneTools();
};

// The 3D editor viewport's axis gizmo, for the second pane: click an axis to
// look along it, or drag to orbit.
class EditorPaneRotationControl : public Control {
	GDCLASS(EditorPaneRotationControl, Control);

	struct Axis2D {
		Vector2 screen_point;
		real_t z_axis = -99.0;
		int axis = -1; // 0-2: +X +Y +Z, 3-5: -X -Y -Z.
		bool is_positive = true;
	};

	struct Axis2DCompare {
		_FORCE_INLINE_ bool operator()(const Axis2D &p_a, const Axis2D &p_b) const {
			return p_a.z_axis < p_b.z_axis;
		}
	};

	Camera3D *camera = nullptr;
	int focused_axis = -2; // -1: over the circle, -2: outside.
	bool pressed = false;
	bool dragged = false;
	Point2 press_position;

	void _get_sorted_axis(Vector<Axis2D> &r_axis) const;
	void _draw_axis(const Axis2D &p_axis);
	void _update_focus();
	real_t _axis_radius() const;

protected:
	void _notification(int p_what);
	static void _bind_methods();
	virtual void gui_input(const Ref<InputEvent> &p_event) override;

public:
	void set_camera(Camera3D *p_camera) { camera = p_camera; }

	EditorPaneRotationControl();
};
