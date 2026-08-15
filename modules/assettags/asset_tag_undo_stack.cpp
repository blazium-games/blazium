/**************************************************************************/
/*  asset_tag_undo_stack.cpp                                              */
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

#include "asset_tag_undo_stack.h"

#include "asset_tag_storage.h"
#include "asset_tag_undo_io.h"
#include "persistence/asset_tag_file_io.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"

static int g_undo_snapshot_depth = 0;

static String _undo_dir() {
	return AssetTagUndoIO::get_undo_dir(AssetTagStorage::get_storage_dir());
}

static String _undo_prev_dir() {
	return AssetTagStorage::get_storage_dir().path_join("undo_prev");
}

static String _undo_index_dirty_dir() {
	return AssetTagUndoIO::get_undo_index_dirty_dir(AssetTagStorage::get_storage_dir());
}

static bool _remove_dir_recursive(const String &p_dir) {
	if (!DirAccess::dir_exists_absolute(p_dir)) {
		return true;
	}
	Ref<DirAccess> dir = DirAccess::open(p_dir);
	if (dir.is_null()) {
		return false;
	}
	dir->list_dir_begin();
	String file_name = dir->get_next();
	while (!file_name.is_empty()) {
		if (!dir->current_is_dir()) {
			dir->remove(file_name);
		}
		file_name = dir->get_next();
	}
	dir->list_dir_end();
	Ref<DirAccess> parent = DirAccess::open(p_dir.get_base_dir());
	if (parent.is_valid()) {
		return parent->remove(p_dir) == OK;
	}
	return false;
}

static bool _has_undo_prev_state() {
	return FileAccess::exists(_undo_prev_dir().path_join("tags.json")) && FileAccess::exists(_undo_prev_dir().path_join("asset_index.json"));
}

static bool _write_undo_snapshot_file(const String &p_source, const String &p_destination, const String &p_empty_payload) {
	if (FileAccess::exists(p_source)) {
		return AssetTagFileIO::copy_file_strict(p_source, p_destination);
	}
	return AssetTagFileIO::atomic_write_text_file(p_destination, p_empty_payload);
}

Error AssetTagUndoStack::snapshot() {
	if (g_undo_snapshot_depth > 0) {
		return OK;
	}
	if (!AssetTagStorage::ensure_storage_dir()) {
		return ERR_CANT_CREATE;
	}
	const String undo_dir = _undo_dir();
	Ref<DirAccess> dir = DirAccess::open(AssetTagStorage::get_storage_dir());
	if (dir.is_null()) {
		return ERR_CANT_CREATE;
	}
	if (!dir->dir_exists(undo_dir)) {
		const Error err = dir->make_dir_recursive(undo_dir);
		if (err != OK && err != ERR_ALREADY_EXISTS) {
			return err;
		}
	}
	if (has_state()) {
		_remove_dir_recursive(_undo_prev_dir());
		if (!AssetTagFileIO::copy_dir_recursive(_undo_dir(), _undo_prev_dir())) {
			clear();
			return ERR_CANT_CREATE;
		}
	}
	_remove_dir_recursive(_undo_dir());
	if (!dir->dir_exists(undo_dir)) {
		const Error err = dir->make_dir_recursive(undo_dir);
		if (err != OK && err != ERR_ALREADY_EXISTS) {
			clear();
			return ERR_CANT_CREATE;
		}
	}
	const String tags_tmp = undo_dir.path_join("tags.json.tmp");
	const String index_tmp = undo_dir.path_join("asset_index.json.tmp");
	static const String k_empty_tags_snapshot = "{\"tags\":{},\"redirects\":[]}";
	static const String k_empty_index_snapshot = "{}";
	if (!_write_undo_snapshot_file(AssetTagStorage::get_tags_file_path(), tags_tmp, k_empty_tags_snapshot)) {
		clear();
		return ERR_CANT_CREATE;
	}
	if (!_write_undo_snapshot_file(AssetTagStorage::get_index_file_path(), index_tmp, k_empty_index_snapshot)) {
		AssetTagFileIO::remove_file_if_exists(tags_tmp);
		clear();
		return ERR_CANT_CREATE;
	}
	Ref<FileAccess> tags_input = FileAccess::open(tags_tmp, FileAccess::READ);
	if (tags_input.is_null()) {
		AssetTagFileIO::remove_file_if_exists(tags_tmp);
		AssetTagFileIO::remove_file_if_exists(index_tmp);
		clear();
		return ERR_CANT_CREATE;
	}
	const String tags_text = tags_input->get_as_text();
	tags_input.unref();
	Ref<FileAccess> index_input = FileAccess::open(index_tmp, FileAccess::READ);
	if (index_input.is_null()) {
		AssetTagFileIO::remove_file_if_exists(tags_tmp);
		AssetTagFileIO::remove_file_if_exists(index_tmp);
		clear();
		return ERR_CANT_CREATE;
	}
	const String index_text = index_input->get_as_text();
	index_input.unref();
	if (!AssetTagFileIO::atomic_write_text_file(undo_dir.path_join("tags.json"), tags_text)) {
		AssetTagFileIO::remove_file_if_exists(tags_tmp);
		AssetTagFileIO::remove_file_if_exists(index_tmp);
		clear();
		return ERR_CANT_CREATE;
	}
	if (!AssetTagFileIO::atomic_write_text_file(undo_dir.path_join("asset_index.json"), index_text)) {
		AssetTagFileIO::remove_file_if_exists(undo_dir.path_join("tags.json"));
		AssetTagFileIO::remove_file_if_exists(tags_tmp);
		AssetTagFileIO::remove_file_if_exists(index_tmp);
		clear();
		return ERR_CANT_CREATE;
	}
	AssetTagFileIO::remove_file_if_exists(tags_tmp);
	AssetTagFileIO::remove_file_if_exists(index_tmp);
	if (!AssetTagFileIO::copy_dir_recursive(AssetTagStorage::get_index_dirty_dir(), _undo_index_dirty_dir())) {
		clear();
		return ERR_CANT_CREATE;
	}
	g_undo_snapshot_depth++;
	return OK;
}

Error AssetTagUndoStack::restore() {
	if (!has_state()) {
		return ERR_DOES_NOT_EXIST;
	}
	return AssetTagUndoIO::restore_undo_state(AssetTagStorage::get_storage_dir());
}

bool AssetTagUndoStack::has_state() {
	const String undo_dir = _undo_dir();
	return FileAccess::exists(undo_dir.path_join("tags.json")) && FileAccess::exists(undo_dir.path_join("asset_index.json"));
}

void AssetTagUndoStack::clear() {
	g_undo_snapshot_depth = 0;
	_remove_dir_recursive(_undo_dir());
	_remove_dir_recursive(_undo_prev_dir());
}

void AssetTagUndoStack::rotate_after_restore() {
	g_undo_snapshot_depth = 0;
	_remove_dir_recursive(_undo_dir());
	if (_has_undo_prev_state()) {
		AssetTagFileIO::copy_dir_recursive(_undo_prev_dir(), _undo_dir());
		_remove_dir_recursive(_undo_prev_dir());
	}
}

void AssetTagUndoStack::mark_committed() {
	g_undo_snapshot_depth = 0;
}
