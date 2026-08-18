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

#ifdef TOOLS_ENABLED
#include "cold_storage_editor_plugin.h"
#include "cold_storage_settings.h"
#include "cold_storage_settings_ui.h"
#include "cold_storage_vcs.h"
#include "editor/editor_node.h"
#include "editor/plugins/editor_plugin.h"
#endif

void initialize_coldstorage_module(ModuleInitializationLevel p_level) {
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		cold_storage_register_project_settings();
		GDREGISTER_CLASS(ColdStorageVCS);
		GDREGISTER_CLASS(ColdStorageSettingsUI);
		GDREGISTER_CLASS(ColdStorageSettingsInspectorPlugin);
		GDREGISTER_CLASS(ColdStorageEditorPlugin);
		EditorPlugins::add_by_type<ColdStorageEditorPlugin>();
	}
#else
	(void)p_level;
#endif
}

void uninitialize_coldstorage_module(ModuleInitializationLevel p_level) {
	(void)p_level;
}
