/**************************************************************************/
/*  multiuser_editor_script_sync.h                                        */
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

#include "core/templates/hash_map.h"
#include "core/templates/list.h"
#include "core/variant/variant.h"
#include "multiuser_editor_crdt_text_buffer.h"
#include "multiuser_editor_security_sink.h"
#include "scene/gui/code_edit.h"

class MultiuserEditorScriptSync {
	struct ScriptBufferState {
		MultiuserEditorCRDTTextBuffer buffer;
	};

	String local_peer_id;
	CodeEdit *active_code_edit = nullptr;
	String active_script_path;
	String cached_text;
	HashMap<String, ScriptBufferState> script_buffers;
	List<String> _buffer_lru;
	int _max_tracked_buffers = 256;
	int _script_attach_max_bytes = 4 * 1024 * 1024;
	int _crdt_atoms_max_per_buffer = 1000000;
	bool suppress_script = false;
	bool sync_pending = false;
	double sync_pending_since = 0.0;

	multiuser_editor::SecuritySink _security;

	void _touch_lru(const String &p_script_path);
	void _evict_lru_if_needed();
	void _apply_atoms_max_to(MultiuserEditorCRDTTextBuffer &p_buffer) const;

	void _initialize_buffer_from_content(const String &p_script_path, const String &p_content);
	void _append_action(const String &p_type, const Dictionary &p_data, Vector<Dictionary> &r_actions) const;

public:
	void set_local_peer_id(const String &p_peer_id);
	void set_sync_pending(bool p_pending);

	bool is_sync_pending() const { return sync_pending; }
	bool is_sync_pending_expired(double p_now, double p_timeout_sec) const;
	void attach_code_edit(CodeEdit *p_code_edit, const String &p_script_path, Vector<Dictionary> &r_actions);
	void detach_code_edit();
	void clear_all_buffers();
	void remove_buffer(const String &p_script_path);
	bool has_buffer(const String &p_script_path) const;
	void initialize_buffer_from_content(const String &p_script_path, const String &p_content);
	void poll_text_changes(Vector<Dictionary> &r_actions);
	Dictionary make_cursor_action() const;
	void apply_remote_crdt(const Dictionary &p_op, const String &p_script_path);

	bool import_buffer_state(const String &p_script_path, const Dictionary &p_state, bool p_clear_sync_pending = true);
	Dictionary export_buffer(const String &p_script_path) const;
	Dictionary export_all_buffers() const;
	String get_active_script_path() const;
	CodeEdit *get_active_code_edit() const;

	void set_max_tracked_buffers(int p_max) { _max_tracked_buffers = MAX(8, p_max); }
	int get_max_tracked_buffers() const { return _max_tracked_buffers; }
	int get_tracked_buffer_count() const { return script_buffers.size(); }

	void set_script_attach_max_bytes(int p_max) { _script_attach_max_bytes = MAX(1024, p_max); }
	int get_script_attach_max_bytes() const { return _script_attach_max_bytes; }

	void set_crdt_atoms_max_per_buffer(int p_max);
	int get_crdt_atoms_max_per_buffer() const { return _crdt_atoms_max_per_buffer; }

	void set_security_sink(const multiuser_editor::SecuritySink &p_sink) { _security = p_sink; }
};

#endif
