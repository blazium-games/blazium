/**************************************************************************/
/*  gdk_title_storage.cpp                                                 */
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
#include "gdk_title_storage.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "gdk_windows.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
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
	return p_value != nullptr && p_value[0] != '\0' ? String::utf8(p_value) : String();
}

String _normalize_token(const String &p_value) {
	return p_value.strip_edges().to_lower().replace("-", "_").replace(" ", "_");
}

bool _try_parse_xuid(const String &p_xuid, uint64_t *r_xuid) {
	if (r_xuid == nullptr) {
		return false;
	}

	const String normalized = p_xuid.strip_edges();
	if (normalized.is_empty()) {
		return false;
	}

	const CharString utf8 = normalized.utf8();
	char *end_ptr = nullptr;
	errno = 0;
	const unsigned long long parsed = std::strtoull(utf8.get_data(), &end_ptr, 10);
	if (errno != 0 || end_ptr == nullptr || *end_ptr != '\0') {
		return false;
	}

	*r_xuid = static_cast<uint64_t>(parsed);
	return true;
}

bool _try_parse_storage_type(const String &p_storage_type, XblTitleStorageType *r_storage_type) {
	if (r_storage_type == nullptr) {
		return false;
	}

	const String token = _normalize_token(p_storage_type);
	if (token == "trusted_platform" || token == "trusted_platform_storage" || token == "trustedplatform") {
		*r_storage_type = XblTitleStorageType::TrustedPlatformStorage;
	} else if (token == "global" || token == "global_storage") {
		*r_storage_type = XblTitleStorageType::GlobalStorage;
	} else if (token == "universal") {
		*r_storage_type = XblTitleStorageType::Universal;
	} else {
		return false;
	}

	return true;
}

String _storage_type_to_string(XblTitleStorageType p_storage_type) {
	switch (p_storage_type) {
		case XblTitleStorageType::TrustedPlatformStorage:
			return "trusted_platform";
		case XblTitleStorageType::GlobalStorage:
			return "global";
		case XblTitleStorageType::Universal:
			return "universal";
		default:
			return "unknown";
	}
}

String _blob_type_to_string(XblTitleStorageBlobType p_blob_type) {
	switch (p_blob_type) {
		case XblTitleStorageBlobType::Binary:
			return "binary";
		case XblTitleStorageBlobType::Json:
			return "json";
		case XblTitleStorageBlobType::Config:
			return "config";
		case XblTitleStorageBlobType::Unknown:
		default:
			return "unknown";
	}
}

bool _try_parse_match_condition(const String &p_match_condition, XblTitleStorageETagMatchCondition *r_match_condition) {
	if (r_match_condition == nullptr) {
		return false;
	}

	const String token = _normalize_token(p_match_condition);
	if (token == "not_used" || token == "none") {
		*r_match_condition = XblTitleStorageETagMatchCondition::NotUsed;
	} else if (token == "if_match") {
		*r_match_condition = XblTitleStorageETagMatchCondition::IfMatch;
	} else if (token == "if_not_match") {
		*r_match_condition = XblTitleStorageETagMatchCondition::IfNotMatch;
	} else {
		return false;
	}

	return true;
}

Ref<GDKResult> _parse_uint32(const String &p_name, int64_t p_value, uint32_t *r_value) {
	if (r_value == nullptr) {
		return GDKResult::error_result(E_POINTER, "invalid_output", "Output storage is unavailable.");
	}
	if (p_value < 0 || p_value > static_cast<int64_t>(UINT32_MAX)) {
		return GDKResult::error_result(E_INVALIDARG, String("invalid_") + p_name, p_name + String(" must fit in a non-negative 32-bit unsigned integer."));
	}

	*r_value = static_cast<uint32_t>(p_value);
	return GDKResult::ok_result();
}

Ref<GDKResult> _copy_utf8_to_buffer(const String &p_value, char *p_buffer, size_t p_buffer_size, const String &p_field_name) {
	if (p_buffer == nullptr || p_buffer_size == 0) {
		return GDKResult::error_result(E_POINTER, "invalid_output", "String output storage is unavailable.");
	}

	const CharString utf8 = p_value.utf8();
	const size_t length = std::strlen(utf8.get_data());
	if (length >= p_buffer_size) {
		return GDKResult::error_result(E_INVALIDARG, String("invalid_") + p_field_name, p_field_name + String(" is too long."));
	}

	std::memset(p_buffer, 0, p_buffer_size);
	std::memcpy(p_buffer, utf8.get_data(), length);
	return GDKResult::ok_result();
}

Ref<GDKResult> _make_blob_metadata(
		const String &p_scid,
		XblTitleStorageType p_storage_type,
		uint64_t p_xbox_user_id,
		const String &p_blob_path,
		const String &p_display_name,
		const String &p_e_tag,
		size_t p_length,
		XblTitleStorageBlobType p_blob_type,
		XblTitleStorageBlobMetadata *r_metadata) {
	if (r_metadata == nullptr) {
		return GDKResult::error_result(E_POINTER, "invalid_output", "Blob metadata output storage is unavailable.");
	}

	const String blob_path = p_blob_path.strip_edges();
	if (blob_path.is_empty()) {
		return GDKResult::error_result(E_INVALIDARG, "invalid_blob_path", "blob_path must be a non-empty string.");
	}

	XblTitleStorageBlobMetadata metadata = {};
	metadata.storageType = p_storage_type;
	metadata.blobType = p_blob_type;
	metadata.length = p_length;
	metadata.xboxUserId = p_storage_type == XblTitleStorageType::GlobalStorage ? 0 : p_xbox_user_id;

	Ref<GDKResult> copy_result = _copy_utf8_to_buffer(p_scid, metadata.serviceConfigurationId, sizeof(metadata.serviceConfigurationId), "service_configuration_id");
	if (!copy_result->is_ok()) {
		return copy_result;
	}
	copy_result = _copy_utf8_to_buffer(blob_path, metadata.blobPath, sizeof(metadata.blobPath), "blob_path");
	if (!copy_result->is_ok()) {
		return copy_result;
	}
	copy_result = _copy_utf8_to_buffer(p_display_name, metadata.displayName, sizeof(metadata.displayName), "display_name");
	if (!copy_result->is_ok()) {
		return copy_result;
	}
	copy_result = _copy_utf8_to_buffer(p_e_tag, metadata.eTag, sizeof(metadata.eTag), "e_tag");
	if (!copy_result->is_ok()) {
		return copy_result;
	}

	*r_metadata = metadata;
	return GDKResult::ok_result();
}

Ref<GDKTitleStorageBlobMetadata> _make_metadata_ref(const XblTitleStorageBlobMetadata &p_metadata) {
	Ref<GDKTitleStorageBlobMetadata> metadata_ref;
	metadata_ref.instantiate();
	metadata_ref->populate_from_native(p_metadata);
	return metadata_ref;
}

PackedByteArray _make_packed_byte_array(const std::vector<uint8_t> &p_data) {
	PackedByteArray result;
	result.resize(static_cast<int64_t>(p_data.size()));
	for (int64_t i = 0; i < result.size(); ++i) {
		result.set(i, p_data[static_cast<size_t>(i)]);
	}
	return result;
}

std::vector<uint8_t> _make_byte_vector(const PackedByteArray &p_data) {
	std::vector<uint8_t> result;
	result.reserve(static_cast<size_t>(p_data.size()));
	for (int64_t i = 0; i < p_data.size(); ++i) {
		result.push_back(static_cast<uint8_t>(p_data[i]));
	}
	return result;
}

const XblTitleStorageBlobMetadata *_find_exact_metadata(const XblTitleStorageBlobMetadata *p_items, size_t p_count, const String &p_blob_path) {
	if (p_items == nullptr) {
		return nullptr;
	}

	const String blob_path = p_blob_path.strip_edges();
	for (size_t i = 0; i < p_count; ++i) {
		if (_utf8_or_empty(p_items[i].blobPath) == blob_path) {
			return &p_items[i];
		}
	}
	return nullptr;
}

class QuotaAsyncContext final : public GDKSignalXAsyncContext {
	XblContextHandle m_context = nullptr;
	String m_storage_type;

protected:
	void finalize(XAsyncBlock *p_async_block) override {
		if (get_runtime()->is_shutting_down() || get_pending_signal()->was_cancel_requested()) {
			Ref<GDKResult> result = GDKResult::cancelled("Title Storage quota query cancelled.");
			get_pending_signal()->complete(result);
			return;
		}

		size_t used_bytes = 0;
		size_t quota_bytes = 0;
		HRESULT result_hr = XblTitleStorageGetQuotaResult(p_async_block, &used_bytes, &quota_bytes);
		if (result_hr == E_ABORT) {
			Ref<GDKResult> result = GDKResult::cancelled("Title Storage quota query cancelled.");
			get_pending_signal()->complete(result);
			return;
		}
		if (FAILED(result_hr)) {
			Ref<GDKResult> result = GDKResult::hresult_error(result_hr, "Failed to retrieve Title Storage quota.", "title_storage_quota_result_failed");
			get_pending_signal()->complete(result);
			return;
		}

		Dictionary payload;
		payload["storage_type"] = m_storage_type;
		payload["used_bytes"] = static_cast<int64_t>(used_bytes);
		payload["quota_bytes"] = static_cast<int64_t>(quota_bytes);
		get_pending_signal()->complete(GDKResult::ok_result(payload));
	}

public:
	QuotaAsyncContext(GDKRuntime *p_runtime, const Ref<GDKPendingSignal> &p_pending_signal, XblContextHandle p_context, const String &p_storage_type) :
			GDKSignalXAsyncContext(p_runtime, p_pending_signal),
			m_context(p_context),
			m_storage_type(p_storage_type) {}

	~QuotaAsyncContext() override {
		if (m_context != nullptr) {
			XblContextCloseHandle(m_context);
			m_context = nullptr;
		}
	}

	XblContextHandle get_context() const {
		return m_context;
	}
};

class BlobMetadataAsyncContext final : public GDKSignalXAsyncContext {
	Ref<GDKUser> m_user;
	XblContextHandle m_context = nullptr;
	String m_storage_type;
	String m_blob_path;
	uint32_t m_max_items = 0;
	bool m_next_page = false;
	XblTitleStorageBlobMetadataResultHandle m_previous_handle = nullptr;

protected:
	void finalize(XAsyncBlock *p_async_block) override {
		if (get_runtime()->is_shutting_down() || get_pending_signal()->was_cancel_requested()) {
			Ref<GDKResult> result = GDKResult::cancelled("Title Storage metadata query cancelled.");
			get_pending_signal()->complete(result);
			return;
		}

		XblTitleStorageBlobMetadataResultHandle handle = nullptr;
		HRESULT result_hr = m_next_page ? XblTitleStorageBlobMetadataResultGetNextResult(p_async_block, &handle) : XblTitleStorageGetBlobMetadataResult(p_async_block, &handle);
		if (result_hr == E_ABORT) {
			Ref<GDKResult> result = GDKResult::cancelled("Title Storage metadata query cancelled.");
			get_pending_signal()->complete(result);
			return;
		}
		if (FAILED(result_hr)) {
			Ref<GDKResult> result = GDKResult::hresult_error(result_hr, "Failed to retrieve Title Storage metadata.", "title_storage_metadata_result_failed");
			get_pending_signal()->complete(result);
			return;
		}

		Ref<GDKTitleStorageBlobMetadataResult> metadata_result;
		metadata_result.instantiate();
		result_hr = metadata_result->populate_from_handle(m_user, m_storage_type, m_blob_path, m_max_items, handle);
		if (FAILED(result_hr)) {
			if (handle != nullptr) {
				XblTitleStorageBlobMetadataResultCloseHandle(handle);
			}
			Ref<GDKResult> result = GDKResult::hresult_error(result_hr, "Failed to translate Title Storage metadata.", "title_storage_metadata_translate_failed");
			get_pending_signal()->complete(result);
			return;
		}

		get_pending_signal()->complete(GDKResult::ok_result(metadata_result));
	}

public:
	BlobMetadataAsyncContext(
			GDKRuntime *p_runtime,
			const Ref<GDKPendingSignal> &p_pending_signal,
			const Ref<GDKUser> &p_user,
			XblContextHandle p_context,
			const String &p_storage_type,
			const String &p_blob_path,
			uint32_t p_max_items,
			bool p_next_page,
			XblTitleStorageBlobMetadataResultHandle p_previous_handle = nullptr) :
			GDKSignalXAsyncContext(p_runtime, p_pending_signal),
			m_user(p_user),
			m_context(p_context),
			m_storage_type(p_storage_type),
			m_blob_path(p_blob_path),
			m_max_items(p_max_items),
			m_next_page(p_next_page),
			m_previous_handle(p_previous_handle) {}

	~BlobMetadataAsyncContext() override {
		if (m_context != nullptr) {
			XblContextCloseHandle(m_context);
			m_context = nullptr;
		}
		if (m_previous_handle != nullptr) {
			XblTitleStorageBlobMetadataResultCloseHandle(m_previous_handle);
			m_previous_handle = nullptr;
		}
	}

	XblContextHandle get_context() const {
		return m_context;
	}

	XblTitleStorageBlobMetadataResultHandle get_previous_handle() const {
		return m_previous_handle;
	}
};

class UploadBlobAsyncContext final : public GDKSignalXAsyncContext {
	XblContextHandle m_context = nullptr;
	XblTitleStorageBlobMetadata m_metadata = {};
	std::vector<uint8_t> m_data;

protected:
	void finalize(XAsyncBlock *p_async_block) override {
		if (get_runtime()->is_shutting_down() || get_pending_signal()->was_cancel_requested()) {
			Ref<GDKResult> result = GDKResult::cancelled("Title Storage upload cancelled.");
			get_pending_signal()->complete(result);
			return;
		}

		XblTitleStorageBlobMetadata native_metadata = {};
		HRESULT result_hr = XblTitleStorageUploadBlobResult(p_async_block, &native_metadata);
		if (result_hr == E_ABORT) {
			Ref<GDKResult> result = GDKResult::cancelled("Title Storage upload cancelled.");
			get_pending_signal()->complete(result);
			return;
		}
		if (FAILED(result_hr)) {
			Ref<GDKResult> result = GDKResult::hresult_error(result_hr, "Failed to upload Title Storage blob.", "title_storage_upload_result_failed");
			get_pending_signal()->complete(result);
			return;
		}

		get_pending_signal()->complete(GDKResult::ok_result(_make_metadata_ref(native_metadata)));
	}

public:
	UploadBlobAsyncContext(GDKRuntime *p_runtime, const Ref<GDKPendingSignal> &p_pending_signal, XblContextHandle p_context, const XblTitleStorageBlobMetadata &p_metadata, std::vector<uint8_t> p_data) :
			GDKSignalXAsyncContext(p_runtime, p_pending_signal),
			m_context(p_context),
			m_metadata(p_metadata),
			m_data(std::move(p_data)) {}

	~UploadBlobAsyncContext() override {
		if (m_context != nullptr) {
			XblContextCloseHandle(m_context);
			m_context = nullptr;
		}
	}

	XblContextHandle get_context() const {
		return m_context;
	}

	XblTitleStorageBlobMetadata get_metadata() const {
		return m_metadata;
	}

	const uint8_t *get_data() const {
		return m_data.empty() ? nullptr : m_data.data();
	}

	size_t get_data_size() const {
		return m_data.size();
	}
};

class DeleteBlobAsyncContext final : public GDKSignalXAsyncContext {
	XblContextHandle m_context = nullptr;

protected:
	void finalize(XAsyncBlock *p_async_block) override {
		if (get_runtime()->is_shutting_down() || get_pending_signal()->was_cancel_requested()) {
			Ref<GDKResult> result = GDKResult::cancelled("Title Storage delete cancelled.");
			get_pending_signal()->complete(result);
			return;
		}

		HRESULT result_hr = XAsyncGetStatus(p_async_block, false);
		if (result_hr == E_ABORT) {
			Ref<GDKResult> result = GDKResult::cancelled("Title Storage delete cancelled.");
			get_pending_signal()->complete(result);
			return;
		}
		if (FAILED(result_hr)) {
			Ref<GDKResult> result = GDKResult::hresult_error(result_hr, "Failed to delete Title Storage blob.", "title_storage_delete_failed");
			get_pending_signal()->complete(result);
			return;
		}

		get_pending_signal()->complete(GDKResult::ok_result());
	}

public:
	DeleteBlobAsyncContext(GDKRuntime *p_runtime, const Ref<GDKPendingSignal> &p_pending_signal, XblContextHandle p_context) :
			GDKSignalXAsyncContext(p_runtime, p_pending_signal),
			m_context(p_context) {}

	~DeleteBlobAsyncContext() override {
		if (m_context != nullptr) {
			XblContextCloseHandle(m_context);
			m_context = nullptr;
		}
	}

	XblContextHandle get_context() const {
		return m_context;
	}
};

class DownloadBlobAsyncContext final : public GDKSignalXAsyncContext {
	XblContextHandle m_context = nullptr;
	XblTitleStorageBlobMetadata m_metadata = {};
	std::vector<uint8_t> m_buffer;

protected:
	void finalize(XAsyncBlock *p_async_block) override {
		if (get_runtime()->is_shutting_down() || get_pending_signal()->was_cancel_requested()) {
			Ref<GDKResult> result = GDKResult::cancelled("Title Storage download cancelled.");
			get_pending_signal()->complete(result);
			return;
		}

		XblTitleStorageBlobMetadata native_metadata = {};
		HRESULT result_hr = XblTitleStorageDownloadBlobResult(p_async_block, &native_metadata);
		if (result_hr == E_ABORT) {
			Ref<GDKResult> result = GDKResult::cancelled("Title Storage download cancelled.");
			get_pending_signal()->complete(result);
			return;
		}
		if (FAILED(result_hr)) {
			Ref<GDKResult> result = GDKResult::hresult_error(result_hr, "Failed to download Title Storage blob.", "title_storage_download_result_failed");
			get_pending_signal()->complete(result);
			return;
		}

		Dictionary payload;
		payload["metadata"] = _make_metadata_ref(native_metadata);
		payload["data"] = _make_packed_byte_array(m_buffer);
		get_pending_signal()->complete(GDKResult::ok_result(payload));
	}

public:
	DownloadBlobAsyncContext(GDKRuntime *p_runtime, const Ref<GDKPendingSignal> &p_pending_signal, XblContextHandle p_context, const XblTitleStorageBlobMetadata &p_metadata) :
			GDKSignalXAsyncContext(p_runtime, p_pending_signal),
			m_context(p_context),
			m_metadata(p_metadata),
			m_buffer(p_metadata.length) {}

	~DownloadBlobAsyncContext() override {
		if (m_context != nullptr) {
			XblContextCloseHandle(m_context);
			m_context = nullptr;
		}
	}

	XblContextHandle get_context() const {
		return m_context;
	}

	XblTitleStorageBlobMetadata get_metadata() const {
		return m_metadata;
	}

	uint8_t *get_buffer() {
		return m_buffer.empty() ? nullptr : m_buffer.data();
	}

	size_t get_buffer_size() const {
		return m_buffer.size();
	}
};

class DownloadMetadataAsyncContext final : public GDKSignalXAsyncContext {
	XblContextHandle m_context = nullptr;
	String m_blob_path;
	XblTitleStorageETagMatchCondition m_match_condition = XblTitleStorageETagMatchCondition::NotUsed;

protected:
	void finalize(XAsyncBlock *p_async_block) override {
		if (get_runtime()->is_shutting_down() || get_pending_signal()->was_cancel_requested()) {
			Ref<GDKResult> result = GDKResult::cancelled("Title Storage download cancelled.");
			get_pending_signal()->complete(result);
			return;
		}

		XblTitleStorageBlobMetadataResultHandle handle = nullptr;
		HRESULT result_hr = XblTitleStorageGetBlobMetadataResult(p_async_block, &handle);
		if (result_hr == E_ABORT) {
			Ref<GDKResult> result = GDKResult::cancelled("Title Storage download cancelled.");
			get_pending_signal()->complete(result);
			return;
		}
		if (FAILED(result_hr)) {
			Ref<GDKResult> result = GDKResult::hresult_error(result_hr, "Failed to retrieve Title Storage blob metadata for download.", "title_storage_download_metadata_failed");
			get_pending_signal()->complete(result);
			return;
		}

		const XblTitleStorageBlobMetadata *items = nullptr;
		size_t item_count = 0;
		result_hr = XblTitleStorageBlobMetadataResultGetItems(handle, &items, &item_count);
		if (FAILED(result_hr)) {
			XblTitleStorageBlobMetadataResultCloseHandle(handle);
			Ref<GDKResult> result = GDKResult::hresult_error(result_hr, "Failed to read Title Storage blob metadata for download.", "title_storage_download_metadata_items_failed");
			get_pending_signal()->complete(result);
			return;
		}

		const XblTitleStorageBlobMetadata *matched_metadata = _find_exact_metadata(items, item_count, m_blob_path);
		if (matched_metadata == nullptr) {
			XblTitleStorageBlobMetadataResultCloseHandle(handle);
			Ref<GDKResult> result = GDKResult::error_result(E_BOUNDS, "blob_not_found", "No Title Storage blob metadata matched blob_path.");
			get_pending_signal()->complete(result);
			return;
		}

		XblTitleStorageBlobMetadata metadata_copy = *matched_metadata;
		XblTitleStorageBlobMetadataResultCloseHandle(handle);

		XblContextHandle transferred_context = m_context;
		m_context = nullptr;
		DownloadBlobAsyncContext *download_context = new DownloadBlobAsyncContext(get_runtime(), get_pending_signal(), transferred_context, metadata_copy);
		download_context->bind_cancel_handler();
		result_hr = XblTitleStorageDownloadBlobAsync(
				download_context->get_context(),
				download_context->get_metadata(),
				download_context->get_buffer(),
				download_context->get_buffer_size(),
				m_match_condition,
				nullptr,
				0,
				download_context->get_async_block());
		if (FAILED(result_hr)) {
			download_context->clear_cancel_handler();
			delete download_context;
			Ref<GDKResult> result = GDKResult::hresult_error(result_hr, "Failed to start Title Storage blob download.", "title_storage_download_start_failed");
			get_pending_signal()->complete(result);
		}
	}

public:
	DownloadMetadataAsyncContext(GDKRuntime *p_runtime, const Ref<GDKPendingSignal> &p_pending_signal, XblContextHandle p_context, const String &p_blob_path, XblTitleStorageETagMatchCondition p_match_condition) :
			GDKSignalXAsyncContext(p_runtime, p_pending_signal),
			m_context(p_context),
			m_blob_path(p_blob_path),
			m_match_condition(p_match_condition) {}

	~DownloadMetadataAsyncContext() override {
		if (m_context != nullptr) {
			XblContextCloseHandle(m_context);
			m_context = nullptr;
		}
	}

	XblContextHandle get_context() const {
		return m_context;
	}
};

} //namespace

#endif

void GDKTitleStorageBlobMetadata::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_blob_path"), &GDKTitleStorageBlobMetadata::get_blob_path);
	ClassDB::bind_method(D_METHOD("get_blob_type"), &GDKTitleStorageBlobMetadata::get_blob_type);
	ClassDB::bind_method(D_METHOD("get_storage_type"), &GDKTitleStorageBlobMetadata::get_storage_type);
	ClassDB::bind_method(D_METHOD("get_display_name"), &GDKTitleStorageBlobMetadata::get_display_name);
	ClassDB::bind_method(D_METHOD("get_e_tag"), &GDKTitleStorageBlobMetadata::get_e_tag);
	ClassDB::bind_method(D_METHOD("get_client_timestamp"), &GDKTitleStorageBlobMetadata::get_client_timestamp);
	ClassDB::bind_method(D_METHOD("get_length"), &GDKTitleStorageBlobMetadata::get_length);
	ClassDB::bind_method(D_METHOD("get_service_configuration_id"), &GDKTitleStorageBlobMetadata::get_service_configuration_id);
	ClassDB::bind_method(D_METHOD("get_xuid"), &GDKTitleStorageBlobMetadata::get_xuid);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "blob_path"), "", "get_blob_path");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "blob_type"), "", "get_blob_type");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "storage_type"), "", "get_storage_type");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "display_name"), "", "get_display_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "e_tag"), "", "get_e_tag");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "client_timestamp"), "", "get_client_timestamp");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "length"), "", "get_length");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "service_configuration_id"), "", "get_service_configuration_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "xuid"), "", "get_xuid");
}

String GDKTitleStorageBlobMetadata::get_blob_path() const {
	return m_blob_path;
}

String GDKTitleStorageBlobMetadata::get_blob_type() const {
	return m_blob_type;
}

String GDKTitleStorageBlobMetadata::get_storage_type() const {
	return m_storage_type;
}

String GDKTitleStorageBlobMetadata::get_display_name() const {
	return m_display_name;
}

String GDKTitleStorageBlobMetadata::get_e_tag() const {
	return m_e_tag;
}

int64_t GDKTitleStorageBlobMetadata::get_client_timestamp() const {
	return m_client_timestamp;
}

int64_t GDKTitleStorageBlobMetadata::get_length() const {
	return m_length;
}

String GDKTitleStorageBlobMetadata::get_service_configuration_id() const {
	return m_service_configuration_id;
}

String GDKTitleStorageBlobMetadata::get_xuid() const {
	return m_xuid;
}

#ifdef XBOX_MODULE_GDK_ENABLED
void GDKTitleStorageBlobMetadata::populate_from_native(const XblTitleStorageBlobMetadata &p_metadata) {
	m_blob_path = _utf8_or_empty(p_metadata.blobPath);
	m_blob_type = _blob_type_to_string(p_metadata.blobType);
	m_storage_type = _storage_type_to_string(p_metadata.storageType);
	m_display_name = _utf8_or_empty(p_metadata.displayName);
	m_e_tag = _utf8_or_empty(p_metadata.eTag);
	m_client_timestamp = static_cast<int64_t>(p_metadata.clientTimestamp);
	m_length = static_cast<int64_t>(p_metadata.length);
	m_service_configuration_id = _utf8_or_empty(p_metadata.serviceConfigurationId);
	m_xuid = p_metadata.xboxUserId == 0 ? String() : String::num_uint64(p_metadata.xboxUserId);
}
#else
void GDKTitleStorageBlobMetadata::populate_from_native(const XblTitleStorageBlobMetadata &p_metadata) {
}
#endif

void GDKTitleStorageBlobMetadataResult::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_items"), &GDKTitleStorageBlobMetadataResult::get_items);
	ClassDB::bind_method(D_METHOD("has_next"), &GDKTitleStorageBlobMetadataResult::has_next);
	ClassDB::bind_method(D_METHOD("get_storage_type"), &GDKTitleStorageBlobMetadataResult::get_storage_type);
	ClassDB::bind_method(D_METHOD("get_blob_path"), &GDKTitleStorageBlobMetadataResult::get_blob_path);

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "items"), "", "get_items");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "has_next"), "", "has_next");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "storage_type"), "", "get_storage_type");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "blob_path"), "", "get_blob_path");
}

#ifdef XBOX_MODULE_GDK_ENABLED
GDKTitleStorageBlobMetadataResult::~GDKTitleStorageBlobMetadataResult() {
	if (m_handle != nullptr) {
		XblTitleStorageBlobMetadataResultCloseHandle(m_handle);
		m_handle = nullptr;
	}
}
#else
GDKTitleStorageBlobMetadataResult::~GDKTitleStorageBlobMetadataResult() {
	m_handle = nullptr;
}
#endif

Array GDKTitleStorageBlobMetadataResult::get_items() const {
	return m_items;
}

bool GDKTitleStorageBlobMetadataResult::has_next() const {
	return m_has_next;
}

String GDKTitleStorageBlobMetadataResult::get_storage_type() const {
	return m_storage_type;
}

String GDKTitleStorageBlobMetadataResult::get_blob_path() const {
	return m_blob_path;
}

#ifdef XBOX_MODULE_GDK_ENABLED
HRESULT GDKTitleStorageBlobMetadataResult::populate_from_handle(
		const Ref<GDKUser> &p_user,
		const String &p_storage_type,
		const String &p_blob_path,
		uint32_t p_max_items,
		XblTitleStorageBlobMetadataResultHandle p_handle) {
	if (p_handle == nullptr) {
		return E_POINTER;
	}

	const XblTitleStorageBlobMetadata *items = nullptr;
	size_t item_count = 0;
	HRESULT hr = XblTitleStorageBlobMetadataResultGetItems(p_handle, &items, &item_count);
	if (FAILED(hr)) {
		return hr;
	}

	Array translated_items;
	for (size_t i = 0; i < item_count; ++i) {
		translated_items.push_back(_make_metadata_ref(items[i]));
	}

	bool has_next = false;
	hr = XblTitleStorageBlobMetadataResultHasNext(p_handle, &has_next);
	if (FAILED(hr)) {
		return hr;
	}

	if (m_handle != nullptr) {
		XblTitleStorageBlobMetadataResultCloseHandle(m_handle);
		m_handle = nullptr;
	}

	m_user = p_user;
	m_storage_type = p_storage_type;
	m_blob_path = p_blob_path;
	m_max_items = p_max_items;
	m_handle = p_handle;
	m_items = translated_items;
	m_has_next = has_next;
	return S_OK;
}

XblTitleStorageBlobMetadataResultHandle GDKTitleStorageBlobMetadataResult::get_handle_internal() const {
	return m_handle;
}

Ref<GDKUser> GDKTitleStorageBlobMetadataResult::get_user_internal() const {
	return m_user;
}

uint32_t GDKTitleStorageBlobMetadataResult::get_max_items_internal() const {
	return m_max_items;
}
#else
HRESULT GDKTitleStorageBlobMetadataResult::populate_from_handle(
		const Ref<GDKUser> &p_user,
		const String &p_storage_type,
		const String &p_blob_path,
		uint32_t p_max_items,
		XblTitleStorageBlobMetadataResultHandle p_handle) {
	return E_NOTIMPL;
}

XblTitleStorageBlobMetadataResultHandle GDKTitleStorageBlobMetadataResult::get_handle_internal() const {
	return m_handle;
}

Ref<GDKUser> GDKTitleStorageBlobMetadataResult::get_user_internal() const {
	return m_user;
}

uint32_t GDKTitleStorageBlobMetadataResult::get_max_items_internal() const {
	return m_max_items;
}
#endif

void GDKTitleStorage::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_quota_async", "user", "storage_type"), &GDKTitleStorage::get_quota_async);
	ClassDB::bind_method(D_METHOD("list_blob_metadata_async", "user", "storage_type", "blob_path", "skip_items", "max_items"), &GDKTitleStorage::list_blob_metadata_async, DEFVAL(String()), DEFVAL(0), DEFVAL(25));
	ClassDB::bind_method(D_METHOD("get_next_blob_metadata_async", "result"), &GDKTitleStorage::get_next_blob_metadata_async);
	ClassDB::bind_method(D_METHOD("download_blob_async", "user", "storage_type", "blob_path"), &GDKTitleStorage::download_blob_async);
	ClassDB::bind_method(D_METHOD("upload_blob_async", "user", "storage_type", "blob_path", "data", "display_name", "e_tag", "match_condition"), &GDKTitleStorage::upload_blob_async, DEFVAL(String()), DEFVAL(String()), DEFVAL(String("not_used")));
	ClassDB::bind_method(D_METHOD("delete_blob_async", "user", "storage_type", "blob_path", "e_tag", "match_condition"), &GDKTitleStorage::delete_blob_async, DEFVAL(String()), DEFVAL(String("not_used")));
}

#ifdef XBOX_MODULE_GDK_ENABLED
void GDKTitleStorage::set_owner(GDK *p_owner) {
	m_owner = p_owner;
}

GDKRuntime *GDKTitleStorage::_get_runtime() const {
	return m_owner == nullptr ? nullptr : m_owner->get_runtime();
}

GDKXboxServices *GDKTitleStorage::_get_xbox_services() const {
	return m_owner == nullptr ? nullptr : m_owner->get_xbox_services();
}

Signal GDKTitleStorage::_make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data) const {
	GDKRuntime *runtime = _get_runtime();
	ERR_FAIL_NULL_V(runtime, Signal());
	return runtime->make_error_signal(p_hresult, p_code, p_message, p_data);
}

Ref<GDKResult> GDKTitleStorage::_ensure_ready_user(const Ref<GDKUser> &p_user) const {
	if (!m_runtime_ready) {
		return GDKResult::error_result(E_FAIL, "runtime_unavailable", "GDK runtime is not initialized.");
	}
	if (!p_user.is_valid() || p_user->get_handle() == nullptr || !p_user->is_signed_in()) {
		return GDKResult::error_result(E_INVALIDARG, "invalid_user", "A signed-in GDKUser is required.");
	}
	return GDKResult::ok_result();
}

Ref<GDKResult> GDKTitleStorage::on_runtime_initialized() {
	m_runtime_ready = true;
	return GDKResult::ok_result();
}

void GDKTitleStorage::shutdown() {
	m_runtime_ready = false;
}

Signal GDKTitleStorage::get_quota_async(const Ref<GDKUser> &p_user, const String &p_storage_type) {
	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr) {
		return Signal();
	}

	Ref<GDKResult> validation = _ensure_ready_user(p_user);
	if (!validation->is_ok()) {
		return _make_error_signal(static_cast<HRESULT>(validation->get_hresult()), validation->get_code(), validation->get_message());
	}

	XblTitleStorageType storage_type = XblTitleStorageType::Universal;
	if (!_try_parse_storage_type(p_storage_type, &storage_type)) {
		return _make_error_signal(E_INVALIDARG, "invalid_storage_type", "Unknown Title Storage type.");
	}

	GDKXboxServices *xbox_services = _get_xbox_services();
	if (xbox_services == nullptr || !xbox_services->is_initialized()) {
		return _make_error_signal(E_FAIL, "xbox_services_not_initialized", "Xbox services are unavailable. Ensure the title has a TitleId before using Title Storage.");
	}

	XblContextHandle context = nullptr;
	HRESULT hr = xbox_services->duplicate_context_for_user(p_user, &context);
	if (FAILED(hr)) {
		return _make_error_signal(hr, "xbox_context_unavailable", "Failed to create an Xbox services context for the user.");
	}

	Ref<GDKPendingSignal> pending_signal = runtime->make_pending_signal();
	QuotaAsyncContext *async_context = new QuotaAsyncContext(runtime, pending_signal, context, _storage_type_to_string(storage_type));
	async_context->bind_cancel_handler();
	const CharString scid_utf8 = xbox_services->get_scid().utf8();
	hr = XblTitleStorageGetQuotaAsync(async_context->get_context(), scid_utf8.get_data(), storage_type, async_context->get_async_block());
	if (FAILED(hr)) {
		async_context->clear_cancel_handler();
		delete async_context;
		return _make_error_signal(hr, "title_storage_quota_start_failed", "Failed to start Title Storage quota query.");
	}
	return pending_signal->get_completed_signal();
}

Signal GDKTitleStorage::list_blob_metadata_async(const Ref<GDKUser> &p_user, const String &p_storage_type, const String &p_blob_path, int64_t p_skip_items, int64_t p_max_items) {
	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr) {
		return Signal();
	}

	Ref<GDKResult> validation = _ensure_ready_user(p_user);
	if (!validation->is_ok()) {
		return _make_error_signal(static_cast<HRESULT>(validation->get_hresult()), validation->get_code(), validation->get_message());
	}

	XblTitleStorageType storage_type = XblTitleStorageType::Universal;
	if (!_try_parse_storage_type(p_storage_type, &storage_type)) {
		return _make_error_signal(E_INVALIDARG, "invalid_storage_type", "Unknown Title Storage type.");
	}

	uint32_t skip_items = 0;
	Ref<GDKResult> parse_result = _parse_uint32("skip_items", p_skip_items, &skip_items);
	if (!parse_result->is_ok()) {
		return _make_error_signal(static_cast<HRESULT>(parse_result->get_hresult()), parse_result->get_code(), parse_result->get_message());
	}
	uint32_t max_items = 0;
	parse_result = _parse_uint32("max_items", p_max_items, &max_items);
	if (!parse_result->is_ok()) {
		return _make_error_signal(static_cast<HRESULT>(parse_result->get_hresult()), parse_result->get_code(), parse_result->get_message());
	}

	GDKXboxServices *xbox_services = _get_xbox_services();
	if (xbox_services == nullptr || !xbox_services->is_initialized()) {
		return _make_error_signal(E_FAIL, "xbox_services_not_initialized", "Xbox services are unavailable. Ensure the title has a TitleId before using Title Storage.");
	}

	XblContextHandle context = nullptr;
	uint64_t xbox_user_id = 0;
	HRESULT hr = xbox_services->duplicate_context_for_user(p_user, &context, &xbox_user_id);
	if (FAILED(hr)) {
		return _make_error_signal(hr, "xbox_context_unavailable", "Failed to create an Xbox services context for the user.");
	}

	Ref<GDKPendingSignal> pending_signal = runtime->make_pending_signal();
	const String storage_type_name = _storage_type_to_string(storage_type);
	const String blob_path = p_blob_path.strip_edges();
	BlobMetadataAsyncContext *async_context = new BlobMetadataAsyncContext(runtime, pending_signal, p_user, context, storage_type_name, blob_path, max_items, false);
	async_context->bind_cancel_handler();
	const CharString scid_utf8 = xbox_services->get_scid().utf8();
	const CharString blob_path_utf8 = blob_path.utf8();
	hr = XblTitleStorageGetBlobMetadataAsync(
			async_context->get_context(),
			scid_utf8.get_data(),
			storage_type,
			blob_path_utf8.get_data(),
			storage_type == XblTitleStorageType::GlobalStorage ? 0 : xbox_user_id,
			skip_items,
			max_items,
			async_context->get_async_block());
	if (FAILED(hr)) {
		async_context->clear_cancel_handler();
		delete async_context;
		return _make_error_signal(hr, "title_storage_metadata_start_failed", "Failed to start Title Storage metadata query.");
	}
	return pending_signal->get_completed_signal();
}

Signal GDKTitleStorage::get_next_blob_metadata_async(const Ref<GDKTitleStorageBlobMetadataResult> &p_result) {
	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr) {
		return Signal();
	}
	if (!m_runtime_ready) {
		return _make_error_signal(E_FAIL, "runtime_unavailable", "GDK runtime is not initialized.");
	}
	if (!p_result.is_valid() || p_result->get_handle_internal() == nullptr) {
		return _make_error_signal(E_INVALIDARG, "invalid_metadata_result", "A metadata result returned by this service is required.");
	}
	if (!p_result->has_next()) {
		return _make_error_signal(E_INVALIDARG, "no_next_page", "Metadata result has no next page.");
	}

	Ref<GDKUser> user = p_result->get_user_internal();
	Ref<GDKResult> validation = _ensure_ready_user(user);
	if (!validation->is_ok()) {
		return _make_error_signal(static_cast<HRESULT>(validation->get_hresult()), validation->get_code(), validation->get_message());
	}

	GDKXboxServices *xbox_services = _get_xbox_services();
	if (xbox_services == nullptr || !xbox_services->is_initialized()) {
		return _make_error_signal(E_FAIL, "xbox_services_not_initialized", "Xbox services are unavailable. Ensure the title has a TitleId before using Title Storage.");
	}

	XblContextHandle context = nullptr;
	HRESULT hr = xbox_services->duplicate_context_for_user(user, &context);
	if (FAILED(hr)) {
		return _make_error_signal(hr, "xbox_context_unavailable", "Failed to create an Xbox services context for the user.");
	}

	Ref<GDKPendingSignal> pending_signal = runtime->make_pending_signal();
	XblTitleStorageBlobMetadataResultHandle previous_handle = nullptr;
	hr = XblTitleStorageBlobMetadataResultDuplicateHandle(p_result->get_handle_internal(), &previous_handle);
	if (FAILED(hr)) {
		XblContextCloseHandle(context);
		return _make_error_signal(hr, "title_storage_metadata_duplicate_failed", "Failed to duplicate Title Storage metadata handle.");
	}

	BlobMetadataAsyncContext *async_context = new BlobMetadataAsyncContext(runtime, pending_signal, user, context, p_result->get_storage_type(), p_result->get_blob_path(), p_result->get_max_items_internal(), true, previous_handle);
	async_context->bind_cancel_handler();
	hr = XblTitleStorageBlobMetadataResultGetNextAsync(
			async_context->get_previous_handle(),
			p_result->get_max_items_internal(),
			async_context->get_async_block());
	if (FAILED(hr)) {
		async_context->clear_cancel_handler();
		delete async_context;
		return _make_error_signal(hr, "title_storage_metadata_next_start_failed", "Failed to start Title Storage metadata next-page query.");
	}
	return pending_signal->get_completed_signal();
}

Signal GDKTitleStorage::download_blob_async(const Ref<GDKUser> &p_user, const String &p_storage_type, const String &p_blob_path) {
	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr) {
		return Signal();
	}

	Ref<GDKResult> validation = _ensure_ready_user(p_user);
	if (!validation->is_ok()) {
		return _make_error_signal(static_cast<HRESULT>(validation->get_hresult()), validation->get_code(), validation->get_message());
	}

	XblTitleStorageType storage_type = XblTitleStorageType::Universal;
	if (!_try_parse_storage_type(p_storage_type, &storage_type)) {
		return _make_error_signal(E_INVALIDARG, "invalid_storage_type", "Unknown Title Storage type.");
	}
	const String blob_path = p_blob_path.strip_edges();
	if (blob_path.is_empty()) {
		return _make_error_signal(E_INVALIDARG, "invalid_blob_path", "blob_path must be a non-empty string.");
	}

	GDKXboxServices *xbox_services = _get_xbox_services();
	if (xbox_services == nullptr || !xbox_services->is_initialized()) {
		return _make_error_signal(E_FAIL, "xbox_services_not_initialized", "Xbox services are unavailable. Ensure the title has a TitleId before using Title Storage.");
	}

	XblContextHandle context = nullptr;
	uint64_t xbox_user_id = 0;
	HRESULT hr = xbox_services->duplicate_context_for_user(p_user, &context, &xbox_user_id);
	if (FAILED(hr)) {
		return _make_error_signal(hr, "xbox_context_unavailable", "Failed to create an Xbox services context for the user.");
	}

	Ref<GDKPendingSignal> pending_signal = runtime->make_pending_signal();
	DownloadMetadataAsyncContext *async_context = new DownloadMetadataAsyncContext(runtime, pending_signal, context, blob_path, XblTitleStorageETagMatchCondition::NotUsed);
	async_context->bind_cancel_handler();
	const CharString scid_utf8 = xbox_services->get_scid().utf8();
	const CharString blob_path_utf8 = blob_path.utf8();
	hr = XblTitleStorageGetBlobMetadataAsync(
			async_context->get_context(),
			scid_utf8.get_data(),
			storage_type,
			blob_path_utf8.get_data(),
			storage_type == XblTitleStorageType::GlobalStorage ? 0 : xbox_user_id,
			0,
			1,
			async_context->get_async_block());
	if (FAILED(hr)) {
		async_context->clear_cancel_handler();
		delete async_context;
		return _make_error_signal(hr, "title_storage_download_metadata_start_failed", "Failed to start Title Storage metadata query for download.");
	}
	return pending_signal->get_completed_signal();
}

Signal GDKTitleStorage::upload_blob_async(const Ref<GDKUser> &p_user, const String &p_storage_type, const String &p_blob_path, const PackedByteArray &p_data, const String &p_display_name, const String &p_e_tag, const String &p_match_condition) {
	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr) {
		return Signal();
	}

	Ref<GDKResult> validation = _ensure_ready_user(p_user);
	if (!validation->is_ok()) {
		return _make_error_signal(static_cast<HRESULT>(validation->get_hresult()), validation->get_code(), validation->get_message());
	}

	XblTitleStorageType storage_type = XblTitleStorageType::Universal;
	if (!_try_parse_storage_type(p_storage_type, &storage_type)) {
		return _make_error_signal(E_INVALIDARG, "invalid_storage_type", "Unknown Title Storage type.");
	}
	XblTitleStorageETagMatchCondition match_condition = XblTitleStorageETagMatchCondition::NotUsed;
	if (!_try_parse_match_condition(p_match_condition, &match_condition)) {
		return _make_error_signal(E_INVALIDARG, "invalid_match_condition", "Unknown Title Storage ETag match condition.");
	}
	if (match_condition != XblTitleStorageETagMatchCondition::NotUsed && p_e_tag.strip_edges().is_empty()) {
		return _make_error_signal(E_INVALIDARG, "invalid_e_tag", "An e_tag is required when match_condition is not not_used.");
	}

	GDKXboxServices *xbox_services = _get_xbox_services();
	if (xbox_services == nullptr || !xbox_services->is_initialized()) {
		return _make_error_signal(E_FAIL, "xbox_services_not_initialized", "Xbox services are unavailable. Ensure the title has a TitleId before using Title Storage.");
	}

	XblContextHandle context = nullptr;
	uint64_t xbox_user_id = 0;
	HRESULT hr = xbox_services->duplicate_context_for_user(p_user, &context, &xbox_user_id);
	if (FAILED(hr)) {
		return _make_error_signal(hr, "xbox_context_unavailable", "Failed to create an Xbox services context for the user.");
	}

	std::vector<uint8_t> data = _make_byte_vector(p_data);
	XblTitleStorageBlobMetadata native_metadata = {};
	Ref<GDKResult> metadata_result = _make_blob_metadata(
			xbox_services->get_scid(),
			storage_type,
			xbox_user_id,
			p_blob_path,
			p_display_name,
			p_e_tag,
			data.size(),
			XblTitleStorageBlobType::Binary,
			&native_metadata);
	if (!metadata_result->is_ok()) {
		XblContextCloseHandle(context);
		return _make_error_signal(static_cast<HRESULT>(metadata_result->get_hresult()), metadata_result->get_code(), metadata_result->get_message());
	}

	Ref<GDKPendingSignal> pending_signal = runtime->make_pending_signal();
	UploadBlobAsyncContext *async_context = new UploadBlobAsyncContext(runtime, pending_signal, context, native_metadata, std::move(data));
	async_context->bind_cancel_handler();
	hr = XblTitleStorageUploadBlobAsync(
			async_context->get_context(),
			async_context->get_metadata(),
			async_context->get_data(),
			async_context->get_data_size(),
			match_condition,
			0,
			async_context->get_async_block());
	if (FAILED(hr)) {
		async_context->clear_cancel_handler();
		delete async_context;
		return _make_error_signal(hr, "title_storage_upload_start_failed", "Failed to start Title Storage blob upload.");
	}
	return pending_signal->get_completed_signal();
}

Signal GDKTitleStorage::delete_blob_async(const Ref<GDKUser> &p_user, const String &p_storage_type, const String &p_blob_path, const String &p_e_tag, const String &p_match_condition) {
	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr) {
		return Signal();
	}

	Ref<GDKResult> validation = _ensure_ready_user(p_user);
	if (!validation->is_ok()) {
		return _make_error_signal(static_cast<HRESULT>(validation->get_hresult()), validation->get_code(), validation->get_message());
	}

	XblTitleStorageType storage_type = XblTitleStorageType::Universal;
	if (!_try_parse_storage_type(p_storage_type, &storage_type)) {
		return _make_error_signal(E_INVALIDARG, "invalid_storage_type", "Unknown Title Storage type.");
	}
	XblTitleStorageETagMatchCondition match_condition = XblTitleStorageETagMatchCondition::NotUsed;
	if (!_try_parse_match_condition(p_match_condition, &match_condition)) {
		return _make_error_signal(E_INVALIDARG, "invalid_match_condition", "Unknown Title Storage ETag match condition.");
	}
	if (match_condition == XblTitleStorageETagMatchCondition::IfNotMatch) {
		return _make_error_signal(E_INVALIDARG, "invalid_match_condition", "delete_blob_async() supports only not_used and if_match.");
	}
	if (match_condition == XblTitleStorageETagMatchCondition::IfMatch && p_e_tag.strip_edges().is_empty()) {
		return _make_error_signal(E_INVALIDARG, "invalid_e_tag", "An e_tag is required for if_match deletes.");
	}

	GDKXboxServices *xbox_services = _get_xbox_services();
	if (xbox_services == nullptr || !xbox_services->is_initialized()) {
		return _make_error_signal(E_FAIL, "xbox_services_not_initialized", "Xbox services are unavailable. Ensure the title has a TitleId before using Title Storage.");
	}

	XblContextHandle context = nullptr;
	uint64_t xbox_user_id = 0;
	HRESULT hr = xbox_services->duplicate_context_for_user(p_user, &context, &xbox_user_id);
	if (FAILED(hr)) {
		return _make_error_signal(hr, "xbox_context_unavailable", "Failed to create an Xbox services context for the user.");
	}

	XblTitleStorageBlobMetadata native_metadata = {};
	Ref<GDKResult> metadata_result = _make_blob_metadata(
			xbox_services->get_scid(),
			storage_type,
			xbox_user_id,
			p_blob_path,
			String(),
			p_e_tag,
			0,
			XblTitleStorageBlobType::Binary,
			&native_metadata);
	if (!metadata_result->is_ok()) {
		XblContextCloseHandle(context);
		return _make_error_signal(static_cast<HRESULT>(metadata_result->get_hresult()), metadata_result->get_code(), metadata_result->get_message());
	}

	Ref<GDKPendingSignal> pending_signal = runtime->make_pending_signal();
	DeleteBlobAsyncContext *async_context = new DeleteBlobAsyncContext(runtime, pending_signal, context);
	async_context->bind_cancel_handler();
	hr = XblTitleStorageDeleteBlobAsync(
			async_context->get_context(),
			native_metadata,
			match_condition == XblTitleStorageETagMatchCondition::IfMatch,
			async_context->get_async_block());
	if (FAILED(hr)) {
		async_context->clear_cancel_handler();
		delete async_context;
		return _make_error_signal(hr, "title_storage_delete_start_failed", "Failed to start Title Storage blob delete.");
	}
	return pending_signal->get_completed_signal();
}

void GDKTitleStorage::on_user_removed(const Ref<GDKUser> &p_user) {
	(void)p_user;
}

#else
void GDKTitleStorage::set_owner(GDK *p_owner) {}

Signal GDKTitleStorage::_make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data) const {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Ref<GDKResult> GDKTitleStorage::_ensure_ready_user(const Ref<GDKUser> &p_user) const {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Ref<GDKResult> GDKTitleStorage::on_runtime_initialized() {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

void GDKTitleStorage::shutdown() {}

Signal GDKTitleStorage::get_quota_async(const Ref<GDKUser> &p_user, const String &p_storage_type) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKTitleStorage::list_blob_metadata_async(const Ref<GDKUser> &p_user, const String &p_storage_type, const String &p_blob_path, int64_t p_skip_items, int64_t p_max_items) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKTitleStorage::get_next_blob_metadata_async(const Ref<GDKTitleStorageBlobMetadataResult> &p_result) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKTitleStorage::download_blob_async(const Ref<GDKUser> &p_user, const String &p_storage_type, const String &p_blob_path) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKTitleStorage::upload_blob_async(const Ref<GDKUser> &p_user, const String &p_storage_type, const String &p_blob_path, const PackedByteArray &p_data, const String &p_display_name, const String &p_e_tag, const String &p_match_condition) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKTitleStorage::delete_blob_async(const Ref<GDKUser> &p_user, const String &p_storage_type, const String &p_blob_path, const String &p_e_tag, const String &p_match_condition) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

void GDKTitleStorage::on_user_removed(const Ref<GDKUser> &p_user) {}
#endif
