/**************************************************************************/
/*  gdk_package.h                                                         */
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

#include "core/variant/callable.h"
#include "gdk_gdk_stubs.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifdef XBOX_MODULE_GDK_ENABLED
#include "gdk_windows.h"
#endif

#include <vector>

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

#ifdef XBOX_MODULE_GDK_ENABLED
#include <XPackage.h>
#endif

class GDK;
class GDKPendingSignal;
class GDKResult;
class GDKRuntime;

class GDKPackageMount : public RefCounted {
	GDCLASS(GDKPackageMount, RefCounted);

	String m_package_identifier;
	String m_mount_path;
	Dictionary m_package_details;
	XPackageMountHandle m_mount_handle = nullptr;
	bool m_valid = false;

	void _close_handle();

protected:
	static void _bind_methods();

public:
	~GDKPackageMount() override;

	void initialize(const String &p_package_identifier, const String &p_mount_path, const Dictionary &p_package_details, XPackageMountHandle p_mount_handle);
	XPackageMountHandle release_handle();

	String get_package_identifier() const;
	String get_mount_path() const;
	Dictionary get_package_details() const;
	bool is_valid() const;
	Ref<GDKResult> resolve_path(const String &p_relative_path) const;
	Ref<GDKResult> close();
};

class GDKPackageResourcePack : public RefCounted {
	GDCLASS(GDKPackageResourcePack, RefCounted);

	String m_package_identifier;
	String m_mount_path;
	String m_pack_relative_path;
	String m_pack_path;
	Dictionary m_package_details;
	bool m_replace_files = false;
	int64_t m_offset = 0;

protected:
	static void _bind_methods();

public:
	void initialize(
			const String &p_package_identifier,
			const String &p_mount_path,
			const String &p_pack_relative_path,
			const String &p_pack_path,
			const Dictionary &p_package_details,
			bool p_replace_files,
			int64_t p_offset);

	String get_package_identifier() const;
	String get_mount_path() const;
	String get_pack_relative_path() const;
	String get_pack_path() const;
	Dictionary get_package_details() const;
	bool get_replace_files() const;
	int64_t get_offset() const;
};

class GDKPackage : public RefCounted {
	GDCLASS(GDKPackage, RefCounted);

public:
	enum PackageKind {
		PACKAGE_KIND_GAME = static_cast<int>(XPackageKind::Game),
		PACKAGE_KIND_CONTENT = static_cast<int>(XPackageKind::Content),
	};

	enum EnumerationScope {
		ENUMERATION_SCOPE_THIS_PUBLISHER = static_cast<int>(XPackageEnumerationScope::ThisPublisher),
		ENUMERATION_SCOPE_THIS_AND_RELATED = static_cast<int>(XPackageEnumerationScope::ThisAndRelated),
	};

private:
	struct LoadedResourcePack {
		String package_identifier;
		String pack_relative_path;
		XPackageMountHandle mount_handle = nullptr;
		Ref<GDKPackageResourcePack> resource_pack;
	};

	GDK *m_owner = nullptr;
	bool m_runtime_ready = false;
	std::vector<LoadedResourcePack> m_loaded_resource_packs;

	GDKRuntime *_get_runtime() const;
	Ref<GDKResult> _ensure_runtime_ready() const;
	Signal _make_completed_signal(const Ref<GDKResult> &p_result) const;
	Signal _make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data = Variant()) const;
	Ref<GDKResult> _start_mount_async(const String &p_package_identifier, bool p_load_resource_pack, const String &p_pack_relative_path, bool p_replace_files, int64_t p_offset, Signal *r_signal);
	LoadedResourcePack *_find_loaded_resource_pack(const String &p_package_identifier, const String &p_pack_relative_path);
	void _clear_loaded_resource_packs();

protected:
	static void _bind_methods();

public:
	void set_owner(GDK *p_owner);
	Ref<GDKResult> on_runtime_initialized();
	void shutdown();

	Ref<GDKResult> finish_resource_pack_load(
			const String &p_package_identifier,
			const Dictionary &p_package_details,
			const String &p_mount_path,
			XPackageMountHandle p_mount_handle,
			const String &p_pack_relative_path,
			bool p_replace_files,
			int64_t p_offset);

	Ref<GDKResult> enumerate_packages(
			int64_t p_package_kind = static_cast<int64_t>(PACKAGE_KIND_CONTENT),
			int64_t p_scope = static_cast<int64_t>(ENUMERATION_SCOPE_THIS_AND_RELATED));
	Ref<GDKResult> find_package_by_identifier(
			const String &p_package_identifier,
			int64_t p_package_kind = static_cast<int64_t>(PACKAGE_KIND_CONTENT),
			int64_t p_scope = static_cast<int64_t>(ENUMERATION_SCOPE_THIS_AND_RELATED));
	Ref<GDKResult> get_current_process_package_identifier();
	Signal mount_package_async(const String &p_package_identifier);
	Signal load_resource_pack_async(const String &p_package_identifier, const String &p_pack_relative_path, bool p_replace_files = false, int64_t p_offset = 0);
	Array get_loaded_resource_packs() const;
	Ref<GDKResult> get_install_progress(const String &p_package_identifier);
};

VARIANT_ENUM_CAST(GDKPackage::PackageKind);
VARIANT_ENUM_CAST(GDKPackage::EnumerationScope);
