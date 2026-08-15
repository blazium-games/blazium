/**************************************************************************/
/*  semantic_index_store.h                                                */
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

#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/templates/vector.h"

struct SemanticAssetEntry {
	String path;
	String caption;
	String asset_class;
	String path_segments;
	Vector<String> tokens;
	Vector<double> embedding_vector;
	String embedding_provider;
};

class SemanticIndexStore {
	static String test_index_override;

	HashMap<String, SemanticAssetEntry> entries;

	static String get_index_file_path();
	static String get_index_dir_path();

public:
#ifdef TESTS_ENABLED
	static void set_test_index_dir(const String &p_dir);
	static void clear_test_index_dir();
	static String test_get_index_file_path() { return get_index_file_path(); }
#endif

	const HashMap<String, SemanticAssetEntry> &get_entries() const { return entries; }
	HashMap<String, SemanticAssetEntry> &get_entries_mut() { return entries; }

	Error load();
	Error save() const;
	static Error save_dirty(const HashSet<String> &p_dirty_paths, const HashMap<String, SemanticAssetEntry> &p_entries);
	static bool save_dirty_sidecars(const HashSet<String> &p_dirty_paths, const HashMap<String, SemanticAssetEntry> &p_entries);
	static bool apply_dirty_sidecars(HashMap<String, SemanticAssetEntry> &r_entries);
	static bool compact_index_sidecars(const HashMap<String, SemanticAssetEntry> &p_entries);
	static bool clear_index_dirty_sidecars();
	static String get_index_dirty_dir();
	static int get_index_version();
	void clear();
	bool has_entry(const String &p_path) const;
	const SemanticAssetEntry *get_entry(const String &p_path) const;
	void set_entry(const String &p_path, const SemanticAssetEntry &p_entry);
	void remove_entry(const String &p_path);
};
