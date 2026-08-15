/**************************************************************************/
/*  asset_tag_coordinator.cpp                                             */
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
#include "asset_tag_coordinator.h"

#include "asset_tag_manager.h"
#include "asset_tag_registry.h"
#include "asset_tag_runtime.h"
#include "asset_tag_storage.h"

#ifdef MODULE_JUSTAMCP_ENABLED
#include "modules/justamcp/justamcp_server.h"
#include "modules/justamcp/tools/resources/justamcp_tags_resource_provider.h"
#endif

static void _notify_justamcp_tag_resources_changed() {
#ifdef MODULE_JUSTAMCP_ENABLED
	JustAMCPTagsResourceProvider::invalidate_dictionary_cache();
	if (JustAMCPServer *server = JustAMCPServer::get_singleton()) {
		server->broadcast_resource_updated("blazium://tags/dictionary");
		server->broadcast_resources_list_changed();
	}
#endif
}

static int g_coordinator_transaction_depth = 0;

AssetTagCoordinator *AssetTagCoordinator::singleton = nullptr;

void AssetTagCoordinator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("rename_tag", "old_name", "new_name"), &AssetTagCoordinator::rename_tag);
	ClassDB::bind_method(D_METHOD("remove_tag", "tag_name"), &AssetTagCoordinator::remove_tag);
	ClassDB::bind_method(D_METHOD("add_tag", "tag_name", "comment"), &AssetTagCoordinator::add_tag, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("update_tag_comment", "tag_name", "comment"), &AssetTagCoordinator::update_tag_comment);
	ClassDB::bind_method(D_METHOD("rename_tag_result", "old_name", "new_name"), &AssetTagCoordinator::rename_tag_result);
	ClassDB::bind_method(D_METHOD("remove_tag_result", "tag_name"), &AssetTagCoordinator::remove_tag_result);
	ClassDB::bind_method(D_METHOD("schedule_prune_removed_paths"), &AssetTagCoordinator::schedule_prune_removed_paths);
	ClassDB::bind_method(D_METHOD("begin_transaction"), &AssetTagCoordinator::begin_transaction);
	ClassDB::bind_method(D_METHOD("commit_transaction"), &AssetTagCoordinator::commit_transaction);
	ClassDB::bind_method(D_METHOD("abort_transaction"), &AssetTagCoordinator::abort_transaction);
	ClassDB::bind_method(D_METHOD("can_undo"), &AssetTagCoordinator::can_undo);
	ClassDB::bind_method(D_METHOD("undo_last_change"), &AssetTagCoordinator::undo_last_change);
}

AssetTagCoordinator *AssetTagCoordinator::get_singleton() {
	return singleton;
}

bool AssetTagCoordinator::is_in_transaction() {
	return g_coordinator_transaction_depth > 0;
}

Error AssetTagCoordinator::rename_tag(const String &p_old_name, const String &p_new_name) {
	ERR_FAIL_COND_V_MSG(this != singleton, ERR_UNCONFIGURED, "AssetTagCoordinator: only the module singleton may mutate tags.");
	Dictionary result = rename_tag_result(p_old_name, p_new_name);
	if (!result.get("ok", false)) {
		return Error(int(result.get("error_code", ERR_CANT_CREATE)));
	}
	return OK;
}

Error AssetTagCoordinator::remove_tag(const String &p_tag_name) {
	ERR_FAIL_COND_V_MSG(this != singleton, ERR_UNCONFIGURED, "AssetTagCoordinator: only the module singleton may mutate tags.");
	Dictionary result = remove_tag_result(p_tag_name);
	if (!result.get("ok", false)) {
		return Error(int(result.get("error_code", ERR_CANT_CREATE)));
	}
	return OK;
}

Error AssetTagCoordinator::add_tag(const String &p_tag_name, const String &p_comment) {
	ERR_FAIL_COND_V_MSG(this != singleton, ERR_UNCONFIGURED, "AssetTagCoordinator: only the module singleton may mutate tags.");
	AssetTagManager *manager = AssetTagManager::get_singleton();
	AssetTagRegistry *registry = AssetTagRegistry::get_singleton();
	if (!manager || !registry) {
		return ERR_UNAVAILABLE;
	}
	const bool nested = registry->is_in_batch();
	if (!nested) {
		const Error snap_err = begin_transaction();
		if (snap_err != OK) {
			return snap_err;
		}
	}
	const Error err = manager->add_tag(p_tag_name, p_comment);
	if (err != OK) {
		if (!nested) {
			abort_transaction();
		}
		return err;
	}
	if (!nested) {
		return commit_transaction();
	}
	return OK;
}

Error AssetTagCoordinator::update_tag_comment(const String &p_tag_name, const String &p_comment) {
	ERR_FAIL_COND_V_MSG(this != singleton, ERR_UNCONFIGURED, "AssetTagCoordinator: only the module singleton may mutate tags.");
	AssetTagManager *manager = AssetTagManager::get_singleton();
	AssetTagRegistry *registry = AssetTagRegistry::get_singleton();
	if (!manager || !registry) {
		return ERR_UNAVAILABLE;
	}
	const bool nested = registry->is_in_batch();
	if (!nested) {
		const Error snap_err = begin_transaction();
		if (snap_err != OK) {
			return snap_err;
		}
	}
	const Error err = manager->update_tag_comment(p_tag_name, p_comment);
	if (err != OK) {
		if (!nested) {
			abort_transaction();
		}
		return err;
	}
	if (!nested) {
		return commit_transaction();
	}
	return OK;
}

Dictionary AssetTagCoordinator::rename_tag_result(const String &p_old_name, const String &p_new_name) {
	Dictionary result;
	AssetTagManager *manager = AssetTagManager::get_singleton();
	AssetTagRegistry *registry = AssetTagRegistry::get_singleton();
	if (!manager || !registry) {
		result["ok"] = false;
		result["error"] = "Asset tag services unavailable";
		return result;
	}

	const String old_canonical = manager->resolve_tag_alias(p_old_name);
	const String new_canonical = manager->resolve_tag_alias(p_new_name);

	const bool nested = registry->is_in_batch();
	if (!nested) {
		const Error snap_err = begin_transaction();
		if (snap_err != OK) {
			result["ok"] = false;
			result["error_code"] = snap_err;
			return result;
		}
	}
	const Error err = manager->rename_tag(p_old_name, p_new_name);
	if (err != OK) {
		if (!nested) {
			abort_transaction();
		}
		result["ok"] = false;
		result["error_code"] = err;
		return result;
	}
	const int affected = registry->apply_tag_rename(old_canonical, new_canonical);
	if (!nested) {
		const Error commit_err = commit_transaction();
		if (commit_err != OK) {
			result["ok"] = false;
			result["error"] = "Failed to persist asset tag index after rename";
			result["error_code"] = commit_err;
			return result;
		}
	}
	result["ok"] = true;
	result["affected_assets"] = affected;
	return result;
}

Dictionary AssetTagCoordinator::remove_tag_result(const String &p_tag_name) {
	Dictionary result;
	AssetTagManager *manager = AssetTagManager::get_singleton();
	AssetTagRegistry *registry = AssetTagRegistry::get_singleton();
	if (!manager || !registry) {
		result["ok"] = false;
		result["error"] = "Asset tag services unavailable";
		return result;
	}

	const String canonical = manager->resolve_tag_alias(p_tag_name);

	const bool nested = registry->is_in_batch();
	if (!nested) {
		const Error snap_err = begin_transaction();
		if (snap_err != OK) {
			result["ok"] = false;
			result["error_code"] = snap_err;
			return result;
		}
	}
	const Error err = manager->remove_tag(p_tag_name);
	if (err != OK) {
		if (!nested) {
			abort_transaction();
		}
		result["ok"] = false;
		result["error_code"] = err;
		return result;
	}
	const int affected = registry->apply_tag_remove(canonical);
	if (!nested) {
		const Error commit_err = commit_transaction();
		if (commit_err != OK) {
			result["ok"] = false;
			result["error"] = "Failed to persist asset tag index after remove";
			result["error_code"] = commit_err;
			return result;
		}
	}
	result["ok"] = true;
	result["affected_assets"] = affected;
	return result;
}

void AssetTagCoordinator::schedule_prune_removed_paths() {
	ERR_FAIL_COND_MSG(this != singleton, "AssetTagCoordinator: only the module singleton may schedule prune.");
	if (AssetTagRegistry *registry = AssetTagRegistry::get_singleton()) {
		registry->schedule_prune_removed_paths();
	}
}

Error AssetTagCoordinator::begin_transaction() {
	ERR_FAIL_COND_V_MSG(this != singleton, ERR_UNCONFIGURED, "AssetTagCoordinator: only the module singleton may begin a transaction.");
	if (g_coordinator_transaction_depth == 0) {
		const Error snap_err = AssetTagStorage::snapshot_undo_state();
		if (snap_err != OK) {
			return snap_err;
		}
	}
	if (AssetTagRegistry *registry = AssetTagRegistry::get_singleton()) {
		registry->begin_batch();
	}
	g_coordinator_transaction_depth++;
	return OK;
}

static void _rollback_coordinator_transaction_preserve_undo() {
	if (AssetTagRegistry *registry = AssetTagRegistry::get_singleton()) {
		while (registry->is_in_batch()) {
			registry->abort_batch();
		}
		registry->load();
	}
	if (AssetTagManager *manager = AssetTagManager::get_singleton()) {
		while (manager->is_in_batch()) {
			manager->abort_batch();
		}
		manager->load();
	}
	g_coordinator_transaction_depth = 0;
}

Error AssetTagCoordinator::commit_transaction() {
	ERR_FAIL_COND_V_MSG(this != singleton, ERR_UNCONFIGURED, "AssetTagCoordinator: only the module singleton may commit a transaction.");
	if (g_coordinator_transaction_depth <= 0) {
		return OK;
	}
	g_coordinator_transaction_depth--;
	if (g_coordinator_transaction_depth > 0) {
		return OK;
	}
	Error commit_err = OK;
	if (AssetTagRegistry *registry = AssetTagRegistry::get_singleton()) {
		commit_err = registry->commit_batch();
	}
	if (commit_err == OK) {
		AssetTagStorage::mark_undo_snapshot_committed();
		_notify_justamcp_tag_resources_changed();
		if (AssetTagRegistry *registry = AssetTagRegistry::get_singleton()) {
			if (registry->consume_queued_prune()) {
				registry->schedule_prune_removed_paths();
			}
		}
		return OK;
	}
	_rollback_coordinator_transaction_preserve_undo();
	return commit_err;
}

void AssetTagCoordinator::abort_transaction() {
	ERR_FAIL_COND_MSG(this != singleton, "AssetTagCoordinator: only the module singleton may abort a transaction.");
	if (g_coordinator_transaction_depth <= 0) {
		return;
	}
	g_coordinator_transaction_depth = 0;
	if (AssetTagRegistry *registry = AssetTagRegistry::get_singleton()) {
		while (registry->is_in_batch()) {
			registry->abort_batch();
		}
	}
	AssetTagStorage::clear_undo_state();
}

bool AssetTagCoordinator::can_undo() const {
	return AssetTagStorage::has_undo_state();
}

Error AssetTagCoordinator::undo_last_change() {
	ERR_FAIL_COND_V_MSG(this != singleton, ERR_UNCONFIGURED, "AssetTagCoordinator: only the module singleton may undo.");
	const Error restore_err = AssetTagStorage::restore_undo_state();
	if (restore_err != OK) {
		return restore_err;
	}
	if (AssetTagManager *manager = AssetTagManager::get_singleton()) {
		manager->load();
		manager->emit_signal(SNAME("tag_dictionary_changed"));
	}
	if (AssetTagRegistry *registry = AssetTagRegistry::get_singleton()) {
		registry->load();
		registry->emit_signal(SNAME("index_reloaded"));
	}
	AssetTagRuntime::invalidate_cache();
#ifdef MODULE_JUSTAMCP_ENABLED
	_notify_justamcp_tag_resources_changed();
#endif
	AssetTagStorage::rotate_undo_stack_after_restore();
	return OK;
}

AssetTagCoordinator::AssetTagCoordinator() {
	if (!singleton) {
		singleton = this;
	}
}

AssetTagCoordinator::~AssetTagCoordinator() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

#ifdef TESTS_ENABLED
void assettags_reset_coordinator_test_state() {
	g_coordinator_transaction_depth = 0;
}
#endif
