/**************************************************************************/
/*  multiuser_editor_ghost_cursor_overlay.cpp                             */
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

#include "multiuser_editor_ghost_cursor_overlay.h"

#include "multiuser_editor_action_interceptor.h"

#include "core/os/os.h"
#include "scene/gui/scroll_bar.h"
#include "scene/resources/font.h"

void MultiuserEditorGhostCursorOverlay::_bind_methods() {}

Color MultiuserEditorGhostCursorOverlay::_get_peer_color(const String &p_peer_id) const {
	return Color::from_hsv(float(p_peer_id.hash() % 360) / 360.0, 0.75, 0.95, 0.9);
}

void MultiuserEditorGhostCursorOverlay::_disconnect_code_edit() {
	if (!code_edit) {
		return;
	}
	if (code_edit->is_connected("draw", callable_mp(this, &MultiuserEditorGhostCursorOverlay::_code_edit_draw))) {
		code_edit->disconnect("draw", callable_mp(this, &MultiuserEditorGhostCursorOverlay::_code_edit_draw));
	}
	if (code_edit->get_v_scroll_bar() && code_edit->get_v_scroll_bar()->is_connected("value_changed", callable_mp(this, &MultiuserEditorGhostCursorOverlay::_scroll_changed))) {
		code_edit->get_v_scroll_bar()->disconnect("value_changed", callable_mp(this, &MultiuserEditorGhostCursorOverlay::_scroll_changed));
	}
	if (code_edit->get_h_scroll_bar() && code_edit->get_h_scroll_bar()->is_connected("value_changed", callable_mp(this, &MultiuserEditorGhostCursorOverlay::_scroll_changed))) {
		code_edit->get_h_scroll_bar()->disconnect("value_changed", callable_mp(this, &MultiuserEditorGhostCursorOverlay::_scroll_changed));
	}
}

void MultiuserEditorGhostCursorOverlay::attach_to(CodeEdit *p_code_edit, const String &p_script_path) {
	if (code_edit == p_code_edit && active_script_path == p_script_path) {
		return;
	}
	_disconnect_code_edit();
	code_edit = p_code_edit;
	active_script_path = p_script_path;
	if (!code_edit) {
		return;
	}
	if (!code_edit->is_connected("draw", callable_mp(this, &MultiuserEditorGhostCursorOverlay::_code_edit_draw))) {
		code_edit->connect("draw", callable_mp(this, &MultiuserEditorGhostCursorOverlay::_code_edit_draw));
	}
	if (code_edit->get_v_scroll_bar() && !code_edit->get_v_scroll_bar()->is_connected("value_changed", callable_mp(this, &MultiuserEditorGhostCursorOverlay::_scroll_changed))) {
		code_edit->get_v_scroll_bar()->connect("value_changed", callable_mp(this, &MultiuserEditorGhostCursorOverlay::_scroll_changed));
	}
	if (code_edit->get_h_scroll_bar() && !code_edit->get_h_scroll_bar()->is_connected("value_changed", callable_mp(this, &MultiuserEditorGhostCursorOverlay::_scroll_changed))) {
		code_edit->get_h_scroll_bar()->connect("value_changed", callable_mp(this, &MultiuserEditorGhostCursorOverlay::_scroll_changed));
	}
	code_edit->queue_redraw();
}

void MultiuserEditorGhostCursorOverlay::detach() {
	_disconnect_code_edit();
	code_edit = nullptr;
	active_script_path = String();
}

void MultiuserEditorGhostCursorOverlay::update_peer_cursor(const String &p_peer_id, const Dictionary &p_data) {
	if (p_peer_id.is_empty()) {
		return;
	}
	PeerCursor cursor;
	const String raw_path = String(p_data.get("script", p_data.get("script_path", p_data.get("node_path", ""))));

	String canonical;
	if (!raw_path.is_empty() && raw_path.begins_with("res://")) {
		if (!MultiuserEditorActionInterceptor::canonicalize_res_path(raw_path, canonical)) {
			return;
		}
	} else {
		canonical = raw_path;
	}
	cursor.script_path = canonical;
	cursor.line = int(p_data.get("line", 0));
	cursor.column = int(p_data.get("column", 0));
	cursor.timestamp = OS::get_singleton()->get_ticks_msec() / 1000.0;

	if (!peer_cursors.has(p_peer_id) && int(peer_cursors.size()) >= _max_peers) {
		_evict_oldest_peer_cursor();
	}
	peer_cursors[p_peer_id] = cursor;
	if (code_edit) {
		code_edit->queue_redraw();
	}
}

void MultiuserEditorGhostCursorOverlay::_evict_oldest_peer_cursor() {
	String oldest_key;
	double oldest_ts = 0.0;
	bool first = true;
	for (const KeyValue<String, PeerCursor> &E : peer_cursors) {
		if (first || E.value.timestamp < oldest_ts) {
			oldest_ts = E.value.timestamp;
			oldest_key = E.key;
			first = false;
		}
	}
	if (!oldest_key.is_empty()) {
		peer_cursors.erase(oldest_key);
	}
}

void MultiuserEditorGhostCursorOverlay::remove_peer(const String &p_peer_id) {
	peer_cursors.erase(p_peer_id);
	if (code_edit) {
		code_edit->queue_redraw();
	}
}

void MultiuserEditorGhostCursorOverlay::_code_edit_draw() {
	if (!code_edit || active_script_path.is_empty()) {
		return;
	}
	double now = OS::get_singleton()->get_ticks_msec() / 1000.0;
	Vector<String> stale_peers;
	for (const KeyValue<String, PeerCursor> &E : peer_cursors) {
		const PeerCursor &cursor = E.value;
		if (now - cursor.timestamp > 10.0) {
			stale_peers.push_back(E.key);
			continue;
		}
		if (cursor.script_path != active_script_path) {
			continue;
		}
		int line = CLAMP(cursor.line, 0, MAX(0, code_edit->get_line_count() - 1));

		const int line_len = code_edit->get_line(line).length();
		int column = CLAMP(cursor.column, 0, line_len);
		if (line < code_edit->get_first_visible_line()) {
			continue;
		}
		Rect2i rect = code_edit->get_rect_at_line_column(line, column);
		if (rect.size.y <= 0) {
			continue;
		}
		Color color = _get_peer_color(E.key);
		Point2 cursor_top = rect.position;
		Point2 cursor_bottom = Point2(rect.position.x, rect.position.y + rect.size.y);
		code_edit->draw_line(cursor_top, cursor_bottom, color, 2.0);
		code_edit->draw_string(code_edit->get_theme_font(SNAME("font")), cursor_top + Point2(4.0f, rect.size.y), E.key, HORIZONTAL_ALIGNMENT_LEFT, -1, 12, color);
	}
	for (const String &peer_id : stale_peers) {
		peer_cursors.erase(peer_id);
	}
}

void MultiuserEditorGhostCursorOverlay::_scroll_changed(double p_value) {
	if (code_edit) {
		code_edit->queue_redraw();
	}
}

void MultiuserEditorGhostCursorOverlay::_notification(int p_what) {
	if (p_what == NOTIFICATION_PREDELETE) {
		detach();
	}
}

#endif
