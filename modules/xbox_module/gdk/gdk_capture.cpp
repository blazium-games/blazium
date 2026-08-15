/**************************************************************************/
/*  gdk_capture.cpp                                                       */
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
#include "gdk_capture.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "gdk_windows.h"

#include <cmath>
#include <ctime>
#include <limits>

#include "gdk.h"
#include "gdk_pending_signal.h"
#include "gdk_result.h"
#include "gdk_runtime.h"

void GDKCaptureMetaData::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_valid"), &GDKCaptureMetaData::is_valid);
	ClassDB::bind_method(D_METHOD("close"), &GDKCaptureMetaData::close);
	ClassDB::bind_method(D_METHOD("stop_all_states"), &GDKCaptureMetaData::stop_all_states);
	ClassDB::bind_method(D_METHOD("get_remaining_storage_bytes"), &GDKCaptureMetaData::get_remaining_storage_bytes);
	ClassDB::bind_method(
			D_METHOD("add_string_event", "name", "value", "priority"),
			&GDKCaptureMetaData::add_string_event,
			DEFVAL(PRIORITY_GAMEPLAY));
	ClassDB::bind_method(
			D_METHOD("add_double_event", "name", "value", "priority"),
			&GDKCaptureMetaData::add_double_event,
			DEFVAL(PRIORITY_GAMEPLAY));
	ClassDB::bind_method(
			D_METHOD("add_int32_event", "name", "value", "priority"),
			&GDKCaptureMetaData::add_int32_event,
			DEFVAL(PRIORITY_GAMEPLAY));
	ClassDB::bind_method(
			D_METHOD("start_string_state", "name", "value", "priority"),
			&GDKCaptureMetaData::start_string_state,
			DEFVAL(PRIORITY_GAMEPLAY));
	ClassDB::bind_method(
			D_METHOD("start_double_state", "name", "value", "priority"),
			&GDKCaptureMetaData::start_double_state,
			DEFVAL(PRIORITY_GAMEPLAY));
	ClassDB::bind_method(
			D_METHOD("start_int32_state", "name", "value", "priority"),
			&GDKCaptureMetaData::start_int32_state,
			DEFVAL(PRIORITY_GAMEPLAY));

	BIND_ENUM_CONSTANT(PRIORITY_GAMEPLAY);
	BIND_ENUM_CONSTANT(PRIORITY_IMPORTANT);
}

GDKCaptureMetaData::~GDKCaptureMetaData() {
	close();
}

Ref<GDKResult> GDKCaptureMetaData::_check_valid() const {
	if (!m_valid) {
		return GDKResult::error_result(
				E_INVALIDARG,
				"invalid_metadata_handle",
				"The capture metadata context is not valid. Call GDKCapture.create_metadata() first.");
	}
	return Ref<GDKResult>();
}

int32_t GDKCaptureMetaData::_clamp_to_int32(int64_t p_value) {
	return static_cast<int32_t>(
			std::max<int64_t>(std::numeric_limits<int32_t>::min(),
					std::min<int64_t>(std::numeric_limits<int32_t>::max(), p_value)));
}

XAppCaptureMetadataPriority GDKCaptureMetaData::_to_native_priority(int64_t p_priority) {
	if (p_priority == PRIORITY_IMPORTANT) {
		return XAppCaptureMetadataPriority::Important;
	}
	return XAppCaptureMetadataPriority::Informational;
}

Ref<GDKResult> GDKCaptureMetaData::_wrap_hresult(HRESULT p_hr, const char *p_action, const char *p_code) {
	if (SUCCEEDED(p_hr)) {
		return GDKResult::ok_result();
	}
	return GDKResult::hresult_error(p_hr, p_action, p_code);
}

bool GDKCaptureMetaData::is_valid() const {
	return m_valid;
}

void GDKCaptureMetaData::close() {
	m_valid = false;
}

#ifdef XBOX_MODULE_GDK_ENABLED

Ref<GDKResult> GDKCaptureMetaData::stop_all_states() {
	Ref<GDKResult> valid_check = _check_valid();
	if (valid_check.is_valid()) {
		return valid_check;
	}
	HRESULT hr = XAppCaptureMetadataStopAllStates();
	return _wrap_hresult(hr, "Failed to stop all metadata states.", "capture_metadata_stop_all_states_failed");
}

int64_t GDKCaptureMetaData::get_remaining_storage_bytes() const {
	if (!m_valid) {
		return -1;
	}
	uint64_t remaining = 0;
	HRESULT hr = XAppCaptureMetadataRemainingStorageBytesAvailable(&remaining);
	if (FAILED(hr)) {
		return -1;
	}
	return static_cast<int64_t>(remaining);
}

Ref<GDKResult> GDKCaptureMetaData::add_string_event(
		const String &p_name,
		const String &p_value,
		int64_t p_priority) {
	Ref<GDKResult> valid_check = _check_valid();
	if (valid_check.is_valid()) {
		return valid_check;
	}
	if (p_name.strip_edges().is_empty()) {
		return GDKResult::error_result(
				E_INVALIDARG,
				"invalid_metadata_name",
				"Metadata event name must not be empty.");
	}
	CharString name_utf8 = p_name.utf8();
	CharString value_utf8 = p_value.utf8();
	HRESULT hr = XAppCaptureMetadataAddStringEvent(
			name_utf8.get_data(),
			value_utf8.get_data(),
			_to_native_priority(p_priority));
	return _wrap_hresult(hr, "Failed to add string event to capture metadata.", "capture_metadata_add_string_event_failed");
}

Ref<GDKResult> GDKCaptureMetaData::add_double_event(
		const String &p_name,
		double p_value,
		int64_t p_priority) {
	Ref<GDKResult> valid_check = _check_valid();
	if (valid_check.is_valid()) {
		return valid_check;
	}
	if (p_name.strip_edges().is_empty()) {
		return GDKResult::error_result(
				E_INVALIDARG,
				"invalid_metadata_name",
				"Metadata event name must not be empty.");
	}
	CharString name_utf8 = p_name.utf8();
	HRESULT hr = XAppCaptureMetadataAddDoubleEvent(
			name_utf8.get_data(),
			p_value,
			_to_native_priority(p_priority));
	return _wrap_hresult(hr, "Failed to add double event to capture metadata.", "capture_metadata_add_double_event_failed");
}

Ref<GDKResult> GDKCaptureMetaData::add_int32_event(
		const String &p_name,
		int64_t p_value,
		int64_t p_priority) {
	Ref<GDKResult> valid_check = _check_valid();
	if (valid_check.is_valid()) {
		return valid_check;
	}
	if (p_name.strip_edges().is_empty()) {
		return GDKResult::error_result(
				E_INVALIDARG,
				"invalid_metadata_name",
				"Metadata event name must not be empty.");
	}
	const int32_t clamped = _clamp_to_int32(p_value);
	CharString name_utf8 = p_name.utf8();
	HRESULT hr = XAppCaptureMetadataAddInt32Event(
			name_utf8.get_data(),
			clamped,
			_to_native_priority(p_priority));
	return _wrap_hresult(hr, "Failed to add int32 event to capture metadata.", "capture_metadata_add_int32_event_failed");
}

Ref<GDKResult> GDKCaptureMetaData::start_string_state(
		const String &p_name,
		const String &p_value,
		int64_t p_priority) {
	Ref<GDKResult> valid_check = _check_valid();
	if (valid_check.is_valid()) {
		return valid_check;
	}
	if (p_name.strip_edges().is_empty()) {
		return GDKResult::error_result(
				E_INVALIDARG,
				"invalid_metadata_name",
				"Metadata state name must not be empty.");
	}
	CharString name_utf8 = p_name.utf8();
	CharString value_utf8 = p_value.utf8();
	HRESULT hr = XAppCaptureMetadataStartStringState(
			name_utf8.get_data(),
			value_utf8.get_data(),
			_to_native_priority(p_priority));
	return _wrap_hresult(hr, "Failed to start string state in capture metadata.", "capture_metadata_start_string_state_failed");
}

Ref<GDKResult> GDKCaptureMetaData::start_double_state(
		const String &p_name,
		double p_value,
		int64_t p_priority) {
	Ref<GDKResult> valid_check = _check_valid();
	if (valid_check.is_valid()) {
		return valid_check;
	}
	if (p_name.strip_edges().is_empty()) {
		return GDKResult::error_result(
				E_INVALIDARG,
				"invalid_metadata_name",
				"Metadata state name must not be empty.");
	}
	CharString name_utf8 = p_name.utf8();
	HRESULT hr = XAppCaptureMetadataStartDoubleState(
			name_utf8.get_data(),
			p_value,
			_to_native_priority(p_priority));
	return _wrap_hresult(hr, "Failed to start double state in capture metadata.", "capture_metadata_start_double_state_failed");
}

Ref<GDKResult> GDKCaptureMetaData::start_int32_state(
		const String &p_name,
		int64_t p_value,
		int64_t p_priority) {
	Ref<GDKResult> valid_check = _check_valid();
	if (valid_check.is_valid()) {
		return valid_check;
	}
	if (p_name.strip_edges().is_empty()) {
		return GDKResult::error_result(
				E_INVALIDARG,
				"invalid_metadata_name",
				"Metadata state name must not be empty.");
	}
	const int32_t clamped = _clamp_to_int32(p_value);
	CharString name_utf8 = p_name.utf8();
	HRESULT hr = XAppCaptureMetadataStartInt32State(
			name_utf8.get_data(),
			clamped,
			_to_native_priority(p_priority));
	return _wrap_hresult(hr, "Failed to start int32 state in capture metadata.", "capture_metadata_start_int32_state_failed");
}

void GDKCaptureMetaData::activate_internal() {
	m_valid = true;
}

#else

Ref<GDKResult> GDKCaptureMetaData::stop_all_states() {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

int64_t GDKCaptureMetaData::get_remaining_storage_bytes() const {
	return -1;
}

Ref<GDKResult> GDKCaptureMetaData::add_string_event(const String &p_name, const String &p_value, int64_t p_priority) {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Ref<GDKResult> GDKCaptureMetaData::add_double_event(const String &p_name, double p_value, int64_t p_priority) {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Ref<GDKResult> GDKCaptureMetaData::add_int32_event(const String &p_name, int64_t p_value, int64_t p_priority) {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Ref<GDKResult> GDKCaptureMetaData::start_string_state(const String &p_name, const String &p_value, int64_t p_priority) {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Ref<GDKResult> GDKCaptureMetaData::start_double_state(const String &p_name, double p_value, int64_t p_priority) {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Ref<GDKResult> GDKCaptureMetaData::start_int32_state(const String &p_name, int64_t p_value, int64_t p_priority) {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

void GDKCaptureMetaData::activate_internal() {
	m_valid = true;
}

#endif

void GDKCapture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("enable_capture"), &GDKCapture::enable_capture);
	ClassDB::bind_method(D_METHOD("disable_capture"), &GDKCapture::disable_capture);
	ClassDB::bind_method(
			D_METHOD("record_diagnostic_clip_async", "duration"),
			&GDKCapture::record_diagnostic_clip_async);
	ClassDB::bind_method(
			D_METHOD("take_diagnostic_screenshot_async", "path_hint"),
			&GDKCapture::take_diagnostic_screenshot_async);
	ClassDB::bind_method(
			D_METHOD("create_metadata", "reserved_bytes"),
			&GDKCapture::create_metadata,
			DEFVAL(0));
}

#ifdef XBOX_MODULE_GDK_ENABLED
GDKRuntime *GDKCapture::_get_runtime() const {
	if (m_owner == nullptr) {
		return nullptr;
	}
	return m_owner->get_runtime();
}

Signal GDKCapture::_make_error_signal(
		HRESULT p_hresult,
		const String &p_code,
		const String &p_message) const {
	GDKRuntime *runtime = _get_runtime();
	return runtime != nullptr ? runtime->make_error_signal(p_hresult, p_code, p_message) : Signal();
}

void GDKCapture::set_owner(GDK *p_owner) {
	m_owner = p_owner;
}

Ref<GDKResult> GDKCapture::on_runtime_initialized() {
	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr || !runtime->is_initialized()) {
		return GDKResult::error_result(
				E_FAIL,
				"runtime_not_initialized",
				"Cannot initialize the capture service before the GDK runtime.");
	}
	m_runtime_ready = true;
	return GDKResult::ok_result();
}

void GDKCapture::shutdown() {
	m_runtime_ready = false;
}

Ref<GDKResult> GDKCapture::enable_capture() {
	GDKRuntime *runtime = _get_runtime();
	if (!m_runtime_ready) {
		Ref<GDKResult> result = GDKResult::error_result(
				E_FAIL,
				"runtime_not_initialized",
				"The GDK runtime must be initialized before enabling capture.");
		if (runtime != nullptr) {
		}
		return result;
	}
	HRESULT hr = XAppCaptureEnableRecord();
	if (FAILED(hr)) {
		Ref<GDKResult> result = GDKResult::hresult_error(hr, "Failed to enable capture.", "capture_enable_failed");
		if (runtime != nullptr) {
		}
		return result;
	}
	if (runtime != nullptr) {
	}
	return GDKResult::ok_result();
}

Ref<GDKResult> GDKCapture::disable_capture() {
	GDKRuntime *runtime = _get_runtime();
	if (!m_runtime_ready) {
		Ref<GDKResult> result = GDKResult::error_result(
				E_FAIL,
				"runtime_not_initialized",
				"The GDK runtime must be initialized before disabling capture.");
		if (runtime != nullptr) {
		}
		return result;
	}
	HRESULT hr = XAppCaptureDisableRecord();
	if (FAILED(hr)) {
		Ref<GDKResult> result = GDKResult::hresult_error(hr, "Failed to disable capture.", "capture_disable_failed");
		if (runtime != nullptr) {
		}
		return result;
	}
	if (runtime != nullptr) {
	}
	return GDKResult::ok_result();
}

Signal GDKCapture::record_diagnostic_clip_async(double p_duration) {
	GDKRuntime *runtime = _get_runtime();

	if (!std::isfinite(p_duration) || p_duration <= 0.0) {
		return _make_error_signal(
				E_INVALIDARG,
				"invalid_capture_duration",
				"Clip duration must be greater than zero.");
	}

	const double duration_seconds = std::ceil(p_duration);
	const double duration_ms = duration_seconds * 1000.0;
	if (duration_ms > static_cast<double>(std::numeric_limits<uint32_t>::max())) {
		return _make_error_signal(
				E_INVALIDARG,
				"invalid_capture_duration",
				"Clip duration is too large.");
	}

	if (!m_runtime_ready || runtime == nullptr) {
		return _make_error_signal(
				E_FAIL,
				"runtime_not_initialized",
				"The GDK runtime must be initialized before recording a diagnostic clip.");
	}

	Ref<GDKPendingSignal> pending_signal = runtime->make_pending_signal();
	ERR_FAIL_COND_V(pending_signal.is_null(), Signal());

	XAppCaptureRecordClipResult clip_result = {};
	const time_t now = std::time(nullptr);
	const time_t start_time = now - static_cast<time_t>(duration_seconds);
	HRESULT hr = XAppCaptureRecordDiagnosticClip(
			start_time,
			static_cast<uint32_t>(duration_ms),
			nullptr,
			&clip_result);
	if (FAILED(hr)) {
		Ref<GDKResult> result = GDKResult::hresult_error(
				hr,
				"Failed to record diagnostic clip.",
				"capture_record_failed");
		pending_signal->complete_deferred(result);
		return pending_signal->get_completed_signal();
	}

	pending_signal->complete_deferred(GDKResult::ok_result());
	return pending_signal->get_completed_signal();
}

Signal GDKCapture::take_diagnostic_screenshot_async(const String &p_path_hint) {
	GDKRuntime *runtime = _get_runtime();

	if (p_path_hint.strip_edges().is_empty()) {
		return _make_error_signal(
				E_INVALIDARG,
				"invalid_screenshot_path_hint",
				"Screenshot path hint must not be empty.");
	}

	if (!m_runtime_ready || runtime == nullptr) {
		return _make_error_signal(
				E_FAIL,
				"runtime_not_initialized",
				"The GDK runtime must be initialized before taking a diagnostic screenshot.");
	}

	Ref<GDKPendingSignal> pending_signal = runtime->make_pending_signal();
	ERR_FAIL_COND_V(pending_signal.is_null(), Signal());

	CharString path_hint_utf8 = p_path_hint.utf8();
	XAppCaptureDiagnosticScreenshotResult screenshot_result = {};
	HRESULT hr = XAppCaptureTakeDiagnosticScreenshot(
			true,
			XAppCaptureScreenshotFormatFlag::SDR,
			path_hint_utf8.get_data(),
			&screenshot_result);
	if (FAILED(hr)) {
		Ref<GDKResult> result = GDKResult::hresult_error(
				hr,
				"Failed to take diagnostic screenshot.",
				"capture_screenshot_failed");
		pending_signal->complete_deferred(result);
		return pending_signal->get_completed_signal();
	}

	pending_signal->complete_deferred(GDKResult::ok_result());
	return pending_signal->get_completed_signal();
}

Ref<GDKCaptureMetaData> GDKCapture::create_metadata(int64_t p_reserved_bytes) {
	(void)p_reserved_bytes;

	if (!m_runtime_ready) {
		return Ref<GDKCaptureMetaData>();
	}

	Ref<GDKCaptureMetaData> capture_metadata;
	capture_metadata.instantiate();
	capture_metadata->activate_internal();
	return capture_metadata;
}

#else
Signal GDKCapture::_make_error_signal(
		HRESULT p_hresult,
		const String &p_code,
		const String &p_message) const {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

void GDKCapture::set_owner(GDK *p_owner) {}

Ref<GDKResult> GDKCapture::on_runtime_initialized() {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

void GDKCapture::shutdown() {}

Ref<GDKResult> GDKCapture::enable_capture() {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Ref<GDKResult> GDKCapture::disable_capture() {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Signal GDKCapture::record_diagnostic_clip_async(double p_duration) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKCapture::take_diagnostic_screenshot_async(const String &p_path_hint) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Ref<GDKCaptureMetaData> GDKCapture::create_metadata(int64_t p_reserved_bytes) {
	return Ref<GDKCaptureMetaData>();
}
#endif
