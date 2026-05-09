/**************************************************************************/
/*  multiuser_editor_lock_manager.cpp                                     */
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

#include "multiuser_editor_lock_manager.h"
#include "multiuser_editor_action_interceptor.h"

#include "core/os/os.h"

String MultiuserEditorLockManager::_clean_path(const String &p_path) const {
	PackedStringArray parts = p_path.split("/");
	for (int i = 0; i < parts.size(); i++) {
		String part = parts[i];
		int bracket_pos = part.rfind(" [locked");
		if (bracket_pos != -1) {
			parts.set(i, part.substr(0, bracket_pos));
		}
	}
	return String("/").join(parts);
}

Node *MultiuserEditorLockManager::_resolve_node(Node *p_root, const String &p_path) const {
	if (!p_root || p_path.is_empty()) {
		return nullptr;
	}
	String clean = _clean_path(p_path);
	if (!MultiuserEditorActionInterceptor::is_safe_node_path(clean)) {
		const String msg = "Multiuser editor (LockManager): Dropped unsafe lock target: " + clean;
		print_verbose(msg);
		_security.record(multiuser_editor::kEvtKindPermissionDenied, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatPermissions, msg);
		return nullptr;
	}
	if (clean == "." || clean == String(p_root->get_name())) {
		return p_root;
	}
	return p_root->has_node(NodePath(clean)) ? p_root->get_node(NodePath(clean)) : nullptr;
}

void MultiuserEditorLockManager::update_peer_selection(const String &p_peer_id, const Array &p_paths) {
	if (p_peer_id.is_empty()) {
		return;
	}
	Vector<String> sel;
	const int hard_cap = _max_locks_per_peer;
	for (int i = 0; i < p_paths.size() && sel.size() < hard_cap; i++) {
		String raw = String(p_paths[i]);
		if (raw.is_empty()) {
			continue;
		}
		String path = _clean_path(raw);
		if (!MultiuserEditorActionInterceptor::is_safe_node_path(path)) {
			continue;
		}
		if (sel.find(path) == -1) {
			sel.push_back(path);
		}
	}
	peer_selection[p_peer_id] = sel;
	for (const String &path : sel) {
		HashMap<String, RemoteLock>::Iterator it = path_locks.find(path);
		if (it != path_locks.end() && it->value.peer_id == p_peer_id) {
			touch_peer_lock(p_peer_id, path);
		}
	}
}

void MultiuserEditorLockManager::add_peer_lock(const String &p_peer_id, const String &p_path) {
	if (p_peer_id.is_empty() || p_path.is_empty()) {
		return;
	}
	String path = _clean_path(p_path);
	if (!MultiuserEditorActionInterceptor::is_safe_node_path(path)) {
		const String msg = "Multiuser editor (LockManager): rejected unsafe lock path from peer " + p_peer_id + ": " + p_path;
		print_verbose(msg);
		_security.record(multiuser_editor::kEvtKindPermissionDenied, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatPermissions, msg);
		return;
	}
	HashMap<String, HashSet<String>>::Iterator pit = peer_locks.find(p_peer_id);
	if (pit == peer_locks.end()) {
		pit = peer_locks.insert(p_peer_id, HashSet<String>());
	}
	HashSet<String> &locks_for_peer = pit->value;
	if (!locks_for_peer.has(path)) {
		if (int(locks_for_peer.size()) >= _max_locks_per_peer) {
			const String msg = vformat("Multiuser editor (LockManager): peer %s exceeded lock cap %d; dropping new lock for %s", p_peer_id, _max_locks_per_peer, path);
			print_verbose(msg);
			_security.record(multiuser_editor::kEvtKindRateLimited, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatPermissions, msg);
			return;
		}

		if (!path_locks.has(path) && int(path_locks.size()) >= _max_total_locks) {
			const String msg = vformat("Multiuser editor (LockManager): GLOBAL lock cap %d reached; dropping new lock from peer %s for %s", _max_total_locks, p_peer_id, path);
			print_verbose(msg);
			_security.record(multiuser_editor::kEvtKindRateLimited, multiuser_editor::kEvtLogWarn, multiuser_editor::kEvtCatPermissions, msg);
			return;
		}
		locks_for_peer.insert(path);
	}
	RemoteLock lock;
	lock.peer_id = p_peer_id;
	lock.last_touched_msec = OS::get_singleton()->get_ticks_msec();
	path_locks[path] = lock;
}

void MultiuserEditorLockManager::touch_peer_lock(const String &p_peer_id, const String &p_path) {
	if (p_peer_id.is_empty() || p_path.is_empty()) {
		return;
	}
	String path = _clean_path(p_path);
	HashMap<String, RemoteLock>::Iterator it = path_locks.find(path);
	if (it == path_locks.end()) {
		return;
	}
	if (it->value.peer_id != p_peer_id) {
		return;
	}
	it->value.last_touched_msec = OS::get_singleton()->get_ticks_msec();
}

void MultiuserEditorLockManager::release_peer(const String &p_peer_id) {
	peer_selection.erase(p_peer_id);
	HashMap<String, HashSet<String>>::Iterator pit = peer_locks.find(p_peer_id);
	if (pit == peer_locks.end()) {
		return;
	}
	for (const String &path : pit->value) {
		path_locks.erase(path);
	}
	peer_locks.remove(pit);
}

void MultiuserEditorLockManager::clear() {
	peer_locks.clear();
	path_locks.clear();
	peer_selection.clear();
	decorated_names.clear();
}

bool MultiuserEditorLockManager::is_locked(const String &p_path) const {
	return path_locks.has(_clean_path(p_path));
}

String MultiuserEditorLockManager::get_lock_owner(const String &p_path) const {
	String clean = _clean_path(p_path);
	HashMap<String, RemoteLock>::ConstIterator it = path_locks.find(clean);
	return it != path_locks.end() ? it->value.peer_id : String();
}

Vector<String> MultiuserEditorLockManager::get_peer_selection(const String &p_peer_id) const {
	HashMap<String, Vector<String>>::ConstIterator sit = peer_selection.find(p_peer_id);
	if (sit != peer_selection.end()) {
		return sit->value;
	}
	Vector<String> out;
	HashMap<String, HashSet<String>>::ConstIterator lit = peer_locks.find(p_peer_id);
	if (lit != peer_locks.end()) {
		for (const String &p : lit->value) {
			out.push_back(p);
		}
	}
	return out;
}

Vector<MultiuserEditorLockManager::EvictedLock> MultiuserEditorLockManager::check_timeouts(double p_now_sec) {
	Vector<EvictedLock> evicted;
	if (_lock_ttl_msec == 0 || path_locks.is_empty()) {
		return evicted;
	}
	const uint64_t now_msec = (uint64_t)(p_now_sec * 1000.0);
	Vector<String> to_remove;
	for (const KeyValue<String, RemoteLock> &E : path_locks) {
		if (now_msec > E.value.last_touched_msec && (now_msec - E.value.last_touched_msec) > _lock_ttl_msec) {
			to_remove.push_back(E.key);
		}
	}
	for (const String &path : to_remove) {
		HashMap<String, RemoteLock>::Iterator lit = path_locks.find(path);
		if (lit == path_locks.end()) {
			continue;
		}
		EvictedLock ev;
		ev.peer_id = lit->value.peer_id;
		ev.path = path;
		ev.last_touched_msec = lit->value.last_touched_msec;
		evicted.push_back(ev);
		HashMap<String, HashSet<String>>::Iterator pit = peer_locks.find(ev.peer_id);
		if (pit != peer_locks.end()) {
			pit->value.erase(path);
			if (pit->value.is_empty()) {
				peer_locks.remove(pit);
			}
		}
		path_locks.remove(lit);
	}
	return evicted;
}

void MultiuserEditorLockManager::refresh_overlay(Node *p_root) {
	if (!p_root) {
		return;
	}

	if (path_locks.is_empty() && decorated_names.is_empty()) {
		return;
	}
	Vector<String> restore;
	for (const KeyValue<String, String> &E : decorated_names) {
		if (!path_locks.has(E.key)) {
			restore.push_back(E.key);
		}
	}
	for (const String &path : restore) {
		Node *node = _resolve_node(p_root, path);
		if (node) {
			node->set_name(decorated_names[path]);
		}
		decorated_names.erase(path);
	}
	for (const KeyValue<String, RemoteLock> &E : path_locks) {
		Node *node = _resolve_node(p_root, E.key);
		if (!node) {
			continue;
		}
		String name = node->get_name();
		String peer_id = E.value.peer_id;
		String decoration = vformat(" [locked %s]", peer_id);

		if (decorated_names.has(E.key)) {
			if (!name.contains(" [locked")) {
				String base_name = decorated_names[E.key];
				node->set_name(base_name + decoration);
			} else if (!name.ends_with(decoration)) {
				int bracket_pos = name.rfind(" [locked");
				if (bracket_pos != -1) {
					node->set_name(name.substr(0, bracket_pos) + decoration);
				}
			}
			continue;
		}

		decorated_names[E.key] = name;
		if (!name.contains(" [locked")) {
			node->set_name(name + decoration);
		}
	}
}

String MultiuserEditorLockManager::clean_path(const String &p_path) const {
	return _clean_path(p_path);
}

#endif
