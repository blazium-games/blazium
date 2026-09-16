/**************************************************************************/
/*  pack_scan.cpp                                                         */
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

#include "io/pack_scan.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/file_access_pack.h"

bool ObfuscationPackScan::is_pack(const String &p_path) {
	String ext = p_path.get_extension().to_lower();
	if (ext == "pck") {
		return true;
	}
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return false;
	}
	uint32_t magic = f->get_32();
	if (magic == PACK_HEADER_MAGIC) {
		return true;
	}
	f->seek_end();
	if (f->get_position() < 12) {
		return false;
	}
	f->seek(f->get_position() - 4);
	return f->get_32() == PACK_HEADER_MAGIC;
}

void ObfuscationPackScan::walk_dir(const String &p_dir, Vector<ObfuscationPackEntry> &r_files) {
	Ref<DirAccess> da = DirAccess::open(p_dir);
	if (da.is_null()) {
		return;
	}
	da->list_dir_begin();
	String fn = da->get_next();
	while (!fn.is_empty()) {
		if (fn == "." || fn == "..") {
			fn = da->get_next();
			continue;
		}
		String path = p_dir.path_join(fn);
		if (da->current_is_dir()) {
			walk_dir(path, r_files);
		} else {
			Ref<FileAccess> f = FileAccess::open(path, FileAccess::READ);
			if (f.is_valid()) {
				ObfuscationPackEntry e;
				e.path = path;
				e.bytes = f->get_buffer(f->get_length());
				r_files.push_back(e);
			}
		}
		fn = da->get_next();
	}
}

static bool find_pck_start(Ref<FileAccess> f, int64_t &r_start) {
	f->seek(0);
	if (f->get_32() == PACK_HEADER_MAGIC) {
		r_start = 0;
		return true;
	}
	f->seek_end();
	int64_t end = f->get_position();
	if (end < 12) {
		return false;
	}
	f->seek(end - 4);
	if (f->get_32() != PACK_HEADER_MAGIC) {
		return false;
	}
	f->seek(end - 12);
	uint64_t ds = f->get_64();
	r_start = end - 4 - (int64_t)ds - 8;
	f->seek(r_start);
	return f->get_32() == PACK_HEADER_MAGIC;
}

Vector<ObfuscationPackEntry> ObfuscationPackScan::list_files(const String &p_path) {
	Vector<ObfuscationPackEntry> files;
	if (DirAccess::dir_exists_absolute(p_path)) {
		walk_dir(p_path, files);
		return files;
	}
	if (!is_pack(p_path) && p_path.get_extension().to_lower() != "exe") {
		Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
		if (f.is_valid()) {
			ObfuscationPackEntry e;
			e.path = p_path;
			e.bytes = f->get_buffer(f->get_length());
			files.push_back(e);
		}
		return files;
	}
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return files;
	}
	int64_t start = 0;
	if (!find_pck_start(f, start)) {
		return files;
	}
	f->seek(start + 4);
	uint32_t version = f->get_32();
	f->get_32();
	f->get_32();
	f->get_32();
	if (version != PACK_FORMAT_VERSION) {
		return files;
	}
	uint32_t pack_flags = f->get_32();
	uint64_t file_base = f->get_64();
	bool rel_filebase = (pack_flags & PACK_REL_FILEBASE);
	if (pack_flags & PACK_DIR_ENCRYPTED) {
		return files;
	}
	for (int i = 0; i < 16; i++) {
		f->get_32();
	}
	int file_count = f->get_32();
	if (rel_filebase) {
		file_base += start;
	}
	struct Rec {
		String path;
		uint64_t ofs = 0;
		uint64_t size = 0;
	};
	Vector<Rec> recs;
	for (int i = 0; i < file_count; i++) {
		uint32_t sl = f->get_32();
		PackedByteArray nameb = f->get_buffer(sl);
		String path;
		path.parse_utf8((const char *)nameb.ptr(), sl);
		uint64_t ofs = f->get_64();
		uint64_t size = f->get_64();
		f->get_buffer(16);
		uint32_t flags = f->get_32();
		if (flags & PACK_FILE_REMOVAL) {
			continue;
		}
		Rec r;
		r.path = path;
		r.ofs = file_base + ofs;
		r.size = size;
		recs.push_back(r);
	}
	for (int i = 0; i < recs.size(); i++) {
		f->seek(recs[i].ofs);
		ObfuscationPackEntry e;
		e.path = recs[i].path;
		e.bytes = f->get_buffer(recs[i].size);
		files.push_back(e);
	}
	return files;
}
