/**************************************************************************/
/*  justamcp_scene_path_lock.cpp                                          */
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

#include "justamcp_scene_path_lock.h"

#include "../justamcp_editor_scene_access.h"
#include "../justamcp_tool_result.h"
#include "core/os/os.h"
#include "core/templates/hash_map.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "scene/main/node.h"

static HashMap<String, Mutex *> _scene_path_mutex_pool;
static Mutex _scene_path_mutex_pool_guard;
static Vector<String> _scene_path_mutex_lru;
static const int SCENE_MUTEX_POOL_MAX = 64;

static void _touch_scene_path_mutex_lru(const String &p_scene_path) {
	for (int i = 0; i < _scene_path_mutex_lru.size(); i++) {
		if (_scene_path_mutex_lru[i] == p_scene_path) {
			_scene_path_mutex_lru.remove_at(i);
			break;
		}
	}
	_scene_path_mutex_lru.push_back(p_scene_path);
}

static void _evict_scene_path_mutex_if_needed() {
	while (_scene_path_mutex_pool.size() >= SCENE_MUTEX_POOL_MAX && !_scene_path_mutex_lru.is_empty()) {
		bool evicted = false;
		for (int i = 0; i < _scene_path_mutex_lru.size(); i++) {
			const String evict_path = _scene_path_mutex_lru[i];
			if (!_scene_path_mutex_pool.has(evict_path)) {
				_scene_path_mutex_lru.remove_at(i);
				evicted = true;
				break;
			}
			Mutex *evict_mutex = _scene_path_mutex_pool[evict_path];
			if (evict_mutex && evict_mutex->try_lock()) {
				evict_mutex->unlock();
				memdelete(evict_mutex);
				_scene_path_mutex_pool.erase(evict_path);
				_scene_path_mutex_lru.remove_at(i);
				evicted = true;
				break;
			}
		}
		if (!evicted) {
			break;
		}
	}
}

static Mutex *_get_scene_path_mutex(const String &p_scene_path) {
	if (p_scene_path.is_empty()) {
		return nullptr;
	}
	MutexLock lock(_scene_path_mutex_pool_guard);
	Mutex **existing = _scene_path_mutex_pool.getptr(p_scene_path);
	if (existing) {
		_touch_scene_path_mutex_lru(p_scene_path);
		return *existing;
	}
	_evict_scene_path_mutex_if_needed();
	Mutex *created = memnew(Mutex);
	_scene_path_mutex_pool[p_scene_path] = created;
	_touch_scene_path_mutex_lru(p_scene_path);
	return created;
}

static bool _try_lock_mutex_with_timeout(Mutex *p_mutex, int p_timeout_ms = 0) {
	if (!p_mutex) {
		return true;
	}
	if (p_mutex->try_lock()) {
		return true;
	}
	if (p_timeout_ms <= 0) {
		return false;
	}
	const uint64_t deadline = OS::get_singleton()->get_ticks_msec() + (uint64_t)p_timeout_ms;
	while (!p_mutex->try_lock()) {
		if (OS::get_singleton()->get_ticks_msec() >= deadline) {
			return false;
		}
		OS::get_singleton()->delay_usec(1000);
	}
	return true;
}

JustAMCPScenePathLock::JustAMCPScenePathLock(const String &p_scene_path) {
	mutex = _get_scene_path_mutex(p_scene_path);
	locked = _try_lock_mutex_with_timeout(mutex);
	if (!locked && mutex) {
		WARN_PRINT(vformat("JustAMCP scene lock timeout for %s", p_scene_path));
	}
}

JustAMCPScenePathLock::~JustAMCPScenePathLock() {
	if (locked && mutex) {
		mutex->unlock();
	}
}

Dictionary justamcp_scene_lock_busy_response() {
	return JustAMCPToolResult::busy("Scene busy; retry after concurrent operation completes", 500);
}

bool justamcp_is_active_scene(const String &p_scene_path) {
	if (!EditorNode::get_singleton() || !EditorInterface::get_singleton()) {
		return false;
	}
	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	return root && root->get_scene_file_path() == p_scene_path;
}

#endif
