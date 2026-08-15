/**************************************************************************/
/*  asset_tag_sidecar_io.h                                                */
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

class AssetTagSidecarIO {
public:
	static String encode_sidecar_name(const String &p_path);
	static uint64_t get_index_dirty_max_mtime(const String &p_dirty_dir);
	static bool apply_dirty_sidecars(const String &p_dirty_dir, HashMap<String, Vector<String>> &r_index);
	static bool save_dirty_sidecars(
			const String &p_dirty_dir,
			const HashMap<String, Vector<String>> &p_full_index,
			const HashSet<String> &p_dirty_paths,
			Vector<String> *r_written_file_names = nullptr);
	static bool clear_index_dirty_sidecars(const String &p_dirty_dir);
	static bool quarantine_index_dirty_sidecars(const String &p_dirty_dir);
	static String resolve_unique_sidecar_file_name(const String &p_dirty_dir, const String &p_path);
};
