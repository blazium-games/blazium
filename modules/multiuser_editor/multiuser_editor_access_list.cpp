/**************************************************************************/
/*  multiuser_editor_access_list.cpp                                      */
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

#ifdef TOOLS_ENABLED

#include "multiuser_editor_access_list.h"

#include "multiuser_editor_constants.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/string/string_builder.h"
#include "core/templates/hash_set.h"

MultiuserEditorAccessList::MultiuserEditorAccessList() {
}

String MultiuserEditorAccessList::default_path() {
	return String(multiuser_editor::kDefaultAccessListPath);
}

void MultiuserEditorAccessList::set_max_entries(int p_max) {
	_max_entries = CLAMP(p_max, 1, multiuser_editor::kAccessListMaxEntriesCeiling);
	while (_entries.size() > _max_entries) {
		_entries.remove_at(_entries.size() - 1);
	}
}

bool MultiuserEditorAccessList::is_valid_role(const String &p_role) {
	return p_role == multiuser_editor::kRoleViewer || p_role == multiuser_editor::kRoleEditor || p_role == multiuser_editor::kRoleAdmin;
}

bool MultiuserEditorAccessList::is_valid_codename(const String &p_codename) {
	if (p_codename.is_empty() || p_codename.length() > MAX_CODENAME_CHARS) {
		return false;
	}
	for (int i = 0; i < p_codename.length(); i++) {
		const char32_t c = p_codename[i];
		const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
				(c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
		if (!ok) {
			return false;
		}
	}
	return true;
}

bool MultiuserEditorAccessList::has_codename(const String &p_codename) const {
	for (int i = 0; i < _entries.size(); i++) {
		if (_entries[i].codename == p_codename) {
			return true;
		}
	}
	return false;
}

Error MultiuserEditorAccessList::add_or_update(const Entry &p_entry) {
	if (!is_valid_codename(p_entry.codename)) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_entry.password.is_empty() || p_entry.password.length() > MAX_PASSWORD_CHARS) {
		return ERR_INVALID_PARAMETER;
	}
	if (!is_valid_role(p_entry.role)) {
		return ERR_INVALID_PARAMETER;
	}
	for (int i = 0; i < _entries.size(); i++) {
		if (_entries[i].codename == p_entry.codename) {
			Entry e = p_entry;
			if (e.source.is_empty()) {
				e.source = _entries[i].source.is_empty() ? String("file") : _entries[i].source;
			}
			_entries.write[i] = e;
			return OK;
		}
	}
	if (_entries.size() >= _max_entries) {
		return ERR_OUT_OF_MEMORY;
	}
	Entry e = p_entry;
	if (e.source.is_empty()) {
		e.source = "file";
	}
	_entries.push_back(e);
	return OK;
}

Error MultiuserEditorAccessList::remove(const String &p_codename) {
	for (int i = 0; i < _entries.size(); i++) {
		if (_entries[i].codename == p_codename) {
			_entries.remove_at(i);
			return OK;
		}
	}
	return ERR_DOES_NOT_EXIST;
}

void MultiuserEditorAccessList::clear() {
	_entries.clear();
}

Error MultiuserEditorAccessList::load_from_file(const String &p_path) {
	if (p_path.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	if (!FileAccess::exists(p_path)) {
		_entries.clear();
		return OK;
	}
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		_entries.clear();
		return ERR_CANT_OPEN;
	}
	const int64_t len = f->get_length();
	if (len < 0 || len > int64_t(MAX_FILE_BYTES)) {
		f->close();

		_entries.clear();
		return ERR_OUT_OF_MEMORY;
	}
	String text = f->get_as_text();
	f->close();

	const Variant parsed = JSON::parse_string(text);
	if (parsed.get_type() != Variant::DICTIONARY) {
		_entries.clear();
		return ERR_FILE_CORRUPT;
	}
	const Dictionary root = parsed;
	if (!root.has("entries")) {
		_entries.clear();
		return OK;
	}
	const Variant entries_v = root["entries"];
	if (entries_v.get_type() != Variant::ARRAY) {
		_entries.clear();
		return ERR_FILE_CORRUPT;
	}
	const Array entries = entries_v;
	_entries.clear();
	HashSet<String> seen;
	for (int i = 0; i < entries.size() && _entries.size() < _max_entries; i++) {
		const Variant ev = entries[i];
		if (ev.get_type() != Variant::DICTIONARY) {
			print_line(vformat("AccessList: skipping non-dict entry at index %d", i));
			continue;
		}
		const Dictionary d = ev;
		Entry e;
		e.codename = String(d.get("codename", ""));
		e.password = String(d.get("password", ""));
		e.role = String(d.get("role", multiuser_editor::kRoleEditor));
		e.source = "file";
		if (!is_valid_codename(e.codename)) {
			print_line(vformat("AccessList: skipping invalid codename at index %d", i));
			continue;
		}
		if (e.password.is_empty() || e.password.length() > MAX_PASSWORD_CHARS) {
			print_line(vformat("AccessList: skipping invalid password (codename=%s)", e.codename));
			continue;
		}
		if (!is_valid_role(e.role)) {
			print_line(vformat("AccessList: skipping invalid role '%s' (codename=%s)", e.role, e.codename));
			continue;
		}
		if (seen.has(e.codename)) {
			print_line(vformat("AccessList: ignoring duplicate codename '%s' (keeping first)", e.codename));
			continue;
		}
		seen.insert(e.codename);
		_entries.push_back(e);
	}
	return OK;
}

Error MultiuserEditorAccessList::save_to_file(const String &p_path) const {
	if (p_path.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	Dictionary root;
	root["version"] = 1;
	Array entries_arr;
	for (int i = 0; i < _entries.size(); i++) {
		Dictionary d;
		d["codename"] = _entries[i].codename;
		d["password"] = _entries[i].password;
		d["role"] = _entries[i].role;
		entries_arr.push_back(d);
	}
	root["entries"] = entries_arr;
	const String pretty = JSON::stringify(root, "  ", true, true);

	const String tmp_path = p_path + ".tmp";
	const String base_dir = p_path.get_base_dir();

	if (!base_dir.is_empty() && ProjectSettings::get_singleton()) {
		const String absolute_base = ProjectSettings::get_singleton()->globalize_path(base_dir);
		if (!absolute_base.is_empty()) {
			DirAccess::make_dir_recursive_absolute(absolute_base);
		}
	}

	Ref<DirAccess> tmp_cleanup_dir = DirAccess::open(base_dir.is_empty() ? String(".") : base_dir);

	auto unlink_tmp = [&]() {
		const String tmp_name = tmp_path.get_file();
		const String tmp_dir = tmp_path.get_base_dir();
		if (tmp_cleanup_dir.is_valid()) {
			tmp_cleanup_dir->remove(tmp_name);
			return;
		}
		if (!FileAccess::exists(tmp_path)) {
			return;
		}
		Ref<DirAccess> fallback = DirAccess::open(tmp_dir.is_empty() ? String(".") : tmp_dir);
		if (fallback.is_valid()) {
			fallback->remove(tmp_name);
		}
	};

	{
		Ref<FileAccess> f = FileAccess::open(tmp_path, FileAccess::WRITE);
		if (f.is_null()) {
			unlink_tmp();
			return ERR_CANT_OPEN;
		}
		f->store_string(pretty);
		f->close();
	}

	if (tmp_cleanup_dir.is_valid()) {
		String tmp_name = tmp_path.get_file();
		String final_name = p_path.get_file();

		if (FileAccess::exists(p_path)) {
			tmp_cleanup_dir->remove(final_name);
		}
		const Error re = tmp_cleanup_dir->rename(tmp_name, final_name);
		if (re == OK) {
			return OK;
		}
	}

	{
		Ref<FileAccess> in = FileAccess::open(tmp_path, FileAccess::READ);
		if (in.is_null()) {
			unlink_tmp();
			return ERR_CANT_OPEN;
		}
		Ref<FileAccess> out = FileAccess::open(p_path, FileAccess::WRITE);
		if (out.is_null()) {
			in->close();
			unlink_tmp();
			return ERR_CANT_OPEN;
		}
		out->store_string(in->get_as_text());
		in->close();
		out->close();
	}
	unlink_tmp();
	return OK;
}

bool MultiuserEditorAccessList::const_time_eq(const String &p_a, const String &p_b) {
	const CharString a = p_a.utf8();
	const CharString b = p_b.utf8();
	const int la = a.length();
	const int lb = b.length();
	const int n = MAX(la, lb);
	uint32_t diff = uint32_t(la ^ lb);
	for (int i = 0; i < n; i++) {
		const uint8_t ca = i < la ? uint8_t(a[i]) : 0;
		const uint8_t cb = i < lb ? uint8_t(b[i]) : 0;
		diff |= uint32_t(ca ^ cb);
	}
	return diff == 0;
}

bool MultiuserEditorAccessList::find_match_for_hmac(const String &p_challenge,
		const String &p_remote_hmac,
		Entry &r_matched) const {
	bool found = false;
	Entry matched;
	for (int i = 0; i < _entries.size(); i++) {
		const String expected = (p_challenge + _entries[i].password).sha256_text();
		const bool eq = const_time_eq(p_remote_hmac, expected);
		if (eq && !found) {
			matched = _entries[i];
			found = true;
		}
	}
	if (found) {
		r_matched = matched;
		return true;
	}
	return false;
}

String MultiuserEditorAccessList::canonicalize_path(const String &p_path) {
	if (p_path.is_empty()) {
		return String();
	}

	String s = p_path.strip_edges().replace("\\", "/");
	if (s.is_empty()) {
		return String();
	}
	String prefix;
	if (s.begins_with("res://")) {
		prefix = "res://";
		s = s.substr(6, s.length() - 6);
	} else if (s.begins_with("user://")) {
		prefix = "user://";
		s = s.substr(7, s.length() - 7);
	}

	Vector<String> parts = s.split("/", false);
	StringBuilder sb;
	bool first = true;
	for (int i = 0; i < parts.size(); i++) {
		const String &p = parts[i];
		if (p == ".") {
			continue;
		}
		if (p == "..") {
			return String();
		}
		if (p.contains(String::chr(0))) {
			return String();
		}
		if (!first) {
			sb += "/";
		}
		sb += p;
		first = false;
	}
	return prefix + sb.as_string();
}

bool MultiuserEditorAccessList::path_equals(const String &p_a, const String &p_b) {
	const String ca = canonicalize_path(p_a);
	const String cb = canonicalize_path(p_b);
	if (ca.is_empty() || cb.is_empty()) {
		return false;
	}
	return ca == cb;
}

Error MultiuserEditorAccessList::ensure_in_gitignore(const String &p_target_file_canonical,
		const String &p_gitignore_dir) {
	if (p_target_file_canonical.is_empty() || p_gitignore_dir.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	const String target = canonicalize_path(p_target_file_canonical);
	String dir_canon = canonicalize_path(p_gitignore_dir);
	if (!dir_canon.ends_with("/")) {
		dir_canon += "/";
	}
	if (!target.begins_with(dir_canon)) {
		return OK;
	}
	const String relative = target.substr(dir_canon.length(), target.length() - dir_canon.length());
	if (relative.is_empty() || relative.contains("..")) {
		return ERR_INVALID_PARAMETER;
	}

	const String gitignore_path = dir_canon + ".gitignore";
	String existing;
	if (FileAccess::exists(gitignore_path)) {
		Ref<FileAccess> in = FileAccess::open(gitignore_path, FileAccess::READ);
		if (in.is_null()) {
			return ERR_CANT_OPEN;
		}
		const int64_t len = in->get_length();
		if (len < 0 || len > int64_t(MAX_GITIGNORE_BYTES)) {
			in->close();
			print_line(vformat("AccessList: refusing to edit oversized .gitignore (%d bytes)", int(len)));
			return ERR_OUT_OF_MEMORY;
		}
		existing = in->get_as_text();
		in->close();
	}

	{
		Vector<String> lines = existing.split("\n", true);
		for (int i = 0; i < lines.size(); i++) {
			const String trimmed = lines[i].strip_edges();
			if (trimmed == relative) {
				return OK;
			}
		}
	}

	StringBuilder out;
	out += existing;
	if (!existing.is_empty() && !existing.ends_with("\n")) {
		out += "\n";
	}
	out += "\n# Multiuser Editor access list (local credentials, do not commit).\n";
	out += relative;
	out += "\n";

	const String tmp_path = gitignore_path + ".tmp";

	if (ProjectSettings::get_singleton()) {
		const String absolute_dir = ProjectSettings::get_singleton()->globalize_path(dir_canon);
		if (!absolute_dir.is_empty()) {
			DirAccess::make_dir_recursive_absolute(absolute_dir);
		}
	}
	{
		Ref<FileAccess> f = FileAccess::open(tmp_path, FileAccess::WRITE);
		if (f.is_null()) {
			return ERR_CANT_OPEN;
		}
		f->store_string(out.as_string());
		f->close();
	}
	Ref<DirAccess> da = DirAccess::open(dir_canon);
	if (da.is_valid()) {
		if (FileAccess::exists(gitignore_path)) {
			da->remove(".gitignore");
		}
		const Error re = da->rename(".gitignore.tmp", ".gitignore");
		if (re == OK) {
			return OK;
		}
	}

	{
		Ref<FileAccess> in = FileAccess::open(tmp_path, FileAccess::READ);
		if (in.is_null()) {
			return ERR_CANT_OPEN;
		}
		Ref<FileAccess> outf = FileAccess::open(gitignore_path, FileAccess::WRITE);
		if (outf.is_null()) {
			in->close();
			return ERR_CANT_OPEN;
		}
		outf->store_string(in->get_as_text());
		in->close();
		outf->close();
	}
	if (da.is_valid()) {
		da->remove(".gitignore.tmp");
	}
	return OK;
}

#endif
