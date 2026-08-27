/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "register_types.h"

#include "author/inter_dvd_ifo_writer.h"
#include "author/inter_dvd_project.h"
#include "machine/inter_dvd_instruction.h"
#include "machine/inter_dvd_machine.h"

#include "core/config/project_settings.h"

#ifdef TOOLS_ENABLED
#include "editor/export/windows_inter_dvd_export_platform.h"
#include "editor/inter_dvd_editor_plugin.h"
#include "editor/inter_dvd_scene_baker.h"
#include "editor/plugins/editor_plugin.h"
#endif

void initialize_inter_dvd_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_CORE) {
		GLOBAL_DEF_BASIC(PropertyInfo(Variant::STRING, "blazium/inter_dvd/project", PROPERTY_HINT_FILE, "*.tres,*.res"), String());
		GLOBAL_DEF_BASIC("blazium/inter_dvd/default_region_mask", 1);
		GLOBAL_DEF_BASIC("blazium/inter_dvd/default_parental_level", 1);
		GLOBAL_DEF_BASIC(PropertyInfo(Variant::STRING, "blazium/inter_dvd/cache_path", PROPERTY_HINT_DIR), "res://.inter_dvd_cache");
		GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "blazium/inter_dvd/ac3_bitrate_k", PROPERTY_HINT_RANGE, "64,448,32"), 192);
		GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "blazium/inter_dvd/ac3_channels", PROPERTY_HINT_RANGE, "1,6,1"), 2);
		GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "blazium/inter_dvd/gop_size", PROPERTY_HINT_RANGE, "1,30,1"), 15);
		GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "blazium/inter_dvd/title_safe_bottom", PROPERTY_HINT_RANGE, "2,480,2"), 432);
		GLOBAL_DEF_BASIC(PropertyInfo(Variant::FLOAT, "blazium/inter_dvd/pip_blackdetect_sec", PROPERTY_HINT_RANGE, "0,10,0.1"), 2.5);
		GLOBAL_DEF_BASIC(PropertyInfo(Variant::FLOAT, "blazium/inter_dvd/pip_blackdetect_pix_th", PROPERTY_HINT_RANGE, "0,1,0.01"), 0.12);
		GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "blazium/inter_dvd/bake_warmup_frames", PROPERTY_HINT_RANGE, "0,30,1"), 2);
		return;
	}

	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(InterDVDInstruction);
		GDREGISTER_CLASS(InterDVDMachine);
		GDREGISTER_CLASS(InterDVDStream);
		GDREGISTER_CLASS(InterDVDCell);
		GDREGISTER_CLASS(InterDVDButton);
		GDREGISTER_CLASS(InterDVDMenu);
		GDREGISTER_CLASS(InterDVDPGC);
		GDREGISTER_CLASS(InterDVDProject);
		GDREGISTER_CLASS(InterDVDExportProgress);
		GDREGISTER_CLASS(InterDVDIfoWriter);
		return;
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		GDREGISTER_VIRTUAL_CLASS(EditorExportPlatformWindowsInterDVD);
		GDREGISTER_CLASS(InterDVDEditorPlugin);
		GDREGISTER_CLASS(InterDVDSceneBaker);
		EditorPlugins::add_by_type<InterDVDEditorPlugin>();
	}
#endif
}

void uninitialize_inter_dvd_module(ModuleInitializationLevel p_level) {
	(void)p_level;
}
