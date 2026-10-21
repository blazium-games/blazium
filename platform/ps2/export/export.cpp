/**************************************************************************/
/*  export.cpp                                                            */
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
	GDREGISTER_CLASS(PS2SpawnPoint);
	GDREGISTER_CLASS(PS2Checkpoint);
	GDREGISTER_CLASS(PS2Portal);
	GDREGISTER_CLASS(PS2SavePoint);
	GDREGISTER_CLASS(PS2MusicZone);
	GDREGISTER_CLASS(PS2LoadZone);
	GDREGISTER_CLASS(PS2UnloadZone);
	GDREGISTER_CLASS(PS2Hazard);
	GDREGISTER_CLASS(PS2Pickup);
	GDREGISTER_CLASS(PS2Talk);
	GDREGISTER_CLASS(PS2FlavorEditorPlugin);
}

#if defined(WINDOWS_ENABLED) || defined(LINUXBSD_ENABLED)
static void _ps2_flavor_init() {
	if (EditorNode::get_singleton()) {
		EditorNode::get_singleton()->add_editor_plugin(memnew(PS2FlavorEditorPlugin));
	}
}
#endif

void register_ps2_exporter() {
#if defined(WINDOWS_ENABLED) || defined(LINUXBSD_ENABLED)
	GLOBAL_DEF(PropertyInfo(Variant::BOOL, "ps2/enabled"), false);
	// Scripts on cooked nodes compile to GDIR; when one cannot (an API outside
	// the PS2 catalog), the export falls back to the keyword tape with a warning,
	// or fails when this is on.
	GLOBAL_DEF(PropertyInfo(Variant::BOOL, "ps2/scripts/require_compiled"), false);
	// Sound effects cooked into SFX00 as a bank, in this order; PS2.play_sfx()
	// takes an index or one of these paths. Empty: the first .wav in the project.
	GLOBAL_DEF(PropertyInfo(Variant::PACKED_STRING_ARRAY, "ps2/audio/sfx_bank", PROPERTY_HINT_TYPE_STRING, vformat("%d/%d:*.wav", Variant::STRING, PROPERTY_HINT_FILE)), PackedStringArray());

	EDITOR_DEF("export/ps2/toolchain", "");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "export/ps2/toolchain", PROPERTY_HINT_GLOBAL_FILE, "*.exe,*.bin,*"));
	EDITOR_DEF("export/ps2/runtime_src", "");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "export/ps2/runtime_src", PROPERTY_HINT_GLOBAL_DIR));
	EDITOR_DEF("export/ps2/runtime_overlay", "");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "export/ps2/runtime_overlay", PROPERTY_HINT_GLOBAL_DIR));

	Ref<EditorExportPlatformPS2> platform;
	platform.instantiate();
	// GDScript only treats PS2.* as callable when the singleton exists at
	// analyze time, so register it with the exporter, before any plugin.
	platform->set_api_singleton(memnew(PS2));
	EditorExport::get_singleton()->add_export_platform(platform);
	EditorNode::add_init_callback(_ps2_flavor_init);
#endif
}
