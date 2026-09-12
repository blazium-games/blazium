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

#include "asset_tag_coordinator.h"
#include "asset_tag_manager.h"
#include "asset_tag_registry.h"
#include "asset_tag_runtime.h"
#include "asset_tag_storage.h"

#include "core/io/file_access.h"
#include "core/templates/pair.h"

#ifdef TOOLS_ENABLED
#include "core/object/class_db.h"
#include "editor/asset_tag_export_plugin.h"
#include "editor/asset_tags_context_menu_plugin.h"
#include "editor/asset_tags_editor_plugin.h"
#include "editor/editor_file_system.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/export/editor_export.h"
#include "editor/filesystem_dock.h"
#include "editor/plugins/editor_plugin.h"
#endif

#include "core/config/project_settings.h"

#ifdef TESTS_ENABLED
#include "tests/test_asset_tag_coordinator.cpp"
#include "tests/test_asset_tag_manager.cpp"
#include "tests/test_asset_tag_registry.cpp"
#include "tests/test_asset_tag_storage.cpp"
#endif

#ifdef TOOLS_ENABLED
static Callable g_assettags_prune_cb;
static Callable g_assettags_first_scan_cb;
static Callable g_assettags_files_moved_cb;
static Vector<Pair<String, String>> g_pending_asset_moves;
static bool g_pending_asset_moves_flush_scheduled = false;

static void _flush_pending_asset_moves() {
	g_pending_asset_moves_flush_scheduled = false;
	if (g_pending_asset_moves.is_empty()) {
		return;
	}
	AssetTagRegistry *registry = AssetTagRegistry::get_singleton();
	if (!registry) {
		g_pending_asset_moves.clear();
		return;
	}
	AssetTagCoordinatorScope scope;
	if (!scope.is_active()) {
		g_pending_asset_moves.clear();
		return;
	}
	for (int i = 0; i < g_pending_asset_moves.size(); i++) {
		registry->rename_asset_path(g_pending_asset_moves[i].first, g_pending_asset_moves[i].second);
	}
	g_pending_asset_moves.clear();
	scope.commit();
}

static void _on_asset_file_moved(const String &p_old_file, const String &p_new_file) {
	const String old_path = AssetTagStorage::normalize_asset_path(p_old_file);
	const String new_path = AssetTagStorage::normalize_asset_path(p_new_file);
	if (!AssetTagStorage::is_taggable_extension(old_path) || !AssetTagStorage::is_taggable_extension(new_path)) {
		return;
	}
	if (!AssetTagRegistry::get_singleton()) {
		return;
	}
	g_pending_asset_moves.push_back(Pair<String, String>(old_path, new_path));
	if (!g_pending_asset_moves_flush_scheduled) {
		g_pending_asset_moves_flush_scheduled = true;
		callable_mp_static(_flush_pending_asset_moves).call_deferred();
	}
}

static void _assettags_connect_prune_hooks(bool p_exist) {
	(void)p_exist;
	EditorFileSystem *efs = EditorFileSystem::get_singleton();
	if (!efs) {
		return;
	}
	if (g_assettags_first_scan_cb.is_valid() && efs->is_connected("sources_changed", g_assettags_first_scan_cb)) {
		efs->disconnect("sources_changed", g_assettags_first_scan_cb);
	}
	g_assettags_first_scan_cb = Callable();

	AssetTagRegistry *registry = AssetTagRegistry::get_singleton();
	if (!registry) {
		return;
	}
	g_assettags_prune_cb = callable_mp(registry, &AssetTagRegistry::schedule_prune_removed_paths);
	if (!efs->is_connected("filesystem_changed", g_assettags_prune_cb)) {
		efs->connect("filesystem_changed", g_assettags_prune_cb);
	}
	registry->schedule_prune_removed_paths();
}

static void _assettags_disconnect_editor_hooks() {
	if (EditorFileSystem::get_singleton()) {
		if (g_assettags_first_scan_cb.is_valid() && EditorFileSystem::get_singleton()->is_connected("sources_changed", g_assettags_first_scan_cb)) {
			EditorFileSystem::get_singleton()->disconnect("sources_changed", g_assettags_first_scan_cb);
		}
		if (g_assettags_prune_cb.is_valid() && EditorFileSystem::get_singleton()->is_connected("filesystem_changed", g_assettags_prune_cb)) {
			EditorFileSystem::get_singleton()->disconnect("filesystem_changed", g_assettags_prune_cb);
		}
	}
	g_assettags_first_scan_cb = Callable();
	g_assettags_prune_cb = Callable();
	if (FileSystemDock *fs_dock = FileSystemDock::get_singleton()) {
		if (g_assettags_files_moved_cb.is_valid() && fs_dock->is_connected("files_moved", g_assettags_files_moved_cb)) {
			fs_dock->disconnect("files_moved", g_assettags_files_moved_cb);
		}
	}
	g_assettags_files_moved_cb = Callable();
	g_pending_asset_moves.clear();
	g_pending_asset_moves_flush_scheduled = false;
}

static void _assettags_editor_init() {
	AssetTagManager *manager = memnew(AssetTagManager);
	const Error manager_load_err = manager->load();
	if (manager_load_err == ERR_FILE_CORRUPT) {
		WARN_PRINT("AssetTagManager: tag dictionary failed to load; quarantining corrupt dictionary.");
		AssetTagStorage::quarantine_corrupt_dictionary();
		(void)manager->load();
	}

	AssetTagRegistry *registry = memnew(AssetTagRegistry);
	manager->connect(SNAME("redirects_changed"), callable_mp(registry, &AssetTagRegistry::_on_redirects_changed));
	const Error registry_load_err = registry->load();
	if (registry_load_err == ERR_FILE_CORRUPT) {
		WARN_PRINT("AssetTagRegistry: asset index failed to load; attempting recovery.");
		registry->recover_after_load_failure();
	}

	memnew(AssetTagCoordinator);

	// Must add the plugin here (after manager/registry exist). EditorPlugins::add_by_type
	// in this late init callback is too late — EditorNode already finished creating plugins.
	EditorNode::get_singleton()->add_editor_plugin(memnew(AssetTagsEditorPlugin));

	Ref<EditorExportAssetTags> asset_tags_export;
	asset_tags_export.instantiate();
	EditorExport::get_singleton()->add_export_plugin(asset_tags_export);

	// Defer prune until after first filesystem scan so splash/UI are not blocked by
	// a full-index existence pass on the same frame the editor unlocks.
	if (EditorFileSystem *efs = EditorFileSystem::get_singleton()) {
		if (AssetTagRegistry::get_singleton()) {
			if (efs->get_filesystem() != nullptr) {
				_assettags_connect_prune_hooks(false);
			} else {
				g_assettags_first_scan_cb = callable_mp_static(_assettags_connect_prune_hooks);
				if (!efs->is_connected("sources_changed", g_assettags_first_scan_cb)) {
					efs->connect("sources_changed", g_assettags_first_scan_cb);
				}
			}
		}
	}
	if (FileSystemDock *fs_dock = FileSystemDock::get_singleton()) {
		g_assettags_files_moved_cb = callable_mp_static(&_on_asset_file_moved);
		if (!fs_dock->is_connected("files_moved", g_assettags_files_moved_cb)) {
			fs_dock->connect("files_moved", g_assettags_files_moved_cb);
		}
	}
}
#endif

void initialize_assettags_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(AssetTagRuntime);
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		// Config keys only — the tag dictionary UI lives on the Project Settings "Asset Tags" tab.
		GLOBAL_DEF_BASIC("blazium/assettags/strict_paths", false);
		GLOBAL_DEF_BASIC("blazium/assettags/strict_tags", false);
		GLOBAL_DEF_BASIC("blazium/assettags/taggable_extensions", "glb,gltf,obj,fbx,blend,png,jpg,jpeg,webp,svg,exr,bmp,tga,tscn,scn,gd,luau,cs,tres,material,gdshader,gdshaderinc,res,remap");
		if (ProjectSettings *ps = ProjectSettings::get_singleton()) {
			ps->set_custom_property_info(PropertyInfo(Variant::BOOL, "blazium/assettags/strict_paths", PROPERTY_HINT_NONE, "Reject tagging paths that do not exist on disk. Browse and edit tags on the Asset Tags tab."));
			ps->set_custom_property_info(PropertyInfo(Variant::BOOL, "blazium/assettags/strict_tags", PROPERTY_HINT_NONE, "Reject unknown tag names. Browse and edit tags on the Asset Tags tab."));
			ps->set_custom_property_info(PropertyInfo(Variant::STRING, "blazium/assettags/taggable_extensions", PROPERTY_HINT_NONE, "Comma-separated extensions. Manage the tag dictionary on the Asset Tags Project Settings tab."));
		}
		GDREGISTER_CLASS(AssetTagManager);
		GDREGISTER_CLASS(AssetTagRegistry);
		GDREGISTER_CLASS(AssetTagCoordinator);
		GDREGISTER_CLASS(AssetTagsEditorPlugin);
		GDREGISTER_CLASS(AssetTagsContextMenuPlugin);
		EditorNode::add_init_callback(_assettags_editor_init);
	}
#endif
}
void uninitialize_assettags_module(ModuleInitializationLevel p_level) {
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		_assettags_disconnect_editor_hooks();
		if (AssetTagCoordinator::get_singleton()) {
			memdelete(AssetTagCoordinator::get_singleton());
		}
		if (AssetTagRegistry::get_singleton()) {
			AssetTagRegistry::get_singleton()->prepare_for_teardown();
			memdelete(AssetTagRegistry::get_singleton());
		}
		if (AssetTagManager::get_singleton()) {
			memdelete(AssetTagManager::get_singleton());
		}
	}
#endif
}
