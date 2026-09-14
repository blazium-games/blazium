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

#include "fgd/blazium_fgd_base_class.h"
#include "fgd/blazium_fgd_entity_class.h"
#include "fgd/blazium_fgd_file.h"
#include "fgd/blazium_fgd_model_point_class.h"
#include "fgd/blazium_fgd_point_class.h"
#include "fgd/blazium_fgd_point_class_display_descriptor.h"
#include "fgd/blazium_fgd_solid_class.h"
#include "import/quake_map_file.h"
#include "import/quake_palette_file.h"
#include "import/quake_wad_file.h"
#include "netradiant/netradiant_custom_gamepack_config.h"
#include "netradiant/netradiant_custom_shader.h"
#include "trenchbroom/trenchbroom_game_config.h"
#include "trenchbroom/trenchbroom_tag.h"
#include "trenchbroom_defaults.h"
#include "trenchbroom_local_config.h"
#include "trenchbroom_map.h"
#include "trenchbroom_map_settings.h"

#ifdef TOOLS_ENABLED
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/resource_importer.h"
#include "editor/editor_node.h"
#include "editor/trenchbroom_editor_plugin.h"
#include "import/resource_importer_map.h"
#include "import/resource_importer_palette.h"
#include "import/resource_importer_wad.h"
#include "import/resource_importer_wal.h"
#endif

#ifdef TESTS_ENABLED
#include "tests/test_trenchbroom_parser.h"
#endif

#ifdef TOOLS_ENABLED
namespace {

void _register_project_setting(const String &p_name, const Variant &p_default, PropertyHint p_hint = PROPERTY_HINT_NONE, const String &p_hint_string = String()) {
	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (!project_settings->has_setting(p_name)) {
		project_settings->set_setting(p_name, p_default);
		project_settings->set_initial_value(p_name, p_default);
		project_settings->set_as_basic(p_name, true);
	}
	if (p_hint != PROPERTY_HINT_NONE) {
		PropertyInfo info(Variant::STRING, p_name, p_hint, p_hint_string);
		project_settings->set_custom_property_info(info);
	}
}

void _register_trenchbroom_project_settings() {
	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	const String auto_defaults_path = "res://trenchbroom_defaults";
	if (project_settings && (!project_settings->has_setting("blazium/trenchbroom/defaults_path") || String(project_settings->get_setting("blazium/trenchbroom/defaults_path")).is_empty())) {
		if (DirAccess::dir_exists_absolute(auto_defaults_path)) {
			project_settings->set_setting("blazium/trenchbroom/defaults_path", auto_defaults_path);
		}
	}
	String defaults_path_value;
	if (project_settings && project_settings->has_setting("blazium/trenchbroom/defaults_path")) {
		defaults_path_value = project_settings->get_setting("blazium/trenchbroom/defaults_path");
	}
	_register_project_setting(
			"blazium/trenchbroom/defaults_path",
			defaults_path_value,
			PROPERTY_HINT_DIR,
			"Link or clone the trenchbroom_defaults repo here (res://trenchbroom_defaults)");
	const String default_settings_path = TrenchbroomDefaults::resolve_defaults_path("trenchbroom_default_map_settings.tres");
	if (default_settings_path.is_empty()) {
		_register_project_setting("blazium/trenchbroom/default_map_settings", String(), PROPERTY_HINT_FILE, "*.tres");
	} else {
		_register_project_setting("blazium/trenchbroom/default_map_settings", default_settings_path, PROPERTY_HINT_FILE, "*.tres");
	}
	_register_project_setting("blazium/trenchbroom/default_inverse_scale_factor", 32.0);
	_register_project_setting("blazium/trenchbroom/model_point_class_save_path", String());
}

} //namespace

static void _trenchbroom_editor_init() {
	const String defaults_dir = TrenchbroomDefaults::get_defaults_dir();
	if (!defaults_dir.is_empty()) {
		TrenchbroomDefaults::ensure_default_assets();
	}
	ResourceFormatImporter::get_singleton()->add_importer(memnew(ResourceImporterQuakeMap));
	ResourceFormatImporter::get_singleton()->add_importer(memnew(ResourceImporterQuakePalette));
	ResourceFormatImporter::get_singleton()->add_importer(memnew(ResourceImporterQuakeWad));
	ResourceFormatImporter::get_singleton()->add_importer(memnew(ResourceImporterQuakeWal));
	EditorPlugins::add_by_type<TrenchbroomEditorPlugin>();
}
#endif

void initialize_trenchbroom_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(TrenchbroomMap);
		GDREGISTER_CLASS(TrenchbroomMapSettings);
		GDREGISTER_CLASS(TrenchbroomDefaults);
		GDREGISTER_CLASS(TrenchbroomLocalConfig);
		GDREGISTER_CLASS(BlaziumFGDFile);
		GDREGISTER_CLASS(BlaziumFGDEntityClass);
		GDREGISTER_CLASS(BlaziumFGDBaseClass);
		GDREGISTER_CLASS(BlaziumFGDSolidClass);
		GDREGISTER_CLASS(BlaziumFGDPointClass);
		GDREGISTER_CLASS(BlaziumFGDModelPointClass);
		GDREGISTER_CLASS(BlaziumFGDPointClassDisplayDescriptor);
		GDREGISTER_CLASS(TrenchbroomGameConfig);
		GDREGISTER_CLASS(TrenchbroomTag);
		GDREGISTER_CLASS(NetRadiantCustomShader);
		GDREGISTER_CLASS(NetRadiantCustomGamePackConfig);
		GDREGISTER_CLASS(QuakeMapFile);
		GDREGISTER_CLASS(QuakeWadFile);
		GDREGISTER_CLASS(QuakePaletteFile);
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		_register_trenchbroom_project_settings();
		GDREGISTER_CLASS(ResourceImporterQuakeMap);
		GDREGISTER_CLASS(ResourceImporterQuakePalette);
		GDREGISTER_CLASS(ResourceImporterQuakeWad);
		GDREGISTER_CLASS(ResourceImporterQuakeWal);
		GDREGISTER_CLASS(TrenchbroomEditorPlugin);
		EditorNode::add_init_callback(_trenchbroom_editor_init);
	}
#endif
}

void uninitialize_trenchbroom_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
	}
}
