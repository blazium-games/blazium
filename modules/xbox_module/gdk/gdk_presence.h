/**************************************************************************/
/*  gdk_presence.h                                                        */
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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifdef XBOX_MODULE_GDK_ENABLED
#include "gdk_windows.h"
#endif

#include "gdk_gdk_stubs.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include "core/object/class_db.h"
#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/variant/array.h"
#include "core/variant/callable.h"
#include "core/variant/dictionary.h"
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

class GDKPresenceRecord : public RefCounted {
	GDCLASS(GDKPresenceRecord, RefCounted);

public:
	enum UserState {
		USER_STATE_UNKNOWN = 0,
		USER_STATE_ONLINE,
		USER_STATE_AWAY,
		USER_STATE_OFFLINE,
	};

private:
	String m_xuid;
	UserState m_user_state = USER_STATE_UNKNOWN;
	Array m_title_records;

protected:
	static void _bind_methods();

public:
	String get_xuid() const;
	UserState get_user_state() const;
	String get_user_state_name() const;
	bool is_online() const;
	Array get_title_records() const;

	void set_data(const String &p_xuid, UserState p_user_state, const Array &p_title_records);
	HRESULT populate_from_presence_record(XblPresenceRecordHandle p_record_handle);
	void populate_from_social_manager_record(uint64_t p_xuid, const XblSocialManagerPresenceRecord &p_record);
};

class GDKPresence : public RefCounted {
	GDCLASS(GDKPresence, RefCounted);

	GDK *m_owner = nullptr;
	bool m_runtime_ready = false;
	std::vector<Ref<GDKPresenceRecord>> m_cached_presence;

	struct HandlerState {
		struct CallbackContext {
			GDKPresence *presence = nullptr;
			XUserLocalId local_id = {};
			std::atomic_bool active = true;
			std::mutex mutex;
		};

		struct CallbackToken {
			std::weak_ptr<CallbackContext> context;
		};

		Ref<GDKUser> user;
		XUserLocalId local_id = {};
		XblContextHandle context = nullptr;
		XblFunctionContext device_token = {};
		XblFunctionContext title_token = {};
		bool device_registered = false;
		bool title_registered = false;
		std::shared_ptr<CallbackContext> callback_context;
		std::shared_ptr<CallbackToken> callback_token;
		std::vector<uint64_t> tracked_xuids;
		std::vector<uint32_t> tracked_title_ids;
	};

	struct PendingPresenceEvent {
		enum EventType {
			EVENT_DEVICE,
			EVENT_TITLE,
		};

		EventType type = EVENT_DEVICE;
		XUserLocalId local_id = {};
		uint64_t xuid = 0;
		XblPresenceDeviceType device_type = XblPresenceDeviceType::Unknown;
		bool device_logged_on = false;
		uint32_t title_id = 0;
		XblPresenceTitleState title_state = XblPresenceTitleState::Unknown;
	};

	std::vector<HandlerState> m_handler_states;
	std::vector<PendingPresenceEvent> m_pending_presence_events;
	mutable std::mutex m_pending_presence_events_mutex;
	std::vector<std::shared_ptr<HandlerState::CallbackToken>> m_retired_callback_tokens;

	GDKRuntime *_get_runtime() const;
	GDKXboxServices *_get_xbox_services() const;
	Signal _make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data = Variant()) const;
	Ref<GDKResult> _ensure_ready_user(const Ref<GDKUser> &p_user) const;
	Ref<GDKUser> _get_presence_calling_user() const;
	Ref<GDKResult> _parse_query_xuids(const PackedStringArray &p_xuids, std::vector<uint64_t> *r_xuids) const;
	Ref<GDKResult> _parse_xuids(const PackedStringArray &p_xuids, bool p_allow_empty, const String &p_missing_code, const String &p_invalid_code, std::vector<uint64_t> *r_xuids) const;
	Ref<GDKResult> _parse_title_ids(const PackedInt64Array &p_title_ids, std::vector<uint32_t> *r_title_ids) const;
	Ref<GDKPresenceRecord> _find_cached_presence(const String &p_xuid) const;
	void _remove_cached_presence(const String &p_xuid);
	HandlerState *_find_handler_state(XUserLocalId p_local_id);
	Ref<GDKResult> _ensure_handler_state(const Ref<GDKUser> &p_user, HandlerState **r_state);
	void _close_handler_state(HandlerState &p_state);
	static void CALLBACK _device_presence_changed_handler(void *p_context, uint64_t p_xuid, XblPresenceDeviceType p_device_type, bool p_is_user_logged_on_device);
	static void CALLBACK _title_presence_changed_handler(void *p_context, uint64_t p_xuid, uint32_t p_title_id, XblPresenceTitleState p_title_state);

protected:
	static void _bind_methods();

public:
	void set_owner(GDK *p_owner);

	Ref<GDKResult> on_runtime_initialized();
	void shutdown();
	int dispatch();

	Signal set_presence_async(const Ref<GDKUser> &p_user, const String &p_state, const Dictionary &p_rich_presence = Dictionary());
	Signal clear_presence_async(const Ref<GDKUser> &p_user);
	Signal get_presence_async(const PackedStringArray &p_xuids);
	Signal get_presence_for_social_group_async(const Ref<GDKUser> &p_user, const String &p_social_group);
	Ref<GDKResult> track_presence(const Ref<GDKUser> &p_user, const PackedStringArray &p_xuids, const PackedInt64Array &p_title_ids = PackedInt64Array());
	Ref<GDKResult> stop_tracking_presence(const Ref<GDKUser> &p_user, const PackedStringArray &p_xuids = PackedStringArray(), const PackedInt64Array &p_title_ids = PackedInt64Array());
	Ref<GDKPresenceRecord> get_cached_presence(const String &p_xuid) const;

	void cache_presence_record(const Ref<GDKPresenceRecord> &p_record, bool p_emit_signal = true);
	void on_user_removed(const Ref<GDKUser> &p_user);
};

VARIANT_ENUM_CAST(GDKPresenceRecord::UserState);
