/**************************************************************************/
/*  gdk_display.cpp                                                       */
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
#include "gdk_display.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "gdk_windows.h"

#include "core/variant/dictionary.h"

#include "gdk.h"
#include "gdk_result.h"
#include "gdk_runtime.h"

void GDKDisplayTimeoutDeferral::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_valid"), &GDKDisplayTimeoutDeferral::is_valid);
	ClassDB::bind_method(D_METHOD("release"), &GDKDisplayTimeoutDeferral::release);
}

GDKDisplayTimeoutDeferral::~GDKDisplayTimeoutDeferral() {
	release();
}

bool GDKDisplayTimeoutDeferral::is_valid() const {
	return m_handle != nullptr;
}

#ifdef XBOX_MODULE_GDK_ENABLED
void GDKDisplayTimeoutDeferral::release() {
	if (m_handle != nullptr) {
		XDisplayCloseTimeoutDeferralHandle(m_handle);
		m_handle = nullptr;
	}
}
#else
void GDKDisplayTimeoutDeferral::release() {
	m_handle = nullptr;
}
#endif

void GDKDisplayTimeoutDeferral::set_handle_internal(XDisplayTimeoutDeferralHandle p_handle) {
	if (m_handle == p_handle) {
		return;
	}
	release();
	m_handle = p_handle;
}

void GDKDisplay::_bind_methods() {
	ClassDB::bind_method(
			D_METHOD("try_enable_hdr_mode", "preference"),
			&GDKDisplay::try_enable_hdr_mode,
			DEFVAL(static_cast<int64_t>(HDR_MODE_PREFERENCE_PREFER_HDR)));
	ClassDB::bind_method(D_METHOD("acquire_timeout_deferral"), &GDKDisplay::acquire_timeout_deferral);

	BIND_ENUM_CONSTANT(HDR_MODE_UNKNOWN);
	BIND_ENUM_CONSTANT(HDR_MODE_ENABLED);
	BIND_ENUM_CONSTANT(HDR_MODE_DISABLED);

	BIND_ENUM_CONSTANT(HDR_MODE_PREFERENCE_PREFER_HDR);
	BIND_ENUM_CONSTANT(HDR_MODE_PREFERENCE_PREFER_REFRESH_RATE);
}

#ifdef XBOX_MODULE_GDK_ENABLED
void GDKDisplay::set_owner(GDK *p_owner) {
	m_owner = p_owner;
}

GDKRuntime *GDKDisplay::_get_runtime() const {
	return m_owner != nullptr ? m_owner->get_runtime() : nullptr;
}

Ref<GDKResult> GDKDisplay::on_runtime_initialized() {
	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr || !runtime->is_initialized()) {
		return GDKResult::error_result(
				E_FAIL,
				"runtime_not_initialized",
				"Cannot initialize the display service before the GDK runtime.");
	}
	m_runtime_ready = true;
	return GDKResult::ok_result();
}

void GDKDisplay::shutdown() {
	m_runtime_ready = false;
}

Ref<GDKResult> GDKDisplay::try_enable_hdr_mode(int64_t p_preference) {
	if (!m_runtime_ready) {
		return GDKResult::error_result(
				E_FAIL,
				"not_initialized",
				"GDK is not initialized. Call GDK.initialize() first.");
	}

	XDisplayHdrModePreference native_preference;
	switch (p_preference) {
		case HDR_MODE_PREFERENCE_PREFER_HDR:
			native_preference = XDisplayHdrModePreference::PreferHdr;
			break;
		case HDR_MODE_PREFERENCE_PREFER_REFRESH_RATE:
			native_preference = XDisplayHdrModePreference::PreferRefreshRate;
			break;
		default:
			return GDKResult::error_result(
					E_INVALIDARG,
					"invalid_preference",
					"Unknown HDR mode preference value.");
	}

	XDisplayHdrModeInfo info_native = {};
	const XDisplayHdrModeResult mode_result = XDisplayTryEnableHdrMode(native_preference, &info_native);

	Dictionary data;
	data["mode"] = static_cast<int64_t>(mode_result);
	if (mode_result == XDisplayHdrModeResult::Enabled) {
		Dictionary info;
		info["min_tone_map_luminance"] = static_cast<double>(info_native.minToneMapLuminance);
		info["max_tone_map_luminance"] = static_cast<double>(info_native.maxToneMapLuminance);
		info["max_full_frame_tone_map_luminance"] = static_cast<double>(info_native.maxFullFrameToneMapLuminance);
		data["info"] = info;
	}
	return GDKResult::ok_result(data);
}

Ref<GDKResult> GDKDisplay::acquire_timeout_deferral() {
	if (!m_runtime_ready) {
		return GDKResult::error_result(
				E_FAIL,
				"not_initialized",
				"GDK is not initialized. Call GDK.initialize() first.");
	}

	XDisplayTimeoutDeferralHandle handle = nullptr;
	HRESULT hr = XDisplayAcquireTimeoutDeferral(&handle);
	if (FAILED(hr)) {
		return GDKResult::hresult_error(
				hr,
				"Failed to acquire display timeout deferral.",
				"acquire_timeout_deferral_failed");
	}

	Ref<GDKDisplayTimeoutDeferral> deferral;
	deferral.instantiate();
	deferral->set_handle_internal(handle);
	return GDKResult::ok_result(deferral);
}

#else
void GDKDisplay::set_owner(GDK *p_owner) {}

Ref<GDKResult> GDKDisplay::on_runtime_initialized() {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

void GDKDisplay::shutdown() {}

Ref<GDKResult> GDKDisplay::try_enable_hdr_mode(int64_t p_preference) {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Ref<GDKResult> GDKDisplay::acquire_timeout_deferral() {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}
#endif
