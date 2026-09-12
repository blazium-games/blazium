/**************************************************************************/
/*  multiuser_editor_crdt_text_buffer.h                                   */
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

#include "core/templates/hash_set.h"
#include "core/templates/vector.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

class MultiuserEditorCRDTTextBuffer {
	struct Atom {
		Vector<int> position;
		String site;
		int clock = 0;
		String character;
	};

	Vector<Atom> atoms;
	HashSet<String> atom_index;
	String site_id;
	int clock = 0;
	int import_atom_cap_override = -1;
	int export_atom_cap_override = -1;
	int atoms_max = 1000000;
	mutable int dropped_due_to_cap = 0;

	static int _compare_positions(const Vector<int> &p_a, const Vector<int> &p_b);
	static int _compare_atoms(const Atom &p_a, const Atom &p_b);

	struct AtomCompare {
		_FORCE_INLINE_ bool operator()(const Atom &a, const Atom &b) const {
			return _compare_atoms(a, b) < 0;
		}
	};

	static Array _position_to_array(const Vector<int> &p_position);
	static Vector<int> _array_to_position(const Array &p_position);
	static String _atom_key(const String &p_site, int p_clock);
	Vector<int> _alloc_position_between(const Vector<int> &p_before, const Vector<int> &p_after) const;
	int _find_insert_index(const Atom &p_atom) const;

public:
	void init(const String &p_site_id);
	Dictionary local_insert(int p_index, const String &p_character);
	Dictionary local_delete(int p_index);
	int remote_insert(const Dictionary &p_op);
	int remote_delete(const Dictionary &p_op);
	String get_text() const;
	Dictionary export_state() const;

	bool import_state(const Dictionary &p_state);
	void set_import_atom_cap_override(int p_cap) { import_atom_cap_override = p_cap; }
	void set_export_atom_cap_override(int p_cap) { export_atom_cap_override = p_cap; }

	void set_atoms_max(int p_max) { atoms_max = MAX(1, p_max); }
	int get_atoms_max() const { return atoms_max; }
	int consume_dropped_due_to_cap() const {
		int v = dropped_due_to_cap;
		dropped_due_to_cap = 0;
		return v;
	}
	int get_atom_count() const { return atoms.size(); }
};

#endif
