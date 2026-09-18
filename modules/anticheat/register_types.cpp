/**************************************************************************/
/*  register_types.cpp                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#include "register_types.h"

#include "anticheat.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "scene/main/scene_tree.h"

#ifdef TOOLS_ENABLED
#include "core/object/class_db.h"
#include "editor/anticheat_editor_plugin.h"
#include "editor/anticheat_export_plugin.h"
#include "editor/export/editor_export.h"
#include "editor/plugins/editor_plugin.h"
#endif

#ifdef TESTS_ENABLED
#include "tests/test_anticheat.h"
#endif

static Anticheat *anticheat_singleton = nullptr;
static bool anticheat_frame_hook_connected = false;
#ifdef TOOLS_ENABLED
static Ref<AnticheatExportPlugin> anticheat_export_plugin;
#endif

static void anticheat_frame_callback() {
	if (anticheat_singleton == nullptr) {
		return;
	}
	if (!anticheat_singleton->is_initialized() && !anticheat_singleton->is_server_initialized()) {
		return;
	}
	anticheat_singleton->tick(0);
}

static void connect_anticheat_frame_hook() {
	if (anticheat_frame_hook_connected) {
		return;
	}
	SceneTree *scene_tree = SceneTree::get_singleton();
	if (scene_tree == nullptr) {
		return;
	}
	scene_tree->connect("process_frame", callable_mp_static(&anticheat_frame_callback));
	anticheat_frame_hook_connected = true;
}

void anticheat_ensure_frame_hook() {
	connect_anticheat_frame_hook();
}

static void disconnect_anticheat_frame_hook() {
	if (!anticheat_frame_hook_connected) {
		return;
	}
	SceneTree *scene_tree = SceneTree::get_singleton();
	if (scene_tree != nullptr) {
		scene_tree->disconnect("process_frame", callable_mp_static(&anticheat_frame_callback));
	}
	anticheat_frame_hook_connected = false;
}

static void anticheat_register_project_settings() {
	GLOBAL_DEF("anticheat/require_agent", false);
	GLOBAL_DEF(PropertyInfo(Variant::STRING, "anticheat/ops/mode", PROPERTY_HINT_ENUM, "internal,saas"), "internal");
	GLOBAL_DEF("anticheat/ops/address", "127.0.0.1");
	GLOBAL_DEF("anticheat/ops/port", 8730);
	GLOBAL_DEF("anticheat/ops/timeout_s", 30);
	GLOBAL_DEF("anticheat/ops/source_id", "");
	GLOBAL_DEF("anticheat/ops/title_id", "");
	GLOBAL_DEF("anticheat/ops/license_path", "bzops.license.json");
	GLOBAL_DEF("anticheat/ops/saas_endpoint", "");
	GLOBAL_DEF("anticheat/export/bin_dir", "res://anticheat/bin");
	GLOBAL_DEF("anticheat/verify_runtime_signature", false);
	GLOBAL_DEF("anticheat/runtime_public_key", "");
}

void initialize_anticheat_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		anticheat_register_project_settings();
		GDREGISTER_CLASS(Anticheat);
		anticheat_singleton = memnew(Anticheat);
		Engine::get_singleton()->add_singleton(Engine::Singleton("Anticheat", Anticheat::get_singleton()));
		connect_anticheat_frame_hook();
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		GDREGISTER_CLASS(AnticheatEditorPlugin);
		EditorPlugins::add_by_type<AnticheatEditorPlugin>();
		if (EditorExport::get_singleton()) {
			anticheat_export_plugin.instantiate();
			EditorExport::get_singleton()->add_export_plugin(anticheat_export_plugin);
		}
	}
#endif
}

void uninitialize_anticheat_module(ModuleInitializationLevel p_level) {
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		if (anticheat_export_plugin.is_valid() && EditorExport::get_singleton()) {
			EditorExport::get_singleton()->remove_export_plugin(anticheat_export_plugin);
		}
		anticheat_export_plugin.unref();
		return;
	}
#endif
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		disconnect_anticheat_frame_hook();
		if (anticheat_singleton) {
			Engine::get_singleton()->remove_singleton("Anticheat");
			memdelete(anticheat_singleton);
			anticheat_singleton = nullptr;
		}
	}
}
