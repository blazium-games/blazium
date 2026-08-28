/**************************************************************************/
/*  export.cpp                                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* SPDX-License-Identifier: MIT                                           */
/**************************************************************************/

#include "export.h"

#include "export_plugin.h"
#include "flavor_plugin.h"

#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/export/editor_export.h"

void register_ps1_exporter_types() {
	GDREGISTER_VIRTUAL_CLASS(EditorExportPlatformPS1);
	GDREGISTER_CLASS(PS1FlavorEditorPlugin);
}

static void _ps1_flavor_init() {
	if (EditorNode::get_singleton()) {
		EditorNode::get_singleton()->add_editor_plugin(memnew(PS1FlavorEditorPlugin));
	}
}

void register_ps1_exporter() {
#if defined(WINDOWS_ENABLED) || defined(LINUXBSD_ENABLED)
	EDITOR_DEF("export/ps1/toolchain", "");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "export/ps1/toolchain", PROPERTY_HINT_GLOBAL_FILE, "*.exe,*.bin,*"));
	EDITOR_DEF("export/ps1/runtime_src", "");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "export/ps1/runtime_src", PROPERTY_HINT_GLOBAL_DIR));

	Ref<EditorExportPlatformPS1> platform;
	platform.instantiate();
	EditorExport::get_singleton()->add_export_platform(platform);
	EditorNode::add_init_callback(_ps1_flavor_init);
#endif
}
