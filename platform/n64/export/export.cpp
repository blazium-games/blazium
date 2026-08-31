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

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/export/editor_export.h"

void register_n64_exporter_types() {
	GDREGISTER_VIRTUAL_CLASS(EditorExportPlatformN64);
	GDREGISTER_CLASS(N64);
	GDREGISTER_CLASS(N64Checkpoint);
	GDREGISTER_CLASS(N64CameraVolume);
	GDREGISTER_CLASS(N64Follow);
	GDREGISTER_CLASS(N64Path);
	GDREGISTER_CLASS(N64Waypoint);
	GDREGISTER_CLASS(N64NavRegion);
	GDREGISTER_CLASS(N64Particles);
	GDREGISTER_CLASS(N64Vehicle);
	GDREGISTER_CLASS(N64Tween);
	GDREGISTER_CLASS(N64Spawner);
	GDREGISTER_CLASS(N64LoadZone);
	GDREGISTER_CLASS(N64UnloadZone);
	GDREGISTER_CLASS(N64FlavorEditorPlugin);
}

static void _n64_flavor_init() {
	if (EditorNode::get_singleton()) {
		EditorNode::get_singleton()->add_editor_plugin(memnew(N64FlavorEditorPlugin));
	}
}

void register_n64_exporter() {
#if defined(WINDOWS_ENABLED) || defined(LINUXBSD_ENABLED)
	GLOBAL_DEF(PropertyInfo(Variant::BOOL, "n64/enabled"), false);

	EDITOR_DEF("export/n64/toolchain", "");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "export/n64/toolchain", PROPERTY_HINT_GLOBAL_FILE, "*.exe,*.bin,*"));
	EDITOR_DEF("export/n64/runtime_src", "");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "export/n64/runtime_src", PROPERTY_HINT_GLOBAL_DIR));
	EDITOR_DEF("export/n64/runtime_overlay", "");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "export/n64/runtime_overlay", PROPERTY_HINT_GLOBAL_DIR));

	Ref<EditorExportPlatformN64> platform;
	platform.instantiate();
	EditorExport::get_singleton()->add_export_platform(platform);
	if (Engine::get_singleton() && !Engine::get_singleton()->has_singleton("N64")) {
		Engine::get_singleton()->add_singleton(Engine::Singleton("N64", memnew(N64)));
	}
	EditorNode::add_init_callback(_n64_flavor_init);
#endif
}
