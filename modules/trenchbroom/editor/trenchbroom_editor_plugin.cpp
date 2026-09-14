/**************************************************************************/
/*  trenchbroom_editor_plugin.cpp                                         */
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

#include "trenchbroom_editor_plugin.h"

#include "modules/trenchbroom/trenchbroom_map.h"

#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/object/class_db.h"
#include "editor/editor_interface.h"
#include "editor/editor_string_names.h"
#include "scene/gui/label.h"
#include "scene/gui/progress_bar.h"

namespace {

void _register_editor_icons() {
	EditorInterface *editor = EditorInterface::get_singleton();
	if (!editor) {
		return;
	}
	Ref<Theme> theme = editor->get_editor_theme();
	if (theme.is_null()) {
		return;
	}
	const String icon_dir = "res://modules/trenchbroom/icons";
	struct IconEntry {
		const char *class_name;
		const char *icon_file;
	};
	static const IconEntry entries[] = {
		{ "TrenchbroomMap", "trenchbroom_map.svg" },
		{ "TrenchbroomMapParser", "trenchbroom_parser.svg" },
		{ "BlaziumFGDFile", "blazium_fgd.svg" },
		{ "TrenchbroomGeometryGenerator", "trenchbroom_geometry.svg" },
		{ "BlaziumFGDModelPointClass", "blazium_fgd_model_point.svg" },
		{ "QuakeMapFile", "quake_file.svg" },
		{ "TrenchbroomEditorPlugin", "trenchbroom_editor.svg" },
	};
	for (const IconEntry &entry : entries) {
		const String path = icon_dir.path_join(entry.icon_file);
		if (FileAccess::exists(path)) {
			theme->set_icon(entry.class_name, EditorStringName(EditorIcons), ResourceLoader::load(path));
		}
	}
}

} //namespace

void TrenchbroomEditorPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_build_progress", "step", "progress"), &TrenchbroomEditorPlugin::_on_build_progress);
	ClassDB::bind_method(D_METHOD("_on_build_finished"), &TrenchbroomEditorPlugin::_on_build_finished);
	ClassDB::bind_method(D_METHOD("_on_build_failed"), &TrenchbroomEditorPlugin::_on_build_failed);
}

void TrenchbroomEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (progress_container) {
				break;
			}
			_register_editor_icons();
			progress_container = _create_progress_bar();
			progress_container->set_visible(false);
			add_control_to_container(CONTAINER_INSPECTOR_BOTTOM, progress_container);
		} break;
	}
}

Control *TrenchbroomEditorPlugin::_create_progress_bar() {
	Control *container = memnew(Control);
	container->set_name("TrenchbroomBuildProgress");

	progress_label = memnew(Label);
	progress_label->set_name("ProgressLabel");
	progress_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	progress_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	progress_label->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	progress_label->set_offset(Side::SIDE_TOP, -9);
	progress_label->set_offset(Side::SIDE_LEFT, 3);

	progress_bar = memnew(ProgressBar);
	progress_bar->set_name("ProgressBar");
	progress_bar->set_show_percentage(false);
	progress_bar->set_min(0.0);
	progress_bar->set_max(1.0);
	progress_bar->set_custom_minimum_size(Size2(0, 30));
	progress_bar->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	progress_bar->add_child(progress_label);

	container->add_child(progress_bar);
	progress_bar->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	container->set_custom_minimum_size(Size2(0, 30));

	return container;
}

void TrenchbroomEditorPlugin::_disconnect_map_signals(TrenchbroomMap *p_map) {
	if (!p_map) {
		return;
	}
	if (p_map->is_connected("build_progress", callable_mp(this, &TrenchbroomEditorPlugin::_on_build_progress))) {
		p_map->disconnect("build_progress", callable_mp(this, &TrenchbroomEditorPlugin::_on_build_progress));
	}
	if (p_map->is_connected("build_complete", callable_mp(this, &TrenchbroomEditorPlugin::_on_build_finished))) {
		p_map->disconnect("build_complete", callable_mp(this, &TrenchbroomEditorPlugin::_on_build_finished));
	}
	if (p_map->is_connected("build_failed", callable_mp(this, &TrenchbroomEditorPlugin::_on_build_failed))) {
		p_map->disconnect("build_failed", callable_mp(this, &TrenchbroomEditorPlugin::_on_build_failed));
	}
}

void TrenchbroomEditorPlugin::_connect_map_signals(TrenchbroomMap *p_map) {
	if (!p_map) {
		return;
	}
	p_map->connect("build_progress", callable_mp(this, &TrenchbroomEditorPlugin::_on_build_progress));
	p_map->connect("build_complete", callable_mp(this, &TrenchbroomEditorPlugin::_on_build_finished));
	p_map->connect("build_failed", callable_mp(this, &TrenchbroomEditorPlugin::_on_build_failed));
}

void TrenchbroomEditorPlugin::_on_build_progress(const String &p_step, real_t p_progress) {
	if (progress_bar) {
		progress_bar->set_value(p_progress);
	}
	if (progress_label) {
		String label_text = p_step;
		if (!label_text.is_empty()) {
			label_text = label_text.substr(0, 1).to_upper() + label_text.substr(1);
		}
		progress_label->set_text(label_text);
	}
}

void TrenchbroomEditorPlugin::_on_build_finished() {
	if (progress_label) {
		progress_label->set_text("Build Complete");
	}
	if (progress_bar) {
		progress_bar->set_value(1.0);
	}

	TrenchbroomMap *edited_map = Object::cast_to<TrenchbroomMap>(ObjectDB::get_instance(edited_map_id));
	_disconnect_map_signals(edited_map);
}

void TrenchbroomEditorPlugin::_on_build_failed() {
	if (progress_label) {
		progress_label->set_text("Build Failed");
	}
	if (progress_bar) {
		progress_bar->set_value(0.0);
	}

	TrenchbroomMap *edited_map = Object::cast_to<TrenchbroomMap>(ObjectDB::get_instance(edited_map_id));
	_disconnect_map_signals(edited_map);
}

bool TrenchbroomEditorPlugin::handles(Object *p_object) const {
	return Object::cast_to<TrenchbroomMap>(p_object) != nullptr;
}

void TrenchbroomEditorPlugin::edit(Object *p_object) {
	TrenchbroomMap *previous_map = Object::cast_to<TrenchbroomMap>(ObjectDB::get_instance(edited_map_id));
	_disconnect_map_signals(previous_map);

	TrenchbroomMap *map = Object::cast_to<TrenchbroomMap>(p_object);
	edited_map_id = map ? map->get_instance_id() : ObjectID();
	_connect_map_signals(map);
}

void TrenchbroomEditorPlugin::make_visible(bool p_visible) {
	if (progress_container) {
		progress_container->set_visible(p_visible);
	}
}

TrenchbroomEditorPlugin::~TrenchbroomEditorPlugin() {
	TrenchbroomMap *edited_map = Object::cast_to<TrenchbroomMap>(ObjectDB::get_instance(edited_map_id));
	_disconnect_map_signals(edited_map);

	if (progress_container) {
		remove_control_from_container(CONTAINER_INSPECTOR_BOTTOM, progress_container);
		memdelete(progress_container);
		progress_container = nullptr;
		progress_bar = nullptr;
		progress_label = nullptr;
	}
}

#endif
