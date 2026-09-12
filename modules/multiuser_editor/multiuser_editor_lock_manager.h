/**************************************************************************/
/*  multiuser_editor_lock_manager.h                                       */
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

#include "multiuser_editor_constants.h"
#include "multiuser_editor_security_sink.h"

#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/templates/vector.h"
#include "core/variant/variant.h"
#include "scene/main/node.h"

class MultiuserEditorLockManager {
public:
	struct RemoteLock {
		String peer_id;
		uint64_t last_touched_msec = 0;
	};

	struct EvictedLock {
		String peer_id;
		String path;
		uint64_t last_touched_msec = 0;
	};

private:
	HashMap<String, HashSet<String>> peer_locks;
	HashMap<String, RemoteLock> path_locks;
	HashMap<String, Vector<String>> peer_selection;
	HashMap<String, String> decorated_names;
	int _max_locks_per_peer = 256;

	int _max_total_locks = 4096;

	uint64_t _lock_ttl_msec = (uint64_t)multiuser_editor::kLockManagerDefaultLockTTLMsec;

	multiuser_editor::SecuritySink _security;

	String _clean_path(const String &p_path) const;
	Node *_resolve_node(Node *p_root, const String &p_path) const;

public:
	void update_peer_selection(const String &p_peer_id, const Array &p_paths);
	void add_peer_lock(const String &p_peer_id, const String &p_path);
	void touch_peer_lock(const String &p_peer_id, const String &p_path);
	void release_peer(const String &p_peer_id);
	void clear();
	bool is_locked(const String &p_path) const;
	String get_lock_owner(const String &p_path) const;
	Vector<String> get_peer_selection(const String &p_peer_id) const;
	Vector<EvictedLock> check_timeouts(double p_now_sec);
	void refresh_overlay(Node *p_root);
	const HashMap<String, HashSet<String>> &get_peer_locks() const { return peer_locks; }
	const HashMap<String, RemoteLock> &get_path_locks() const { return path_locks; }
	void set_max_locks_per_peer(int p_max) { _max_locks_per_peer = MAX(1, p_max); }
	int get_max_locks_per_peer() const { return _max_locks_per_peer; }
	void set_max_total_locks(int p_max) { _max_total_locks = MAX(1, p_max); }
	int get_max_total_locks() const { return _max_total_locks; }
	int get_total_lock_count() const { return path_locks.size(); }
	void set_lock_ttl_msec(uint64_t p_ttl) { _lock_ttl_msec = p_ttl; }
	uint64_t get_lock_ttl_msec() const { return _lock_ttl_msec; }
	void set_security_sink(const multiuser_editor::SecuritySink &p_sink) { _security = p_sink; }

	String clean_path(const String &p_path) const;
};

#endif
