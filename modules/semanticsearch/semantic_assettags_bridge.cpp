/**************************************************************************/
/*  semantic_assettags_bridge.cpp                                         */
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

#include "semantic_assettags_bridge.h"

#include "semantic_asset_index.h"

#include "core/io/file_access.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "modules/modules_enabled.gen.h"

#ifdef MODULE_ASSETTAGS_ENABLED
#include "modules/assettags/asset_tag_manager.h"
#include "modules/assettags/asset_tag_registry.h"
#endif

#ifdef TOOLS_ENABLED
#include "editor/editor_file_system.h"
#endif

#ifdef MODULE_JUSTAMCP_ENABLED
#include "modules/justamcp/justamcp_server.h"
#include "modules/justamcp/tools/justamcp_resource_subscriptions.h"
#include "modules/justamcp/tools/resources/justamcp_tags_resource_provider.h"
#endif

#ifdef MODULE_ASSETTAGS_ENABLED
static HashMap<String, String> g_dictionary_comment_snapshot;
static AssetTagRegistry *g_attached_registry = nullptr;
static bool g_bridge_signals_connected = false;
#ifdef TOOLS_ENABLED
static Callable g_semantic_first_scan_cb;
#endif

static void _broadcast_tag_resources(const String &p_path) {
#ifdef MODULE_JUSTAMCP_ENABLED
	if (JustAMCPServer *server = JustAMCPServer::get_singleton()) {
		const String encoded = p_path.uri_encode();
		server->broadcast_resource_updated("blazium://tags/asset/" + encoded);
		server->broadcast_resource_updated("blazium://semantic/asset/" + encoded);
	}
#else
	(void)p_path;
#endif
}

static void _on_asset_tags_changed(const String &p_path) {
	if (SemanticAssetIndex *index = SemanticAssetIndex::get_singleton()) {
		if (FileAccess::exists(p_path)) {
			index->upsert_entry(p_path);
		} else {
			index->remove_entry(p_path);
		}
	}
	_broadcast_tag_resources(p_path);
#ifdef MODULE_JUSTAMCP_ENABLED
	JustAMCPResourceSubscriptions::notify_uri_changed("blazium://tags/asset/" + p_path.uri_encode());
	JustAMCPResourceSubscriptions::notify_uri_changed("blazium://semantic/asset/" + p_path.uri_encode());
	if (JustAMCPServer *server = JustAMCPServer::get_singleton()) {
		server->broadcast_resource_updated("blazium://semantic/index/stats");
	}
#endif
}

static void _on_asset_tags_batch_changed(const PackedStringArray &p_paths) {
	if (SemanticAssetIndex *index = SemanticAssetIndex::get_singleton()) {
		index->upsert_paths(p_paths);
	}
#ifdef MODULE_JUSTAMCP_ENABLED
	if (JustAMCPServer *server = JustAMCPServer::get_singleton()) {
		for (int i = 0; i < p_paths.size(); i++) {
			_broadcast_tag_resources(p_paths[i]);
		}
		server->broadcast_resource_updated("blazium://semantic/index/stats");
		server->broadcast_resources_list_changed();
	}
#endif
}

static void _reconcile_semantic_from_registry() {
	if (AssetTagRegistry *registry = AssetTagRegistry::get_singleton()) {
		if (SemanticAssetIndex *index = SemanticAssetIndex::get_singleton()) {
			index->reconcile_with_registry(registry);
		}
	}
#ifdef MODULE_JUSTAMCP_ENABLED
	if (JustAMCPServer *server = JustAMCPServer::get_singleton()) {
		server->broadcast_resource_updated("blazium://semantic/index/stats");
		server->broadcast_resources_list_changed();
	}
#endif
}

#ifdef TOOLS_ENABLED
static void _on_first_scan_reconcile(bool p_exist) {
	(void)p_exist;
	if (EditorFileSystem *efs = EditorFileSystem::get_singleton()) {
		if (g_semantic_first_scan_cb.is_valid() && efs->is_connected("sources_changed", g_semantic_first_scan_cb)) {
			efs->disconnect("sources_changed", g_semantic_first_scan_cb);
		}
	}
	g_semantic_first_scan_cb = Callable();
	_reconcile_semantic_from_registry();
}

static void _schedule_reconcile_after_first_scan() {
	EditorFileSystem *efs = EditorFileSystem::get_singleton();
	if (!efs) {
		_reconcile_semantic_from_registry();
		return;
	}
	if (efs->get_filesystem() != nullptr) {
		callable_mp_static(_reconcile_semantic_from_registry).call_deferred();
		return;
	}
	g_semantic_first_scan_cb = callable_mp_static(_on_first_scan_reconcile);
	if (!efs->is_connected("sources_changed", g_semantic_first_scan_cb)) {
		efs->connect("sources_changed", g_semantic_first_scan_cb);
	}
}

static void _cancel_pending_first_scan_reconcile() {
	if (EditorFileSystem *efs = EditorFileSystem::get_singleton()) {
		if (g_semantic_first_scan_cb.is_valid() && efs->is_connected("sources_changed", g_semantic_first_scan_cb)) {
			efs->disconnect("sources_changed", g_semantic_first_scan_cb);
		}
	}
	g_semantic_first_scan_cb = Callable();
}
#endif

static void _reconcile_dictionary_comment_changes() {
	AssetTagManager *manager = AssetTagManager::get_singleton();
	AssetTagRegistry *registry = AssetTagRegistry::get_singleton();
	SemanticAssetIndex *index = SemanticAssetIndex::get_singleton();
	if (!manager || !registry || !index) {
		return;
	}

	HashSet<String> current_keys;
	HashMap<String, String> current_comments;
	const PackedStringArray all_tags = manager->list_all_tags();
	for (int i = 0; i < all_tags.size(); i++) {
		const String tag = all_tags[i];
		current_keys.insert(tag);
		const Dictionary info = manager->get_tag_info(tag);
		current_comments[tag] = String(info.get("comment", ""));
	}

	bool dictionary_keys_changed = g_dictionary_comment_snapshot.is_empty();
	if (!dictionary_keys_changed) {
		for (const String &tag : current_keys) {
			if (!g_dictionary_comment_snapshot.has(tag)) {
				dictionary_keys_changed = true;
				break;
			}
		}
		if (!dictionary_keys_changed) {
			for (const KeyValue<String, String> &kv : g_dictionary_comment_snapshot) {
				if (!current_keys.has(kv.key)) {
					dictionary_keys_changed = true;
					break;
				}
			}
		}
	}

	if (dictionary_keys_changed) {
		_reconcile_semantic_from_registry();
	} else {
		PackedStringArray changed_paths;
		HashSet<String> seen_paths;
		for (int i = 0; i < all_tags.size(); i++) {
			const String tag = all_tags[i];
			const String previous_comment = g_dictionary_comment_snapshot.has(tag) ? g_dictionary_comment_snapshot[tag] : String();
			if (previous_comment == current_comments[tag]) {
				continue;
			}
			const PackedStringArray paths = registry->find_assets_by_tag(tag, true);
			for (int j = 0; j < paths.size(); j++) {
				if (!seen_paths.has(paths[j])) {
					seen_paths.insert(paths[j]);
					changed_paths.push_back(paths[j]);
				}
			}
		}
		if (!changed_paths.is_empty()) {
			index->upsert_paths(changed_paths);
		}
	}

	g_dictionary_comment_snapshot = current_comments;
}

static void _on_index_reloaded() {
	g_dictionary_comment_snapshot.clear();
	_reconcile_semantic_from_registry();
}

static void _on_tag_dictionary_changed() {
#ifdef MODULE_JUSTAMCP_ENABLED
	JustAMCPTagsResourceProvider::invalidate_dictionary_cache();
#endif
	_reconcile_dictionary_comment_changes();
#ifdef MODULE_JUSTAMCP_ENABLED
	if (JustAMCPServer *server = JustAMCPServer::get_singleton()) {
		server->broadcast_resource_updated("blazium://tags/dictionary");
		server->broadcast_resource_updated("blazium://semantic/index/stats");
	}
#endif
}

void SemanticAssettagsBridge::detach() {
	if (!g_bridge_signals_connected) {
		g_attached_registry = nullptr;
#ifdef TOOLS_ENABLED
		_cancel_pending_first_scan_reconcile();
#endif
		return;
	}
#ifdef TOOLS_ENABLED
	_cancel_pending_first_scan_reconcile();
#endif
	AssetTagRegistry *registry = g_attached_registry;
	g_attached_registry = nullptr;
	g_bridge_signals_connected = false;
	g_dictionary_comment_snapshot.clear();

	if (registry && registry == AssetTagRegistry::get_singleton()) {
		const Callable asset_tags_changed = callable_mp_static(&_on_asset_tags_changed);
		const Callable asset_tags_batch_changed = callable_mp_static(&_on_asset_tags_batch_changed);
		const Callable index_reloaded = callable_mp_static(&_on_index_reloaded);
		if (registry->is_connected("asset_tags_changed", asset_tags_changed)) {
			registry->disconnect("asset_tags_changed", asset_tags_changed);
		}
		if (registry->is_connected("asset_tags_batch_changed", asset_tags_batch_changed)) {
			registry->disconnect("asset_tags_batch_changed", asset_tags_batch_changed);
		}
		if (registry->is_connected("index_reloaded", index_reloaded)) {
			registry->disconnect("index_reloaded", index_reloaded);
		}
	}
	if (AssetTagManager *manager = AssetTagManager::get_singleton()) {
		const Callable tag_dictionary_changed = callable_mp_static(&_on_tag_dictionary_changed);
		if (manager->is_connected("tag_dictionary_changed", tag_dictionary_changed)) {
			manager->disconnect("tag_dictionary_changed", tag_dictionary_changed);
		}
	}
}

void SemanticAssettagsBridge::attach(AssetTagRegistry *p_registry) {
	if (!p_registry) {
		return;
	}
	if (g_attached_registry == p_registry && g_bridge_signals_connected) {
		return;
	}
	if (g_attached_registry) {
		detach();
	}
	p_registry->connect("asset_tags_changed", callable_mp_static(&_on_asset_tags_changed));
	p_registry->connect("asset_tags_batch_changed", callable_mp_static(&_on_asset_tags_batch_changed));
	p_registry->connect("index_reloaded", callable_mp_static(&_on_index_reloaded));
	if (AssetTagManager *manager = AssetTagManager::get_singleton()) {
		manager->connect("tag_dictionary_changed", callable_mp_static(&_on_tag_dictionary_changed));
	}
	g_attached_registry = p_registry;
	g_bridge_signals_connected = true;
	// Defer full registry reconcile until after first FS scan so editor init stays responsive.
#ifdef TOOLS_ENABLED
	_schedule_reconcile_after_first_scan();
#else
	_reconcile_semantic_from_registry();
#endif
}
#endif
