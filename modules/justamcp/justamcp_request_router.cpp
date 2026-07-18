/**************************************************************************/
/*  justamcp_request_router.cpp                                           */
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

#include "justamcp_request_router.h"

#include "tools/justamcp_json_rpc_helpers.h"

#include "core/math/math_funcs.h"
#include "core/os/time.h"

String JustAMCPRequestRouter::request_id_key(const Variant &p_request_id) {
	if (p_request_id.get_type() == Variant::NIL) {
		return String();
	}
	if (p_request_id.get_type() == Variant::STRING) {
		return "s:" + String(p_request_id);
	}
	if (p_request_id.get_type() == Variant::INT) {
		return "i:" + itos(p_request_id);
	}
	if (p_request_id.get_type() == Variant::FLOAT) {
		const double d = p_request_id;
		if (Math::is_equal_approx(d, Math::round(d))) {
			return "i:" + itos((int64_t)Math::round(d));
		}
		return "f:" + String(p_request_id);
	}
	return "v:" + String(p_request_id);
}

bool JustAMCPRequestRouter::request_ids_equal(const Variant &p_a, const Variant &p_b) {
	return JustAMCPJsonRpcHelpers::request_ids_equal(p_a, p_b);
}

void JustAMCPRequestRouter::bind(const Variant &p_request_id, const String &p_session_id, int p_connection_id) {
	const String key = request_id_key(p_request_id);
	if (key.is_empty() || p_session_id.is_empty()) {
		return;
	}
	MutexLock lock(mutex);
	RouteEntry entry;
	entry.session_id = p_session_id;
	entry.connection_id = p_connection_id;
	entry.created_usec = Time::get_singleton()->get_ticks_usec();
	routes[key] = entry;
}

bool JustAMCPRequestRouter::lookup(const Variant &p_request_id, String &r_session_id, int &r_connection_id) const {
	const String key = request_id_key(p_request_id);
	if (key.is_empty()) {
		return false;
	}
	MutexLock lock(mutex);
	if (!routes.has(key)) {
		return false;
	}
	r_session_id = routes[key].session_id;
	r_connection_id = routes[key].connection_id;
	return true;
}

void JustAMCPRequestRouter::clear(const Variant &p_request_id) {
	const String key = request_id_key(p_request_id);
	if (key.is_empty()) {
		return;
	}
	MutexLock lock(mutex);
	routes.erase(key);
}

void JustAMCPRequestRouter::clear_for_connection(int p_connection_id) {
	MutexLock lock(mutex);
	Vector<String> to_erase;
	for (const KeyValue<String, RouteEntry> &kv : routes) {
		if (kv.value.connection_id == p_connection_id) {
			to_erase.push_back(kv.key);
		}
	}
	for (int i = 0; i < to_erase.size(); i++) {
		routes.erase(to_erase[i]);
	}
}

void JustAMCPRequestRouter::prune_expired(uint64_t p_ttl_usec) {
	if (p_ttl_usec == 0) {
		return;
	}
	const uint64_t now = Time::get_singleton()->get_ticks_usec();
	MutexLock lock(mutex);
	Vector<String> to_erase;
	for (const KeyValue<String, RouteEntry> &kv : routes) {
		if (kv.value.created_usec > 0 && now - kv.value.created_usec > p_ttl_usec) {
			to_erase.push_back(kv.key);
		}
	}
	for (int i = 0; i < to_erase.size(); i++) {
		routes.erase(to_erase[i]);
	}
}

void JustAMCPRequestRouter::clear_all() {
	MutexLock lock(mutex);
	routes.clear();
}

#ifdef TESTS_ENABLED
void JustAMCPRequestRouter::test_backdate_route(const Variant &p_request_id, uint64_t p_age_usec) {
	const String key = request_id_key(p_request_id);
	if (key.is_empty()) {
		return;
	}
	MutexLock lock(mutex);
	if (!routes.has(key)) {
		return;
	}
	const uint64_t now = Time::get_singleton()->get_ticks_usec();
	routes[key].created_usec = now > p_age_usec ? now - p_age_usec : 1;
}

void JustAMCPRequestRouter::test_set_route_created_usec(const Variant &p_request_id, uint64_t p_created_usec) {
	const String key = request_id_key(p_request_id);
	if (key.is_empty()) {
		return;
	}
	MutexLock lock(mutex);
	if (!routes.has(key)) {
		return;
	}
	routes[key].created_usec = p_created_usec;
}
#endif
