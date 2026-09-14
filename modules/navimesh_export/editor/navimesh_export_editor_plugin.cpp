/**************************************************************************/
/*  navimesh_export_editor_plugin.cpp                                     */
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

#ifdef TOOLS_ENABLED

#include "navimesh_export_editor_plugin.h"

#include "../navimesh_exporter.h"

#include "core/object/callable_mp.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/gui/editor_file_dialog.h"
#include "scene/2d/navigation/navigation_link_2d.h"
#include "scene/2d/navigation/navigation_region_2d.h"
#include "scene/3d/navigation/navigation_link_3d.h"
#include "scene/3d/navigation/navigation_region_3d.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/scene_string_names.h"

static bool _is_nav_export_node(Object *p_object) {
	return Object::cast_to<NavigationRegion3D>(p_object) || Object::cast_to<NavigationLink3D>(p_object) ||
			Object::cast_to<NavigationRegion2D>(p_object) || Object::cast_to<NavigationLink2D>(p_object);
}

static String _default_export_name(Object *p_object) {
	Node *node = Object::cast_to<Node>(p_object);
	if (node && !node->get_scene_file_path().is_empty()) {
		return node->get_scene_file_path().get_file().get_basename() + ".nav.json";
	}
	if (node) {
		return String(node->get_name()) + ".nav.json";
	}
	return "navimesh.nav.json";
}

bool NavimeshExportInspectorPlugin::can_handle(Object *p_object) {
	return _is_nav_export_node(p_object);
}

void NavimeshExportInspectorPlugin::parse_begin(Object *p_object) {
	current_node = Object::cast_to<Node>(p_object);
	ERR_FAIL_NULL(current_node);

	Button *export_button = memnew(Button);
	export_button->set_text(TTR("Export Navimesh"));
	export_button->set_tooltip_text(TTR("Bake (if needed) and export this navigation data as JSON and binary for a third-party server."));
	export_button->connect(SceneStringName(pressed), callable_mp(this, &NavimeshExportInspectorPlugin::_on_export_pressed));
	add_custom_control(export_button);

	if (!file_dialog) {
		file_dialog = memnew(EditorFileDialog);
		file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
		file_dialog->add_filter("*.nav.json", TTR("Navimesh JSON"));
		file_dialog->add_filter("*.nav.bin", TTR("Navimesh Binary"));
		file_dialog->set_title(TTR("Export Navimesh"));
		file_dialog->connect("file_selected", callable_mp(this, &NavimeshExportInspectorPlugin::_on_file_selected));
		EditorNode::get_singleton()->get_gui_base()->add_child(file_dialog);
	}
}

void NavimeshExportInspectorPlugin::_on_export_pressed() {
	ERR_FAIL_NULL(current_node);
	ERR_FAIL_NULL(file_dialog);
	file_dialog->set_current_file(_default_export_name(current_node));
	file_dialog->popup_file_dialog();
}

void NavimeshExportInspectorPlugin::_on_file_selected(const String &p_path) {
	ERR_FAIL_NULL(current_node);
	NavimeshExporter *exporter = NavimeshExporter::get_singleton();
	ERR_FAIL_NULL(exporter);
	const Error err = exporter->export_node(current_node, p_path, NavimeshExporter::FORMAT_BOTH);
	if (err == OK) {
		print_line(vformat("NavimeshExporter: exported '%s'.", p_path));
	} else {
		ERR_PRINT(vformat("NavimeshExporter: failed to export '%s'.", p_path));
	}
}

void NavimeshExportEditorPlugin::_ensure_file_dialog() {
	if (file_dialog) {
		return;
	}
	file_dialog = memnew(EditorFileDialog);
	file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
	file_dialog->add_filter("*.nav.json", TTR("Navimesh JSON"));
	file_dialog->add_filter("*.nav.bin", TTR("Navimesh Binary"));
	file_dialog->set_title(TTR("Export Navimesh"));
	file_dialog->connect("file_selected", callable_mp(this, &NavimeshExportEditorPlugin::_on_file_selected));
	add_child(file_dialog);
}

void NavimeshExportEditorPlugin::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		if (spatial_export) {
			spatial_export->set_text(TTR("Export Navimesh"));
		}
		if (canvas_export) {
			canvas_export->set_text(TTR("Export Navimesh"));
		}
	}
}

bool NavimeshExportEditorPlugin::handles(Object *p_object) const {
	return Object::cast_to<NavigationRegion3D>(p_object) || Object::cast_to<NavigationRegion2D>(p_object);
}

void NavimeshExportEditorPlugin::edit(Object *p_object) {
	current_object = p_object ? p_object->get_instance_id() : ObjectID();
}

void NavimeshExportEditorPlugin::make_visible(bool p_visible) {
	Object *obj = ObjectDB::get_instance(current_object);
	const bool show_3d = p_visible && Object::cast_to<NavigationRegion3D>(obj);
	const bool show_2d = p_visible && Object::cast_to<NavigationRegion2D>(obj);
	if (spatial_hbox) {
		spatial_hbox->set_visible(show_3d);
	}
	if (canvas_hbox) {
		canvas_hbox->set_visible(show_2d);
	}
	if (!p_visible) {
		current_object = ObjectID();
	}
}

void NavimeshExportEditorPlugin::_on_toolbar_export() {
	_ensure_file_dialog();
	ERR_FAIL_NULL(file_dialog);
	Object *obj = ObjectDB::get_instance(current_object);
	file_dialog->set_current_file(_default_export_name(obj));
	file_dialog->popup_file_dialog();
}

void NavimeshExportEditorPlugin::_on_file_selected(const String &p_path) {
	Node *node = Object::cast_to<Node>(ObjectDB::get_instance(current_object));
	ERR_FAIL_NULL(node);
	NavimeshExporter *exporter = NavimeshExporter::get_singleton();
	ERR_FAIL_NULL(exporter);
	const Error err = exporter->export_node(node, p_path, NavimeshExporter::FORMAT_BOTH);
	if (err == OK) {
		print_line(vformat("NavimeshExporter: exported '%s'.", p_path));
	} else {
		ERR_PRINT(vformat("NavimeshExporter: failed to export '%s'.", p_path));
	}
}

NavimeshExportEditorPlugin::NavimeshExportEditorPlugin() {
	inspector_plugin.instantiate();
	add_inspector_plugin(inspector_plugin);

	spatial_hbox = memnew(HBoxContainer);
	spatial_export = memnew(Button);
	spatial_export->set_theme_type_variation(SceneStringName(FlatButton));
	spatial_export->set_text(TTR("Export Navimesh"));
	spatial_export->set_tooltip_text(TTR("Bake and export the selected NavigationRegion3D (and scene links) for a third-party server."));
	spatial_export->connect(SceneStringName(pressed), callable_mp(this, &NavimeshExportEditorPlugin::_on_toolbar_export));
	spatial_hbox->add_child(spatial_export);
	add_control_to_container(CONTAINER_SPATIAL_EDITOR_MENU, spatial_hbox);
	spatial_hbox->hide();

	canvas_hbox = memnew(HBoxContainer);
	canvas_export = memnew(Button);
	canvas_export->set_theme_type_variation(SceneStringName(FlatButton));
	canvas_export->set_text(TTR("Export Navimesh"));
	canvas_export->set_tooltip_text(TTR("Bake and export the selected NavigationRegion2D (and scene links) for a third-party server."));
	canvas_export->connect(SceneStringName(pressed), callable_mp(this, &NavimeshExportEditorPlugin::_on_toolbar_export));
	canvas_hbox->add_child(canvas_export);
	add_control_to_container(CONTAINER_CANVAS_EDITOR_MENU, canvas_hbox);
	canvas_hbox->hide();
}

NavimeshExportEditorPlugin::~NavimeshExportEditorPlugin() {
	if (inspector_plugin.is_valid()) {
		remove_inspector_plugin(inspector_plugin);
	}
}

#endif
