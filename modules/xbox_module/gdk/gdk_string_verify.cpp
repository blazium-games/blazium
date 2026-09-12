/**************************************************************************/
/*  gdk_string_verify.cpp                                                 */
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

#include "gdk_string_verify.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "gdk_windows.h"

#include <utility>
#include <vector>

#include "gdk.h"
#include "gdk_pending_signal.h"
#include "gdk_result.h"
#include "gdk_runtime.h"
#include "gdk_signal_xasync_context.h"
#include "gdk_user.h"
#include "gdk_xbox_services.h"

#ifdef XBOX_MODULE_GDK_ENABLED

namespace {

String _utf8_or_empty(const char *p_value) {
	if (p_value == nullptr || p_value[0] == '\0') {
		return String();
	}
	return String::utf8(p_value);
}

String _verify_result_code_to_string(XblVerifyStringResultCode p_result_code) {
	switch (p_result_code) {
		case XblVerifyStringResultCode::Success:
			return "success";
		case XblVerifyStringResultCode::Offensive:
			return "offensive";
		case XblVerifyStringResultCode::TooLong:
			return "too_long";
		case XblVerifyStringResultCode::UnknownError:
		default:
			return "unknown_error";
	}
}

Dictionary _make_verify_result_dictionary(const XblVerifyStringResult &p_result) {
	Dictionary result;
	result["result_code"] = _verify_result_code_to_string(p_result.resultCode);
	result["acceptable"] = p_result.resultCode == XblVerifyStringResultCode::Success;
	result["first_offending_substring"] = _utf8_or_empty(p_result.firstOffendingSubstring);
	return result;
}

Array _make_verify_result_array(const XblVerifyStringResult *p_results, size_t p_count) {
	Array result;
	if (p_results == nullptr) {
		return result;
	}
	for (size_t i = 0; i < p_count; ++i) {
		result.push_back(_make_verify_result_dictionary(p_results[i]));
	}
	return result;
}

class StringVerifyAsyncContext final : public GDKSignalXAsyncContext {
	XblContextHandle m_context = nullptr;
	bool m_batch = false;
	String m_text;
	CharString m_text_utf8;
	PackedStringArray m_strings;
	std::vector<CharString> m_string_utf8_values;
	std::vector<const char *> m_string_ptrs;

protected:
	void finalize(XAsyncBlock *p_async_block) override {
		Ref<GDKResult> result;

		if (get_runtime()->is_shutting_down() || get_pending_signal()->was_cancel_requested()) {
			result = GDKResult::cancelled("String verification cancelled.");
			get_pending_signal()->complete(result);
			return;
		}

		size_t result_size = 0;
		HRESULT result_hr = m_batch ? XblStringVerifyStringsResultSize(p_async_block, &result_size) : XblStringVerifyStringResultSize(p_async_block, &result_size);
		if (result_hr == E_ABORT) {
			result = GDKResult::cancelled("String verification cancelled.");
			get_pending_signal()->complete(result);
			return;
		}
		if (FAILED(result_hr)) {
			result = GDKResult::hresult_error(result_hr, "Failed to retrieve string verification result size.", "string_verify_result_size_failed");
			get_pending_signal()->complete(result);
			return;
		}

		std::vector<uint8_t> buffer(result_size);
		size_t buffer_used = 0;
		if (m_batch) {
			XblVerifyStringResult *native_results = nullptr;
			size_t result_count = 0;
			result_hr = XblStringVerifyStringsResult(
					p_async_block,
					buffer.size(),
					buffer.empty() ? nullptr : buffer.data(),
					&native_results,
					&result_count,
					&buffer_used);
			if (FAILED(result_hr)) {
				result = result_hr == E_ABORT ? GDKResult::cancelled("String verification cancelled.") : GDKResult::hresult_error(result_hr, "Failed to retrieve string verification results.", "string_verify_results_failed");
				get_pending_signal()->complete(result);
				return;
			}

			get_pending_signal()->complete(GDKResult::ok_result(_make_verify_result_array(native_results, result_count)));
			return;
		}

		XblVerifyStringResult *native_result = nullptr;
		result_hr = XblStringVerifyStringResult(
				p_async_block,
				buffer.size(),
				buffer.empty() ? nullptr : buffer.data(),
				&native_result,
				&buffer_used);
		if (FAILED(result_hr)) {
			result = result_hr == E_ABORT ? GDKResult::cancelled("String verification cancelled.") : GDKResult::hresult_error(result_hr, "Failed to retrieve string verification result.", "string_verify_result_failed");
			get_pending_signal()->complete(result);
			return;
		}

		Dictionary payload = native_result == nullptr ? Dictionary() : _make_verify_result_dictionary(*native_result);
		get_pending_signal()->complete(GDKResult::ok_result(payload));
	}

public:
	StringVerifyAsyncContext(GDKRuntime *p_runtime, const Ref<GDKPendingSignal> &p_pending_signal, XblContextHandle p_context, const String &p_text) :
			GDKSignalXAsyncContext(p_runtime, p_pending_signal),
			m_context(p_context),
			m_text(p_text) {
		m_text_utf8 = m_text.utf8();
	}

	StringVerifyAsyncContext(GDKRuntime *p_runtime, const Ref<GDKPendingSignal> &p_pending_signal, XblContextHandle p_context, const PackedStringArray &p_strings) :
			GDKSignalXAsyncContext(p_runtime, p_pending_signal),
			m_context(p_context),
			m_batch(true),
			m_strings(p_strings) {
		m_string_utf8_values.reserve(static_cast<size_t>(m_strings.size()));
		m_string_ptrs.reserve(static_cast<size_t>(m_strings.size()));
		for (int64_t i = 0; i < m_strings.size(); ++i) {
			m_string_utf8_values.push_back(m_strings[i].utf8());
		}
		for (const CharString &utf8 : m_string_utf8_values) {
			m_string_ptrs.push_back(utf8.get_data());
		}
	}

	~StringVerifyAsyncContext() override {
		if (m_context != nullptr) {
			XblContextCloseHandle(m_context);
			m_context = nullptr;
		}
	}

	XblContextHandle get_context() const {
		return m_context;
	}

	const char *get_text() const {
		return m_text_utf8.get_data();
	}

	const char **get_strings_data() {
		return m_string_ptrs.data();
	}

	uint64_t get_strings_count() const {
		return static_cast<uint64_t>(m_string_ptrs.size());
	}
};

} //namespace

#endif

void GDKStringVerify::_bind_methods() {
	ClassDB::bind_method(D_METHOD("verify_string_async", "user", "text"), &GDKStringVerify::verify_string_async);
	ClassDB::bind_method(D_METHOD("verify_strings_async", "user", "strings"), &GDKStringVerify::verify_strings_async);
}

#ifdef XBOX_MODULE_GDK_ENABLED
void GDKStringVerify::set_owner(GDK *p_owner) {
	m_owner = p_owner;
}

GDKRuntime *GDKStringVerify::_get_runtime() const {
	return m_owner == nullptr ? nullptr : m_owner->get_runtime();
}

GDKXboxServices *GDKStringVerify::_get_xbox_services() const {
	return m_owner == nullptr ? nullptr : m_owner->get_xbox_services();
}

Signal GDKStringVerify::_make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data) const {
	GDKRuntime *runtime = _get_runtime();
	ERR_FAIL_NULL_V(runtime, Signal());
	return runtime->make_error_signal(p_hresult, p_code, p_message, p_data);
}

Ref<GDKResult> GDKStringVerify::_ensure_ready_user(const Ref<GDKUser> &p_user) const {
	if (!m_runtime_ready) {
		return GDKResult::error_result(E_FAIL, "runtime_unavailable", "GDK runtime is not initialized.");
	}
	if (!p_user.is_valid() || p_user->get_handle() == nullptr || !p_user->is_signed_in()) {
		return GDKResult::error_result(E_INVALIDARG, "invalid_user", "A signed-in GDKUser is required.");
	}
	return GDKResult::ok_result();
}

Ref<GDKResult> GDKStringVerify::on_runtime_initialized() {
	m_runtime_ready = true;
	return GDKResult::ok_result();
}

void GDKStringVerify::shutdown() {
	m_runtime_ready = false;
}

Signal GDKStringVerify::verify_string_async(const Ref<GDKUser> &p_user, const String &p_text) {
	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr) {
		return Signal();
	}

	Ref<GDKResult> validation = _ensure_ready_user(p_user);
	if (!validation->is_ok()) {
		return _make_error_signal(static_cast<HRESULT>(validation->get_hresult()), validation->get_code(), validation->get_message());
	}

	GDKXboxServices *xbox_services = _get_xbox_services();
	if (xbox_services == nullptr || !xbox_services->is_initialized()) {
		return _make_error_signal(E_FAIL, "xbox_services_not_initialized", "Xbox services are unavailable. Ensure the title has a TitleId before using string verification.");
	}

	XblContextHandle context = nullptr;
	HRESULT hr = xbox_services->duplicate_context_for_user(p_user, &context);
	if (FAILED(hr)) {
		return _make_error_signal(hr, "xbox_context_unavailable", "Failed to create an Xbox services context for the user.");
	}

	Ref<GDKPendingSignal> pending_signal = runtime->make_pending_signal();
	StringVerifyAsyncContext *async_context = new StringVerifyAsyncContext(runtime, pending_signal, context, p_text);
	async_context->bind_cancel_handler();
	hr = XblStringVerifyStringAsync(async_context->get_context(), async_context->get_text(), async_context->get_async_block());
	if (FAILED(hr)) {
		async_context->clear_cancel_handler();
		delete async_context;
		return _make_error_signal(hr, "string_verify_start_failed", "Failed to start string verification.");
	}
	return pending_signal->get_completed_signal();
}

Signal GDKStringVerify::verify_strings_async(const Ref<GDKUser> &p_user, const PackedStringArray &p_strings) {
	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr) {
		return Signal();
	}

	Ref<GDKResult> validation = _ensure_ready_user(p_user);
	if (!validation->is_ok()) {
		return _make_error_signal(static_cast<HRESULT>(validation->get_hresult()), validation->get_code(), validation->get_message());
	}

	if (p_strings.is_empty()) {
		return _make_error_signal(E_INVALIDARG, "invalid_strings", "At least one string is required.");
	}

	GDKXboxServices *xbox_services = _get_xbox_services();
	if (xbox_services == nullptr || !xbox_services->is_initialized()) {
		return _make_error_signal(E_FAIL, "xbox_services_not_initialized", "Xbox services are unavailable. Ensure the title has a TitleId before using string verification.");
	}

	XblContextHandle context = nullptr;
	HRESULT hr = xbox_services->duplicate_context_for_user(p_user, &context);
	if (FAILED(hr)) {
		return _make_error_signal(hr, "xbox_context_unavailable", "Failed to create an Xbox services context for the user.");
	}

	Ref<GDKPendingSignal> pending_signal = runtime->make_pending_signal();
	StringVerifyAsyncContext *async_context = new StringVerifyAsyncContext(runtime, pending_signal, context, p_strings);
	async_context->bind_cancel_handler();
	hr = XblStringVerifyStringsAsync(async_context->get_context(), async_context->get_strings_data(), async_context->get_strings_count(), async_context->get_async_block());
	if (FAILED(hr)) {
		async_context->clear_cancel_handler();
		delete async_context;
		return _make_error_signal(hr, "string_verify_start_failed", "Failed to start batch string verification.");
	}
	return pending_signal->get_completed_signal();
}

void GDKStringVerify::on_user_removed(const Ref<GDKUser> &p_user) {
	(void)p_user;
}

#else
void GDKStringVerify::set_owner(GDK *p_owner) {}

Signal GDKStringVerify::_make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data) const {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Ref<GDKResult> GDKStringVerify::_ensure_ready_user(const Ref<GDKUser> &p_user) const {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Ref<GDKResult> GDKStringVerify::on_runtime_initialized() {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

void GDKStringVerify::shutdown() {}

Signal GDKStringVerify::verify_string_async(const Ref<GDKUser> &p_user, const String &p_text) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKStringVerify::verify_strings_async(const Ref<GDKUser> &p_user, const PackedStringArray &p_strings) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

void GDKStringVerify::on_user_removed(const Ref<GDKUser> &p_user) {}
#endif
