/**************************************************************************/
/*  multiuser_editor_access_list.h                                        */
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

#ifdef TOOLS_ENABLED

#include "core/error/error_list.h"
#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"

class MultiuserEditorAccessList : public RefCounted {
public:
	struct Entry {
		String codename;
		String password;
		String role;
		String source;
	};

private:
	Vector<Entry> _entries;
	int _max_entries = 256;

public:
	static String default_path();

	enum {
		MAX_CODENAME_CHARS = 64,
		MAX_PASSWORD_CHARS = 1024,
		MAX_FILE_BYTES = 1024 * 1024,
		MAX_GITIGNORE_BYTES = 1024 * 1024,
	};

	Error load_from_file(const String &p_path);
	Error save_to_file(const String &p_path) const;

	Vector<Entry> get_entries() const { return _entries; }
	int get_entry_count() const { return _entries.size(); }
	bool has_codename(const String &p_codename) const;

	Error add_or_update(const Entry &p_entry);
	Error remove(const String &p_codename);
	void clear();

	bool find_match_for_hmac(const String &p_challenge,
			const String &p_remote_hmac,
			Entry &r_matched) const;

	int get_max_entries() const { return _max_entries; }
	void set_max_entries(int p_max);

	static bool is_valid_role(const String &p_role);
	static bool is_valid_codename(const String &p_codename);

	static String canonicalize_path(const String &p_path);
	static bool path_equals(const String &p_a, const String &p_b);

	static Error ensure_in_gitignore(const String &p_target_file_canonical,
			const String &p_gitignore_dir);

	static bool const_time_eq(const String &p_a, const String &p_b);

	MultiuserEditorAccessList();
};

#endif
