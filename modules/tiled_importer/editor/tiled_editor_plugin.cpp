/**************************************************************************/
/*  tiled_editor_plugin.cpp                                               */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
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

#include "tiled_editor_plugin.h"
#include "core/input/shortcut.h"
#include "editor/editor_node.h"
#include "scene/gui/box_container.h"
#include "scene/gui/separator.h"

void TiledEditorPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_reimport_pressed"), &TiledEditorPlugin::_on_reimport_pressed);
}

void TiledEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			main_panel = memnew(PanelContainer);
			main_panel->set_v_size_flags(Control::SIZE_EXPAND_FILL);

			VBoxContainer *vbox = memnew(VBoxContainer);
			main_panel->add_child(vbox);

			status_label = memnew(Label);
			status_label->set_text("Tiled Importer: Native Tiled Architecture Loaded");
			status_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
			vbox->add_child(status_label);

			HSeparator *sep = memnew(HSeparator);
			vbox->add_child(sep);

			btn_reimport = memnew(Button);
			btn_reimport->set_text("Force Rebuild Selected Tiled Map");
			btn_reimport->connect("pressed", callable_mp(this, &TiledEditorPlugin::_on_reimport_pressed));
			vbox->add_child(btn_reimport);

			add_control_to_bottom_panel(main_panel, "Tiled Maps");
		} break;
		case NOTIFICATION_EXIT_TREE: {
			if (main_panel) {
				remove_control_from_bottom_panel(main_panel);
				memdelete(main_panel);
			}
		} break;
	}
}

void TiledEditorPlugin::_on_reimport_pressed() {
	if (status_label) {
		status_label->set_text("Attempting rebuild via native TiledTilemapCreator... (Select .tmx file first)");
	}
}

void TiledEditorPlugin::make_visible(bool p_visible) {
	if (main_panel) {
		main_panel->set_visible(p_visible);
	}
}

TiledEditorPlugin::TiledEditorPlugin() {
}

TiledEditorPlugin::~TiledEditorPlugin() {
}
