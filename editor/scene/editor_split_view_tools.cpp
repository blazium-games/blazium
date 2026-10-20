/**************************************************************************/
/*  editor_split_view_tools.cpp                                           */
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

#include "editor_split_view_tools.h"

#include "core/input/input_event.h"
#include "core/math/geometry_2d.h"
#include "core/math/plane.h"
#include "core/math/triangle_mesh.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/editor_data.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/scene/3d/node_3d_editor_plugin.h"
#include "editor/scene/canvas_item_editor_plugin.h"
#include "editor/settings/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/2d/node_2d.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/3d/visual_instance_3d.h"
#include "scene/gui/button.h"
#include "scene/gui/control.h"
#include "scene/gui/separator.h"
#include "scene/main/viewport.h"
#include "scene/resources/font.h"
#include "scene/resources/mesh.h"

// Sizes on screen, before editor scale.
static constexpr real_t HANDLE_LENGTH = 80.0;
static constexpr real_t HANDLE_LENGTH_2D = 60.0;
static constexpr real_t RING_RADIUS_2D = 40.0;
static constexpr real_t GRAB_DISTANCE = 8.0;
static constexpr real_t CENTER_GRAB_DISTANCE = 10.0;
static constexpr int RING_SEGMENTS = 64;
// Snap steps where the 3D editor's don't apply.
// The 2D editor's snapping steps, so both views snap alike.
static void _get_snap_2d(Point2 &r_step, Point2 &r_offset, real_t &r_rotation_step, real_t &r_scale_step) {
	const Ref<CanvasItemManipulator> manipulator = CanvasItemEditor::get_singleton()->get_canvas_item_manipulator();
	r_step = manipulator->get_grid_step() * Math::pow(2.0, manipulator->get_grid_step_multiplier());
	r_offset = manipulator->get_grid_offset();
	r_rotation_step = manipulator->get_snap_rotation_step();
	r_scale_step = manipulator->get_snap_scale_step();
}

static real_t _wrap_angle(real_t p_angle) {
	return Math::fposmod(p_angle + Math::PI, Math::TAU) - Math::PI;
}

static real_t _distance_to_polyline(const Point2 &p_point, const Vector<Point2> &p_points, bool p_closed) {
	real_t best = Math::INF;
	const int count = p_points.size();
	for (int i = 0; i < count - (p_closed ? 0 : 1); i++) {
		const Point2 a = p_points[i];
		const Point2 b = p_points[(i + 1) % count];
		best = MIN(best, p_point.distance_to(Geometry2D::get_closest_point_to_segment(p_point, a, b)));
	}
	return best;
}

static real_t _distance_to_segment(const Point2 &p_point, const Point2 &p_a, const Point2 &p_b) {
	return p_point.distance_to(Geometry2D::get_closest_point_to_segment(p_point, p_a, p_b));
}

Node *EditorPaneTools::_get_root() const {
	return ObjectDB::get_instance<Node>(root_id);
}

bool EditorPaneTools::_is_3d() const {
	return Object::cast_to<Node3D>(_get_root()) != nullptr;
}

int EditorPaneTools::_get_scene_index() const {
	if (root_id.is_null()) {
		return -1;
	}
	EditorData &editor_data = EditorNode::get_editor_data();
	for (int i = 0; i < editor_data.get_edited_scene_count(); i++) {
		const Node *root = editor_data.get_edited_scene_root(i);
		if (root && root->get_instance_id() == root_id) {
			return i;
		}
	}
	return -1;
}

int EditorPaneTools::get_history_id() const {
	const int idx = _get_scene_index();
	if (idx < 0) {
		return EditorUndoRedoManager::INVALID_HISTORY;
	}
	return EditorNode::get_editor_data().get_scene_history_id(idx);
}

bool EditorPaneTools::_is_snapping(bool p_ctrl) const {
	// As in the 2D and 3D editors: holding Ctrl flips the snap button.
	return snap_button->is_pressed() != p_ctrl;
}

Node *EditorPaneTools::get_selected() const {
	Node *node = ObjectDB::get_instance<Node>(selected_id);
	Node *root = _get_root();
	if (!node || !root || (node != root && !root->is_ancestor_of(node))) {
		return nullptr;
	}
	return node;
}

void EditorPaneTools::set_selected(Node *p_node, bool p_emit) {
	const ObjectID id = p_node ? p_node->get_instance_id() : ObjectID();
	if (id == selected_id) {
		return;
	}
	if (dragging) {
		_end_drag(false);
	}
	selected_id = id;
	hover = HANDLE_NONE;
	if (overlay) {
		overlay->queue_redraw();
	}
	if (p_emit) {
		emit_signal(SNAME("selection_changed"));
	}
}

void EditorPaneTools::set_scene(Node *p_root) {
	const ObjectID id = p_root ? p_root->get_instance_id() : ObjectID();
	if (id == root_id) {
		return;
	}
	if (dragging) {
		_end_drag(false);
	}
	root_id = id;
	selected_id = ObjectID();
	hover = HANDLE_NONE;
	_history_changed();
}

Node *EditorPaneTools::_selectable(Node *p_node) const {
	// What a click picks, as in the Scene dock: nodes inside an instanced
	// scene pick that instance, unless its children are editable.
	Node *root = _get_root();
	for (Node *node = p_node; node; node = node->get_parent()) {
		if (node == root) {
			return root;
		}
		Node *owner = node->get_owner();
		if (owner == root || (owner && root->is_editable_instance(owner))) {
			return node;
		}
	}
	return nullptr;
}

void EditorPaneTools::_pick_3d(Node *p_node, const Vector3 &p_from, const Vector3 &p_dir, Node *&r_best, real_t &r_best_distance) const {
	GeometryInstance3D *vi = Object::cast_to<GeometryInstance3D>(p_node);
	if (vi && vi->is_visible_in_tree()) {
		const Transform3D xform = vi->get_global_transform();
		const AABB aabb = xform.xform(vi->get_aabb());
		bool inside = false;
		Vector3 hit;
		if (aabb.find_intersects_ray(p_from, p_dir, inside, &hit)) {
			real_t distance = inside ? 0.0 : p_from.distance_to(hit);
			bool accept = true;
			// Meshes: their actual faces, not just the box around them.
			MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(p_node);
			if (mi && mi->get_mesh().is_valid()) {
				Ref<TriangleMesh> triangles = mi->get_mesh()->generate_triangle_mesh();
				if (triangles.is_valid()) {
					const Transform3D inverse = xform.affine_inverse();
					Vector3 point;
					Vector3 normal;
					accept = triangles->intersect_ray(inverse.xform(p_from), inverse.basis.xform(p_dir).normalized(), point, normal);
					if (accept) {
						distance = p_from.distance_to(xform.xform(point));
					}
				}
			}
			if (accept && distance < r_best_distance) {
				r_best = p_node;
				r_best_distance = distance;
			}
		}
	}
	for (Node *child : p_node->iterate_children()) {
		_pick_3d(child, p_from, p_dir, r_best, r_best_distance);
	}
}

void EditorPaneTools::_pick_3d_origin(Node *p_node, const Point2 &p_pos, Node *&r_best, real_t &r_best_distance) const {
	// Nodes with nothing to click on (lights, markers, cameras): their origin.
	Node3D *node_3d = Object::cast_to<Node3D>(p_node);
	if (node_3d && !Object::cast_to<GeometryInstance3D>(p_node) && node_3d->is_visible_in_tree()) {
		const Vector3 origin = node_3d->get_global_position();
		if (!camera->is_position_behind(origin)) {
			const real_t distance = camera->unproject_position(origin).distance_to(p_pos);
			if (distance < CENTER_GRAB_DISTANCE * EDSCALE && distance < r_best_distance) {
				r_best = p_node;
				r_best_distance = distance;
			}
		}
	}
	for (Node *child : p_node->iterate_children()) {
		_pick_3d_origin(child, p_pos, r_best, r_best_distance);
	}
}

void EditorPaneTools::_collect_canvas_items(Node *p_node, Vector<CanvasItem *> &r_items) const {
	if (CanvasItem *item = Object::cast_to<CanvasItem>(p_node)) {
		r_items.push_back(item);
	}
	for (Node *child : p_node->iterate_children()) {
		_collect_canvas_items(child, r_items);
	}
}

Transform2D EditorPaneTools::_screen_transform_2d(const CanvasItem *p_item) const {
	return viewport->get_global_canvas_transform() * p_item->get_global_transform();
}

Node *EditorPaneTools::_pick(const Point2 &p_pos) const {
	Node *root = _get_root();
	if (!root) {
		return nullptr;
	}
	if (_is_3d()) {
		if (!camera->is_inside_tree()) {
			return nullptr;
		}
		Node *best = nullptr;
		real_t best_distance = Math::INF;
		_pick_3d(root, camera->project_ray_origin(p_pos), camera->project_ray_normal(p_pos), best, best_distance);
		if (!best) {
			best_distance = Math::INF;
			_pick_3d_origin(root, p_pos, best, best_distance);
		}
		return best ? _selectable(best) : nullptr;
	}
	// 2D: the topmost item under the mouse, drawn last.
	Vector<CanvasItem *> items;
	_collect_canvas_items(root, items);
	for (int i = items.size() - 1; i >= 0; i--) {
		CanvasItem *item = items[i];
		if (!item->is_visible_in_tree()) {
			continue;
		}
		const Transform2D xform = _screen_transform_2d(item);
		const real_t scale = MAX(xform.get_scale().abs().x, (real_t)CMP_EPSILON);
		if (item->_edit_is_selected_on_click(xform.affine_inverse().xform(p_pos), GRAB_DISTANCE * 0.5 * EDSCALE / scale)) {
			return _selectable(item);
		}
	}
	return nullptr;
}

bool EditorPaneTools::_get_gizmo_3d(Vector3 &r_origin, Basis &r_axes, real_t &r_size) const {
	Node3D *node = Object::cast_to<Node3D>(get_selected());
	if (!node || !node->is_inside_tree() || !camera->is_inside_tree()) {
		return false;
	}
	const Transform3D xform = node->get_global_transform();
	r_origin = xform.origin;
	if (camera->is_position_behind(r_origin)) {
		return false;
	}
	// Scaling is always along the node's own axes.
	r_axes = local_button->is_pressed() || tool == TOOL_SCALE ? xform.basis.orthonormalized() : Basis();
	if (r_axes.determinant() == 0) {
		r_axes = Basis();
	}
	// The same size on screen, however far the node is.
	const Vector3 right = camera->get_global_transform().basis.get_column(0).normalized();
	const real_t pixels_per_unit = camera->unproject_position(r_origin).distance_to(camera->unproject_position(r_origin + right));
	if (pixels_per_unit < CMP_EPSILON) {
		return false;
	}
	r_size = HANDLE_LENGTH * EDSCALE / pixels_per_unit;
	return true;
}

bool EditorPaneTools::_get_gizmo_2d(Point2 &r_origin, Vector2 *r_axes) const {
	CanvasItem *item = Object::cast_to<CanvasItem>(get_selected());
	if (!item || !item->is_inside_tree()) {
		return false;
	}
	const Transform2D xform = _screen_transform_2d(item);
	// Controls turn and scale around their pivot.
	const Control *control = Object::cast_to<Control>(item);
	r_origin = control ? xform.xform(control->get_combined_pivot_offset()) : xform.get_origin();
	if (local_button->is_pressed() || tool == TOOL_SCALE) {
		r_axes[0] = xform.columns[0].normalized();
		r_axes[1] = xform.columns[1].normalized();
		if (r_axes[0].is_zero_approx() || r_axes[1].is_zero_approx()) {
			r_axes[0] = Vector2(1, 0);
			r_axes[1] = Vector2(0, 1);
		}
	} else {
		r_axes[0] = Vector2(1, 0);
		r_axes[1] = Vector2(0, 1);
	}
	return true;
}

Vector<Point2> EditorPaneTools::_ring_points_3d(const Vector3 &p_origin, const Basis &p_axes, real_t p_size, int p_axis) const {
	Vector<Point2> points;
	const Vector3 u = p_axes.get_column((p_axis + 1) % 3);
	const Vector3 v = p_axes.get_column((p_axis + 2) % 3);
	for (int i = 0; i < RING_SEGMENTS; i++) {
		const real_t angle = Math::TAU * i / RING_SEGMENTS;
		points.push_back(camera->unproject_position(p_origin + (u * Math::cos(angle) + v * Math::sin(angle)) * p_size));
	}
	return points;
}

EditorPaneTools::Handle EditorPaneTools::_handle_at(const Point2 &p_pos) const {
	if (tool == TOOL_SELECT || !show_handles) {
		return HANDLE_NONE;
	}
	Handle best = HANDLE_NONE;
	real_t best_distance = GRAB_DISTANCE * EDSCALE;
	if (_is_3d()) {
		Vector3 origin;
		Basis axes;
		real_t size = 1.0;
		if (!_get_gizmo_3d(origin, axes, size)) {
			return HANDLE_NONE;
		}
		const Point2 screen_origin = camera->unproject_position(origin);
		if (tool != TOOL_ROTATE && p_pos.distance_to(screen_origin) < CENTER_GRAB_DISTANCE * EDSCALE) {
			return HANDLE_CENTER;
		}
		for (int i = 0; i < 3; i++) {
			real_t distance;
			if (tool == TOOL_ROTATE) {
				distance = _distance_to_polyline(p_pos, _ring_points_3d(origin, axes, size, i), true);
			} else {
				distance = _distance_to_segment(p_pos, screen_origin, camera->unproject_position(origin + axes.get_column(i) * size));
			}
			if (distance < best_distance) {
				best = Handle(HANDLE_X + i);
				best_distance = distance;
			}
		}
		return best;
	}
	Point2 origin;
	Vector2 axes[2];
	if (!_get_gizmo_2d(origin, axes)) {
		return HANDLE_NONE;
	}
	if (tool == TOOL_ROTATE) {
		return Math::abs(p_pos.distance_to(origin) - RING_RADIUS_2D * EDSCALE) < best_distance ? HANDLE_CENTER : HANDLE_NONE;
	}
	if (p_pos.distance_to(origin) < CENTER_GRAB_DISTANCE * EDSCALE) {
		return HANDLE_CENTER;
	}
	for (int i = 0; i < 2; i++) {
		const real_t distance = _distance_to_segment(p_pos, origin, origin + axes[i] * HANDLE_LENGTH_2D * EDSCALE);
		if (distance < best_distance) {
			best = Handle(HANDLE_X + i);
			best_distance = distance;
		}
	}
	return best;
}

void EditorPaneTools::_begin_drag(Handle p_handle, const Point2 &p_pos) {
	Node *node = get_selected();
	if (!node) {
		return;
	}
	if (Node3D *node_3d = Object::cast_to<Node3D>(node)) {
		Vector3 origin;
		if (!_get_gizmo_3d(origin, drag_axes, drag_size)) {
			return;
		}
		drag_start_3d = node_3d->get_global_transform();
		drag_start_local_3d = node_3d->get_transform();
	} else if (CanvasItem *item = Object::cast_to<CanvasItem>(node)) {
		Point2 origin;
		if (!_get_gizmo_2d(origin, drag_axes_2d)) {
			return;
		}
		if (Node2D *node_2d = Object::cast_to<Node2D>(item)) {
			drag_start_position = node_2d->get_position();
			drag_start_global_position = node_2d->get_global_position();
			drag_start_rotation = node_2d->get_rotation();
			drag_start_scale = node_2d->get_scale();
		} else if (Control *control = Object::cast_to<Control>(item)) {
			drag_start_position = control->get_position();
			drag_start_global_position = control->get_global_position();
			drag_start_rotation = control->get_rotation();
			drag_start_scale = control->get_scale();
		} else {
			return;
		}
	} else {
		return;
	}
	dragging = true;
	drag_changed = false;
	drag_handle = p_handle;
	drag_from = p_pos;
}

void EditorPaneTools::_update_drag(const Point2 &p_pos, bool p_ctrl) {
	Node *node = get_selected();
	if (!node) {
		_end_drag(false);
		return;
	}
	if (!drag_changed && p_pos.distance_to(drag_from) < 2 * EDSCALE) {
		return;
	}
	drag_changed = true;
	if (Object::cast_to<Node3D>(node)) {
		_update_drag_3d(node, p_pos, _is_snapping(p_ctrl));
	} else if (CanvasItem *item = Object::cast_to<CanvasItem>(node)) {
		_update_drag_2d(item, p_pos, _is_snapping(p_ctrl));
	}
	overlay->queue_redraw();
}

void EditorPaneTools::_update_drag_3d(Node *p_node, const Point2 &p_pos, bool p_snap) {
	Node3D *node = Object::cast_to<Node3D>(p_node);
	const Transform3D camera_xform = camera->get_global_transform();
	const Vector3 forward = -camera_xform.basis.get_column(2).normalized();
	const Vector3 origin = drag_start_3d.origin;
	const Point2 screen_origin = camera->unproject_position(origin);
	Transform3D xform = drag_start_3d;

	switch (tool) {
		case TOOL_MOVE: {
			Vector3 normal = forward;
			Vector3 axis;
			const bool on_axis = drag_handle >= HANDLE_X && drag_handle <= HANDLE_Z;
			if (on_axis) {
				// The plane through the axis that faces the camera the most.
				axis = drag_axes.get_column(drag_handle - HANDLE_X);
				const Vector3 facing = axis.cross(forward).cross(axis);
				if (facing.length_squared() > CMP_EPSILON) {
					normal = facing.normalized();
				}
			}
			const Plane plane(normal, origin);
			Vector3 from_hit;
			Vector3 to_hit;
			if (!plane.intersects_ray(camera->project_ray_origin(drag_from), camera->project_ray_normal(drag_from), &from_hit) || !plane.intersects_ray(camera->project_ray_origin(p_pos), camera->project_ray_normal(p_pos), &to_hit)) {
				return;
			}
			const real_t step = Node3DEditor::get_singleton()->get_translate_snap();
			Vector3 motion = to_hit - from_hit;
			if (on_axis) {
				real_t distance = motion.dot(axis);
				if (p_snap && step > 0) {
					distance = Math::snapped(distance, step);
				}
				motion = axis * distance;
			} else if (p_snap && step > 0) {
				motion = motion.snapped(Vector3(step, step, step));
			}
			xform.origin = origin + motion;
		} break;

		case TOOL_ROTATE: {
			// Turned by the mouse's angle around the node, as seen on screen.
			const Vector3 axis = drag_handle >= HANDLE_X && drag_handle <= HANDLE_Z ? drag_axes.get_column(drag_handle - HANDLE_X) : -forward;
			real_t angle = _wrap_angle((p_pos - screen_origin).angle() - (drag_from - screen_origin).angle());
			// Screen angles grow clockwise; seen from where an axis points,
			// turning around it goes counterclockwise.
			if (axis.dot(camera_xform.origin - origin) > 0) {
				angle = -angle;
			}
			const real_t step = Math::deg_to_rad(Node3DEditor::get_singleton()->get_rotate_snap());
			if (p_snap && step > 0) {
				angle = Math::snapped(angle, step);
			}
			xform.basis = Basis(axis.normalized(), angle) * drag_start_3d.basis;
		} break;

		case TOOL_SCALE: {
			const real_t step = Node3DEditor::get_singleton()->get_scale_snap() / 100.0;
			if (drag_handle >= HANDLE_X && drag_handle <= HANDLE_Z) {
				const int axis = drag_handle - HANDLE_X;
				const Vector2 screen_axis = camera->unproject_position(origin + drag_axes.get_column(axis) * drag_size) - screen_origin;
				const real_t length = screen_axis.length();
				if (length < 1.0) {
					return;
				}
				real_t factor = 1.0 + (p_pos - drag_from).dot(screen_axis / length) / length;
				if (p_snap && step > 0) {
					factor = Math::snapped(factor, step);
				}
				factor = MAX(factor, (real_t)0.01);
				xform.basis.set_column(axis, drag_start_3d.basis.get_column(axis) * factor);
			} else {
				const Vector2 motion = p_pos - drag_from;
				real_t factor = 1.0 + (motion.x - motion.y) / (HANDLE_LENGTH * EDSCALE);
				if (p_snap && step > 0) {
					factor = Math::snapped(factor, step);
				}
				factor = MAX(factor, (real_t)0.01);
				xform.basis = drag_start_3d.basis * factor;
			}
		} break;

		default:
			return;
	}
	node->set_global_transform(xform);
}

void EditorPaneTools::_update_drag_2d(CanvasItem *p_item, const Point2 &p_pos, bool p_snap) {
	Node2D *node_2d = Object::cast_to<Node2D>(p_item);
	Control *control = Object::cast_to<Control>(p_item);
	const Transform2D canvas_inverse = viewport->get_global_canvas_transform().affine_inverse();
	Point2 origin;
	Vector2 axes[2];
	_get_gizmo_2d(origin, axes);
	Point2 snap_step;
	Point2 snap_offset;
	real_t snap_rotation = 0.0;
	real_t snap_scale = 0.0;
	_get_snap_2d(snap_step, snap_offset, snap_rotation, snap_scale);

	switch (tool) {
		case TOOL_MOVE: {
			const Vector2 motion = p_pos - drag_from;
			Point2 position;
			if (drag_handle == HANDLE_X || drag_handle == HANDLE_Y) {
				const int axis = drag_handle - HANDLE_X;
				const Vector2 canvas_axis = canvas_inverse.basis_xform(drag_axes_2d[axis]).normalized();
				real_t distance = canvas_inverse.basis_xform(motion).dot(canvas_axis);
				if (p_snap && snap_step[axis] > 0) {
					distance = Math::snapped(distance, snap_step[axis]);
				}
				position = drag_start_global_position + canvas_axis * distance;
			} else {
				position = drag_start_global_position + canvas_inverse.basis_xform(motion);
				if (p_snap) {
					// Onto the 2D editor's grid.
					for (int i = 0; i < 2; i++) {
						if (snap_step[i] > 0) {
							position[i] = Math::snapped(position[i] - snap_offset[i], snap_step[i]) + snap_offset[i];
						}
					}
				}
			}
			if (node_2d) {
				node_2d->set_global_position(position);
			} else if (control) {
				control->set_global_position(position);
			}
		} break;

		case TOOL_ROTATE: {
			// The pivot stays where it was on screen while turning.
			real_t angle = _wrap_angle((p_pos - origin).angle() - (drag_from - origin).angle());
			if (p_snap && snap_rotation > 0) {
				angle = Math::snapped(angle, snap_rotation);
			}
			if (node_2d) {
				node_2d->set_rotation(drag_start_rotation + angle);
			} else if (control) {
				control->set_rotation(drag_start_rotation + angle);
			}
		} break;

		case TOOL_SCALE: {
			const Vector2 motion = p_pos - drag_from;
			const real_t length = HANDLE_LENGTH_2D * EDSCALE;
			Vector2 factor = Vector2(1, 1);
			if (drag_handle == HANDLE_X || drag_handle == HANDLE_Y) {
				const int axis = drag_handle - HANDLE_X;
				factor[axis] = 1.0 + motion.dot(drag_axes_2d[axis]) / length;
			} else {
				factor.x = 1.0 + (motion.x - motion.y) / length;
				factor.y = factor.x;
			}
			for (int i = 0; i < 2; i++) {
				if (p_snap && snap_scale > 0) {
					factor[i] = Math::snapped(factor[i], snap_scale);
				}
				factor[i] = MAX(factor[i], (real_t)0.01);
			}
			if (node_2d) {
				node_2d->set_scale(drag_start_scale * factor);
			} else if (control) {
				control->set_scale(drag_start_scale * factor);
			}
		} break;

		default:
			break;
	}
}

void EditorPaneTools::_end_drag(bool p_commit) {
	if (!dragging) {
		return;
	}
	dragging = false;
	Node *node = get_selected();
	if (!node || !drag_changed) {
		return;
	}
	Node3D *node_3d = Object::cast_to<Node3D>(node);
	Node2D *node_2d = Object::cast_to<Node2D>(node);
	Control *control = Object::cast_to<Control>(node);
	const int history_id = get_history_id();
	if (!p_commit || history_id == EditorUndoRedoManager::INVALID_HISTORY) {
		// Put it back.
		if (node_3d) {
			node_3d->set_transform(drag_start_local_3d);
		} else if (node_2d) {
			node_2d->set_position(drag_start_position);
			node_2d->set_rotation(drag_start_rotation);
			node_2d->set_scale(drag_start_scale);
		} else if (control) {
			control->set_position(drag_start_position);
			control->set_rotation(drag_start_rotation);
			control->set_scale(drag_start_scale);
		}
		overlay->queue_redraw();
		return;
	}

	static const char *action_names[TOOL_MAX] = { "", TTRC("Move"), TTRC("Rotate"), TTRC("Scale") };
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action_for_history(TTR(action_names[tool]), history_id);
	undo_redo->force_fixed_history();
	if (node_3d) {
		undo_redo->add_do_property(node_3d, "transform", node_3d->get_transform());
		undo_redo->add_undo_property(node_3d, "transform", drag_start_local_3d);
	} else {
		CanvasItem *item = Object::cast_to<CanvasItem>(node);
		switch (tool) {
			case TOOL_MOVE:
				undo_redo->add_do_property(item, "position", item->get("position"));
				undo_redo->add_undo_property(item, "position", drag_start_position);
				break;
			case TOOL_ROTATE:
				undo_redo->add_do_property(item, "rotation", item->get("rotation"));
				undo_redo->add_undo_property(item, "rotation", drag_start_rotation);
				break;
			case TOOL_SCALE:
				undo_redo->add_do_property(item, "scale", item->get("scale"));
				undo_redo->add_undo_property(item, "scale", drag_start_scale);
				break;
			default:
				break;
		}
	}
	undo_redo->commit_action();
	overlay->queue_redraw();
}

bool EditorPaneTools::handle_input(const Ref<InputEvent> &p_event) {
	Node *root = _get_root();
	if (!root || !is_visible_in_tree()) {
		return false;
	}

	const Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid()) {
		if (mb->get_button_index() == MouseButton::LEFT) {
			if (mb->is_pressed()) {
				const Handle handle = _handle_at(mb->get_position());
				if (handle != HANDLE_NONE) {
					_begin_drag(handle, mb->get_position());
					return true;
				}
				Node *picked = _pick(mb->get_position());
				set_selected(picked);
				// With a transform tool, what was clicked can be dragged right away.
				if (picked && tool != TOOL_SELECT) {
					_begin_drag(HANDLE_BODY, mb->get_position());
				}
				overlay->queue_redraw();
			} else {
				_end_drag(true);
			}
			return true;
		}
		if (mb->get_button_index() == MouseButton::RIGHT && mb->is_pressed() && dragging) {
			_end_drag(false);
			return true;
		}
		return false;
	}

	const Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid()) {
		if (dragging) {
			_update_drag(mm->get_position(), mm->is_command_or_control_pressed());
			return true;
		}
		const Handle handle = _handle_at(mm->get_position());
		if (handle != hover) {
			hover = handle;
			overlay->queue_redraw();
		}
		return false;
	}

	const Ref<InputEventKey> k = p_event;
	if (k.is_valid() && k->is_pressed() && !k->is_echo() && !k->is_command_or_control_pressed() && !k->is_alt_pressed() && !k->is_shift_pressed() && !k->is_meta_pressed()) {
		switch (k->get_keycode()) {
			case Key::Q:
				_set_tool(TOOL_SELECT);
				return true;
			case Key::W:
				_set_tool(TOOL_MOVE);
				return true;
			case Key::E:
				_set_tool(TOOL_ROTATE);
				return true;
			case Key::R:
				_set_tool(TOOL_SCALE);
				return true;
			case Key::T:
				local_button->set_pressed(!local_button->is_pressed());
				return true;
			case Key::Y:
				snap_button->set_pressed(!snap_button->is_pressed());
				return true;
			case Key::ESCAPE:
				if (dragging) {
					_end_drag(false);
				} else {
					set_selected(nullptr);
				}
				return true;
			default:
				break;
		}
	}
	return false;
}

void EditorPaneTools::_draw_selection_3d(Node *p_node) {
	// The box around everything the node shows, as the 3D editor draws it.
	AABB bounds;
	bool has_bounds = false;
	List<Node *> pending;
	pending.push_back(p_node);
	while (!pending.is_empty()) {
		Node *node = pending.front()->get();
		pending.pop_front();
		const GeometryInstance3D *vi = Object::cast_to<GeometryInstance3D>(node);
		if (vi && vi->is_visible_in_tree()) {
			const AABB aabb = vi->get_global_transform().xform(vi->get_aabb());
			if (has_bounds) {
				bounds.merge_with(aabb);
			} else {
				bounds = aabb;
				has_bounds = true;
			}
		}
		for (Node *child : node->iterate_children()) {
			pending.push_back(child);
		}
	}
	const Color color = Color(1.0, 0.55, 0.1);
	const real_t width = Math::round(EDSCALE);
	if (!has_bounds) {
		Node3D *node_3d = Object::cast_to<Node3D>(p_node);
		const Vector3 origin = node_3d->get_global_position();
		if (!camera->is_position_behind(origin)) {
			const Point2 at = camera->unproject_position(origin);
			const real_t r = 6 * EDSCALE;
			overlay->draw_line(at - Vector2(r, 0), at + Vector2(r, 0), color, width * 2);
			overlay->draw_line(at - Vector2(0, r), at + Vector2(0, r), color, width * 2);
		}
		return;
	}
	for (int i = 0; i < 12; i++) {
		Vector3 from;
		Vector3 to;
		bounds.get_edge(i, from, to);
		if (camera->is_position_behind(from) || camera->is_position_behind(to)) {
			continue;
		}
		overlay->draw_line(camera->unproject_position(from), camera->unproject_position(to), color, width);
	}
}

void EditorPaneTools::_draw_selection_2d(CanvasItem *p_item) {
	const Color color = Color(1.0, 0.55, 0.1);
	const real_t width = Math::round(EDSCALE);
	const Transform2D xform = _screen_transform_2d(p_item);
	if (p_item->_edit_use_rect()) {
		const Rect2 rect = p_item->_edit_get_rect();
		const Point2 corners[4] = { rect.position, Point2(rect.get_end().x, rect.position.y), rect.get_end(), Point2(rect.position.x, rect.get_end().y) };
		for (int i = 0; i < 4; i++) {
			overlay->draw_line(xform.xform(corners[i]), xform.xform(corners[(i + 1) % 4]), color, width);
		}
		return;
	}
	const Point2 at = xform.get_origin();
	const real_t r = 6 * EDSCALE;
	overlay->draw_line(at - Vector2(r, 0), at + Vector2(r, 0), color, width * 2);
	overlay->draw_line(at - Vector2(0, r), at + Vector2(0, r), color, width * 2);
}

static void _draw_arrow_head(Control *p_overlay, const Point2 &p_from, const Point2 &p_to, const Color &p_color) {
	const Vector2 dir = (p_to - p_from).normalized();
	const Vector2 side = dir.orthogonal() * 5 * EDSCALE;
	const Vector<Point2> points = { p_to + dir * 12 * EDSCALE, p_to + side, p_to - side };
	p_overlay->draw_colored_polygon(points, p_color);
}

static void _draw_square(Control *p_overlay, const Point2 &p_at, const Color &p_color, bool p_filled) {
	const real_t half = 4 * EDSCALE;
	p_overlay->draw_rect(Rect2(p_at - Vector2(half, half), Vector2(half, half) * 2), p_color, p_filled, p_filled ? -1.0 : Math::round(2 * EDSCALE));
}

void EditorPaneTools::_draw_gizmo_3d() {
	Vector3 origin;
	Basis axes;
	real_t size = 1.0;
	if (!_get_gizmo_3d(origin, axes, size)) {
		return;
	}
	const Color axis_colors[3] = {
		get_theme_color(SNAME("axis_x_color"), EditorStringName(Editor)),
		get_theme_color(SNAME("axis_y_color"), EditorStringName(Editor)),
		get_theme_color(SNAME("axis_z_color"), EditorStringName(Editor)),
	};
	const Handle active = dragging ? drag_handle : hover;
	const Point2 screen_origin = camera->unproject_position(origin);
	for (int i = 0; i < 3; i++) {
		const bool lit = active == Handle(HANDLE_X + i);
		const Color color = lit ? axis_colors[i].lightened(0.5) : axis_colors[i];
		const real_t width = Math::round((lit ? 3 : 2) * EDSCALE);
		if (tool == TOOL_ROTATE) {
			const Vector<Point2> ring = _ring_points_3d(origin, axes, size, i);
			for (int j = 0; j < ring.size(); j++) {
				overlay->draw_line(ring[j], ring[(j + 1) % ring.size()], color, width, true);
			}
			continue;
		}
		const Point2 end = camera->unproject_position(origin + axes.get_column(i) * size);
		if (end.distance_to(screen_origin) < 6 * EDSCALE) {
			continue; // Pointing at the camera.
		}
		overlay->draw_line(screen_origin, end, color, width, true);
		if (tool == TOOL_MOVE) {
			_draw_arrow_head(overlay, screen_origin, end, color);
		} else {
			_draw_square(overlay, end, color, true);
		}
	}
	if (tool != TOOL_ROTATE) {
		const Color center = active == HANDLE_CENTER ? Color(1, 1, 1) : Color(1, 1, 1, 0.7);
		if (tool == TOOL_MOVE) {
			_draw_square(overlay, screen_origin, center, false);
		} else {
			overlay->draw_arc(screen_origin, 6 * EDSCALE, 0, Math::TAU, 24, center, Math::round(2 * EDSCALE), true);
		}
	}
}

void EditorPaneTools::_draw_gizmo_2d() {
	Point2 origin;
	Vector2 axes[2];
	if (!_get_gizmo_2d(origin, axes)) {
		return;
	}
	const Color axis_colors[2] = {
		get_theme_color(SNAME("axis_x_color"), EditorStringName(Editor)),
		get_theme_color(SNAME("axis_y_color"), EditorStringName(Editor)),
	};
	const Handle active = dragging ? drag_handle : hover;
	if (tool == TOOL_ROTATE) {
		const bool lit = active != HANDLE_NONE;
		overlay->draw_arc(origin, RING_RADIUS_2D * EDSCALE, 0, Math::TAU, RING_SEGMENTS, lit ? Color(1, 1, 1) : Color(1, 1, 1, 0.7), Math::round((lit ? 3 : 2) * EDSCALE), true);
		overlay->draw_circle(origin, 3 * EDSCALE, Color(1, 1, 1, 0.9));
		return;
	}
	for (int i = 0; i < 2; i++) {
		const bool lit = active == Handle(HANDLE_X + i);
		const Color color = lit ? axis_colors[i].lightened(0.5) : axis_colors[i];
		const Point2 end = origin + axes[i] * HANDLE_LENGTH_2D * EDSCALE;
		overlay->draw_line(origin, end, color, Math::round((lit ? 3 : 2) * EDSCALE), true);
		if (tool == TOOL_MOVE) {
			_draw_arrow_head(overlay, origin, end, color);
		} else {
			_draw_square(overlay, end, color, true);
		}
	}
	const Color center = active == HANDLE_CENTER ? Color(1, 1, 1) : Color(1, 1, 1, 0.7);
	if (tool == TOOL_MOVE) {
		_draw_square(overlay, origin, center, false);
	} else {
		overlay->draw_arc(origin, 6 * EDSCALE, 0, Math::TAU, 24, center, Math::round(2 * EDSCALE), true);
	}
}

void EditorPaneTools::draw_overlay() {
	Node *node = get_selected();
	if (!node || !is_visible_in_tree()) {
		return;
	}
	if (Object::cast_to<Node3D>(node)) {
		if (camera->is_inside_tree()) {
			if (show_selection) {
				_draw_selection_3d(node);
			}
			if (tool != TOOL_SELECT && show_handles) {
				_draw_gizmo_3d();
			}
		}
	} else if (CanvasItem *item = Object::cast_to<CanvasItem>(node)) {
		if (item->is_inside_tree()) {
			if (show_selection) {
				_draw_selection_2d(item);
			}
			if (tool != TOOL_SELECT && show_handles) {
				_draw_gizmo_2d();
			}
		}
	}
}

void EditorPaneTools::set_show_selection(bool p_show) {
	show_selection = p_show;
	_queue_overlay_redraw();
}

void EditorPaneTools::set_show_handles(bool p_show) {
	show_handles = p_show;
	hover = HANDLE_NONE;
	_queue_overlay_redraw();
}

bool EditorPaneTools::is_snap_enabled() const {
	return snap_button->is_pressed();
}

void EditorPaneTools::_snap_toggled(bool p_pressed) {
	_queue_overlay_redraw();
}

void EditorPaneTools::_set_tool(int p_tool) {
	ERR_FAIL_INDEX(p_tool, TOOL_MAX);
	if (dragging) {
		_end_drag(false);
	}
	tool = Tool(p_tool);
	for (int i = 0; i < TOOL_MAX; i++) {
		tool_buttons[i]->set_pressed_no_signal(i == p_tool);
	}
	hover = HANDLE_NONE;
	if (overlay) {
		overlay->queue_redraw();
	}
}

void EditorPaneTools::_undo() {
	const int history_id = get_history_id();
	if (history_id == EditorUndoRedoManager::INVALID_HISTORY) {
		return;
	}
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	if (undo_redo->get_or_create_history(history_id).undo_redo->has_undo()) {
		undo_redo->undo_history(history_id);
	}
}

void EditorPaneTools::_redo() {
	const int history_id = get_history_id();
	if (history_id == EditorUndoRedoManager::INVALID_HISTORY) {
		return;
	}
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	if (undo_redo->get_or_create_history(history_id).undo_redo->has_redo()) {
		undo_redo->redo_history(history_id);
	}
}

void EditorPaneTools::_history_changed() {
	const int history_id = get_history_id();
	bool has_undo = false;
	bool has_redo = false;
	if (history_id != EditorUndoRedoManager::INVALID_HISTORY) {
		const UndoRedo *undo_redo = EditorUndoRedoManager::get_singleton()->get_or_create_history(history_id).undo_redo;
		has_undo = undo_redo->has_undo();
		has_redo = undo_redo->has_redo();
	}
	undo_button->set_disabled(!has_undo);
	redo_button->set_disabled(!has_redo);
	if (overlay) {
		// An undo may have moved what's selected.
		overlay->queue_redraw();
	}
}

void EditorPaneTools::_queue_overlay_redraw() {
	if (overlay) {
		overlay->queue_redraw();
	}
}

void EditorPaneTools::setup(SubViewport *p_viewport, Camera3D *p_camera, Control *p_overlay) {
	viewport = p_viewport;
	camera = p_camera;
	overlay = p_overlay;
}

void EditorPaneTools::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			EditorUndoRedoManager::get_singleton()->connect("history_changed", callable_mp(this, &EditorPaneTools::_history_changed));
			EditorUndoRedoManager::get_singleton()->connect("version_changed", callable_mp(this, &EditorPaneTools::_history_changed));
			_history_changed();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			EditorUndoRedoManager::get_singleton()->disconnect("history_changed", callable_mp(this, &EditorPaneTools::_history_changed));
			EditorUndoRedoManager::get_singleton()->disconnect("version_changed", callable_mp(this, &EditorPaneTools::_history_changed));
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			tool_buttons[TOOL_SELECT]->set_button_icon(get_editor_theme_icon(SNAME("ToolSelect")));
			tool_buttons[TOOL_MOVE]->set_button_icon(get_editor_theme_icon(SNAME("ToolMove")));
			tool_buttons[TOOL_ROTATE]->set_button_icon(get_editor_theme_icon(SNAME("ToolRotate")));
			tool_buttons[TOOL_SCALE]->set_button_icon(get_editor_theme_icon(SNAME("ToolScale")));
			local_button->set_button_icon(get_editor_theme_icon(SNAME("Object")));
			snap_button->set_button_icon(get_editor_theme_icon(SNAME("Snap")));
			undo_button->set_button_icon(get_editor_theme_icon(SNAME("UndoRedo")));
			redo_button->set_button_icon(get_editor_theme_icon(SNAME("Redo")));
		} break;
	}
}

void EditorPaneTools::_bind_methods() {
	ADD_SIGNAL(MethodInfo("selection_changed"));
}

EditorPaneTools::EditorPaneTools() {
	add_theme_constant_override("separation", Math::round(2 * EDSCALE));

	static const char *tooltips[TOOL_MAX] = {
		TTRC("Select Mode (Q)\nClick a node in this pane to select it."),
		TTRC("Move Mode (W)\nDrag the selection, or one of its arrows to move along that axis."),
		TTRC("Rotate Mode (E)\nDrag the selection to turn it toward the camera, or a ring to turn it around that axis."),
		TTRC("Scale Mode (R)\nDrag the selection to scale it evenly, or a handle to scale along that axis."),
	};
	static const char *names[TOOL_MAX] = { TTRC("Select Mode"), TTRC("Move Mode"), TTRC("Rotate Mode"), TTRC("Scale Mode") };
	for (int i = 0; i < TOOL_MAX; i++) {
		tool_buttons[i] = memnew(Button);
		tool_buttons[i]->set_theme_type_variation(SceneStringName(FlatButton));
		tool_buttons[i]->set_toggle_mode(true);
		tool_buttons[i]->set_focus_mode(FOCUS_ACCESSIBILITY);
		tool_buttons[i]->set_tooltip_text(tooltips[i]);
		tool_buttons[i]->set_accessibility_name(names[i]);
		tool_buttons[i]->connect(SceneStringName(pressed), callable_mp(this, &EditorPaneTools::_set_tool).bind(i));
		add_child(tool_buttons[i]);
	}
	tool_buttons[TOOL_SELECT]->set_pressed(true);

	add_child(memnew(VSeparator));

	local_button = memnew(Button);
	local_button->set_theme_type_variation(SceneStringName(FlatButton));
	local_button->set_toggle_mode(true);
	local_button->set_focus_mode(FOCUS_ACCESSIBILITY);
	local_button->set_tooltip_text(TTRC("Use Local Space (T)\nMove and turn along the node's own axes."));
	local_button->set_accessibility_name(TTRC("Use Local Space"));
	local_button->connect(SceneStringName(toggled), callable_mp(this, &EditorPaneTools::_queue_overlay_redraw).unbind(1));
	add_child(local_button);

	snap_button = memnew(Button);
	snap_button->set_theme_type_variation(SceneStringName(FlatButton));
	snap_button->set_toggle_mode(true);
	snap_button->set_focus_mode(FOCUS_ACCESSIBILITY);
	snap_button->set_tooltip_text(TTRC("Use Snap (Y)\nHold Ctrl while dragging to do the opposite. The steps are the 2D or 3D editor's."));
	snap_button->set_accessibility_name(TTRC("Use Snap"));
	// The 2D grid can be shown only while snapping.
	snap_button->connect(SceneStringName(toggled), callable_mp(this, &EditorPaneTools::_snap_toggled));
	add_child(snap_button);

	add_child(memnew(VSeparator));

	undo_button = memnew(Button);
	undo_button->set_theme_type_variation(SceneStringName(FlatButton));
	undo_button->set_focus_mode(FOCUS_ACCESSIBILITY);
	undo_button->set_tooltip_text(TTRC("Undo the last change to this scene.\nCtrl+Z does the same while this side is the one being edited."));
	undo_button->set_accessibility_name(TTRC("Undo"));
	undo_button->connect(SceneStringName(pressed), callable_mp(this, &EditorPaneTools::_undo));
	add_child(undo_button);

	redo_button = memnew(Button);
	redo_button->set_theme_type_variation(SceneStringName(FlatButton));
	redo_button->set_focus_mode(FOCUS_ACCESSIBILITY);
	redo_button->set_tooltip_text(TTRC("Redo the last change to this scene that was undone."));
	redo_button->set_accessibility_name(TTRC("Redo"));
	redo_button->connect(SceneStringName(pressed), callable_mp(this, &EditorPaneTools::_redo));
	add_child(redo_button);
}

// EditorPaneRotationControl

real_t EditorPaneRotationControl::_axis_radius() const {
	return 8.0 * EDSCALE;
}

void EditorPaneRotationControl::_get_sorted_axis(Vector<Axis2D> &r_axis) const {
	// As the 3D editor's: each world axis as the camera sees it.
	const Vector2 center = get_size() / 2.0;
	const real_t radius = get_size().x / 2.0 - _axis_radius() - 2.0 * EDSCALE;
	const Basis camera_basis = camera->get_global_transform().basis.orthonormalized().inverse();
	for (int i = 0; i < 3; i++) {
		const Vector3 axis_3d = camera_basis.get_column(i);
		const Vector2 axis_vector = Vector2(axis_3d.x, -axis_3d.y) * radius;
		if (Math::abs(axis_3d.z) < 1.0) {
			Axis2D positive;
			positive.axis = i;
			positive.screen_point = center + axis_vector;
			positive.z_axis = axis_3d.z;
			r_axis.push_back(positive);
			Axis2D negative;
			negative.axis = i + 3;
			negative.screen_point = center - axis_vector;
			negative.z_axis = -axis_3d.z;
			negative.is_positive = false;
			r_axis.push_back(negative);
		} else {
			// Looking straight along the axis.
			Axis2D axis;
			axis.axis = i + (axis_3d.z <= 0 ? 0 : 3);
			axis.screen_point = center;
			axis.z_axis = 1.0;
			axis.is_positive = axis_3d.z > 0;
			r_axis.push_back(axis);
		}
	}
	r_axis.sort_custom<Axis2DCompare>();
}

void EditorPaneRotationControl::_draw_axis(const Axis2D &p_axis) {
	const bool focused = focused_axis == p_axis.axis;
	const int direction = p_axis.axis % 3;
	static const char *colors[3] = { "axis_x_color", "axis_y_color", "axis_z_color" };
	const Color axis_color = get_theme_color(colors[direction], EditorStringName(Editor));
	const double alpha = focused ? 1.0 : Math::remap((p_axis.z_axis + 1.0) / 2.0, 0, 0.5, 0.35, 1.0);
	const Color c = focused ? Color(axis_color.lightened(0.25), 1.0) : Color(axis_color, alpha);
	const Ref<Font> font = get_theme_font(SNAME("rotation_control"), EditorStringName(EditorFonts));
	const int font_size = get_theme_font_size(SNAME("rotation_control_size"), EditorStringName(EditorFonts));
	const real_t circle_radius = _axis_radius();
	if (p_axis.is_positive) {
		const Vector2 center = get_size() / 2.0;
		const Vector2 diff = p_axis.screen_point - center;
		const real_t line_length = MAX(diff.length() - circle_radius - 0.5 * EDSCALE, 0);
		draw_line(center + diff.limit_length(0.5 * EDSCALE), center + diff.limit_length(line_length), c, 1.5 * EDSCALE, true);
		draw_circle(p_axis.screen_point, circle_radius, c, true, -1.0, true);
		const String name = direction == 0 ? "X" : (direction == 1 ? "Y" : "Z");
		const Size2 char_size = font->get_char_size(name[0], font_size);
		const Color text = focused ? Color(1, 1, 1, alpha) : Color(0, 0, 0, alpha * 0.6);
		draw_char(font, p_axis.screen_point + Vector2(-char_size.width / 2.0, char_size.height * 0.25), name, font_size, text);
	} else {
		draw_circle(p_axis.screen_point, circle_radius, c, true, -1.0, true);
		draw_circle(p_axis.screen_point, circle_radius * 0.8, c.darkened(0.4), true, -1.0, true);
		const String name = direction == 0 ? "-X" : (direction == 1 ? "-Y" : "-Z");
		const Size2 string_size = font->get_string_size(name, HORIZONTAL_ALIGNMENT_LEFT, -1.0, font_size);
		const real_t string_height = font->get_ascent(font_size) + font->get_descent(font_size);
		// Only named while hovered, as in the 3D editor.
		const Color text = focused ? Color(1, 1, 1, alpha) : Color(axis_color, 0);
		draw_string(font, p_axis.screen_point + Vector2(-string_size.width / 2.0, string_height * 0.25), name, HORIZONTAL_ALIGNMENT_LEFT, -1.0, font_size, text);
	}
}

void EditorPaneRotationControl::_update_focus() {
	const int original = focused_axis;
	focused_axis = -2;
	const Vector2 mouse = get_local_mouse_position();
	if (mouse.distance_to(get_size() / 2.0) < get_size().x / 2.0) {
		focused_axis = -1;
	}
	Vector<Axis2D> axes;
	_get_sorted_axis(axes);
	for (const Axis2D &axis : axes) {
		if (mouse.distance_to(axis.screen_point) < _axis_radius()) {
			focused_axis = axis.axis;
		}
	}
	if (focused_axis != original) {
		queue_redraw();
	}
}

void EditorPaneRotationControl::gui_input(const Ref<InputEvent> &p_event) {
	if (!camera || !camera->is_inside_tree()) {
		return;
	}
	const Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->get_button_index() == MouseButton::LEFT) {
		if (mb->is_pressed()) {
			if (mb->get_position().distance_to(get_size() / 2.0) < get_size().x / 2.0) {
				pressed = true;
				dragged = false;
				press_position = mb->get_position();
				emit_signal(SNAME("pressed"));
			}
		} else if (pressed) {
			pressed = false;
			if (!dragged && focused_axis >= 0) {
				emit_signal(SNAME("axis_clicked"), focused_axis);
			}
			_update_focus();
		}
		accept_event();
		return;
	}
	const Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid()) {
		if (pressed && (dragged || mm->get_position().distance_to(press_position) > 4 * EDSCALE)) {
			dragged = true;
			focused_axis = -1;
			emit_signal(SNAME("orbit"), mm->get_relative());
			queue_redraw();
			accept_event();
		} else {
			_update_focus();
		}
	}
}

void EditorPaneRotationControl::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW: {
			if (!camera || !camera->is_inside_tree()) {
				return;
			}
			if (focused_axis > -2 || pressed) {
				draw_circle(get_size() / 2.0, get_size().x / 2.0, Color(0.5, 0.5, 0.5, 0.25), true, -1.0, true);
			}
			Vector<Axis2D> axes;
			_get_sorted_axis(axes);
			for (const Axis2D &axis : axes) {
				_draw_axis(axis);
			}
		} break;

		case NOTIFICATION_MOUSE_EXIT: {
			focused_axis = -2;
			queue_redraw();
		} break;
	}
}

void EditorPaneRotationControl::_bind_methods() {
	ADD_SIGNAL(MethodInfo("pressed"));
	// 0-2: +X +Y +Z, 3-5: -X -Y -Z.
	ADD_SIGNAL(MethodInfo("axis_clicked", PropertyInfo(Variant::INT, "axis")));
	ADD_SIGNAL(MethodInfo("orbit", PropertyInfo(Variant::VECTOR2, "relative")));
}

EditorPaneRotationControl::EditorPaneRotationControl() {
	set_custom_minimum_size(Size2(80, 80) * EDSCALE);
	set_focus_mode(FOCUS_NONE);
}
