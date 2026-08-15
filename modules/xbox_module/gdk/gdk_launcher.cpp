/**************************************************************************/
/*  gdk_launcher.cpp                                                      */
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

#include "core/object/class_db.h"
#include "gdk_launcher.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "gdk_windows.h"

#include "gdk.h"
#include "gdk_result.h"
#include "gdk_runtime.h"
#include "gdk_user.h"

void GDKLauncher::_bind_methods() {
	ClassDB::bind_method(D_METHOD("launch_uri", "uri", "user"), &GDKLauncher::launch_uri, DEFVAL(Ref<GDKUser>()));
}

#ifdef XBOX_MODULE_GDK_ENABLED
void GDKLauncher::set_owner(GDK *p_owner) {
	m_owner = p_owner;
}

Ref<GDKResult> GDKLauncher::on_runtime_initialized() {
	GDKRuntime *runtime = m_owner != nullptr ? m_owner->get_runtime() : nullptr;
	if (runtime == nullptr || !runtime->is_initialized()) {
		return GDKResult::error_result(
				E_FAIL,
				"runtime_not_initialized",
				"Cannot initialize the launcher service before the GDK runtime.");
	}

	m_runtime_ready = true;
	return GDKResult::ok_result();
}

void GDKLauncher::shutdown() {
	m_runtime_ready = false;
}

Ref<GDKResult> GDKLauncher::launch_uri(const String &p_uri, const Ref<GDKUser> &p_user) {
	if (!m_runtime_ready) {
		return GDKResult::error_result(
				E_FAIL,
				"not_initialized",
				"GDK is not initialized. Call GDK.initialize() first.");
	}

	const String uri = p_uri.strip_edges();
	String scheme;
	if (!try_parse_uri_scheme_internal(uri, &scheme)) {
		return GDKResult::error_result(
				E_INVALIDARG,
				"invalid_uri",
				"A non-empty absolute URI with a valid scheme is required.");
	}

	if (is_disallowed_scheme_internal(scheme)) {
		Dictionary data;
		data["uri"] = uri;
		data["scheme"] = scheme;
		return GDKResult::error_result(
				E_NOTIMPL,
				"unsupported_launcher_destination",
				"This URI destination is not supported by GDK.launcher on PC.",
				data);
	}

	if (!is_supported_scheme_internal(scheme)) {
		Dictionary data;
		data["uri"] = uri;
		data["scheme"] = scheme;
		return GDKResult::error_result(
				E_NOTIMPL,
				"unsupported_launcher_destination",
				"This URI destination is not supported by GDK.launcher on PC.",
				data);
	}

	XUserHandle user_handle = nullptr;
	if (p_user.is_valid()) {
		user_handle = p_user->get_handle();
		if (user_handle == nullptr) {
			return GDKResult::error_result(
					E_INVALIDARG,
					"invalid_user",
					"A signed-in GDKUser is required when a user is provided.");
		}
	}

	const CharString uri_utf8 = uri.utf8();
	HRESULT hr = XLaunchUri(user_handle, uri_utf8.get_data());
	if (FAILED(hr)) {
		Dictionary data;
		data["uri"] = uri;
		data["destination"] = "uri";
		return GDKResult::hresult_error(hr, "Failed to launch the requested URI.", "launch_uri_failed", data);
	}

	Dictionary data;
	data["uri"] = uri;
	data["destination"] = "uri";
	return GDKResult::ok_result(data);
}

bool GDKLauncher::try_parse_uri_scheme_internal(const String &p_uri, String *r_scheme) {
	if (r_scheme == nullptr) {
		return false;
	}

	*r_scheme = String();

	const String uri = p_uri.strip_edges();
	if (uri.is_empty() || uri.contains(" ")) {
		return false;
	}

	const int64_t separator = uri.find(":");
	if (separator <= 0) {
		return false;
	}

	const String scheme = uri.substr(0, separator).to_lower();
	if (scheme.is_empty()) {
		return false;
	}

	*r_scheme = scheme;
	return true;
}

bool GDKLauncher::is_supported_scheme_internal(const String &p_scheme) {
	const String scheme = p_scheme.to_lower();
	if (scheme.begins_with("ms-")) {
		return scheme == "ms-settings" || scheme == "ms-windows-store";
	}
	return true;
}

bool GDKLauncher::is_disallowed_scheme_internal(const String &p_scheme) {
	const String scheme = p_scheme.to_lower();
	return scheme == "file" || scheme == "javascript" || scheme == "data" || scheme == "about";
}

#else
void GDKLauncher::set_owner(GDK *p_owner) {}

Ref<GDKResult> GDKLauncher::on_runtime_initialized() {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

void GDKLauncher::shutdown() {}

Ref<GDKResult> GDKLauncher::launch_uri(const String &p_uri, const Ref<GDKUser> &p_user) {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

bool GDKLauncher::try_parse_uri_scheme_internal(const String &p_uri, String *r_scheme) {
	return false;
}

bool GDKLauncher::is_supported_scheme_internal(const String &p_scheme) {
	return false;
}

bool GDKLauncher::is_disallowed_scheme_internal(const String &p_scheme) {
	return false;
}
#endif
