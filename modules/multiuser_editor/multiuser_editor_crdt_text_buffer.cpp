/**************************************************************************/
/*  multiuser_editor_crdt_text_buffer.cpp                                 */
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

#include "multiuser_editor_crdt_text_buffer.h"

#include "multiuser_editor_constants.h"

#include "editor/editor_settings.h"

int MultiuserEditorCRDTTextBuffer::_compare_positions(const Vector<int> &p_a, const Vector<int> &p_b) {
	int count = MAX(p_a.size(), p_b.size());
	for (int i = 0; i < count; i++) {
		int av = i < p_a.size() ? p_a[i] : 0;
		int bv = i < p_b.size() ? p_b[i] : 0;
		if (av != bv) {
			return av < bv ? -1 : 1;
		}
	}
	return 0;
}

int MultiuserEditorCRDTTextBuffer::_compare_atoms(const Atom &p_a, const Atom &p_b) {
	int pos_cmp = _compare_positions(p_a.position, p_b.position);
	if (pos_cmp != 0) {
		return pos_cmp;
	}
	if (p_a.site == p_b.site) {
		return p_a.clock == p_b.clock ? 0 : (p_a.clock < p_b.clock ? -1 : 1);
	}
	return p_a.site < p_b.site ? -1 : 1;
}

Array MultiuserEditorCRDTTextBuffer::_position_to_array(const Vector<int> &p_position) {
	Array array;
	for (int value : p_position) {
		array.append(value);
	}
	return array;
}

Vector<int> MultiuserEditorCRDTTextBuffer::_array_to_position(const Array &p_position) {
	Vector<int> position;
	for (int i = 0; i < p_position.size(); i++) {
		position.push_back(int(p_position[i]));
	}
	return position;
}

String MultiuserEditorCRDTTextBuffer::_atom_key(const String &p_site, int p_clock) {
	return p_site + ":" + itos(p_clock);
}

Vector<int> MultiuserEditorCRDTTextBuffer::_alloc_position_between(const Vector<int> &p_before, const Vector<int> &p_after) const {
	Vector<int> ret;
	int depth = 0;
	while (true) {
		int low = depth < p_before.size() ? p_before[depth] : 0;
		int high = depth < p_after.size() ? p_after[depth] : multiuser_editor::kCRDTPositionFractionMax;
		if (high - low > 1) {
			ret.push_back(low + ((high - low) / 2));
			return ret;
		}
		ret.push_back(low);
		depth++;
	}
}

int MultiuserEditorCRDTTextBuffer::_find_insert_index(const Atom &p_atom) const {
	int low = 0;
	int high = atoms.size();
	while (low < high) {
		int mid = low + ((high - low) / 2);
		if (_compare_atoms(p_atom, atoms[mid]) < 0) {
			high = mid;
		} else {
			low = mid + 1;
		}
	}
	return low;
}

void MultiuserEditorCRDTTextBuffer::init(const String &p_site_id) {
	site_id = p_site_id;
}

Dictionary MultiuserEditorCRDTTextBuffer::local_insert(int p_index, const String &p_character) {
	if (atoms.size() >= atoms_max) {
		dropped_due_to_cap++;
		print_line(vformat("Multiuser CRDT: local_insert dropped: atoms cap %d reached", atoms_max));
		return Dictionary();
	}
	int index = CLAMP(p_index, 0, atoms.size());
	Vector<int> before = index > 0 ? atoms[index - 1].position : Vector<int>();
	Vector<int> after = index < atoms.size() ? atoms[index].position : Vector<int>();
	Atom atom;
	atom.position = _alloc_position_between(before, after);
	atom.site = site_id;
	atom.clock = ++clock;
	atom.character = p_character;
	atoms.insert(index, atom);
	atom_index.insert(_atom_key(atom.site, atom.clock));

	Dictionary op;
	op["op"] = "insert";
	op["position"] = _position_to_array(atom.position);
	op["site"] = atom.site;
	op["clock"] = atom.clock;
	op["char"] = atom.character;
	return op;
}

Dictionary MultiuserEditorCRDTTextBuffer::local_delete(int p_index) {
	Dictionary op;
	if (p_index < 0 || p_index >= atoms.size()) {
		return op;
	}
	Atom atom = atoms[p_index];
	atoms.remove_at(p_index);
	atom_index.erase(_atom_key(atom.site, atom.clock));
	op["op"] = "delete";
	op["site"] = atom.site;
	op["clock"] = atom.clock;
	return op;
}

int MultiuserEditorCRDTTextBuffer::remote_insert(const Dictionary &p_op) {
	String site = String(p_op.get("site", ""));
	if (site.length() > multiuser_editor::kCRDTSiteIDMax) {
		print_line("Multiuser CRDT: remote_insert dropped: site too long");
		return -1;
	}
	String character = String(p_op.get("char", ""));
	if (character.length() > multiuser_editor::kCRDTSiteIDMax) {
		print_line("Multiuser CRDT: remote_insert dropped: char too long");
		return -1;
	}
	Array position_array = p_op.get("position", Array());
	if (position_array.size() > multiuser_editor::kCRDTSiteIDMax) {
		print_line("Multiuser CRDT: remote_insert dropped: position array too long");
		return -1;
	}

	const int64_t raw_clock = int64_t(p_op.get("clock", 0));
	if (raw_clock < 0 || raw_clock > int64_t(multiuser_editor::kCRDTClockMax)) {
		print_line(vformat("Multiuser CRDT: remote_insert dropped: clock out of range (%s)", itos(raw_clock)));
		return -1;
	}
	const int remote_clock = int(raw_clock);

	if (atoms.size() >= atoms_max) {
		dropped_due_to_cap++;
		print_line(vformat("Multiuser CRDT: remote_insert dropped: atoms cap %d reached", atoms_max));
		return -1;
	}
	String key = _atom_key(site, remote_clock);
	if (atom_index.has(key)) {
		return -1;
	}

	Atom atom;
	atom.position = _array_to_position(position_array);
	atom.site = site;
	atom.clock = remote_clock;
	atom.character = character;
	int index = _find_insert_index(atom);
	atoms.insert(index, atom);
	atom_index.insert(key);
	return index;
}

int MultiuserEditorCRDTTextBuffer::remote_delete(const Dictionary &p_op) {
	String site = String(p_op.get("site", ""));
	if (site.length() > multiuser_editor::kCRDTSiteIDMax) {
		print_line("Multiuser CRDT: remote_delete dropped: site too long");
		return -1;
	}
	String key = _atom_key(site, int(p_op.get("clock", 0)));
	for (int i = 0; i < atoms.size(); i++) {
		if (_atom_key(atoms[i].site, atoms[i].clock) == key) {
			atoms.remove_at(i);
			atom_index.erase(key);
			return i;
		}
	}
	return -1;
}

String MultiuserEditorCRDTTextBuffer::get_text() const {
	String ret;
	for (const Atom &atom : atoms) {
		ret += atom.character;
	}
	return ret;
}

Dictionary MultiuserEditorCRDTTextBuffer::export_state() const {
	int max_export = 512000;
	if (export_atom_cap_override > 0) {
		max_export = export_atom_cap_override;
	} else if (EditorSettings::get_singleton() && EditorSettings::get_singleton()->has_setting("blazium/multiuser_editor/limits/crdt_export_max_atoms")) {
		const int v = int(EditorSettings::get_singleton()->get("blazium/multiuser_editor/limits/crdt_export_max_atoms"));
		if (v > 0) {
			max_export = v;
		}
	}
	const int export_limit = MIN(atoms.size(), max_export);
	if (atoms.size() > export_limit) {
		print_line(vformat("Multiuser CRDT: export_state truncated %d atoms to cap %d", atoms.size(), export_limit));
	}
	Array atom_array;
	for (int i = 0; i < export_limit; i++) {
		const Atom &atom = atoms[i];
		Dictionary item;
		item["position"] = _position_to_array(atom.position);
		item["site"] = atom.site;
		item["clock"] = atom.clock;
		item["char"] = atom.character;
		atom_array.append(item);
	}
	Dictionary state;
	state["atoms"] = atom_array;
	state["clock"] = clock;
	state["truncated"] = atoms.size() > export_limit;
	return state;
}

bool MultiuserEditorCRDTTextBuffer::import_state(const Dictionary &p_state) {
	if (bool(p_state.get("truncated", false))) {
		print_line("Multiuser CRDT: refusing import_state with truncated==true (peer would diverge); keeping existing buffer.");
		return false;
	}
	atoms.clear();
	atom_index.clear();
	clock = MAX(clock, int(p_state.get("clock", clock)));
	Array atom_array = p_state.get("atoms", Array());

	int max_atoms = 512000;
	if (import_atom_cap_override > 0) {
		max_atoms = import_atom_cap_override;
	} else if (EditorSettings::get_singleton() && EditorSettings::get_singleton()->has_setting("blazium/multiuser_editor/limits/crdt_import_max_atoms")) {
		const int v = int(EditorSettings::get_singleton()->get("blazium/multiuser_editor/limits/crdt_import_max_atoms"));
		if (v > 0) {
			max_atoms = v;
		}
	}
	int insert_limit = MIN(atom_array.size(), max_atoms);

	const bool caller_exceeds_insert_limit = atom_array.size() > insert_limit;

	int dropped_atoms = 0;
	const int prior_drops_due_to_cap = dropped_due_to_cap;
	for (int i = 0; i < insert_limit; i++) {
		const Variant raw_item = atom_array[i];
		if (raw_item.get_type() != Variant::DICTIONARY) {
			dropped_atoms++;
			continue;
		}
		Dictionary item = raw_item;
		Array pos_array = item.get("position", Array());
		const String site = String(item.get("site", ""));
		const String character = String(item.get("char", ""));

		if (pos_array.size() > multiuser_editor::kCRDTSiteIDMax || site.length() > multiuser_editor::kCRDTSiteIDMax || character.length() > multiuser_editor::kCRDTSiteIDMax || site.is_empty()) {
			dropped_atoms++;
			continue;
		}

		const int64_t raw_clock = int64_t(item.get("clock", 0));
		if (raw_clock < 0 || raw_clock > int64_t(multiuser_editor::kCRDTClockMax)) {
			dropped_atoms++;
			continue;
		}

		if (atoms.size() >= atoms_max) {
			dropped_due_to_cap++;
			break;
		}
		Atom atom;
		atom.position = _array_to_position(pos_array);
		atom.site = site;
		atom.clock = int(raw_clock);
		atom.character = character;
		const String key = _atom_key(atom.site, atom.clock);
		if (atom_index.has(key)) {
			dropped_atoms++;
			continue;
		}
		atoms.push_back(atom);
		atom_index.insert(key);
	}
	if (dropped_atoms > 0) {
		print_line(vformat("Multiuser CRDT: import_state dropped %d invalid atoms", dropped_atoms));
	}

	atoms.sort_custom<AtomCompare>();

	const int new_cap_drops = dropped_due_to_cap - prior_drops_due_to_cap;
	if (dropped_atoms > 0 || new_cap_drops > 0 || caller_exceeds_insert_limit) {
		print_line(vformat(
				"Multiuser CRDT: import_state partial (dropped_atoms=%d, dropped_due_to_cap=%d, sender_exceeded_limit=%s); refusing.",
				dropped_atoms, new_cap_drops, caller_exceeds_insert_limit ? "true" : "false"));
		return false;
	}
	return true;
}

#endif
