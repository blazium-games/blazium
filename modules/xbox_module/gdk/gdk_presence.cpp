/**************************************************************************/
/*  gdk_presence.cpp                                                      */
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
#include "gdk_presence.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifdef XBOX_MODULE_GDK_ENABLED
#include "gdk_windows.h"
#endif

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "gdk.h"
#include "gdk_pending_signal.h"
#include "gdk_result.h"
#include "gdk_runtime.h"
#include "gdk_signal_xasync_context.h"
#include "gdk_user.h"
#include "gdk_xbox_services.h"

static String _presence_user_state_to_name(GDKPresenceRecord::UserState p_state) {
	switch (p_state) {
		case GDKPresenceRecord::USER_STATE_ONLINE:
			return "online";
		case GDKPresenceRecord::USER_STATE_AWAY:
			return "away";
		case GDKPresenceRecord::USER_STATE_OFFLINE:
			return "offline";
		case GDKPresenceRecord::USER_STATE_UNKNOWN:
		default:
			return "unknown";
	}
}

#ifdef XBOX_MODULE_GDK_ENABLED

namespace {

String _utf8_or_empty(const char *p_value) {
	if (p_value == nullptr || p_value[0] == '\0') {
		return String();
	}

	return String::utf8(p_value);
}

GDKPresenceRecord::UserState _presence_user_state_to_enum(XblPresenceUserState p_state) {
	switch (p_state) {
		case XblPresenceUserState::Online:
			return GDKPresenceRecord::USER_STATE_ONLINE;
		case XblPresenceUserState::Away:
			return GDKPresenceRecord::USER_STATE_AWAY;
		case XblPresenceUserState::Offline:
			return GDKPresenceRecord::USER_STATE_OFFLINE;
		case XblPresenceUserState::Unknown:
		default:
			return GDKPresenceRecord::USER_STATE_UNKNOWN;
	}
}

String _presence_view_state_to_name(XblPresenceTitleViewState p_view_state) {
	switch (p_view_state) {
		case XblPresenceTitleViewState::FullScreen:
			return "full_screen";
		case XblPresenceTitleViewState::Filled:
			return "filled";
		case XblPresenceTitleViewState::Snapped:
			return "snapped";
		case XblPresenceTitleViewState::Background:
			return "background";
		case XblPresenceTitleViewState::Unknown:
		default:
			return "unknown";
	}
}

String _presence_device_type_to_name(XblPresenceDeviceType p_device_type) {
	switch (p_device_type) {
		case XblPresenceDeviceType::WindowsPhone:
			return "windows_phone";
		case XblPresenceDeviceType::WindowsPhone7:
			return "windows_phone_7";
		case XblPresenceDeviceType::Web:
			return "web";
		case XblPresenceDeviceType::Xbox360:
			return "xbox_360";
		case XblPresenceDeviceType::PC:
			return "pc";
		case XblPresenceDeviceType::Windows8:
			return "windows_8";
		case XblPresenceDeviceType::XboxOne:
			return "xbox_one";
		case XblPresenceDeviceType::WindowsOneCore:
			return "windows_one_core";
		case XblPresenceDeviceType::WindowsOneCoreMobile:
			return "windows_one_core_mobile";
		case XblPresenceDeviceType::iOS:
			return "ios";
		case XblPresenceDeviceType::Android:
			return "android";
		case XblPresenceDeviceType::AppleTV:
			return "apple_tv";
		case XblPresenceDeviceType::Nintendo:
			return "nintendo";
		case XblPresenceDeviceType::PlayStation:
			return "playstation";
		case XblPresenceDeviceType::Win32:
			return "win32";
		case XblPresenceDeviceType::Scarlett:
			return "scarlett";
		case XblPresenceDeviceType::Unknown:
		default:
			return "unknown";
	}
}

String _presence_broadcast_provider_to_name(XblPresenceBroadcastProvider p_provider) {
	switch (p_provider) {
		case XblPresenceBroadcastProvider::Twitch:
			return "twitch";
		case XblPresenceBroadcastProvider::Unknown:
		default:
			return "unknown";
	}
}

Dictionary _make_title_record_dictionary(const XblPresenceTitleRecord &p_title_record, XblPresenceDeviceType p_device_type) {
	Dictionary title_record;
	title_record["title_id"] = static_cast<int64_t>(p_title_record.titleId);
	title_record["title_name"] = _utf8_or_empty(p_title_record.titleName);
	title_record["last_modified"] = static_cast<int64_t>(p_title_record.lastModified);
	title_record["title_active"] = p_title_record.titleActive;
	title_record["rich_presence_string"] = _utf8_or_empty(p_title_record.richPresenceString);
	title_record["view_state"] = static_cast<int64_t>(static_cast<uint32_t>(p_title_record.viewState));
	title_record["view_state_name"] = _presence_view_state_to_name(p_title_record.viewState);
	title_record["device_type"] = static_cast<int64_t>(static_cast<uint32_t>(p_device_type));
	title_record["device_type_name"] = _presence_device_type_to_name(p_device_type);

	bool is_broadcasting = p_title_record.broadcastRecord != nullptr;
	title_record["is_broadcasting"] = is_broadcasting;
	if (is_broadcasting) {
		title_record["broadcast_id"] = _utf8_or_empty(p_title_record.broadcastRecord->broadcastId);
		title_record["broadcast_session"] = _utf8_or_empty(p_title_record.broadcastRecord->session);
		title_record["broadcast_provider"] = static_cast<int64_t>(static_cast<uint32_t>(p_title_record.broadcastRecord->provider));
		title_record["broadcast_provider_name"] = _presence_broadcast_provider_to_name(p_title_record.broadcastRecord->provider);
		title_record["viewer_count"] = static_cast<int64_t>(p_title_record.broadcastRecord->viewerCount);
		title_record["broadcast_start_time"] = static_cast<int64_t>(p_title_record.broadcastRecord->startTime);
	}

	return title_record;
}

Dictionary _make_social_title_record_dictionary(const XblSocialManagerPresenceTitleRecord &p_title_record) {
	Dictionary title_record;
	title_record["title_id"] = static_cast<int64_t>(p_title_record.titleId);
	title_record["title_name"] = _utf8_or_empty(p_title_record.titleName);
	title_record["title_active"] = p_title_record.isTitleActive;
	title_record["rich_presence_string"] = _utf8_or_empty(p_title_record.presenceText);
	title_record["device_type"] = static_cast<int64_t>(static_cast<uint32_t>(p_title_record.deviceType));
	title_record["device_type_name"] = _presence_device_type_to_name(p_title_record.deviceType);
	title_record["is_broadcasting"] = p_title_record.isBroadcasting;
	title_record["is_primary"] = p_title_record.isPrimary;
	return title_record;
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

Signal _make_presence_error_signal(
		GDKRuntime *p_runtime,
		HRESULT p_hresult,
		const String &p_code,
		const String &p_message,
		const Variant &p_data = Variant()) {
	ERR_FAIL_NULL_V(p_runtime, Signal());
	return p_runtime->make_error_signal(p_hresult, p_code, p_message, p_data);
}

class SetPresenceAsyncContext final : public GDKSignalXAsyncContext {
	GDKPresence *m_presence = nullptr;
	Ref<GDKUser> m_user;
	XblContextHandle m_context = nullptr;
	bool m_is_active = true;
	String m_presence_id;
	CharString m_presence_id_utf8;
	std::vector<CharString> m_token_values;
	std::vector<const char *> m_token_ptrs;
	XblPresenceRichPresenceIds m_rich_presence_ids = {};
	bool m_has_rich_presence = false;

protected:
	void finalize(XAsyncBlock *p_async_block) override {
		Ref<GDKResult> result;

		if (get_runtime()->is_shutting_down() || get_pending_signal()->was_cancel_requested()) {
			result = GDKResult::cancelled("Presence update cancelled.");
			get_pending_signal()->complete(result);
			return;
		}

		HRESULT result_hr = XAsyncGetStatus(p_async_block, false);
		if (result_hr == E_ABORT) {
			result = GDKResult::cancelled("Presence update cancelled.");
			get_pending_signal()->complete(result);
			return;
		}
		if (FAILED(result_hr)) {
			result = GDKResult::hresult_error(result_hr, "Failed to update presence.", "presence_update_failed");
			get_pending_signal()->complete(result);
			return;
		}

		Ref<GDKPresenceRecord> record;
		record.instantiate();
		record->set_data(
				m_user.is_valid() ? m_user->get_xuid() : String(),
				m_is_active ? GDKPresenceRecord::USER_STATE_ONLINE : GDKPresenceRecord::USER_STATE_AWAY,
				Array());
		m_presence->cache_presence_record(record, false);
		m_presence->emit_signal("local_presence_set", m_user);
		m_presence->emit_signal("presence_changed", record->get_xuid(), record);

		Dictionary data;
		data["xuid"] = record->get_xuid();
		data["active"] = m_is_active;
		if (!m_presence_id.is_empty()) {
			data["presence_id"] = m_presence_id;
		}
		get_pending_signal()->complete(GDKResult::ok_result(data));
	}

public:
	SetPresenceAsyncContext(
			GDKPresence *p_presence,
			const Ref<GDKUser> &p_user,
			GDKRuntime *p_runtime,
			const Ref<GDKPendingSignal> &p_pending_signal,
			XblContextHandle p_context,
			bool p_is_active,
			const String &p_scid,
			const String &p_presence_id,
			const Array &p_token_ids) :
			GDKSignalXAsyncContext(p_runtime, p_pending_signal),
			m_presence(p_presence),
			m_user(p_user),
			m_context(p_context),
			m_is_active(p_is_active),
			m_presence_id(p_presence_id) {
		if (!p_presence_id.is_empty()) {
			std::memset(&m_rich_presence_ids, 0, sizeof(m_rich_presence_ids));
			CharString scid_utf8 = p_scid.utf8();
			std::snprintf(m_rich_presence_ids.scid, sizeof(m_rich_presence_ids.scid), "%s", scid_utf8.get_data());
			m_presence_id = p_presence_id;
			m_presence_id_utf8 = m_presence_id.utf8();
			m_rich_presence_ids.presenceId = m_presence_id_utf8.get_data();

			const int64_t token_count = p_token_ids.size();
			m_token_values.reserve(static_cast<size_t>(token_count));
			m_token_ptrs.reserve(static_cast<size_t>(token_count));
			for (int64_t i = 0; i < token_count; ++i) {
				m_token_values.push_back(String(p_token_ids[i]).utf8());
			}
			for (const CharString &token_value : m_token_values) {
				m_token_ptrs.push_back(token_value.get_data());
			}

			m_rich_presence_ids.presenceTokenIds = m_token_ptrs.empty() ? nullptr : m_token_ptrs.data();
			m_rich_presence_ids.presenceTokenIdsCount = m_token_ptrs.size();
			m_has_rich_presence = true;
		}
	}

	~SetPresenceAsyncContext() override {
		if (m_context != nullptr) {
			XblContextCloseHandle(m_context);
			m_context = nullptr;
		}
	}

	XblContextHandle get_context() const {
		return m_context;
	}

	XblPresenceRichPresenceIds *get_rich_presence_ids() {
		return m_has_rich_presence ? &m_rich_presence_ids : nullptr;
	}
};

class GetPresenceAsyncContext final : public GDKSignalXAsyncContext {
public:
	enum QueryMode {
		MODE_XUIDS,
		MODE_SOCIAL_GROUP,
	};

private:
	GDKPresence *m_presence = nullptr;
	XblContextHandle m_context = nullptr;
	std::vector<uint64_t> m_xuids;
	QueryMode m_mode = MODE_XUIDS;
	String m_social_group;
	CharString m_social_group_utf8;
	XblPresenceQueryFilters m_filters = {};

protected:
	void finalize(XAsyncBlock *p_async_block) override {
		Ref<GDKResult> result;

		if (get_runtime()->is_shutting_down() || get_pending_signal()->was_cancel_requested()) {
			result = GDKResult::cancelled("Presence query cancelled.");
			get_pending_signal()->complete(result);
			return;
		}

		Array records;
		if (m_mode == MODE_XUIDS && m_xuids.size() == 1) {
			XblPresenceRecordHandle record_handle = nullptr;
			HRESULT result_hr = XblPresenceGetPresenceResult(p_async_block, &record_handle);
			if (result_hr == E_ABORT) {
				result = GDKResult::cancelled("Presence query cancelled.");
				get_pending_signal()->complete(result);
				return;
			}
			if (FAILED(result_hr)) {
				result = GDKResult::hresult_error(result_hr, "Failed to retrieve the presence query result.", "presence_result_failed");
				get_pending_signal()->complete(result);
				return;
			}

			Ref<GDKPresenceRecord> record;
			record.instantiate();
			result_hr = record->populate_from_presence_record(record_handle);
			XblPresenceRecordCloseHandle(record_handle);
			if (FAILED(result_hr)) {
				result = GDKResult::hresult_error(result_hr, "Failed to translate a presence record.", "presence_record_translate_failed");
				get_pending_signal()->complete(result);
				return;
			}

			m_presence->cache_presence_record(record, true);
			records.push_back(record);
		} else {
			size_t result_count = 0;
			HRESULT result_hr = m_mode == MODE_SOCIAL_GROUP ? XblPresenceGetPresenceForSocialGroupResultCount(p_async_block, &result_count) : XblPresenceGetPresenceForMultipleUsersResultCount(p_async_block, &result_count);
			if (result_hr == E_ABORT) {
				result = GDKResult::cancelled("Presence query cancelled.");
				get_pending_signal()->complete(result);
				return;
			}
			if (FAILED(result_hr)) {
				result = GDKResult::hresult_error(result_hr, "Failed to retrieve the presence query result count.", "presence_result_count_failed");
				get_pending_signal()->complete(result);
				return;
			}

			std::vector<XblPresenceRecordHandle> handles(result_count, nullptr);
			if (result_count > 0) {
				result_hr = m_mode == MODE_SOCIAL_GROUP ? XblPresenceGetPresenceForSocialGroupResult(p_async_block, handles.data(), result_count) : XblPresenceGetPresenceForMultipleUsersResult(p_async_block, handles.data(), result_count);
				if (result_hr == E_ABORT) {
					result = GDKResult::cancelled("Presence query cancelled.");
					get_pending_signal()->complete(result);
					return;
				}
				if (FAILED(result_hr)) {
					result = GDKResult::hresult_error(result_hr, "Failed to retrieve presence records.", "presence_results_failed");
					get_pending_signal()->complete(result);
					return;
				}
			}

			for (size_t handle_index = 0; handle_index < handles.size(); ++handle_index) {
				XblPresenceRecordHandle handle = handles[handle_index];
				if (handle == nullptr) {
					continue;
				}

				Ref<GDKPresenceRecord> record;
				record.instantiate();
				result_hr = record->populate_from_presence_record(handle);
				XblPresenceRecordCloseHandle(handle);
				handles[handle_index] = nullptr;
				if (FAILED(result_hr)) {
					for (size_t remaining_index = handle_index + 1; remaining_index < handles.size(); ++remaining_index) {
						if (handles[remaining_index] != nullptr) {
							XblPresenceRecordCloseHandle(handles[remaining_index]);
							handles[remaining_index] = nullptr;
						}
					}

					result = GDKResult::hresult_error(result_hr, "Failed to translate a presence record.", "presence_record_translate_failed");
					get_pending_signal()->complete(result);
					return;
				}

				m_presence->cache_presence_record(record, true);
				records.push_back(record);
			}
		}

		get_pending_signal()->complete(GDKResult::ok_result(records));
	}

public:
	GetPresenceAsyncContext(
			GDKPresence *p_presence,
			GDKRuntime *p_runtime,
			const Ref<GDKPendingSignal> &p_pending_signal,
			XblContextHandle p_context,
			std::vector<uint64_t> p_xuids) :
			GDKSignalXAsyncContext(p_runtime, p_pending_signal),
			m_presence(p_presence),
			m_context(p_context),
			m_xuids(std::move(p_xuids)),
			m_mode(MODE_XUIDS) {}

	GetPresenceAsyncContext(
			GDKPresence *p_presence,
			GDKRuntime *p_runtime,
			const Ref<GDKPendingSignal> &p_pending_signal,
			XblContextHandle p_context,
			const String &p_social_group) :
			GDKSignalXAsyncContext(p_runtime, p_pending_signal),
			m_presence(p_presence),
			m_context(p_context),
			m_mode(MODE_SOCIAL_GROUP),
			m_social_group(p_social_group) {
		m_social_group_utf8 = m_social_group.utf8();
		m_filters.detailLevel = XblPresenceDetailLevel::All;
	}

	~GetPresenceAsyncContext() override {
		if (m_context != nullptr) {
			XblContextCloseHandle(m_context);
			m_context = nullptr;
		}
	}

	XblContextHandle get_context() const {
		return m_context;
	}

	const std::vector<uint64_t> &get_xuids() const {
		return m_xuids;
	}

	const char *get_social_group_name() const {
		return m_social_group_utf8.get_data();
	}

	XblPresenceQueryFilters *get_filters() {
		return &m_filters;
	}
};

} //namespace

#endif

void GDKPresenceRecord::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_xuid"), &GDKPresenceRecord::get_xuid);
	ClassDB::bind_method(D_METHOD("get_user_state"), &GDKPresenceRecord::get_user_state);
	ClassDB::bind_method(D_METHOD("get_user_state_name"), &GDKPresenceRecord::get_user_state_name);
	ClassDB::bind_method(D_METHOD("is_online"), &GDKPresenceRecord::is_online);
	ClassDB::bind_method(D_METHOD("get_title_records"), &GDKPresenceRecord::get_title_records);

	BIND_ENUM_CONSTANT(USER_STATE_UNKNOWN);
	BIND_ENUM_CONSTANT(USER_STATE_ONLINE);
	BIND_ENUM_CONSTANT(USER_STATE_AWAY);
	BIND_ENUM_CONSTANT(USER_STATE_OFFLINE);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "xuid"), "", "get_xuid");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "user_state", PROPERTY_HINT_ENUM, "Unknown,Online,Away,Offline"), "", "get_user_state");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "title_records"), "", "get_title_records");
}

String GDKPresenceRecord::get_xuid() const {
	return m_xuid;
}

GDKPresenceRecord::UserState GDKPresenceRecord::get_user_state() const {
	return m_user_state;
}

String GDKPresenceRecord::get_user_state_name() const {
	return _presence_user_state_to_name(m_user_state);
}

bool GDKPresenceRecord::is_online() const {
	return m_user_state == USER_STATE_ONLINE;
}

Array GDKPresenceRecord::get_title_records() const {
	return m_title_records;
}

void GDKPresenceRecord::set_data(const String &p_xuid, UserState p_user_state, const Array &p_title_records) {
	m_xuid = p_xuid;
	m_user_state = p_user_state;
	m_title_records = p_title_records;
}

#ifdef XBOX_MODULE_GDK_ENABLED
HRESULT GDKPresenceRecord::populate_from_presence_record(XblPresenceRecordHandle p_record_handle) {
	if (p_record_handle == nullptr) {
		return E_INVALIDARG;
	}

	uint64_t xuid = 0;
	HRESULT hr = XblPresenceRecordGetXuid(p_record_handle, &xuid);
	if (FAILED(hr)) {
		return hr;
	}

	XblPresenceUserState user_state = XblPresenceUserState::Unknown;
	hr = XblPresenceRecordGetUserState(p_record_handle, &user_state);
	if (FAILED(hr)) {
		return hr;
	}

	const XblPresenceDeviceRecord *device_records = nullptr;
	size_t device_record_count = 0;
	hr = XblPresenceRecordGetDeviceRecords(p_record_handle, &device_records, &device_record_count);
	if (FAILED(hr)) {
		return hr;
	}

	Array title_records;
	for (size_t device_index = 0; device_index < device_record_count; ++device_index) {
		const XblPresenceDeviceRecord &device_record = device_records[device_index];
		for (size_t title_index = 0; title_index < device_record.titleRecordsCount; ++title_index) {
			title_records.push_back(_make_title_record_dictionary(device_record.titleRecords[title_index], device_record.deviceType));
		}
	}

	set_data(String::num_uint64(xuid), _presence_user_state_to_enum(user_state), title_records);
	return S_OK;
}

void GDKPresenceRecord::populate_from_social_manager_record(uint64_t p_xuid, const XblSocialManagerPresenceRecord &p_record) {
	Array title_records;
	for (uint32_t i = 0; i < p_record.presenceTitleRecordCount && i < XBL_NUM_PRESENCE_RECORDS; ++i) {
		title_records.push_back(_make_social_title_record_dictionary(p_record.presenceTitleRecords[i]));
	}

	set_data(String::num_uint64(p_xuid), _presence_user_state_to_enum(p_record.userState), title_records);
}
#else
HRESULT GDKPresenceRecord::populate_from_presence_record(XblPresenceRecordHandle p_record_handle) {
	return E_NOTIMPL;
}

void GDKPresenceRecord::populate_from_social_manager_record(uint64_t p_xuid, const XblSocialManagerPresenceRecord &p_record) {
}
#endif

void GDKPresence::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_presence_async", "user", "state", "rich_presence"), &GDKPresence::set_presence_async, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("clear_presence_async", "user"), &GDKPresence::clear_presence_async);
	ClassDB::bind_method(D_METHOD("get_presence_async", "xuids"), &GDKPresence::get_presence_async);
	ClassDB::bind_method(D_METHOD("get_presence_for_social_group_async", "user", "social_group"), &GDKPresence::get_presence_for_social_group_async);
	ClassDB::bind_method(D_METHOD("track_presence", "user", "xuids", "title_ids"), &GDKPresence::track_presence, DEFVAL(PackedInt64Array()));
	ClassDB::bind_method(D_METHOD("stop_tracking_presence", "user", "xuids", "title_ids"), &GDKPresence::stop_tracking_presence, DEFVAL(PackedStringArray()), DEFVAL(PackedInt64Array()));
	ClassDB::bind_method(D_METHOD("get_cached_presence", "xuid"), &GDKPresence::get_cached_presence);

	ADD_SIGNAL(MethodInfo("presence_changed",
			PropertyInfo(Variant::STRING, "xuid"),
			PropertyInfo(Variant::OBJECT, "presence", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "GDKPresenceRecord")));
	ADD_SIGNAL(MethodInfo("local_presence_set", PropertyInfo(Variant::OBJECT, "user", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "GDKUser")));
	ADD_SIGNAL(MethodInfo("device_presence_changed", PropertyInfo(Variant::STRING, "xuid")));
	ADD_SIGNAL(MethodInfo("title_presence_changed", PropertyInfo(Variant::STRING, "xuid"), PropertyInfo(Variant::INT, "title_id")));
}

#ifdef XBOX_MODULE_GDK_ENABLED
void GDKPresence::set_owner(GDK *p_owner) {
	m_owner = p_owner;
}

Ref<GDKResult> GDKPresence::on_runtime_initialized() {
	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr || !runtime->is_initialized()) {
		return GDKResult::error_result(E_FAIL, "runtime_not_initialized", "Cannot initialize the presence service before the GDK runtime.");
	}

	m_runtime_ready = true;
	return GDKResult::ok_result();
}

void GDKPresence::shutdown() {
	m_runtime_ready = false;

	for (HandlerState &state : m_handler_states) {
		_close_handler_state(state);
	}
	m_handler_states.clear();
	{
		std::lock_guard<std::mutex> lock(m_pending_presence_events_mutex);
		m_pending_presence_events.clear();
	}
	m_cached_presence.clear();

	m_retired_callback_tokens.clear();
}

int GDKPresence::dispatch() {
	if (!m_runtime_ready) {
		return 0;
	}

	std::vector<PendingPresenceEvent> events;
	{
		std::lock_guard<std::mutex> lock(m_pending_presence_events_mutex);
		events.swap(m_pending_presence_events);
	}

	int dispatched = 0;
	for (const PendingPresenceEvent &event : events) {
		const String xuid = String::num_uint64(event.xuid);
		if (event.type == PendingPresenceEvent::EVENT_DEVICE) {
			emit_signal("device_presence_changed", xuid);
		} else {
			emit_signal("title_presence_changed", xuid, static_cast<int64_t>(event.title_id));
		}
		++dispatched;
		if (!m_runtime_ready) {
			break;
		}
	}
	return dispatched;
}

Signal GDKPresence::set_presence_async(const Ref<GDKUser> &p_user, const String &p_state, const Dictionary &p_rich_presence) {
	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr || !runtime->is_initialized()) {
		return _make_error_signal(E_FAIL, "not_initialized", "GDK is not initialized. Call GDK.initialize() first.");
	}
	if (!p_user.is_valid() || p_user->get_handle() == nullptr) {
		return _make_error_signal(E_INVALIDARG, "invalid_user", "A signed-in GDKUser is required for presence.");
	}

	const String presence_id = p_state.strip_edges();
	if (presence_id.is_empty()) {
		return _make_error_signal(E_INVALIDARG, "invalid_presence_state", "Presence updates require a non-empty state string.");
	}

	GDKXboxServices *xbox_services = _get_xbox_services();
	if (xbox_services == nullptr || !xbox_services->is_initialized()) {
		return _make_error_signal(E_FAIL, "xbox_services_not_initialized", "Xbox services are unavailable. Ensure the title has a TitleId before using presence.");
	}

	String scid = xbox_services->get_scid();
	if (p_rich_presence.has("scid")) {
		scid = String(p_rich_presence["scid"]).strip_edges();
	}
	if (scid.is_empty()) {
		return _make_error_signal(E_FAIL, "missing_presence_scid", "Presence updates require a non-empty SCID.");
	}

	Array token_ids;
	if (p_rich_presence.has("token_ids")) {
		Variant token_value = p_rich_presence["token_ids"];
		if (token_value.get_type() == Variant::ARRAY) {
			token_ids = token_value;
		} else if (token_value.get_type() == Variant::PACKED_STRING_ARRAY) {
			PackedStringArray packed_tokens = token_value;
			for (int64_t i = 0; i < packed_tokens.size(); ++i) {
				token_ids.push_back(packed_tokens[i]);
			}
		} else {
			return _make_error_signal(E_INVALIDARG, "invalid_presence_token_ids", "rich_presence.token_ids must be an Array or PackedStringArray.");
		}
	} else if (p_rich_presence.has("tokens")) {
		Variant token_value = p_rich_presence["tokens"];
		if (token_value.get_type() == Variant::ARRAY) {
			token_ids = token_value;
		} else if (token_value.get_type() == Variant::PACKED_STRING_ARRAY) {
			PackedStringArray packed_tokens = token_value;
			for (int64_t i = 0; i < packed_tokens.size(); ++i) {
				token_ids.push_back(packed_tokens[i]);
			}
		} else {
			return _make_error_signal(E_INVALIDARG, "invalid_presence_token_ids", "rich_presence.tokens must be an Array or PackedStringArray.");
		}
	}

	Ref<GDKPendingSignal> pending_signal = runtime->make_pending_signal();

	XblContextHandle context = nullptr;
	HRESULT hr = xbox_services->duplicate_context_for_user(p_user, &context);
	if (FAILED(hr)) {
		Ref<GDKResult> result = GDKResult::hresult_error(hr, "Failed to resolve the Xbox services context for the presence update.", "presence_context_failed");
		pending_signal->complete_deferred(result);
		return pending_signal->get_completed_signal();
	}

	auto *context_state = new SetPresenceAsyncContext(this, p_user, runtime, pending_signal, context, true, scid, presence_id, token_ids);
	context_state->bind_cancel_handler();

	hr = XblPresenceSetPresenceAsync(
			context_state->get_context(),
			true,
			context_state->get_rich_presence_ids(),
			context_state->get_async_block());
	if (FAILED(hr)) {
		pending_signal->clear_cancel_handler();
		delete context_state;

		Ref<GDKResult> result = GDKResult::hresult_error(hr, "Failed to start the presence update request.", "presence_update_start_failed");
		pending_signal->complete_deferred(result);
	}

	return pending_signal->get_completed_signal();
}

Signal GDKPresence::clear_presence_async(const Ref<GDKUser> &p_user) {
	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr || !runtime->is_initialized()) {
		return _make_error_signal(E_FAIL, "not_initialized", "GDK is not initialized. Call GDK.initialize() first.");
	}
	if (!p_user.is_valid() || p_user->get_handle() == nullptr) {
		return _make_error_signal(E_INVALIDARG, "invalid_user", "A signed-in GDKUser is required for presence.");
	}

	GDKXboxServices *xbox_services = _get_xbox_services();
	if (xbox_services == nullptr || !xbox_services->is_initialized()) {
		return _make_error_signal(E_FAIL, "xbox_services_not_initialized", "Xbox services are unavailable. Ensure the title has a TitleId before using presence.");
	}

	Ref<GDKPendingSignal> pending_signal = runtime->make_pending_signal();

	XblContextHandle context = nullptr;
	HRESULT hr = xbox_services->duplicate_context_for_user(p_user, &context);
	if (FAILED(hr)) {
		Ref<GDKResult> result = GDKResult::hresult_error(hr, "Failed to resolve the Xbox services context for the presence update.", "presence_context_failed");
		pending_signal->complete_deferred(result);
		return pending_signal->get_completed_signal();
	}

	auto *context_state = new SetPresenceAsyncContext(this, p_user, runtime, pending_signal, context, false, String(), String(), Array());
	context_state->bind_cancel_handler();

	hr = XblPresenceSetPresenceAsync(
			context_state->get_context(),
			false,
			nullptr,
			context_state->get_async_block());
	if (FAILED(hr)) {
		pending_signal->clear_cancel_handler();
		delete context_state;

		Ref<GDKResult> result = GDKResult::hresult_error(hr, "Failed to start the presence clear request.", "presence_clear_start_failed");
		pending_signal->complete_deferred(result);
	}

	return pending_signal->get_completed_signal();
}

Signal GDKPresence::get_presence_async(const PackedStringArray &p_xuids) {
	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr || !runtime->is_initialized()) {
		return _make_error_signal(E_FAIL, "not_initialized", "GDK is not initialized. Call GDK.initialize() first.");
	}

	std::vector<uint64_t> xuids;
	Ref<GDKResult> parse_result = _parse_query_xuids(p_xuids, &xuids);
	if (!parse_result->is_ok()) {
		return _make_error_signal(
				static_cast<HRESULT>(parse_result->get_hresult()),
				parse_result->get_code(),
				parse_result->get_message());
	}

	GDKXboxServices *xbox_services = _get_xbox_services();
	if (xbox_services == nullptr || !xbox_services->is_initialized()) {
		return _make_error_signal(E_FAIL, "xbox_services_not_initialized", "Xbox services are unavailable. Ensure the title has a TitleId before using presence.");
	}

	Ref<GDKUser> calling_user = _get_presence_calling_user();
	if (!calling_user.is_valid() || calling_user->get_handle() == nullptr) {
		return _make_error_signal(E_FAIL, "presence_requires_primary_user", "Presence queries require a signed-in primary user context.");
	}

	Ref<GDKPendingSignal> pending_signal = runtime->make_pending_signal();

	XblContextHandle context = nullptr;
	HRESULT hr = xbox_services->duplicate_context_for_user(calling_user, &context);
	if (FAILED(hr)) {
		Ref<GDKResult> result = GDKResult::hresult_error(hr, "Failed to resolve the Xbox services context for the presence query.", "presence_context_failed");
		pending_signal->complete_deferred(result);
		return pending_signal->get_completed_signal();
	}

	auto *context_state = new GetPresenceAsyncContext(this, runtime, pending_signal, context, std::move(xuids));
	context_state->bind_cancel_handler();

	if (context_state->get_xuids().size() == 1) {
		hr = XblPresenceGetPresenceAsync(
				context_state->get_context(),
				context_state->get_xuids().front(),
				context_state->get_async_block());
	} else {
		XblPresenceQueryFilters filters = {};
		filters.detailLevel = XblPresenceDetailLevel::All;
		hr = XblPresenceGetPresenceForMultipleUsersAsync(
				context_state->get_context(),
				const_cast<uint64_t *>(context_state->get_xuids().data()),
				context_state->get_xuids().size(),
				&filters,
				context_state->get_async_block());
	}

	if (FAILED(hr)) {
		pending_signal->clear_cancel_handler();
		delete context_state;

		Ref<GDKResult> result = GDKResult::hresult_error(hr, "Failed to start the presence query.", "presence_query_start_failed");
		pending_signal->complete_deferred(result);
	}

	return pending_signal->get_completed_signal();
}

Signal GDKPresence::get_presence_for_social_group_async(const Ref<GDKUser> &p_user, const String &p_social_group) {
	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr) {
		return Signal();
	}

	Ref<GDKResult> validation = _ensure_ready_user(p_user);
	if (!validation->is_ok()) {
		return _make_error_signal(static_cast<HRESULT>(validation->get_hresult()), validation->get_code(), validation->get_message());
	}

	const String social_group = p_social_group.strip_edges();
	if (social_group.is_empty()) {
		return _make_error_signal(E_INVALIDARG, "invalid_social_group", "Presence social-group queries require a non-empty social group name.");
	}

	GDKXboxServices *xbox_services = _get_xbox_services();
	if (xbox_services == nullptr || !xbox_services->is_initialized()) {
		return _make_error_signal(E_FAIL, "xbox_services_not_initialized", "Xbox services are unavailable. Ensure the title has a TitleId before using presence.");
	}

	XblContextHandle context = nullptr;
	HRESULT hr = xbox_services->duplicate_context_for_user(p_user, &context);
	if (FAILED(hr)) {
		return _make_error_signal(hr, "presence_context_failed", "Failed to resolve the Xbox services context for the social-group presence query.");
	}

	Ref<GDKPendingSignal> pending_signal = runtime->make_pending_signal();
	auto *context_state = new GetPresenceAsyncContext(this, runtime, pending_signal, context, social_group);
	context_state->bind_cancel_handler();

	hr = XblPresenceGetPresenceForSocialGroupAsync(
			context_state->get_context(),
			context_state->get_social_group_name(),
			nullptr,
			context_state->get_filters(),
			context_state->get_async_block());
	if (FAILED(hr)) {
		context_state->clear_cancel_handler();
		delete context_state;
		return _make_error_signal(hr, "presence_social_group_query_start_failed", "Failed to start the social-group presence query.");
	}

	return pending_signal->get_completed_signal();
}

Ref<GDKResult> GDKPresence::track_presence(const Ref<GDKUser> &p_user, const PackedStringArray &p_xuids, const PackedInt64Array &p_title_ids) {
	Ref<GDKResult> validation = _ensure_ready_user(p_user);
	if (!validation->is_ok()) {
		return validation;
	}

	std::vector<uint64_t> xuids;
	Ref<GDKResult> parse_result = _parse_xuids(p_xuids, false, "missing_presence_xuids", "invalid_presence_xuid", &xuids);
	if (!parse_result->is_ok()) {
		return parse_result;
	}

	std::vector<uint32_t> title_ids;
	parse_result = _parse_title_ids(p_title_ids, &title_ids);
	if (!parse_result->is_ok()) {
		return parse_result;
	}

	HandlerState *state = nullptr;
	Ref<GDKResult> handler_result = _ensure_handler_state(p_user, &state);
	if (!handler_result->is_ok()) {
		return handler_result;
	}

	HRESULT hr = XblPresenceTrackUsers(state->context, xuids.data(), xuids.size());
	if (FAILED(hr)) {
		return GDKResult::hresult_error(hr, "Failed to track Xbox Services presence users.", "presence_track_users_failed");
	}

	if (!title_ids.empty()) {
		hr = XblPresenceTrackAdditionalTitles(state->context, title_ids.data(), title_ids.size());
		if (FAILED(hr)) {
			return GDKResult::hresult_error(hr, "Failed to track additional Xbox Services presence titles.", "presence_track_titles_failed");
		}
	}

	for (uint64_t xuid : xuids) {
		if (std::find(state->tracked_xuids.begin(), state->tracked_xuids.end(), xuid) == state->tracked_xuids.end()) {
			state->tracked_xuids.push_back(xuid);
		}
	}
	for (uint32_t title_id : title_ids) {
		if (std::find(state->tracked_title_ids.begin(), state->tracked_title_ids.end(), title_id) == state->tracked_title_ids.end()) {
			state->tracked_title_ids.push_back(title_id);
		}
	}

	Dictionary data;
	data["tracked_users"] = static_cast<int64_t>(state->tracked_xuids.size());
	data["tracked_titles"] = static_cast<int64_t>(state->tracked_title_ids.size());
	return GDKResult::ok_result(data);
}

Ref<GDKResult> GDKPresence::stop_tracking_presence(const Ref<GDKUser> &p_user, const PackedStringArray &p_xuids, const PackedInt64Array &p_title_ids) {
	Ref<GDKResult> validation = _ensure_ready_user(p_user);
	if (!validation->is_ok()) {
		return validation;
	}

	std::vector<uint64_t> xuids;
	Ref<GDKResult> parse_result = _parse_xuids(p_xuids, true, "missing_presence_xuids", "invalid_presence_xuid", &xuids);
	if (!parse_result->is_ok()) {
		return parse_result;
	}

	std::vector<uint32_t> title_ids;
	parse_result = _parse_title_ids(p_title_ids, &title_ids);
	if (!parse_result->is_ok()) {
		return parse_result;
	}

	XUserLocalId local_id = {};
	local_id.value = static_cast<uint64_t>(p_user->get_local_id());
	HandlerState *state = _find_handler_state(local_id);
	if (state == nullptr) {
		return GDKResult::ok_result();
	}

	if (xuids.empty()) {
		xuids = state->tracked_xuids;
	}
	if (title_ids.empty()) {
		title_ids = state->tracked_title_ids;
	}

	if (!xuids.empty()) {
		HRESULT hr = XblPresenceStopTrackingUsers(state->context, xuids.data(), xuids.size());
		if (FAILED(hr)) {
			return GDKResult::hresult_error(hr, "Failed to stop tracking Xbox Services presence users.", "presence_stop_tracking_users_failed");
		}
		state->tracked_xuids.erase(
				std::remove_if(
						state->tracked_xuids.begin(),
						state->tracked_xuids.end(),
						[&xuids](uint64_t tracked_xuid) {
							return std::find(xuids.begin(), xuids.end(), tracked_xuid) != xuids.end();
						}),
				state->tracked_xuids.end());
	}

	if (!title_ids.empty()) {
		HRESULT hr = XblPresenceStopTrackingAdditionalTitles(state->context, title_ids.data(), title_ids.size());
		if (FAILED(hr)) {
			return GDKResult::hresult_error(hr, "Failed to stop tracking additional Xbox Services presence titles.", "presence_stop_tracking_titles_failed");
		}
		state->tracked_title_ids.erase(
				std::remove_if(
						state->tracked_title_ids.begin(),
						state->tracked_title_ids.end(),
						[&title_ids](uint32_t tracked_title_id) {
							return std::find(title_ids.begin(), title_ids.end(), tracked_title_id) != title_ids.end();
						}),
				state->tracked_title_ids.end());
	}

	Dictionary data;
	data["tracked_users"] = static_cast<int64_t>(state->tracked_xuids.size());
	data["tracked_titles"] = static_cast<int64_t>(state->tracked_title_ids.size());
	return GDKResult::ok_result(data);
}

Ref<GDKPresenceRecord> GDKPresence::get_cached_presence(const String &p_xuid) const {
	return _find_cached_presence(p_xuid.strip_edges());
}

void GDKPresence::cache_presence_record(const Ref<GDKPresenceRecord> &p_record, bool p_emit_signal) {
	if (!m_runtime_ready || !p_record.is_valid()) {
		return;
	}

	const String xuid = p_record->get_xuid();
	if (xuid.is_empty()) {
		return;
	}

	for (Ref<GDKPresenceRecord> &cached_record : m_cached_presence) {
		if (cached_record.is_valid() && cached_record->get_xuid() == xuid) {
			cached_record = p_record;
			if (p_emit_signal) {
				emit_signal("presence_changed", xuid, p_record);
			}
			return;
		}
	}

	m_cached_presence.push_back(p_record);
	if (p_emit_signal) {
		emit_signal("presence_changed", xuid, p_record);
	}
}

void GDKPresence::on_user_removed(const Ref<GDKUser> &p_user) {
	if (!p_user.is_valid()) {
		return;
	}

	XUserLocalId local_id = {};
	local_id.value = static_cast<uint64_t>(p_user->get_local_id());
	m_handler_states.erase(
			std::remove_if(
					m_handler_states.begin(),
					m_handler_states.end(),
					[this, local_id](HandlerState &state) {
						if (state.local_id.value != local_id.value) {
							return false;
						}
						_close_handler_state(state);
						return true;
					}),
			m_handler_states.end());

	_remove_cached_presence(p_user->get_xuid());
}

GDKRuntime *GDKPresence::_get_runtime() const {
	return m_owner != nullptr ? m_owner->get_runtime() : nullptr;
}

GDKXboxServices *GDKPresence::_get_xbox_services() const {
	return m_owner != nullptr ? m_owner->get_xbox_services() : nullptr;
}

Signal GDKPresence::_make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data) const {
	return _make_presence_error_signal(_get_runtime(), p_hresult, p_code, p_message, p_data);
}

Ref<GDKResult> GDKPresence::_ensure_ready_user(const Ref<GDKUser> &p_user) const {
	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr || !runtime->is_initialized() || !m_runtime_ready) {
		return GDKResult::error_result(E_FAIL, "not_initialized", "GDK is not initialized. Call GDK.initialize() first.");
	}
	if (!p_user.is_valid() || p_user->get_handle() == nullptr || !p_user->is_signed_in()) {
		return GDKResult::error_result(E_INVALIDARG, "invalid_user", "A signed-in GDKUser is required for presence.");
	}
	return GDKResult::ok_result();
}

Ref<GDKUser> GDKPresence::_get_presence_calling_user() const {
	if (m_owner == nullptr) {
		return Ref<GDKUser>();
	}

	Ref<GDKUsers> users = m_owner->get_users();
	if (!users.is_valid()) {
		return Ref<GDKUser>();
	}

	return users->get_primary_user();
}

Ref<GDKResult> GDKPresence::_parse_query_xuids(const PackedStringArray &p_xuids, std::vector<uint64_t> *r_xuids) const {
	return _parse_xuids(p_xuids, false, "missing_presence_xuids", "invalid_presence_xuid", r_xuids);
}

Ref<GDKResult> GDKPresence::_parse_xuids(const PackedStringArray &p_xuids, bool p_allow_empty, const String &p_missing_code, const String &p_invalid_code, std::vector<uint64_t> *r_xuids) const {
	ERR_FAIL_COND_V(r_xuids == nullptr, GDKResult::error_result(E_POINTER, "internal_error", "Missing presence query output vector."));

	r_xuids->clear();
	if (p_xuids.is_empty()) {
		return p_allow_empty ? GDKResult::ok_result() : GDKResult::error_result(E_INVALIDARG, p_missing_code, "Presence operations require at least one XUID.");
	}

	r_xuids->reserve(static_cast<size_t>(p_xuids.size()));
	for (int64_t i = 0; i < p_xuids.size(); ++i) {
		uint64_t xuid = 0;
		if (!_try_parse_xuid(p_xuids[i], &xuid)) {
			return GDKResult::error_result(E_INVALIDARG, p_invalid_code, "Presence operations require numeric XUID strings.");
		}
		r_xuids->push_back(xuid);
	}

	return GDKResult::ok_result();
}

Ref<GDKResult> GDKPresence::_parse_title_ids(const PackedInt64Array &p_title_ids, std::vector<uint32_t> *r_title_ids) const {
	ERR_FAIL_COND_V(r_title_ids == nullptr, GDKResult::error_result(E_POINTER, "internal_error", "Missing presence title-id output vector."));

	r_title_ids->clear();
	r_title_ids->reserve(static_cast<size_t>(p_title_ids.size()));
	for (int64_t i = 0; i < p_title_ids.size(); ++i) {
		const int64_t title_id = p_title_ids[i];
		if (title_id < 0 || title_id > UINT32_MAX) {
			return GDKResult::error_result(E_INVALIDARG, "invalid_title_id", "Presence title IDs must be unsigned 32-bit values.");
		}
		r_title_ids->push_back(static_cast<uint32_t>(title_id));
	}

	return GDKResult::ok_result();
}

Ref<GDKPresenceRecord> GDKPresence::_find_cached_presence(const String &p_xuid) const {
	for (const Ref<GDKPresenceRecord> &record : m_cached_presence) {
		if (record.is_valid() && record->get_xuid() == p_xuid) {
			return record;
		}
	}

	return Ref<GDKPresenceRecord>();
}

void GDKPresence::_remove_cached_presence(const String &p_xuid) {
	m_cached_presence.erase(
			std::remove_if(
					m_cached_presence.begin(),
					m_cached_presence.end(),
					[&p_xuid](const Ref<GDKPresenceRecord> &record) {
						return record.is_null() || record->get_xuid() == p_xuid;
					}),
			m_cached_presence.end());
}

GDKPresence::HandlerState *GDKPresence::_find_handler_state(XUserLocalId p_local_id) {
	for (HandlerState &state : m_handler_states) {
		if (state.local_id.value == p_local_id.value) {
			return &state;
		}
	}
	return nullptr;
}

Ref<GDKResult> GDKPresence::_ensure_handler_state(const Ref<GDKUser> &p_user, HandlerState **r_state) {
	ERR_FAIL_COND_V(r_state == nullptr, GDKResult::error_result(E_POINTER, "internal_error", "Missing presence handler-state output."));
	*r_state = nullptr;

	XUserLocalId local_id = {};
	local_id.value = static_cast<uint64_t>(p_user->get_local_id());
	HandlerState *existing_state = _find_handler_state(local_id);
	if (existing_state != nullptr) {
		*r_state = existing_state;
		return GDKResult::ok_result();
	}

	GDKXboxServices *xbox_services = _get_xbox_services();
	if (xbox_services == nullptr || !xbox_services->is_initialized()) {
		return GDKResult::error_result(E_FAIL, "xbox_services_not_initialized", "Xbox services are unavailable. Ensure the title has a TitleId before using presence.");
	}

	HandlerState state;
	state.user = p_user;
	state.local_id = local_id;

	HRESULT hr = xbox_services->duplicate_context_for_user(p_user, &state.context);
	if (FAILED(hr)) {
		return GDKResult::hresult_error(hr, "Failed to resolve the Xbox services context for presence tracking.", "presence_context_failed");
	}

	state.callback_context = std::make_shared<HandlerState::CallbackContext>();
	state.callback_context->presence = this;
	state.callback_context->local_id = local_id;
	state.callback_token = std::make_shared<HandlerState::CallbackToken>();
	state.callback_token->context = state.callback_context;
	state.device_token = XblPresenceAddDevicePresenceChangedHandler(state.context, _device_presence_changed_handler, state.callback_token.get());
	state.title_token = XblPresenceAddTitlePresenceChangedHandler(state.context, _title_presence_changed_handler, state.callback_token.get());
	state.device_registered = true;
	state.title_registered = true;

	m_handler_states.push_back(state);
	*r_state = &m_handler_states.back();
	return GDKResult::ok_result();
}

void GDKPresence::_close_handler_state(HandlerState &p_state) {
	if (p_state.callback_context) {
		std::lock_guard<std::mutex> lock(p_state.callback_context->mutex);
		p_state.callback_context->active.store(false, std::memory_order_release);
		p_state.callback_context->presence = nullptr;
	}

	if (p_state.context != nullptr) {
		if (p_state.device_registered) {
			XblPresenceRemoveDevicePresenceChangedHandler(p_state.context, p_state.device_token);
			p_state.device_registered = false;
		}
		if (p_state.title_registered) {
			XblPresenceRemoveTitlePresenceChangedHandler(p_state.context, p_state.title_token);
			p_state.title_registered = false;
		}
		XblContextCloseHandle(p_state.context);
		p_state.context = nullptr;
	}

	if (p_state.callback_token) {
		constexpr size_t MAX_RETIRED_CALLBACK_TOKENS = 16;
		m_retired_callback_tokens.push_back(p_state.callback_token);
		if (m_retired_callback_tokens.size() > MAX_RETIRED_CALLBACK_TOKENS) {
			m_retired_callback_tokens.erase(m_retired_callback_tokens.begin());
		}
		p_state.callback_token.reset();
	}
	p_state.callback_context.reset();
}

void CALLBACK GDKPresence::_device_presence_changed_handler(void *p_context, uint64_t p_xuid, XblPresenceDeviceType p_device_type, bool p_is_user_logged_on_device) {
	HandlerState::CallbackToken *token = static_cast<HandlerState::CallbackToken *>(p_context);
	std::shared_ptr<HandlerState::CallbackContext> callback_context = token != nullptr ? token->context.lock() : nullptr;
	if (!callback_context || !callback_context->active.load(std::memory_order_acquire)) {
		return;
	}

	PendingPresenceEvent event;
	event.type = PendingPresenceEvent::EVENT_DEVICE;
	event.local_id = callback_context->local_id;
	event.xuid = p_xuid;
	event.device_type = p_device_type;
	event.device_logged_on = p_is_user_logged_on_device;

	std::lock_guard<std::mutex> context_lock(callback_context->mutex);
	GDKPresence *presence = callback_context->presence;
	if (!callback_context->active.load(std::memory_order_acquire) || presence == nullptr) {
		return;
	}

	std::lock_guard<std::mutex> queue_lock(presence->m_pending_presence_events_mutex);
	presence->m_pending_presence_events.push_back(event);
}

void CALLBACK GDKPresence::_title_presence_changed_handler(void *p_context, uint64_t p_xuid, uint32_t p_title_id, XblPresenceTitleState p_title_state) {
	HandlerState::CallbackToken *token = static_cast<HandlerState::CallbackToken *>(p_context);
	std::shared_ptr<HandlerState::CallbackContext> callback_context = token != nullptr ? token->context.lock() : nullptr;
	if (!callback_context || !callback_context->active.load(std::memory_order_acquire)) {
		return;
	}

	PendingPresenceEvent event;
	event.type = PendingPresenceEvent::EVENT_TITLE;
	event.local_id = callback_context->local_id;
	event.xuid = p_xuid;
	event.title_id = p_title_id;
	event.title_state = p_title_state;

	std::lock_guard<std::mutex> context_lock(callback_context->mutex);
	GDKPresence *presence = callback_context->presence;
	if (!callback_context->active.load(std::memory_order_acquire) || presence == nullptr) {
		return;
	}

	std::lock_guard<std::mutex> queue_lock(presence->m_pending_presence_events_mutex);
	presence->m_pending_presence_events.push_back(event);
}

#else
void GDKPresence::set_owner(GDK *p_owner) {}

Ref<GDKResult> GDKPresence::on_runtime_initialized() {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

void GDKPresence::shutdown() {}

int GDKPresence::dispatch() {
	return 0;
}

Signal GDKPresence::set_presence_async(const Ref<GDKUser> &p_user, const String &p_state, const Dictionary &p_rich_presence) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKPresence::clear_presence_async(const Ref<GDKUser> &p_user) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKPresence::get_presence_async(const PackedStringArray &p_xuids) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKPresence::get_presence_for_social_group_async(const Ref<GDKUser> &p_user, const String &p_social_group) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Ref<GDKResult> GDKPresence::track_presence(const Ref<GDKUser> &p_user, const PackedStringArray &p_xuids, const PackedInt64Array &p_title_ids) {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Ref<GDKResult> GDKPresence::stop_tracking_presence(const Ref<GDKUser> &p_user, const PackedStringArray &p_xuids, const PackedInt64Array &p_title_ids) {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Ref<GDKPresenceRecord> GDKPresence::get_cached_presence(const String &p_xuid) const {
	return Ref<GDKPresenceRecord>();
}

void GDKPresence::cache_presence_record(const Ref<GDKPresenceRecord> &p_record, bool p_emit_signal) {}

void GDKPresence::on_user_removed(const Ref<GDKUser> &p_user) {}

Signal GDKPresence::_make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data) const {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Ref<GDKResult> GDKPresence::_ensure_ready_user(const Ref<GDKUser> &p_user) const {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Ref<GDKUser> GDKPresence::_get_presence_calling_user() const {
	return Ref<GDKUser>();
}

Ref<GDKResult> GDKPresence::_parse_query_xuids(const PackedStringArray &p_xuids, std::vector<uint64_t> *r_xuids) const {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Ref<GDKResult> GDKPresence::_parse_xuids(const PackedStringArray &p_xuids, bool p_allow_empty, const String &p_missing_code, const String &p_invalid_code, std::vector<uint64_t> *r_xuids) const {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Ref<GDKResult> GDKPresence::_parse_title_ids(const PackedInt64Array &p_title_ids, std::vector<uint32_t> *r_title_ids) const {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Ref<GDKPresenceRecord> GDKPresence::_find_cached_presence(const String &p_xuid) const {
	return Ref<GDKPresenceRecord>();
}

void GDKPresence::_remove_cached_presence(const String &p_xuid) {}

Ref<GDKResult> GDKPresence::_ensure_handler_state(const Ref<GDKUser> &p_user, HandlerState **r_state) {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

void GDKPresence::_close_handler_state(HandlerState &p_state) {}
#endif
