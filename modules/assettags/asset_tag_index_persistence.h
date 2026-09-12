/**************************************************************************/
/*  asset_tag_index_persistence.h                                         */
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

#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/templates/vector.h"

class AssetTagIndexPersistence {
public:
	static bool load_index(HashMap<String, Vector<String>> &r_index);
	static bool save_index(const HashMap<String, Vector<String>> &p_index);
	static bool save_index_merge(const HashMap<String, Vector<String>> &p_full_index, const HashSet<String> &p_dirty_paths);
	static String get_index_dirty_dir();
	static bool clear_index_dirty_sidecars();
	static bool quarantine_index_dirty_sidecars();
	static bool quarantine_corrupt_index();
	static bool apply_dirty_sidecars(HashMap<String, Vector<String>> &r_index);
	static bool compact_index_sidecars(const HashMap<String, Vector<String>> &p_full_index);
	static bool save_index_dirty_sidecars(const HashMap<String, Vector<String>> &p_full_index, const HashSet<String> &p_dirty_paths);
	static bool write_index_payload(const HashMap<String, Vector<String>> &p_index, const String &p_path);
};
