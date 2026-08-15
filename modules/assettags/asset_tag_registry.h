/**************************************************************************/
/*  asset_tag_registry.h                                                  */
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

#pragma once

#include "asset_tag_storage.h"
#include "core/object/object.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"

class AssetTagRegistry : public Object {
	GDCLASS(AssetTagRegistry, Object);

	static AssetTagRegistry *singleton;

	HashMap<String, Vector<String>> asset_index;

	void _rebuild_reverse_lookup();
	void _insert_tag_into_reverse_lookup(const String &p_tag, const String &p_path);
	void _erase_tag_from_reverse_lookup(const String &p_tag, const String &p_path);
	void _update_reverse_lookup_for_path(const String &p_path, const Vector<String> &p_old_tags, const Vector<String> &p_new_tags);
	void _remove_path_from_reverse_lookup(const String &p_path, const Vector<String> &p_tags);
	static Vector<String> _dedup_tags(const Vector<String> &p_tags);

	HashMap<String, HashSet<String>> reverse_lookup;
	int batch_depth = 0;
	bool index_dirty = false;
	bool index_load_failed = false;
	bool prune_scheduled = false;
	bool prune_debounce_pending = false;
	bool prune_queued_while_busy = false;
	bool prune_scan_active = false;
	bool shutting_down = false;
	uint64_t last_prune_invoke_msec = 0;
	Vector<String> prune_scan_paths;
	Vector<String> prune_stale_paths;
	int prune_scan_cursor = 0;
	HashSet<String> pending_changed_paths;
	HashSet<String> dirty_index_paths;
	bool save_scheduled = false;

	void _notify_asset_tags_changed(const String &p_path);
	void _emit_batch_changed(const HashSet<String> &p_paths);
	void _restore_index_state(const HashMap<String, Vector<String>> &p_index, const HashMap<String, HashSet<String>> &p_reverse);
	Error _persist_and_notify(const HashSet<String> &p_paths);
	Error _save_index_incremental();
	void _deferred_save();
	void _schedule_save();
	void _debounced_prune_removed_paths();
	void _on_prune_debounce_timeout();

protected:
	static void _bind_methods();

public:
	static AssetTagRegistry *get_singleton();

	void _on_redirects_changed();

	Error load();
	Error recover_after_load_failure();
	bool is_index_load_failed() const { return index_load_failed; }
	Error save();
	void begin_batch();
	Error commit_batch();
	void abort_batch();
	void cancel_opened_batch();
	bool is_in_batch() const { return batch_depth > 0; }
	void schedule_prune_removed_paths();
	void prune_removed_paths();
	void prepare_for_teardown();
	bool consume_queued_prune();
	void flush_deferred_work_for_tests();

	PackedStringArray get_tags_for_asset(const String &p_path) const;
	Error set_tags_for_asset(const String &p_path, const PackedStringArray &p_tags);
	Error add_tags_to_asset(const String &p_path, const PackedStringArray &p_tags);
	Error remove_tags_from_asset(const String &p_path, const PackedStringArray &p_tags);

	PackedStringArray find_assets_by_tag(const String &p_tag, bool p_match_parent = true) const;
	Dictionary search_assets(const PackedStringArray &p_tags, const String &p_type_filter = String(), const String &p_path_glob = String(), const String &p_path_regex = String(), bool p_require_all = true) const;

	PackedStringArray get_unused_tags() const;
	Error rescan();
	bool is_index_dirty() const { return index_dirty; }
	HashSet<String> collect_paths_for_tag_filter(const PackedStringArray &p_tags, bool p_require_all = true) const;

	PackedStringArray get_indexed_paths() const;

	int apply_tag_rename(const String &p_old_name, const String &p_new_name);
	int apply_tag_remove(const String &p_tag_name);

	bool rename_asset_path(const String &p_old_path, const String &p_new_path);

	bool asset_matches_tag_filter(const String &p_path, const PackedStringArray &p_tag_tokens, bool p_require_all = true) const;

	static String remap_tag_for_rename(const String &p_tag, const String &p_old_name, const String &p_new_name);
	static bool tag_matches_prefix(const String &p_tag, const String &p_prefix);

	AssetTagRegistry();
	~AssetTagRegistry();
};
