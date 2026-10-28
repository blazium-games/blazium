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
#include "core/input/input.h"
#include "core/input/input_event.h"
#include "core/io/resource_loader.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/object/editor_language.h"
#include "core/object/script_language.h"
#include "core/string/char_utils.h"
#include "core/templates/pair.h"
#include "editor/docks/groups_dock.h"
#include "editor/docks/inspector_dock.h"
#include "editor/docks/scene_tree_dock.h"
#include "editor/docks/signals_dock.h"
#include "editor/editor_data.h"
#include "editor/editor_interface.h"
#include "editor/editor_main_screen.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/gui/code_editor.h"
#include "editor/gui/create_dialog.h"
#include "editor/gui/editor_quick_open_dialog.h"
#include "editor/inspector/editor_inspector.h"
#include "editor/scene/3d/node_3d_editor_plugin.h"
#include "editor/scene/canvas_item_editor_plugin.h"
#include "editor/scene/editor_scene_tabs.h"
#include "editor/scene/editor_split_view_tools.h"
#include "editor/scene/scene_tree_editor.h"
#include "editor/script/script_create_dialog.h"
#include "editor/script/script_editor_base.h"
#include "editor/script/script_editor_plugin.h"
#include "editor/script/syntax_highlighters.h"
#include "editor/settings/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/decal.h"
#include "scene/3d/light_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/3d/visual_instance_3d.h"
#include "scene/3d/world_environment.h"
#include "scene/gui/button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/code_edit.h"
#include "scene/gui/flow_container.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/panel.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/popup.h"
#include "scene/gui/popup_menu.h"
#include "scene/gui/separator.h"
#include "scene/gui/subviewport_container.h"
#include "scene/gui/tree.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "scene/resources/3d/sky_material.h"
#include "scene/resources/environment.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/sky.h"
#include "scene/resources/style_box_flat.h"
#include "servers/display/display_server.h"
#include "servers/rendering/rendering_server.h"

// The look of the 3D editor's viewport menu and panels, drawn over the view.
// A render layer the 3D editor's cameras don't draw (they use 0-19 and
// 24-30), for the second pane's grid.
static constexpr int SPLIT_VIEW_GRID_LAYER = 23;

static void _split_view_dark_background_style(Control *p_control) {
	const Ref<StyleBox> stylebox = EditorNode::get_singleton()->get_gui_base()->get_theme_stylebox(SNAME("Information3dViewport"), EditorStringName(EditorStyles));
	const Ref<Theme> &editor_theme = EditorNode::get_singleton()->get_editor_theme();
	p_control->begin_bulk_theme_override();
	static const char *styles[] = { "normal", "normal_mirrored", "hover", "hover_mirrored", "hover_pressed", "hover_pressed_mirrored", "pressed", "pressed_mirrored", "focus", "focus_mirrored", "disabled", "disabled_mirrored", "panel" };
	for (const char *style : styles) {
		p_control->add_theme_style_override(style, stylebox);
	}
	p_control->add_theme_color_override(SceneStringName(font_color), editor_theme->get_color(SNAME("font_dark_background_color"), EditorStringName(Editor)));
	p_control->add_theme_color_override("font_hover_color", editor_theme->get_color(SNAME("font_dark_background_hover_color"), EditorStringName(Editor)));
	p_control->add_theme_color_override("font_focus_color", editor_theme->get_color(SNAME("font_dark_background_focus_color"), EditorStringName(Editor)));
	p_control->add_theme_color_override("font_pressed_color", editor_theme->get_color(SNAME("font_dark_background_pressed_color"), EditorStringName(Editor)));
	p_control->add_theme_color_override("font_hover_pressed_color", editor_theme->get_color(SNAME("font_dark_background_hover_pressed_color"), EditorStringName(Editor)));
	p_control->end_bulk_theme_override();
}

// EditorScenePane

void EditorScenePane::_draw_dot() {
	const Color color = active ? get_theme_color(SNAME("accent_color"), EditorStringName(Editor)) : get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor));
	const Size2 size = dot->get_size();
	dot->draw_circle(size / 2, MIN(size.x, size.y) / 2, color);
}

void EditorScenePane::_queue_fit_tree_strip() {
	if (tree_fit_queued) {
		return;
	}
	tree_fit_queued = true;
	callable_mp(this, &EditorScenePane::_fit_tree_strip).call_deferred();
}

void EditorScenePane::_fit_tree_strip() {
	tree_fit_queued = false;
	const Tree *scene_tree = tree->get_scene_tree();
	// The widest row, its panel and room for the scroll bar.
	real_t needed = scene_tree->get_column_minimum_width(0) + 16 * EDSCALE;
	const Ref<StyleBox> panel = scene_tree->get_theme_stylebox(SceneStringName(panel));
	if (panel.is_valid()) {
		needed += panel->get_minimum_size().width;
	}
	// Never narrower than before, and never more than about half the pane,
	// so the view keeps its room; past that, the tree scrolls sideways.
	const real_t narrowest = (focused ? 160 : 200) * EDSCALE;
	const real_t widest = MAX(narrowest, Math::round(get_size().width * 0.45));
	const real_t width = Math::ceil(CLAMP(needed, narrowest, widest));
	if (!Math::is_equal_approx(width, tree_strip->get_custom_minimum_size().width)) {
		tree_strip->set_custom_minimum_size(Size2(width, 0));
	}
}

void EditorScenePane::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_RESIZED: {
			_queue_fit_tree_strip(); // How wide the strip may get follows the pane.
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			_update_style();
			const Color dim = get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor));
			type_label->add_theme_color_override(SceneStringName(font_color), dim);
			tree_title->add_theme_color_override(SceneStringName(font_color), dim);
			inspector_title->add_theme_color_override(SceneStringName(font_color), dim);
			file_label->add_theme_font_override(SceneStringName(font), get_theme_font(SNAME("bold"), EditorStringName(EditorFonts)));
			if (pick_button) {
				pick_button->add_theme_font_override(SceneStringName(font), get_theme_font(SNAME("bold"), EditorStringName(EditorFonts)));
				pick_button->set_button_icon(get_theme_icon(SNAME("arrow"), SNAME("OptionButton")));
			}
			if (code_toolbar) {
				code_back_button->set_button_icon(get_editor_theme_icon(is_layout_rtl() ? SNAME("Forward") : SNAME("Back")));
				code_forward_button->set_button_icon(get_editor_theme_icon(is_layout_rtl() ? SNAME("Back") : SNAME("Forward")));
				code_both_button->set_button_icon(get_editor_theme_icon(SNAME("Instance")));
				code_function_menu->set_button_icon(get_editor_theme_icon(SNAME("MemberMethod")));
				code_find_prev->set_button_icon(get_editor_theme_icon(SNAME("MoveUp")));
				code_find_next->set_button_icon(get_editor_theme_icon(SNAME("MoveDown")));
				code_find_close->set_button_icon(get_editor_theme_icon(SNAME("Close")));
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
			if (tree_toolbar) {
				add_node_button->set_button_icon(get_editor_theme_icon(SNAME("Add")));
				instantiate_button->set_button_icon(get_editor_theme_icon(SNAME("Instance")));
				tree_filter->set_right_icon(get_editor_theme_icon(SNAME("Search")));
				attach_script_button->set_button_icon(get_editor_theme_icon(SNAME("ScriptCreate")));
				detach_script_button->set_button_icon(get_editor_theme_icon(SNAME("ScriptRemove")));
				tree_menu->set_button_icon(get_editor_theme_icon(SNAME("GuiTabMenuHl")));
			}
			if (view_menu) {
				view_menu->set_button_icon(get_editor_theme_icon(SNAME("GuiTabMenuHlDarkBackground")));
				_split_view_dark_background_style(view_menu);
				_split_view_dark_background_style(info_panel);
				_split_view_dark_background_style(info_label);
				_split_view_dark_background_style(frame_time_panel);
			}
			dot->queue_redraw();
		} break;
	}
}

void EditorScenePane::_update_style() {
	Ref<StyleBoxFlat> header_style;
	header_style.instantiate();
	header_style->set_bg_color(get_theme_color(active ? SNAME("dark_color_1") : SNAME("dark_color_2"), EditorStringName(Editor)));
	header_style->set_content_margin_individual(8 * EDSCALE, 4 * EDSCALE, 4 * EDSCALE, 4 * EDSCALE);
	if (active) {
		// The side being edited gets the accent edge of a selected tab.
		const int edge = MAX(1, int(Math::round(2 * EDSCALE)));
		header_style->set_border_width(SIDE_TOP, edge);
		header_style->set_border_color(get_theme_color(SNAME("accent_color"), EditorStringName(Editor)));
		header_style->set_content_margin(SIDE_TOP, 4 * EDSCALE - edge);
	}
	header->add_theme_style_override(SceneStringName(panel), header_style);
	state_label->add_theme_color_override(SceneStringName(font_color), active ? get_theme_color(SNAME("accent_color"), EditorStringName(Editor)) : get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
	dot->queue_redraw();
}

void EditorScenePane::set_active(bool p_active, bool p_editable) {
	if (active == p_active && editable == p_editable && !state_label->get_text().is_empty()) {
		return;
	}
	active = p_active;
	editable = p_editable;
	if (active) {
		state_label->set_text(TTRC("Editing"));
		state_label->set_tooltip_text(TTRC("Undo, redo and shortcuts go to this side."));
	} else if (editable) {
		state_label->set_text(TTRC("Editable"));
		state_label->set_tooltip_text(TTRC("Click this side to edit it."));
	} else {
		state_label->set_text(TTRC("View only"));
		state_label->set_tooltip_text(TTRC("This side is for looking. Clicking it doesn't switch what you're editing."));
	}
	if (is_inside_tree()) {
		_update_style();
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

	state_label = memnew(Label);
	state_label->set_mouse_filter(MOUSE_FILTER_PASS);
	header_hb->add_child(state_label);

	if (!focused) {
		code_button = memnew(Button);
		code_button->set_flat(true);
		code_button->set_toggle_mode(true);
		code_button->set_tooltip_text(TTRC("Show another open script here instead of the scene. The script you switch away from moves here."));
		code_button->set_accessibility_name(TTRC("Compare Scripts"));
		code_button->hide();
		header_hb->add_child(code_button);

		swap_button = memnew(Button);
		swap_button->set_flat(true);
		swap_button->set_tooltip_text(TTRC("Edit the other side: the two scenes (or scripts) trade places, and the panes stay where they are."));
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
	tree_strip->set_custom_minimum_size(Size2(focused ? 160 : 200, 0) * EDSCALE);
	body->add_child(tree_strip);
	tree_title = memnew(Label);
	tree_title->set_text(TTRC("Scene"));
	tree_title->set_theme_type_variation("HeaderSmall");
	tree_strip->add_child(tree_title);
	if (!focused) {
		// The Scene dock's toolbar, for this pane's scene.
		tree_title->hide();
		tree_toolbar = memnew(HBoxContainer);
		tree_toolbar->add_theme_constant_override("separation", 0);
		tree_strip->add_child(tree_toolbar);

		add_node_button = memnew(Button);
		add_node_button->set_theme_type_variation("FlatMenuButton");
		add_node_button->set_tooltip_text(TTRC("Add/Create a New Node."));
		add_node_button->set_accessibility_name(TTRC("Add Child Node"));
		tree_toolbar->add_child(add_node_button);

		instantiate_button = memnew(Button);
		instantiate_button->set_theme_type_variation("FlatMenuButton");
		instantiate_button->set_tooltip_text(TTRC("Instantiate a scene file as a Node."));
		instantiate_button->set_accessibility_name(TTRC("Instantiate Child Scene"));
		tree_toolbar->add_child(instantiate_button);

		tree_filter = memnew(LineEdit);
		tree_filter->set_h_size_flags(SIZE_EXPAND_FILL);
		tree_filter->set_placeholder(TTRC("Filter Nodes"));
		tree_filter->set_accessibility_name(TTRC("Filter Nodes"));
		tree_filter->set_clear_button_enabled(true);
		tree_filter->add_theme_constant_override("minimum_character_width", 0);
		tree_filter->set_tooltip_text(TTRC("Filter nodes by entering a part of their name, type (if prefixed with \"type:\" or \"t:\")\nor group (if prefixed with \"group:\" or \"g:\"). Filtering is case-insensitive."));
		tree_toolbar->add_child(tree_filter);

		attach_script_button = memnew(Button);
		attach_script_button->set_theme_type_variation("FlatMenuButton");
		attach_script_button->set_tooltip_text(TTRC("Attach a new or existing script to the selected node."));
		attach_script_button->set_accessibility_name(TTRC("Attach Script"));
		tree_toolbar->add_child(attach_script_button);

		detach_script_button = memnew(Button);
		detach_script_button->set_theme_type_variation("FlatMenuButton");
		detach_script_button->set_tooltip_text(TTRC("Detach the script from the selected node."));
		detach_script_button->set_accessibility_name(TTRC("Detach Script"));
		detach_script_button->hide();
		tree_toolbar->add_child(detach_script_button);

		tree_menu = memnew(MenuButton);
		tree_menu->set_flat(false);
		tree_menu->set_theme_type_variation("FlatMenuButton");
		tree_menu->set_tooltip_text(TTRC("Extra scene options."));
		tree_toolbar->add_child(tree_menu);
		subresource_menu = memnew(PopupMenu);
		subresource_menu->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
		// Owned by the menu from the start, even if it's never opened.
		tree_menu->get_popup()->add_child(subresource_menu);
	}
	tree = memnew(SceneTreeEditor(false, false, false));
	// The Scene dock's script, visibility and instance buttons.
	tree->set_show_node_buttons(!focused);
	tree->set_v_size_flags(SIZE_EXPAND_FILL);
	tree->set_update_when_invisible(false);
	tree_strip->add_child(tree);
	// Measure the widest row, so the strip can fit names instead of cutting
	// them off; it is redrawn whenever its rows change.
	tree->get_scene_tree()->set_column_clip_content(0, false);
	tree->get_scene_tree()->connect(SceneStringName(draw), callable_mp(this, &EditorScenePane::_queue_fit_tree_strip));

	content = memnew(VBoxContainer);
	content->set_h_size_flags(SIZE_EXPAND_FILL);
	content->set_v_size_flags(SIZE_EXPAND_FILL);
	content->add_theme_constant_override("separation", 0);
	body->add_child(content);

	if (!focused) {
		tools = memnew(EditorPaneTools);
		content->add_child(tools);

		// The 2D editor's View menu, shown for 2D scenes.
		HBoxContainer *view_2d_box = memnew(HBoxContainer);
		view_2d_box->hide();
		tools->add_child(view_2d_box);
		view_2d_box->add_child(memnew(VSeparator));
		view_menu_2d = memnew(MenuButton);
		view_menu_2d->set_flat(false);
		view_menu_2d->set_theme_type_variation("FlatMenuButton");
		// TRANSLATORS: Noun, name of the 2D/3D View menus.
		view_menu_2d->set_text(TTRC("View"));
		view_menu_2d->set_switch_on_hover(true);
		// Shown in the menu, but taken only by the pane (see _preview_gui_input()).
		view_menu_2d->set_disable_shortcuts(true);
		view_menu_2d->get_popup()->set_hide_on_checkable_item_selection(false);
		view_2d_box->add_child(view_menu_2d);
		view_grid_menu_2d = memnew(PopupMenu);
		view_menu_2d->get_popup()->add_child(view_grid_menu_2d);
		view_gizmos_menu_2d = memnew(PopupMenu);
		view_gizmos_menu_2d->set_hide_on_checkable_item_selection(false);
		view_menu_2d->get_popup()->add_child(view_gizmos_menu_2d);

		viewport_container = memnew(SubViewportContainer);
		viewport_container->set_stretch(true);
		viewport_container->set_v_size_flags(SIZE_EXPAND_FILL);
		viewport_container->set_h_size_flags(SIZE_EXPAND_FILL);
		viewport_container->set_mouse_filter(MOUSE_FILTER_STOP);
		// For the tools' shortcuts.
		viewport_container->set_focus_mode(FOCUS_CLICK);
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
		// Handles of things near the camera reach far past the pane.
		overlay->set_clip_contents(true);
		overlay->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
		viewport_container->add_child(overlay);

		// For 3D scenes: the 3D editor's viewport menu and panels. The menu's
		// items are added by EditorSplitView, once the 3D editor's shortcuts exist.
		view_menu = memnew(MenuButton);
		view_menu->set_flat(false);
		view_menu->set_position(Point2(8, 8) * EDSCALE);
		view_menu->set_accessibility_name(TTRC("View"));
		view_menu->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
		view_menu->get_popup()->set_auto_translate_mode(AUTO_TRANSLATE_MODE_ALWAYS);
		view_menu->get_popup()->set_hide_on_checkable_item_selection(false);
		// The pane handles the shortcuts, while it has focus.
		view_menu->set_disable_shortcuts(true);
		view_menu->hide();
		viewport_container->add_child(view_menu);
		view_advanced_menu = memnew(PopupMenu);
		view_advanced_menu->set_hide_on_checkable_item_selection(false);
		// Owned by the menu from the start, even if its items are never added.
		view_menu->get_popup()->add_child(view_advanced_menu);

		info_panel = memnew(PanelContainer);
		info_panel->set_mouse_filter(MOUSE_FILTER_IGNORE);
		info_panel->set_h_grow_direction(GROW_DIRECTION_BEGIN);
		info_panel->set_v_grow_direction(GROW_DIRECTION_BEGIN);
		info_panel->set_anchors_and_offsets_preset(PRESET_BOTTOM_RIGHT, PRESET_MODE_MINSIZE, 8 * EDSCALE);
		info_panel->hide();
		viewport_container->add_child(info_panel);
		info_label = memnew(Label);
		info_label->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
		info_panel->add_child(info_label);

		rotation_control = memnew(EditorPaneRotationControl);
		rotation_control->set_h_grow_direction(GROW_DIRECTION_BEGIN);
		rotation_control->set_anchors_and_offsets_preset(PRESET_TOP_RIGHT, PRESET_MODE_MINSIZE, 8 * EDSCALE);
		rotation_control->hide();
		viewport_container->add_child(rotation_control);

		frame_time_panel = memnew(PanelContainer);
		frame_time_panel->set_mouse_filter(MOUSE_FILTER_IGNORE);
		frame_time_panel->set_h_grow_direction(GROW_DIRECTION_BEGIN);
		frame_time_panel->set_anchors_and_offsets_preset(PRESET_TOP_RIGHT, PRESET_MODE_MINSIZE, 8 * EDSCALE);
		// Below the axis gizmo, as in the 3D editor.
		frame_time_panel->set_offset(SIDE_TOP, (8 + 80 + 8) * EDSCALE);
		frame_time_panel->set_offset(SIDE_BOTTOM, (8 + 80 + 8) * EDSCALE);
		frame_time_panel->hide();
		viewport_container->add_child(frame_time_panel);
		VBoxContainer *frame_time_vbox = memnew(VBoxContainer);
		frame_time_panel->add_child(frame_time_vbox);
		cpu_time_label = memnew(Label);
		frame_time_vbox->add_child(cpu_time_label);
		gpu_time_label = memnew(Label);
		frame_time_vbox->add_child(gpu_time_label);
		fps_label = memnew(Label);
		frame_time_vbox->add_child(fps_label);

		camera_3d = memnew(Camera3D);
		// The scene's layers and the pane's grid, never the 3D editor's own
		// gizmos and grid, which share the editor's world with it.
		camera_3d->set_cull_mask(((1 << 20) - 1) | (1 << SPLIT_VIEW_GRID_LAYER));
		viewport->add_child(camera_3d);
		tools->setup(viewport, camera_3d, overlay);
		rotation_control->set_camera(camera_3d);

		code_container = memnew(VBoxContainer);
		code_container->set_v_size_flags(SIZE_EXPAND_FILL);
		code_container->set_h_size_flags(SIZE_EXPAND_FILL);
		code_container->hide();
		content->add_child(code_container);

		// The script editor's Search and Go To menus, read-only, and its
		// back and forward arrows for the scripts shown here.
		code_toolbar = memnew(HBoxContainer);
		code_toolbar->add_theme_constant_override("separation", 0);
		code_container->add_child(code_toolbar);

		code_search_menu = memnew(MenuButton);
		code_search_menu->set_flat(false);
		code_search_menu->set_theme_type_variation("FlatMenuButton");
		code_search_menu->set_text(TTRC("Search"));
		code_search_menu->set_switch_on_hover(true);
		// Shown in the menu, but taken only over this pane (see input()).
		code_search_menu->set_disable_shortcuts(true);
		code_toolbar->add_child(code_search_menu);

		code_goto_menu = memnew(MenuButton);
		code_goto_menu->set_flat(false);
		code_goto_menu->set_theme_type_variation("FlatMenuButton");
		code_goto_menu->set_text(TTRC("Go To"));
		code_goto_menu->set_switch_on_hover(true);
		code_goto_menu->set_disable_shortcuts(true);
		code_toolbar->add_child(code_goto_menu);

		code_function_menu = memnew(MenuButton);
		code_function_menu->set_flat(false);
		code_function_menu->set_theme_type_variation("FlatMenuButton");
		code_function_menu->set_text(TTRC("Function"));
		code_function_menu->set_tooltip_text(TTRC("Jump to a function in this script."));
		code_function_menu->set_disable_shortcuts(true);
		code_function_menu->get_popup()->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
		code_toolbar->add_child(code_function_menu);

		code_toolbar->add_child(memnew(VSeparator));

		code_back_button = memnew(Button);
		code_back_button->set_theme_type_variation(SceneStringName(FlatButton));
		code_back_button->set_tooltip_text(TTRC("Show the previous script shown here."));
		code_back_button->set_accessibility_name(TTRC("Previous Script"));
		code_back_button->set_disabled(true);
		code_toolbar->add_child(code_back_button);

		code_forward_button = memnew(Button);
		code_forward_button->set_theme_type_variation(SceneStringName(FlatButton));
		code_forward_button->set_tooltip_text(TTRC("Show the next script shown here."));
		code_forward_button->set_accessibility_name(TTRC("Next Script"));
		code_forward_button->set_disabled(true);
		code_toolbar->add_child(code_forward_button);

		Control *code_toolbar_spacer = memnew(Control);
		code_toolbar_spacer->set_h_size_flags(SIZE_EXPAND_FILL);
		code_toolbar->add_child(code_toolbar_spacer);

		code_both_button = memnew(Button);
		code_both_button->set_theme_type_variation(SceneStringName(FlatButton));
		code_both_button->set_toggle_mode(true);
		code_both_button->set_text(TTRC("Both Scripts"));
		code_both_button->set_tooltip_text(TTRC("Search and Go to Function also act on the script being edited, to line the two up."));
		code_toolbar->add_child(code_both_button);

		code_view = memnew(CodeEdit);
		code_view->set_v_size_flags(SIZE_EXPAND_FILL);
		code_view->set_editable(false);
		code_view->set_draw_line_numbers(true);
		// Shows where Go To and Find landed.
		code_view->set_highlight_current_line(true);
		code_view->set_context_menu_enabled(false);
		code_container->add_child(code_view);

		code_placeholder = memnew(Label);
		code_placeholder->set_v_size_flags(SIZE_EXPAND_FILL);
		code_placeholder->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
		code_placeholder->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
		code_placeholder->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
		code_placeholder->set_text(TTRC("Open another script to compare it here.\nThe script you switch away from appears here."));
		code_container->add_child(code_placeholder);

		// Find, as the script editor's find bar, without replacing. It wraps
		// onto a second row rather than widen a narrow pane.
		code_find_bar = memnew(HFlowContainer);
		code_find_bar->hide();
		code_container->add_child(code_find_bar);
		code_find_text = memnew(LineEdit);
		code_find_text->set_h_size_flags(SIZE_EXPAND_FILL);
		code_find_text->set_custom_minimum_size(Size2(120, 0) * EDSCALE);
		code_find_text->set_placeholder(TTRC("Find"));
		code_find_text->set_tooltip_text(TTRC("Find"));
		code_find_text->set_accessibility_name(TTRC("Find"));
		code_find_text->set_keep_editing_on_text_submit(true);
		code_find_bar->add_child(code_find_text);
		code_find_matches = memnew(Label);
		code_find_matches->set_focus_mode(FOCUS_ACCESSIBILITY);
		code_find_bar->add_child(code_find_matches);
		code_find_prev = memnew(Button);
		code_find_prev->set_flat(true);
		code_find_prev->set_tooltip_text(TTRC("Previous Match"));
		code_find_prev->set_accessibility_name(TTRC("Previous Match"));
		code_find_prev->set_focus_mode(FOCUS_ACCESSIBILITY);
		code_find_bar->add_child(code_find_prev);
		code_find_next = memnew(Button);
		code_find_next->set_flat(true);
		code_find_next->set_tooltip_text(TTRC("Next Match"));
		code_find_next->set_accessibility_name(TTRC("Next Match"));
		code_find_next->set_focus_mode(FOCUS_ACCESSIBILITY);
		code_find_bar->add_child(code_find_next);
		code_find_case = memnew(CheckBox);
		code_find_case->set_text(TTRC("Match Case"));
		code_find_case->set_focus_mode(FOCUS_ACCESSIBILITY);
		code_find_bar->add_child(code_find_case);
		code_find_words = memnew(CheckBox);
		code_find_words->set_text(TTRC("Whole Words"));
		code_find_words->set_focus_mode(FOCUS_ACCESSIBILITY);
		code_find_bar->add_child(code_find_words);
		code_find_close = memnew(Button);
		code_find_close->set_flat(true);
		code_find_close->set_tooltip_text(TTRC("Hide"));
		code_find_close->set_accessibility_name(TTRC("Hide"));
		code_find_close->set_focus_mode(FOCUS_ACCESSIBILITY);
		code_find_bar->add_child(code_find_close);

		code_line_popup = memnew(PopupPanel);
		code_line_popup->set_title(TTRC("Go to Line"));
		add_child(code_line_popup);
		HBoxContainer *line_hb = memnew(HBoxContainer);
		code_line_popup->add_child(line_hb);
		Label *line_label = memnew(Label);
		line_label->set_text(TTRC("Line Number:"));
		line_hb->add_child(line_label);
		code_line_text = memnew(LineEdit);
		code_line_text->set_custom_minimum_size(Size2(100, 0) * EDSCALE);
		code_line_text->set_select_all_on_focus(true);
		code_line_text->set_accessibility_name(TTRC("Line Number:"));
		line_hb->add_child(code_line_text);
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
	inspector->set_property_name_style(EditorPropertyNameProcessor::get_default_inspector_style());
	inspector_strip->add_child(inspector);

	set_active(focused);
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
	if (pane_live) {
		// Shown once the pane stops editing the edited scene.
		preview_scene = p_root->get_instance_id();
		preview_view_custom = false;
		preview_fit_root = ObjectID(); // Fit again.
		return;
	}
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
	preview_view_custom = false;
	preview_fit_root = ObjectID(); // Fit again.

	_setup_preview_lighting(p_root);
	_set_pane_tree_root(p_root);
	preview_pane->tree->update_tree();
	preview_pane->tools->set_scene(p_root);

	// What was selected while it was edited stays selected.
	Node *selected = ObjectDB::get_instance<Node>(last_focus_selected);
	if (selected && (selected == p_root || p_root->is_ancestor_of(selected))) {
		preview_pane->tools->set_selected(selected, false);
	}
	_pane_selection_changed();
	_update_preview_camera();
}

void EditorSplitView::_release_preview() {
	Node *root = ObjectDB::get_instance<Node>(preview_scene);
	if (!pane_live) {
		preview_pane->tools->set_scene(nullptr);
		preview_pane->inspector->edit(nullptr);
		_set_pane_tree_root(nullptr);
	}
	if (preview_pane->fallback_lighting) {
		memdelete(preview_pane->fallback_lighting);
		preview_pane->fallback_lighting = nullptr;
	}
	if (root && root->get_parent() == preview_pane->viewport) {
		preview_pane->viewport->remove_child(root);
		preview_pane->viewport->set_editor_preview_scene_root(nullptr);
	}
	preview_scene = ObjectID();
}

Node *EditorSplitView::_pane_scene() const {
	// The scene the pane shows and edits.
	return pane_live ? EditorNode::get_singleton()->get_edited_scene() : get_preview_scene();
}

void EditorSplitView::_set_pane_live(bool p_live) {
	SubViewport *viewport = preview_pane->viewport;
	if (p_live) {
		if (!pane_live) {
			// The other scene waits out of the tree, as open scenes do.
			Node *root = ObjectDB::get_instance<Node>(preview_scene);
			if (root && root->get_parent() == viewport) {
				viewport->remove_child(root);
			}
			viewport->set_editor_preview_scene_root(nullptr);
			if (preview_pane->fallback_lighting && preview_pane->fallback_lighting->get_parent() == viewport) {
				// The 3D editor's preview sun and environment light it now.
				// Only taken out: freeing its sky just before the world goes
				// leaks the sky's radiance maps.
				viewport->remove_child(preview_pane->fallback_lighting);
			}
			if (preview_world_2d.is_null()) {
				preview_world_2d = viewport->get_world_2d();
			}
			// Without a world of its own the viewport uses the editor's, as
			// the 3D editor's viewports do. Let go of the old world's scenario
			// first: switching frees that world before the viewport moves.
			RenderingServer::get_singleton()->viewport_set_scenario(viewport->get_viewport_rid(), RID());
			viewport->set_use_own_world_3d(false);
			viewport->set_world_2d(EditorNode::get_singleton()->get_scene_root()->get_world_2d());
			pane_live = true;
		}
		Node *edited = EditorNode::get_singleton()->get_edited_scene();
		const ObjectID id = edited ? edited->get_instance_id() : ObjectID();
		if (id != pane_live_root) {
			pane_live_root = id;
			preview_view_custom = false;
			preview_fit_root = ObjectID(); // Fit again.
			preview_pane->tools->set_scene(edited);
			_set_pane_tree_root(edited);
			_sync_pane_live_selection();
		}
		return;
	}
	if (!pane_live) {
		return;
	}
	pane_live = false;
	pane_live_root = ObjectID();
	preview_pane->tools->set_scene(nullptr);
	_set_pane_tree_root(nullptr);
	viewport->set_use_own_world_3d(true);
	viewport->set_world_2d(preview_world_2d);
	preview_view_custom = false;
	preview_fit_root = ObjectID(); // Fit again.
	Node *root = get_preview_scene();
	if (root && !root->get_parent()) {
		_take_into_preview(root);
	}
}

void EditorSplitView::_sync_pane_live_selection() {
	// Editing the edited scene, the pane shares the editor's selection.
	Node *node = nullptr;
	for (Node *selected : EditorNode::get_singleton()->get_editor_selection()->get_top_selected_node_list()) {
		node = selected;
		break;
	}
	if (preview_pane->tools->get_selected() != node) {
		preview_pane->tools->set_selected(node, false);
	}
	syncing_live_selection = true;
	_pane_selection_changed();
	syncing_live_selection = false;
}

void EditorSplitView::_collect_bounds(Node *p_node, AABB &r_bounds, bool &r_has_bounds) {
	const GeometryInstance3D *vi = Object::cast_to<GeometryInstance3D>(p_node);
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
	Node *root = _pane_scene();
	if (!root) {
		return;
	}
	if (preview_view_custom) {
		// Looked around in: keep that view.
		if (Object::cast_to<Node3D>(root)) {
			_apply_orbit(preview_orbit, preview_pane->camera_3d, preview_pane->viewport);
			_update_preview_grid();
		} else {
			preview_pane->viewport->set_global_canvas_transform(preview_view_2d);
		}
		preview_pane->overlay->queue_redraw();
		preview_pane->rotation_control->queue_redraw();
		return;
	}
	if (Object::cast_to<Node3D>(root)) {
		_apply_orbit(_preview_orbit_from_state(root), preview_pane->camera_3d, preview_pane->viewport);
		_update_preview_grid();
	} else {
		// Fitted once it has a size; after that a resize, such as a bottom
		// panel opening for the node just selected, leaves the view where it
		// is, as in the 2D editor, so nothing moves under the cursor.
		const Size2 size = preview_pane->viewport_container->get_size();
		if (root->get_instance_id() != preview_fit_root || preview_fit_size.x <= 0 || preview_fit_size.y <= 0) {
			preview_pane->viewport->set_global_canvas_transform(_fit_project_window(size));
			preview_fit_root = root->get_instance_id();
			preview_fit_size = size;
		}
	}
	// The scene's own view: not a side view.
	preview_view_type = PANE_VIEW_TYPE_USER;
	preview_auto_orthogonal = false;
	preview_pane->overlay->queue_redraw();
	preview_pane->rotation_control->queue_redraw();
}

void EditorSplitView::_begin_custom_view() {
	if (preview_view_custom) {
		return;
	}
	Node *root = _pane_scene();
	if (!root) {
		return;
	}
	// Start from what the pane shows now.
	preview_orbit = _preview_orbit_from_state(root);
	preview_view_2d = preview_pane->viewport->get_global_canvas_transform();
	preview_view_custom = true;
}

EditorSplitView::OrbitView EditorSplitView::_orbit_from_state(Node *p_root, const Dictionary &p_state_3d) {
	OrbitView orbit;
	if (!p_state_3d.has("viewports")) {
		// Never opened in 3D: frame everything visible in the scene.
		AABB bounds;
		bool has_bounds = false;
		_collect_bounds(p_root, bounds, has_bounds);
		if (has_bounds) {
			orbit.target = bounds.get_center();
			orbit.distance = MAX(bounds.get_longest_axis_size(), (real_t)1.0) * 1.1;
			orbit.x_rot = 0.45;
			orbit.y_rot = -0.6;
		}
		return orbit;
	}
	// Where its 3D editor camera was.
	const Array viewports = p_state_3d["viewports"];
	if (!viewports.is_empty()) {
		const Dictionary vp = viewports[0];
		orbit.target = vp.get("position", orbit.target);
		orbit.x_rot = vp.get("x_rotation", orbit.x_rot);
		orbit.y_rot = vp.get("y_rotation", orbit.y_rot);
		orbit.distance = vp.get("distance", orbit.distance);
		orbit.orthogonal = vp.get("orthogonal", false);
	}
	orbit.fov = p_state_3d.get("fov", orbit.fov);
	return orbit;
}

EditorSplitView::OrbitView EditorSplitView::_preview_orbit_from_state(Node *p_root) const {
	// Where the scene's 3D editor camera was, but always in perspective:
	// view 2 only goes orthogonal from its view menu.
	Dictionary state_3d;
	if (pane_live) {
		// Where the 3D editor's camera is now.
		state_3d = Node3DEditor::get_singleton()->get_state();
	} else {
		const int idx = _find_scene_index(preview_scene);
		state_3d = EditorNode::get_editor_data().get_scene_editor_states(idx).get("3D", Dictionary());
	}
	OrbitView orbit = _orbit_from_state(p_root, state_3d);
	orbit.orthogonal = false;
	return orbit;
}

void EditorSplitView::_apply_orbit(const OrbitView &p_orbit, Camera3D *p_camera, SubViewport *p_viewport) {
	Transform3D xform;
	xform.translate_local(p_orbit.target);
	xform.basis.rotate(Vector3(1, 0, 0), -p_orbit.x_rot);
	xform.basis.rotate(Vector3(0, 1, 0), -p_orbit.y_rot);
	xform.translate_local(0, 0, p_orbit.orthogonal ? 50.0 : p_orbit.distance);
	p_camera->set_global_transform(xform);
	if (p_orbit.orthogonal) {
		p_camera->set_orthogonal(2 * p_orbit.distance, 0.05, 4000);
	} else {
		p_camera->set_perspective(p_orbit.fov, 0.05, 4000);
	}
	p_camera->make_current();
	p_viewport->set_global_canvas_transform(Transform2D());
}

Transform2D EditorSplitView::_fit_project_window(const Size2 &p_size) {
	// Fit the project's window into the pane, like "Center View" in 2D.
	const Size2 project_size = Size2(GLOBAL_GET("display/window/size/viewport_width"), GLOBAL_GET("display/window/size/viewport_height"));
	Transform2D xform;
	if (project_size.x <= 0 || project_size.y <= 0 || p_size.x <= 0 || p_size.y <= 0) {
		return xform;
	}
	const real_t scale = MIN(p_size.x / project_size.x, p_size.y / project_size.y) * 0.9;
	xform.scale_basis(Size2(scale, scale));
	xform.columns[2] = (p_size - project_size * scale) / 2;
	return xform;
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
	code_static_script.unref();

	_style_code_view(p_source);
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

void EditorSplitView::_style_code_view(CodeEdit *p_style_from) {
	// Look like the script editor: its font and colors.
	CodeEdit *view = preview_pane->code_view;
	if (!p_style_from) {
		return;
	}
	view->add_theme_font_override(SceneStringName(font), p_style_from->get_theme_font(SceneStringName(font)));
	view->add_theme_font_size_override(SceneStringName(font_size), p_style_from->get_theme_font_size(SceneStringName(font_size)));
	static const char *colors[] = { "background_color", "font_color", "line_number_color", "current_line_color", "selection_color", "word_highlighted_color", "caret_color", "brace_mismatch_color" };
	for (const char *color : colors) {
		view->add_theme_color_override(color, p_style_from->get_theme_color(color));
	}
	// Read-only text would otherwise be drawn dimmed.
	view->add_theme_color_override("font_readonly_color", p_style_from->get_theme_color(SceneStringName(font_color)));
}

void EditorSplitView::_set_code_static(const Ref<Script> &p_script) {
	// A script that isn't open in the script editor: show its source.
	CodeEdit *view = preview_pane->code_view;
	if (code_static_script == p_script && view->get_text() == p_script->get_source_code()) {
		return;
	}
	_set_code_source(nullptr);
	const bool same = code_static_script == p_script;
	code_static_script = p_script;
	// Borrow the look of any open script editor.
	DocumentEditorContainer *container = ScriptEditor::get_singleton()->get_script_container();
	for (const Ref<Script> &scr : ScriptEditor::get_singleton()->get_open_scripts()) {
		if (TextEditorBase *editor = Object::cast_to<TextEditorBase>(container->get_resource_editor(scr))) {
			_style_code_view(editor->get_code_editor()->get_text_editor());
			break;
		}
	}
	view->set_syntax_highlighter(container->create_syntax_highlighter(p_script));
	const double scroll = view->get_v_scroll();
	view->set_text(p_script->get_source_code());
	view->set_v_scroll(same ? scroll : 0);
}

Ref<Script> EditorSplitView::_selected_node_script() const {
	EditorSelection *selection = EditorNode::get_singleton()->get_editor_selection();
	for (Node *node : selection->get_top_selected_node_list()) {
		const Ref<Script> scr = node->get_script();
		if (scr.is_valid()) {
			return scr;
		}
	}
	Node *root = EditorNode::get_singleton()->get_edited_scene();
	return root ? Ref<Script>(root->get_script()) : Ref<Script>();
}

void EditorSplitView::_selection_changed() {
	if (pane_live) {
		_sync_pane_live_selection();
	}
	// Follow the selection: show the script of the node just selected.
	EditorSelection *selection = EditorNode::get_singleton()->get_editor_selection();
	for (Node *node : selection->get_top_selected_node_list()) {
		const Ref<Script> scr = node->get_script();
		if (scr.is_valid()) {
			scene_code_script = scr;
			if (showing_scene_code) {
				_queue_update();
			}
			return;
		}
	}
}

void EditorSplitView::_update_scene_code_view() {
	if (scene_code_script.is_null()) {
		scene_code_script = _selected_node_script();
	}
	const Ref<Script> scr = scene_code_script;
	TextEditorBase *open = scr.is_valid() ? Object::cast_to<TextEditorBase>(ScriptEditor::get_singleton()->get_script_container()->get_resource_editor(scr)) : nullptr;
	preview_pane->code_view->set_visible(scr.is_valid());
	preview_pane->code_placeholder->set_visible(scr.is_null());
	preview_pane->code_placeholder->set_text(TTR("Select a node with a script, or pick a script from the menu above."));
	if (open) {
		// Open in the script editor: show its current text, unsaved edits included.
		_set_code_source(open->get_code_editor()->get_text_editor());
	} else if (scr.is_valid()) {
		_set_code_static(scr);
	} else {
		_set_code_source(nullptr);
	}

	if (scr.is_null()) {
		preview_pane->set_title(TTR("No script"));
		preview_pane->type_label->set_text(String());
	} else {
		String title = scr->get_path().is_empty() ? TTR("[unsaved]") : scr->get_path().get_file();
		if (open && open->is_unsaved()) {
			title += "(*)";
		}
		preview_pane->set_title(title);
		preview_pane->type_label->set_text(scr->get_class());
		_record_code_history(scr->get_path());
	}
	if (floating) {
		float_window->set_title(vformat(TTR("%s - Split View"), preview_pane->file_label->get_text()));
	}
	_update_code_toolbar();
}

// View 2's code toolbar: Search, Go To and back/forward, all read-only.

Ref<Script> EditorSplitView::_shown_code_script() const {
	if (showing_scene_code) {
		return scene_code_script;
	}
	const TextEditorBase *shown = showing_code ? ObjectDB::get_instance<TextEditorBase>(code_editor_id) : nullptr;
	if (!shown) {
		return Ref<Script>();
	}
	const Ref<Script> scr = shown->get_edited_resource();
	return scr;
}

CodeEdit *EditorSplitView::_edited_code_edit() const {
	// The script being edited, when view 2 compares another with it.
	TextEditorBase *edited = showing_code ? ObjectDB::get_instance<TextEditorBase>(current_code_editor_id) : nullptr;
	return edited ? edited->get_code_editor()->get_text_editor() : nullptr;
}

bool EditorSplitView::_code_both_scripts() const {
	return showing_code && preview_pane->code_both_button->is_pressed() && _edited_code_edit();
}

void EditorSplitView::_update_code_toolbar() {
	const bool has_code = preview_pane->code_view->is_visible();
	preview_pane->code_search_menu->set_disabled(!has_code);
	preview_pane->code_goto_menu->set_disabled(!has_code);
	preview_pane->code_function_menu->set_disabled(!has_code);
	// Only beside the script editor is there a second script to line up.
	preview_pane->code_both_button->set_visible(showing_code);
	if (!has_code) {
		_code_find_hide();
	} else if (preview_pane->code_find_bar->is_visible()) {
		// The text was replaced: show the search on it again.
		_code_find_options_changed(false);
	}
	if (!_code_both_scripts()) {
		_clear_other_search();
	}
	_update_code_history_buttons();
}

void EditorSplitView::_fill_code_menus() {
	// Filled once the script editor has registered its shortcuts, which is
	// after the panes are built.
	PopupMenu *search = preview_pane->code_search_menu->get_popup();
	if (search->get_item_count() == 0) {
		search->add_shortcut(ED_GET_SHORTCUT("script_text_editor/find"), EditorScenePane::CODE_FIND);
		search->add_shortcut(ED_GET_SHORTCUT("script_text_editor/find_next"), EditorScenePane::CODE_FIND_NEXT);
		search->add_shortcut(ED_GET_SHORTCUT("script_text_editor/find_previous"), EditorScenePane::CODE_FIND_PREVIOUS);
	}
	PopupMenu *go_to = preview_pane->code_goto_menu->get_popup();
	if (go_to->get_item_count() == 0) {
		go_to->add_shortcut(ED_GET_SHORTCUT("script_text_editor/goto_line"), EditorScenePane::CODE_GOTO_LINE);
		go_to->add_shortcut(ED_GET_SHORTCUT("script_text_editor/goto_function"), EditorScenePane::CODE_GOTO_FUNCTION);
	}
}

void EditorSplitView::_code_menu_id_pressed(int p_id) {
	switch (p_id) {
		case EditorScenePane::CODE_FIND: {
			_code_find_show();
		} break;
		case EditorScenePane::CODE_FIND_NEXT: {
			_code_find(false);
		} break;
		case EditorScenePane::CODE_FIND_PREVIOUS: {
			_code_find(true);
		} break;
		case EditorScenePane::CODE_GOTO_LINE: {
			_code_line_popup();
		} break;
		case EditorScenePane::CODE_GOTO_FUNCTION: {
			preview_pane->code_function_menu->show_popup();
		} break;
	}
}

bool EditorSplitView::_code_shortcut_input(const Ref<InputEvent> &p_event) {
	// Over view 2's code, or typing in its find bar, the script editor's
	// search shortcuts work on it instead.
	const Ref<InputEventKey> k = p_event;
	if (k.is_null() || !k->is_pressed() || !enabled) {
		return false;
	}
	Control *container = preview_pane->code_container;
	if (!container->is_visible_in_tree() || !preview_pane->code_view->is_visible()) {
		return false;
	}
	const bool hovered = Rect2(Point2(), container->get_size()).has_point(container->get_local_mouse_position());
	const Control *focus = container->get_viewport()->gui_get_focus_owner();
	if (!hovered && !(focus && container->is_ancestor_of(focus))) {
		return false;
	}
	if (ED_IS_SHORTCUT("script_text_editor/find", p_event)) {
		_code_find_show();
	} else if (ED_IS_SHORTCUT("script_text_editor/find_next", p_event)) {
		_code_find(false);
	} else if (ED_IS_SHORTCUT("script_text_editor/find_previous", p_event)) {
		_code_find(true);
	} else if (ED_IS_SHORTCUT("script_text_editor/goto_line", p_event)) {
		_code_line_popup();
	} else if (ED_IS_SHORTCUT("script_text_editor/goto_function", p_event)) {
		preview_pane->code_function_menu->show_popup();
	} else if (ED_IS_SHORTCUT("script_editor/history_previous", p_event)) {
		_code_history_step(-1);
	} else if (ED_IS_SHORTCUT("script_editor/history_next", p_event)) {
		_code_history_step(1);
	} else {
		return false;
	}
	return true;
}

void EditorSplitView::_code_find_show() {
	CodeEdit *view = preview_pane->code_view;
	LineEdit *text = preview_pane->code_find_text;
	// Start from a word or line selected here, as the script editor does.
	if (view->has_selection() && view->get_selection_from_line() == view->get_selection_to_line()) {
		text->set_text(view->get_selected_text());
	}
	preview_pane->code_find_bar->show();
	text->grab_focus();
	text->select_all();
	_code_find_text_changed(text->get_text());
}

void EditorSplitView::_code_find_hide() {
	if (!preview_pane->code_find_bar->is_visible()) {
		return;
	}
	preview_pane->code_find_bar->hide();
	preview_pane->code_view->set_search_text(String());
	preview_pane->code_view->queue_redraw();
	_clear_other_search();
}

uint32_t EditorSplitView::_code_find_flags() const {
	uint32_t flags = 0;
	if (preview_pane->code_find_case->is_pressed()) {
		flags |= TextEdit::SEARCH_MATCH_CASE;
	}
	if (preview_pane->code_find_words->is_pressed()) {
		flags |= TextEdit::SEARCH_WHOLE_WORDS;
	}
	return flags;
}

void EditorSplitView::_code_find_text_changed(const String &p_text) {
	// Highlight every match, and move to the first from where it was.
	const uint32_t flags = _code_find_flags();
	CodeEdit *view = preview_pane->code_view;
	view->set_search_text(p_text);
	view->set_search_flags(flags);
	view->queue_redraw();
	if (!p_text.is_empty()) {
		_code_search_in(view, p_text, flags, false, true);
	}
	CodeEdit *other = _code_both_scripts() ? _edited_code_edit() : nullptr;
	if (other && current_code_editor_id != code_other_search_id) {
		_clear_other_search();
	}
	if (other) {
		other->set_search_text(p_text);
		other->set_search_flags(flags);
		other->queue_redraw();
		code_other_search_id = current_code_editor_id;
		if (!p_text.is_empty()) {
			_code_search_in(other, p_text, flags, false, true);
		}
	} else {
		_clear_other_search();
	}
	_update_code_find_matches();
}

void EditorSplitView::_code_find_submitted(const String &p_text) {
	_code_find(Input::get_singleton()->is_key_pressed(Key::SHIFT));
}

void EditorSplitView::_code_find_options_changed(bool p_pressed) {
	_code_find_text_changed(preview_pane->code_find_text->get_text());
}

void EditorSplitView::_code_find_input(const Ref<InputEvent> &p_event) {
	const Ref<InputEventKey> k = p_event;
	if (k.is_valid() && k->is_pressed() && k->get_keycode() == Key::ESCAPE) {
		_code_find_hide();
		preview_pane->code_find_text->accept_event();
	}
}

void EditorSplitView::_code_find(bool p_backwards) {
	const String text = preview_pane->code_find_text->get_text();
	if (!preview_pane->code_find_bar->is_visible() || text.is_empty()) {
		_code_find_show();
		return;
	}
	const uint32_t flags = _code_find_flags();
	_code_search_in(preview_pane->code_view, text, flags, p_backwards, false);
	if (CodeEdit *other = _code_both_scripts() ? _edited_code_edit() : nullptr) {
		_code_search_in(other, text, flags, p_backwards, false);
	}
	_update_code_find_matches();
}

bool EditorSplitView::_code_search_in(CodeEdit *p_edit, const String &p_text, uint32_t p_flags, bool p_backwards, bool p_from_start) {
	// From the selected match: the next one after it, the one before it, or
	// (while typing) the first at or after its start.
	int line = p_edit->get_caret_line();
	int column = p_edit->get_caret_column();
	if (p_edit->has_selection()) {
		const bool from = p_backwards || p_from_start;
		line = from ? p_edit->get_selection_from_line() : p_edit->get_selection_to_line();
		column = from ? p_edit->get_selection_from_column() : p_edit->get_selection_to_column();
	}
	const Point2i pos = p_edit->search(p_text, p_flags | (p_backwards ? (uint32_t)TextEdit::SEARCH_BACKWARDS : 0), line, column);
	if (pos.x < 0) {
		p_edit->deselect();
		return false;
	}
	p_edit->unfold_line(pos.y);
	p_edit->select(pos.y, pos.x, pos.y, pos.x + p_text.length());
	p_edit->center_viewport_to_caret();
	return true;
}

int EditorSplitView::_code_count_matches(CodeEdit *p_edit, const String &p_text, uint32_t p_flags, int &r_current) {
	// As the script editor's find bar counts them.
	r_current = 0;
	if (p_text.is_empty()) {
		return 0;
	}
	const bool match_case = p_flags & TextEdit::SEARCH_MATCH_CASE;
	const bool whole_words = p_flags & TextEdit::SEARCH_WHOLE_WORDS;
	const bool start_is_symbol = is_symbol(p_text[0]);
	const bool end_is_symbol = is_symbol(p_text[p_text.length() - 1]);
	const bool selected = p_edit->has_selection();
	const int selected_line = p_edit->get_selection_from_line();
	const int selected_column = p_edit->get_selection_from_column();
	int count = 0;
	for (int i = 0; i < p_edit->get_line_count(); i++) {
		const String line = p_edit->get_line(i);
		int column = 0;
		while (true) {
			column = match_case ? line.find(p_text, column) : line.findn(p_text, column);
			if (column < 0) {
				break;
			}
			const int end = column + p_text.length();
			if (whole_words && ((!start_is_symbol && column > 0 && !is_symbol(line[column - 1])) || (!end_is_symbol && end < line.length() && !is_symbol(line[end])))) {
				column = end;
				continue;
			}
			count++;
			if (selected && i == selected_line && column == selected_column) {
				r_current = count;
			}
			column = end;
		}
	}
	return count;
}

void EditorSplitView::_update_code_find_matches() {
	Label *label = preview_pane->code_find_matches;
	const String text = preview_pane->code_find_text->get_text();
	if (text.is_empty()) {
		label->set_text(String());
		return;
	}
	const uint32_t flags = _code_find_flags();
	int current = 0;
	const int count = _code_count_matches(preview_pane->code_view, text, flags, current);
	String result;
	if (count == 0) {
		result = TTR("No match");
	} else if (current > 0) {
		result = vformat(TTRN("%d of %d match", "%d of %d matches", count), current, count);
	} else {
		result = vformat(TTRN("%d match", "%d matches", count), count);
	}
	if (CodeEdit *other = _code_both_scripts() ? _edited_code_edit() : nullptr) {
		int other_current = 0;
		result += "  " + vformat(TTR("(%d in the edited script)"), _code_count_matches(other, text, flags, other_current));
	}
	label->set_text(result);
	label->add_theme_color_override(SceneStringName(font_color), count > 0 ? get_theme_color(SceneStringName(font_color), SNAME("Label")) : get_theme_color(SNAME("error_color"), EditorStringName(Editor)));
}

void EditorSplitView::_clear_other_search() {
	// Take the search off the edited script, unless its own find bar is
	// searching it now.
	TextEditorBase *editor = ObjectDB::get_instance<TextEditorBase>(code_other_search_id);
	if (editor) {
		const FindReplaceBar *bar = editor->get_code_editor()->get_find_replace_bar();
		if (!bar || !bar->is_visible_in_tree() || bar->get_search_text().is_empty()) {
			editor->get_code_editor()->get_text_editor()->set_search_text(String());
			editor->get_code_editor()->get_text_editor()->queue_redraw();
		}
	}
	code_other_search_id = ObjectID();
}

void EditorSplitView::_code_line_popup() {
	CodeEdit *view = preview_pane->code_view;
	if (!view->is_visible_in_tree()) {
		return;
	}
	LineEdit *text = preview_pane->code_line_text;
	text->set_text(itos(view->get_caret_line() + 1));
	MenuButton *anchor = preview_pane->code_goto_menu;
	preview_pane->code_line_popup->popup(Rect2i(anchor->get_screen_position() + Vector2(0, anchor->get_size().y), Size2i()));
	text->grab_focus();
	text->select_all();
}

void EditorSplitView::_code_line_submitted(const String &p_text) {
	CodeEdit *view = preview_pane->code_view;
	if (p_text.is_valid_int()) {
		const int line = CLAMP(p_text.to_int() - 1, 0, view->get_line_count() - 1);
		view->deselect();
		view->unfold_line(line);
		view->set_caret_line(line, false);
		view->set_caret_column(0, false);
		view->center_viewport_to_caret();
	}
	preview_pane->code_line_popup->hide();
}

void EditorSplitView::_fill_code_function_menu() {
	// The functions of the script shown, in the order they appear.
	PopupMenu *menu = preview_pane->code_function_menu->get_popup();
	menu->clear();
	const Ref<Script> scr = _shown_code_script();
	List<String> functions;
	if (scr.is_valid() && scr->get_language() && scr->get_language()->get_editor_language()) {
		scr->get_language()->get_editor_language()->validate(preview_pane->code_view->get_text(), scr->get_path(), nullptr, nullptr, &functions, nullptr);
	}
	Vector<Pair<int, String>> sorted;
	for (const String &function : functions) {
		sorted.push_back(Pair<int, String>(function.get_slicec(':', 1).to_int(), function.get_slicec(':', 0)));
	}
	sorted.sort_custom<PairSort<int, String>>();
	for (const Pair<int, String> &function : sorted) {
		menu->add_item(function.second);
		menu->set_item_metadata(-1, function.first);
		menu->set_item_tooltip(-1, vformat(TTR("Line %d"), function.first));
	}
	if (sorted.is_empty()) {
		menu->add_item(TTR("No functions found"));
		menu->set_item_disabled(-1, true);
	}
}

void EditorSplitView::_code_function_pressed(int p_index) {
	PopupMenu *menu = preview_pane->code_function_menu->get_popup();
	const int line_number = menu->get_item_metadata(p_index);
	const int line = line_number - 1;
	CodeEdit *view = preview_pane->code_view;
	if (line < 0 || line >= view->get_line_count()) {
		return;
	}
	view->deselect();
	view->unfold_line(line);
	view->set_caret_line(line, false);
	view->set_caret_column(0, false);
	view->center_viewport_to_caret();
	if (!_code_both_scripts()) {
		return;
	}
	// Line up the function of the same name in the script being edited.
	CodeEditorBase *edited = ObjectDB::get_instance<CodeEditorBase>(current_code_editor_id);
	if (!edited) {
		return; // Not a script with functions, such as a text file.
	}
	const String name = menu->get_item_text(p_index);
	for (const String &function : edited->get_functions()) {
		if (function.get_slicec(':', 0) == name) {
			edited->goto_line_centered(function.get_slicec(':', 1).to_int() - 1);
			break;
		}
	}
}

void EditorSplitView::_record_code_history(const String &p_path) {
	if (p_path.is_empty() || (code_history_pos >= 0 && code_history[code_history_pos] == p_path)) {
		return;
	}
	// A new script drops the ones stepped back from, like a browser.
	code_history.resize(code_history_pos + 1);
	code_history.push_back(p_path);
	if (code_history.size() > CODE_HISTORY_MAX) {
		code_history.remove_at(0);
	}
	code_history_pos = code_history.size() - 1;
	_update_code_history_buttons();
}

void EditorSplitView::_code_history_step(int p_step) {
	// Skip scripts that can't be shown any more, such as one closed since.
	for (int pos = code_history_pos + p_step; pos >= 0 && pos < code_history.size(); pos += p_step) {
		const String &path = code_history[pos];
		bool shown = false;
		if (showing_code) {
			DocumentEditorContainer *container = ScriptEditor::get_singleton()->get_script_container();
			for (const Ref<Script> &scr : ScriptEditor::get_singleton()->get_open_scripts()) {
				TextEditorBase *editor = Object::cast_to<TextEditorBase>(container->get_resource_editor(scr));
				if (editor && scr->get_path() == path && editor->get_instance_id() != current_code_editor_id) {
					code_editor_id = editor->get_instance_id();
					shown = true;
					break;
				}
			}
		} else if (showing_scene_code) {
			const Ref<Script> scr = ResourceLoader::load(path, "Script");
			if (scr.is_valid()) {
				scene_code_script = scr;
				shown = true;
			}
		}
		if (shown) {
			code_history_pos = pos;
			_update_code_history_buttons();
			_queue_update();
			return;
		}
	}
}

void EditorSplitView::_update_code_history_buttons() {
	preview_pane->code_back_button->set_disabled(code_history_pos <= 0);
	preview_pane->code_forward_button->set_disabled(code_history_pos < 0 || code_history_pos >= code_history.size() - 1);
}

void EditorSplitView::_set_setting(const String &p_name, const Variant &p_value) {
	EditorSettings::get_singleton()->set(p_name, p_value);
	EditorSettings::get_singleton()->notify_changes();
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
	preview_pane->code_placeholder->set_text(TTR("Open another script to compare it here.\nThe script you switch away from appears here."));
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
		_record_code_history(res->get_path());
	}
	if (floating) {
		float_window->set_title(vformat(TTR("%s - Split View"), preview_pane->file_label->get_text()));
	}
	_update_code_toolbar();
}

void EditorSplitView::_fill_pick_menu() {
	PopupMenu *menu = preview_pane->pick_button->get_popup();
	menu->clear();
	if (showing_scene_code) {
		// Scripts used in the edited scene, then other open scripts.
		Vector<Ref<Script>> scripts;
		Node *root = EditorNode::get_singleton()->get_edited_scene();
		List<Node *> stack;
		if (root) {
			stack.push_back(root);
		}
		while (!stack.is_empty()) {
			Node *node = stack.front()->get();
			stack.pop_front();
			const Ref<Script> scr = node->get_script();
			if (scr.is_valid() && !scripts.has(scr)) {
				scripts.push_back(scr);
			}
			for (int i = 0; i < node->get_child_count(); i++) {
				Node *child = node->get_child(i);
				if (child->get_owner() == root || child->get_owner() == nullptr) {
					stack.push_back(child);
				}
			}
		}
		const int scene_count = scripts.size();
		for (const Ref<Script> &scr : ScriptEditor::get_singleton()->get_open_scripts()) {
			if (!scripts.has(scr)) {
				scripts.push_back(scr);
			}
		}
		for (int i = 0; i < scripts.size(); i++) {
			if (i == scene_count && i > 0) {
				menu->add_separator(TTR("Open Scripts"));
			}
			const Ref<Script> &scr = scripts[i];
			menu->add_radio_check_item(scr->get_path().is_empty() ? TTR("[unsaved]") : scr->get_path().get_file());
			const int idx = menu->get_item_count() - 1;
			menu->set_item_metadata(idx, scr);
			menu->set_item_tooltip(idx, scr->get_path());
			menu->set_item_checked(idx, scr == scene_code_script);
		}
		if (scripts.is_empty()) {
			menu->add_item(TTR("No scripts in this scene"));
			menu->set_item_disabled(-1, true);
		}
		return;
	}
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
	if (showing_scene_code) {
		scene_code_script = meta;
	} else if (showing_code) {
		code_editor_id = ObjectID(uint64_t(meta));
	} else {
		show_scene_in_preview(int(meta));
	}
	_queue_update();
}

void EditorSplitView::_code_toggled(bool p_pressed) {
	if (_is_script_screen()) {
		compare_scripts = p_pressed;
		_set_setting("interface/editor/split_view/compare_scripts", p_pressed);
	} else {
		code_beside_scene = p_pressed;
		_set_setting("interface/editor/split_view/code_beside_scene", p_pressed);
	}
	_queue_update();
}

void EditorSplitView::show_script_in_preview(ScriptEditorBase *p_editor) {
	TextEditorBase *shown = Object::cast_to<TextEditorBase>(p_editor);
	ERR_FAIL_NULL(shown);
	_track_current_script();
	// Picking it in the script list made it the edited script; go back to the
	// one edited before it, which leaves this one in the pane.
	TextEditorBase *back = ObjectDB::get_instance<TextEditorBase>(current_code_editor_id == shown->get_instance_id() ? code_editor_id : current_code_editor_id);
	if (!back || back == shown) {
		back = nullptr;
		DocumentEditorContainer *container = ScriptEditor::get_singleton()->get_script_container();
		for (const Ref<Script> &scr : ScriptEditor::get_singleton()->get_open_scripts()) {
			TextEditorBase *editor = Object::cast_to<TextEditorBase>(container->get_resource_editor(scr));
			if (editor && editor != shown) {
				back = editor;
				break;
			}
		}
	}
	if (!back) {
		return;
	}
	if (!enabled) {
		enabled = true;
		_set_setting("interface/editor/split_view/enabled", true);
	}
	if (!compare_scripts) {
		compare_scripts = true;
		_set_setting("interface/editor/split_view/compare_scripts", true);
	}
	if (ScriptEditor::get_singleton()->get_current_editor() != back) {
		ScriptEditor::get_singleton()->edit(back->get_edited_resource());
	}
	_track_current_script();
	code_editor_id = shown->get_instance_id();
	_queue_update();
}

bool EditorSplitView::_can_drop(const Point2 &p_point, const Variant &p_data) const {
	const Dictionary d = p_data;
	const String type = d.get("type", "");
	const bool scene_screen = _is_scene_screen();
	const bool script_screen = _is_script_screen();
	if (type == "tab") {
		// A scene tab.
		const Node *from = get_node_or_null(NodePath(d.get("from_path", NodePath())));
		return scene_screen && from && EditorSceneTabs::get_singleton()->is_ancestor_of(from);
	}
	if (type == "script_list_element") {
		return script_screen && Object::cast_to<TextEditorBase>(d.get("script_list_element", Variant()).operator Object *());
	}
	if (type == "resource") {
		const Ref<Script> scr = d.get("resource", Variant());
		return scr.is_valid() && (scene_screen || script_screen);
	}
	if (type == "files") {
		const Vector<String> files = d.get("files", Vector<String>());
		if (files.is_empty()) {
			return false;
		}
		const String file_type = ResourceLoader::get_resource_type(files[0]);
		if (ClassDB::is_parent_class(file_type, "PackedScene")) {
			return scene_screen;
		}
		return ClassDB::is_parent_class(file_type, "Script") && (scene_screen || script_screen);
	}
	return false;
}

void EditorSplitView::_drop(const Point2 &p_point, const Variant &p_data) {
	const Dictionary d = p_data;
	const String type = d.get("type", "");
	EditorData &ed = EditorNode::get_editor_data();
	Ref<Script> scr;
	if (type == "tab") {
		int idx = d.get("tab_index", -1);
		if (idx < 0 || idx >= ed.get_edited_scene_count()) {
			return;
		}
		if (idx == ed.get_edited_scene()) {
			// Pressing the tab made it the edited scene; go back to the one
			// edited before it, which leaves the dragged scene in the pane.
			const ObjectID dragged = ed.get_edited_scene_root(idx) ? ed.get_edited_scene_root(idx)->get_instance_id() : ObjectID();
			const int back = _find_scene_index(preview_scene);
			if (back < 0 || back == idx) {
				return;
			}
			EditorSceneTabs::get_singleton()->set_current_tab(back);
			idx = _find_scene_index(dragged);
		}
		if (idx >= 0 && idx != ed.get_edited_scene()) {
			code_beside_scene = false;
			_set_setting("interface/editor/split_view/code_beside_scene", false);
			show_scene_in_preview(idx);
		}
		return;
	}
	if (type == "script_list_element") {
		show_script_in_preview(Object::cast_to<ScriptEditorBase>(d.get("script_list_element", Variant()).operator Object *()));
		return;
	}
	if (type == "resource") {
		scr = d.get("resource", Variant());
	} else if (type == "files") {
		const Vector<String> files = d.get("files", Vector<String>());
		ERR_FAIL_COND(files.is_empty());
		const String path = files[0];
		if (ClassDB::is_parent_class(ResourceLoader::get_resource_type(path), "PackedScene")) {
			int idx = -1;
			for (int i = 0; i < ed.get_edited_scene_count(); i++) {
				if (ed.get_scene_path(i) == path) {
					idx = i;
				}
			}
			if (idx < 0) {
				// Open it, then go back to the scene being edited.
				const int current = ed.get_edited_scene();
				if (EditorNode::get_singleton()->load_scene(path) != OK) {
					return;
				}
				for (int i = 0; i < ed.get_edited_scene_count(); i++) {
					if (ed.get_scene_path(i) == path) {
						idx = i;
					}
				}
				if (current >= 0 && current < ed.get_edited_scene_count()) {
					EditorSceneTabs::get_singleton()->set_current_tab(current);
				}
			}
			if (idx >= 0 && idx != ed.get_edited_scene()) {
				code_beside_scene = false;
				_set_setting("interface/editor/split_view/code_beside_scene", false);
				show_scene_in_preview(idx);
			}
			return;
		}
		scr = ResourceLoader::load(path);
	}
	if (scr.is_null()) {
		return;
	}
	if (_is_script_screen()) {
		// Open it in the script editor, then keep it beside the current one.
		ScriptEditor::get_singleton()->edit(scr, false);
		show_script_in_preview(ScriptEditor::get_singleton()->get_script_container()->get_resource_editor(scr));
		return;
	}
	scene_code_script = scr;
	if (!code_beside_scene) {
		code_beside_scene = true;
		_set_setting("interface/editor/split_view/code_beside_scene", true);
	}
	_queue_update();
}

void EditorSplitView::_draw_preview_overlay() {
	Control *overlay = preview_pane->overlay;
	Node *root = _pane_scene();
	const bool scene_2d = root && !Object::cast_to<Node3D>(root);
	if (scene_2d) {
		_draw_preview_2d_under();
	}
	preview_pane->tools->draw_overlay();
	if (scene_2d) {
		_draw_preview_2d_over();
	}
	if (zoom_indicator_delay > 0.0 && Object::cast_to<Node3D>(root) && !preview_cinematic) {
		// The 3D editor's zoom bar (how far the view is from what it orbits)
		// or, while flying, its speed bar; on a log scale between the
		// camera's near and far limits.
		const real_t min_distance = MAX(preview_pane->camera_3d->get_near() * 4, (real_t)0.01);
		const real_t max_distance = MIN(preview_pane->camera_3d->get_far() / 4, (real_t)10000.0);
		const real_t scale_length = max_distance - min_distance;
		if (!Math::is_zero_approx(scale_length)) {
			const real_t distance = preview_freelook ? freelook_speed : preview_orbit.distance;
			const real_t log_fill = Math::log1p(MAX(distance - min_distance, (real_t)0.0)) / Math::log1p(scale_length);
			const real_t fill = CLAMP(preview_freelook ? log_fill : 1.0 - log_fill, 0.0, 1.0);
			const Color color = preview_freelook ? Color(1.0, 0.95, 0.7) : Color(0.7, 0.95, 1.0);
			const real_t height = overlay->get_size().y / 2.0;
			const Rect2 bar(10 * EDSCALE, (overlay->get_size().y - height) / 2.0, 6 * EDSCALE, height);
			const real_t filled = bar.size.y * fill;
			// Readable over any background: both a light and a dark edge.
			overlay->draw_rect(bar, color * Color(1, 1, 1, 0.2));
			overlay->draw_rect(Rect2(bar.position.x, bar.position.y + bar.size.y - filled, bar.size.x, filled), color * Color(1, 1, 1, 0.6));
			overlay->draw_rect(bar.grow(1), Color(0, 0, 0, 0.7), false, Math::round(EDSCALE));
			const Ref<Texture2D> icon = get_editor_theme_icon(preview_freelook ? SNAME("ViewportSpeed") : SNAME("ViewportZoom"));
			const Vector2 icon_size = icon->get_size();
			const Vector2 icon_pos = Vector2(bar.position.x - (icon_size.x - bar.size.x) / 2, bar.position.y + bar.size.y + 2 * EDSCALE);
			overlay->draw_texture(icon, icon_pos, color);
			const Ref<Font> font = get_theme_font(SNAME("bold"), EditorStringName(EditorFonts));
			const int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));
			const String text = vformat(preview_freelook ? "%s m/s" : "%s m", String::num(distance).pad_decimals(distance < 1.0 ? 2 : 1));
			const Vector2 text_pos = Vector2(icon_pos.x, icon_pos.y + icon_size.y + 16 * EDSCALE);
			overlay->draw_string_outline(font, text_pos, text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, Math::round(4 * EDSCALE), Color(0, 0, 0));
			overlay->draw_string(font, text_pos, text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, color);
		}
	}
}

// The second pane's Scene toolbar: the Scene dock's, for the pane's scene.

Node *EditorSplitView::_pane_target_node() const {
	// What's selected in the pane, or its scene's root.
	Node *selected = preview_pane->tools->get_selected();
	return selected ? selected : _pane_scene();
}

void EditorSplitView::_pane_add_child(Node *p_parent, Node *p_child, const String &p_action) {
	Node *root = _pane_scene();
	const int history_id = preview_pane->tools->get_history_id();
	if (!root || history_id == EditorUndoRedoManager::INVALID_HISTORY) {
		memdelete(p_child);
		return;
	}
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action_for_history(p_action, history_id);
	undo_redo->force_fixed_history();
	undo_redo->add_do_method(p_parent, "add_child", p_child, true);
	undo_redo->add_do_method(p_child, "set_owner", root);
	undo_redo->add_do_reference(p_child);
	undo_redo->add_undo_method(p_parent, "remove_child", p_child);
	undo_redo->commit_action();

	// Select it, as the Scene dock does.
	_set_preview_active(true);
	preview_pane->tree->update_tree();
	preview_pane->tools->set_selected(p_child, false);
	_pane_selection_changed();
}

void EditorSplitView::_pane_add_node() {
	Node *parent = _pane_target_node();
	if (!parent) {
		return;
	}
	_set_preview_active(true);
	pane_new_node_parent = parent->get_instance_id();
	if (!pane_create_dialog) {
		pane_create_dialog = memnew(CreateDialog);
		pane_create_dialog->set_base_type("Node");
		pane_create_dialog->connect("create", callable_mp(this, &EditorSplitView::_pane_create_node));
		add_child(pane_create_dialog);
	}
	pane_create_dialog->popup_create(true);
}

void EditorSplitView::_pane_create_node() {
	Node *parent = ObjectDB::get_instance<Node>(pane_new_node_parent);
	Node *root = _pane_scene();
	if (!parent || !root || (parent != root && !root->is_ancestor_of(parent))) {
		return;
	}
	Node *child = pane_create_dialog->instantiate_selected<Node>();
	ERR_FAIL_NULL(child);

	// Named as the Scene dock names new nodes.
	String name = child->get_name();
	if (name.is_empty()) {
		name = child->get_class();
	}
	if (GLOBAL_GET("editor/naming/node_name_casing").operator int() != Node::NAME_CASING_PASCAL_CASE) {
		name = Node::adjust_name_casing(name);
	}
	child->set_name(parent->prevalidate_child_name(child, name));
	if (Control *control = Object::cast_to<Control>(child)) {
		// As the Scene dock does: not so small it can't be seen.
		Size2 size = control->get_minimum_size();
		size.width = size.width < 4 ? 40 : size.width;
		size.height = size.height < 4 ? 40 : size.height;
		control->set_size(size);
	}
	_pane_add_child(parent, child, TTR("Create Node"));
}

void EditorSplitView::_pane_instantiate_scene() {
	Node *parent = _pane_target_node();
	if (!parent) {
		return;
	}
	_set_preview_active(true);
	pane_new_node_parent = parent->get_instance_id();
	EditorNode::get_singleton()->get_quick_open_dialog()->popup_dialog({ "PackedScene" }, callable_mp(this, &EditorSplitView::_pane_scene_picked));
}

void EditorSplitView::_pane_scene_picked(const String &p_path) {
	Node *parent = ObjectDB::get_instance<Node>(pane_new_node_parent);
	Node *root = _pane_scene();
	if (!parent || !root || (parent != root && !root->is_ancestor_of(parent))) {
		return;
	}
	Ref<PackedScene> packed = ResourceLoader::load(p_path);
	if (packed.is_null()) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("Error loading scene from %s"), p_path));
		return;
	}
	Node *instance = packed->instantiate(PackedScene::GEN_EDIT_STATE_INSTANCE);
	if (!instance) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("Error instantiating scene from %s"), p_path));
		return;
	}
	const String root_path = root->get_scene_file_path();
	if (!root_path.is_empty() && SceneTreeDock::get_singleton()->would_instantiate_cyclically(root_path, instance)) {
		memdelete(instance);
		EditorNode::get_singleton()->show_warning(vformat(TTR("Cannot instantiate the scene '%s' because the current scene exists within one of its nodes."), p_path));
		return;
	}
	instance->set_scene_file_path(ProjectSettings::get_singleton()->localize_path(p_path));
	_pane_add_child(parent, instance, TTR("Instantiate Scene"));
}

void EditorSplitView::_pane_attach_script() {
	Node *node = _pane_target_node();
	if (!node) {
		return;
	}
	if (ScriptServer::get_language_count() == 0) {
		EditorNode::get_singleton()->show_warning(TTR("Cannot attach a script: there are no languages registered.\nThis is probably because this editor was built with all language modules disabled."));
		return;
	}
	_set_preview_active(true);
	pane_script_target = node->get_instance_id();
	// Where the Scene dock suggests it: beside the scene.
	String path = node->get_scene_file_path();
	if (path.is_empty()) {
		const String root_path = _pane_scene()->get_scene_file_path();
		path = (root_path.is_empty() ? String("res://") : root_path.get_base_dir()).path_join(node->get_name());
	}
	if (!pane_script_dialog) {
		pane_script_dialog = memnew(ScriptCreateDialog);
		pane_script_dialog->connect("script_created", callable_mp(this, &EditorSplitView::_pane_script_created));
		add_child(pane_script_dialog);
	}
	pane_script_dialog->set_inheritance_base_type("Node");
	pane_script_dialog->config(node->get_class(), path);
	pane_script_dialog->popup_centered();
}

void EditorSplitView::_pane_script_created(const Ref<Script> &p_script) {
	Node *node = ObjectDB::get_instance<Node>(pane_script_target);
	Node *root = _pane_scene();
	const int history_id = preview_pane->tools->get_history_id();
	if (!node || !root || (node != root && !root->is_ancestor_of(node)) || history_id == EditorUndoRedoManager::INVALID_HISTORY) {
		return;
	}
	if (p_script->is_built_in()) {
		EditorNode::setup_built_in_resource(p_script, root->get_scene_file_path());
	}
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action_for_history(TTR("Attach Script"), history_id);
	undo_redo->force_fixed_history();
	undo_redo->add_do_method(InspectorDock::get_singleton(), "store_script_properties", node);
	undo_redo->add_undo_method(InspectorDock::get_singleton(), "store_script_properties", node);
	undo_redo->add_do_method(node, "set_script", p_script);
	undo_redo->add_undo_method(node, "set_script", node->get_script());
	undo_redo->add_do_method(InspectorDock::get_singleton(), "apply_script_properties", node);
	undo_redo->add_undo_method(InspectorDock::get_singleton(), "apply_script_properties", node);
	undo_redo->commit_action();
	// Open it, as the Scene dock does.
	EditorNode::get_singleton()->push_item_no_inspector(p_script.ptr());
	_update_pane_toolbar();
}

void EditorSplitView::_pane_detach_script() {
	Node *node = _pane_target_node();
	const int history_id = preview_pane->tools->get_history_id();
	if (!node || history_id == EditorUndoRedoManager::INVALID_HISTORY) {
		return;
	}
	_set_preview_active(true);
	const Ref<Script> existing = node->get_script();
	const Ref<Script> empty = EditorNode::get_singleton()->get_object_custom_type_base(node);
	if (existing == empty) {
		return;
	}
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action_for_history(TTR("Detach Script"), history_id);
	undo_redo->force_fixed_history();
	undo_redo->add_do_method(node, "set_script", empty);
	undo_redo->add_undo_method(node, "set_script", existing);
	// The script's properties come back with it.
	List<PropertyInfo> properties;
	node->get_property_list(&properties);
	for (const PropertyInfo &property : properties) {
		if (property.usage & (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_EDITOR)) {
			undo_redo->add_undo_property(node, property.name, node->get(property.name));
		}
	}
	undo_redo->commit_action();
	_update_pane_toolbar();
}

void EditorSplitView::_pane_open_scene(const String &p_path) {
	EditorNode::get_singleton()->open_scene(p_path);
}

void EditorSplitView::_pane_open_script(const Ref<Script> &p_script) {
	EditorNode::get_singleton()->push_item_no_inspector(p_script.ptr());
}

void EditorSplitView::_pane_filter_changed(const String &p_filter) {
	preview_pane->tree->set_filter(p_filter);
}

void EditorSplitView::_pane_tree_menu_about_to_popup() {
	_set_preview_active(true);
	PopupMenu *menu = preview_pane->tree_menu->get_popup();
	if (menu->get_item_count() == 0) {
		menu->add_check_item(TTRC("Auto Expand to Selected"), PANE_TREE_AUTO_EXPAND);
		menu->add_check_item(TTRC("Hide Filtered Out Parents"), PANE_TREE_HIDE_FILTERED_OUT_PARENTS);
		menu->add_separator();
		menu->add_check_item(TTRC("Show Accessibility Warnings"), PANE_TREE_ACCESSIBILITY_WARNINGS);
		menu->add_submenu_node_item(TTRC("All Scene Sub-Resources"), preview_pane->subresource_menu);
		menu->add_separator();
		menu->add_item(TTRC("Filter by Type"), PANE_TREE_FILTER_BY_TYPE);
		menu->set_item_tooltip(-1, TTRC("Selects all Nodes of the given type.\nInserts \"type:\". You can also use the shorthand \"t:\"."));
		menu->add_item(TTRC("Filter by Group"), PANE_TREE_FILTER_BY_GROUP);
		menu->set_item_tooltip(-1, TTRC("Selects all Nodes belonging to the given group.\nIf empty, selects any Node belonging to any group.\nInserts \"group:\". You can also use the shorthand \"g:\"."));
	}
	menu->set_item_checked(menu->get_item_index(PANE_TREE_AUTO_EXPAND), EDITOR_GET("docks/scene_tree/auto_expand_to_selected"));
	menu->set_item_checked(menu->get_item_index(PANE_TREE_HIDE_FILTERED_OUT_PARENTS), EDITOR_GET("docks/scene_tree/hide_filtered_out_parents"));
	menu->set_item_checked(menu->get_item_index(PANE_TREE_ACCESSIBILITY_WARNINGS), EDITOR_GET("docks/scene_tree/accessibility_warnings"));
}

void EditorSplitView::_pane_tree_menu_option(int p_option) {
	// The same settings as the Scene dock's, applied to both trees.
	SceneTreeEditor *dock_tree = SceneTreeDock::get_singleton()->get_tree_editor();
	switch (p_option) {
		case PANE_TREE_AUTO_EXPAND: {
			const bool expand = !EDITOR_GET("docks/scene_tree/auto_expand_to_selected");
			dock_tree->set_auto_expand_selected(expand, true);
			preview_pane->tree->set_auto_expand_selected(expand, false);
		} break;
		case PANE_TREE_HIDE_FILTERED_OUT_PARENTS: {
			const bool hide = !EDITOR_GET("docks/scene_tree/hide_filtered_out_parents");
			dock_tree->set_hide_filtered_out_parents(hide, true);
			preview_pane->tree->set_hide_filtered_out_parents(hide, false);
		} break;
		case PANE_TREE_ACCESSIBILITY_WARNINGS: {
			const bool warnings = !EDITOR_GET("docks/scene_tree/accessibility_warnings");
			dock_tree->set_accessibility_warnings(warnings, true);
			preview_pane->tree->set_accessibility_warnings(warnings, false);
		} break;
		case PANE_TREE_FILTER_BY_TYPE:
		case PANE_TREE_FILTER_BY_GROUP: {
			LineEdit *filter = preview_pane->tree_filter;
			const String parameter = p_option == PANE_TREE_FILTER_BY_TYPE ? "type" : "group";
			filter->set_text((filter->get_text() + " " + parameter + ":").strip_edges());
			_pane_filter_changed(filter->get_text());
			filter->set_caret_column(filter->get_text().length());
			filter->grab_focus();
		} break;
	}
}

void EditorSplitView::_pane_list_subresources() {
	SceneTreeDock::get_singleton()->list_subresources(preview_pane->subresource_menu, _pane_scene());
}

void EditorSplitView::_pane_edit_subresource(int p_index) {
	const ObjectID id = preview_pane->subresource_menu->get_item_metadata(p_index);
	Object *object = ObjectDB::get_instance(id);
	if (object) {
		EditorNode::get_singleton()->push_item(object);
	}
}

void EditorSplitView::_update_pane_toolbar() {
	const bool pane_scene = !showing_code && !showing_scene_code && _pane_scene();
	preview_pane->tree_toolbar->set_visible(pane_scene);
	preview_pane->tree_title->set_visible(!pane_scene);
	if (!pane_scene) {
		return;
	}
	Node *node = _pane_target_node();
	const Ref<Script> script = node ? node->get_script() : Variant();
	const bool has_script = script.is_valid() && script != EditorNode::get_singleton()->get_object_custom_type_base(node);
	preview_pane->attach_script_button->set_visible(!has_script);
	preview_pane->detach_script_button->set_visible(has_script);
}

// The second pane's view menu: the 3D editor viewport menu's items, for the
// scene shown in the pane.

void EditorSplitView::_build_view_menu() {
	PopupMenu *p = preview_pane->view_menu->get_popup();
	p->add_shortcut(ED_GET_SHORTCUT("spatial_editor/top_view"), PANE_VIEW_TOP);
	p->add_shortcut(ED_GET_SHORTCUT("spatial_editor/bottom_view"), PANE_VIEW_BOTTOM);
	p->add_shortcut(ED_GET_SHORTCUT("spatial_editor/left_view"), PANE_VIEW_LEFT);
	p->add_shortcut(ED_GET_SHORTCUT("spatial_editor/right_view"), PANE_VIEW_RIGHT);
	p->add_shortcut(ED_GET_SHORTCUT("spatial_editor/front_view"), PANE_VIEW_FRONT);
	p->add_shortcut(ED_GET_SHORTCUT("spatial_editor/rear_view"), PANE_VIEW_REAR);
	p->add_separator();
	p->add_shortcut(ED_GET_SHORTCUT("spatial_editor/switch_perspective_orthogonal"), PANE_VIEW_SWITCH_PERSPECTIVE_ORTHOGONAL);
	p->add_radio_check_item(TTRC("Perspective"), PANE_VIEW_PERSPECTIVE);
	p->add_radio_check_item(TTRC("Orthogonal"), PANE_VIEW_ORTHOGONAL);
	p->add_check_item(TTRC("Auto Orthogonal Enabled"), PANE_VIEW_AUTO_ORTHOGONAL);
	p->set_item_checked(p->get_item_index(PANE_VIEW_AUTO_ORTHOGONAL), true);
	p->add_separator();
	p->add_check_shortcut(ED_GET_SHORTCUT("spatial_editor/view_lock_rotation"), PANE_VIEW_LOCK_ROTATION);
	p->add_separator();
	p->add_radio_check_shortcut(ED_GET_SHORTCUT("spatial_editor/view_display_normal"), PANE_VIEW_DISPLAY_BASE + (int)Viewport::DEBUG_DRAW_DISABLED);
	p->add_radio_check_shortcut(ED_GET_SHORTCUT("spatial_editor/view_display_wireframe"), PANE_VIEW_DISPLAY_BASE + (int)Viewport::DEBUG_DRAW_WIREFRAME);
	p->add_radio_check_shortcut(ED_GET_SHORTCUT("spatial_editor/view_display_overdraw"), PANE_VIEW_DISPLAY_BASE + (int)Viewport::DEBUG_DRAW_OVERDRAW);
	p->add_radio_check_shortcut(ED_GET_SHORTCUT("spatial_editor/view_display_lighting"), PANE_VIEW_DISPLAY_BASE + (int)Viewport::DEBUG_DRAW_LIGHTING);
	p->add_radio_check_shortcut(ED_GET_SHORTCUT("spatial_editor/view_display_unshaded"), PANE_VIEW_DISPLAY_BASE + (int)Viewport::DEBUG_DRAW_UNSHADED);
	p->set_item_checked(p->get_item_index(PANE_VIEW_DISPLAY_BASE + (int)Viewport::DEBUG_DRAW_DISABLED), true);

	// As in the 3D editor: each mode, and the renderers that can draw it.
	enum Renderers {
		ALL = 0,
		FORWARD_PLUS = 1,
		FORWARD_PLUS_MOBILE = 2,
	};
	struct AdvancedMode {
		const char *name;
		Viewport::DebugDraw mode;
		Renderers renderers;
		bool separator_before;
	};
	static const AdvancedMode advanced_modes[] = {
		{ TTRC("Directional Shadow Splits"), Viewport::DEBUG_DRAW_PSSM_SPLITS, FORWARD_PLUS_MOBILE, false },
		{ TTRC("Normal Buffer"), Viewport::DEBUG_DRAW_NORMAL_BUFFER, FORWARD_PLUS, true },
		{ TTRC("Shadow Atlas"), Viewport::DEBUG_DRAW_SHADOW_ATLAS, ALL, true },
		{ TTRC("Directional Shadow Map"), Viewport::DEBUG_DRAW_DIRECTIONAL_SHADOW_ATLAS, ALL, false },
		{ TTRC("Decal Atlas"), Viewport::DEBUG_DRAW_DECAL_ATLAS, FORWARD_PLUS_MOBILE, true },
		{ TTRC("AreaLight3D Atlas"), Viewport::DEBUG_DRAW_AREA_LIGHT_ATLAS, FORWARD_PLUS_MOBILE, false },
		{ TTRC("VoxelGI Lighting"), Viewport::DEBUG_DRAW_VOXEL_GI_LIGHTING, FORWARD_PLUS, true },
		{ TTRC("VoxelGI Albedo"), Viewport::DEBUG_DRAW_VOXEL_GI_ALBEDO, FORWARD_PLUS, false },
		{ TTRC("VoxelGI Emission"), Viewport::DEBUG_DRAW_VOXEL_GI_EMISSION, FORWARD_PLUS, false },
		{ TTRC("SDFGI Cascades"), Viewport::DEBUG_DRAW_SDFGI, FORWARD_PLUS, true },
		{ TTRC("SDFGI Probes"), Viewport::DEBUG_DRAW_SDFGI_PROBES, FORWARD_PLUS, false },
		{ TTRC("Scene Luminance"), Viewport::DEBUG_DRAW_SCENE_LUMINANCE, FORWARD_PLUS_MOBILE, true },
		{ TTRC("SSAO"), Viewport::DEBUG_DRAW_SSAO, FORWARD_PLUS, true },
		{ TTRC("SSIL"), Viewport::DEBUG_DRAW_SSIL, FORWARD_PLUS, false },
		{ TTRC("VoxelGI/SDFGI Buffer"), Viewport::DEBUG_DRAW_GI_BUFFER, FORWARD_PLUS, true },
		{ TTRC("Disable Mesh LOD"), Viewport::DEBUG_DRAW_DISABLE_LOD, ALL, true },
		{ TTRC("OmniLight3D Cluster"), Viewport::DEBUG_DRAW_CLUSTER_OMNI_LIGHTS, FORWARD_PLUS, true },
		{ TTRC("SpotLight3D Cluster"), Viewport::DEBUG_DRAW_CLUSTER_SPOT_LIGHTS, FORWARD_PLUS, false },
		{ TTRC("AreaLight3D Cluster"), Viewport::DEBUG_DRAW_CLUSTER_AREA_LIGHTS, FORWARD_PLUS, false },
		{ TTRC("Decal Cluster"), Viewport::DEBUG_DRAW_CLUSTER_DECALS, FORWARD_PLUS, false },
		{ TTRC("ReflectionProbe Cluster"), Viewport::DEBUG_DRAW_CLUSTER_REFLECTION_PROBES, FORWARD_PLUS, false },
		{ TTRC("Occlusion Culling Buffer"), Viewport::DEBUG_DRAW_OCCLUDERS, FORWARD_PLUS_MOBILE, false },
		{ TTRC("Motion Vectors"), Viewport::DEBUG_DRAW_MOTION_VECTORS, FORWARD_PLUS, false },
		{ TTRC("Internal Buffer"), Viewport::DEBUG_DRAW_INTERNAL_BUFFER, FORWARD_PLUS_MOBILE, false },
	};
	const String method = RenderingServer::get_singleton()->get_current_rendering_method();
	PopupMenu *advanced = preview_pane->view_advanced_menu;
	for (const AdvancedMode &mode : advanced_modes) {
		if (mode.separator_before) {
			advanced->add_separator();
		}
		advanced->add_radio_check_item(mode.name, PANE_VIEW_DISPLAY_BASE + (int)mode.mode);
		const bool supported = mode.renderers == ALL || method == "forward_plus" || (mode.renderers == FORWARD_PLUS_MOBILE && method == "mobile");
		if (!supported) {
			advanced->set_item_disabled(-1, true);
			advanced->set_item_tooltip(-1, TTR("Not available when using the Compatibility renderer."));
		}
	}
	p->add_submenu_node_item(TTRC("Display Advanced..."), advanced, PANE_VIEW_DISPLAY_ADVANCED);

	p->add_separator();
	p->add_check_shortcut(ED_GET_SHORTCUT("spatial_editor/view_environment"), PANE_VIEW_ENVIRONMENT);
	p->add_check_shortcut(ED_GET_SHORTCUT("spatial_editor/view_gizmos"), PANE_VIEW_GIZMOS);
	p->add_check_shortcut(ED_GET_SHORTCUT("spatial_editor/view_transform_gizmo"), PANE_VIEW_TRANSFORM_GIZMO);
	p->add_check_shortcut(ED_GET_SHORTCUT("spatial_editor/view_grid_lines"), PANE_VIEW_GRID);
	p->add_check_shortcut(ED_GET_SHORTCUT("spatial_editor/view_information"), PANE_VIEW_INFORMATION);
	p->add_check_shortcut(ED_GET_SHORTCUT("spatial_editor/view_fps"), PANE_VIEW_FRAME_TIME);
	p->set_item_checked(p->get_item_index(PANE_VIEW_ENVIRONMENT), true);
	p->set_item_checked(p->get_item_index(PANE_VIEW_GIZMOS), true);
	p->set_item_checked(p->get_item_index(PANE_VIEW_TRANSFORM_GIZMO), true);
	p->set_item_checked(p->get_item_index(PANE_VIEW_GRID), true);
	p->add_separator();
	p->add_check_shortcut(ED_GET_SHORTCUT("spatial_editor/view_half_resolution"), PANE_VIEW_HALF_RESOLUTION);
	p->add_separator();
	p->add_check_shortcut(ED_GET_SHORTCUT("spatial_editor/view_audio_listener"), PANE_VIEW_AUDIO_LISTENER);
	p->add_check_shortcut(ED_GET_SHORTCUT("spatial_editor/view_audio_doppler"), PANE_VIEW_AUDIO_DOPPLER);
	p->add_separator();
	p->add_check_shortcut(ED_GET_SHORTCUT("spatial_editor/view_cinematic_preview"), PANE_VIEW_CINEMATIC_PREVIEW);
	p->add_separator();
	p->add_shortcut(ED_GET_SHORTCUT("spatial_editor/focus_origin"), PANE_VIEW_CENTER_TO_ORIGIN);
	p->add_shortcut(ED_GET_SHORTCUT("spatial_editor/focus_selection"), PANE_VIEW_CENTER_TO_SELECTION);
	p->add_shortcut(ED_GET_SHORTCUT("spatial_editor/focus_aabb"), PANE_VIEW_CENTER_TO_AABB);
	p->add_shortcut(ED_GET_SHORTCUT("spatial_editor/align_transform_with_view"), PANE_VIEW_ALIGN_TRANSFORM_WITH_VIEW);
	p->add_shortcut(ED_GET_SHORTCUT("spatial_editor/align_rotation_with_view"), PANE_VIEW_ALIGN_ROTATION_WITH_VIEW);

	p->connect(SceneStringName(id_pressed), callable_mp(this, &EditorSplitView::_view_menu_option));
	advanced->connect(SceneStringName(id_pressed), callable_mp(this, &EditorSplitView::_view_menu_option));
	p->connect("about_to_popup", callable_mp(this, &EditorSplitView::_set_preview_active).bind(true));
}

bool EditorSplitView::_view_menu_checked(int p_option) const {
	const PopupMenu *p = preview_pane->view_menu->get_popup();
	int idx = p->get_item_index(p_option);
	if (idx >= 0) {
		return p->is_item_checked(idx);
	}
	idx = preview_pane->view_advanced_menu->get_item_index(p_option);
	return idx >= 0 && preview_pane->view_advanced_menu->is_item_checked(idx);
}

void EditorSplitView::_set_view_menu_checked(int p_option, bool p_checked) {
	PopupMenu *p = preview_pane->view_menu->get_popup();
	int idx = p->get_item_index(p_option);
	if (idx >= 0) {
		p->set_item_checked(idx, p_checked);
	}
	idx = preview_pane->view_advanced_menu->get_item_index(p_option);
	if (idx >= 0) {
		preview_pane->view_advanced_menu->set_item_checked(idx, p_checked);
	}
}

bool EditorSplitView::_view_menu_shortcut(const Ref<InputEvent> &p_event) {
	// The 3D editor's viewport shortcuts, while the pane has focus.
	const Ref<InputEventKey> k = p_event;
	if (k.is_null() || !k->is_pressed() || k->is_echo()) {
		return false;
	}
	const PopupMenu *p = preview_pane->view_menu->get_popup();
	for (int i = 0; i < p->get_item_count(); i++) {
		const Ref<Shortcut> shortcut = p->get_item_shortcut(i);
		if (shortcut.is_valid() && !p->is_item_disabled(i) && shortcut->matches_event(p_event)) {
			_view_menu_option(p->get_item_id(i));
			return true;
		}
	}
	return false;
}

void EditorSplitView::_build_view_menu_2d() {
	// The 2D editor's View menu, with the items that change what the pane
	// shows. The 2D editor's own preview settings (theme, translation, canvas
	// scale and resampling) apply to the whole project, so they stay there.
	PopupMenu *grid = preview_pane->view_grid_menu_2d;
	grid->add_radio_check_item(TTRC("Show"), PANE_2D_GRID_SHOW);
	grid->add_radio_check_item(TTRC("Show When Snapping"), PANE_2D_GRID_SHOW_WHEN_SNAPPING);
	grid->add_radio_check_item(TTRC("Hide"), PANE_2D_GRID_HIDE);
	grid->add_separator();
	grid->add_shortcut(ED_GET_SHORTCUT("canvas_item_editor/toggle_grid"), PANE_2D_GRID_TOGGLE);
	grid->set_item_checked(grid->get_item_index(PANE_2D_GRID_SHOW_WHEN_SNAPPING), true);
	grid->connect(SceneStringName(id_pressed), callable_mp(this, &EditorSplitView::_view_menu_2d_option));

	PopupMenu *p = preview_pane->view_menu_2d->get_popup();
	p->add_submenu_node_item(TTRC("Grid"), grid, PANE_2D_GRID_MENU);
	p->add_check_shortcut(ED_GET_SHORTCUT("canvas_item_editor/show_rulers"), PANE_2D_SHOW_RULERS);
	// Y toggles the pane's snapping, as in 3D, so this has no shortcut here.
	p->add_check_item(TTRC("Show Guides"), PANE_2D_SHOW_GUIDES);
	p->add_check_shortcut(ED_GET_SHORTCUT("canvas_item_editor/show_origin"), PANE_2D_SHOW_ORIGIN);
	p->add_check_shortcut(ED_GET_SHORTCUT("canvas_item_editor/show_viewport"), PANE_2D_SHOW_VIEWPORT);
	p->add_check_shortcut(ED_GET_SHORTCUT("canvas_item_editor/show_information"), PANE_2D_SHOW_INFORMATION);
	p->add_check_shortcut(ED_GET_SHORTCUT("canvas_item_editor/show_frame_time"), PANE_2D_SHOW_FRAME_TIME);
	p->add_separator();

	PopupMenu *gizmos = preview_pane->view_gizmos_menu_2d;
	gizmos->add_check_shortcut(ED_GET_SHORTCUT("canvas_item_editor/show_position_gizmos"), PANE_2D_GIZMO_POSITION);
	gizmos->add_check_shortcut(ED_GET_SHORTCUT("canvas_item_editor/show_lock_gizmos"), PANE_2D_GIZMO_LOCK);
	gizmos->add_check_shortcut(ED_GET_SHORTCUT("canvas_item_editor/show_group_gizmos"), PANE_2D_GIZMO_GROUP);
	gizmos->add_check_shortcut(ED_GET_SHORTCUT("canvas_item_editor/show_transformation_gizmos"), PANE_2D_GIZMO_TRANSFORMATION);
	gizmos->connect(SceneStringName(id_pressed), callable_mp(this, &EditorSplitView::_view_menu_2d_option));
	p->add_submenu_node_item(TTRC("Gizmos"), gizmos, PANE_2D_GIZMOS_MENU);
	p->add_separator();
	p->add_shortcut(ED_GET_SHORTCUT("canvas_item_editor/center_selection"), PANE_2D_CENTER_SELECTION);
	p->add_shortcut(ED_GET_SHORTCUT("canvas_item_editor/frame_selection"), PANE_2D_FRAME_SELECTION);
	p->add_shortcut(ED_GET_SHORTCUT("canvas_item_editor/clear_guides"), PANE_2D_CLEAR_GUIDES);

	// As the 2D editor starts.
	static const int shown[] = { PANE_2D_SHOW_RULERS, PANE_2D_SHOW_GUIDES, PANE_2D_SHOW_ORIGIN, PANE_2D_SHOW_VIEWPORT, PANE_2D_GIZMO_POSITION, PANE_2D_GIZMO_LOCK, PANE_2D_GIZMO_GROUP, PANE_2D_GIZMO_TRANSFORMATION };
	for (int option : shown) {
		_set_view_menu_2d_checked(option, true);
	}
	p->connect(SceneStringName(id_pressed), callable_mp(this, &EditorSplitView::_view_menu_2d_option));
	p->connect("about_to_popup", callable_mp(this, &EditorSplitView::_set_preview_active).bind(true));
}

bool EditorSplitView::_view_menu_2d_checked(int p_option) const {
	const PopupMenu *menus[] = { preview_pane->view_menu_2d->get_popup(), preview_pane->view_grid_menu_2d, preview_pane->view_gizmos_menu_2d };
	for (const PopupMenu *menu : menus) {
		const int idx = menu->get_item_index(p_option);
		if (idx >= 0) {
			return menu->is_item_checked(idx);
		}
	}
	return false;
}

void EditorSplitView::_set_view_menu_2d_checked(int p_option, bool p_checked) {
	PopupMenu *menus[] = { preview_pane->view_menu_2d->get_popup(), preview_pane->view_grid_menu_2d, preview_pane->view_gizmos_menu_2d };
	for (PopupMenu *menu : menus) {
		const int idx = menu->get_item_index(p_option);
		if (idx >= 0) {
			menu->set_item_checked(idx, p_checked);
			return;
		}
	}
}

bool EditorSplitView::_is_grid_2d_visible() const {
	if (_view_menu_2d_checked(PANE_2D_GRID_SHOW)) {
		return true;
	}
	return _view_menu_2d_checked(PANE_2D_GRID_SHOW_WHEN_SNAPPING) && preview_pane->tools->is_snap_enabled();
}

void EditorSplitView::_view_menu_2d_option(int p_option) {
	_set_preview_active(true);
	switch (p_option) {
		case PANE_2D_GRID_SHOW:
		case PANE_2D_GRID_SHOW_WHEN_SNAPPING:
		case PANE_2D_GRID_HIDE: {
			for (int option = PANE_2D_GRID_SHOW; option <= PANE_2D_GRID_HIDE; option++) {
				_set_view_menu_2d_checked(option, option == p_option);
			}
		} break;
		case PANE_2D_GRID_TOGGLE: {
			// As in the 2D editor: shown or hidden, whatever snapping does.
			const bool grid_shown = _is_grid_2d_visible();
			for (int option = PANE_2D_GRID_SHOW; option <= PANE_2D_GRID_HIDE; option++) {
				_set_view_menu_2d_checked(option, option == (grid_shown ? PANE_2D_GRID_HIDE : PANE_2D_GRID_SHOW));
			}
		} break;
		case PANE_2D_SHOW_RULERS:
		case PANE_2D_SHOW_GUIDES:
		case PANE_2D_SHOW_ORIGIN:
		case PANE_2D_SHOW_VIEWPORT:
		case PANE_2D_SHOW_INFORMATION:
		case PANE_2D_SHOW_FRAME_TIME:
		case PANE_2D_GIZMO_POSITION:
		case PANE_2D_GIZMO_LOCK:
		case PANE_2D_GIZMO_GROUP:
		case PANE_2D_GIZMO_TRANSFORMATION: {
			_set_view_menu_2d_checked(p_option, !_view_menu_2d_checked(p_option));
		} break;
		case PANE_2D_CENTER_SELECTION: {
			_focus_preview_2d(false);
		} break;
		case PANE_2D_FRAME_SELECTION: {
			_focus_preview_2d(true);
		} break;
		case PANE_2D_CLEAR_GUIDES: {
			Node *root = _pane_scene();
			const int history_id = preview_pane->tools->get_history_id();
			if (!root || history_id == EditorUndoRedoManager::INVALID_HISTORY || (!root->has_meta("_edit_horizontal_guides_") && !root->has_meta("_edit_vertical_guides_"))) {
				break;
			}
			// As the 2D editor clears them, in the scene's own history.
			EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
			undo_redo->create_action_for_history(TTR("Clear Guides"), history_id);
			undo_redo->force_fixed_history();
			static const char *metas[] = { "_edit_horizontal_guides_", "_edit_vertical_guides_" };
			for (const char *meta : metas) {
				if (root->has_meta(meta)) {
					undo_redo->add_do_method(root, "remove_meta", meta);
					undo_redo->add_undo_method(root, "set_meta", meta, root->get_meta(meta));
				}
			}
			Control *overlay = preview_pane->overlay;
			undo_redo->add_do_method(overlay, "queue_redraw");
			undo_redo->add_undo_method(overlay, "queue_redraw");
			undo_redo->commit_action();
		} break;
	}
	_update_view_menu();
	preview_pane->overlay->queue_redraw();
}

bool EditorSplitView::_view_menu_2d_shortcut(const Ref<InputEvent> &p_event) {
	// The 2D editor's View shortcuts, while the pane has focus.
	const Ref<InputEventKey> k = p_event;
	if (k.is_null() || !k->is_pressed() || k->is_echo()) {
		return false;
	}
	const PopupMenu *menus[] = { preview_pane->view_menu_2d->get_popup(), preview_pane->view_grid_menu_2d, preview_pane->view_gizmos_menu_2d };
	for (const PopupMenu *menu : menus) {
		for (int i = 0; i < menu->get_item_count(); i++) {
			const Ref<Shortcut> shortcut = menu->get_item_shortcut(i);
			if (shortcut.is_valid() && !menu->is_item_disabled(i) && shortcut->matches_event(p_event)) {
				_view_menu_2d_option(menu->get_item_id(i));
				return true;
			}
		}
	}
	return false;
}

static Rect2 _split_view_canvas_rect(CanvasItem *p_item) {
	// Where a canvas item is, in its canvas: its rectangle, or its position.
	const Transform2D xform = p_item->get_global_transform();
	if (p_item->_edit_use_rect()) {
		return xform.xform(p_item->_edit_get_rect());
	}
	return Rect2(xform.get_origin(), Size2());
}

static void _split_view_merge_canvas_rects(Node *p_node, Rect2 &r_rect, bool &r_has_rect) {
	CanvasItem *item = Object::cast_to<CanvasItem>(p_node);
	if (item && !item->is_visible_in_tree()) {
		return;
	}
	if (item) {
		const Rect2 rect = _split_view_canvas_rect(item);
		r_rect = r_has_rect ? r_rect.merge(rect) : rect;
		r_has_rect = true;
	}
	for (Node *child : p_node->iterate_children()) {
		_split_view_merge_canvas_rects(child, r_rect, r_has_rect);
	}
}

void EditorSplitView::_focus_preview_2d(bool p_frame) {
	Node *root = _pane_scene();
	if (!root || Object::cast_to<Node3D>(root)) {
		return;
	}
	_begin_custom_view();
	CanvasItem *selected = Object::cast_to<CanvasItem>(preview_pane->tools->get_selected());
	Rect2 rect;
	bool has_rect = false;
	if (selected) {
		rect = _split_view_canvas_rect(selected);
		has_rect = true;
	} else if (p_frame) {
		_split_view_merge_canvas_rects(root, rect, has_rect);
	}
	if (!has_rect) {
		return;
	}
	const Size2 size = preview_pane->viewport_container->get_size();
	real_t zoom = preview_view_2d.get_scale().x;
	if (p_frame && rect.has_area()) {
		// As the 2D editor frames: the selection fills most of the view.
		zoom = MIN(size.x / rect.size.x, size.y / rect.size.y) * 0.9;
	}
	Transform2D xform;
	xform.scale_basis(Size2(zoom, zoom));
	xform.columns[2] = size / 2 - rect.get_center() * zoom;
	preview_view_2d = xform;
	_update_preview_camera();
}

void EditorSplitView::_draw_preview_2d_under() {
	// Under the selection, as the 2D editor draws them: the grid, the
	// origin, the project's window, locks, groups and node positions.
	Control *overlay = preview_pane->overlay;
	Node *root = _pane_scene();
	const Transform2D xform = preview_pane->viewport->get_global_canvas_transform();
	const Size2 size = overlay->get_size();
	if (_is_grid_2d_visible()) {
		// The 2D editor's grid, with its step and offset.
		const Ref<CanvasItemManipulator> manipulator = CanvasItemEditor::get_singleton()->get_canvas_item_manipulator();
		const Dictionary state = CanvasItemEditor::get_singleton()->get_state();
		const Vector2i primary_grid_step = state.get("primary_grid_step", Vector2i(8, 8));
		const Point2 step = manipulator->get_grid_step() * Math::pow(2.0, manipulator->get_grid_step_multiplier());
		const Point2 offset = manipulator->get_grid_offset();
		const Color secondary_grid_color = EDITOR_GET("editors/2d/grid_color");
		const Color primary_grid_color = Color(secondary_grid_color.r, secondary_grid_color.g, secondary_grid_color.b, secondary_grid_color.a * 2.5);
		const Transform2D inverse = xform.affine_inverse();
		for (int axis = 0; axis < 2; axis++) {
			if (step[axis] <= 0) {
				continue;
			}
			const int length = axis == 0 ? size.width : size.height;
			int last_cell = 0;
			for (int i = 0; i < length; i++) {
				const Vector2 at = axis == 0 ? Vector2(i, 0) : Vector2(0, i);
				const int cell = Math::fast_ftoi(Math::floor((inverse.xform(at)[axis] - offset[axis]) / step[axis]));
				if (i > 0 && cell != last_cell) {
					const Color color = primary_grid_step[axis] > 1 && cell % primary_grid_step[axis] == 0 ? primary_grid_color : secondary_grid_color;
					overlay->draw_line(at, axis == 0 ? Vector2(i, size.height) : Vector2(size.width, i), color, Math::round(EDSCALE));
				}
				last_cell = cell;
			}
		}
	}
	if (_view_menu_2d_checked(PANE_2D_SHOW_ORIGIN)) {
		const Point2 origin = xform.get_origin();
		overlay->draw_line(Point2(0, origin.y), Point2(size.width, origin.y), get_theme_color(SNAME("axis_x_color"), EditorStringName(Editor)) * Color(1, 1, 1, 0.75));
		overlay->draw_line(Point2(origin.x, 0), Point2(origin.x, size.height), get_theme_color(SNAME("axis_y_color"), EditorStringName(Editor)) * Color(1, 1, 1, 0.75));
	}
	if (_view_menu_2d_checked(PANE_2D_SHOW_VIEWPORT)) {
		const Size2 project_size = Size2(GLOBAL_GET("display/window/size/viewport_width"), GLOBAL_GET("display/window/size/viewport_height"));
		const Color border_color = EDITOR_GET("editors/2d/viewport_border_color");
		overlay->draw_rect(xform.xform(Rect2(Point2(), project_size)), border_color, false);
	}
	if (root && (_view_menu_2d_checked(PANE_2D_GIZMO_LOCK) || _view_menu_2d_checked(PANE_2D_GIZMO_GROUP) || _view_menu_2d_checked(PANE_2D_GIZMO_POSITION))) {
		_draw_locks_and_positions_2d(root, xform, root);
	}
}

void EditorSplitView::_draw_locks_and_positions_2d(Node *p_node, const Transform2D &p_xform, Node *p_root) {
	if (p_node != p_root && p_node->get_owner() != p_root && !p_root->is_editable_instance(p_node->get_owner())) {
		return;
	}
	CanvasItem *item = Object::cast_to<CanvasItem>(p_node);
	if (item && !item->is_visible_in_tree()) {
		return;
	}
	for (Node *child : p_node->iterate_children()) {
		_draw_locks_and_positions_2d(child, p_xform, p_root);
	}
	if (!item) {
		return;
	}
	Control *overlay = preview_pane->overlay;
	const Point2 at = p_xform.xform(item->get_global_transform().get_origin());
	real_t offset = 0;
	if (_view_menu_2d_checked(PANE_2D_GIZMO_LOCK) && p_node->has_meta("_edit_lock_")) {
		const Ref<Texture2D> lock = get_editor_theme_icon(SNAME("LockViewport"));
		overlay->draw_texture(lock, at + Point2(offset, 0));
		offset += lock->get_size().x;
	}
	if (_view_menu_2d_checked(PANE_2D_GIZMO_GROUP) && item->has_meta("_edit_group_")) {
		overlay->draw_texture(get_editor_theme_icon(SNAME("GroupViewport")), at + Point2(offset, 0));
	}
	// Where nodes without a rectangle are, unless they are selected.
	if (_view_menu_2d_checked(PANE_2D_GIZMO_POSITION) && !item->_edit_use_rect() && item != preview_pane->tools->get_selected()) {
		const Ref<Texture2D> position = get_editor_theme_icon(SNAME("EditorPositionUnselected"));
		overlay->draw_texture(position, at - position->get_size() / 2, Color(1, 1, 1, 0.5));
	}
}

void EditorSplitView::_draw_preview_2d_over() {
	// Over the selection, as the 2D editor draws them: guides and rulers.
	Control *overlay = preview_pane->overlay;
	Node *root = _pane_scene();
	const Transform2D xform = preview_pane->viewport->get_global_canvas_transform();
	const Size2 size = overlay->get_size();
	if (root && _view_menu_2d_checked(PANE_2D_SHOW_GUIDES)) {
		const Color guide_color = EDITOR_GET("editors/2d/guides_color");
		const Array vguides = root->get_meta("_edit_vertical_guides_", Array());
		for (int i = 0; i < vguides.size(); i++) {
			const real_t x = xform.xform(Point2(vguides[i], 0)).x;
			overlay->draw_line(Point2(x, 0), Point2(x, size.height), guide_color, Math::round(EDSCALE));
		}
		const Array hguides = root->get_meta("_edit_horizontal_guides_", Array());
		for (int i = 0; i < hguides.size(); i++) {
			const real_t y = xform.xform(Point2(0, hguides[i])).y;
			overlay->draw_line(Point2(0, y), Point2(size.width, y), guide_color, Math::round(EDSCALE));
		}
	}
	if (!_view_menu_2d_checked(PANE_2D_SHOW_RULERS)) {
		return;
	}
	// The 2D editor's rulers, along the top and left.
	const Color bg_color = get_theme_color(SNAME("ruler_color"), EditorStringName(Editor));
	const Color graduation_color = get_theme_color(SceneStringName(font_color), EditorStringName(Editor)).lerp(bg_color, 0.5);
	Color font_color = get_theme_color(SceneStringName(font_color), EditorStringName(Editor));
	font_color.a = 0.9;
	const Ref<Font> font = get_theme_font(SNAME("rulers"), EditorStringName(EditorFonts));
	const int font_size = get_theme_font_size(SNAME("rulers_size"), EditorStringName(EditorFonts));
	const real_t ruler_width = CanvasItemEditor::get_singleton()->get_canvas_item_manipulator()->get_ruler_width();
	const real_t tick_scale = ruler_width / 15.0;
	const real_t zoom = xform.get_scale().x;
	real_t rule = 100;
	for (int i = 0; rule * zoom > 100 * tick_scale; i++) {
		rule /= (i % 2) ? 5.0 : 2.0;
	}
	for (int i = 0; rule * zoom < 60 * tick_scale; i++) {
		rule *= (i % 2) ? 2.0 : 5.0;
	}
	Transform2D ruler_transform;
	ruler_transform.scale(Size2(rule, rule));
	const int major = 2;
	const int minor = 5;
	Transform2D subdivide;
	subdivide.scale(Size2(1.0 / (major * minor), 1.0 / (major * minor)));
	const Transform2D to_screen = xform * ruler_transform * subdivide;
	const Point2 first = to_screen.affine_inverse().xform(Point2(ruler_width, ruler_width));
	const Point2 last = to_screen.affine_inverse().xform(size);
	overlay->draw_rect(Rect2(Point2(ruler_width, 0), Size2(size.width, ruler_width)), bg_color);
	for (int i = Math::ceil(first.x); i < last.x; i++) {
		const Point2 position = to_screen.xform(Point2(i, 0)).round();
		if (i % (major * minor) == 0) {
			overlay->draw_line(Point2(position.x, 0), Point2(position.x, ruler_width), graduation_color, Math::round(EDSCALE));
			const real_t value = (ruler_transform * subdivide).xform(Point2(i, 0)).x;
			overlay->draw_string(font, Point2(position.x + MAX(Math::round(font_size / 8.0), 2), font->get_ascent(font_size) + Math::round(EDSCALE)), vformat(((int)value == value) ? "%d" : "%.1f", value), HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, font_color);
		} else {
			overlay->draw_line(Point2(position.x, ruler_width * (i % minor == 0 ? 0.33 : 0.75)), Point2(position.x, ruler_width), graduation_color, Math::round(EDSCALE));
		}
	}
	overlay->draw_rect(Rect2(Point2(0, ruler_width), Size2(ruler_width, size.height)), bg_color);
	for (int i = Math::ceil(first.y); i < last.y; i++) {
		const Point2 position = to_screen.xform(Point2(0, i)).round();
		if (i % (major * minor) == 0) {
			overlay->draw_line(Point2(0, position.y), Point2(ruler_width, position.y), graduation_color, Math::round(EDSCALE));
			const real_t value = (ruler_transform * subdivide).xform(Point2(0, i)).y;
			overlay->draw_set_transform_matrix(Transform2D(-Math::PI / 2.0, Point2(font->get_ascent(font_size) + Math::round(EDSCALE), position.y - 2)));
			overlay->draw_string(font, Point2(), vformat(((int)value == value) ? "%d" : "%.1f", value), HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, font_color);
			overlay->draw_set_transform_matrix(Transform2D());
		} else {
			overlay->draw_line(Point2(ruler_width * (i % minor == 0 ? 0.33 : 0.75), position.y), Point2(ruler_width, position.y), graduation_color, Math::round(EDSCALE));
		}
	}
	overlay->draw_rect(Rect2(Point2(), Size2(ruler_width, ruler_width)), graduation_color);
}

void EditorSplitView::_orbit_preview(const Vector2 &p_relative) {
	if (_view_menu_checked(PANE_VIEW_LOCK_ROTATION)) {
		return;
	}
	// Orbit, out of a side view.
	_begin_custom_view();
	_end_side_view();
	preview_orbit.x_rot = CLAMP(preview_orbit.x_rot + p_relative.y * 0.005, -Math::PI / 2, Math::PI / 2);
	preview_orbit.y_rot += p_relative.x * 0.005;
	_update_preview_camera();
}

Basis EditorSplitView::_orbit_basis(const OrbitView &p_orbit) {
	// As _apply_orbit() turns the camera.
	Basis basis;
	basis.rotate(Vector3(1, 0, 0), -p_orbit.x_rot);
	basis.rotate(Vector3(0, 1, 0), -p_orbit.y_rot);
	return basis;
}

bool EditorSplitView::_is_shortcut_held(const String &p_name) {
	const Ref<Shortcut> shortcut = ED_GET_SHORTCUT(p_name);
	if (shortcut.is_null()) {
		return false;
	}
	for (const Ref<InputEventKey> k : shortcut->get_events()) {
		if (k.is_valid() && (Input::get_singleton()->is_key_pressed(k->get_keycode()) || Input::get_singleton()->is_physical_key_pressed(k->get_physical_keycode()))) {
			return true;
		}
	}
	return false;
}

void EditorSplitView::_set_freelook(bool p_enabled, const Point2 &p_mouse_position) {
	if (preview_freelook == p_enabled) {
		return;
	}
	preview_freelook = p_enabled;
	if (p_enabled) {
		_set_preview_active(true);
		preview_pane->viewport_container->grab_focus();
		// Looking around from where the camera is, in perspective.
		_begin_custom_view();
		_end_side_view();
		if (preview_orbit.orthogonal) {
			preview_orbit.orthogonal = false;
			_update_preview_camera();
		}
		if (!bool(EDITOR_GET("editors/3d/freelook/freelook_speed_zoom_link"))) {
			freelook_speed = MAX((real_t)EDITOR_GET("editors/3d/freelook/freelook_base_speed"), (real_t)0.01);
		} else {
			freelook_speed = MAX((real_t)EDITOR_GET("editors/3d/freelook/freelook_base_speed") * preview_orbit.distance, (real_t)0.01);
		}
		freelook_mouse_start = p_mouse_position;
		Input::get_singleton()->set_mouse_mode(Input::MouseMode::MOUSE_MODE_CAPTURED);
	} else {
		Input::get_singleton()->set_mouse_mode(Input::MouseMode::MOUSE_MODE_VISIBLE);
		Input::get_singleton()->warp_mouse(freelook_mouse_start);
		zoom_indicator_delay = 0.0;
		preview_pane->overlay->queue_redraw();
	}
	_update_view_menu();
}

void EditorSplitView::_freelook_look(const Vector2 &p_relative) {
	// Turn around the camera, not around what it orbits.
	const Vector3 eye = preview_orbit.target + _orbit_basis(preview_orbit).xform(Vector3(0, 0, preview_orbit.distance));
	const real_t radians_per_pixel = Math::deg_to_rad((real_t)EDITOR_GET("editors/3d/freelook/freelook_sensitivity"));
	const bool invert_y = EDITOR_GET("editors/3d/freelook/freelook_invert_y_axis");
	preview_orbit.x_rot = CLAMP(preview_orbit.x_rot + p_relative.y * radians_per_pixel * (invert_y ? -1 : 1), -Math::PI / 2, Math::PI / 2);
	preview_orbit.y_rot += p_relative.x * radians_per_pixel;
	preview_orbit.target = eye - _orbit_basis(preview_orbit).xform(Vector3(0, 0, preview_orbit.distance));
	_update_preview_camera();
}

void EditorSplitView::_freelook_move(double p_delta) {
	const Basis basis = _orbit_basis(preview_orbit);
	Vector3 forward = basis.xform(Vector3(0, 0, -1));
	Vector3 up = basis.xform(Vector3(0, 1, 0));
	if (int(EDITOR_GET("editors/3d/freelook/freelook_navigation_scheme")) != 0) {
		// The axis-locked schemes: level forward, and straight up.
		forward = Vector3(0, 0, -1).rotated(Vector3(0, 1, 0), -preview_orbit.y_rot);
		up = Vector3(0, 1, 0);
	}
	const Vector3 right = basis.xform(Vector3(1, 0, 0));
	Vector3 direction;
	if (_is_shortcut_held("spatial_editor/freelook_left")) {
		direction -= right;
	}
	if (_is_shortcut_held("spatial_editor/freelook_right")) {
		direction += right;
	}
	if (_is_shortcut_held("spatial_editor/freelook_forward")) {
		direction += forward;
	}
	if (_is_shortcut_held("spatial_editor/freelook_backwards")) {
		direction -= forward;
	}
	if (_is_shortcut_held("spatial_editor/freelook_up")) {
		direction += up;
	}
	if (_is_shortcut_held("spatial_editor/freelook_down")) {
		direction -= up;
	}
	if (direction.is_zero_approx()) {
		return;
	}
	real_t speed = freelook_speed;
	if (_is_shortcut_held("spatial_editor/freelook_speed_modifier")) {
		speed *= 3.0;
	}
	if (_is_shortcut_held("spatial_editor/freelook_slow_modifier")) {
		speed *= 0.333333;
	}
	preview_orbit.target += direction * speed * p_delta;
	_update_preview_camera();
}

void EditorSplitView::_rotation_axis_clicked(int p_axis) {
	// As the 3D editor's gizmo: look from the axis clicked.
	static const PaneViewMenu views[6] = { PANE_VIEW_RIGHT, PANE_VIEW_TOP, PANE_VIEW_FRONT, PANE_VIEW_LEFT, PANE_VIEW_BOTTOM, PANE_VIEW_REAR };
	ERR_FAIL_INDEX(p_axis, 6);
	_view_menu_option(views[p_axis]);
}

void EditorSplitView::_end_side_view() {
	if (preview_view_type == PANE_VIEW_TYPE_USER && !preview_auto_orthogonal) {
		return;
	}
	preview_view_type = PANE_VIEW_TYPE_USER;
	if (preview_auto_orthogonal) {
		// Made orthogonal by the side view: back to perspective.
		preview_auto_orthogonal = false;
		preview_orbit.orthogonal = false;
	}
	_update_view_menu();
}

Camera3D *EditorSplitView::_find_scene_camera(Node *p_node) const {
	// The scene's current camera, or its first one.
	Camera3D *first = nullptr;
	List<Node *> pending;
	pending.push_back(p_node);
	while (!pending.is_empty()) {
		Node *node = pending.front()->get();
		pending.pop_front();
		if (Camera3D *camera = Object::cast_to<Camera3D>(node)) {
			if (camera->is_current()) {
				return camera;
			}
			if (!first) {
				first = camera;
			}
		}
		for (Node *child : node->iterate_children()) {
			pending.push_back(child);
		}
	}
	return first;
}

void EditorSplitView::_set_cinematic_preview(bool p_enabled) {
	preview_cinematic = p_enabled;
	_set_view_menu_checked(PANE_VIEW_CINEMATIC_PREVIEW, p_enabled);
	if (!p_enabled) {
		preview_cinematic_camera = ObjectID();
		RenderingServer::get_singleton()->viewport_attach_camera(preview_pane->viewport->get_viewport_rid(), preview_pane->camera_3d->get_camera());
	}
	_update_view_menu();
	_update_view_panels();
}

void EditorSplitView::_focus_preview_bounds(bool p_frame) {
	Node *root = _pane_scene();
	if (!root) {
		return;
	}
	_begin_custom_view();
	Node *selected = preview_pane->tools->get_selected();
	Node3D *selected_3d = Object::cast_to<Node3D>(selected);
	if (!p_frame) {
		// Focus Selection: look at it from where the view is.
		if (selected_3d) {
			preview_orbit.target = selected_3d->get_global_position();
		}
	} else {
		// Focus and Frame: everything it shows, or the whole scene.
		AABB bounds;
		bool has_bounds = false;
		_collect_bounds(selected ? selected : root, bounds, has_bounds);
		if (has_bounds) {
			preview_orbit.target = bounds.get_center();
			preview_orbit.distance = MAX(bounds.get_longest_axis_size(), (real_t)0.1) * 1.1;
		} else if (selected_3d) {
			preview_orbit.target = selected_3d->get_global_position();
		}
	}
	_update_preview_camera();
}

void EditorSplitView::_align_selection_with_view(bool p_rotation_only) {
	Node3D *node = Object::cast_to<Node3D>(preview_pane->tools->get_selected());
	const int history_id = preview_pane->tools->get_history_id();
	if (!node || history_id == EditorUndoRedoManager::INVALID_HISTORY) {
		return;
	}
	const Transform3D camera_xform = preview_pane->camera_3d->get_global_transform();
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	if (p_rotation_only) {
		Basis basis = camera_xform.basis;
		if (Object::cast_to<Decal>(node)) {
			// As in the 3D editor: a decal looks the way the camera does.
			basis.rotate_local(Vector3(1, 0, 0), Math::TAU * 0.25);
		}
		undo_redo->create_action_for_history(TTR("Align Rotation with View"), history_id);
		undo_redo->force_fixed_history();
		undo_redo->add_do_method(node, "set_rotation", basis.get_euler_normalized());
		undo_redo->add_undo_method(node, "set_rotation", node->get_rotation());
		undo_redo->commit_action();
		return;
	}
	Transform3D xform = camera_xform;
	if (preview_orbit.orthogonal) {
		xform.origin = preview_orbit.target + camera_xform.basis.xform(Vector3(0, 0, preview_orbit.distance));
	} else {
		xform.scale_basis(node->get_scale());
	}
	if (Object::cast_to<Decal>(node)) {
		xform.basis.rotate_local(Vector3(1, 0, 0), Math::TAU * 0.25);
	}
	Node3D *parent = node->get_parent_node_3d();
	const Transform3D local_xform = parent ? parent->get_global_transform().affine_inverse() * xform : xform;
	undo_redo->create_action_for_history(TTR("Align Transform with View"), history_id);
	undo_redo->force_fixed_history();
	undo_redo->add_do_method(node, "set_transform", local_xform);
	undo_redo->add_undo_method(node, "set_transform", node->get_transform());
	undo_redo->commit_action();
}

void EditorSplitView::_view_menu_option(int p_option) {
	_set_preview_active(true);
	static const real_t side_views[6][2] = {
		// x_rot, y_rot.
		{ Math::PI / 2.0, 0 }, // Top.
		{ -Math::PI / 2.0, 0 }, // Bottom.
		{ 0, Math::PI / 2.0 }, // Left.
		{ 0, -Math::PI / 2.0 }, // Right.
		{ 0, 0 }, // Front.
		{ 0, Math::PI }, // Rear.
	};
	if (p_option >= PANE_VIEW_DISPLAY_BASE) {
		const Viewport::DebugDraw mode = Viewport::DebugDraw(p_option - PANE_VIEW_DISPLAY_BASE);
		for (int i = 0; i <= Viewport::DEBUG_DRAW_AREA_LIGHT_ATLAS; i++) {
			_set_view_menu_checked(PANE_VIEW_DISPLAY_BASE + i, i == mode);
		}
		if (mode == Viewport::DEBUG_DRAW_WIREFRAME) {
			RenderingServer::get_singleton()->set_debug_generate_wireframes(true);
		}
		preview_pane->viewport->set_debug_draw(mode);
		return;
	}
	switch (p_option) {
		case PANE_VIEW_TOP:
		case PANE_VIEW_BOTTOM:
		case PANE_VIEW_LEFT:
		case PANE_VIEW_RIGHT:
		case PANE_VIEW_FRONT:
		case PANE_VIEW_REAR: {
			_begin_custom_view();
			preview_orbit.x_rot = side_views[p_option - PANE_VIEW_TOP][0];
			preview_orbit.y_rot = side_views[p_option - PANE_VIEW_TOP][1];
			preview_view_type = PaneViewType(PANE_VIEW_TYPE_TOP + p_option - PANE_VIEW_TOP);
			if (_view_menu_checked(PANE_VIEW_AUTO_ORTHOGONAL) && !preview_orbit.orthogonal) {
				preview_orbit.orthogonal = true;
				preview_auto_orthogonal = true;
			}
			_update_preview_camera();
		} break;
		case PANE_VIEW_SWITCH_PERSPECTIVE_ORTHOGONAL:
		case PANE_VIEW_PERSPECTIVE:
		case PANE_VIEW_ORTHOGONAL: {
			_begin_custom_view();
			if (p_option == PANE_VIEW_SWITCH_PERSPECTIVE_ORTHOGONAL) {
				preview_orbit.orthogonal = !preview_orbit.orthogonal;
			} else {
				preview_orbit.orthogonal = p_option == PANE_VIEW_ORTHOGONAL;
			}
			preview_auto_orthogonal = false;
			_update_preview_camera();
		} break;
		case PANE_VIEW_AUTO_ORTHOGONAL: {
			const bool checked = !_view_menu_checked(PANE_VIEW_AUTO_ORTHOGONAL);
			_set_view_menu_checked(PANE_VIEW_AUTO_ORTHOGONAL, checked);
			if (!checked && preview_auto_orthogonal) {
				preview_auto_orthogonal = false;
				preview_orbit.orthogonal = false;
				_update_preview_camera();
			}
		} break;
		case PANE_VIEW_ENVIRONMENT: {
			const bool checked = !_view_menu_checked(PANE_VIEW_ENVIRONMENT);
			_set_view_menu_checked(PANE_VIEW_ENVIRONMENT, checked);
			preview_pane->camera_3d->set_environment(checked ? Ref<Environment>() : Node3DEditor::get_singleton()->get_viewport_environment());
		} break;
		case PANE_VIEW_LOCK_ROTATION:
		case PANE_VIEW_GIZMOS:
		case PANE_VIEW_TRANSFORM_GIZMO:
		case PANE_VIEW_GRID:
		case PANE_VIEW_INFORMATION:
		case PANE_VIEW_FRAME_TIME: {
			_set_view_menu_checked(p_option, !_view_menu_checked(p_option));
			if (p_option == PANE_VIEW_FRAME_TIME) {
				for (int i = 0; i < FRAME_TIME_HISTORY; i++) {
					// As the 3D editor does: start from 120 FPS.
					cpu_time_history[i] = 8.333333;
					gpu_time_history[i] = 8.333333;
				}
				RenderingServer::get_singleton()->viewport_set_measure_render_time(preview_pane->viewport->get_viewport_rid(), _view_menu_checked(PANE_VIEW_FRAME_TIME));
			}
		} break;
		case PANE_VIEW_HALF_RESOLUTION: {
			const bool checked = !_view_menu_checked(PANE_VIEW_HALF_RESOLUTION);
			_set_view_menu_checked(PANE_VIEW_HALF_RESOLUTION, checked);
			preview_pane->viewport->set_scaling_3d_scale(checked ? 0.5 : 1.0);
		} break;
		case PANE_VIEW_AUDIO_LISTENER: {
			const bool checked = !_view_menu_checked(PANE_VIEW_AUDIO_LISTENER);
			_set_view_menu_checked(PANE_VIEW_AUDIO_LISTENER, checked);
			preview_pane->viewport->set_as_audio_listener_3d(checked);
		} break;
		case PANE_VIEW_AUDIO_DOPPLER: {
			const bool checked = !_view_menu_checked(PANE_VIEW_AUDIO_DOPPLER);
			_set_view_menu_checked(PANE_VIEW_AUDIO_DOPPLER, checked);
			preview_pane->camera_3d->set_doppler_tracking(checked ? Camera3D::DOPPLER_TRACKING_IDLE_STEP : Camera3D::DOPPLER_TRACKING_DISABLED);
		} break;
		case PANE_VIEW_CINEMATIC_PREVIEW: {
			_set_cinematic_preview(!preview_cinematic);
		} break;
		case PANE_VIEW_CENTER_TO_ORIGIN: {
			_begin_custom_view();
			preview_orbit.target = Vector3();
			_update_preview_camera();
		} break;
		case PANE_VIEW_CENTER_TO_SELECTION: {
			_focus_preview_bounds(false);
		} break;
		case PANE_VIEW_CENTER_TO_AABB: {
			_focus_preview_bounds(true);
		} break;
		case PANE_VIEW_ALIGN_TRANSFORM_WITH_VIEW: {
			_align_selection_with_view(false);
		} break;
		case PANE_VIEW_ALIGN_ROTATION_WITH_VIEW: {
			_align_selection_with_view(true);
		} break;
		default:
			break;
	}
	_update_view_menu();
}

void EditorSplitView::_update_view_menu() {
	if (!preview_pane->grid) {
		return; // Not built yet.
	}
	Node *root = _pane_scene();
	const bool view_3d = enabled && Object::cast_to<Node3D>(root) && !showing_code && !showing_scene_code;
	MenuButton *menu = preview_pane->view_menu;
	menu->set_visible(view_3d);
	if (!view_3d && preview_cinematic) {
		_set_cinematic_preview(false);
		return;
	}

	bool orthogonal = preview_orbit.orthogonal;
	if (!preview_view_custom && root) {
		orthogonal = _preview_orbit_from_state(root).orthogonal;
	}
	_set_view_menu_checked(PANE_VIEW_PERSPECTIVE, !orthogonal);
	_set_view_menu_checked(PANE_VIEW_ORTHOGONAL, orthogonal);

	// Named as the 3D editor names its viewports.
	static const char *names[7][2] = {
		{ TTRC("Perspective"), TTRC("Orthogonal") },
		{ TTRC("Top Perspective"), TTRC("Top Orthogonal") },
		{ TTRC("Bottom Perspective"), TTRC("Bottom Orthogonal") },
		{ TTRC("Left Perspective"), TTRC("Left Orthogonal") },
		{ TTRC("Right Perspective"), TTRC("Right Orthogonal") },
		{ TTRC("Front Perspective"), TTRC("Front Orthogonal") },
		{ TTRC("Rear Perspective"), TTRC("Rear Orthogonal") },
	};
	String name = TTR(names[preview_view_type][orthogonal ? 1 : 0]);
	if (preview_auto_orthogonal) {
		name += " " + TTR("[auto]");
	}
	if (preview_cinematic) {
		name = TTR("Cinematic Preview");
		if (preview_cinematic_camera.is_null()) {
			name += " " + TTR("(no Camera3D)");
		}
	}
	if (menu->get_text() != name) {
		menu->set_text(name);
		menu->reset_size();
	}

	preview_pane->grid->set_visible(view_3d && !preview_cinematic && _view_menu_checked(PANE_VIEW_GRID));
	preview_pane->rotation_control->set_visible(view_3d && !preview_cinematic && bool(EDITOR_GET("editors/3d/navigation/show_viewport_rotation_gizmo")));
	preview_pane->rotation_control->queue_redraw();
	const bool scene_2d = !Object::cast_to<Node3D>(root);
	preview_pane->tools->set_show_selection(scene_2d || (!preview_cinematic && _view_menu_checked(PANE_VIEW_GIZMOS)));
	preview_pane->tools->set_show_handles(scene_2d || (!preview_cinematic && _view_menu_checked(PANE_VIEW_TRANSFORM_GIZMO)));
	const bool view_2d = enabled && root && !Object::cast_to<Node3D>(root) && !showing_code && !showing_scene_code;
	preview_pane->view_menu_2d->get_parent_control()->set_visible(view_2d);
	if (view_2d) {
		preview_pane->tools->set_show_handles(_view_menu_2d_checked(PANE_2D_GIZMO_TRANSFORMATION));
	}
	const bool information = (view_3d && _view_menu_checked(PANE_VIEW_INFORMATION)) || (view_2d && _view_menu_2d_checked(PANE_2D_SHOW_INFORMATION));
	const bool frame_time = (view_3d && _view_menu_checked(PANE_VIEW_FRAME_TIME)) || (view_2d && _view_menu_2d_checked(PANE_2D_SHOW_FRAME_TIME));
	preview_pane->info_panel->set_visible(information);
	preview_pane->frame_time_panel->set_visible(frame_time);
	if (!view_3d && preview_freelook) {
		_set_freelook(false);
	}
	set_process(information || frame_time || (view_3d && (preview_cinematic || preview_freelook || zoom_indicator_delay > 0.0)));
}

void EditorSplitView::_update_view_panels() {
	Node *root = _pane_scene();
	SubViewport *viewport = preview_pane->viewport;
	if (preview_cinematic && root) {
		// Look through the scene's camera, as the 3D editor does.
		Camera3D *camera = _find_scene_camera(root);
		const ObjectID id = camera ? camera->get_instance_id() : ObjectID();
		RenderingServer::get_singleton()->viewport_attach_camera(viewport->get_viewport_rid(), camera ? camera->get_camera() : preview_pane->camera_3d->get_camera());
		if (id != preview_cinematic_camera) {
			preview_cinematic_camera = id;
			_update_view_menu();
		}
	}
	if (preview_pane->info_panel->is_visible() && root && !Object::cast_to<Node3D>(root)) {
		// As the 2D editor's: the view's size and what it draws.
		String text = vformat(TTR("Size: %s (%.1fMP)") + "\n", vformat(U"%d × %d", viewport->get_size().x, viewport->get_size().y), viewport->get_size().x * viewport->get_size().y * 0.000001);
		text += "\n";
		text += vformat(TTR("Objects: %d"), viewport->get_render_info(Viewport::RENDER_INFO_TYPE_CANVAS, Viewport::RENDER_INFO_OBJECTS_IN_FRAME)) + "\n";
		text += vformat(TTR("Primitives: %d"), viewport->get_render_info(Viewport::RENDER_INFO_TYPE_CANVAS, Viewport::RENDER_INFO_PRIMITIVES_IN_FRAME)) + "\n";
		text += vformat(TTR("Draw Calls: %d"), viewport->get_render_info(Viewport::RENDER_INFO_TYPE_CANVAS, Viewport::RENDER_INFO_DRAW_CALLS_IN_FRAME));
		preview_pane->info_label->set_text(text);
	} else if (preview_pane->info_panel->is_visible()) {
		const Camera3D *camera = ObjectDB::get_instance<Camera3D>(preview_cinematic_camera);
		if (!camera) {
			camera = preview_pane->camera_3d;
		}
		const Vector3 position = camera->get_global_position();
		const real_t scale = viewport->get_scaling_3d_scale();
		String text;
		text += vformat(TTR("X: %s"), rtos(position.x).pad_decimals(1)) + "\n";
		text += vformat(TTR("Y: %s"), rtos(position.y).pad_decimals(1)) + "\n";
		text += vformat(TTR("Z: %s"), rtos(position.z).pad_decimals(1)) + "\n\n";
		text += vformat(TTR("Size: %s (%.1fMP)") + "\n", vformat(U"%d × %d", viewport->get_size().x * scale, viewport->get_size().y * scale), viewport->get_size().x * viewport->get_size().y * scale * scale * 0.000001);
		text += "\n";
		text += vformat(TTR("Objects: %d"), viewport->get_render_info(Viewport::RENDER_INFO_TYPE_VISIBLE, Viewport::RENDER_INFO_OBJECTS_IN_FRAME)) + "\n";
		text += vformat(TTR("Primitives: %d"), viewport->get_render_info(Viewport::RENDER_INFO_TYPE_VISIBLE, Viewport::RENDER_INFO_PRIMITIVES_IN_FRAME)) + "\n";
		text += vformat(TTR("Draw Calls: %d"), viewport->get_render_info(Viewport::RENDER_INFO_TYPE_VISIBLE, Viewport::RENDER_INFO_DRAW_CALLS_IN_FRAME));
		preview_pane->info_label->set_text(text);
	}
	if (preview_pane->frame_time_panel->is_visible()) {
		const RID rid = viewport->get_viewport_rid();
		cpu_time_history[frame_time_history_index] = RenderingServer::get_singleton()->viewport_get_measured_render_time_cpu(rid);
		gpu_time_history[frame_time_history_index] = RenderingServer::get_singleton()->viewport_get_measured_render_time_gpu(rid);
		frame_time_history_index = (frame_time_history_index + 1) % FRAME_TIME_HISTORY;
		double cpu_time = 0.0;
		double gpu_time = 0.0;
		for (int i = 0; i < FRAME_TIME_HISTORY; i++) {
			cpu_time += cpu_time_history[i];
			gpu_time += gpu_time_history[i];
		}
		// As the 3D editor shows it: averaged, and never quite zero.
		cpu_time = MAX(0.01, cpu_time / FRAME_TIME_HISTORY);
		gpu_time = MAX(0.01, gpu_time / FRAME_TIME_HISTORY);
		const double fps = 1000.0 / gpu_time;
		const Color good = get_theme_color(SNAME("success_color"), EditorStringName(Editor));
		const Color ok = get_theme_color(SNAME("warning_color"), EditorStringName(Editor));
		const Color bad = get_theme_color(SNAME("error_color"), EditorStringName(Editor));
		const auto color_for = [&](double p_amount) {
			// 0: good, 0.5: OK, 1: bad.
			const double amount = CLAMP(p_amount, 0.0, 1.0);
			return amount < 0.5 ? good.lerp(ok, amount * 2.0) : ok.lerp(bad, (amount - 0.5) * 2.0);
		};
		preview_pane->cpu_time_label->set_text(vformat(TTR("CPU Time: %s ms"), rtos(cpu_time).pad_decimals(2)));
		preview_pane->cpu_time_label->add_theme_color_override(SceneStringName(font_color), color_for(cpu_time / 30.0));
		preview_pane->gpu_time_label->set_text(vformat(TTR("GPU Time: %s ms"), rtos(gpu_time).pad_decimals(2)));
		preview_pane->gpu_time_label->add_theme_color_override(SceneStringName(font_color), color_for(gpu_time / 30.0));
		preview_pane->fps_label->set_text(vformat(TTR("FPS: %d"), fps));
		preview_pane->fps_label->add_theme_color_override(SceneStringName(font_color), color_for((110.0 - fps) / 100.0));
	}
}

void EditorSplitView::_build_preview_grid() {
	// The 3D editor's grid shader (see Node3DEditor): lines fade out with
	// distance from the camera and as the plane turns edge-on.
	Ref<Shader> grid_shader;
	grid_shader.instantiate();
	grid_shader->set_code(R"(
// 3D editor grid shader.

shader_type spatial;

render_mode unshaded, fog_disabled;

uniform bool orthogonal;
uniform float grid_size;

void vertex() {
	// From FLAG_SRGB_VERTEX_COLOR.
	if (!OUTPUT_IS_SRGB) {
		COLOR.rgb = mix(pow((COLOR.rgb + vec3(0.055)) * (1.0 / (1.0 + 0.055)), vec3(2.4)), COLOR.rgb * (1.0 / 12.92), lessThan(COLOR.rgb, vec3(0.04045)));
	}
}

void fragment() {
	ALBEDO = COLOR.rgb;
	vec3 dir = orthogonal ? -vec3(0, 0, 1) : VIEW;
	float angle_fade = abs(dot(dir, NORMAL));
	angle_fade = smoothstep(0.05, 0.2, angle_fade);

	vec3 world_pos = (INV_VIEW_MATRIX * vec4(VERTEX, 1.0)).xyz;
	vec3 world_normal = (INV_VIEW_MATRIX * vec4(NORMAL, 0.0)).xyz;
	vec3 camera_world_pos = INV_VIEW_MATRIX[3].xyz;
	vec3 camera_world_pos_on_plane = camera_world_pos * (1.0 - world_normal);
	float dist_fade = 1.0 - (distance(world_pos, camera_world_pos_on_plane) / grid_size);
	dist_fade = smoothstep(0.02, 0.3, dist_fade);

	ALPHA = COLOR.a * dist_fade * angle_fade;
}
)");
	for (int i = 0; i < 3; i++) {
		preview_grid_materials[i].instantiate();
		preview_grid_materials[i]->set_shader(grid_shader);
	}

	Ref<ArrayMesh> grid_mesh;
	grid_mesh.instantiate();
	MeshInstance3D *grid = memnew(MeshInstance3D);
	grid->set_mesh(grid_mesh);
	// Out of sight of the 3D editor's cameras when the pane shares its world.
	grid->set_layer_mask(1 << SPLIT_VIEW_GRID_LAYER);
	grid->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	grid->hide();
	preview_pane->viewport->add_child(grid);
	preview_pane->grid = grid;

	// The X, Y and Z axes through the origin, which the grid leaves out, as
	// the 3D editor draws them.
	const Ref<Theme> &theme = EditorNode::get_singleton()->get_editor_theme();
	const real_t length = 100000.0;
	PackedVector3Array points;
	PackedColorArray colors;
	const StringName axis_colors[3] = { SNAME("axis_x_color"), SNAME("axis_y_color"), SNAME("axis_z_color") };
	for (int i = 0; i < 3; i++) {
		Vector3 axis;
		axis[i] = length;
		points.push_back(-axis);
		points.push_back(axis);
		const Color color = theme->get_color(axis_colors[i], EditorStringName(Editor));
		colors.push_back(color);
		colors.push_back(color);
	}
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = points;
	arrays[Mesh::ARRAY_COLOR] = colors;
	Ref<ArrayMesh> axes_mesh;
	axes_mesh.instantiate();
	axes_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, arrays);
	Ref<StandardMaterial3D> axes_material;
	axes_material.instantiate();
	axes_material->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	axes_material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	axes_material->set_flag(BaseMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);
	axes_material->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	axes_mesh->surface_set_material(0, axes_material);
	MeshInstance3D *axes = memnew(MeshInstance3D);
	axes->set_mesh(axes_mesh);
	axes->set_layer_mask(1 << SPLIT_VIEW_GRID_LAYER);
	axes->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	grid->add_child(axes);

	_update_view_menu();
}

void EditorSplitView::_update_preview_grid(bool p_force) {
	MeshInstance3D *grid = preview_pane->grid;
	if (!grid || !preview_pane->camera_3d->is_inside_tree()) {
		return;
	}
	const Camera3D *camera = preview_pane->camera_3d;
	Vector3 camera_position = camera->get_global_position();
	const bool orthogonal = camera->get_projection() == Camera3D::PROJECTION_ORTHOGONAL;
	const real_t ortho_size = orthogonal ? camera->get_size() : 0.0;
	if (!p_force && preview_grid_drawn && orthogonal == preview_grid_orthogonal && Math::is_equal_approx(ortho_size, preview_grid_ortho_size) && preview_grid_position.distance_squared_to(camera_position) < 100.0) {
		return; // Close enough to where it was drawn, as in the 3D editor.
	}
	preview_grid_drawn = true;
	preview_grid_orthogonal = orthogonal;
	preview_grid_ortho_size = ortho_size;
	preview_grid_position = camera_position;

	// From here on, as Node3DEditor::_init_grid() builds the 3D editor's
	// grid, but from this pane's camera.
	const Color primary_grid_color = EDITOR_GET("editors/3d/primary_grid_color");
	const Color secondary_grid_color = EDITOR_GET("editors/3d/secondary_grid_color");
	const int grid_size = EDITOR_GET("editors/3d/grid_size");
	const int primary_grid_steps = EDITOR_GET("editors/3d/primary_grid_steps");
	bool grid_enable[3];
	grid_enable[0] = orthogonal || EDITOR_GET("editors/3d/grid_xy_plane");
	grid_enable[1] = orthogonal || EDITOR_GET("editors/3d/grid_yz_plane");
	grid_enable[2] = orthogonal || EDITOR_GET("editors/3d/grid_xz_plane");
	const real_t division_level_bias = EDITOR_GET("editors/3d/grid_division_level_bias");
	int division_level_max = EDITOR_GET("editors/3d/grid_division_level_max");
	int division_level_min = EDITOR_GET("editors/3d/grid_division_level_min");
	if (division_level_max < division_level_min || primary_grid_steps < 2) {
		return; // The 3D editor reports this one.
	}
	if (primary_grid_steps != 10) {
		const real_t div = Math::log((real_t)primary_grid_steps) / (real_t)2.302585092994045901094; // ln(10).
		division_level_max = (int)(division_level_max / div);
		division_level_min = (int)(division_level_min / div);
	}

	Ref<ArrayMesh> mesh = grid->get_mesh();
	mesh->clear_surfaces();
	for (int a = 0; a < 3; a++) {
		if (!grid_enable[a]) {
			continue;
		}
		const int b = (a + 1) % 3;
		const int c = (a + 2) % 3;
		Vector3 normal;
		normal[c] = 1.0;

		Vector3 position = camera_position;
		real_t camera_distance = Math::abs(position[c]);
		if (orthogonal) {
			camera_distance = camera->get_size() / 2.0;
			const Vector3 camera_direction = -camera->get_global_transform().get_basis().get_column(2);
			Vector3 intersection;
			if (Plane(normal).intersects_ray(camera_position, camera_direction, &intersection)) {
				position = intersection;
			}
		}

		const real_t division_level = Math::log(Math::abs(camera_distance)) / Math::log((double)primary_grid_steps) + division_level_bias;
		const real_t clamped_division_level = CLAMP(division_level, division_level_min, division_level_max);
		const real_t division_level_floored = Math::floor(clamped_division_level);
		const real_t division_level_decimals = clamped_division_level - division_level_floored;

		const real_t small_step_size = Math::pow((real_t)primary_grid_steps, division_level_floored);
		const real_t large_step_size = small_step_size * primary_grid_steps;
		const real_t center_a = large_step_size * (int)(position[a] / large_step_size);
		const real_t center_b = large_step_size * (int)(position[b] / large_step_size);
		const real_t bgn_a = center_a - grid_size * small_step_size;
		const real_t end_a = center_a + grid_size * small_step_size;
		const real_t bgn_b = center_b - grid_size * small_step_size;
		const real_t end_b = center_b + grid_size * small_step_size;

		real_t fade_size = Math::pow((real_t)primary_grid_steps, division_level - (real_t)1.0);
		const real_t min_fade_size = Math::pow((real_t)primary_grid_steps, (real_t)division_level_min);
		const real_t max_fade_size = Math::pow((real_t)primary_grid_steps, (real_t)division_level_max);
		fade_size = CLAMP(fade_size, min_fade_size, max_fade_size);
		preview_grid_materials[c]->set_shader_parameter("grid_size", (grid_size - primary_grid_steps) * fade_size);
		preview_grid_materials[c]->set_shader_parameter("orthogonal", orthogonal);

		PackedVector3Array points;
		PackedVector3Array normals;
		PackedColorArray colors;
		for (int i = -grid_size; i <= grid_size; i++) {
			Color line_color;
			if (i % primary_grid_steps == 0) {
				line_color = primary_grid_color.lerp(secondary_grid_color, division_level_decimals);
			} else {
				line_color = secondary_grid_color;
				line_color.a = line_color.a * (1 - division_level_decimals);
			}
			const real_t position_a = center_a + i * small_step_size;
			const real_t position_b = center_b + i * small_step_size;
			// Not over the axes, which are drawn on their own.
			if (!Math::is_zero_approx(position_a)) {
				Vector3 line_bgn;
				Vector3 line_end;
				line_bgn[a] = position_a;
				line_end[a] = position_a;
				line_bgn[b] = bgn_b;
				line_end[b] = end_b;
				points.push_back(line_bgn);
				points.push_back(line_end);
				colors.push_back(line_color);
				colors.push_back(line_color);
				normals.push_back(normal);
				normals.push_back(normal);
			}
			if (!Math::is_zero_approx(position_b)) {
				Vector3 line_bgn;
				Vector3 line_end;
				line_bgn[b] = position_b;
				line_end[b] = position_b;
				line_bgn[a] = bgn_a;
				line_end[a] = end_a;
				points.push_back(line_bgn);
				points.push_back(line_end);
				colors.push_back(line_color);
				colors.push_back(line_color);
				normals.push_back(normal);
				normals.push_back(normal);
			}
		}
		if (points.is_empty()) {
			continue;
		}
		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = points;
		arrays[Mesh::ARRAY_NORMAL] = normals;
		arrays[Mesh::ARRAY_COLOR] = colors;
		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, arrays);
		mesh->surface_set_material(mesh->get_surface_count() - 1, preview_grid_materials[c]);
	}
}

void EditorSplitView::_set_preview_active(bool p_active) {
	if (preview_active == p_active) {
		return;
	}
	preview_active = p_active;
	_update_active_pane();
}

void EditorSplitView::_update_active_pane() {
	// The second pane can only be edited while it shows a scene.
	const bool pane_scene = enabled && !showing_code && !showing_scene_code && _pane_scene();
	focus_pane->set_active(!(preview_active && pane_scene));
	preview_pane->set_active(preview_active && pane_scene, pane_scene);
	// Undo and redo in the menu follow the side being edited.
	EditorNode::get_singleton()->update_undo_redo_allowed();
	_sync_main_inspector();
}

void EditorSplitView::_sync_main_inspector() {
	// Editing the edited scene, the editor's own selection drives the docks.
	if (!pane_live && get_active_history_id() != EditorUndoRedoManager::INVALID_HISTORY) {
		// What's selected in the second pane, or its scene.
		Node *node = preview_pane->tools->get_selected();
		if (!node) {
			node = _pane_scene();
		}
		pane_in_main_inspector = true;
		if (InspectorDock::get_inspector_singleton()->get_edited_object() != node) {
			InspectorDock::get_singleton()->edit_outside_history(node);
			SignalsDock::get_singleton()->set_object(node);
			GroupsDock::get_singleton()->set_selection(Vector<Node *>{ node });
		}
	} else if (pane_in_main_inspector) {
		// Back to what's selected in the scene being edited, once the editor
		// is done with whatever made the second pane stop being edited.
		pane_in_main_inspector = false;
		callable_mp(this, &EditorSplitView::_restore_main_inspector).call_deferred();
	}
}

void EditorSplitView::_restore_main_inspector() {
	if (!pane_in_main_inspector) {
		EditorNode::get_singleton()->edit_current();
	}
}

int EditorSplitView::get_active_history_id() const {
	if (!preview_active || !enabled || showing_code || showing_scene_code || !preview_pane->is_visible_in_tree() || !_pane_scene()) {
		return EditorUndoRedoManager::INVALID_HISTORY;
	}
	return preview_pane->tools->get_history_id();
}

void EditorSplitView::_pane_selection_changed() {
	// The second pane's own selection: its tree and inspector follow it.
	Node *root = _pane_scene();
	Node *selected = preview_pane->tools->get_selected();
	if (pane_live && !syncing_live_selection) {
		// Picked in the pane: select it in the editor too, as the 3D editor's
		// viewport does.
		EditorSelection *selection = EditorNode::get_singleton()->get_editor_selection();
		selection->clear();
		if (selected) {
			selection->add_node(selected);
			EditorNode::get_singleton()->edit_node(selected);
		}
	}
	Node *inspected = selected ? selected : root;
	preview_pane->inspector->edit(inspected);
	preview_pane->inspector_title->set_text(inspected ? vformat(TTR("Inspector: %s"), inspected->get_name()) : TTR("Inspector"));
	if (preview_pane->tree->get_selected() != selected || !selected) {
		preview_pane->tree->set_selected(nullptr, false);
		preview_pane->tree->get_scene_tree()->deselect_all();
		if (selected) {
			preview_pane->tree->set_selected(selected, false);
		}
	}
	preview_pane->overlay->queue_redraw();
	_update_pane_toolbar();
	_sync_main_inspector();
}

void EditorSplitView::input(const Ref<InputEvent> &p_event) {
	if (!floating && _code_shortcut_input(p_event)) {
		get_viewport()->set_input_as_handled();
		return;
	}
	// Undo and redo go to the side clicked last.
	const Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->is_pressed() && enabled && (mb->get_button_index() == MouseButton::LEFT || mb->get_button_index() == MouseButton::RIGHT || mb->get_button_index() == MouseButton::MIDDLE)) {
		const Point2 pos = mb->get_position();
		if (!floating && preview_pane->is_visible_in_tree() && preview_pane->get_global_rect().has_point(pos)) {
			_set_preview_active(true);
		} else if (focus_pane->get_global_rect().has_point(pos) || (SceneTreeDock::get_singleton()->is_visible_in_tree() && SceneTreeDock::get_singleton()->get_global_rect().has_point(pos))) {
			// The other docks, the Inspector among them, work on either side.
			_set_preview_active(false);
		}
	}
}

void EditorSplitView::_float_window_input(const Ref<InputEvent> &p_event) {
	if (floating && _code_shortcut_input(p_event)) {
		float_window->set_input_as_handled();
		return;
	}
	const Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->is_pressed() && (mb->get_button_index() == MouseButton::LEFT || mb->get_button_index() == MouseButton::RIGHT || mb->get_button_index() == MouseButton::MIDDLE)) {
		_set_preview_active(true);
	}
}

void EditorSplitView::_trade_sides() {
	// Edit what the other pane shows. The panes stay; switching makes the
	// scene (or script) left behind the one the other pane shows.
	if (showing_code) {
		TextEditorBase *other = ObjectDB::get_instance<TextEditorBase>(code_editor_id);
		if (other && other->get_instance_id() != current_code_editor_id) {
			ScriptEditor::get_singleton()->edit(other->get_edited_resource());
			_track_current_script();
			_queue_update();
		}
		return;
	}
	if (showing_live || showing_scene_code) {
		return;
	}
	const int idx = _find_scene_index(preview_scene);
	if (idx >= 0 && idx != EditorNode::get_editor_data().get_edited_scene()) {
		// What's selected here stays selected once it's edited there.
		Node *root = get_preview_scene();
		Node *selected = preview_pane->tools->get_selected();
		if (root && selected) {
			pending_select = root->get_path_to(selected);
			pending_select_scene = preview_scene;
		}
		_set_preview_active(false);
		EditorSceneTabs::get_singleton()->set_current_tab(idx);
	}
}

void EditorSplitView::_preview_gui_input(const Ref<InputEvent> &p_event) {
	Node *root = _pane_scene();
	const bool is_3d = Object::cast_to<Node3D>(root);
	const Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->is_pressed() && (mb->get_button_index() == MouseButton::LEFT || mb->get_button_index() == MouseButton::RIGHT || mb->get_button_index() == MouseButton::MIDDLE)) {
		_set_preview_active(true);
		preview_pane->viewport_container->grab_focus();
	}
	if (is_3d ? _view_menu_shortcut(p_event) : _view_menu_2d_shortcut(p_event)) {
		preview_pane->viewport_container->accept_event();
		return;
	}
	if (is_3d && !preview_cinematic && !preview_pane->tools->is_dragging()) {
		// Freelook, as in the 3D editor: hold the right mouse button to look
		// around, with WASD, Q and E to fly and the wheel for speed.
		if (mb.is_valid() && mb->get_button_index() == MouseButton::RIGHT) {
			if (mb->is_pressed() && !preview_freelook) {
				_set_freelook(true, mb->get_global_position());
			} else if (!mb->is_pressed() && preview_freelook) {
				_set_freelook(false);
			}
			preview_pane->viewport_container->accept_event();
			return;
		}
		if (preview_freelook) {
			const Ref<InputEventMouseMotion> motion = p_event;
			if (motion.is_valid()) {
				_freelook_look(motion->get_relative());
			} else if (mb.is_valid() && mb->is_pressed() && (mb->get_button_index() == MouseButton::WHEEL_UP || mb->get_button_index() == MouseButton::WHEEL_DOWN)) {
				// The wheel sets the speed while flying.
				const real_t factor = mb->get_button_index() == MouseButton::WHEEL_UP ? 1.08 : 1.0 / 1.08;
				freelook_speed = CLAMP(freelook_speed * factor, (real_t)0.01, (real_t)10000.0);
				zoom_indicator_delay = 1.5;
				preview_pane->overlay->queue_redraw();
			} else if (Ref<InputEventKey>(p_event).is_valid() && Ref<InputEventKey>(p_event)->is_action_pressed(SNAME("ui_cancel"), false, true)) {
				_set_freelook(false);
			}
			// Keys fly instead of picking tools.
			preview_pane->viewport_container->accept_event();
			return;
		}
	}
	if (is_3d && preview_cinematic) {
		// Looking through the scene's camera: nothing to move or pick.
		const Ref<InputEventMouseMotion> motion = p_event;
		if (mb.is_valid() || (motion.is_valid() && !motion->get_button_mask().is_empty())) {
			preview_pane->viewport_container->accept_event();
		}
		return;
	}
	if (preview_pane->tools->handle_input(p_event)) {
		preview_pane->viewport_container->accept_event();
		return;
	}
	if (mb.is_valid() && mb->is_pressed()) {
		switch (mb->get_button_index()) {
			case MouseButton::WHEEL_UP:
			case MouseButton::WHEEL_DOWN: {
				// Zoom, towards the mouse in 2D.
				_begin_custom_view();
				const real_t factor = mb->get_button_index() == MouseButton::WHEEL_UP ? 1.15 : 1.0 / 1.15;
				if (is_3d) {
					preview_orbit.distance = CLAMP(preview_orbit.distance / factor, 0.05, 10000.0);
					// Show how far it is, for a moment.
					zoom_indicator_delay = 1.5;
					_update_view_menu();
				} else {
					const Vector2 at = mb->get_position();
					preview_view_2d = Transform2D(0, at) * Transform2D().scaled(Size2(factor, factor)) * Transform2D(0, -at) * preview_view_2d;
				}
				_update_preview_camera();
			} break;
			case MouseButton::MIDDLE: {
				if (mb->is_double_click()) {
					// Back to the scene's own view.
					preview_view_custom = false;
					preview_fit_root = ObjectID(); // Fit again.
					_update_preview_camera();
					_update_view_menu();
				}
			} break;
			default:
				break;
		}
		preview_pane->viewport_container->accept_event();
		return;
	}
	const Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid()) {
		if (root && mm->get_button_mask().has_flag(MouseButtonMask::MIDDLE)) {
			_begin_custom_view();
			const Vector2 rel = mm->get_relative();
			if (!is_3d) {
				preview_view_2d.columns[2] += rel;
			} else if (mm->is_shift_pressed()) {
				// Pan.
				const Basis basis = preview_pane->camera_3d->get_global_transform().basis;
				const real_t speed = preview_orbit.distance * 0.002;
				preview_orbit.target += (-basis.get_column(0) * rel.x + basis.get_column(1) * rel.y) * speed;
			} else {
				_orbit_preview(rel);
			}
			_update_preview_camera();
			preview_pane->viewport_container->accept_event();
		}
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
	// Selects it in this pane; its scene stays where it is.
	_set_preview_active(true);
	if (selected != preview_pane->tools->get_selected()) {
		preview_pane->tools->set_selected(selected, false);
		_pane_selection_changed();
	}
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
	if (pane_in_main_inspector) {
		return; // Showing the second pane's selection.
	}
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

bool EditorSplitView::_uses_scene(const Node *p_node, const String &p_path) {
	// An instance of the scene, or a scene inheriting it, anywhere below.
	if (p_node->get_scene_file_path() == p_path) {
		return true;
	}
	for (Ref<SceneState> state = p_node->get_scene_inherited_state(); state.is_valid(); state = state->get_base_scene_state()) {
		if (state->get_path() == p_path) {
			return true;
		}
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		if (_uses_scene(p_node->get_child(i), p_path)) {
			return true;
		}
	}
	return false;
}

void EditorSplitView::_scene_saved(const String &p_path) {
	// After the save is done: the "(*)" in the headers, and the second pane's
	// scene if it uses the saved scene.
	_queue_update();
	if (saved_scenes.is_empty()) {
		// On the next frame: rebuilding shows a progress dialog, which can't
		// be done from a deferred call.
		get_tree()->connect("process_frame", callable_mp(this, &EditorSplitView::_refresh_preview_instances), CONNECT_ONE_SHOT);
	}
	if (!saved_scenes.has(p_path)) {
		saved_scenes.push_back(p_path);
	}
}

bool EditorSplitView::_uses_any_scene(int p_idx, const Vector<String> &p_paths) {
	EditorData &data = EditorNode::get_editor_data();
	const Node *root = data.get_edited_scene_root(p_idx);
	if (!root) {
		return false;
	}
	for (const String &path : p_paths) {
		if (data.get_scene_path(p_idx) == path) {
			continue;
		}
		for (int i = 0; i < root->get_child_count(); i++) {
			if (_uses_scene(root->get_child(i), path)) {
				return true;
			}
		}
		for (Ref<SceneState> state = root->get_scene_inherited_state(); state.is_valid(); state = state->get_base_scene_state()) {
			if (state->get_path() == path) {
				return true;
			}
		}
	}
	return false;
}

void EditorSplitView::_refresh_preview_instances() {
	const Vector<String> saved = saved_scenes;
	saved_scenes.clear();

	// The scene being edited, when the saved scene was the one in the second
	// pane: the editor only rebuilds it when switching to its tab, so it would
	// keep showing the old version beside the new one.
	const int current = EditorNode::get_editor_data().get_edited_scene();
	if (current >= 0 && _uses_any_scene(current, saved)) {
		EditorNode::get_singleton()->update_edited_scene_instances();
	}

	Node *root = get_preview_scene();
	if (!root) {
		return;
	}
	const int idx = _find_scene_index(preview_scene);
	if (idx < 0 || !_uses_any_scene(idx, saved)) {
		return;
	}
	// The editor rebuilds such scenes when switching to them; do it now, as
	// this one is on screen. Keep the view looked around in.
	const bool keep_view = preview_view_custom;
	const OrbitView orbit = preview_orbit;
	const Transform2D view_2d = preview_view_2d;
	_release_preview();
	if (!EditorNode::get_editor_data().reload_scene_from_memory(idx, false)) {
		_queue_update();
		return;
	}
	Node *new_root = EditorNode::get_editor_data().get_edited_scene_root(idx);
	if (new_root && !new_root->get_parent()) {
		_take_into_preview(new_root);
		preview_view_custom = keep_view;
		preview_orbit = orbit;
		preview_view_2d = view_2d;
		_update_preview_camera();
	}
	_queue_update();
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
	const bool unsaved = idx >= 0 && EditorUndoRedoManager::get_singleton()->is_history_unsaved(EditorNode::get_editor_data().get_scene_history_id(idx));
	if (unsaved) {
		title += "(*)";
	}
	if (p_pane == preview_pane && unsaved != preview_unsaved) {
		// Edited in the pane: the scene tabs only follow the edited scene.
		preview_unsaved = unsaved;
		EditorSceneTabs::get_singleton()->update_scene_tabs();
	}
	p_pane->set_title(title);
	p_pane->type_label->set_text(p_root->get_class());
	if (p_pane == preview_pane && floating) {
		float_window->set_title(vformat(TTR("%s - Split View"), title));
	}
	p_pane->dot->queue_redraw();
}

static bool _split_view_dock_open(Control *p_dock) {
	// A dock is open while it's in a slot that's shown, whether or not its
	// tab is the current one; a closed dock is out of the tree.
	if (!p_dock->is_inside_tree()) {
		return false;
	}
	const Control *parent = Object::cast_to<Control>(p_dock->get_parent());
	return parent ? parent->is_visible_in_tree() : p_dock->is_visible_in_tree();
}

void EditorSplitView::_update_strips() {
	const bool split = floating ? float_window->is_visible() : preview_pane->is_visible();
	// The focused scene already has the Scene and Inspector docks; only give
	// its pane its own strips when those docks are closed.
	// Open counts even behind another tab of their slot, such as the
	// Inspector while Signals or Groups is shown.
	const bool docks_open = _split_view_dock_open(SceneTreeDock::get_singleton()) && _split_view_dock_open(InspectorDock::get_singleton());
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
	const bool pane_code = showing_code || showing_scene_code;
	preview_pane->tree_strip->set_visible(!pane_code && show_pane_docks && preview_width >= 520 * EDSCALE);
	preview_pane->inspector_strip->set_visible(!showing_live && !pane_code && show_pane_docks && preview_width >= 760 * EDSCALE);
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
	_set_pane_live(showing_live && !showing_code);
	if (showing_live != was_live && !floating && preview_pane->get_parent() == this) {
		if (was_live) {
			script_split_offset = get_split_offset();
			has_script_split_offset = true;
			set_split_offset(scene_split_offset);
		} else {
			scene_split_offset = get_split_offset();
			if (!has_script_split_offset) {
				// Comparing, give the code about two thirds of the width; an
				// editable scene gets half, room for its tree beside it.
				const int sixth = showing_code ? int(get_size().x / 6) : 0;
				script_split_offset = preview_pane->get_index() > focus_pane->get_index() ? sixth : -sixth;
			}
			set_split_offset(script_split_offset);
		}
	}
	const bool scene_screen = enabled && _is_scene_screen();
	showing_scene_code = scene_screen && code_beside_scene;
	const bool split = showing_live || showing_scene_code || (scene_screen && preview);

	preview_pane->viewport_container->set_visible(!showing_code && !showing_scene_code);
	preview_pane->tools->set_visible(!showing_code && !showing_scene_code);
	preview_pane->code_container->set_visible(showing_code || showing_scene_code);
	// Scene or code, on every screen; each screen keeps its own choice.
	preview_pane->code_button->set_visible(script_screen || scene_screen);
	preview_pane->code_button->set_pressed_no_signal(script_screen ? compare_scripts : code_beside_scene);
	// Trading sides needs something to edit on the other side.
	preview_pane->swap_button->set_visible(showing_code || (scene_screen && !showing_scene_code));
	preview_pane->code_button->set_tooltip_text(script_screen ? TTR("Show another open script here instead of the scene. The script you switch away from moves here.") : TTR("Show the selected node's script here instead of another scene."));
	if (!showing_code && !showing_scene_code) {
		_set_code_source(nullptr);
		code_static_script.unref();
	}
	_set_pane_tree_root(showing_live ? (showing_code ? nullptr : edited) : (showing_scene_code ? nullptr : preview));

	if (floating) {
		preview_pane->show();
		_show_float_window(split);
	} else {
		preview_pane->set_visible(split);
	}
	focus_pane->header->set_visible(split && !showing_live);
	_update_active_pane();
	_update_strips();
	_update_view_menu();
	_update_pane_toolbar();

	if (!split) {
		focus_pane->inspector->edit(nullptr);
		return;
	}

	if (showing_code) {
		_update_code_view();
		return;
	}
	if (showing_scene_code) {
		_update_pane_header(focus_pane, edited);
		focus_pane->tree->update_tree();
		_inspected_object_changed();
		_update_scene_code_view();
		return;
	}
	_update_pane_header(preview_pane, showing_live ? edited : preview);
	preview_pane->tree->update_tree();
	if (showing_live) {
		_sync_pane_live_selection();
		_update_preview_camera();
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
	if (floating == p_floating) {
		return;
	}
	if (p_floating) {
		if (!float_rect.has_area()) {
			float_rect = _default_float_rect();
		}
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
		// The second pane is always on the right, away from the Scene dock.
		move_child(preview_pane, focus_pane->get_index() + 1);
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
	// Show the new scene's script, not one from the scene left behind.
	scene_code_script.unref();
	Node *preview = ObjectDB::get_instance<Node>(preview_scene);
	if (preview && preview == p_new_root) {
		// The previewed scene is being edited now; EditorNode moved it.
		_set_pane_tree_root(nullptr);
		preview_pane->tools->set_scene(nullptr);
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
			EditorNode::get_singleton()->connect("scene_saved", callable_mp(this, &EditorSplitView::_scene_saved));
			// Keeps the "(*)" unsaved marks in the pane headers current.
			EditorUndoRedoManager::get_singleton()->connect("history_changed", callable_mp(this, &EditorSplitView::_queue_update));
			EditorUndoRedoManager::get_singleton()->connect("version_changed", callable_mp(this, &EditorSplitView::_queue_update));
			InspectorDock::get_inspector_singleton()->connect("edited_object_changed", callable_mp(this, &EditorSplitView::_inspected_object_changed));
			focus_pane->tree->set_editor_selection(EditorNode::get_singleton()->get_editor_selection());
			SceneTreeDock::get_singleton()->connect(SceneStringName(visibility_changed), callable_mp(this, &EditorSplitView::_queue_update));
			InspectorDock::get_singleton()->connect(SceneStringName(visibility_changed), callable_mp(this, &EditorSplitView::_queue_update));
			set_show_pane_docks(EDITOR_GET("interface/editor/split_view/show_pane_docks"));
			compare_scripts = EDITOR_GET("interface/editor/split_view/compare_scripts");
			code_beside_scene = EDITOR_GET("interface/editor/split_view/code_beside_scene");
			ScriptEditor::get_singleton()->connect("editor_script_changed", callable_mp(this, &EditorSplitView::_script_changed));
			EditorNode::get_singleton()->get_editor_selection()->connect("selection_changed", callable_mp(this, &EditorSplitView::_selection_changed));
			set_split_enabled(EDITOR_GET("interface/editor/split_view/enabled"));
			set_process_input(true);
			_build_view_menu();
			_build_view_menu_2d();
			_build_preview_grid();
			// The Scene dock's tree settings.
			preview_pane->tree->set_auto_expand_selected(EDITOR_GET("docks/scene_tree/auto_expand_to_selected"), false);
			preview_pane->tree->set_hide_filtered_out_parents(EDITOR_GET("docks/scene_tree/hide_filtered_out_parents"), false);
			preview_pane->tree->set_accessibility_warnings(EDITOR_GET("docks/scene_tree/accessibility_warnings"), false);
			// Whether the editor uses native windows is only settled once
			// startup is done.
			callable_mp(this, &EditorSplitView::_apply_float_setting).call_deferred();
		} break;

		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (EditorSettings::get_singleton()->check_changed_settings_in_group("editors/3d/navigation")) {
				_update_view_menu(); // Showing the axis gizmo or not.
			}
			if (EditorSettings::get_singleton()->check_changed_settings_in_group("editors/3d")) {
				_update_preview_grid(true); // Grid size, steps, colors or planes.
			}
			if (EditorSettings::get_singleton()->check_changed_settings_in_group("interface/editor/split_view")) {
				set_show_pane_docks(EDITOR_GET("interface/editor/split_view/show_pane_docks"));
				compare_scripts = EDITOR_GET("interface/editor/split_view/compare_scripts");
				code_beside_scene = EDITOR_GET("interface/editor/split_view/code_beside_scene");
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
			// Taken out while the pane edited the edited scene; nothing else
			// would free it.
			if (preview_pane->fallback_lighting && !preview_pane->fallback_lighting->get_parent()) {
				memdelete(preview_pane->fallback_lighting);
				preview_pane->fallback_lighting = nullptr;
			}
		} break;

		case NOTIFICATION_PROCESS: {
			if (preview_freelook) {
				_freelook_move(get_process_delta_time());
			}
			if (zoom_indicator_delay > 0.0) {
				zoom_indicator_delay -= get_process_delta_time();
				if (zoom_indicator_delay <= 0.0) {
					zoom_indicator_delay = 0.0;
					preview_pane->overlay->queue_redraw();
					_update_view_menu();
				}
			}
			_update_view_panels();
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
	// As the Scene dock: open an instance's scene, or a node's script.
	preview_pane->tree->connect("open", callable_mp(this, &EditorSplitView::_pane_open_scene));
	preview_pane->tree->connect("open_script", callable_mp(this, &EditorSplitView::_pane_open_script));
	preview_pane->tools->connect("selection_changed", callable_mp(this, &EditorSplitView::_pane_selection_changed));
	preview_pane->rotation_control->connect(SceneStringName(pressed), callable_mp(this, &EditorSplitView::_set_preview_active).bind(true));
	preview_pane->rotation_control->connect("axis_clicked", callable_mp(this, &EditorSplitView::_rotation_axis_clicked));
	preview_pane->rotation_control->connect("orbit", callable_mp(this, &EditorSplitView::_orbit_preview));
	preview_pane->add_node_button->connect(SceneStringName(pressed), callable_mp(this, &EditorSplitView::_pane_add_node));
	preview_pane->instantiate_button->connect(SceneStringName(pressed), callable_mp(this, &EditorSplitView::_pane_instantiate_scene));
	preview_pane->tree_filter->connect(SceneStringName(text_changed), callable_mp(this, &EditorSplitView::_pane_filter_changed));
	preview_pane->attach_script_button->connect(SceneStringName(pressed), callable_mp(this, &EditorSplitView::_pane_attach_script));
	preview_pane->detach_script_button->connect(SceneStringName(pressed), callable_mp(this, &EditorSplitView::_pane_detach_script));
	preview_pane->tree_menu->connect("about_to_popup", callable_mp(this, &EditorSplitView::_pane_tree_menu_about_to_popup));
	preview_pane->tree_menu->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &EditorSplitView::_pane_tree_menu_option));
	preview_pane->subresource_menu->connect("about_to_popup", callable_mp(this, &EditorSplitView::_pane_list_subresources));
	preview_pane->subresource_menu->connect("index_pressed", callable_mp(this, &EditorSplitView::_pane_edit_subresource));
	preview_pane->tree->set_scene_root_override(nullptr);
	preview_pane->viewport_container->connect(SceneStringName(gui_input), callable_mp(this, &EditorSplitView::_preview_gui_input));
	preview_pane->viewport_container->connect(SceneStringName(mouse_exited), callable_mp((CanvasItem *)preview_pane->overlay, &CanvasItem::queue_redraw));
	preview_pane->viewport_container->connect(SceneStringName(resized), callable_mp(this, &EditorSplitView::_update_preview_camera));
	preview_pane->overlay->connect(SceneStringName(draw), callable_mp(this, &EditorSplitView::_draw_preview_overlay));
	preview_pane->close_button->connect(SceneStringName(pressed), callable_mp(this, &EditorSplitView::_close_preview));
	preview_pane->float_button->connect(SceneStringName(toggled), callable_mp(this, &EditorSplitView::_float_toggled));
	preview_pane->swap_button->connect(SceneStringName(pressed), callable_mp(this, &EditorSplitView::_trade_sides));
	preview_pane->code_button->connect(SceneStringName(toggled), callable_mp(this, &EditorSplitView::_code_toggled));
	preview_pane->pick_button->connect("about_to_popup", callable_mp(this, &EditorSplitView::_fill_pick_menu));
	preview_pane->pick_button->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &EditorSplitView::_pick_menu_id_pressed));
	preview_pane->code_search_menu->connect("about_to_popup", callable_mp(this, &EditorSplitView::_fill_code_menus));
	preview_pane->code_goto_menu->connect("about_to_popup", callable_mp(this, &EditorSplitView::_fill_code_menus));
	preview_pane->code_search_menu->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &EditorSplitView::_code_menu_id_pressed));
	preview_pane->code_goto_menu->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &EditorSplitView::_code_menu_id_pressed));
	preview_pane->code_function_menu->connect("about_to_popup", callable_mp(this, &EditorSplitView::_fill_code_function_menu));
	preview_pane->code_function_menu->get_popup()->connect("index_pressed", callable_mp(this, &EditorSplitView::_code_function_pressed));
	preview_pane->code_both_button->connect(SceneStringName(toggled), callable_mp(this, &EditorSplitView::_code_find_options_changed));
	preview_pane->code_back_button->connect(SceneStringName(pressed), callable_mp(this, &EditorSplitView::_code_history_step).bind(-1));
	preview_pane->code_forward_button->connect(SceneStringName(pressed), callable_mp(this, &EditorSplitView::_code_history_step).bind(1));
	preview_pane->code_find_text->connect(SceneStringName(text_changed), callable_mp(this, &EditorSplitView::_code_find_text_changed));
	preview_pane->code_find_text->connect(SceneStringName(text_submitted), callable_mp(this, &EditorSplitView::_code_find_submitted));
	preview_pane->code_find_text->connect(SceneStringName(gui_input), callable_mp(this, &EditorSplitView::_code_find_input));
	preview_pane->code_find_prev->connect(SceneStringName(pressed), callable_mp(this, &EditorSplitView::_code_find).bind(true));
	preview_pane->code_find_next->connect(SceneStringName(pressed), callable_mp(this, &EditorSplitView::_code_find).bind(false));
	preview_pane->code_find_case->connect(SceneStringName(toggled), callable_mp(this, &EditorSplitView::_code_find_options_changed));
	preview_pane->code_find_words->connect(SceneStringName(toggled), callable_mp(this, &EditorSplitView::_code_find_options_changed));
	preview_pane->code_find_close->connect(SceneStringName(pressed), callable_mp(this, &EditorSplitView::_code_find_hide));
	preview_pane->code_line_text->connect(SceneStringName(text_submitted), callable_mp(this, &EditorSplitView::_code_line_submitted));
	// Drop scene tabs, scenes, scripts and script list entries on the pane.
	const Callable can_drop = callable_mp(this, &EditorSplitView::_can_drop);
	const Callable drop = callable_mp(this, &EditorSplitView::_drop);
	Control *drop_targets[] = { preview_pane, preview_pane->header, preview_pane->viewport_container, preview_pane->code_view, preview_pane->code_container };
	for (Control *target : drop_targets) {
		target->set_drag_forwarding(Callable(), can_drop, drop);
	}
	preview_pane->connect(SceneStringName(resized), callable_mp(this, &EditorSplitView::_update_strips), CONNECT_DEFERRED);
	preview_pane->hide();

	float_window = memnew(Window);
	float_window->set_visible(false);
	float_window->set_title(TTR("Split View"));
	float_window->set_wrap_controls(true);
	float_window->set_transient(true);
	float_window->set_propagate_shortcuts_to_parent(true);
	float_window->connect("close_requested", callable_mp(this, &EditorSplitView::_float_window_close_requested));
	float_window->connect("window_input", callable_mp(this, &EditorSplitView::_float_window_input));
	Panel *float_background = memnew(Panel);
	float_background->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
	float_window->add_child(float_background);
	add_child(float_window);
}

EditorSplitView::~EditorSplitView() {
	singleton = nullptr;
}
