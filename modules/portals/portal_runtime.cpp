/**************************************************************************/
/*  portal_runtime.cpp                                                    */
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

#include "portal_runtime.h"

#ifdef WEB_ENABLED
#include "platform/web/api/javascript_bridge_singleton.h"
#endif

bool PortalRuntime::is_available(const String &p_interface, const String &p_host) {
#ifdef WEB_ENABLED
	JavaScriptBridge *bridge = JavaScriptBridge::get_singleton();
	if (bridge == nullptr || p_interface.is_empty() || p_host.is_empty()) {
		return false;
	}
	Ref<JavaScriptObject> host = bridge->get_interface(p_interface);
	if (host.is_null()) {
		return false;
	}
	Variant available = host->call(SNAME("available"), p_host);
	return available.operator bool();
#else
	(void)p_interface;
	(void)p_host;
	return false;
#endif
}

Variant PortalRuntime::invoke(const String &p_interface, const String &p_host, const String &p_method, const String &p_json_args) {
#ifdef WEB_ENABLED
	JavaScriptBridge *bridge = JavaScriptBridge::get_singleton();
	if (bridge == nullptr) {
		return Variant();
	}
	Ref<JavaScriptObject> host = bridge->get_interface(p_interface);
	if (host.is_null()) {
		return Variant();
	}
	return host->call(SNAME("call"), p_host, p_method, p_json_args);
#else
	(void)p_interface;
	(void)p_host;
	(void)p_method;
	(void)p_json_args;
	return Variant();
#endif
}

Variant PortalRuntime::make_callback(const Callable &p_callable) {
#ifdef WEB_ENABLED
	JavaScriptBridge *bridge = JavaScriptBridge::get_singleton();
	if (bridge == nullptr || !p_callable.is_valid()) {
		return Variant();
	}
	Ref<JavaScriptObject> callback = bridge->create_callback(p_callable);
	return callback;
#else
	(void)p_callable;
	return Variant();
#endif
}

void PortalRuntime::bind(const String &p_interface, const String &p_slot, const Variant &p_callback) {
#ifdef WEB_ENABLED
	JavaScriptBridge *bridge = JavaScriptBridge::get_singleton();
	if (bridge == nullptr || p_callback.get_type() == Variant::NIL) {
		return;
	}
	Ref<JavaScriptObject> host = bridge->get_interface(p_interface);
	if (host.is_null()) {
		return;
	}
	host->call(SNAME("bind"), p_slot, p_callback);
#else
	(void)p_interface;
	(void)p_slot;
	(void)p_callback;
#endif
}
