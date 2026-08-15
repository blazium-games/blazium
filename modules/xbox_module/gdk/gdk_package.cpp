/**************************************************************************/
/*  gdk_package.cpp                                                       */
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
#include "gdk_package.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "gdk_windows.h"

#include <limits>
#include <string>
#include <vector>

#include "core/config/project_settings.h"
#include "core/io/file_access.h"
#include "core/io/file_access_pack.h"

#include "gdk.h"
#include "gdk_pending_signal.h"
#include "gdk_result.h"
#include "gdk_runtime.h"
#include "gdk_signal_xasync_context.h"

#ifdef XBOX_MODULE_GDK_ENABLED

namespace {

String to_string_or_empty(const char *p_text) {
	return p_text != nullptr ? String::utf8(p_text) : String();
}

int64_t to_variant_u64(uint64_t p_value) {
	if (p_value == UINT64_MAX) {
		return -1;
	}
	if (p_value > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
		return std::numeric_limits<int64_t>::max();
	}
	return static_cast<int64_t>(p_value);
}

Dictionary package_details_to_dictionary(const XPackageDetails &p_details) {
	Dictionary package;
	package["package_identifier"] = to_string_or_empty(p_details.packageIdentifier);
	package["store_id"] = to_string_or_empty(p_details.storeId);
	package["display_name"] = to_string_or_empty(p_details.displayName);
	package["description"] = to_string_or_empty(p_details.description);
	package["publisher"] = to_string_or_empty(p_details.publisher);
	package["title_id"] = to_string_or_empty(p_details.titleId);
	package["installing"] = p_details.installing;
	package["age_restricted"] = p_details.ageRestricted;
	package["kind"] = static_cast<int64_t>(p_details.kind);
	package["kind_name"] = p_details.kind == XPackageKind::Game ? "game" : "content";
	package["index"] = static_cast<int64_t>(p_details.index);
	package["count"] = static_cast<int64_t>(p_details.count);
	return package;
}

Ref<GDKResult> parse_package_kind(int64_t p_kind, XPackageKind *r_kind) {
	ERR_FAIL_COND_V(r_kind == nullptr, GDKResult::error_result(E_POINTER, "internal_error", "Missing package kind output."));
	if (p_kind != static_cast<int64_t>(GDKPackage::PACKAGE_KIND_GAME) &&
			p_kind != static_cast<int64_t>(GDKPackage::PACKAGE_KIND_CONTENT)) {
		return GDKResult::error_result(E_INVALIDARG, "invalid_package_kind", "Package kind must be PACKAGE_KIND_GAME or PACKAGE_KIND_CONTENT.");
	}

	*r_kind = static_cast<XPackageKind>(p_kind);
	return GDKResult::ok_result();
}

Ref<GDKResult> parse_package_scope(int64_t p_scope, XPackageEnumerationScope *r_scope) {
	ERR_FAIL_COND_V(r_scope == nullptr, GDKResult::error_result(E_POINTER, "internal_error", "Missing package scope output."));
	if (p_scope != static_cast<int64_t>(GDKPackage::ENUMERATION_SCOPE_THIS_PUBLISHER) &&
			p_scope != static_cast<int64_t>(GDKPackage::ENUMERATION_SCOPE_THIS_AND_RELATED)) {
		return GDKResult::error_result(
				E_INVALIDARG,
				"invalid_package_scope",
				"Enumeration scope must be ENUMERATION_SCOPE_THIS_PUBLISHER or ENUMERATION_SCOPE_THIS_AND_RELATED.");
	}

	*r_scope = static_cast<XPackageEnumerationScope>(p_scope);
	return GDKResult::ok_result();
}

Ref<GDKResult> validate_package_identifier(const String &p_package_identifier) {
	String trimmed = p_package_identifier.strip_edges();
	if (trimmed.is_empty()) {
		return GDKResult::error_result(E_INVALIDARG, "invalid_package_identifier", "Package identifier cannot be empty.");
	}
	if (trimmed.length() >= XPACKAGE_IDENTIFIER_MAX_LENGTH) {
		return GDKResult::error_result(E_INVALIDARG, "invalid_package_identifier", "Package identifier exceeds the native XPackage identifier length.");
	}
	if (trimmed.contains("/") || trimmed.contains("\\")) {
		return GDKResult::error_result(E_INVALIDARG, "invalid_package_identifier", "Package identifier cannot contain path separators.");
	}
	return GDKResult::ok_result(trimmed);
}

Ref<GDKResult> normalize_package_relative_path(const String &p_relative_path, String *r_normalized) {
	ERR_FAIL_COND_V(r_normalized == nullptr, GDKResult::error_result(E_POINTER, "internal_error", "Missing relative path output."));

	String trimmed = p_relative_path.strip_edges();
	if (trimmed.is_empty()) {
		return GDKResult::error_result(E_INVALIDARG, "invalid_package_path", "Package-relative path cannot be empty.");
	}

	String candidate = trimmed.replace("\\", "/");
	if (candidate.begins_with("/") || candidate.contains("://") || candidate.contains(":")) {
		return GDKResult::error_result(E_INVALIDARG, "invalid_package_path", "Package-relative path must not be absolute.");
	}

	PackedStringArray segments = candidate.split("/", false);
	for (int64_t i = 0; i < segments.size(); ++i) {
		String segment = segments[i];
		if (segment == "." || segment == "..") {
			return GDKResult::error_result(E_INVALIDARG, "invalid_package_path", "Package-relative path cannot contain current or parent directory segments.");
		}
	}

	String normalized = candidate.simplify_path();
	if (normalized.is_empty() || normalized == "." || normalized.begins_with("../") || normalized == "..") {
		return GDKResult::error_result(E_INVALIDARG, "invalid_package_path", "Package-relative path must stay inside the mounted package.");
	}

	*r_normalized = normalized;
	return GDKResult::ok_result(normalized);
}

Ref<GDKResult> resolve_under_mount_path(const String &p_mount_path, const String &p_relative_path) {
	if (p_mount_path.strip_edges().is_empty()) {
		return GDKResult::error_result(E_FAIL, "package_mount_invalid", "Package mount path is not available.");
	}

	String normalized;
	Ref<GDKResult> path_result = normalize_package_relative_path(p_relative_path, &normalized);
	if (!path_result->is_ok()) {
		return path_result;
	}

	return GDKResult::ok_result(p_mount_path.path_join(normalized));
}

Ref<GDKResult> read_mount_path(XPackageMountHandle p_mount_handle, String *r_mount_path) {
	ERR_FAIL_COND_V(r_mount_path == nullptr, GDKResult::error_result(E_POINTER, "internal_error", "Missing mount path output."));
	if (p_mount_handle == nullptr) {
		return GDKResult::error_result(E_POINTER, "package_mount_failed", "Package mount handle was null.");
	}

	size_t mount_path_size = 0;
	HRESULT hr = XPackageGetMountPathSize(p_mount_handle, &mount_path_size);
	if (FAILED(hr) || mount_path_size == 0) {
		return GDKResult::hresult_error(
				FAILED(hr) ? hr : E_FAIL,
				"Failed to read the mount-path size for the package.",
				"package_path_unavailable");
	}

	std::vector<char> mount_path_buffer(mount_path_size, 0);
	hr = XPackageGetMountPath(p_mount_handle, mount_path_size, mount_path_buffer.data());
	if (FAILED(hr)) {
		return GDKResult::hresult_error(hr, "Failed to read the package mount path.", "package_path_unavailable");
	}

	String mount_path = String::utf8(mount_path_buffer.data()).strip_edges();
	if (mount_path.is_empty()) {
		return GDKResult::error_result(E_FAIL, "package_path_unavailable", "The package mount path was empty.");
	}

	*r_mount_path = mount_path;
	return GDKResult::ok_result(mount_path);
}

struct PackageEnumerationContext {
	Array *packages = nullptr;
	String target_identifier;
	bool find_target = false;
	bool found_target = false;
	Dictionary found_package;
};

class PackageMountAsyncContext : public GDKSignalXAsyncContext {
	GDKPackage *m_package_service = nullptr;
	String m_package_identifier;
	Dictionary m_package_details;
	bool m_load_resource_pack = false;
	String m_pack_relative_path;
	bool m_replace_files = false;
	int64_t m_offset = 0;
	std::string m_package_identifier_utf8;

protected:
	void finalize(XAsyncBlock *p_async_block) override {
		if (get_runtime()->is_shutting_down() || get_pending_signal()->was_cancel_requested()) {
			get_pending_signal()->complete(GDKResult::cancelled("Package mount cancelled."));
			return;
		}

		XPackageMountHandle mount_handle = nullptr;
		HRESULT hr = XPackageMountWithUiResult(p_async_block, &mount_handle);
		if (FAILED(hr)) {
			m_pending_signal->complete(GDKResult::hresult_error(hr, "Failed to mount the package.", "package_mount_failed"));
			return;
		}

		String mount_path;
		Ref<GDKResult> mount_path_result = read_mount_path(mount_handle, &mount_path);
		if (!mount_path_result->is_ok()) {
			XPackageCloseMountHandle(mount_handle);
			m_pending_signal->complete(mount_path_result);
			return;
		}

		if (m_load_resource_pack) {
			if (m_package_service == nullptr) {
				XPackageCloseMountHandle(mount_handle);
				m_pending_signal->complete(GDKResult::error_result(E_FAIL, "not_initialized", "GDK package service is unavailable."));
				return;
			}

			Ref<GDKResult> result = m_package_service->finish_resource_pack_load(
					m_package_identifier,
					m_package_details,
					mount_path,
					mount_handle,
					m_pack_relative_path,
					m_replace_files,
					m_offset);
			m_pending_signal->complete(result);
			return;
		}

		Ref<GDKPackageMount> mount;
		mount.instantiate();
		mount->initialize(m_package_identifier, mount_path, m_package_details, mount_handle);
		m_pending_signal->complete(GDKResult::ok_result(mount));
	}

public:
	PackageMountAsyncContext(
			GDKRuntime *p_runtime,
			const Ref<GDKPendingSignal> &p_pending_signal,
			GDKPackage *p_package_service,
			const String &p_package_identifier,
			const Dictionary &p_package_details,
			bool p_load_resource_pack,
			const String &p_pack_relative_path,
			bool p_replace_files,
			int64_t p_offset) :
			GDKSignalXAsyncContext(p_runtime, p_pending_signal),
			m_package_service(p_package_service),
			m_package_identifier(p_package_identifier),
			m_package_details(p_package_details),
			m_load_resource_pack(p_load_resource_pack),
			m_pack_relative_path(p_pack_relative_path),
			m_replace_files(p_replace_files),
			m_offset(p_offset),
			m_package_identifier_utf8(p_package_identifier.utf8().get_data()) {}

	const char *get_package_identifier_data() const {
		return m_package_identifier_utf8.c_str();
	}
};

} //namespace

#endif

void GDKPackageMount::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_package_identifier"), &GDKPackageMount::get_package_identifier);
	ClassDB::bind_method(D_METHOD("get_mount_path"), &GDKPackageMount::get_mount_path);
	ClassDB::bind_method(D_METHOD("get_package_details"), &GDKPackageMount::get_package_details);
	ClassDB::bind_method(D_METHOD("is_valid"), &GDKPackageMount::is_valid);
	ClassDB::bind_method(D_METHOD("resolve_path", "relative_path"), &GDKPackageMount::resolve_path);
	ClassDB::bind_method(D_METHOD("close"), &GDKPackageMount::close);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "package_identifier"), "", "get_package_identifier");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "mount_path"), "", "get_mount_path");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "package_details"), "", "get_package_details");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "valid"), "", "is_valid");
}

GDKPackageMount::~GDKPackageMount() {
	_close_handle();
}

#ifdef XBOX_MODULE_GDK_ENABLED
void GDKPackageMount::_close_handle() {
	if (m_mount_handle != nullptr) {
		XPackageCloseMountHandle(m_mount_handle);
		m_mount_handle = nullptr;
	}
	m_valid = false;
}

void GDKPackageMount::initialize(const String &p_package_identifier, const String &p_mount_path, const Dictionary &p_package_details, XPackageMountHandle p_mount_handle) {
	_close_handle();
	m_package_identifier = p_package_identifier;
	m_mount_path = p_mount_path;
	m_package_details = p_package_details.duplicate(true);
	m_mount_handle = p_mount_handle;
	m_valid = m_mount_handle != nullptr;
}

XPackageMountHandle GDKPackageMount::release_handle() {
	XPackageMountHandle handle = m_mount_handle;
	m_mount_handle = nullptr;
	m_valid = false;
	return handle;
}
#else
void GDKPackageMount::_close_handle() {
	m_mount_handle = nullptr;
	m_valid = false;
}

void GDKPackageMount::initialize(const String &p_package_identifier, const String &p_mount_path, const Dictionary &p_package_details, XPackageMountHandle p_mount_handle) {
	_close_handle();
	m_package_identifier = p_package_identifier;
	m_mount_path = p_mount_path;
	m_package_details = p_package_details.duplicate(true);
	m_mount_handle = p_mount_handle;
	m_valid = m_mount_handle != nullptr;
}

XPackageMountHandle GDKPackageMount::release_handle() {
	XPackageMountHandle handle = m_mount_handle;
	m_mount_handle = nullptr;
	m_valid = false;
	return handle;
}
#endif

String GDKPackageMount::get_package_identifier() const {
	return m_package_identifier;
}

String GDKPackageMount::get_mount_path() const {
	return m_mount_path;
}

Dictionary GDKPackageMount::get_package_details() const {
	return m_package_details.duplicate(true);
}

bool GDKPackageMount::is_valid() const {
	return m_valid && m_mount_handle != nullptr;
}

#ifdef XBOX_MODULE_GDK_ENABLED
Ref<GDKResult> GDKPackageMount::resolve_path(const String &p_relative_path) const {
	if (!is_valid()) {
		return GDKResult::error_result(E_FAIL, "package_mount_invalid", "Package mount is no longer valid.");
	}

	return resolve_under_mount_path(m_mount_path, p_relative_path);
}
#else
Ref<GDKResult> GDKPackageMount::resolve_path(const String &p_relative_path) const {
	if (!is_valid()) {
		return GDKResult::error_result(E_FAIL, "package_mount_invalid", "Package mount is no longer valid.");
	}
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_available", "GDK package services are not available.");
}
#endif

Ref<GDKResult> GDKPackageMount::close() {
	_close_handle();
	return GDKResult::ok_result();
}

void GDKPackageResourcePack::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_package_identifier"), &GDKPackageResourcePack::get_package_identifier);
	ClassDB::bind_method(D_METHOD("get_mount_path"), &GDKPackageResourcePack::get_mount_path);
	ClassDB::bind_method(D_METHOD("get_pack_relative_path"), &GDKPackageResourcePack::get_pack_relative_path);
	ClassDB::bind_method(D_METHOD("get_pack_path"), &GDKPackageResourcePack::get_pack_path);
	ClassDB::bind_method(D_METHOD("get_package_details"), &GDKPackageResourcePack::get_package_details);
	ClassDB::bind_method(D_METHOD("get_replace_files"), &GDKPackageResourcePack::get_replace_files);
	ClassDB::bind_method(D_METHOD("get_offset"), &GDKPackageResourcePack::get_offset);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "package_identifier"), "", "get_package_identifier");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "mount_path"), "", "get_mount_path");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "pack_relative_path"), "", "get_pack_relative_path");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "pack_path"), "", "get_pack_path");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "package_details"), "", "get_package_details");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "replace_files"), "", "get_replace_files");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "offset"), "", "get_offset");
}

void GDKPackageResourcePack::initialize(
		const String &p_package_identifier,
		const String &p_mount_path,
		const String &p_pack_relative_path,
		const String &p_pack_path,
		const Dictionary &p_package_details,
		bool p_replace_files,
		int64_t p_offset) {
	m_package_identifier = p_package_identifier;
	m_mount_path = p_mount_path;
	m_pack_relative_path = p_pack_relative_path;
	m_pack_path = p_pack_path;
	m_package_details = p_package_details.duplicate(true);
	m_replace_files = p_replace_files;
	m_offset = p_offset;
}

String GDKPackageResourcePack::get_package_identifier() const {
	return m_package_identifier;
}

String GDKPackageResourcePack::get_mount_path() const {
	return m_mount_path;
}

String GDKPackageResourcePack::get_pack_relative_path() const {
	return m_pack_relative_path;
}

String GDKPackageResourcePack::get_pack_path() const {
	return m_pack_path;
}

Dictionary GDKPackageResourcePack::get_package_details() const {
	return m_package_details.duplicate(true);
}

bool GDKPackageResourcePack::get_replace_files() const {
	return m_replace_files;
}

int64_t GDKPackageResourcePack::get_offset() const {
	return m_offset;
}

void GDKPackage::_bind_methods() {
	ClassDB::bind_method(D_METHOD("enumerate_packages", "package_kind", "scope"), &GDKPackage::enumerate_packages, DEFVAL(static_cast<int64_t>(PACKAGE_KIND_CONTENT)), DEFVAL(static_cast<int64_t>(ENUMERATION_SCOPE_THIS_AND_RELATED)));
	ClassDB::bind_method(D_METHOD("find_package_by_identifier", "package_identifier", "package_kind", "scope"), &GDKPackage::find_package_by_identifier, DEFVAL(static_cast<int64_t>(PACKAGE_KIND_CONTENT)), DEFVAL(static_cast<int64_t>(ENUMERATION_SCOPE_THIS_AND_RELATED)));
	ClassDB::bind_method(D_METHOD("get_current_process_package_identifier"), &GDKPackage::get_current_process_package_identifier);
	ClassDB::bind_method(D_METHOD("mount_package_async", "package_identifier"), &GDKPackage::mount_package_async);
	ClassDB::bind_method(D_METHOD("load_resource_pack_async", "package_identifier", "pack_relative_path", "replace_files", "offset"), &GDKPackage::load_resource_pack_async, DEFVAL(false), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_loaded_resource_packs"), &GDKPackage::get_loaded_resource_packs);
	ClassDB::bind_method(D_METHOD("get_install_progress", "package_identifier"), &GDKPackage::get_install_progress);

	BIND_ENUM_CONSTANT(PACKAGE_KIND_GAME);
	BIND_ENUM_CONSTANT(PACKAGE_KIND_CONTENT);
	BIND_ENUM_CONSTANT(ENUMERATION_SCOPE_THIS_PUBLISHER);
	BIND_ENUM_CONSTANT(ENUMERATION_SCOPE_THIS_AND_RELATED);
}

#ifdef XBOX_MODULE_GDK_ENABLED
void GDKPackage::set_owner(GDK *p_owner) {
	m_owner = p_owner;
}

Ref<GDKResult> GDKPackage::on_runtime_initialized() {
	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr || !runtime->is_initialized()) {
		return GDKResult::error_result(E_FAIL, "runtime_not_initialized", "Cannot initialize the package service before the GDK runtime.");
	}

	m_runtime_ready = true;
	return GDKResult::ok_result();
}

void GDKPackage::shutdown() {
	m_runtime_ready = false;
	_clear_loaded_resource_packs();
}

Ref<GDKResult> GDKPackage::enumerate_packages(int64_t p_package_kind, int64_t p_scope) {
	Ref<GDKResult> runtime_ready = _ensure_runtime_ready();
	if (!runtime_ready->is_ok()) {
		return runtime_ready;
	}

	XPackageKind kind = XPackageKind::Content;
	Ref<GDKResult> kind_result = parse_package_kind(p_package_kind, &kind);
	if (!kind_result->is_ok()) {
		return kind_result;
	}

	XPackageEnumerationScope scope = XPackageEnumerationScope::ThisAndRelated;
	Ref<GDKResult> scope_result = parse_package_scope(p_scope, &scope);
	if (!scope_result->is_ok()) {
		return scope_result;
	}

	Array packages;
	PackageEnumerationContext context;
	context.packages = &packages;

	HRESULT hr = XPackageEnumeratePackages(
			kind,
			scope,
			&context,
			[](void *p_context, const XPackageDetails *p_details) -> bool {
				if (p_context == nullptr || p_details == nullptr) {
					return true;
				}

				auto *context = static_cast<PackageEnumerationContext *>(p_context);
				Dictionary package = package_details_to_dictionary(*p_details);
				if (context->packages != nullptr) {
					context->packages->push_back(package);
				}

				if (context->find_target) {
					String identifier = package["package_identifier"];
					if (identifier == context->target_identifier) {
						context->found_target = true;
						context->found_package = package;
					}
				}

				return true;
			});
	if (FAILED(hr)) {
		return GDKResult::hresult_error(hr, "Failed to enumerate installed packages.", "package_enumeration_failed");
	}

	return GDKResult::ok_result(packages);
}

Ref<GDKResult> GDKPackage::find_package_by_identifier(const String &p_package_identifier, int64_t p_package_kind, int64_t p_scope) {
	Ref<GDKResult> id_result = validate_package_identifier(p_package_identifier);
	if (!id_result->is_ok()) {
		return id_result;
	}
	String package_identifier = static_cast<String>(id_result->get_data());

	Ref<GDKResult> runtime_ready = _ensure_runtime_ready();
	if (!runtime_ready->is_ok()) {
		return runtime_ready;
	}

	XPackageKind kind = XPackageKind::Content;
	Ref<GDKResult> kind_result = parse_package_kind(p_package_kind, &kind);
	if (!kind_result->is_ok()) {
		return kind_result;
	}

	XPackageEnumerationScope scope = XPackageEnumerationScope::ThisAndRelated;
	Ref<GDKResult> scope_result = parse_package_scope(p_scope, &scope);
	if (!scope_result->is_ok()) {
		return scope_result;
	}

	PackageEnumerationContext context;
	context.find_target = true;
	context.target_identifier = package_identifier;

	HRESULT hr = XPackageEnumeratePackages(
			kind,
			scope,
			&context,
			[](void *p_context, const XPackageDetails *p_details) -> bool {
				if (p_context == nullptr || p_details == nullptr) {
					return true;
				}

				auto *context = static_cast<PackageEnumerationContext *>(p_context);
				if (!context->find_target || context->found_target) {
					return true;
				}

				Dictionary package = package_details_to_dictionary(*p_details);
				String identifier = package["package_identifier"];
				if (identifier == context->target_identifier) {
					context->found_target = true;
					context->found_package = package;
				}

				return true;
			});
	if (FAILED(hr)) {
		return GDKResult::hresult_error(hr, "Failed to enumerate installed packages.", "package_enumeration_failed");
	}
	if (!context.found_target) {
		return GDKResult::error_result(E_FAIL, "package_not_found", "No installed content package matched the specified package identifier.");
	}

	return GDKResult::ok_result(context.found_package);
}

Ref<GDKResult> GDKPackage::get_current_process_package_identifier() {
	Ref<GDKResult> runtime_ready = _ensure_runtime_ready();
	if (!runtime_ready->is_ok()) {
		return runtime_ready;
	}

	char package_identifier[XPACKAGE_IDENTIFIER_MAX_LENGTH] = {};
	HRESULT hr = XPackageGetCurrentProcessPackageIdentifier(ARRAYSIZE(package_identifier), package_identifier);
	if (FAILED(hr)) {
		return GDKResult::hresult_error(
				hr,
				"Failed to resolve the current process package identifier. Ensure the title is running as a registered package or loose layout.",
				"package_identifier_unavailable");
	}

	String identifier = String::utf8(package_identifier).strip_edges();
	if (identifier.is_empty()) {
		return GDKResult::error_result(E_FAIL, "package_identifier_unavailable", "The current process package identifier was empty.");
	}

	return GDKResult::ok_result(identifier);
}

Signal GDKPackage::mount_package_async(const String &p_package_identifier) {
	Signal signal;
	Ref<GDKResult> start_result = _start_mount_async(p_package_identifier, false, "", false, 0, &signal);
	if (!start_result->is_ok()) {
		return _make_completed_signal(start_result);
	}
	return signal;
}

Signal GDKPackage::load_resource_pack_async(const String &p_package_identifier, const String &p_pack_relative_path, bool p_replace_files, int64_t p_offset) {
	String normalized_path;
	Ref<GDKResult> path_result = normalize_package_relative_path(p_pack_relative_path, &normalized_path);
	if (!path_result->is_ok()) {
		return _make_completed_signal(path_result);
	}

	if (p_offset < 0) {
		return _make_error_signal(E_INVALIDARG, "invalid_package_offset", "Resource pack offset cannot be negative.");
	}

	String extension = normalized_path.get_extension().to_lower();
	if (extension != "pck" && extension != "zip") {
		return _make_error_signal(E_INVALIDARG, "invalid_resource_pack", "Resource pack path must point to a .pck or .zip file.");
	}

	Ref<GDKResult> id_result = validate_package_identifier(p_package_identifier);
	if (!id_result->is_ok()) {
		return _make_completed_signal(id_result);
	}
	String package_identifier = static_cast<String>(id_result->get_data());

	LoadedResourcePack *loaded = _find_loaded_resource_pack(package_identifier, normalized_path);
	if (loaded != nullptr && loaded->resource_pack.is_valid()) {
		Dictionary data;
		data["resource_pack"] = loaded->resource_pack;
		data["already_loaded"] = true;
		return _make_completed_signal(GDKResult::ok_result(data));
	}

	Signal signal;
	Ref<GDKResult> start_result = _start_mount_async(package_identifier, true, normalized_path, p_replace_files, p_offset, &signal);
	if (!start_result->is_ok()) {
		return _make_completed_signal(start_result);
	}
	return signal;
}

Array GDKPackage::get_loaded_resource_packs() const {
	Array packs;
	for (const LoadedResourcePack &loaded : m_loaded_resource_packs) {
		if (loaded.resource_pack.is_valid()) {
			packs.push_back(loaded.resource_pack);
		}
	}
	return packs;
}

Ref<GDKResult> GDKPackage::get_install_progress(const String &p_package_identifier) {
	Ref<GDKResult> id_result = validate_package_identifier(p_package_identifier);
	if (!id_result->is_ok()) {
		return id_result;
	}
	String package_identifier = static_cast<String>(id_result->get_data());

	Ref<GDKResult> package_result = find_package_by_identifier(package_identifier);
	if (!package_result->is_ok()) {
		return package_result;
	}

	GDKRuntime *runtime = _get_runtime();
	ERR_FAIL_COND_V(runtime == nullptr, GDKResult::error_result(E_FAIL, "not_initialized", "GDK runtime is not available."));

	CharString identifier_utf8 = package_identifier.utf8();
	XPackageInstallationMonitorHandle monitor = nullptr;
	HRESULT hr = XPackageCreateInstallationMonitor(
			identifier_utf8.get_data(),
			0,
			nullptr,
			1000,
			runtime->get_task_queue(),
			&monitor);
	if (FAILED(hr)) {
		return GDKResult::hresult_error(hr, "Failed to create a package installation monitor.", "package_install_monitor_create_failed");
	}

	XPackageInstallationProgress progress = {};
	XPackageGetInstallationProgress(monitor, &progress);
	XPackageCloseInstallationMonitorHandle(monitor);

	Dictionary data;
	data["completed"] = progress.completed;
	data["installed_bytes"] = to_variant_u64(progress.installedBytes);
	data["total_bytes"] = to_variant_u64(progress.totalBytes);
	data["total_bytes_unknown"] = progress.totalBytes == UINT64_MAX;

	double progress_ratio = 0.0;
	if (progress.totalBytes > 0 && progress.totalBytes != UINT64_MAX) {
		progress_ratio = static_cast<double>(progress.installedBytes) / static_cast<double>(progress.totalBytes);
	} else if (progress.completed) {
		progress_ratio = 1.0;
	}
	data["progress_ratio"] = progress_ratio;

	return GDKResult::ok_result(data);
}

GDKRuntime *GDKPackage::_get_runtime() const {
	return m_owner != nullptr ? m_owner->get_runtime() : nullptr;
}

Ref<GDKResult> GDKPackage::_ensure_runtime_ready() const {
	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr || !runtime->is_initialized() || !m_runtime_ready) {
		return GDKResult::error_result(E_FAIL, "not_initialized", "GDK is not initialized. Call GDK.initialize() first.");
	}

	return GDKResult::ok_result();
}

Signal GDKPackage::_make_completed_signal(const Ref<GDKResult> &p_result) const {
	GDKRuntime *runtime = _get_runtime();
	if (runtime != nullptr && p_result.is_valid() && !p_result->is_ok()) {
	}
	Ref<GDKPendingSignal> pending_signal = runtime != nullptr ? runtime->make_pending_signal() : Ref<GDKPendingSignal>();
	if (pending_signal.is_null()) {
		pending_signal.instantiate();
	}
	pending_signal->complete_deferred(p_result);
	return pending_signal->get_completed_signal();
}

Signal GDKPackage::_make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data) const {
	Ref<GDKResult> result = GDKResult::error_result(p_hresult, p_code, p_message, p_data);
	GDKRuntime *runtime = _get_runtime();
	if (runtime != nullptr) {
	}
	return _make_completed_signal(result);
}

Ref<GDKResult> GDKPackage::_start_mount_async(const String &p_package_identifier, bool p_load_resource_pack, const String &p_pack_relative_path, bool p_replace_files, int64_t p_offset, Signal *r_signal) {
	ERR_FAIL_COND_V(r_signal == nullptr, GDKResult::error_result(E_POINTER, "internal_error", "Missing mount signal output."));

	Ref<GDKResult> runtime_ready = _ensure_runtime_ready();
	if (!runtime_ready->is_ok()) {
		return runtime_ready;
	}

	Ref<GDKResult> id_result = validate_package_identifier(p_package_identifier);
	if (!id_result->is_ok()) {
		return id_result;
	}
	String package_identifier = static_cast<String>(id_result->get_data());

	Ref<GDKResult> package_result = find_package_by_identifier(package_identifier);
	if (!package_result->is_ok()) {
		return package_result;
	}
	Dictionary package_details = static_cast<Dictionary>(package_result->get_data());

	int64_t kind = static_cast<int64_t>(package_details.get("kind", static_cast<int64_t>(PACKAGE_KIND_CONTENT)));
	if (kind != static_cast<int64_t>(PACKAGE_KIND_CONTENT)) {
		return GDKResult::error_result(E_ACCESSDENIED, "package_not_content", "Only content packages can be mounted for DLC asset access.");
	}

	GDKRuntime *runtime = _get_runtime();
	Ref<GDKPendingSignal> pending_signal = runtime->make_pending_signal();
	auto *context = new PackageMountAsyncContext(
			runtime,
			pending_signal,
			this,
			package_identifier,
			package_details,
			p_load_resource_pack,
			p_pack_relative_path,
			p_replace_files,
			p_offset);

	HRESULT hr = XPackageMountWithUiAsync(context->get_package_identifier_data(), context->get_async_block());
	if (FAILED(hr)) {
		delete context;
		return GDKResult::hresult_error(hr, "Failed to start package mounting.", "package_mount_start_failed");
	}

	context->bind_cancel_handler();
	*r_signal = pending_signal->get_completed_signal();
	return GDKResult::ok_result();
}

Ref<GDKResult> GDKPackage::finish_resource_pack_load(
		const String &p_package_identifier,
		const Dictionary &p_package_details,
		const String &p_mount_path,
		XPackageMountHandle p_mount_handle,
		const String &p_pack_relative_path,
		bool p_replace_files,
		int64_t p_offset) {
	String normalized_pack_path;
	Ref<GDKResult> path_result = normalize_package_relative_path(p_pack_relative_path, &normalized_pack_path);
	if (!path_result->is_ok()) {
		XPackageCloseMountHandle(p_mount_handle);
		return path_result;
	}

	Ref<GDKResult> absolute_pack_result = resolve_under_mount_path(p_mount_path, normalized_pack_path);
	if (!absolute_pack_result->is_ok()) {
		XPackageCloseMountHandle(p_mount_handle);
		return absolute_pack_result;
	}
	String absolute_pack_path = static_cast<String>(absolute_pack_result->get_data());

	if (!FileAccess::exists(absolute_pack_path)) {
		XPackageCloseMountHandle(p_mount_handle);
		return GDKResult::error_result(E_FAIL, "resource_pack_not_found", "Resource pack file was not found inside the mounted package.");
	}

	PackedData *packed_data = PackedData::get_singleton();
	if (packed_data == nullptr) {
		XPackageCloseMountHandle(p_mount_handle);
		return GDKResult::error_result(E_FAIL, "packed_data_unavailable", "PackedData singleton is unavailable.");
	}

	Error pack_err = packed_data->add_pack(absolute_pack_path, p_replace_files, static_cast<uint64_t>(p_offset));
	if (pack_err != OK) {
		XPackageCloseMountHandle(p_mount_handle);
		return GDKResult::error_result(E_FAIL, "resource_pack_load_failed", "Godot failed to load the package resource pack.");
	}

	Ref<GDKPackageResourcePack> resource_pack;
	resource_pack.instantiate();
	resource_pack->initialize(
			p_package_identifier,
			p_mount_path,
			normalized_pack_path,
			absolute_pack_path,
			p_package_details,
			p_replace_files,
			p_offset);

	LoadedResourcePack loaded_pack;
	loaded_pack.package_identifier = p_package_identifier;
	loaded_pack.pack_relative_path = normalized_pack_path;
	loaded_pack.mount_handle = p_mount_handle;
	loaded_pack.resource_pack = resource_pack;
	m_loaded_resource_packs.push_back(loaded_pack);

	Dictionary data;
	data["resource_pack"] = resource_pack;
	data["already_loaded"] = false;
	return GDKResult::ok_result(data);
}

GDKPackage::LoadedResourcePack *GDKPackage::_find_loaded_resource_pack(const String &p_package_identifier, const String &p_pack_relative_path) {
	for (LoadedResourcePack &loaded : m_loaded_resource_packs) {
		if (loaded.package_identifier == p_package_identifier && loaded.pack_relative_path == p_pack_relative_path) {
			return &loaded;
		}
	}
	return nullptr;
}

void GDKPackage::_clear_loaded_resource_packs() {
	for (LoadedResourcePack &loaded : m_loaded_resource_packs) {
		if (loaded.mount_handle != nullptr) {
			XPackageCloseMountHandle(loaded.mount_handle);
			loaded.mount_handle = nullptr;
		}
	}
	m_loaded_resource_packs.clear();
}

#else
void GDKPackage::set_owner(GDK *p_owner) {}

Ref<GDKResult> GDKPackage::on_runtime_initialized() {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

void GDKPackage::shutdown() {}

Ref<GDKResult> GDKPackage::enumerate_packages(int64_t p_package_kind, int64_t p_scope) {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Ref<GDKResult> GDKPackage::find_package_by_identifier(const String &p_package_identifier, int64_t p_package_kind, int64_t p_scope) {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Ref<GDKResult> GDKPackage::get_current_process_package_identifier() {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Signal GDKPackage::mount_package_async(const String &p_package_identifier) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKPackage::load_resource_pack_async(const String &p_package_identifier, const String &p_pack_relative_path, bool p_replace_files, int64_t p_offset) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Array GDKPackage::get_loaded_resource_packs() const {
	return Array();
}

Ref<GDKResult> GDKPackage::get_install_progress(const String &p_package_identifier) {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Ref<GDKResult> GDKPackage::_ensure_runtime_ready() const {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Signal GDKPackage::_make_completed_signal(const Ref<GDKResult> &p_result) const {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKPackage::_make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data) const {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Ref<GDKResult> GDKPackage::_start_mount_async(const String &p_package_identifier, bool p_load_resource_pack, const String &p_pack_relative_path, bool p_replace_files, int64_t p_offset, Signal *r_signal) {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Ref<GDKResult> GDKPackage::finish_resource_pack_load(
		const String &p_package_identifier,
		const Dictionary &p_package_details,
		const String &p_mount_path,
		XPackageMountHandle p_mount_handle,
		const String &p_pack_relative_path,
		bool p_replace_files,
		int64_t p_offset) {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

void GDKPackage::_clear_loaded_resource_packs() {}
#endif
