/**************************************************************************/
/*  asset_tag_manager.h                                                   */
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
#include "core/templates/vector.h"

class AssetTagManager : public Object {
	GDCLASS(AssetTagManager, Object);

	static AssetTagManager *singleton;

	HashMap<String, AssetTagEntry> tags;
	Vector<AssetTagRedirect> redirects;
	HashMap<String, Vector<String>> children_by_parent;
	int batch_depth = 0;
	bool dictionary_dirty = false;

	void _rebuild_children_index();
	void _update_children_for_tag(const String &p_tag_name, bool p_add);

	String _resolve_redirect(const String &p_tag) const;
	bool _is_valid_tag_name(const String &p_tag) const;
	Vector<String> _split_tag_parts(const String &p_tag) const;

protected:
	static void _bind_methods();

public:
	static AssetTagManager *get_singleton();

	Error load();
	Error save();
	void begin_batch();
	Error commit_batch(bool p_persist = true);
	void abort_batch();
	bool is_in_batch() const { return batch_depth > 0; }
	bool is_dictionary_dirty() const { return dictionary_dirty; }
	void get_dictionary_snapshot(HashMap<String, AssetTagEntry> &r_tags, Vector<AssetTagRedirect> &r_redirects) const;
	void mark_dictionary_persisted() { dictionary_dirty = false; }

	PackedStringArray list_tags(const String &p_parent_tag = String()) const;
	PackedStringArray list_all_tags() const;
	Dictionary get_tag_info(const String &p_tag_name) const;

	Error add_tag(const String &p_tag_name, const String &p_comment = String(), const String &p_source = "default");
	Error remove_tag(const String &p_tag_name);
	Error rename_tag(const String &p_old_name, const String &p_new_name);
	Error update_tag_comment(const String &p_tag_name, const String &p_comment);

	bool has_tag_in_dictionary(const String &p_tag_name) const;
	String resolve_tag_alias(const String &p_tag_name) const;
	bool matches_tag(const String &p_tag, const String &p_query_tag) const;
	bool container_has_tag(const PackedStringArray &p_container, const String &p_tag) const;
	bool container_has_any(const PackedStringArray &p_container, const PackedStringArray &p_query_tags) const;
	bool container_has_all(const PackedStringArray &p_container, const PackedStringArray &p_query_tags) const;

	PackedStringArray get_unused_tags(const HashMap<String, Vector<String>> &p_index) const;

	AssetTagManager();
	~AssetTagManager();
};
