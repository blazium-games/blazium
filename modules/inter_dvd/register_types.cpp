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
#include "scene/inter_dvd_chapter.h"
#include "scene/inter_dvd_disc.h"
#include "scene/inter_dvd_hotspot.h"
#include "scene/inter_dvd_menu_page.h"
#include "scene/inter_dvd_title.h"
#include "scene/inter_dvd_title_set.h"

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
		GLOBAL_DEF_BASIC(PropertyInfo(Variant::STRING, "blazium/inter_dvd/cache_path", PROPERTY_HINT_DIR), "res://.inter_dvd_cache");
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
		GDREGISTER_CLASS(InterDVDHotspot);
		GDREGISTER_CLASS(InterDVDChapter);
		GDREGISTER_CLASS(InterDVDTitle);
		GDREGISTER_CLASS(InterDVDTitleSet);
		GDREGISTER_CLASS(InterDVDMenuPage);
		GDREGISTER_CLASS(InterDVDDisc);
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
