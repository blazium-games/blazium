/**************************************************************************/
/*  portal_host.h                                                         */
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

#include "portal_runtime.h"

#include "core/io/json.h"
#include "core/object/callable_method_pointer.h"
#include "core/object/class_db.h"

// Script singletons must inherit Object directly. Shared host calls live in
// this helper, which is not itself a script class.

#define PORTAL_HOST_HANDLERS(m_class)           \
	friend class PortalApi<m_class>;            \
	void _portal_paused() {                     \
		emit_signal(SNAME("paused"));           \
	}                                           \
	void _portal_resumed() {                    \
		emit_signal(SNAME("resumed"));          \
	}                                           \
	void _portal_result(const String &p_json) { \
		emit_signal(SNAME("result"), p_json);   \
	}

#define PORTAL_HOST_PUBLIC(m_class)                  \
	bool is_on_host() const {                        \
		return PortalApi<m_class>::is_on_host();     \
	}                                                \
	String get_last_error() const {                  \
		return PortalApi<m_class>::get_last_error(); \
	}

template <class T>
class PortalApi {
	mutable String last_error;
	bool lifecycle_bound = false;
	Variant pause_callback;
	Variant resume_callback;
	Variant result_callback;

	T *self() { return static_cast<T *>(this); }
	const T *self() const { return static_cast<const T *>(this); }

	void fail() {
		last_error = "Not on this host";
		self()->emit_signal(SNAME("unavailable"), last_error);
	}

	void bind_lifecycle() {
		if (lifecycle_bound) {
			return;
		}
		lifecycle_bound = true;
		pause_callback = PortalRuntime::make_callback(callable_mp(self(), &T::_portal_paused));
		resume_callback = PortalRuntime::make_callback(callable_mp(self(), &T::_portal_resumed));
		result_callback = PortalRuntime::make_callback(callable_mp(self(), &T::_portal_result));
		PortalRuntime::bind(js_interface, "paused", pause_callback);
		PortalRuntime::bind(js_interface, "resumed", resume_callback);
		PortalRuntime::bind(js_interface, "result", result_callback);
		self()->bind_host_extra();
	}

protected:
	String host_id;
	String js_interface = "BlaziumPortals";

	String host_call(const String &p_method, const Dictionary &p_args = Dictionary()) {
		if (!is_on_host()) {
			fail();
			return String();
		}
		last_error = String();
		bind_lifecycle();
		Variant result = PortalRuntime::invoke(js_interface, host_id, p_method, JSON::stringify(p_args));
		if (result.get_type() == Variant::NIL) {
			return String();
		}
		return String(result);
	}

	Dictionary host_dictionary(const String &p_json) const {
		Variant parsed = JSON::parse_string(p_json);
		if (parsed.get_type() == Variant::DICTIONARY) {
			return parsed;
		}
		return Dictionary();
	}

public:
	void bind_host_extra() {}

	bool is_on_host() const {
		return PortalRuntime::is_available(js_interface, host_id);
	}

	String get_last_error() const { return last_error; }

	static void bind_common() {
		ClassDB::bind_method(D_METHOD("is_on_host"), &T::is_on_host);
		ClassDB::bind_method(D_METHOD("get_last_error"), &T::get_last_error);
		ClassDB::add_signal(T::get_class_static(), MethodInfo("paused"));
		ClassDB::add_signal(T::get_class_static(), MethodInfo("resumed"));
		ClassDB::add_signal(T::get_class_static(), MethodInfo("unavailable", PropertyInfo(Variant::STRING, "message")));
		ClassDB::add_signal(T::get_class_static(), MethodInfo("result", PropertyInfo(Variant::STRING, "json")));
	}
};
