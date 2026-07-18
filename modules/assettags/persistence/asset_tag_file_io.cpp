/**************************************************************************/
/*  asset_tag_file_io.cpp                                                 */
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

#include "asset_tag_file_io.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"

static Ref<DirAccess> _open_dir_for_path(const String &p_path) {
	const String base_dir = p_path.get_base_dir();
	Ref<DirAccess> dir = DirAccess::open(base_dir);
	if (dir.is_valid()) {
		return dir;
	}
	if (base_dir.begins_with("user://")) {
		return DirAccess::create(DirAccess::ACCESS_USERDATA);
	}
	if (base_dir.begins_with("res://")) {
		return DirAccess::create(DirAccess::ACCESS_RESOURCES);
	}
	DirAccess::make_dir_recursive_absolute(base_dir);
	return DirAccess::create_for_path(base_dir);
}

bool AssetTagFileIO::remove_file_if_exists(const String &p_path) {
	if (!FileAccess::exists(p_path)) {
		return true;
	}
	const String parent = p_path.get_base_dir();
	const String file_name = p_path.get_file();
	Ref<DirAccess> dir = DirAccess::open(parent);
	if (dir.is_valid() && dir->remove(file_name) == OK) {
		return true;
	}
	if (ProjectSettings::get_singleton()) {
		Ref<DirAccess> fs = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		if (fs.is_valid()) {
			return fs->remove(ProjectSettings::get_singleton()->globalize_path(p_path)) == OK;
		}
	}
	return false;
}

bool AssetTagFileIO::rename_file_absolute(const String &p_old_path, const String &p_new_path) {
	if (!FileAccess::exists(p_old_path)) {
		return false;
	}
	remove_file_if_exists(p_new_path);
	if (!copy_file_strict(p_old_path, p_new_path)) {
		return false;
	}
	return remove_file_if_exists(p_old_path);
}

bool AssetTagFileIO::atomic_write_text_file(const String &p_path, const String &p_text) {
	const String temp_path = p_path + ".tmp";
	Ref<FileAccess> file = FileAccess::open(temp_path, FileAccess::WRITE);
	if (file.is_null()) {
		return false;
	}
	file->store_string(p_text);
	file.unref();
	Ref<DirAccess> dir = _open_dir_for_path(p_path);
	if (dir.is_null()) {
		return false;
	}
	if (dir->rename_absolute(temp_path, p_path) == OK) {
		return true;
	}
	if (!copy_file_strict(temp_path, p_path)) {
		remove_file_if_exists(temp_path);
		return false;
	}
	remove_file_if_exists(temp_path);
	return true;
}

bool AssetTagFileIO::copy_file_strict(const String &p_source, const String &p_destination) {
	if (!FileAccess::exists(p_source)) {
		return false;
	}
	if (DirAccess::copy_absolute(p_source, p_destination) == OK) {
		return true;
	}
	Ref<FileAccess> input = FileAccess::open(p_source, FileAccess::READ);
	if (input.is_null()) {
		return false;
	}
	const String text = input->get_as_text();
	input.unref();
	Ref<DirAccess> parent = _open_dir_for_path(p_destination);
	if (parent.is_valid() && !parent->dir_exists(p_destination.get_base_dir())) {
		parent->make_dir_recursive(p_destination.get_base_dir());
	}
	Ref<FileAccess> output = FileAccess::open(p_destination, FileAccess::WRITE);
	if (output.is_null()) {
		return false;
	}
	output->store_string(text);
	return true;
}

bool AssetTagFileIO::backup_file_for_rollback(const String &p_path, const String &p_backup_path) {
	if (!FileAccess::exists(p_path)) {
		return remove_file_if_exists(p_backup_path);
	}
	return copy_file_strict(p_path, p_backup_path);
}

bool AssetTagFileIO::restore_file_from_rollback(const String &p_backup_path, const String &p_path) {
	if (!FileAccess::exists(p_backup_path)) {
		return remove_file_if_exists(p_path);
	}
	return copy_file_strict(p_backup_path, p_path);
}

bool AssetTagFileIO::copy_dir_recursive(const String &p_source_dir, const String &p_destination_dir) {
	if (!DirAccess::dir_exists_absolute(p_source_dir)) {
		return true;
	}
	Ref<DirAccess> source = DirAccess::open(p_source_dir);
	if (source.is_null()) {
		return false;
	}
	Ref<DirAccess> destination = _open_dir_for_path(p_destination_dir.path_join("."));
	if (destination.is_null()) {
		return false;
	}
	if (!destination->dir_exists(p_destination_dir)) {
		const Error mkdir_err = destination->make_dir_recursive(p_destination_dir);
		if (mkdir_err != OK && mkdir_err != ERR_ALREADY_EXISTS) {
			return false;
		}
	}
	source->list_dir_begin();
	String file_name = source->get_next();
	while (!file_name.is_empty()) {
		if (file_name == "." || file_name == "..") {
			file_name = source->get_next();
			continue;
		}
		const String source_path = p_source_dir.path_join(file_name);
		const String destination_path = p_destination_dir.path_join(file_name);
		if (source->current_is_dir()) {
			if (!copy_dir_recursive(source_path, destination_path)) {
				source->list_dir_end();
				return false;
			}
		} else if (!copy_file_strict(source_path, destination_path)) {
			source->list_dir_end();
			return false;
		}
		file_name = source->get_next();
	}
	source->list_dir_end();
	return true;
}
