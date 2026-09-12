/**************************************************************************/
/*  multiuser_editor_script_sync.cpp                                      */
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

#include "multiuser_editor_script_sync.h"

#include "core/os/os.h"
#include "multiuser_editor_action_interceptor.h"
#include "multiuser_editor_constants.h"

static Point2i multiuser_flat_index_to_line_column(const String &p_text, int p_index) {
	int line = 0;
	int column = 0;
	int limit = CLAMP(p_index, 0, p_text.length());
	for (int i = 0; i < limit; i++) {
		if (p_text[i] == '\n') {
			line++;
			column = 0;
		} else {
			column++;
		}
	}
	return Point2i(line, column);
}

static int multiuser_line_column_to_flat_index(const String &p_text, int p_line, int p_column) {
	int line = 0;
	int column = 0;
	for (int i = 0; i < p_text.length(); i++) {
		if (line == p_line && column >= p_column) {
			return i;
		}
		if (p_text[i] == '\n') {
			if (line >= p_line) {
				return i;
			}
			line++;
			column = 0;
		} else {
			column++;
		}
	}
	return p_text.length();
}

static int multiuser_adjust_flat_index_for_remote_edit(int p_index, int p_changed_index, int p_delta) {
	if (p_changed_index < 0 || p_delta == 0) {
		return p_index;
	}
	if (p_delta > 0) {
		return p_changed_index <= p_index ? p_index + p_delta : p_index;
	}
	if (p_changed_index < p_index) {
		return MAX(p_changed_index, p_index + p_delta);
	}
	return p_index;
}

void MultiuserEditorScriptSync::_touch_lru(const String &p_script_path) {
	for (List<String>::Element *E = _buffer_lru.front(); E; E = E->next()) {
		if (E->get() == p_script_path) {
			_buffer_lru.erase(E);
			break;
		}
	}
	_buffer_lru.push_front(p_script_path);
}

void MultiuserEditorScriptSync::_evict_lru_if_needed() {
	while (int(script_buffers.size()) > _max_tracked_buffers && !_buffer_lru.is_empty()) {
		const String victim = _buffer_lru.back()->get();
		_buffer_lru.pop_back();
		if (victim == active_script_path) {
			continue;
		}
		script_buffers.erase(victim);
	}
}

void MultiuserEditorScriptSync::_apply_atoms_max_to(MultiuserEditorCRDTTextBuffer &p_buffer) const {
	p_buffer.set_atoms_max(_crdt_atoms_max_per_buffer);
}

void MultiuserEditorScriptSync::_initialize_buffer_from_content(const String &p_script_path, const String &p_content) {
	ScriptBufferState state;
	state.buffer.init(local_peer_id);
	_apply_atoms_max_to(state.buffer);

	const int byte_cap = MAX(multiuser_editor::kScriptSyncFloor, _script_attach_max_bytes);
	if (p_content.length() > byte_cap) {
		const String msg = vformat("Multiuser script_sync: dropped script_attach for %s (len=%d > cap=%d)", p_script_path, p_content.length(), byte_cap);
		print_verbose(msg);
		_security.record(multiuser_editor::kEvtKindReplicationFailed, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatReplication, msg);

		script_buffers[p_script_path] = state;
		_touch_lru(p_script_path);
		_evict_lru_if_needed();
		return;
	}
	for (int i = 0; i < p_content.length(); i++) {
		state.buffer.local_insert(i, String::chr(p_content[i]));
	}
	script_buffers[p_script_path] = state;
	_touch_lru(p_script_path);
	_evict_lru_if_needed();
}

void MultiuserEditorScriptSync::set_crdt_atoms_max_per_buffer(int p_max) {
	_crdt_atoms_max_per_buffer = MAX(multiuser_editor::kScriptSyncFloor, p_max);
	for (KeyValue<String, ScriptBufferState> &E : script_buffers) {
		E.value.buffer.set_atoms_max(_crdt_atoms_max_per_buffer);
	}
}

void MultiuserEditorScriptSync::_append_action(const String &p_type, const Dictionary &p_data, Vector<Dictionary> &r_actions) const {
	Dictionary action;
	Dictionary data = p_data;
	action["type"] = p_type;
	if (data.has("node_path")) {
		action["node_path"] = data["node_path"];
		data.erase("node_path");
	}
	if (p_type == multiuser_editor::kActionCrdt && data.has("op")) {
		action["data"] = data["op"];
	} else {
		action["data"] = data;
	}
	r_actions.push_back(action);
}

void MultiuserEditorScriptSync::set_local_peer_id(const String &p_peer_id) {
	local_peer_id = p_peer_id;
}

void MultiuserEditorScriptSync::set_sync_pending(bool p_pending) {
	sync_pending = p_pending;
	sync_pending_since = OS::get_singleton()->get_ticks_msec() / 1000.0;
}

bool MultiuserEditorScriptSync::is_sync_pending_expired(double p_now, double p_timeout_sec) const {
	return sync_pending && p_now - sync_pending_since > p_timeout_sec;
}

void MultiuserEditorScriptSync::attach_code_edit(CodeEdit *p_code_edit, const String &p_script_path, Vector<Dictionary> &r_actions) {
	if (!p_code_edit || p_script_path.is_empty()) {
		detach_code_edit();
		return;
	}
	String canonical;
	if (!MultiuserEditorActionInterceptor::canonicalize_res_path(p_script_path, canonical)) {
		const String msg = "Multiuser editor: attach_code_edit rejected unsafe script path: " + p_script_path;
		print_verbose(msg);
		_security.record(multiuser_editor::kEvtKindReplicationFailed, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatReplication, msg);
		detach_code_edit();
		return;
	}
	if (active_code_edit == p_code_edit && active_script_path == canonical) {
		_touch_lru(canonical);
		return;
	}
	active_code_edit = p_code_edit;
	active_script_path = canonical;
	active_code_edit->set_meta("multiuser_script_path", active_script_path);
	cached_text = active_code_edit->get_text();
	if (!script_buffers.has(active_script_path)) {
		if (!sync_pending) {
			_initialize_buffer_from_content(active_script_path, cached_text);
			Dictionary data = script_buffers[active_script_path].buffer.export_state();
			data["node_path"] = active_script_path;
			_append_action(multiuser_editor::kActionCrdtSync, data, r_actions);
		}
	} else {
		String buffer_text = script_buffers[active_script_path].buffer.get_text();
		if (buffer_text != cached_text) {
			suppress_script = true;
			int line = active_code_edit->get_caret_line();
			int column = active_code_edit->get_caret_column();
			active_code_edit->set_text(buffer_text);
			active_code_edit->set_caret_line(CLAMP(line, 0, MAX(0, active_code_edit->get_line_count() - 1)));
			active_code_edit->set_caret_column(column);
			cached_text = buffer_text;
			suppress_script = false;
		}
		_touch_lru(active_script_path);
	}
}

void MultiuserEditorScriptSync::detach_code_edit() {
	active_code_edit = nullptr;
	active_script_path = String();
	cached_text = String();
}

void MultiuserEditorScriptSync::clear_all_buffers() {
	script_buffers.clear();
	_buffer_lru.clear();
	detach_code_edit();
	sync_pending = false;
}

void MultiuserEditorScriptSync::remove_buffer(const String &p_script_path) {
	if (!p_script_path.is_empty()) {
		script_buffers.erase(p_script_path);
		for (List<String>::Element *E = _buffer_lru.front(); E; E = E->next()) {
			if (E->get() == p_script_path) {
				_buffer_lru.erase(E);
				break;
			}
		}
	}
}

bool MultiuserEditorScriptSync::has_buffer(const String &p_script_path) const {
	return !p_script_path.is_empty() && script_buffers.has(p_script_path);
}

void MultiuserEditorScriptSync::initialize_buffer_from_content(const String &p_script_path, const String &p_content) {
	String canonical;
	if (!MultiuserEditorActionInterceptor::canonicalize_res_path(p_script_path, canonical)) {
		const String msg = "Multiuser editor: initialize_buffer_from_content rejected unsafe script path: " + p_script_path;
		print_verbose(msg);
		_security.record(multiuser_editor::kEvtKindReplicationFailed, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatReplication, msg);
		return;
	}
	if (!script_buffers.has(canonical)) {
		_initialize_buffer_from_content(canonical, p_content);
	}
}

void MultiuserEditorScriptSync::poll_text_changes(Vector<Dictionary> &r_actions) {
	if (!active_code_edit || suppress_script || sync_pending || !script_buffers.has(active_script_path)) {
		return;
	}
	String next_text = active_code_edit->get_text();
	if (next_text == cached_text) {
		return;
	}

	int prefix = 0;
	while (prefix < cached_text.length() && prefix < next_text.length() && cached_text[prefix] == next_text[prefix]) {
		prefix++;
	}
	int old_suffix = cached_text.length() - 1;
	int new_suffix = next_text.length() - 1;
	while (old_suffix >= prefix && new_suffix >= prefix && cached_text[old_suffix] == next_text[new_suffix]) {
		old_suffix--;
		new_suffix--;
	}

	ScriptBufferState &state = script_buffers[active_script_path];
	for (int i = old_suffix; i >= prefix; i--) {
		Dictionary data;
		data["node_path"] = active_script_path;
		data["op"] = state.buffer.local_delete(prefix);
		_append_action(multiuser_editor::kActionCrdt, data, r_actions);
	}
	for (int i = prefix; i <= new_suffix; i++) {
		Dictionary data;
		data["node_path"] = active_script_path;
		data["op"] = state.buffer.local_insert(i, String::chr(next_text[i]));
		_append_action(multiuser_editor::kActionCrdt, data, r_actions);
	}
	cached_text = next_text;
}

Dictionary MultiuserEditorScriptSync::make_cursor_action() const {
	Dictionary action;
	if (!active_code_edit || active_script_path.is_empty()) {
		return action;
	}
	Dictionary data;
	data["line"] = active_code_edit->get_caret_line();
	data["column"] = active_code_edit->get_caret_column();
	action["type"] = multiuser_editor::kActionCursorUpdate;
	action["node_path"] = active_script_path;
	action["data"] = data;
	return action;
}

void MultiuserEditorScriptSync::apply_remote_crdt(const Dictionary &p_op, const String &p_script_path) {
	if (p_script_path.is_empty() || p_op.is_empty()) {
		return;
	}
	String canonical;
	if (!MultiuserEditorActionInterceptor::canonicalize_res_path(p_script_path, canonical)) {
		const String msg = "Multiuser editor: apply_remote_crdt rejected unsafe script path: " + p_script_path;
		print_verbose(msg);
		_security.record(multiuser_editor::kEvtKindReplicationFailed, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatCRDT, msg);
		return;
	}
	if (!script_buffers.has(canonical)) {
		_initialize_buffer_from_content(canonical, String());
	}
	_touch_lru(canonical);
	ScriptBufferState &state = script_buffers[canonical];
	String op = String(p_op.get("op", ""));
	int changed_index = -1;
	String changed_character;
	if (op == "insert") {
		changed_character = String(p_op.get("char", ""));
		changed_index = state.buffer.remote_insert(p_op);
	} else if (op == "delete") {
		changed_index = state.buffer.remote_delete(p_op);
	}
	if (active_code_edit && active_script_path == canonical) {
		String old_text = cached_text;
		String text = state.buffer.get_text();
		int caret_flat = multiuser_line_column_to_flat_index(old_text, active_code_edit->get_caret_line(), active_code_edit->get_caret_column());
		bool had_selection = active_code_edit->has_selection();
		int selection_from_flat = had_selection ? multiuser_line_column_to_flat_index(old_text, active_code_edit->get_selection_from_line(), active_code_edit->get_selection_from_column()) : 0;
		int selection_to_flat = had_selection ? multiuser_line_column_to_flat_index(old_text, active_code_edit->get_selection_to_line(), active_code_edit->get_selection_to_column()) : 0;
		int delta = 0;
		if (changed_index >= 0 && op == "insert") {
			delta = changed_character.length();
		} else if (changed_index >= 0 && op == "delete") {
			delta = -1;
		}

		suppress_script = true;
		if (changed_index >= 0 && op == "insert") {
			Point2i at = multiuser_flat_index_to_line_column(old_text, changed_index);
			active_code_edit->insert_text(changed_character, at.x, at.y);
		} else if (changed_index >= 0 && op == "delete") {
			Point2i from = multiuser_flat_index_to_line_column(old_text, changed_index);
			Point2i to = multiuser_flat_index_to_line_column(old_text, changed_index + 1);
			active_code_edit->remove_text(from.x, from.y, to.x, to.y);
		}
		if (active_code_edit->get_text() != text) {
			active_code_edit->set_text(text);
		}

		caret_flat = multiuser_adjust_flat_index_for_remote_edit(caret_flat, changed_index, delta);
		Point2i caret = multiuser_flat_index_to_line_column(text, caret_flat);
		active_code_edit->set_caret_line(CLAMP(caret.x, 0, MAX(0, active_code_edit->get_line_count() - 1)));
		active_code_edit->set_caret_column(caret.y);
		if (had_selection) {
			selection_from_flat = multiuser_adjust_flat_index_for_remote_edit(selection_from_flat, changed_index, delta);
			selection_to_flat = multiuser_adjust_flat_index_for_remote_edit(selection_to_flat, changed_index, delta);
			Point2i selection_from = multiuser_flat_index_to_line_column(text, selection_from_flat);
			Point2i selection_to = multiuser_flat_index_to_line_column(text, selection_to_flat);
			active_code_edit->select(selection_from.x, selection_from.y, selection_to.x, selection_to.y);
		} else {
			active_code_edit->deselect();
		}
		cached_text = text;
		suppress_script = false;
	}
}

bool MultiuserEditorScriptSync::import_buffer_state(const String &p_script_path, const Dictionary &p_state, bool p_clear_sync_pending) {
	String canonical;
	if (!MultiuserEditorActionInterceptor::canonicalize_res_path(p_script_path, canonical)) {
		const String msg = "Multiuser editor: import_buffer_state rejected unsafe script path: " + p_script_path;
		print_verbose(msg);
		_security.record(multiuser_editor::kEvtKindReplicationFailed, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatCRDT, msg);
		return false;
	}
	if (!script_buffers.has(canonical)) {
		ScriptBufferState state;
		state.buffer.init(local_peer_id);
		_apply_atoms_max_to(state.buffer);
		script_buffers[canonical] = state;
	}

	const bool imported_ok = script_buffers[canonical].buffer.import_state(p_state);
	_touch_lru(canonical);
	_evict_lru_if_needed();
	if (imported_ok && active_code_edit && active_script_path == canonical) {
		String text = script_buffers[canonical].buffer.get_text();
		suppress_script = true;
		int line = active_code_edit->get_caret_line();
		int column = active_code_edit->get_caret_column();
		active_code_edit->set_text(text);
		active_code_edit->set_caret_line(CLAMP(line, 0, MAX(0, active_code_edit->get_line_count() - 1)));
		active_code_edit->set_caret_column(column);
		cached_text = text;
		suppress_script = false;
	}

	if (imported_ok && p_clear_sync_pending) {
		sync_pending = false;
	} else if (!imported_ok) {
		const String msg = vformat("Multiuser editor: import_buffer_state refused for %s; sync_pending kept true to force re-sync.", canonical);
		print_verbose(msg);
		_security.record(multiuser_editor::kEvtKindCRDTRefused, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatCRDT, msg);
	}
	return imported_ok;
}

Dictionary MultiuserEditorScriptSync::export_buffer(const String &p_script_path) const {
	if (p_script_path.is_empty() || !script_buffers.has(p_script_path)) {
		return Dictionary();
	}
	return script_buffers[p_script_path].buffer.export_state();
}

Dictionary MultiuserEditorScriptSync::export_all_buffers() const {
	Dictionary buffers;
	for (const KeyValue<String, ScriptBufferState> &E : script_buffers) {
		buffers[E.key] = E.value.buffer.export_state();
	}
	return buffers;
}

String MultiuserEditorScriptSync::get_active_script_path() const {
	return active_script_path;
}

CodeEdit *MultiuserEditorScriptSync::get_active_code_edit() const {
	return active_code_edit;
}

#endif
