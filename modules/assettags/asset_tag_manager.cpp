/**************************************************************************/
/*  asset_tag_manager.cpp                                                 */
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
#include "asset_tag_manager.h"

#include "asset_tag_coordinator.h"

#include "asset_tag_hierarchy.h"
#include "core/templates/hash_set.h"

AssetTagManager *AssetTagManager::singleton = nullptr;

void AssetTagManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load"), &AssetTagManager::load);
	ClassDB::bind_method(D_METHOD("save"), &AssetTagManager::save);
	ClassDB::bind_method(D_METHOD("list_tags", "parent_tag"), &AssetTagManager::list_tags, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("list_all_tags"), &AssetTagManager::list_all_tags);
	ClassDB::bind_method(D_METHOD("get_tag_info", "tag_name"), &AssetTagManager::get_tag_info);
	ClassDB::bind_method(D_METHOD("add_tag", "tag_name", "comment", "source"), &AssetTagManager::add_tag, DEFVAL(String()), DEFVAL("default"));
	ClassDB::bind_method(D_METHOD("remove_tag", "tag_name"), &AssetTagManager::remove_tag);
	ClassDB::bind_method(D_METHOD("rename_tag", "old_name", "new_name"), &AssetTagManager::rename_tag);
	ClassDB::bind_method(D_METHOD("update_tag_comment", "tag_name", "comment"), &AssetTagManager::update_tag_comment);
	ClassDB::bind_method(D_METHOD("has_tag_in_dictionary", "tag_name"), &AssetTagManager::has_tag_in_dictionary);
	ClassDB::bind_method(D_METHOD("resolve_tag_alias", "tag_name"), &AssetTagManager::resolve_tag_alias);
	ClassDB::bind_method(D_METHOD("matches_tag", "tag", "query_tag"), &AssetTagManager::matches_tag);
	ClassDB::bind_method(D_METHOD("container_has_tag", "container", "tag"), &AssetTagManager::container_has_tag);
	ClassDB::bind_method(D_METHOD("container_has_any", "container", "query_tags"), &AssetTagManager::container_has_any);
	ClassDB::bind_method(D_METHOD("begin_batch"), &AssetTagManager::begin_batch);
	ClassDB::bind_method(D_METHOD("commit_batch", "persist"), &AssetTagManager::commit_batch, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("abort_batch"), &AssetTagManager::abort_batch);
	ClassDB::bind_method(D_METHOD("container_has_all", "container", "query_tags"), &AssetTagManager::container_has_all);

	ADD_SIGNAL(MethodInfo("tag_dictionary_changed"));
	ADD_SIGNAL(MethodInfo("redirects_changed"));
}

AssetTagManager *AssetTagManager::get_singleton() {
	return singleton;
}

String AssetTagManager::_resolve_redirect(const String &p_tag) const {
	String current = p_tag;
	HashSet<String> visited;
	for (int pass = 0; pass < 8; pass++) {
		if (visited.has(current)) {
			return p_tag;
		}
		visited.insert(current);
		bool changed = false;
		for (int i = 0; i < redirects.size(); i++) {
			if (redirects[i].old_name == current) {
				current = redirects[i].new_name;
				changed = true;
				break;
			}
		}
		if (!changed) {
			break;
		}
	}
	return current;
}

bool AssetTagManager::_is_valid_tag_name(const String &p_tag) const {
	if (p_tag.is_empty()) {
		return false;
	}
	for (int i = 0; i < p_tag.length(); i++) {
		const char32_t c = p_tag[i];
		if (!(c == '.' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) {
			return false;
		}
	}
	if (p_tag.begins_with(".") || p_tag.ends_with(".")) {
		return false;
	}
	if (p_tag.contains("..")) {
		return false;
	}
	return true;
}

Vector<String> AssetTagManager::_split_tag_parts(const String &p_tag) const {
	return p_tag.split(".", false);
}

static bool _redirects_equal(const Vector<AssetTagRedirect> &p_left, const Vector<AssetTagRedirect> &p_right) {
	if (p_left.size() != p_right.size()) {
		return false;
	}
	for (int i = 0; i < p_left.size(); i++) {
		if (p_left[i].old_name != p_right[i].old_name || p_left[i].new_name != p_right[i].new_name) {
			return false;
		}
	}
	return true;
}

Error AssetTagManager::load() {
	ERR_FAIL_COND_V_MSG(this != singleton, ERR_UNCONFIGURED, "AssetTagManager: only the module singleton may load the tag dictionary.");
	const Vector<AssetTagRedirect> prior_redirects = redirects;
	if (!AssetTagStorage::load_dictionary(tags, redirects)) {
		return ERR_FILE_CORRUPT;
	}
	_rebuild_children_index();
	if (!_redirects_equal(prior_redirects, redirects)) {
		emit_signal(SNAME("redirects_changed"));
	}
	return OK;
}

Error AssetTagManager::save() {
	ERR_FAIL_COND_V_MSG(this != singleton, ERR_UNCONFIGURED, "AssetTagManager: only the module singleton may save the tag dictionary.");
	if (batch_depth > 0) {
		dictionary_dirty = true;
		return OK;
	}
	if (!AssetTagStorage::save_dictionary(tags, redirects)) {
		return ERR_CANT_CREATE;
	}
	dictionary_dirty = false;
	return OK;
}

void AssetTagManager::begin_batch() {
	batch_depth++;
}

Error AssetTagManager::commit_batch(bool p_persist) {
	if (batch_depth <= 0) {
		return OK;
	}
	batch_depth--;
	if (batch_depth == 0 && dictionary_dirty) {
		if (p_persist) {
			const Error err = save();
			if (err != OK) {
				load();
				return err;
			}
			emit_signal(SNAME("tag_dictionary_changed"));
		}
	}
	return OK;
}

void AssetTagManager::get_dictionary_snapshot(HashMap<String, AssetTagEntry> &r_tags, Vector<AssetTagRedirect> &r_redirects) const {
	r_tags = tags;
	r_redirects = redirects;
}

void AssetTagManager::abort_batch() {
	if (batch_depth <= 0) {
		return;
	}
	batch_depth--;
	if (batch_depth == 0) {
		dictionary_dirty = false;
		load();
	}
}

void AssetTagManager::_rebuild_children_index() {
	children_by_parent.clear();
	for (const KeyValue<String, AssetTagEntry> &kv : tags) {
		const int dot = kv.key.rfind(".");
		if (dot == -1) {
			continue;
		}
		const String parent = kv.key.substr(0, dot);
		const String remainder = kv.key.substr(dot + 1);
		if (!remainder.contains(".")) {
			children_by_parent[parent].push_back(kv.key);
		}
	}
}

void AssetTagManager::_update_children_for_tag(const String &p_tag_name, bool p_add) {
	const int dot = p_tag_name.rfind(".");
	if (dot == -1) {
		return;
	}
	const String parent = p_tag_name.substr(0, dot);
	const String remainder = p_tag_name.substr(dot + 1);
	if (remainder.contains(".")) {
		return;
	}
	if (p_add) {
		Vector<String> &children = children_by_parent[parent];
		bool found = false;
		for (int i = 0; i < children.size(); i++) {
			if (children[i] == p_tag_name) {
				found = true;
				break;
			}
		}
		if (!found) {
			children.push_back(p_tag_name);
		}
	}
}

PackedStringArray AssetTagManager::list_tags(const String &p_parent_tag) const {
	PackedStringArray result;
	if (p_parent_tag.is_empty()) {
		for (const KeyValue<String, AssetTagEntry> &kv : tags) {
			if (!kv.key.contains(".")) {
				result.push_back(kv.key);
			}
		}
	} else if (children_by_parent.has(p_parent_tag)) {
		for (int i = 0; i < children_by_parent[p_parent_tag].size(); i++) {
			result.push_back(children_by_parent[p_parent_tag][i]);
		}
	}
	result.sort();
	return result;
}

PackedStringArray AssetTagManager::list_all_tags() const {
	PackedStringArray result;
	for (const KeyValue<String, AssetTagEntry> &kv : tags) {
		result.push_back(kv.key);
	}
	result.sort();
	return result;
}

Dictionary AssetTagManager::get_tag_info(const String &p_tag_name) const {
	Dictionary result;
	const String resolved = _resolve_redirect(p_tag_name);
	if (!tags.has(resolved)) {
		result["ok"] = false;
		result["error"] = "Tag not found: " + p_tag_name;
		return result;
	}

	const AssetTagEntry &entry = tags[resolved];
	result["ok"] = true;
	result["tag"] = resolved;
	result["comment"] = entry.comment;
	result["source"] = entry.source;

	PackedStringArray children;
	if (children_by_parent.has(resolved)) {
		for (int i = 0; i < children_by_parent[resolved].size(); i++) {
			children.push_back(children_by_parent[resolved][i]);
		}
	}
	children.sort();
	result["children"] = children;
	return result;
}

Error AssetTagManager::add_tag(const String &p_tag_name, const String &p_comment, const String &p_source) {
	const String resolved = _resolve_redirect(p_tag_name);
	if (!_is_valid_tag_name(resolved)) {
		return ERR_INVALID_PARAMETER;
	}
	if (tags.has(resolved)) {
		return ERR_ALREADY_EXISTS;
	}

	Vector<String> parts = _split_tag_parts(resolved);
	String parent;
	for (int i = 0; i < parts.size() - 1; i++) {
		parent = i == 0 ? parts[0] : parent + "." + parts[i];
		if (!tags.has(parent)) {
			AssetTagEntry parent_entry;
			parent_entry.source = p_source;
			tags[parent] = parent_entry;
			_update_children_for_tag(parent, true);
		}
	}

	AssetTagEntry entry;
	entry.comment = p_comment;
	entry.source = p_source;
	tags[resolved] = entry;
	_update_children_for_tag(resolved, true);
	Error err = save();
	if (err == OK && batch_depth == 0) {
		emit_signal(SNAME("tag_dictionary_changed"));
	}
	return err;
}

Error AssetTagManager::remove_tag(const String &p_tag_name) {
	if (AssetTagCoordinator *coordinator = AssetTagCoordinator::get_singleton()) {
		if (!coordinator->is_in_transaction() && batch_depth == 0) {
			WARN_PRINT_ONCE("AssetTagManager::remove_tag called outside AssetTagCoordinator; registry may desync.");
		}
	}
	const String resolved = _resolve_redirect(p_tag_name);
	if (!tags.has(resolved)) {
		return ERR_DOES_NOT_EXIST;
	}

	Vector<String> to_remove;
	for (const KeyValue<String, AssetTagEntry> &kv : tags) {
		if (kv.key == resolved || kv.key.begins_with(resolved + ".")) {
			to_remove.push_back(kv.key);
		}
	}
	for (int i = 0; i < to_remove.size(); i++) {
		tags.erase(to_remove[i]);
	}
	_rebuild_children_index();
	Error err = save();
	if (err == OK && batch_depth == 0) {
		emit_signal(SNAME("tag_dictionary_changed"));
	}
	return err;
}

Error AssetTagManager::rename_tag(const String &p_old_name, const String &p_new_name) {
	if (AssetTagCoordinator *coordinator = AssetTagCoordinator::get_singleton()) {
		if (!coordinator->is_in_transaction() && batch_depth == 0) {
			WARN_PRINT_ONCE("AssetTagManager::rename_tag called outside AssetTagCoordinator; registry may desync.");
		}
	}
	const String old_resolved = _resolve_redirect(p_old_name);
	const String new_resolved = _resolve_redirect(p_new_name);
	if (!tags.has(old_resolved)) {
		return ERR_DOES_NOT_EXIST;
	}
	if (!_is_valid_tag_name(new_resolved)) {
		return ERR_INVALID_PARAMETER;
	}
	if (old_resolved == new_resolved) {
		return OK;
	}
	if (tags.has(new_resolved)) {
		return ERR_ALREADY_EXISTS;
	}

	HashMap<String, AssetTagEntry> updated;
	for (const KeyValue<String, AssetTagEntry> &kv : tags) {
		String new_name = kv.key;
		if (kv.key == old_resolved) {
			new_name = new_resolved;
		} else if (kv.key.begins_with(old_resolved + ".")) {
			new_name = new_resolved + kv.key.substr(old_resolved.length());
		}
		updated[new_name] = kv.value;
	}
	tags = updated;
	_rebuild_children_index();

	Vector<AssetTagRedirect> pruned;
	for (int i = 0; i < redirects.size(); i++) {
		if (redirects[i].new_name != old_resolved && redirects[i].new_name != new_resolved) {
			pruned.push_back(redirects[i]);
		}
	}
	redirects = pruned;

	AssetTagRedirect redirect;
	redirect.old_name = old_resolved;
	redirect.new_name = new_resolved;
	redirects.push_back(redirect);
	Error err = save();
	if (err == OK && batch_depth == 0) {
		emit_signal(SNAME("tag_dictionary_changed"));
		emit_signal(SNAME("redirects_changed"));
	}
	return err;
}

Error AssetTagManager::update_tag_comment(const String &p_tag_name, const String &p_comment) {
	const String resolved = _resolve_redirect(p_tag_name);
	if (!tags.has(resolved)) {
		return ERR_DOES_NOT_EXIST;
	}
	tags[resolved].comment = p_comment;
	const Error err = save();
	if (err == OK && batch_depth == 0) {
		emit_signal(SNAME("tag_dictionary_changed"));
	}
	return err;
}

bool AssetTagManager::has_tag_in_dictionary(const String &p_tag_name) const {
	return tags.has(_resolve_redirect(p_tag_name));
}

String AssetTagManager::resolve_tag_alias(const String &p_tag_name) const {
	return _resolve_redirect(p_tag_name);
}

bool AssetTagManager::matches_tag(const String &p_tag, const String &p_query_tag) const {
	const String tag = _resolve_redirect(p_tag);
	const String query = _resolve_redirect(p_query_tag);
	return AssetTagHierarchy::matches_tag(tag, query);
}

bool AssetTagManager::container_has_tag(const PackedStringArray &p_container, const String &p_tag) const {
	for (int i = 0; i < p_container.size(); i++) {
		if (matches_tag(p_container[i], p_tag) || matches_tag(p_tag, p_container[i])) {
			return true;
		}
	}
	return false;
}

bool AssetTagManager::container_has_any(const PackedStringArray &p_container, const PackedStringArray &p_query_tags) const {
	for (int i = 0; i < p_query_tags.size(); i++) {
		if (container_has_tag(p_container, p_query_tags[i])) {
			return true;
		}
	}
	return false;
}

bool AssetTagManager::container_has_all(const PackedStringArray &p_container, const PackedStringArray &p_query_tags) const {
	for (int i = 0; i < p_query_tags.size(); i++) {
		if (!container_has_tag(p_container, p_query_tags[i])) {
			return false;
		}
	}
	return true;
}

PackedStringArray AssetTagManager::get_unused_tags(const HashMap<String, Vector<String>> &p_index) const {
	HashSet<String> referenced;
	for (const KeyValue<String, Vector<String>> &kv : p_index) {
		for (int i = 0; i < kv.value.size(); i++) {
			const String &tag = kv.value[i];
			referenced.insert(tag);
			String prefix = tag;
			while (true) {
				const int dot = prefix.rfind(".");
				if (dot == -1) {
					break;
				}
				prefix = prefix.substr(0, dot);
				referenced.insert(prefix);
			}
		}
	}

	PackedStringArray unused;
	for (const KeyValue<String, AssetTagEntry> &kv : tags) {
		if (referenced.has(kv.key)) {
			continue;
		}
		bool referenced_via_hierarchy = false;
		for (const String &ref_tag : referenced) {
			if (matches_tag(ref_tag, kv.key)) {
				referenced_via_hierarchy = true;
				break;
			}
		}
		if (!referenced_via_hierarchy) {
			unused.push_back(kv.key);
		}
	}
	unused.sort();
	return unused;
}

AssetTagManager::AssetTagManager() {
	if (!singleton) {
		singleton = this;
	}
}

AssetTagManager::~AssetTagManager() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
