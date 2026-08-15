/**************************************************************************/
/*  gdk_title_storage.h                                                   */
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

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/variant/array.h"
#include "core/variant/callable.h"
#include "core/variant/variant.h"

#ifdef XBOX_MODULE_GDK_ENABLED
#include <XUser.h>
#include <xsapi-c/services_c.h>
#endif

class GDK;
class GDKResult;
class GDKRuntime;
class GDKUser;
class GDKXboxServices;

class GDKTitleStorageBlobMetadata : public RefCounted {
	GDCLASS(GDKTitleStorageBlobMetadata, RefCounted);

	String m_blob_path;
	String m_blob_type;
	String m_storage_type;
	String m_display_name;
	String m_e_tag;
	int64_t m_client_timestamp = 0;
	int64_t m_length = 0;
	String m_service_configuration_id;
	String m_xuid;

protected:
	static void _bind_methods();

public:
	String get_blob_path() const;
	String get_blob_type() const;
	String get_storage_type() const;
	String get_display_name() const;
	String get_e_tag() const;
	int64_t get_client_timestamp() const;
	int64_t get_length() const;
	String get_service_configuration_id() const;
	String get_xuid() const;

	void populate_from_native(const XblTitleStorageBlobMetadata &p_metadata);
};

class GDKTitleStorageBlobMetadataResult : public RefCounted {
	GDCLASS(GDKTitleStorageBlobMetadataResult, RefCounted);

	Array m_items;
	bool m_has_next = false;
	String m_storage_type;
	String m_blob_path;
	uint32_t m_max_items = 0;
	Ref<GDKUser> m_user;
	XblTitleStorageBlobMetadataResultHandle m_handle = nullptr;

protected:
	static void _bind_methods();

public:
	~GDKTitleStorageBlobMetadataResult() override;

	Array get_items() const;
	bool has_next() const;
	String get_storage_type() const;
	String get_blob_path() const;

	HRESULT populate_from_handle(
			const Ref<GDKUser> &p_user,
			const String &p_storage_type,
			const String &p_blob_path,
			uint32_t p_max_items,
			XblTitleStorageBlobMetadataResultHandle p_handle);
	XblTitleStorageBlobMetadataResultHandle get_handle_internal() const;
	Ref<GDKUser> get_user_internal() const;
	uint32_t get_max_items_internal() const;
};

class GDKTitleStorage : public RefCounted {
	GDCLASS(GDKTitleStorage, RefCounted);

	GDK *m_owner = nullptr;
	bool m_runtime_ready = false;

	GDKRuntime *_get_runtime() const;
	GDKXboxServices *_get_xbox_services() const;
	Signal _make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data = Variant()) const;
	Ref<GDKResult> _ensure_ready_user(const Ref<GDKUser> &p_user) const;

protected:
	static void _bind_methods();

public:
	void set_owner(GDK *p_owner);

	Ref<GDKResult> on_runtime_initialized();
	void shutdown();

	Signal get_quota_async(const Ref<GDKUser> &p_user, const String &p_storage_type);
	Signal list_blob_metadata_async(const Ref<GDKUser> &p_user, const String &p_storage_type, const String &p_blob_path = String(), int64_t p_skip_items = 0, int64_t p_max_items = 25);
	Signal get_next_blob_metadata_async(const Ref<GDKTitleStorageBlobMetadataResult> &p_result);
	Signal download_blob_async(const Ref<GDKUser> &p_user, const String &p_storage_type, const String &p_blob_path);
	Signal upload_blob_async(const Ref<GDKUser> &p_user, const String &p_storage_type, const String &p_blob_path, const PackedByteArray &p_data, const String &p_display_name = String(), const String &p_e_tag = String(), const String &p_match_condition = "not_used");
	Signal delete_blob_async(const Ref<GDKUser> &p_user, const String &p_storage_type, const String &p_blob_path, const String &p_e_tag = String(), const String &p_match_condition = "not_used");

	void on_user_removed(const Ref<GDKUser> &p_user);
};
