/**************************************************************************/
/*  editor_split_view.cpp                                                 */
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

#include "editor_split_view.h"

#include "core/config/project_settings.h"
#include "core/input/input_event.h"
#include "core/object/callable_mp.h"
#include "editor/docks/inspector_dock.h"
#include "editor/docks/scene_tree_dock.h"
#include "editor/editor_data.h"
#include "editor/editor_interface.h"
#include "editor/editor_main_screen.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/gui/code_editor.h"
#include "editor/inspector/editor_inspector.h"
#include "editor/scene/3d/node_3d_editor_plugin.h"
#include "editor/scene/canvas_item_editor_plugin.h"
#include "editor/scene/editor_scene_tabs.h"
#include "editor/scene/scene_tree_editor.h"
#include "editor/script/script_editor_base.h"
#include "editor/script/script_editor_plugin.h"
#include "editor/script/syntax_highlighters.h"
#include "editor/settings/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/light_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/3d/visual_instance_3d.h"
#include "scene/3d/world_environment.h"
#include "scene/gui/button.h"
#include "scene/gui/code_edit.h"
#include "scene/gui/label.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/panel.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/popup_menu.h"
#include "scene/gui/subviewport_container.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "scene/resources/3d/sky_material.h"
#include "scene/resources/environment.h"
#include "scene/resources/sky.h"
#include "scene/resources/style_box_flat.h"
#include "servers/display/display_server.h"

// EditorScenePane

void EditorScenePane::_draw_dot() {
	const Color color = focused ? get_theme_color(SNAME("accent_color"), EditorStringName(Editor)) : get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor));
	const Size2 size = dot->get_size();
	dot->draw_circle(size / 2, MIN(size.x, size.y) / 2, color);
}

void EditorScenePane::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			Ref<StyleBoxFlat> header_style;
			header_style.instantiate();
			header_style->set_bg_color(get_theme_color(focused ? SNAME("dark_color_1") : SNAME("dark_color_2"), EditorStringName(Editor)));
			header_style->set_content_margin_individual(8 * EDSCALE, 4 * EDSCALE, 4 * EDSCALE, 4 * EDSCALE);
			header->add_theme_style_override(SceneStringName(panel), header_style);
			const Color dim = get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor));
			type_label->add_theme_color_override(SceneStringName(font_color), dim);
			tree_title->add_theme_color_override(SceneStringName(font_color), dim);
			inspector_title->add_theme_color_override(SceneStringName(font_color), dim);
			file_label->add_theme_font_override(SceneStringName(font), get_theme_font(SNAME("bold"), EditorStringName(EditorFonts)));
			if (pick_button) {
				pick_button->add_theme_font_override(SceneStringName(font), get_theme_font(SNAME("bold"), EditorStringName(EditorFonts)));
				pick_button->set_button_icon(get_theme_icon(SNAME("arrow"), SNAME("OptionButton")));
			}
			if (close_button) {
				close_button->set_button_icon(get_editor_theme_icon(SNAME("Close")));
			}
			if (code_button) {
				code_button->set_button_icon(get_editor_theme_icon(SNAME("Script")));
			}
			if (swap_button) {
				swap_button->set_button_icon(get_editor_theme_icon(SNAME("MirrorX")));
			}
			if (float_button) {
				float_button->set_button_icon(get_editor_theme_icon(SNAME("MakeFloating")));
			}
			dot->queue_redraw();
		} break;
	}
}

void EditorScenePane::set_title(const String &p_title) {
	file_label->set_text(p_title);
	if (pick_button) {
		pick_button->set_text(p_title);
	}
}

EditorScenePane::EditorScenePane(bool p_focused) {
	focused = p_focused;
	set_h_size_flags(SIZE_EXPAND_FILL);
	set_v_size_flags(SIZE_EXPAND_FILL);
	add_theme_constant_override("separation", 0);

	header = memnew(PanelContainer);
	add_child(header);
	HBoxContainer *header_hb = memnew(HBoxContainer);
	header->add_child(header_hb);

	dot = memnew(Control);
	dot->set_custom_minimum_size(Size2(8, 8) * EDSCALE);
	dot->set_v_size_flags(SIZE_SHRINK_CENTER);
	dot->set_mouse_filter(MOUSE_FILTER_IGNORE);
	dot->connect(SceneStringName(draw), callable_mp(this, &EditorScenePane::_draw_dot));
	header_hb->add_child(dot);

	file_label = memnew(Label);
	file_label->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	header_hb->add_child(file_label);

	if (!focused) {
		file_label->hide();
		pick_button = memnew(MenuButton);
		pick_button->set_flat(true);
		pick_button->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
		pick_button->set_icon_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
		pick_button->set_tooltip_text(TTRC("Choose what to show here."));
		pick_button->set_accessibility_name(TTRC("Choose Split View Content"));
		header_hb->add_child(pick_button);
	}

	type_label = memnew(Label);
	type_label->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	type_label->set_h_size_flags(SIZE_EXPAND_FILL);
	type_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	header_hb->add_child(type_label);

	if (!focused) {
		code_button = memnew(Button);
		code_button->set_flat(true);
		code_button->set_toggle_mode(true);
		code_button->set_tooltip_text(TTRC("Show another open script here instead of the scene. The script you switch away from moves here; click it to edit it."));
		code_button->set_accessibility_name(TTRC("Compare Scripts"));
		code_button->hide();
		header_hb->add_child(code_button);

		swap_button = memnew(Button);
		swap_button->set_flat(true);
		swap_button->set_tooltip_text(TTRC("Swap the two sides of the split view."));
		swap_button->set_accessibility_name(TTRC("Swap Split View Sides"));
		swap_button->set_shortcut(ED_SHORTCUT("editor/swap_split_view_sides", TTRC("Swap Split View Sides"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::ALT | KeyModifierMask::SHIFT | Key::BACKSLASH));
		swap_button->set_shortcut_in_tooltip(true);
		header_hb->add_child(swap_button);

		float_button = memnew(Button);
		float_button->set_flat(true);
		float_button->set_toggle_mode(true);
		float_button->set_tooltip_text(TTRC("Show this scene in a window of its own. Close the window to put it back beside the current scene."));
		float_button->set_accessibility_name(TTRC("Float Split View"));
		header_hb->add_child(float_button);

		close_button = memnew(Button);
		close_button->set_flat(true);
		close_button->set_tooltip_text(TTRC("Close the split view."));
		close_button->set_accessibility_name(TTRC("Close Split View"));
		header_hb->add_child(close_button);
	}

	HBoxContainer *body = memnew(HBoxContainer);
	body->set_v_size_flags(SIZE_EXPAND_FILL);
	body->add_theme_constant_override("separation", 0);
	add_child(body);

	tree_strip = memnew(VBoxContainer);
	tree_strip->set_custom_minimum_size(Size2(160, 0) * EDSCALE);
	body->add_child(tree_strip);
	tree_title = memnew(Label);
	tree_title->set_text(TTRC("Scene"));
	tree_title->set_theme_type_variation("HeaderSmall");
	tree_strip->add_child(tree_title);
	tree = memnew(SceneTreeEditor(false, false, false));
	tree->set_v_size_flags(SIZE_EXPAND_FILL);
	tree->set_update_when_invisible(false);
	tree_strip->add_child(tree);

	content = memnew(VBoxContainer);
	content->set_h_size_flags(SIZE_EXPAND_FILL);
	content->set_v_size_flags(SIZE_EXPAND_FILL);
	content->add_theme_constant_override("separation", 0);
	body->add_child(content);

	if (!focused) {
		viewport_container = memnew(SubViewportContainer);
		viewport_container->set_stretch(true);
		viewport_container->set_v_size_flags(SIZE_EXPAND_FILL);
		viewport_container->set_h_size_flags(SIZE_EXPAND_FILL);
		viewport_container->set_mouse_filter(MOUSE_FILTER_STOP);
		viewport_container->set_tooltip_text(TTRC("Click to edit this scene."));
		content->add_child(viewport_container);

		viewport = memnew(SubViewport);
		viewport->set_disable_input(true);
		viewport->set_use_own_world_3d(true);
		viewport->set_embedding_subwindows(true);
		viewport->set_update_mode(SubViewport::UPDATE_WHEN_VISIBLE);
		viewport->set_auto_translate_mode(AUTO_TRANSLATE_MODE_ALWAYS);
		viewport->set_translation_domain(StringName());
		viewport_container->add_child(viewport);

		overlay = memnew(Control);
		overlay->set_mouse_filter(MOUSE_FILTER_IGNORE);
		overlay->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
		viewport_container->add_child(overlay);

		camera_3d = memnew(Camera3D);
		viewport->add_child(camera_3d);

		live_container = memnew(SubViewportContainer);
		live_container->set_stretch(true);
		live_container->set_v_size_flags(SIZE_EXPAND_FILL);
		live_container->set_h_size_flags(SIZE_EXPAND_FILL);
		live_container->set_mouse_filter(MOUSE_FILTER_STOP);
		live_container->set_tooltip_text(TTRC("Click to edit this scene in the 2D or 3D editor."));
		live_container->hide();
		content->add_child(live_container);

		live_viewport = memnew(SubViewport);
		live_viewport->set_disable_input(true);
		live_viewport->set_update_mode(SubViewport::UPDATE_WHEN_VISIBLE);
		live_container->add_child(live_viewport);

		live_camera = memnew(Camera3D);
		live_viewport->add_child(live_camera);

		code_container = memnew(VBoxContainer);
		code_container->set_v_size_flags(SIZE_EXPAND_FILL);
		code_container->set_h_size_flags(SIZE_EXPAND_FILL);
		code_container->hide();
		content->add_child(code_container);

		code_view = memnew(CodeEdit);
		code_view->set_v_size_flags(SIZE_EXPAND_FILL);
		code_view->set_editable(false);
		code_view->set_draw_line_numbers(true);
		code_view->set_context_menu_enabled(false);
		code_view->set_tooltip_text(TTRC("Click to edit this script."));
		code_container->add_child(code_view);

		code_placeholder = memnew(Label);
		code_placeholder->set_v_size_flags(SIZE_EXPAND_FILL);
		code_placeholder->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
		code_placeholder->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
		code_placeholder->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
		code_placeholder->set_text(TTRC("Open another script to compare it here.\nThe script you switch away from appears here."));
		code_container->add_child(code_placeholder);
	}

	inspector_strip = memnew(VBoxContainer);
	inspector_strip->set_custom_minimum_size(Size2(200, 0) * EDSCALE);
	body->add_child(inspector_strip);
	inspector_title = memnew(Label);
	inspector_title->set_text(TTRC("Inspector"));
	inspector_title->set_theme_type_variation("HeaderSmall");
	inspector_title->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	inspector_strip->add_child(inspector_title);
	inspector = memnew(EditorInspector);
	inspector->set_v_size_flags(SIZE_EXPAND_FILL);
	inspector->set_use_folding(true);
	inspector->set_use_doc_hints(false);
	inspector->set_hide_script(true);
	inspector->set_hide_metadata(true);
	inspector->set_read_only(!focused);
	inspector->set_property_name_style(EditorPropertyNameProcessor::get_default_inspector_style());
	inspector_strip->add_child(inspector);
}

// EditorSplitView

EditorSplitView *EditorSplitView::singleton = nullptr;

bool EditorSplitView::_is_scene_screen() const {
	const EditorMainScreen *ms = Object::cast_to<EditorMainScreen>(main_screen);
	if (!ms || ms->get_current_tab() < 0) {
		return false;
	}
	const EditorDock *dock = ms->get_dock(ms->get_current_tab());
	return dock && (dock == CanvasItemEditor::get_singleton() || dock == Node3DEditor::get_singleton());
}

bool EditorSplitView::_is_script_screen() const {
	const EditorMainScreen *ms = Object::cast_to<EditorMainScreen>(main_screen);
	if (!ms || ms->get_current_tab() < 0) {
		return false;
	}
	return ms->get_dock(ms->get_current_tab()) == ScriptEditor::get_singleton();
}

void EditorSplitView::_set_pane_tree_root(Node *p_root) {
	const ObjectID id = p_root ? p_root->get_instance_id() : ObjectID();
	if (id == pane_tree_root) {
		return;
	}
	pane_tree_root = id;
	preview_pane->tree->set_scene_root_override(p_root);
}

int EditorSplitView::_find_scene_index(ObjectID p_root) const {
	if (p_root.is_null()) {
		return -1;
	}
	EditorData &ed = EditorNode::get_editor_data();
	for (int i = 0; i < ed.get_edited_scene_count(); i++) {
		const Node *root = ed.get_edited_scene_root(i);
		if (root && root->get_instance_id() == p_root) {
			return i;
		}
	}
	return -1;
}

Node *EditorSplitView::get_preview_scene() const {
	if (_find_scene_index(preview_scene) < 0) {
		return nullptr;
	}
	return ObjectDB::get_instance<Node>(preview_scene);
}

void EditorSplitView::_take_into_preview(Node *p_root) {
	ERR_FAIL_NULL(p_root);
	if (p_root->get_parent() == preview_pane->viewport) {
		preview_scene = p_root->get_instance_id();
		return;
	}
	if (p_root->get_parent()) {
		p_root->get_parent()->remove_child(p_root);
	}
	// Before adding it, so its nodes keep their editor behavior from the start.
	preview_pane->viewport->set_editor_preview_scene_root(p_root);
	preview_pane->viewport->add_child(p_root);
	preview_scene = p_root->get_instance_id();

	_setup_preview_lighting(p_root);
	_set_pane_tree_root(p_root);
	preview_pane->tree->update_tree();
	// Nothing in the preview is selected; picking a node there edits its scene.
	preview_pane->tree->get_scene_tree()->deselect_all();

	Node *inspected = ObjectDB::get_instance<Node>(last_focus_selected);
	if (!inspected || (inspected != p_root && !p_root->is_ancestor_of(inspected))) {
		inspected = p_root;
	}
	preview_pane->inspector->edit(inspected);
	preview_pane->inspector_title->set_text(vformat(TTR("Inspector: %s"), inspected->get_name()));
	_update_preview_camera();
}

void EditorSplitView::_release_preview() {
	Node *root = ObjectDB::get_instance<Node>(preview_scene);
	preview_pane->inspector->edit(nullptr);
	_set_pane_tree_root(nullptr);
	if (preview_pane->fallback_lighting) {
		memdelete(preview_pane->fallback_lighting);
		preview_pane->fallback_lighting = nullptr;
	}
	if (root && root->get_parent() == preview_pane->viewport) {
		preview_pane->viewport->remove_child(root);
	}
	preview_pane->viewport->set_editor_preview_scene_root(nullptr);
	preview_scene = ObjectID();
}

void EditorSplitView::_collect_bounds(Node *p_node, AABB &r_bounds, bool &r_has_bounds) {
	const VisualInstance3D *vi = Object::cast_to<VisualInstance3D>(p_node);
	if (vi && vi->is_visible_in_tree()) {
		const AABB aabb = vi->get_global_transform().xform(vi->get_aabb());
		if (aabb.has_volume() || aabb.size.length() > 0) {
			if (r_has_bounds) {
				r_bounds.merge_with(aabb);
			} else {
				r_bounds = aabb;
				r_has_bounds = true;
			}
		}
	}
	for (Node *child : p_node->iterate_children()) {
		_collect_bounds(child, r_bounds, r_has_bounds);
	}
}

template <typename T>
static bool _split_view_has_node_of_type(Node *p_node) {
	if (Object::cast_to<T>(p_node)) {
		return true;
	}
	for (Node *child : p_node->iterate_children()) {
		if (_split_view_has_node_of_type<T>(child)) {
			return true;
		}
	}
	return false;
}

void EditorSplitView::_setup_preview_lighting(Node *p_root) {
	if (preview_pane->fallback_lighting) {
		memdelete(preview_pane->fallback_lighting);
		preview_pane->fallback_lighting = nullptr;
	}
	if (!Object::cast_to<Node3D>(p_root)) {
		return;
	}
	// Like the 3D editor's preview sun and environment: each is only added
	// when the scene has none of its own.
	const bool needs_sun = !_split_view_has_node_of_type<DirectionalLight3D>(p_root);
	const bool needs_environment = !_split_view_has_node_of_type<WorldEnvironment>(p_root);
	if (!needs_sun && !needs_environment) {
		return;
	}
	Node3D *lighting = memnew(Node3D);
	if (needs_sun) {
		DirectionalLight3D *sun = memnew(DirectionalLight3D);
		sun->set_rotation(Vector3(Math::deg_to_rad(-60.0), Math::deg_to_rad(150.0), 0));
		sun->set_shadow(true);
		lighting->add_child(sun);
	}
	if (needs_environment) {
		Ref<ProceduralSkyMaterial> sky_material;
		sky_material.instantiate();
		Ref<Sky> sky;
		sky.instantiate();
		sky->set_material(sky_material);
		Ref<Environment> env;
		env.instantiate();
		env->set_background(Environment::BG_SKY);
		env->set_sky(sky);
		env->set_tonemapper(Environment::TONE_MAPPER_FILMIC);
		WorldEnvironment *world_env = memnew(WorldEnvironment);
		world_env->set_environment(env);
		lighting->add_child(world_env);
	}
	preview_pane->viewport->add_child(lighting);
	preview_pane->fallback_lighting = lighting;
}

void EditorSplitView::_update_preview_camera() {
	Node *root = get_preview_scene();
	if (!root) {
		return;
	}
	const int idx = _find_scene_index(preview_scene);
	const Dictionary states = EditorNode::get_editor_data().get_scene_editor_states(idx);
	_frame_view(root, states.get("3D", Dictionary()), preview_pane->camera_3d, preview_pane->viewport, preview_pane->viewport_container->get_size());
	preview_pane->overlay->queue_redraw();
}

void EditorSplitView::_update_live_camera() {
	Node *root = EditorNode::get_singleton()->get_edited_scene();
	if (!root || !showing_live || showing_code) {
		return;
	}
	// The edited scene is drawn in the editor's own 3D world and the scene
	// root's 2D canvas; look into those rather than moving the scene.
	preview_pane->live_viewport->set_world_3d(get_tree()->get_root()->get_world_3d());
	preview_pane->live_viewport->set_world_2d(EditorNode::get_singleton()->get_scene_root()->get_world_2d());
	// Where the 3D editor's camera is now.
	_frame_view(root, Node3DEditor::get_singleton()->get_state(), preview_pane->live_camera, preview_pane->live_viewport, preview_pane->live_container->get_size());
}

void EditorSplitView::_frame_view(Node *p_root, const Dictionary &p_state_3d, Camera3D *p_camera, SubViewport *p_viewport, const Size2 &p_size) {
	if (Object::cast_to<Node3D>(p_root)) {
		// Look at the scene from where its 3D editor camera was.
		Vector3 position;
		real_t x_rot = 0.5;
		real_t y_rot = -0.5;
		real_t distance = 4.0;
		real_t fov = 70.0;
		bool orthogonal = false;
		const Dictionary &state_3d = p_state_3d;
		if (!state_3d.has("viewports")) {
			// Never opened in 3D: frame everything visible in the scene.
			AABB bounds;
			bool has_bounds = false;
			_collect_bounds(p_root, bounds, has_bounds);
			if (has_bounds) {
				position = bounds.get_center();
				distance = MAX(bounds.get_longest_axis_size(), (real_t)1.0) * 1.1;
				x_rot = 0.45;
				y_rot = -0.6;
			}
		} else {
			const Array viewports = state_3d["viewports"];
			if (!viewports.is_empty()) {
				const Dictionary vp = viewports[0];
				position = vp.get("position", position);
				x_rot = vp.get("x_rotation", x_rot);
				y_rot = vp.get("y_rotation", y_rot);
				distance = vp.get("distance", distance);
				orthogonal = vp.get("orthogonal", false);
			}
			fov = state_3d.get("fov", fov);
		}
		Transform3D xform;
		xform.translate_local(position);
		xform.basis.rotate(Vector3(1, 0, 0), -x_rot);
		xform.basis.rotate(Vector3(0, 1, 0), -y_rot);
		xform.translate_local(0, 0, orthogonal ? 50.0 : distance);
		p_camera->set_global_transform(xform);
		if (orthogonal) {
			p_camera->set_orthogonal(2 * distance, 0.05, 4000);
		} else {
			p_camera->set_perspective(fov, 0.05, 4000);
		}
		p_camera->make_current();
		p_viewport->set_global_canvas_transform(Transform2D());
	} else {
		// Fit the project's window into the pane, like "Center View" in 2D.
		const Size2 project_size = Size2(GLOBAL_GET("display/window/size/viewport_width"), GLOBAL_GET("display/window/size/viewport_height"));
		if (project_size.x <= 0 || project_size.y <= 0 || p_size.x <= 0 || p_size.y <= 0) {
			return;
		}
		const real_t scale = MIN(p_size.x / project_size.x, p_size.y / project_size.y) * 0.9;
		Transform2D xform;
		xform.scale_basis(Size2(scale, scale));
		xform.columns[2] = (p_size - project_size * scale) / 2;
		p_viewport->set_global_canvas_transform(xform);
	}
}

void EditorSplitView::_live_gui_input(const Ref<InputEvent> &p_event) {
	const Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
		callable_mp(this, &EditorSplitView::_open_live_scene_editor).call_deferred();
		preview_pane->live_container->accept_event();
	}
}

void EditorSplitView::_open_live_scene_editor() {
	Node *root = EditorNode::get_singleton()->get_edited_scene();
	if (root) {
		EditorInterface::get_singleton()->set_main_screen_editor(Object::cast_to<Node3D>(root) ? "3D" : "2D");
	}
}

void EditorSplitView::_track_current_script() {
	ScriptEditorBase *current = ScriptEditor::get_singleton()->get_current_editor();
	TextEditorBase *current_text = Object::cast_to<TextEditorBase>(current);
	const ObjectID current_id = current_text ? current_text->get_instance_id() : ObjectID();
	if (current_id == current_code_editor_id) {
		return;
	}
	// The script being left goes into the pane, like scenes do.
	if (ObjectDB::get_instance<TextEditorBase>(current_code_editor_id)) {
		code_editor_id = current_code_editor_id;
	}
	current_code_editor_id = current_id;
}

void EditorSplitView::_script_changed(const Ref<Script> &p_script) {
	_track_current_script();
	_queue_update();
}

void EditorSplitView::_set_code_source(CodeEdit *p_source) {
	CodeEdit *old_source = ObjectDB::get_instance<CodeEdit>(code_source_id);
	if (old_source == p_source && p_source) {
		return;
	}
	if (old_source && old_source->is_connected(SceneStringName(text_changed), callable_mp(this, &EditorSplitView::_code_source_changed))) {
		old_source->disconnect(SceneStringName(text_changed), callable_mp(this, &EditorSplitView::_code_source_changed));
	}
	code_source_id = p_source ? p_source->get_instance_id() : ObjectID();
	CodeEdit *view = preview_pane->code_view;
	if (!p_source) {
		view->set_syntax_highlighter(Ref<SyntaxHighlighter>());
		view->set_text(String());
		return;
	}
	p_source->connect(SceneStringName(text_changed), callable_mp(this, &EditorSplitView::_code_source_changed));

	// Look like the script editor: its font, colors and highlighting.
	view->add_theme_font_override(SceneStringName(font), p_source->get_theme_font(SceneStringName(font)));
	view->add_theme_font_size_override(SceneStringName(font_size), p_source->get_theme_font_size(SceneStringName(font_size)));
	static const char *colors[] = { "background_color", "font_color", "line_number_color", "current_line_color", "selection_color", "word_highlighted_color", "caret_color", "brace_mismatch_color" };
	for (const char *color : colors) {
		view->add_theme_color_override(color, p_source->get_theme_color(color));
	}
	// Read-only text would otherwise be drawn dimmed.
	view->add_theme_color_override("font_readonly_color", p_source->get_theme_color(SceneStringName(font_color)));
	Ref<EditorSyntaxHighlighter> highlighter = p_source->get_syntax_highlighter();
	if (highlighter.is_valid()) {
		Ref<EditorSyntaxHighlighter> copy = highlighter->_create();
		if (copy.is_valid()) {
			copy->_set_edited_resource(highlighter->_get_edited_resource());
		}
		view->set_syntax_highlighter(copy);
	} else {
		view->set_syntax_highlighter(Ref<SyntaxHighlighter>());
	}
	view->set_text(p_source->get_text());
	view->set_v_scroll(p_source->get_v_scroll());
}

void EditorSplitView::_code_source_changed() {
	CodeEdit *source = ObjectDB::get_instance<CodeEdit>(code_source_id);
	if (!source) {
		return;
	}
	CodeEdit *view = preview_pane->code_view;
	const double scroll = view->get_v_scroll();
	view->set_text(source->get_text());
	view->set_v_scroll(scroll);
}

void EditorSplitView::_update_code_view() {
	_track_current_script();
	TextEditorBase *shown = ObjectDB::get_instance<TextEditorBase>(code_editor_id);
	if (!shown || code_editor_id == current_code_editor_id) {
		// Nothing left from before: take another open script.
		shown = nullptr;
		code_editor_id = ObjectID();
		DocumentEditorContainer *container = ScriptEditor::get_singleton()->get_script_container();
		for (const Ref<Script> &scr : ScriptEditor::get_singleton()->get_open_scripts()) {
			TextEditorBase *editor = Object::cast_to<TextEditorBase>(container->get_resource_editor(scr));
			if (editor && editor->get_instance_id() != current_code_editor_id) {
				shown = editor;
				code_editor_id = editor->get_instance_id();
				break;
			}
		}
	}
	preview_pane->code_view->set_visible(shown);
	preview_pane->code_placeholder->set_visible(!shown);
	_set_code_source(shown ? shown->get_code_editor()->get_text_editor() : nullptr);

	const Ref<Resource> res = shown ? shown->get_edited_resource() : Ref<Resource>();
	if (res.is_null()) {
		preview_pane->set_title(TTR("No other script"));
		preview_pane->type_label->set_text(String());
	} else {
		String title = res->get_path().is_empty() ? TTR("[unsaved]") : res->get_path().get_file();
		if (shown->is_unsaved()) {
			title += "(*)";
		}
		preview_pane->set_title(title);
		preview_pane->type_label->set_text(res->get_class());
	}
	if (floating) {
		float_window->set_title(vformat(TTR("%s - Split View"), preview_pane->file_label->get_text()));
	}
}

void EditorSplitView::_code_gui_input(const Ref<InputEvent> &p_event) {
	const Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
		const Point2i pos = preview_pane->code_view->get_line_column_at_pos(mb->get_position());
		callable_mp(this, &EditorSplitView::_edit_code_at).call_deferred(pos.y, pos.x);
		preview_pane->code_view->accept_event();
	}
}

void EditorSplitView::_edit_code_at(int p_line, int p_column) {
	TextEditorBase *shown = ObjectDB::get_instance<TextEditorBase>(code_editor_id);
	if (!shown) {
		return;
	}
	ScriptEditor::get_singleton()->edit(shown->get_edited_resource(), p_line, p_column);
	_track_current_script();
	_queue_update();
}

void EditorSplitView::_fill_pick_menu() {
	PopupMenu *menu = preview_pane->pick_button->get_popup();
	menu->clear();
	if (showing_code) {
		// Open scripts other than the one being edited.
		DocumentEditorContainer *container = ScriptEditor::get_singleton()->get_script_container();
		for (const Ref<Script> &scr : ScriptEditor::get_singleton()->get_open_scripts()) {
			TextEditorBase *editor = Object::cast_to<TextEditorBase>(container->get_resource_editor(scr));
			if (!editor || editor->get_instance_id() == current_code_editor_id) {
				continue;
			}
			const String name = scr->get_path().is_empty() ? TTR("[unsaved]") : scr->get_path().get_file();
			menu->add_radio_check_item(name);
			const int idx = menu->get_item_count() - 1;
			menu->set_item_metadata(idx, editor->get_instance_id());
			menu->set_item_tooltip(idx, scr->get_path());
			menu->set_item_checked(idx, editor->get_instance_id() == code_editor_id);
		}
		if (menu->get_item_count() == 0) {
			menu->add_item(TTR("No other open script"));
			menu->set_item_disabled(-1, true);
		}
		return;
	}
	if (showing_live) {
		menu->add_item(TTR("This is the scene being edited"));
		menu->set_item_disabled(-1, true);
		return;
	}
	// Open scenes other than the one being edited.
	EditorData &ed = EditorNode::get_editor_data();
	for (int i = 0; i < ed.get_edited_scene_count(); i++) {
		if (i == ed.get_edited_scene() || !ed.get_edited_scene_root(i)) {
			continue;
		}
		const String path = ed.get_scene_path(i);
		menu->add_radio_check_item(path.is_empty() ? TTR("[unsaved]") : path.get_file());
		const int idx = menu->get_item_count() - 1;
		menu->set_item_metadata(idx, i);
		menu->set_item_tooltip(idx, path);
		menu->set_item_checked(idx, ed.get_edited_scene_root(i)->get_instance_id() == preview_scene);
	}
	if (menu->get_item_count() == 0) {
		menu->add_item(TTR("No other open scene"));
		menu->set_item_disabled(-1, true);
	}
}

void EditorSplitView::_pick_menu_id_pressed(int p_id) {
	PopupMenu *menu = preview_pane->pick_button->get_popup();
	const int idx = menu->get_item_index(p_id);
	if (idx < 0) {
		return;
	}
	const Variant meta = menu->get_item_metadata(idx);
	if (showing_code) {
		code_editor_id = ObjectID(uint64_t(meta));
	} else {
		show_scene_in_preview(int(meta));
	}
	_queue_update();
}

void EditorSplitView::_code_toggled(bool p_pressed) {
	EditorSettings::get_singleton()->set("interface/editor/split_view/compare_scripts", p_pressed);
	EditorSettings::get_singleton()->notify_changes();
	compare_scripts = p_pressed;
	_queue_update();
}

void EditorSplitView::_draw_preview_overlay() {
	Control *overlay = preview_pane->overlay;
	Node *root = get_preview_scene();
	if (root && !Object::cast_to<Node3D>(root)) {
		// The project's window, as the 2D editor draws it.
		const Size2 project_size = Size2(GLOBAL_GET("display/window/size/viewport_width"), GLOBAL_GET("display/window/size/viewport_height"));
		const Transform2D xform = preview_pane->viewport->get_global_canvas_transform();
		const Rect2 frame = xform.xform(Rect2(Point2(), project_size));
		overlay->draw_rect(frame, get_theme_color(SNAME("accent_color"), EditorStringName(Editor)) * Color(1, 1, 1, 0.6), false, Math::round(EDSCALE));
	}
	if (overlay->get_global_rect().has_point(overlay->get_global_mouse_position())) {
		const Ref<Font> font = get_theme_font(SNAME("main"), EditorStringName(EditorFonts));
		const int font_size = get_theme_font_size(SNAME("main_size"), EditorStringName(EditorFonts));
		const String hint = TTR("Click to edit this scene");
		const Size2 text_size = font->get_string_size(hint, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size);
		const Point2 pos = Point2((overlay->get_size().x - text_size.x) / 2, overlay->get_size().y - 16 * EDSCALE);
		const Rect2 bg = Rect2(pos - Point2(8, font->get_ascent(font_size) + 4) * EDSCALE, text_size + Size2(16, 8) * EDSCALE);
		overlay->draw_rect(bg, get_theme_color(SNAME("dark_color_1"), EditorStringName(Editor)) * Color(1, 1, 1, 0.85));
		overlay->draw_string(font, pos, hint, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, get_theme_color(SceneStringName(font_color), EditorStringName(Editor)));
	}
}

void EditorSplitView::_focus_preview() {
	const int idx = _find_scene_index(preview_scene);
	if (idx < 0 || idx == EditorNode::get_editor_data().get_edited_scene()) {
		return;
	}
	swap_on_change = true;
	EditorSceneTabs::get_singleton()->set_current_tab(idx);
	swap_on_change = false;
}

void EditorSplitView::_swap_sides() {
	if (floating || preview_pane->get_parent() != this) {
		return;
	}
	move_child(focus_pane, preview_pane->get_index());
	set_split_offset(-get_split_offset());
	// The divider kept for the other screen is now mirrored too.
	if (showing_live) {
		scene_split_offset = -scene_split_offset;
	} else {
		script_split_offset = -script_split_offset;
	}
}

void EditorSplitView::_preview_gui_input(const Ref<InputEvent> &p_event) {
	const Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->is_pressed() && (mb->get_button_index() == MouseButton::LEFT || mb->get_button_index() == MouseButton::RIGHT)) {
		callable_mp(this, &EditorSplitView::_focus_preview).call_deferred();
		preview_pane->viewport_container->accept_event();
	}
	const Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid()) {
		preview_pane->overlay->queue_redraw();
	}
}

void EditorSplitView::_preview_tree_selected() {
	Node *selected = preview_pane->tree->get_selected();
	if (showing_live) {
		// It's the edited scene: select the node as the Scene dock would.
		if (selected) {
			EditorSelection *selection = EditorNode::get_singleton()->get_editor_selection();
			selection->clear();
			selection->add_node(selected);
			EditorNode::get_singleton()->push_node_item(selected);
		}
		return;
	}
	Node *root = get_preview_scene();
	if (!root || !selected) {
		return;
	}
	pending_select = root->get_path_to(selected);
	pending_select_scene = preview_scene;
	callable_mp(this, &EditorSplitView::_focus_preview).call_deferred();
}

void EditorSplitView::_focus_tree_selected() {
	Node *selected = focus_pane->tree->get_selected();
	if (selected) {
		last_focus_selected = selected->get_instance_id();
		EditorNode::get_singleton()->push_node_item(selected);
	}
}

void EditorSplitView::_close_preview() {
	_show_float_window(false);
	EditorSettings::get_singleton()->set("interface/editor/split_view/enabled", false);
	EditorSettings::get_singleton()->notify_changes();
	set_split_enabled(false);
}

void EditorSplitView::_inspected_object_changed() {
	Object *edited = InspectorDock::get_inspector_singleton()->get_edited_object();
	if (Node *node = Object::cast_to<Node>(edited)) {
		last_focus_selected = node->get_instance_id();
	}
	if (focus_pane->inspector_strip->is_visible_in_tree()) {
		focus_pane->inspector->edit(edited);
		Node *node = Object::cast_to<Node>(edited);
		focus_pane->inspector_title->set_text(node ? vformat(TTR("Inspector: %s"), node->get_name()) : TTR("Inspector"));
	}
}

void EditorSplitView::_update_pane_header(EditorScenePane *p_pane, Node *p_root) {
	if (!p_root) {
		p_pane->set_title(String());
		p_pane->type_label->set_text(String());
		return;
	}
	const int idx = _find_scene_index(p_root->get_instance_id());
	const String path = idx >= 0 ? EditorNode::get_editor_data().get_scene_path(idx) : String();
	String title = path.is_empty() ? TTR("[unsaved]") : path.get_file();
	// Not EditorData::is_scene_changed(), which resets the change it reports.
	if (idx >= 0 && EditorUndoRedoManager::get_singleton()->is_history_unsaved(EditorNode::get_editor_data().get_scene_history_id(idx))) {
		title += "(*)";
	}
	p_pane->set_title(title);
	p_pane->type_label->set_text(p_root->get_class());
	if (p_pane == preview_pane && floating) {
		float_window->set_title(vformat(TTR("%s - Split View"), title));
	}
	p_pane->dot->queue_redraw();
}

void EditorSplitView::_update_strips() {
	const bool split = floating ? float_window->is_visible() : preview_pane->is_visible();
	// The focused scene already has the Scene and Inspector docks; only give
	// its pane its own strips when those docks are closed.
	const bool docks_open = SceneTreeDock::get_singleton()->is_visible_in_tree() && InspectorDock::get_singleton()->is_visible_in_tree();
	// Beside the script editor, the focused pane is the script editor.
	const bool focus_strips = split && !showing_live && show_pane_docks && !docks_open;
	focus_pane->tree_strip->set_visible(focus_strips);
	focus_pane->inspector_strip->set_visible(focus_strips && focus_pane->get_size().x >= 900 * EDSCALE);
	if (!focus_pane->inspector_strip->is_visible()) {
		// It only follows the main inspector while shown; don't keep a
		// pointer to an object that may be freed meanwhile.
		focus_pane->inspector->edit(nullptr);
	} else if (focus_pane->inspector->get_edited_object() != InspectorDock::get_inspector_singleton()->get_edited_object()) {
		_inspected_object_changed();
	}
	// Keep the preview's viewport usable on narrow panes.
	const real_t preview_width = preview_pane->get_size().x;
	preview_pane->tree_strip->set_visible(!showing_code && show_pane_docks && preview_width >= 520 * EDSCALE);
	preview_pane->inspector_strip->set_visible(!showing_live && show_pane_docks && preview_width >= 760 * EDSCALE);
}

void EditorSplitView::_queue_update() {
	if (update_queued) {
		return;
	}
	update_queued = true;
	callable_mp(this, &EditorSplitView::_update).call_deferred();
}

void EditorSplitView::_update() {
	update_queued = false;
	Node *preview = get_preview_scene();
	if (!preview && preview_scene.is_valid()) {
		_release_preview();
	}
	if (!preview && enabled) {
		// Scenes opened at startup or from the file system don't go through a
		// scene switch, so there may be nothing in the preview yet.
		_pick_default_preview();
		preview = get_preview_scene();
	}
	Node *edited = EditorNode::get_singleton()->get_edited_scene();
	// Beside the script editor, show the scene being edited, live.
	const bool was_live = showing_live;
	const bool script_screen = enabled && _is_script_screen();
	showing_code = script_screen && compare_scripts;
	showing_live = script_screen && (showing_code || edited);
	if (showing_live != was_live && !floating && preview_pane->get_parent() == this) {
		if (was_live) {
			script_split_offset = get_split_offset();
			has_script_split_offset = true;
			set_split_offset(scene_split_offset);
		} else {
			scene_split_offset = get_split_offset();
			if (!has_script_split_offset) {
				// Give the code about two thirds of the width.
				const int sixth = int(get_size().x / 6);
				script_split_offset = preview_pane->get_index() > focus_pane->get_index() ? sixth : -sixth;
			}
			set_split_offset(script_split_offset);
		}
	}
	const bool split = showing_live || (enabled && preview && _is_scene_screen());

	preview_pane->viewport_container->set_visible(!showing_live);
	preview_pane->live_container->set_visible(showing_live && !showing_code);
	preview_pane->code_container->set_visible(showing_code);
	preview_pane->code_button->set_visible(script_screen);
	preview_pane->code_button->set_pressed_no_signal(compare_scripts);
	if (!showing_code) {
		_set_code_source(nullptr);
	}
	_set_pane_tree_root(showing_live ? (showing_code ? nullptr : edited) : preview);

	if (floating) {
		preview_pane->show();
		_show_float_window(split);
	} else {
		preview_pane->set_visible(split);
	}
	focus_pane->header->set_visible(split && !showing_live);
	_update_strips();

	if (!split) {
		focus_pane->inspector->edit(nullptr);
		return;
	}

	if (showing_code) {
		_update_code_view();
		return;
	}
	_update_pane_header(preview_pane, showing_live ? edited : preview);
	preview_pane->tree->update_tree();
	preview_pane->tree->get_scene_tree()->deselect_all();
	if (showing_live) {
		_update_live_camera();
		return;
	}
	_update_pane_header(focus_pane, edited);
	focus_pane->tree->update_tree();
	_inspected_object_changed();
	_update_preview_camera();
}

Rect2i EditorSplitView::_default_float_rect() const {
	// Where the pane was, or the right half of the editor's screen.
	if (preview_pane->is_visible_in_tree() && preview_pane->get_size().x > 0) {
		return Rect2i(preview_pane->get_screen_position(), preview_pane->get_size());
	}
	const Window *editor_window = EditorNode::get_singleton()->get_window();
	const int screen = editor_window ? editor_window->get_current_screen() : DisplayServer::get_singleton()->get_primary_screen();
	const Rect2i usable = DisplayServer::get_singleton()->screen_get_usable_rect(screen);
	const Size2i size = Size2i(usable.size.x / 2, usable.size.y * 3 / 4);
	return Rect2i(usable.position + Point2i(usable.size.x - size.x, (usable.size.y - size.y) / 2), size);
}

void EditorSplitView::_show_float_window(bool p_show) {
	if (!float_window || p_show == float_window->is_visible()) {
		return;
	}
	if (!p_show) {
		float_rect = Rect2i(float_window->get_position(), float_window->get_size());
		float_window->hide();
		return;
	}
	const Rect2i rect = float_rect.has_area() ? float_rect : _default_float_rect();
	float_window->set_position(rect.position);
	float_window->set_size(rect.size);
	float_window->show();
	// Applied while hidden; apply again now that the native window exists,
	// or it can stay blank until it's resized.
	float_window->set_size(rect.size);
}

void EditorSplitView::_set_floating(bool p_floating) {
	if (p_floating && !EditorNode::get_singleton()->is_multi_window_enabled()) {
		p_floating = false;
	}
	preview_pane->float_button->set_pressed_no_signal(p_floating);
	preview_pane->swap_button->set_visible(!p_floating);
	if (floating == p_floating) {
		return;
	}
	if (p_floating) {
		if (!float_rect.has_area()) {
			float_rect = _default_float_rect();
		}
		docked_index = preview_pane->get_index();
		remove_child(preview_pane);
		float_window->add_child(preview_pane);
		preview_pane->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
		preview_pane->show();
		floating = true;
	} else {
		_show_float_window(false);
		float_window->remove_child(preview_pane);
		preview_pane->set_anchors_and_offsets_preset(PRESET_TOP_LEFT);
		add_child(preview_pane);
		move_child(preview_pane, CLAMP(docked_index, 0, get_child_count() - 1));
		floating = false;
	}
	_queue_update();
}

void EditorSplitView::_apply_float_setting() {
	preview_pane->float_button->set_visible(EditorNode::get_singleton()->is_multi_window_enabled());
	_set_floating(EDITOR_GET("interface/editor/split_view/floating"));
}

void EditorSplitView::_float_toggled(bool p_pressed) {
	_set_floating(p_pressed);
	EditorSettings::get_singleton()->set("interface/editor/split_view/floating", floating);
	EditorSettings::get_singleton()->notify_changes();
}

void EditorSplitView::_float_window_close_requested() {
	_float_toggled(false);
}

void EditorSplitView::set_main_screen(Control *p_main_screen) {
	ERR_FAIL_COND(main_screen != nullptr);
	main_screen = p_main_screen;
	focus_pane->content->add_child(main_screen);
	main_screen->set_v_size_flags(SIZE_EXPAND_FILL);
	main_screen->connect("tab_changed", callable_mp(this, &EditorSplitView::_queue_update).unbind(1));
}

void EditorSplitView::set_split_enabled(bool p_enabled) {
	if (enabled == p_enabled) {
		return;
	}
	enabled = p_enabled;
	if (!enabled) {
		_release_preview();
	} else {
		_pick_default_preview();
	}
	_queue_update();
}

void EditorSplitView::_pick_default_preview() {
	if (!enabled || get_preview_scene()) {
		return;
	}
	// The tab next to the current one: the previous tab if there is one.
	EditorData &ed = EditorNode::get_editor_data();
	const int current = ed.get_edited_scene();
	const int other = current > 0 ? current - 1 : (ed.get_edited_scene_count() > 1 ? 1 : -1);
	Node *root = other >= 0 && other != current ? ed.get_edited_scene_root(other) : nullptr;
	// Only take a scene nothing else holds (it is out of the tree while it
	// isn't the edited one).
	if (root && !root->get_parent()) {
		_take_into_preview(root);
	}
}

void EditorSplitView::set_show_pane_docks(bool p_show) {
	show_pane_docks = p_show;
	_queue_update();
}

void EditorSplitView::edited_scene_changed(Node *p_old_root, Node *p_new_root) {
	if (!enabled) {
		return;
	}
	Node *preview = ObjectDB::get_instance<Node>(preview_scene);
	if (preview && preview == p_new_root) {
		// The previewed scene is being edited now; EditorNode moved it.
		_set_pane_tree_root(nullptr);
		preview_pane->inspector->edit(nullptr);
		preview_pane->viewport->set_editor_preview_scene_root(nullptr);
		preview_scene = ObjectID();
		preview = nullptr;
	}

	// A scene being closed is about to be freed; don't bring it into the tree.
	const bool old_closing = p_old_root && p_old_root->get_instance_id() == closing_scene;
	if (p_old_root && !old_closing && p_old_root != p_new_root && _find_scene_index(p_old_root->get_instance_id()) >= 0) {
		if (preview) {
			_release_preview();
		}
		_take_into_preview(p_old_root);
		if (swap_on_change && !floating && preview_pane->get_parent() == this) {
			// Keep each scene on its side of the split.
			_swap_sides();
		}
	}

	if (pending_select_scene.is_valid() && p_new_root && p_new_root->get_instance_id() == pending_select_scene) {
		// EditorNode restores the scene's saved selection after this, so
		// select once the switch is done.
		callable_mp(this, &EditorSplitView::_apply_pending_select).call_deferred();
	} else {
		pending_select = NodePath();
		pending_select_scene = ObjectID();
	}
	_queue_update();
}

void EditorSplitView::_apply_pending_select() {
	Node *root = EditorNode::get_singleton()->get_edited_scene();
	if (root && root->get_instance_id() == pending_select_scene) {
		Node *node = root->get_node_or_null(pending_select);
		if (node) {
			EditorSelection *selection = EditorNode::get_singleton()->get_editor_selection();
			selection->clear();
			selection->add_node(node);
			EditorNode::get_singleton()->push_node_item(node);
		}
	}
	pending_select = NodePath();
	pending_select_scene = ObjectID();
}

void EditorSplitView::release_scene(Node *p_root) {
	if (p_root && p_root->get_instance_id() == closing_scene) {
		closing_scene = ObjectID();
	}
	if (p_root && p_root->get_instance_id() == preview_scene) {
		_release_preview();
		_queue_update();
	}
}

void EditorSplitView::set_closing_scene(Node *p_root) {
	closing_scene = p_root ? p_root->get_instance_id() : ObjectID();
}

void EditorSplitView::queue_update() {
	_queue_update();
}

void EditorSplitView::show_scene_in_preview(int p_idx) {
	EditorData &ed = EditorNode::get_editor_data();
	ERR_FAIL_INDEX(p_idx, ed.get_edited_scene_count());
	if (p_idx == ed.get_edited_scene()) {
		return;
	}
	Node *root = ed.get_edited_scene_root(p_idx);
	ERR_FAIL_NULL(root);
	if (!enabled) {
		EditorSettings::get_singleton()->set("interface/editor/split_view/enabled", true);
		EditorSettings::get_singleton()->notify_changes();
		enabled = true;
	}
	if (root->get_instance_id() != preview_scene) {
		_release_preview();
		_take_into_preview(root);
	}
	_queue_update();
}

void EditorSplitView::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			EditorNode::get_singleton()->connect("scene_changed", callable_mp(this, &EditorSplitView::_queue_update));
			// Keeps the "(*)" unsaved marks in the pane headers current.
			EditorUndoRedoManager::get_singleton()->connect("history_changed", callable_mp(this, &EditorSplitView::_queue_update));
			EditorUndoRedoManager::get_singleton()->connect("version_changed", callable_mp(this, &EditorSplitView::_queue_update));
			InspectorDock::get_inspector_singleton()->connect("edited_object_changed", callable_mp(this, &EditorSplitView::_inspected_object_changed));
			focus_pane->tree->set_editor_selection(EditorNode::get_singleton()->get_editor_selection());
			SceneTreeDock::get_singleton()->connect(SceneStringName(visibility_changed), callable_mp(this, &EditorSplitView::_queue_update));
			InspectorDock::get_singleton()->connect(SceneStringName(visibility_changed), callable_mp(this, &EditorSplitView::_queue_update));
			set_show_pane_docks(EDITOR_GET("interface/editor/split_view/show_pane_docks"));
			compare_scripts = EDITOR_GET("interface/editor/split_view/compare_scripts");
			ScriptEditor::get_singleton()->connect("editor_script_changed", callable_mp(this, &EditorSplitView::_script_changed));
			set_split_enabled(EDITOR_GET("interface/editor/split_view/enabled"));
			// Whether the editor uses native windows is only settled once
			// startup is done.
			callable_mp(this, &EditorSplitView::_apply_float_setting).call_deferred();
		} break;

		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (EditorSettings::get_singleton()->check_changed_settings_in_group("interface/editor/split_view")) {
				set_show_pane_docks(EDITOR_GET("interface/editor/split_view/show_pane_docks"));
				compare_scripts = EDITOR_GET("interface/editor/split_view/compare_scripts");
				set_split_enabled(EDITOR_GET("interface/editor/split_view/enabled"));
				// Can't move the pane while this notification goes through the tree.
				callable_mp(this, &EditorSplitView::_apply_float_setting).call_deferred();
			}
		} break;

		case NOTIFICATION_PREDELETE: {
			// Open scenes are owned by EditorData; children are freed right
			// after this, so never let the viewport free one with the pane.
			Node *root = ObjectDB::get_instance<Node>(preview_scene);
			if (root && root->get_parent() == preview_pane->viewport) {
				preview_pane->viewport->remove_child(root);
			}
			preview_scene = ObjectID();
		} break;

		case NOTIFICATION_RESIZED: {
			if (preview_pane && preview_pane->is_visible()) {
				callable_mp(this, &EditorSplitView::_update_strips).call_deferred();
				callable_mp(this, &EditorSplitView::_update_preview_camera).call_deferred();
			}
		} break;
	}
}

EditorSplitView::EditorSplitView() {
	singleton = this;
	set_v_size_flags(SIZE_EXPAND_FILL);
	set_h_size_flags(SIZE_EXPAND_FILL);

	focus_pane = memnew(EditorScenePane(true));
	add_child(focus_pane);
	focus_pane->tree->connect("node_selected", callable_mp(this, &EditorSplitView::_focus_tree_selected));

	preview_pane = memnew(EditorScenePane(false));
	add_child(preview_pane);
	preview_pane->tree->connect("node_selected", callable_mp(this, &EditorSplitView::_preview_tree_selected));
	preview_pane->tree->set_scene_root_override(nullptr);
	preview_pane->viewport_container->connect(SceneStringName(gui_input), callable_mp(this, &EditorSplitView::_preview_gui_input));
	preview_pane->viewport_container->connect(SceneStringName(mouse_exited), callable_mp((CanvasItem *)preview_pane->overlay, &CanvasItem::queue_redraw));
	preview_pane->viewport_container->connect(SceneStringName(resized), callable_mp(this, &EditorSplitView::_update_preview_camera));
	preview_pane->live_container->connect(SceneStringName(gui_input), callable_mp(this, &EditorSplitView::_live_gui_input));
	preview_pane->live_container->connect(SceneStringName(resized), callable_mp(this, &EditorSplitView::_update_live_camera));
	preview_pane->overlay->connect(SceneStringName(draw), callable_mp(this, &EditorSplitView::_draw_preview_overlay));
	preview_pane->close_button->connect(SceneStringName(pressed), callable_mp(this, &EditorSplitView::_close_preview));
	preview_pane->float_button->connect(SceneStringName(toggled), callable_mp(this, &EditorSplitView::_float_toggled));
	preview_pane->swap_button->connect(SceneStringName(pressed), callable_mp(this, &EditorSplitView::_swap_sides));
	preview_pane->code_button->connect(SceneStringName(toggled), callable_mp(this, &EditorSplitView::_code_toggled));
	preview_pane->pick_button->connect("about_to_popup", callable_mp(this, &EditorSplitView::_fill_pick_menu));
	preview_pane->pick_button->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &EditorSplitView::_pick_menu_id_pressed));
	preview_pane->code_view->connect(SceneStringName(gui_input), callable_mp(this, &EditorSplitView::_code_gui_input));
	preview_pane->connect(SceneStringName(resized), callable_mp(this, &EditorSplitView::_update_strips), CONNECT_DEFERRED);
	preview_pane->hide();

	float_window = memnew(Window);
	float_window->set_visible(false);
	float_window->set_title(TTR("Split View"));
	float_window->set_wrap_controls(true);
	float_window->set_transient(true);
	float_window->set_propagate_shortcuts_to_parent(true);
	float_window->connect("close_requested", callable_mp(this, &EditorSplitView::_float_window_close_requested));
	Panel *float_background = memnew(Panel);
	float_background->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
	float_window->add_child(float_background);
	add_child(float_window);
}

EditorSplitView::~EditorSplitView() {
	singleton = nullptr;
}
