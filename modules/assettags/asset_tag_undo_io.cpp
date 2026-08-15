/**************************************************************************/
/*  asset_tag_undo_io.cpp                                                 */
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

#include "asset_tag_undo_io.h"

#include "asset_tag_sidecar_io.h"
#include "persistence/asset_tag_file_io.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"

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

String AssetTagUndoIO::get_undo_dir(const String &p_storage_dir) {
	return p_storage_dir.path_join("undo");
}

String AssetTagUndoIO::get_undo_index_dirty_dir(const String &p_storage_dir) {
	return get_undo_dir(p_storage_dir).path_join("index_dirty");
}

Error AssetTagUndoIO::restore_undo_state(const String &p_storage_dir) {
	const String undo_dir = get_undo_dir(p_storage_dir);
	const String tags_undo = undo_dir.path_join("tags.json");
	const String index_undo = undo_dir.path_join("asset_index.json");
	if (!FileAccess::exists(tags_undo) || !FileAccess::exists(index_undo)) {
		return ERR_FILE_MISSING_DEPENDENCIES;
	}

	const String tags_path = p_storage_dir.path_join("tags.json");
	const String index_path = p_storage_dir.path_join("asset_index.json");
	const String dirty_dir = p_storage_dir.path_join("index_dirty");
	const String rollback_dir = p_storage_dir.path_join("undo_rollback");

	if (!AssetTagFileIO::backup_file_for_rollback(tags_path, rollback_dir.path_join("tags.json")) ||
			!AssetTagFileIO::backup_file_for_rollback(index_path, rollback_dir.path_join("asset_index.json"))) {
		_remove_dir_recursive(rollback_dir);
		return ERR_CANT_CREATE;
	}
	if (!AssetTagFileIO::copy_dir_recursive(dirty_dir, rollback_dir.path_join("index_dirty"))) {
		_remove_dir_recursive(rollback_dir);
		return ERR_CANT_CREATE;
	}

	auto rollback_live = [&]() {
		AssetTagFileIO::restore_file_from_rollback(rollback_dir.path_join("tags.json"), tags_path);
		AssetTagFileIO::restore_file_from_rollback(rollback_dir.path_join("asset_index.json"), index_path);
		AssetTagSidecarIO::clear_index_dirty_sidecars(dirty_dir);
		AssetTagFileIO::copy_dir_recursive(rollback_dir.path_join("index_dirty"), dirty_dir);
		_remove_dir_recursive(rollback_dir);
	};

	if (!AssetTagFileIO::copy_file_strict(tags_undo, tags_path)) {
		rollback_live();
		return ERR_CANT_CREATE;
	}
	if (!AssetTagFileIO::copy_file_strict(index_undo, index_path)) {
		rollback_live();
		return ERR_CANT_CREATE;
	}
	AssetTagSidecarIO::clear_index_dirty_sidecars(dirty_dir);
	if (!AssetTagFileIO::copy_dir_recursive(get_undo_index_dirty_dir(p_storage_dir), dirty_dir)) {
		rollback_live();
		return ERR_CANT_CREATE;
	}
	_remove_dir_recursive(rollback_dir);
	return OK;
}
