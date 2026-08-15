/**************************************************************************/
/*  xbox_editor_plugin.cpp                                                */
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

#include "core/object/class_db.h"
#include "core/object/callable_mp.h"
#include "xbox_editor_plugin.h"

#include "export/xbox_export_platform.h"
#include "gdk_toolchain.h"
#include "microsoft_game_config.h"

#include "core/config/engine.h"
#include "editor/editor_node.h"
#include "editor/export/editor_export.h"
#include "modules/xbox_module/gdk/gdk.h"

void XboxEditorPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_create_game_config"), &XboxEditorPlugin::_create_game_config);
	ClassDB::bind_method(D_METHOD("_validate_configuration"), &XboxEditorPlugin::_validate_configuration);
	ClassDB::bind_method(D_METHOD("_open_gdk_docs"), &XboxEditorPlugin::_open_gdk_docs);
}

void XboxEditorPlugin::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		export_platform = Ref<EditorExportPlatform>(memnew(EditorExportPlatformXbox));
		EditorExport::get_singleton()->add_export_platform(export_platform);

		add_tool_menu_item("Xbox: Create Game Config", callable_mp(this, &XboxEditorPlugin::_create_game_config));
		add_tool_menu_item("Xbox: Validate Configuration", callable_mp(this, &XboxEditorPlugin::_validate_configuration));
		add_tool_menu_item("Xbox: Open GDK Documentation", callable_mp(this, &XboxEditorPlugin::_open_gdk_docs));
	} else if (p_what == NOTIFICATION_EXIT_TREE) {
		if (export_platform.is_valid()) {
			EditorExport::get_singleton()->remove_export_platform(export_platform);
			export_platform.unref();
		}
	}
}

void XboxEditorPlugin::_create_game_config() {
	Error err = MicrosoftGameConfig::write_template_to_project();
	if (err == ERR_ALREADY_EXISTS) {
		EditorNode::get_singleton()->show_warning("MicrosoftGame.config already exists at the project root.");
		return;
	}
	if (err != OK) {
		EditorNode::get_singleton()->show_warning(vformat("Failed to create MicrosoftGame.config (error %d).", err));
		return;
	}
	EditorNode::get_singleton()->show_warning("Created MicrosoftGame.config template at project root. Edit Partner Center values before exporting.");
}

void XboxEditorPlugin::_validate_configuration() {
	Ref<GDKToolchain> toolchain = GDKToolchain::create();
	String message;
	if (!toolchain->is_gdk_available()) {
		message = "GDK tools not found. Install via: winget install Microsoft.Gaming.GDK";
	} else {
		message = vformat("GDK tools found (version %s).", toolchain->get_gdk_version().is_empty() ? "unknown" : toolchain->get_gdk_version());
	}

	String config_error;
	const String exe_name = MicrosoftGameConfig::validate_project_config(&config_error);
	if (exe_name.is_empty()) {
		message += "\n" + config_error;
	} else {
		message += vformat("\nMicrosoftGame.config OK (executable: %s).", exe_name);
	}

	GDK *gdk = GDK::get_singleton();
	if (gdk) {
		message += vformat("\nGDK runtime available: %s", gdk->is_available() ? "yes" : "no (stub build or GDK not linked)");
	}

	EditorNode::get_singleton()->show_warning(message);
}

void XboxEditorPlugin::_open_gdk_docs() {
	OS::get_singleton()->shell_open("https://learn.microsoft.com/gaming/gdk/");
}
