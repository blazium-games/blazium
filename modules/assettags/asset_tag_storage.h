/**************************************************************************/
/*  asset_tag_storage.h                                                   */
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

#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/templates/vector.h"

struct AssetTagEntry {
	String comment;
	String source;
};

struct AssetTagRedirect {
	String old_name;
	String new_name;
};

class AssetTagStorage {
	static String test_storage_override;

public:
#ifdef TESTS_ENABLED
	static void set_test_storage_dir(const String &p_dir);
	static void clear_test_storage_dir();
	static void set_test_fail_index_commit(bool p_fail);
#endif

	static String get_storage_dir();
	static String get_tags_file_path();
	static String get_index_file_path();

	static bool ensure_storage_dir();
	static bool load_dictionary(HashMap<String, AssetTagEntry> &r_tags, Vector<AssetTagRedirect> &r_redirects);
	static bool save_dictionary(const HashMap<String, AssetTagEntry> &p_tags, const Vector<AssetTagRedirect> &p_redirects);
	static bool load_index(HashMap<String, Vector<String>> &r_index);
	static bool save_index(const HashMap<String, Vector<String>> &p_index);
	static bool save_index_merge(const HashMap<String, Vector<String>> &p_full_index, const HashSet<String> &p_dirty_paths);
	static bool save_index_dirty_sidecars(const HashMap<String, Vector<String>> &p_full_index, const HashSet<String> &p_dirty_paths);
	static bool apply_dirty_sidecars(HashMap<String, Vector<String>> &r_index);
	static bool compact_index_sidecars(const HashMap<String, Vector<String>> &p_full_index);
	static bool clear_index_dirty_sidecars();
	static bool quarantine_index_dirty_sidecars();
	static bool quarantine_corrupt_index();
	static bool quarantine_corrupt_dictionary();
	static String get_index_dirty_dir();
	static bool commit_dictionary_and_index(
			const HashMap<String, AssetTagEntry> &p_tags,
			const Vector<AssetTagRedirect> &p_redirects,
			const HashMap<String, Vector<String>> &p_index,
			bool p_save_dictionary,
			bool p_save_index);

	static void set_index_write_blocked(bool p_blocked);
	static bool is_index_write_blocked();

	static Error snapshot_undo_state();
	static Error restore_undo_state();
	static bool has_undo_state();
	static void clear_undo_state();
	static void mark_undo_snapshot_committed();
	static void rotate_undo_stack_after_restore();

	static bool is_taggable_extension(const String &p_path);
	static String normalize_asset_path(const String &p_path);
};
