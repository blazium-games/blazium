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

#include "core/object/class_db.h"
#include "register_types.h"

#include "dddbrowser_audio.h"
#include "dddbrowser_exporter.h"
#include "dddbrowser_font.h"
#include "dddbrowser_level.h"
#include "dddbrowser_model.h"
#include "dddbrowser_picturebox.h"
#include "dddbrowser_portal.h"
#include "dddbrowser_preview_server.h"
#include "dddbrowser_script.h"
#include "dddbrowser_spawn.h"
#include "dddbrowser_textbox.h"
#include "dddbrowser_volume.h"

#ifdef TOOLS_ENABLED
#include "editor/dddbrowser_editor_plugin.h"
#include "editor/plugins/editor_plugin.h"
#endif

void initialize_dddbrowser_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(DDDBrowserLevel);
		GDREGISTER_CLASS(DDDBrowserSpawn);
		GDREGISTER_CLASS(DDDBrowserPortal);
		GDREGISTER_CLASS(DDDBrowserVolume);
		GDREGISTER_CLASS(DDDBrowserModel);
		GDREGISTER_CLASS(DDDBrowserTextbox);
		GDREGISTER_CLASS(DDDBrowserPicturebox);
		GDREGISTER_CLASS(DDDBrowserAudio);
		GDREGISTER_CLASS(DDDBrowserFont);
		GDREGISTER_CLASS(DDDBrowserScript);
		GDREGISTER_CLASS(DDDBrowserExporter);
		GDREGISTER_CLASS(DDDBrowserPreviewServer);
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		EditorPlugins::add_by_type<DDDBrowserEditorPlugin>();
	}
#endif
}

void uninitialize_dddbrowser_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
	}
}
