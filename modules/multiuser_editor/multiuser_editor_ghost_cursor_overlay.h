/**************************************************************************/
/*  multiuser_editor_ghost_cursor_overlay.h                               */
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
#include "core/variant/variant.h"
#include "scene/gui/code_edit.h"

class MultiuserEditorGhostCursorOverlay : public Control {
	GDCLASS(MultiuserEditorGhostCursorOverlay, Control);

	struct PeerCursor {
		String script_path;
		int line = 0;
		int column = 0;
		double timestamp = 0.0;
	};

	HashMap<String, PeerCursor> peer_cursors;
	CodeEdit *code_edit = nullptr;
	String active_script_path;

	int _max_peers = 64;

	Color _get_peer_color(const String &p_peer_id) const;
	void _code_edit_draw();
	void _scroll_changed(double p_value);
	void _disconnect_code_edit();

	void _evict_oldest_peer_cursor();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void attach_to(CodeEdit *p_code_edit, const String &p_script_path);
	void detach();
	void update_peer_cursor(const String &p_peer_id, const Dictionary &p_data);
	void remove_peer(const String &p_peer_id);

	void set_max_peers(int p_max) { _max_peers = MAX(1, p_max); }
	int get_max_peers() const { return _max_peers; }
	int get_peer_cursor_count() const { return peer_cursors.size(); }
};

#endif
