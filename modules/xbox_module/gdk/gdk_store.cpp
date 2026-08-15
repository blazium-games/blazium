/**************************************************************************/
/*  gdk_store.cpp                                                         */
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
#include "gdk_store.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "gdk_windows.h"

#include <string>

#include "core/variant/dictionary.h"

#include "gdk.h"
#include "gdk_pending_signal.h"
#include "gdk_result.h"
#include "gdk_runtime.h"
#include "gdk_signal_xasync_context.h"
#include "gdk_user.h"

#ifdef XBOX_MODULE_GDK_ENABLED

namespace {

class StoreXAsyncContext : public GDKSignalXAsyncContext {
protected:
	Ref<GDKStore> m_service;
	String m_store_id;
	std::string m_store_id_utf8;

public:
	StoreXAsyncContext(
			GDKStore *p_service,
			GDKRuntime *p_runtime,
			const Ref<GDKPendingSignal> &p_pending_signal,
			const String &p_store_id) :
			GDKSignalXAsyncContext(p_runtime, p_pending_signal),
			m_service(p_service),
			m_store_id(p_store_id),
			m_store_id_utf8(p_store_id.utf8().get_data()) {}

	const char *get_store_id_utf8() const {
		return m_store_id_utf8.c_str();
	}

	String get_store_id() const {
		return m_store_id;
	}
};

class StoreLicenseStatusAsyncContext final : public StoreXAsyncContext {
	bool m_is_refresh = false;

protected:
	void finalize(XAsyncBlock *p_async_block) override {
		Ref<GDKResult> result;
		if (get_runtime()->is_shutting_down() || get_pending_signal()->was_cancel_requested()) {
			result = GDKResult::cancelled("Store license status request cancelled.");
			get_pending_signal()->complete(result);
			return;
		}

		XStoreCanAcquireLicenseResult native_result = {};
		HRESULT result_hr = XStoreCanAcquireLicenseForStoreIdResult(p_async_block, &native_result);
		if (result_hr == E_ABORT) {
			result = GDKResult::cancelled("Store license status request cancelled.");
			get_pending_signal()->complete(result);
			return;
		}
		if (FAILED(result_hr)) {
			result = GDKResult::hresult_error(
					result_hr,
					m_is_refresh ? "Failed to refresh store entitlements." : "Failed to query store license status.",
					m_is_refresh ? "store_entitlements_refresh_result_failed" : "store_license_status_result_failed");
			get_pending_signal()->complete(result);
			return;
		}

		if (!m_service.is_valid() || !m_service->is_runtime_ready()) {
			result = GDKResult::cancelled("GDKStore is shutting down.");
			get_pending_signal()->complete(result);
			return;
		}

		Ref<GDKStoreLicenseStatus> status;
		status.instantiate();
		status->set_values(get_store_id(), String::utf8(native_result.licensableSku), static_cast<int64_t>(native_result.status));
		Ref<GDKStoreLicenseStatus> cached_status = m_service->_cache_license_status(status);
		get_pending_signal()->complete(GDKResult::ok_result(cached_status));
	}

public:
	StoreLicenseStatusAsyncContext(
			GDKStore *p_service,
			GDKRuntime *p_runtime,
			const Ref<GDKPendingSignal> &p_pending_signal,
			const String &p_store_id,
			bool p_is_refresh) :
			StoreXAsyncContext(p_service, p_runtime, p_pending_signal, p_store_id),
			m_is_refresh(p_is_refresh) {}
};

class StorePurchaseUiAsyncContext final : public StoreXAsyncContext {
protected:
	void finalize(XAsyncBlock *p_async_block) override {
		Ref<GDKResult> result;
		if (get_runtime()->is_shutting_down() || get_pending_signal()->was_cancel_requested()) {
			result = GDKResult::cancelled("Store purchase UI request cancelled.");
			get_pending_signal()->complete(result);
			return;
		}

		HRESULT result_hr = XStoreShowPurchaseUIResult(p_async_block);
		if (result_hr == E_ABORT) {
			result = GDKResult::cancelled("Store purchase UI was dismissed.");
			get_pending_signal()->complete(result);
			return;
		}
		if (FAILED(result_hr)) {
			result = GDKResult::hresult_error(
					result_hr,
					"Failed to complete the store purchase UI flow.",
					"store_purchase_result_failed");
			get_pending_signal()->complete(result);
			return;
		}

		Dictionary data;
		data["store_id"] = get_store_id();
		get_pending_signal()->complete(GDKResult::ok_result(data));
	}

public:
	StorePurchaseUiAsyncContext(
			GDKStore *p_service,
			GDKRuntime *p_runtime,
			const Ref<GDKPendingSignal> &p_pending_signal,
			const String &p_store_id) :
			StoreXAsyncContext(p_service, p_runtime, p_pending_signal, p_store_id) {}
};

} //namespace

#endif

void GDKStoreLicenseStatus::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_store_id"), &GDKStoreLicenseStatus::get_store_id);
	ClassDB::bind_method(D_METHOD("get_licensable_sku"), &GDKStoreLicenseStatus::get_licensable_sku);
	ClassDB::bind_method(D_METHOD("get_status"), &GDKStoreLicenseStatus::get_status);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "store_id"), "", "get_store_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "licensable_sku"), "", "get_licensable_sku");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "status"), "", "get_status");
}

String GDKStoreLicenseStatus::get_store_id() const {
	return m_store_id;
}

String GDKStoreLicenseStatus::get_licensable_sku() const {
	return m_licensable_sku;
}

int64_t GDKStoreLicenseStatus::get_status() const {
	return m_status;
}

void GDKStoreLicenseStatus::set_values(const String &p_store_id, const String &p_licensable_sku, int64_t p_status) {
	m_store_id = p_store_id;
	m_licensable_sku = p_licensable_sku;
	m_status = p_status;
}

void GDKStore::_bind_methods() {
	ClassDB::bind_method(D_METHOD("query_license_status_async", "user", "store_id"), &GDKStore::query_license_status_async);
	ClassDB::bind_method(D_METHOD("refresh_entitlements_async", "user", "store_id"), &GDKStore::refresh_entitlements_async);
	ClassDB::bind_method(D_METHOD("show_purchase_ui_async", "user", "store_id"), &GDKStore::show_purchase_ui_async);
	ClassDB::bind_method(D_METHOD("get_cached_license_status", "store_id"), &GDKStore::get_cached_license_status);
	ClassDB::bind_method(D_METHOD("check_cached_license_status", "store_id"), &GDKStore::check_cached_license_status);
}

#ifdef XBOX_MODULE_GDK_ENABLED
GDKStore::~GDKStore() {
	shutdown();
}

void GDKStore::set_owner(GDK *p_owner) {
	m_owner = p_owner;
}

Ref<GDKResult> GDKStore::on_runtime_initialized() {
	m_runtime_ready = true;
	return GDKResult::ok_result();
}

void GDKStore::shutdown() {
	m_runtime_ready = false;
	_close_store_context();
	m_cached_license_status.clear();
}

bool GDKStore::is_runtime_ready() const {
	return m_runtime_ready;
}

void GDKStore::on_user_removed(const Ref<GDKUser> &p_user) {
	(void)p_user;
	m_cached_license_status.clear();
}

Signal GDKStore::query_license_status_async(const Ref<GDKUser> &p_user, const String &p_store_id) {
	return _start_license_status_async(p_user, p_store_id, false);
}

Signal GDKStore::refresh_entitlements_async(const Ref<GDKUser> &p_user, const String &p_store_id) {
	return _start_license_status_async(p_user, p_store_id, true);
}

Signal GDKStore::show_purchase_ui_async(const Ref<GDKUser> &p_user, const String &p_store_id) {
	const String store_id = _normalize_store_id(p_store_id);
	if (store_id.is_empty()) {
		return _make_error_signal(E_INVALIDARG, "invalid_product_id", "A non-empty Store product ID is required.");
	}

	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr || !runtime->is_available()) {
		return _make_error_signal(E_FAIL, "runtime_unavailable", "GDK runtime is unavailable in the current process.");
	}
	if (!m_runtime_ready || !runtime->is_initialized()) {
		return _make_error_signal(E_FAIL, "not_initialized", "GDK is not initialized. Call GDK.initialize() first.");
	}
	if (!p_user.is_valid() || p_user->get_handle() == nullptr) {
		return _make_error_signal(E_INVALIDARG, "invalid_user", "A signed-in GDKUser is required.");
	}

	HRESULT context_hr = S_OK;
	XStoreContextHandle store_context = _get_or_create_store_context(context_hr);
	if (FAILED(context_hr) || store_context == nullptr) {
		return _make_error_signal(context_hr, "store_context_create_failed", "Failed to create an XStore context for the user.");
	}

	Ref<GDKPendingSignal> pending_signal = runtime->make_pending_signal();
	auto *context = new StorePurchaseUiAsyncContext(this, runtime, pending_signal, store_id);
	context->bind_cancel_handler();

	HRESULT start_hr = XStoreShowPurchaseUIAsync(store_context, context->get_store_id_utf8(), nullptr, nullptr, context->get_async_block());
	if (FAILED(start_hr)) {
		pending_signal->clear_cancel_handler();
		delete context;

		Ref<GDKResult> result = GDKResult::hresult_error(
				start_hr,
				"Failed to start the store purchase UI flow.",
				"store_purchase_start_failed");
		pending_signal->complete_deferred(result);
	}

	return pending_signal->get_completed_signal();
}

Ref<GDKStoreLicenseStatus> GDKStore::get_cached_license_status(const String &p_store_id) const {
	return _find_cached_license_status(_normalize_store_id(p_store_id));
}

Ref<GDKResult> GDKStore::check_cached_license_status(const String &p_store_id) const {
	const String store_id = _normalize_store_id(p_store_id);
	if (store_id.is_empty()) {
		return GDKResult::error_result(E_INVALIDARG, "invalid_product_id", "A non-empty Store product ID is required.");
	}

	Ref<GDKStoreLicenseStatus> status = _find_cached_license_status(store_id);
	if (!status.is_valid()) {
		return GDKResult::error_result(E_FAIL, "license_status_not_cached", "No cached license status is available for the requested Store product ID.");
	}

	return GDKResult::ok_result(status);
}

GDKRuntime *GDKStore::_get_runtime() const {
	return m_owner != nullptr ? m_owner->get_runtime() : nullptr;
}

void GDKStore::_close_store_context() {
	if (m_store_context != nullptr) {
		XStoreCloseContextHandle(m_store_context);
		m_store_context = nullptr;
	}
}

XStoreContextHandle GDKStore::_get_or_create_store_context(HRESULT &r_hresult) {
	if (m_store_context != nullptr) {
		r_hresult = S_OK;
		return m_store_context;
	}

	XStoreContextHandle store_context = nullptr;
	r_hresult = XStoreCreateContext(nullptr, &store_context);
	if (FAILED(r_hresult) || store_context == nullptr) {
		if (SUCCEEDED(r_hresult)) {
			r_hresult = E_UNEXPECTED;
		}
		return nullptr;
	}

	m_store_context = store_context;
	return m_store_context;
}

Signal GDKStore::_make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data) const {
	GDKRuntime *runtime = _get_runtime();
	if (runtime != nullptr) {
		return runtime->make_error_signal(p_hresult, p_code, p_message, p_data);
	}

	Ref<GDKPendingSignal> pending_signal;
	pending_signal.instantiate();
	pending_signal->complete_deferred(GDKResult::error_result(p_hresult, p_code, p_message, p_data));
	return pending_signal->get_completed_signal();
}

Signal GDKStore::_start_license_status_async(const Ref<GDKUser> &p_user, const String &p_store_id, bool p_is_refresh) {
	const String store_id = _normalize_store_id(p_store_id);
	if (store_id.is_empty()) {
		return _make_error_signal(E_INVALIDARG, "invalid_product_id", "A non-empty Store product ID is required.");
	}

	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr || !runtime->is_available()) {
		return _make_error_signal(E_FAIL, "runtime_unavailable", "GDK runtime is unavailable in the current process.");
	}
	if (!m_runtime_ready || !runtime->is_initialized()) {
		return _make_error_signal(E_FAIL, "not_initialized", "GDK is not initialized. Call GDK.initialize() first.");
	}
	if (!p_user.is_valid() || p_user->get_handle() == nullptr) {
		return _make_error_signal(E_INVALIDARG, "invalid_user", "A signed-in GDKUser is required.");
	}

	HRESULT context_hr = S_OK;
	XStoreContextHandle store_context = _get_or_create_store_context(context_hr);
	if (FAILED(context_hr) || store_context == nullptr) {
		return _make_error_signal(context_hr, "store_context_create_failed", "Failed to create an XStore context for the user.");
	}

	Ref<GDKPendingSignal> pending_signal = runtime->make_pending_signal();
	auto *context = new StoreLicenseStatusAsyncContext(this, runtime, pending_signal, store_id, p_is_refresh);
	context->bind_cancel_handler();

	HRESULT start_hr = XStoreCanAcquireLicenseForStoreIdAsync(store_context, context->get_store_id_utf8(), context->get_async_block());
	if (FAILED(start_hr)) {
		pending_signal->clear_cancel_handler();
		delete context;

		Ref<GDKResult> result = GDKResult::hresult_error(
				start_hr,
				p_is_refresh ? "Failed to start entitlements refresh." : "Failed to start store license status query.",
				p_is_refresh ? "store_entitlements_refresh_start_failed" : "store_license_status_start_failed");
		pending_signal->complete_deferred(result);
	}

	return pending_signal->get_completed_signal();
}

Ref<GDKStoreLicenseStatus> GDKStore::_find_cached_license_status(const String &p_store_id) const {
	for (const CachedLicenseStatus &entry : m_cached_license_status) {
		if (entry.store_id == p_store_id) {
			return entry.status;
		}
	}
	return Ref<GDKStoreLicenseStatus>();
}

Ref<GDKStoreLicenseStatus> GDKStore::_cache_license_status(const Ref<GDKStoreLicenseStatus> &p_status) {
	if (!p_status.is_valid()) {
		return Ref<GDKStoreLicenseStatus>();
	}

	const String store_id = p_status->get_store_id();
	for (CachedLicenseStatus &entry : m_cached_license_status) {
		if (entry.store_id == store_id) {
			entry.status = p_status;
			return p_status;
		}
	}

	m_cached_license_status.push_back({ store_id, p_status });
	return p_status;
}

String GDKStore::_normalize_store_id(const String &p_store_id) {
	return p_store_id.strip_edges();
}

#else
GDKStore::~GDKStore() {
}

void GDKStore::set_owner(GDK *p_owner) {}

Ref<GDKResult> GDKStore::on_runtime_initialized() {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

void GDKStore::shutdown() {}

bool GDKStore::is_runtime_ready() const {
	return false;
}

void GDKStore::on_user_removed(const Ref<GDKUser> &p_user) {}

Signal GDKStore::query_license_status_async(const Ref<GDKUser> &p_user, const String &p_store_id) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKStore::refresh_entitlements_async(const Ref<GDKUser> &p_user, const String &p_store_id) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKStore::show_purchase_ui_async(const Ref<GDKUser> &p_user, const String &p_store_id) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Ref<GDKStoreLicenseStatus> GDKStore::get_cached_license_status(const String &p_store_id) const {
	return Ref<GDKStoreLicenseStatus>();
}

Ref<GDKResult> GDKStore::check_cached_license_status(const String &p_store_id) const {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

void GDKStore::_close_store_context() {}

Signal GDKStore::_make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data) const {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKStore::_start_license_status_async(const Ref<GDKUser> &p_user, const String &p_store_id, bool p_is_refresh) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Ref<GDKStoreLicenseStatus> GDKStore::_find_cached_license_status(const String &p_store_id) const {
	return Ref<GDKStoreLicenseStatus>();
}

Ref<GDKStoreLicenseStatus> GDKStore::_cache_license_status(const Ref<GDKStoreLicenseStatus> &p_status) {
	return Ref<GDKStoreLicenseStatus>();
}

String GDKStore::_normalize_store_id(const String &p_store_id) {
	return String();
}
#endif
