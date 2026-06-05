/**************************************************************************/
/*  gdk_store.h                                                           */
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

#include "gdk_gdk_stubs.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifdef XBOX_MODULE_GDK_ENABLED
#include "gdk_windows.h"
#endif

#include <vector>

#include "core/object/class_db.h"
#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/variant/callable.h"
#include "core/variant/variant.h"

#ifdef XBOX_MODULE_GDK_ENABLED
#include <XStore.h>
#endif

class GDK;
class GDKPendingSignal;
class GDKResult;
class GDKRuntime;
class GDKUser;

class GDKStoreLicenseStatus : public RefCounted {
	GDCLASS(GDKStoreLicenseStatus, RefCounted);

	String m_store_id;
	String m_licensable_sku;
	int64_t m_status = 0;

protected:
	static void _bind_methods();

public:
	String get_store_id() const;
	String get_licensable_sku() const;
	int64_t get_status() const;

	void set_values(const String &p_store_id, const String &p_licensable_sku, int64_t p_status);
};

class GDKStore : public RefCounted {
	GDCLASS(GDKStore, RefCounted);

	struct CachedLicenseStatus {
		String store_id;
		Ref<GDKStoreLicenseStatus> status;
	};

	GDK *m_owner = nullptr;
	bool m_runtime_ready = false;
	XStoreContextHandle m_store_context = nullptr;
	std::vector<CachedLicenseStatus> m_cached_license_status;

	GDKRuntime *_get_runtime() const;
	void _close_store_context();
	XStoreContextHandle _get_or_create_store_context(HRESULT &r_hresult);
	Signal _make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data = Variant()) const;
	Signal _start_license_status_async(const Ref<GDKUser> &p_user, const String &p_store_id, bool p_is_refresh);
	static String _normalize_store_id(const String &p_store_id);

protected:
	static void _bind_methods();

public:
	~GDKStore() override;

	void set_owner(GDK *p_owner);

	Ref<GDKResult> on_runtime_initialized();
	void shutdown();
	bool is_runtime_ready() const;
	void on_user_removed(const Ref<GDKUser> &p_user);

	Signal query_license_status_async(const Ref<GDKUser> &p_user, const String &p_store_id);
	Signal refresh_entitlements_async(const Ref<GDKUser> &p_user, const String &p_store_id);
	Signal show_purchase_ui_async(const Ref<GDKUser> &p_user, const String &p_store_id);

	Ref<GDKStoreLicenseStatus> get_cached_license_status(const String &p_store_id) const;
	Ref<GDKResult> check_cached_license_status(const String &p_store_id) const;
	Ref<GDKStoreLicenseStatus> _find_cached_license_status(const String &p_store_id) const;
	Ref<GDKStoreLicenseStatus> _cache_license_status(const Ref<GDKStoreLicenseStatus> &p_status);
};
