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

#include "claim.h"
#include "key.h"
#include "obfuscation.h"
#include "scan.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/object/class_db.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_node.h"
#include "editor/export/editor_export.h"
#include "editor/obfuscation_editor_plugin.h"
#include "editor/obfuscation_export_plugin.h"
#include "editor/plugins/editor_plugin.h"
#endif

static Obfuscation *obfuscation_singleton = nullptr;
#ifdef TOOLS_ENABLED
static Ref<ObfuscationExportPlugin> obfuscation_export_plugin;

static void _obfuscation_editor_init() {
	if (!EditorExport::get_singleton() || obfuscation_export_plugin.is_valid()) {
		return;
	}
	obfuscation_export_plugin.instantiate();
	EditorExport::get_singleton()->add_export_plugin(obfuscation_export_plugin);
}
#endif

static void register_obfuscation_settings() {
	GLOBAL_DEF_BASIC("obfuscation/enabled", true);
	GLOBAL_DEF_BASIC("obfuscation/project_id", String());
	GLOBAL_DEF_BASIC("obfuscation/publisher_id", String());
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::STRING, "obfuscation/copyright/text", PROPERTY_HINT_MULTILINE_TEXT), String());
	GLOBAL_DEF_BASIC("obfuscation/copyright/author", String());
	GLOBAL_DEF_BASIC("obfuscation/copyright/license", String());
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "obfuscation/copyright/shard_count", PROPERTY_HINT_RANGE, "2,64,1"), 8);
	GLOBAL_DEF_BASIC("obfuscation/textures/dwt", true);
	GLOBAL_DEF_BASIC("obfuscation/textures/gutter", false);
	GLOBAL_DEF_BASIC("obfuscation/textures/overlay", false);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "obfuscation/textures/min_size", PROPERTY_HINT_RANGE, "64,4096,1"), 256);
	GLOBAL_DEF_BASIC("obfuscation/audio/spread_spectrum", true);
	GLOBAL_DEF_BASIC("obfuscation/audio/spectro_path", String());
	GLOBAL_DEF_BASIC("obfuscation/scripts/inject_ck", true);
	GLOBAL_DEF_BASIC("obfuscation/scripts/inject_copyright", true);
	GLOBAL_DEF_BASIC("obfuscation/scripts/minify", true);
	GLOBAL_DEF_BASIC("obfuscation/scripts/comment_lattice", true);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "obfuscation/scripts/comment_decoys", PROPERTY_HINT_RANGE, "0,64,1"), 8);
	GLOBAL_DEF_BASIC("obfuscation/pack/inject_seal", true);
	GLOBAL_DEF_BASIC("obfuscation/pack/scramble_names", true);
}

void initialize_obfuscation_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(ObfuscationClaim);
		GDREGISTER_CLASS(ObfuscationScan);
		GDREGISTER_CLASS(ObfuscationKey);
		GDREGISTER_CLASS(Obfuscation);
		register_obfuscation_settings();
		obfuscation_singleton = memnew(Obfuscation);
		Engine::get_singleton()->add_singleton(Engine::Singleton("Obfuscation", obfuscation_singleton));
	}
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		EditorNode::add_init_callback(_obfuscation_editor_init);
	}
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		EditorPlugins::add_by_type<ObfuscationEditorPlugin>();
	}
#endif
}

void uninitialize_obfuscation_module(ModuleInitializationLevel p_level) {
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		// EditorExport is a Node freed in SceneTree::finalize() before module
		// uninit, so do not call get_singleton() or remove_export_plugin here.
		obfuscation_export_plugin.unref();
		return;
	}
#endif
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		if (obfuscation_singleton) {
			Engine::get_singleton()->remove_singleton("Obfuscation");
			memdelete(obfuscation_singleton);
			obfuscation_singleton = nullptr;
		}
	}
}
