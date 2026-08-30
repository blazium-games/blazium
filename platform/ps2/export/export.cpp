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

#include "core/config/project_settings.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/export/editor_export.h"

void register_ps2_exporter_types() {
	GDREGISTER_VIRTUAL_CLASS(EditorExportPlatformPS2);
	GDREGISTER_CLASS(PS2);
	GDREGISTER_CLASS(PS2Spawner);
	GDREGISTER_CLASS(PS2CameraVolume);
	GDREGISTER_CLASS(PS2Follow);
	GDREGISTER_CLASS(PS2Waypoint);
	GDREGISTER_CLASS(PS2Path);
	GDREGISTER_CLASS(PS2NavRegion);
	GDREGISTER_CLASS(PS2Vehicle);
	GDREGISTER_CLASS(PS2Particles);
	GDREGISTER_CLASS(PS2Tween);
	GDREGISTER_CLASS(PS2FlavorEditorPlugin);
}

static void _ps2_flavor_init() {
	if (EditorNode::get_singleton()) {
		EditorNode::get_singleton()->add_editor_plugin(memnew(PS2FlavorEditorPlugin));
	}
}

void register_ps2_exporter() {
#if defined(WINDOWS_ENABLED) || defined(LINUXBSD_ENABLED)
	GLOBAL_DEF(PropertyInfo(Variant::BOOL, "ps2/enabled"), false);

	EDITOR_DEF("export/ps2/toolchain", "");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "export/ps2/toolchain", PROPERTY_HINT_GLOBAL_FILE, "*.exe,*.bin,*"));
	EDITOR_DEF("export/ps2/runtime_src", "");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "export/ps2/runtime_src", PROPERTY_HINT_GLOBAL_DIR));
	EDITOR_DEF("export/ps2/runtime_overlay", "");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "export/ps2/runtime_overlay", PROPERTY_HINT_GLOBAL_DIR));

	Ref<EditorExportPlatformPS2> platform;
	platform.instantiate();
	EditorExport::get_singleton()->add_export_platform(platform);
	EditorNode::add_init_callback(_ps2_flavor_init);
#endif
}
