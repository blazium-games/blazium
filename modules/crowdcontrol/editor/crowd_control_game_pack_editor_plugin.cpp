/**************************************************************************/
/*  crowd_control_game_pack_editor_plugin.cpp                             */
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

#include "crowd_control_game_pack_editor_plugin.h"

#ifdef TOOLS_ENABLED

#include "editor/editor_node.h"
#include "editor/gui/editor_file_dialog.h"
#include "scene/gui/box_container.h"

// CrowdControlGamePackInspectorPlugin

bool CrowdControlGamePackInspectorPlugin::can_handle(Object *p_object) {
	return Object::cast_to<CrowdControlGamePack>(p_object) != nullptr;
}

void CrowdControlGamePackInspectorPlugin::parse_begin(Object *p_object) {
	CrowdControlGamePack *game_pack = Object::cast_to<CrowdControlGamePack>(p_object);
	ERR_FAIL_NULL(game_pack);

	current_game_pack = game_pack;

	// Create export button
	export_button = memnew(Button);
	export_button->set_text("Export to JSON");
	export_button->connect("pressed", callable_mp(this, &CrowdControlGamePackInspectorPlugin::_on_export_button_pressed));

	// Add button to inspector
	add_custom_control(export_button);

	// Create file dialog (if not already created)
	if (!file_dialog) {
		file_dialog = memnew(EditorFileDialog);
		file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
		file_dialog->add_filter("*.json", "JSON Files");
		file_dialog->set_title("Export Crowd Control Game Pack");
		file_dialog->connect("file_selected", callable_mp(this, &CrowdControlGamePackInspectorPlugin::_on_file_selected));
		EditorNode::get_singleton()->get_gui_base()->add_child(file_dialog);
	}
}

void CrowdControlGamePackInspectorPlugin::_on_export_button_pressed() {
	ERR_FAIL_NULL(current_game_pack);
	ERR_FAIL_NULL(file_dialog);

	// Get default filename from game name
	String default_filename = "game_pack.json";
	Ref<CrowdControlGamePackMeta> meta = current_game_pack->get_pack_meta();
	if (meta.is_valid()) {
		Variant name_var = meta->get_game_name();
		String name_str;
		if (name_var.get_type() == Variant::STRING) {
			name_str = name_var;
		} else if (name_var.get_type() == Variant::DICTIONARY) {
			Dictionary name_dict = name_var;
			if (name_dict.has("public")) {
				name_str = name_dict["public"];
			}
		}

		if (!name_str.is_empty()) {
			// Sanitize filename
			name_str = name_str.replace(" ", "_").replace("/", "_").replace("\\", "_");
			default_filename = name_str.to_lower() + ".json";
		}
	}

	file_dialog->set_current_file(default_filename);
	file_dialog->popup_file_dialog();
}

void CrowdControlGamePackInspectorPlugin::_on_file_selected(const String &p_path) {
	ERR_FAIL_NULL(current_game_pack);

	Error err = current_game_pack->export_to_json(p_path);

	if (err == OK) {
		print_line(vformat("CrowdControl: Game pack exported successfully to: %s", p_path));
	} else {
		ERR_PRINT(vformat("CrowdControl: Failed to export game pack to: %s", p_path));
	}
}

CrowdControlGamePackInspectorPlugin::CrowdControlGamePackInspectorPlugin() {
}

// CrowdControlGamePackEditorPlugin

CrowdControlGamePackEditorPlugin::CrowdControlGamePackEditorPlugin() {
	inspector_plugin.instantiate();
	add_inspector_plugin(inspector_plugin);
}

CrowdControlGamePackEditorPlugin::~CrowdControlGamePackEditorPlugin() {
	remove_inspector_plugin(inspector_plugin);
}

#endif // TOOLS_ENABLED
