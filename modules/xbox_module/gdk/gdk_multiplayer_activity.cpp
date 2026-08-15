/**************************************************************************/
/*  gdk_multiplayer_activity.cpp                                          */
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
#include "gdk_multiplayer_activity.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifdef XBOX_MODULE_GDK_ENABLED
#include "gdk_windows.h"
#include <XGameUI.h>
#endif

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "gdk.h"
#include "gdk_activation.h"
#include "gdk_pending_signal.h"
#include "gdk_result.h"
#include "gdk_runtime.h"
#include "gdk_signal_xasync_context.h"
#include "gdk_user.h"
#include "gdk_xbox_services.h"

#ifdef XBOX_MODULE_GDK_ENABLED

namespace {

String _utf8_or_empty(const char *p_value) {
	return p_value != nullptr ? String::utf8(p_value) : String();
}

Ref<GDKResult> _parse_xuids(
		const PackedStringArray &p_xuids,
		std::vector<uint64_t> *r_xuids,
		std::vector<String> *r_normalized_xuids,
		size_t p_max_count,
		const String &p_empty_code,
		const String &p_invalid_code,
		const String &p_invalid_message) {
	ERR_FAIL_COND_V(r_xuids == nullptr, GDKResult::error_result(E_POINTER, "internal_error", "Missing XUID output buffer."));

	if (p_xuids.is_empty()) {
		return GDKResult::error_result(E_INVALIDARG, p_empty_code, "At least one XUID is required.");
	}
	if (p_max_count > 0 && static_cast<size_t>(p_xuids.size()) > p_max_count) {
		return GDKResult::error_result(E_INVALIDARG, p_invalid_code, "Too many XUIDs were provided for this request.");
	}

	r_xuids->clear();
	r_xuids->reserve(static_cast<size_t>(p_xuids.size()));

	if (r_normalized_xuids != nullptr) {
		r_normalized_xuids->clear();
		r_normalized_xuids->reserve(static_cast<size_t>(p_xuids.size()));
	}

	for (int64_t i = 0; i < p_xuids.size(); ++i) {
		const String xuid_string = p_xuids[i].strip_edges();
		uint64_t xuid = 0;
		if (!GDKMultiplayerActivity::try_parse_xuid_internal(xuid_string, &xuid)) {
			return GDKResult::error_result(E_INVALIDARG, p_invalid_code, p_invalid_message);
		}

		r_xuids->push_back(xuid);
		if (r_normalized_xuids != nullptr) {
			r_normalized_xuids->push_back(String::num_uint64(xuid));
		}
	}

	return GDKResult::ok_result();
}

class MultiplayerActivityXAsyncContext : public GDKSignalXAsyncContext {
protected:
	GDKMultiplayerActivity *m_service = nullptr;
	XblContextHandle m_context = nullptr;

public:
	MultiplayerActivityXAsyncContext(
			GDKMultiplayerActivity *p_service,
			GDKRuntime *p_runtime,
			const Ref<GDKPendingSignal> &p_pending_signal,
			XblContextHandle p_context) :
			GDKSignalXAsyncContext(p_runtime, p_pending_signal),
			m_service(p_service),
			m_context(p_context) {}

	~MultiplayerActivityXAsyncContext() override {
		if (m_context != nullptr) {
			XblContextCloseHandle(m_context);
			m_context = nullptr;
		}
	}

	XblContextHandle get_context() const {
		return m_context;
	}
};

class SetActivityAsyncContext final : public MultiplayerActivityXAsyncContext {
	String m_local_xuid;
	String m_connection_string;
	String m_join_restriction;
	int64_t m_max_players = 0;
	int64_t m_current_players = 0;
	String m_group_id;

protected:
	void finalize(XAsyncBlock *p_async_block) override {
		Ref<GDKResult> result;

		if (get_runtime()->is_shutting_down() || get_pending_signal()->was_cancel_requested()) {
			result = GDKResult::cancelled("Multiplayer activity update cancelled.");
			get_pending_signal()->complete(result);
			return;
		}

		HRESULT result_hr = XAsyncGetStatus(p_async_block, false);
		if (result_hr == E_ABORT) {
			result = GDKResult::cancelled("Multiplayer activity update cancelled.");
			get_pending_signal()->complete(result);
			return;
		}

		if (FAILED(result_hr)) {
			result = GDKResult::hresult_error(
					result_hr,
					"Failed to set the multiplayer activity.",
					"multiplayer_activity_set_result_failed");
			get_pending_signal()->complete(result);
			return;
		}

		Ref<GDKMultiplayerActivityInfo> activity;
		activity.instantiate();
		activity->set_values(
				m_local_xuid,
				m_connection_string,
				m_join_restriction,
				m_max_players,
				m_current_players,
				m_group_id,
				"unknown");
		m_service->cache_activity_internal(activity);
		m_service->emit_activities_updated_internal({ m_local_xuid });

		get_pending_signal()->complete(GDKResult::ok_result(activity));
	}

public:
	SetActivityAsyncContext(
			GDKMultiplayerActivity *p_service,
			GDKRuntime *p_runtime,
			const Ref<GDKPendingSignal> &p_pending_signal,
			XblContextHandle p_context,
			const String &p_local_xuid,
			const String &p_connection_string,
			const String &p_join_restriction,
			int64_t p_max_players,
			int64_t p_current_players,
			const String &p_group_id) :
			MultiplayerActivityXAsyncContext(p_service, p_runtime, p_pending_signal, p_context),
			m_local_xuid(p_local_xuid),
			m_connection_string(p_connection_string),
			m_join_restriction(p_join_restriction),
			m_max_players(p_max_players),
			m_current_players(p_current_players),
			m_group_id(p_group_id) {}
};

class GetActivitiesAsyncContext final : public MultiplayerActivityXAsyncContext {
protected:
	void finalize(XAsyncBlock *p_async_block) override {
		Ref<GDKResult> result;

		if (get_runtime()->is_shutting_down() || get_pending_signal()->was_cancel_requested()) {
			result = GDKResult::cancelled("Multiplayer activity query cancelled.");
			get_pending_signal()->complete(result);
			return;
		}

		size_t buffer_size = 0;
		HRESULT size_hr = XblMultiplayerActivityGetActivityResultSize(p_async_block, &buffer_size);
		if (size_hr == E_ABORT) {
			result = GDKResult::cancelled("Multiplayer activity query cancelled.");
			get_pending_signal()->complete(result);
			return;
		}

		if (FAILED(size_hr)) {
			result = GDKResult::hresult_error(
					size_hr,
					"Failed to get the multiplayer activity result size.",
					"multiplayer_activity_get_result_size_failed");
			get_pending_signal()->complete(result);
			return;
		}

		std::vector<uint8_t> buffer(buffer_size);
		XblMultiplayerActivityInfo *activities = nullptr;
		size_t result_count = 0;
		HRESULT result_hr = XblMultiplayerActivityGetActivityResult(
				p_async_block,
				buffer.size(),
				buffer.empty() ? nullptr : buffer.data(),
				&activities,
				&result_count,
				nullptr);
		if (result_hr == E_ABORT) {
			result = GDKResult::cancelled("Multiplayer activity query cancelled.");
			get_pending_signal()->complete(result);
			return;
		}

		if (FAILED(result_hr)) {
			result = GDKResult::hresult_error(
					result_hr,
					"Failed to translate the multiplayer activity result.",
					"multiplayer_activity_get_result_failed");
			get_pending_signal()->complete(result);
			return;
		}

		Array godot_activities;
		std::vector<String> updated_xuids;
		updated_xuids.reserve(result_count);
		for (size_t i = 0; i < result_count; ++i) {
			Ref<GDKMultiplayerActivityInfo> activity;
			activity.instantiate();
			activity->populate_from_native(activities[i]);
			m_service->cache_activity_internal(activity);
			godot_activities.push_back(activity);
			updated_xuids.push_back(activity->get_xuid());
		}

		m_service->emit_activities_updated_internal(updated_xuids);
		get_pending_signal()->complete(GDKResult::ok_result(godot_activities));
	}

public:
	GetActivitiesAsyncContext(
			GDKMultiplayerActivity *p_service,
			GDKRuntime *p_runtime,
			const Ref<GDKPendingSignal> &p_pending_signal,
			XblContextHandle p_context) :
			MultiplayerActivityXAsyncContext(p_service, p_runtime, p_pending_signal, p_context) {}
};

class DeleteActivityAsyncContext final : public MultiplayerActivityXAsyncContext {
	String m_local_xuid;

protected:
	void finalize(XAsyncBlock *p_async_block) override {
		Ref<GDKResult> result;

		if (get_runtime()->is_shutting_down() || get_pending_signal()->was_cancel_requested()) {
			result = GDKResult::cancelled("Multiplayer activity delete cancelled.");
			get_pending_signal()->complete(result);
			return;
		}

		HRESULT result_hr = XAsyncGetStatus(p_async_block, false);
		if (result_hr == E_ABORT) {
			result = GDKResult::cancelled("Multiplayer activity delete cancelled.");
			get_pending_signal()->complete(result);
			return;
		}

		if (FAILED(result_hr)) {
			result = GDKResult::hresult_error(
					result_hr,
					"Failed to delete the multiplayer activity.",
					"multiplayer_activity_delete_result_failed");
			get_pending_signal()->complete(result);
			return;
		}

		m_service->remove_cached_activity_internal(m_local_xuid);
		m_service->emit_activities_updated_internal({ m_local_xuid });
		get_pending_signal()->complete(GDKResult::ok_result(m_local_xuid));
	}

public:
	DeleteActivityAsyncContext(
			GDKMultiplayerActivity *p_service,
			GDKRuntime *p_runtime,
			const Ref<GDKPendingSignal> &p_pending_signal,
			XblContextHandle p_context,
			const String &p_local_xuid) :
			MultiplayerActivityXAsyncContext(p_service, p_runtime, p_pending_signal, p_context),
			m_local_xuid(p_local_xuid) {}
};

class SendInvitesAsyncContext final : public MultiplayerActivityXAsyncContext {
	size_t m_recipient_count = 0;
	bool m_allow_cross_platform_join = false;
	String m_connection_string;

protected:
	void finalize(XAsyncBlock *p_async_block) override {
		Ref<GDKResult> result;

		if (get_runtime()->is_shutting_down() || get_pending_signal()->was_cancel_requested()) {
			result = GDKResult::cancelled("Multiplayer invite send cancelled.");
			get_pending_signal()->complete(result);
			return;
		}

		HRESULT result_hr = XAsyncGetStatus(p_async_block, false);
		if (result_hr == E_ABORT) {
			result = GDKResult::cancelled("Multiplayer invite send cancelled.");
			get_pending_signal()->complete(result);
			return;
		}

		if (FAILED(result_hr)) {
			result = GDKResult::hresult_error(
					result_hr,
					"Failed to send multiplayer invites.",
					"multiplayer_activity_send_invites_result_failed");
			get_pending_signal()->complete(result);
			return;
		}

		Dictionary data;
		data["recipient_count"] = static_cast<int64_t>(m_recipient_count);
		data["allow_cross_platform_join"] = m_allow_cross_platform_join;
		data["connection_string"] = m_connection_string;

		get_pending_signal()->complete(GDKResult::ok_result(data));
	}

public:
	SendInvitesAsyncContext(
			GDKMultiplayerActivity *p_service,
			GDKRuntime *p_runtime,
			const Ref<GDKPendingSignal> &p_pending_signal,
			XblContextHandle p_context,
			size_t p_recipient_count,
			bool p_allow_cross_platform_join,
			const String &p_connection_string) :
			MultiplayerActivityXAsyncContext(p_service, p_runtime, p_pending_signal, p_context),
			m_recipient_count(p_recipient_count),
			m_allow_cross_platform_join(p_allow_cross_platform_join),
			m_connection_string(p_connection_string) {}
};

class InviteUiAsyncContext final : public GDKSignalXAsyncContext {
protected:
	void finalize(XAsyncBlock *p_async_block) override {
		Ref<GDKResult> result;

		if (get_runtime()->is_shutting_down() || get_pending_signal()->was_cancel_requested()) {
			result = GDKResult::cancelled("Invite UI cancelled.");
			get_pending_signal()->complete(result);
			return;
		}

		HRESULT result_hr = XGameUiShowMultiplayerActivityGameInviteResult(p_async_block);
		if (result_hr == E_ABORT) {
			result = GDKResult::cancelled("Invite UI cancelled.");
			get_pending_signal()->complete(result);
			return;
		}

		if (FAILED(result_hr)) {
			result = GDKResult::hresult_error(
					result_hr,
					"Failed to complete the multiplayer invite UI flow.",
					"multiplayer_activity_invite_ui_result_failed");
			get_pending_signal()->complete(result);
			return;
		}

		get_pending_signal()->complete(GDKResult::ok_result());
	}

public:
	InviteUiAsyncContext(GDKRuntime *p_runtime, const Ref<GDKPendingSignal> &p_pending_signal) :
			GDKSignalXAsyncContext(p_runtime, p_pending_signal) {}
};

class FlushRecentPlayersAsyncContext final : public MultiplayerActivityXAsyncContext {
protected:
	void finalize(XAsyncBlock *p_async_block) override {
		Ref<GDKResult> result;

		if (get_runtime()->is_shutting_down() || get_pending_signal()->was_cancel_requested()) {
			result = GDKResult::cancelled("Recent-player flush cancelled.");
			get_pending_signal()->complete(result);
			return;
		}

		HRESULT result_hr = XAsyncGetStatus(p_async_block, false);
		if (result_hr == E_ABORT) {
			result = GDKResult::cancelled("Recent-player flush cancelled.");
			get_pending_signal()->complete(result);
			return;
		}

		if (FAILED(result_hr)) {
			result = GDKResult::hresult_error(
					result_hr,
					"Failed to flush recent-player updates.",
					"multiplayer_activity_flush_recent_players_result_failed");
			get_pending_signal()->complete(result);
			return;
		}

		get_pending_signal()->complete(GDKResult::ok_result());
	}

public:
	FlushRecentPlayersAsyncContext(
			GDKMultiplayerActivity *p_service,
			GDKRuntime *p_runtime,
			const Ref<GDKPendingSignal> &p_pending_signal,
			XblContextHandle p_context) :
			MultiplayerActivityXAsyncContext(p_service, p_runtime, p_pending_signal, p_context) {}
};

} //namespace

#endif

void GDKMultiplayerActivityInfo::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_xuid"), &GDKMultiplayerActivityInfo::get_xuid);
	ClassDB::bind_method(D_METHOD("get_connection_string"), &GDKMultiplayerActivityInfo::get_connection_string);
	ClassDB::bind_method(D_METHOD("get_join_restriction"), &GDKMultiplayerActivityInfo::get_join_restriction);
	ClassDB::bind_method(D_METHOD("get_max_players"), &GDKMultiplayerActivityInfo::get_max_players);
	ClassDB::bind_method(D_METHOD("get_current_players"), &GDKMultiplayerActivityInfo::get_current_players);
	ClassDB::bind_method(D_METHOD("get_group_id"), &GDKMultiplayerActivityInfo::get_group_id);
	ClassDB::bind_method(D_METHOD("get_platform"), &GDKMultiplayerActivityInfo::get_platform);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "xuid"), "", "get_xuid");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "connection_string"), "", "get_connection_string");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "join_restriction"), "", "get_join_restriction");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_players"), "", "get_max_players");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "current_players"), "", "get_current_players");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "group_id"), "", "get_group_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "platform"), "", "get_platform");
}

String GDKMultiplayerActivityInfo::get_xuid() const {
	return m_xuid;
}

String GDKMultiplayerActivityInfo::get_connection_string() const {
	return m_connection_string;
}

String GDKMultiplayerActivityInfo::get_join_restriction() const {
	return m_join_restriction;
}

int64_t GDKMultiplayerActivityInfo::get_max_players() const {
	return m_max_players;
}

int64_t GDKMultiplayerActivityInfo::get_current_players() const {
	return m_current_players;
}

String GDKMultiplayerActivityInfo::get_group_id() const {
	return m_group_id;
}

String GDKMultiplayerActivityInfo::get_platform() const {
	return m_platform;
}

void GDKMultiplayerActivityInfo::set_values(
		const String &p_xuid,
		const String &p_connection_string,
		const String &p_join_restriction,
		int64_t p_max_players,
		int64_t p_current_players,
		const String &p_group_id,
		const String &p_platform) {
	m_xuid = p_xuid;
	m_connection_string = p_connection_string;
	m_join_restriction = p_join_restriction;
	m_max_players = p_max_players;
	m_current_players = p_current_players;
	m_group_id = p_group_id;
	m_platform = p_platform;
}

#ifdef XBOX_MODULE_GDK_ENABLED
void GDKMultiplayerActivityInfo::populate_from_native(const XblMultiplayerActivityInfo &p_native_activity) {
	set_values(
			String::num_uint64(p_native_activity.xuid),
			_utf8_or_empty(p_native_activity.connectionString),
			GDKMultiplayerActivity::join_restriction_to_string_internal(p_native_activity.joinRestriction),
			static_cast<int64_t>(p_native_activity.maxPlayers),
			static_cast<int64_t>(p_native_activity.currentPlayers),
			_utf8_or_empty(p_native_activity.groupId),
			GDKMultiplayerActivity::platform_to_string_internal(p_native_activity.platform));
}
#else
void GDKMultiplayerActivityInfo::populate_from_native(const XblMultiplayerActivityInfo &p_native_activity) {
}
#endif

void GDKMultiplayerActivity::_bind_methods() {
	ClassDB::bind_method(
			D_METHOD("set_activity_async", "user", "connection_string", "join_restriction", "max_players", "current_players", "group_id", "allow_cross_platform_join"),
			&GDKMultiplayerActivity::set_activity_async,
			DEFVAL(String("followed")),
			DEFVAL(static_cast<int64_t>(0)),
			DEFVAL(static_cast<int64_t>(0)),
			DEFVAL(String()),
			DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_activities_async", "user", "xuids"), &GDKMultiplayerActivity::get_activities_async);
	ClassDB::bind_method(D_METHOD("get_cached_activity", "xuid"), &GDKMultiplayerActivity::get_cached_activity);
	ClassDB::bind_method(D_METHOD("delete_activity_async", "user"), &GDKMultiplayerActivity::delete_activity_async);
	ClassDB::bind_method(
			D_METHOD("send_invites_async", "user", "xuids", "allow_cross_platform_join", "connection_string"),
			&GDKMultiplayerActivity::send_invites_async,
			DEFVAL(true),
			DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("show_invite_ui_async", "user"), &GDKMultiplayerActivity::show_invite_ui_async);
	ClassDB::bind_method(
			D_METHOD("update_recent_players", "user", "xuids", "encounter_type"),
			&GDKMultiplayerActivity::update_recent_players,
			DEFVAL(String("default")));
	ClassDB::bind_method(D_METHOD("flush_recent_players_async", "user"), &GDKMultiplayerActivity::flush_recent_players_async);
	ClassDB::bind_method(D_METHOD("accept_pending_invite", "invite_uri"), &GDKMultiplayerActivity::accept_pending_invite);

	ADD_SIGNAL(MethodInfo("activities_updated", PropertyInfo(Variant::PACKED_STRING_ARRAY, "xuids")));
	ADD_SIGNAL(MethodInfo("pending_invite_received", PropertyInfo(Variant::DICTIONARY, "invite")));
	ADD_SIGNAL(MethodInfo("invite_accepted", PropertyInfo(Variant::DICTIONARY, "invite")));
}

#ifdef XBOX_MODULE_GDK_ENABLED
void GDKMultiplayerActivity::set_owner(GDK *p_owner) {
	m_owner = p_owner;
}

Ref<GDKResult> GDKMultiplayerActivity::on_runtime_initialized() {
	GDKRuntime *runtime = get_runtime_internal();
	if (runtime == nullptr || !runtime->is_initialized()) {
		return GDKResult::error_result(
				E_FAIL,
				"runtime_not_initialized",
				"Cannot initialize the multiplayer activity service before the GDK runtime.");
	}

	if (m_activation_listener_id == 0 && m_owner != nullptr) {
		Ref<GDKActivation> activation = m_owner->get_activation();
		if (activation.is_valid()) {
			m_activation_listener_id = activation->add_activation_listener([this](const Dictionary &p_info) {
				handle_activation_internal(p_info);
			});
		}
	}

	m_runtime_ready = true;
	return GDKResult::ok_result();
}

void GDKMultiplayerActivity::shutdown() {
	m_runtime_ready = false;

	if (m_activation_listener_id != 0 && m_owner != nullptr) {
		Ref<GDKActivation> activation = m_owner->get_activation();
		if (activation.is_valid()) {
			activation->remove_activation_listener(m_activation_listener_id);
		}
		m_activation_listener_id = 0;
	}

	m_cached_activities.clear();
}

void GDKMultiplayerActivity::on_user_removed(const Ref<GDKUser> &p_user) {
	if (!p_user.is_valid()) {
		return;
	}

	remove_cached_activity_internal(p_user->get_xuid());
}

Signal GDKMultiplayerActivity::set_activity_async(
		const Ref<GDKUser> &p_user,
		const String &p_connection_string,
		const String &p_join_restriction,
		int64_t p_max_players,
		int64_t p_current_players,
		const String &p_group_id,
		bool p_allow_cross_platform_join) {
	GDKRuntime *runtime = get_runtime_internal();

	if (p_connection_string.strip_edges().is_empty()) {
		return make_error_signal_internal(
				E_INVALIDARG,
				"invalid_connection_string",
				"A non-empty connection string is required to set the multiplayer activity.");
	}
	if (p_max_players < 0 || p_current_players < 0) {
		return make_error_signal_internal(
				E_INVALIDARG,
				"invalid_player_counts",
				"Player counts must be zero or greater.");
	}
	if (p_max_players > 0 && p_current_players > p_max_players) {
		return make_error_signal_internal(
				E_INVALIDARG,
				"invalid_player_counts",
				"current_players cannot exceed max_players.");
	}

	XblMultiplayerActivityJoinRestriction join_restriction = XblMultiplayerActivityJoinRestriction::Followed;
	if (!try_parse_join_restriction_internal(p_join_restriction, &join_restriction)) {
		return make_error_signal_internal(
				E_INVALIDARG,
				"invalid_join_restriction",
				"join_restriction must be 'public', 'invite_only', or 'followed'.");
	}

	XblContextHandle context = nullptr;
	uint64_t xbox_user_id = 0;
	Ref<GDKResult> context_result = duplicate_context_for_user_internal(p_user, &context, &xbox_user_id);
	if (!context_result->is_ok()) {
		return make_error_signal_internal(
				static_cast<HRESULT>(context_result->get_hresult()),
				context_result->get_code(),
				context_result->get_message(),
				context_result->get_data());
	}

	Ref<GDKPendingSignal> pending_signal = runtime != nullptr ? runtime->make_pending_signal() : Ref<GDKPendingSignal>();
	ERR_FAIL_COND_V(pending_signal.is_null(), Signal());

	auto *context_wrapper = new SetActivityAsyncContext(
			this,
			runtime,
			pending_signal,
			context,
			String::num_uint64(xbox_user_id),
			p_connection_string,
			join_restriction_to_string_internal(join_restriction),
			p_max_players,
			p_current_players,
			p_group_id);
	context_wrapper->bind_cancel_handler();

	CharString connection_string_utf8 = p_connection_string.utf8();
	CharString group_id_utf8 = p_group_id.utf8();

	XblMultiplayerActivityInfo activity_info = {};
	activity_info.connectionString = connection_string_utf8.get_data();
	activity_info.joinRestriction = join_restriction;
	activity_info.maxPlayers = static_cast<size_t>(p_max_players);
	activity_info.currentPlayers = static_cast<size_t>(p_current_players);
	activity_info.groupId = p_group_id.is_empty() ? nullptr : group_id_utf8.get_data();

	HRESULT hr = XblMultiplayerActivitySetActivityAsync(
			context_wrapper->get_context(),
			&activity_info,
			p_allow_cross_platform_join,
			context_wrapper->get_async_block());
	if (FAILED(hr)) {
		pending_signal->clear_cancel_handler();
		delete context_wrapper;

		Ref<GDKResult> result = GDKResult::hresult_error(
				hr,
				"Failed to start the multiplayer activity update.",
				"multiplayer_activity_set_start_failed");
		pending_signal->complete_deferred(result);
	}

	return pending_signal->get_completed_signal();
}

Signal GDKMultiplayerActivity::get_activities_async(const Ref<GDKUser> &p_user, const PackedStringArray &p_xuids) {
	GDKRuntime *runtime = get_runtime_internal();

	std::vector<uint64_t> query_xuids;
	Ref<GDKResult> parse_result = _parse_xuids(
			p_xuids,
			&query_xuids,
			nullptr,
			30,
			"missing_xuids",
			"invalid_xuids",
			"Each queried XUID must be a non-empty numeric string.");
	if (!parse_result->is_ok()) {
		return make_error_signal_internal(
				static_cast<HRESULT>(parse_result->get_hresult()),
				parse_result->get_code(),
				parse_result->get_message());
	}

	XblContextHandle context = nullptr;
	Ref<GDKResult> context_result = duplicate_context_for_user_internal(p_user, &context);
	if (!context_result->is_ok()) {
		return make_error_signal_internal(
				static_cast<HRESULT>(context_result->get_hresult()),
				context_result->get_code(),
				context_result->get_message(),
				context_result->get_data());
	}

	Ref<GDKPendingSignal> pending_signal = runtime != nullptr ? runtime->make_pending_signal() : Ref<GDKPendingSignal>();
	ERR_FAIL_COND_V(pending_signal.is_null(), Signal());

	auto *context_wrapper = new GetActivitiesAsyncContext(this, runtime, pending_signal, context);
	context_wrapper->bind_cancel_handler();

	HRESULT hr = XblMultiplayerActivityGetActivityAsync(
			context_wrapper->get_context(),
			query_xuids.data(),
			query_xuids.size(),
			context_wrapper->get_async_block());
	if (FAILED(hr)) {
		pending_signal->clear_cancel_handler();
		delete context_wrapper;

		Ref<GDKResult> result = GDKResult::hresult_error(
				hr,
				"Failed to start the multiplayer activity query.",
				"multiplayer_activity_get_start_failed");
		pending_signal->complete_deferred(result);
	}

	return pending_signal->get_completed_signal();
}

Ref<GDKMultiplayerActivityInfo> GDKMultiplayerActivity::get_cached_activity(const String &p_xuid) const {
	const String normalized_xuid = p_xuid.strip_edges();
	for (const CachedActivityState &state : m_cached_activities) {
		if (state.xuid == normalized_xuid) {
			return state.info;
		}
	}

	return Ref<GDKMultiplayerActivityInfo>();
}

Signal GDKMultiplayerActivity::delete_activity_async(const Ref<GDKUser> &p_user) {
	GDKRuntime *runtime = get_runtime_internal();

	XblContextHandle context = nullptr;
	uint64_t xbox_user_id = 0;
	Ref<GDKResult> context_result = duplicate_context_for_user_internal(p_user, &context, &xbox_user_id);
	if (!context_result->is_ok()) {
		return make_error_signal_internal(
				static_cast<HRESULT>(context_result->get_hresult()),
				context_result->get_code(),
				context_result->get_message(),
				context_result->get_data());
	}

	Ref<GDKPendingSignal> pending_signal = runtime != nullptr ? runtime->make_pending_signal() : Ref<GDKPendingSignal>();
	ERR_FAIL_COND_V(pending_signal.is_null(), Signal());

	auto *context_wrapper = new DeleteActivityAsyncContext(
			this,
			runtime,
			pending_signal,
			context,
			String::num_uint64(xbox_user_id));
	context_wrapper->bind_cancel_handler();

	HRESULT hr = XblMultiplayerActivityDeleteActivityAsync(
			context_wrapper->get_context(),
			context_wrapper->get_async_block());
	if (FAILED(hr)) {
		pending_signal->clear_cancel_handler();
		delete context_wrapper;

		Ref<GDKResult> result = GDKResult::hresult_error(
				hr,
				"Failed to start the multiplayer activity delete.",
				"multiplayer_activity_delete_start_failed");
		pending_signal->complete_deferred(result);
	}

	return pending_signal->get_completed_signal();
}

Signal GDKMultiplayerActivity::send_invites_async(
		const Ref<GDKUser> &p_user,
		const PackedStringArray &p_xuids,
		bool p_allow_cross_platform_join,
		const String &p_connection_string) {
	GDKRuntime *runtime = get_runtime_internal();

	std::vector<uint64_t> target_xuids;
	Ref<GDKResult> parse_result = _parse_xuids(
			p_xuids,
			&target_xuids,
			nullptr,
			0,
			"missing_xuids",
			"invalid_xuids",
			"Each invited XUID must be a non-empty numeric string.");
	if (!parse_result->is_ok()) {
		return make_error_signal_internal(
				static_cast<HRESULT>(parse_result->get_hresult()),
				parse_result->get_code(),
				parse_result->get_message());
	}

	XblContextHandle context = nullptr;
	uint64_t xbox_user_id = 0;
	Ref<GDKResult> context_result = duplicate_context_for_user_internal(p_user, &context, &xbox_user_id);
	if (!context_result->is_ok()) {
		return make_error_signal_internal(
				static_cast<HRESULT>(context_result->get_hresult()),
				context_result->get_code(),
				context_result->get_message(),
				context_result->get_data());
	}

	String resolved_connection_string = p_connection_string;
	if (resolved_connection_string.is_empty()) {
		Ref<GDKMultiplayerActivityInfo> cached_activity = get_cached_activity(String::num_uint64(xbox_user_id));
		if (cached_activity.is_valid()) {
			resolved_connection_string = cached_activity->get_connection_string();
		}
	}
	if (resolved_connection_string.strip_edges().is_empty()) {
		XblContextCloseHandle(context);
		return make_error_signal_internal(
				E_INVALIDARG,
				"missing_connection_string",
				"A connection string is required to send multiplayer invites. Set the local activity first or pass one explicitly.");
	}

	Ref<GDKPendingSignal> pending_signal = runtime != nullptr ? runtime->make_pending_signal() : Ref<GDKPendingSignal>();
	ERR_FAIL_COND_V(pending_signal.is_null(), Signal());

	auto *context_wrapper = new SendInvitesAsyncContext(
			this,
			runtime,
			pending_signal,
			context,
			target_xuids.size(),
			p_allow_cross_platform_join,
			resolved_connection_string);
	context_wrapper->bind_cancel_handler();

	CharString connection_string_utf8 = resolved_connection_string.utf8();
	HRESULT hr = XblMultiplayerActivitySendInvitesAsync(
			context_wrapper->get_context(),
			target_xuids.data(),
			target_xuids.size(),
			p_allow_cross_platform_join,
			connection_string_utf8.get_data(),
			context_wrapper->get_async_block());
	if (FAILED(hr)) {
		pending_signal->clear_cancel_handler();
		delete context_wrapper;

		Ref<GDKResult> result = GDKResult::hresult_error(
				hr,
				"Failed to start the multiplayer invite request.",
				"multiplayer_activity_send_invites_start_failed");
		pending_signal->complete_deferred(result);
	}

	return pending_signal->get_completed_signal();
}

Signal GDKMultiplayerActivity::show_invite_ui_async(const Ref<GDKUser> &p_user) {
	GDKRuntime *runtime = get_runtime_internal();
	if (runtime == nullptr || !runtime->is_initialized()) {
		return make_error_signal_internal(
				E_FAIL,
				"not_initialized",
				"GDK is not initialized. Call GDK.initialize() first.");
	}
	if (!p_user.is_valid() || p_user->get_handle() == nullptr) {
		return make_error_signal_internal(
				E_INVALIDARG,
				"invalid_user",
				"A signed-in GDKUser is required to show the invite UI.");
	}

	Ref<GDKPendingSignal> pending_signal = runtime->make_pending_signal();

	auto *context = new InviteUiAsyncContext(runtime, pending_signal);
	context->bind_cancel_handler();

	HRESULT hr = XGameUiShowMultiplayerActivityGameInviteAsync(
			context->get_async_block(),
			p_user->get_handle());
	if (FAILED(hr)) {
		pending_signal->clear_cancel_handler();
		delete context;

		Ref<GDKResult> result = GDKResult::hresult_error(
				hr,
				"Failed to start the multiplayer invite UI flow.",
				"multiplayer_activity_invite_ui_start_failed");
		pending_signal->complete_deferred(result);
	}

	return pending_signal->get_completed_signal();
}

Ref<GDKResult> GDKMultiplayerActivity::update_recent_players(
		const Ref<GDKUser> &p_user,
		const PackedStringArray &p_xuids,
		const String &p_encounter_type) {
	XblContextHandle context = nullptr;
	Ref<GDKResult> context_result = duplicate_context_for_user_internal(p_user, &context);
	if (!context_result->is_ok()) {
		return context_result;
	}

	std::vector<uint64_t> recent_player_xuids;
	Ref<GDKResult> parse_result = _parse_xuids(
			p_xuids,
			&recent_player_xuids,
			nullptr,
			0,
			"missing_xuids",
			"invalid_xuids",
			"Each recent-player XUID must be a non-empty numeric string.");
	if (!parse_result->is_ok()) {
		XblContextCloseHandle(context);
		return parse_result;
	}

	XblMultiplayerActivityEncounterType encounter_type = XblMultiplayerActivityEncounterType::Default;
	if (!try_parse_encounter_type_internal(p_encounter_type, &encounter_type)) {
		XblContextCloseHandle(context);
		return GDKResult::error_result(
				E_INVALIDARG,
				"invalid_encounter_type",
				"encounter_type must be 'default', 'teammate', or 'opponent'.");
	}

	std::vector<XblMultiplayerActivityRecentPlayerUpdate> updates;
	updates.reserve(recent_player_xuids.size());
	for (uint64_t xuid : recent_player_xuids) {
		XblMultiplayerActivityRecentPlayerUpdate update = {};
		update.xuid = xuid;
		update.encounterType = encounter_type;
		updates.push_back(update);
	}

	HRESULT hr = XblMultiplayerActivityUpdateRecentPlayers(
			context,
			updates.data(),
			updates.size());
	XblContextCloseHandle(context);
	if (FAILED(hr)) {
		return GDKResult::hresult_error(
				hr,
				"Failed to update the recent-player list.",
				"multiplayer_activity_update_recent_players_failed");
	}

	Dictionary data;
	data["player_count"] = static_cast<int64_t>(updates.size());
	data["encounter_type"] = encounter_type_to_string_internal(encounter_type);
	return GDKResult::ok_result(data);
}

Signal GDKMultiplayerActivity::flush_recent_players_async(const Ref<GDKUser> &p_user) {
	GDKRuntime *runtime = get_runtime_internal();

	XblContextHandle context = nullptr;
	Ref<GDKResult> context_result = duplicate_context_for_user_internal(p_user, &context);
	if (!context_result->is_ok()) {
		return make_error_signal_internal(
				static_cast<HRESULT>(context_result->get_hresult()),
				context_result->get_code(),
				context_result->get_message(),
				context_result->get_data());
	}

	Ref<GDKPendingSignal> pending_signal = runtime != nullptr ? runtime->make_pending_signal() : Ref<GDKPendingSignal>();
	ERR_FAIL_COND_V(pending_signal.is_null(), Signal());

	auto *context_wrapper = new FlushRecentPlayersAsyncContext(this, runtime, pending_signal, context);
	context_wrapper->bind_cancel_handler();

	HRESULT hr = XblMultiplayerActivityFlushRecentPlayersAsync(
			context_wrapper->get_context(),
			context_wrapper->get_async_block());
	if (FAILED(hr)) {
		pending_signal->clear_cancel_handler();
		delete context_wrapper;

		Ref<GDKResult> result = GDKResult::hresult_error(
				hr,
				"Failed to start the recent-player flush.",
				"multiplayer_activity_flush_recent_players_start_failed");
		pending_signal->complete_deferred(result);
	}

	return pending_signal->get_completed_signal();
}

Ref<GDKResult> GDKMultiplayerActivity::accept_pending_invite(const String &p_invite_uri) {
	if (!m_runtime_ready) {
		return GDKResult::error_result(
				E_FAIL,
				"not_initialized",
				"GDK is not initialized. Call GDK.initialize() first.");
	}

	const String invite_uri = p_invite_uri.strip_edges();
	if (invite_uri.is_empty()) {
		return GDKResult::error_result(
				E_INVALIDARG,
				"invalid_invite_uri",
				"A non-empty invite URI is required.");
	}

	const CharString invite_uri_utf8 = invite_uri.utf8();
	HRESULT hr = XGameActivationAcceptPendingInvite(invite_uri_utf8.get_data());
	if (FAILED(hr)) {
		return GDKResult::hresult_error(
				hr,
				"Failed to accept the pending multiplayer invite.",
				"multiplayer_activity_accept_pending_invite_failed");
	}

	return GDKResult::ok_result(parse_invite_uri_internal(invite_uri, "pending_game_invite"));
}

GDKRuntime *GDKMultiplayerActivity::get_runtime_internal() const {
	return m_owner != nullptr ? m_owner->get_runtime() : nullptr;
}

GDKXboxServices *GDKMultiplayerActivity::get_xbox_services_internal() const {
	return m_owner != nullptr ? m_owner->get_xbox_services() : nullptr;
}

Signal GDKMultiplayerActivity::make_completed_signal_internal(const Ref<GDKResult> &p_result) const {
	GDKRuntime *runtime = get_runtime_internal();
	Ref<GDKPendingSignal> pending_signal = runtime != nullptr ? runtime->make_pending_signal() : Ref<GDKPendingSignal>();
	if (pending_signal.is_null()) {
		pending_signal.instantiate();
	}
	pending_signal->complete_deferred(p_result);
	return pending_signal->get_completed_signal();
}

Signal GDKMultiplayerActivity::make_error_signal_internal(
		HRESULT p_hresult,
		const String &p_code,
		const String &p_message,
		const Variant &p_data) const {
	GDKRuntime *runtime = get_runtime_internal();
	if (runtime != nullptr) {
		return runtime->make_error_signal(p_hresult, p_code, p_message, p_data);
	}

	Ref<GDKPendingSignal> pending_signal;
	pending_signal.instantiate();
	pending_signal->complete_deferred(GDKResult::error_result(p_hresult, p_code, p_message, p_data));
	return pending_signal->get_completed_signal();
}

Ref<GDKResult> GDKMultiplayerActivity::duplicate_context_for_user_internal(
		const Ref<GDKUser> &p_user,
		XblContextHandle *r_context,
		uint64_t *r_xbox_user_id) const {
	ERR_FAIL_COND_V(r_context == nullptr, GDKResult::error_result(E_POINTER, "internal_error", "Missing multiplayer activity context output."));

	*r_context = nullptr;

	GDKRuntime *runtime = get_runtime_internal();
	if (runtime == nullptr || !runtime->is_initialized()) {
		return GDKResult::error_result(E_FAIL, "not_initialized", "GDK is not initialized. Call GDK.initialize() first.");
	}

	if (!p_user.is_valid() || p_user->get_handle() == nullptr) {
		return GDKResult::error_result(
				E_INVALIDARG,
				"invalid_user",
				"A signed-in GDKUser is required for multiplayer activity.");
	}

	GDKXboxServices *xbox_services = get_xbox_services_internal();
	if (xbox_services == nullptr || !xbox_services->is_initialized()) {
		return GDKResult::error_result(
				E_FAIL,
				"xbox_services_not_initialized",
				"Xbox services are unavailable. Ensure the title has a TitleId before using multiplayer activity.");
	}

	HRESULT hr = xbox_services->duplicate_context_for_user(p_user, r_context, r_xbox_user_id);
	if (FAILED(hr)) {
		return GDKResult::hresult_error(
				hr,
				"Failed to resolve the Xbox services context for multiplayer activity.",
				"multiplayer_activity_context_failed");
	}

	return GDKResult::ok_result();
}

Ref<GDKMultiplayerActivityInfo> GDKMultiplayerActivity::cache_activity_internal(const Ref<GDKMultiplayerActivityInfo> &p_info) {
	if (!p_info.is_valid()) {
		return Ref<GDKMultiplayerActivityInfo>();
	}

	const String xuid = p_info->get_xuid();
	for (CachedActivityState &state : m_cached_activities) {
		if (state.xuid == xuid) {
			state.info = p_info;
			return p_info;
		}
	}

	CachedActivityState new_state;
	new_state.xuid = xuid;
	new_state.info = p_info;
	m_cached_activities.push_back(new_state);
	return p_info;
}

void GDKMultiplayerActivity::remove_cached_activity_internal(const String &p_xuid) {
	const String normalized_xuid = p_xuid.strip_edges();
	m_cached_activities.erase(
			std::remove_if(
					m_cached_activities.begin(),
					m_cached_activities.end(),
					[&normalized_xuid](const CachedActivityState &state) {
						return state.xuid == normalized_xuid;
					}),
			m_cached_activities.end());
}

void GDKMultiplayerActivity::emit_activities_updated_internal(const std::vector<String> &p_xuids) {
	PackedStringArray updated_xuids;
	for (const String &xuid : p_xuids) {
		if (xuid.is_empty()) {
			continue;
		}

		bool already_present = false;
		for (int64_t i = 0; i < updated_xuids.size(); ++i) {
			if (updated_xuids[i] == xuid) {
				already_present = true;
				break;
			}
		}
		if (!already_present) {
			updated_xuids.push_back(xuid);
		}
	}

	if (!updated_xuids.is_empty()) {
		emit_signal("activities_updated", updated_xuids);
	}
}

void GDKMultiplayerActivity::handle_activation_internal(const Dictionary &p_activation_info) {
	if (!m_runtime_ready) {
		return;
	}

	const int64_t activation_type = static_cast<int64_t>(p_activation_info.get("type", static_cast<int64_t>(-1)));
	const String invite_uri = p_activation_info.get("invite_uri", String());
	Dictionary invite = p_activation_info.get("invite", Dictionary());

	if (invite.is_empty() && !invite_uri.is_empty()) {
		if (activation_type == static_cast<int64_t>(XGameActivationType::PendingGameInvite)) {
			invite = parse_invite_uri_internal(invite_uri, "pending_game_invite");
		} else if (activation_type == static_cast<int64_t>(XGameActivationType::AcceptedGameInvite)) {
			invite = parse_invite_uri_internal(invite_uri, "accepted_game_invite");
		}
	}

	switch (static_cast<XGameActivationType>(activation_type)) {
		case XGameActivationType::PendingGameInvite:
			emit_signal("pending_invite_received", invite);
			break;
		case XGameActivationType::AcceptedGameInvite:
			emit_signal("invite_accepted", invite);
			break;
		default:
			break;
	}
}

String GDKMultiplayerActivity::join_restriction_to_string_internal(XblMultiplayerActivityJoinRestriction p_join_restriction) {
	switch (p_join_restriction) {
		case XblMultiplayerActivityJoinRestriction::Public:
			return "public";
		case XblMultiplayerActivityJoinRestriction::InviteOnly:
			return "invite_only";
		case XblMultiplayerActivityJoinRestriction::Followed:
		default:
			return "followed";
	}
}

bool GDKMultiplayerActivity::try_parse_join_restriction_internal(
		const String &p_join_restriction,
		XblMultiplayerActivityJoinRestriction *r_join_restriction) {
	ERR_FAIL_COND_V(r_join_restriction == nullptr, false);

	const String normalized = p_join_restriction.strip_edges().to_lower();
	if (normalized == "public") {
		*r_join_restriction = XblMultiplayerActivityJoinRestriction::Public;
		return true;
	}
	if (normalized == "invite_only" || normalized == "invite-only" || normalized == "inviteonly") {
		*r_join_restriction = XblMultiplayerActivityJoinRestriction::InviteOnly;
		return true;
	}
	if (normalized == "followed") {
		*r_join_restriction = XblMultiplayerActivityJoinRestriction::Followed;
		return true;
	}

	return false;
}

String GDKMultiplayerActivity::platform_to_string_internal(XblMultiplayerActivityPlatform p_platform) {
	switch (p_platform) {
		case XblMultiplayerActivityPlatform::XboxOne:
			return "xbox_one";
		case XblMultiplayerActivityPlatform::WindowsOneCore:
			return "windows_one_core";
		case XblMultiplayerActivityPlatform::Win32:
			return "win32";
		case XblMultiplayerActivityPlatform::Scarlett:
			return "scarlett";
		case XblMultiplayerActivityPlatform::iOS:
			return "ios";
		case XblMultiplayerActivityPlatform::Android:
			return "android";
		case XblMultiplayerActivityPlatform::Nintendo:
			return "nintendo";
		case XblMultiplayerActivityPlatform::PlayStation:
			return "playstation";
		case XblMultiplayerActivityPlatform::All:
			return "all";
		case XblMultiplayerActivityPlatform::Unknown:
		default:
			return "unknown";
	}
}

String GDKMultiplayerActivity::encounter_type_to_string_internal(XblMultiplayerActivityEncounterType p_encounter_type) {
	switch (p_encounter_type) {
		case XblMultiplayerActivityEncounterType::Teammate:
			return "teammate";
		case XblMultiplayerActivityEncounterType::Opponent:
			return "opponent";
		case XblMultiplayerActivityEncounterType::Default:
		default:
			return "default";
	}
}

bool GDKMultiplayerActivity::try_parse_encounter_type_internal(
		const String &p_encounter_type,
		XblMultiplayerActivityEncounterType *r_encounter_type) {
	ERR_FAIL_COND_V(r_encounter_type == nullptr, false);

	const String normalized = p_encounter_type.strip_edges().to_lower();
	if (normalized == "default") {
		*r_encounter_type = XblMultiplayerActivityEncounterType::Default;
		return true;
	}
	if (normalized == "teammate") {
		*r_encounter_type = XblMultiplayerActivityEncounterType::Teammate;
		return true;
	}
	if (normalized == "opponent") {
		*r_encounter_type = XblMultiplayerActivityEncounterType::Opponent;
		return true;
	}

	return false;
}

Dictionary GDKMultiplayerActivity::parse_invite_uri_internal(const String &p_uri, const String &p_activation_type) {
	return GDKActivation::make_invite_dictionary_internal(p_uri, p_activation_type);
}

bool GDKMultiplayerActivity::try_parse_xuid_internal(const String &p_xuid, uint64_t *r_xuid) {
	ERR_FAIL_COND_V(r_xuid == nullptr, false);

	const String normalized = p_xuid.strip_edges();
	if (normalized.is_empty()) {
		return false;
	}

	const CharString utf8 = normalized.utf8();
	const char *value = utf8.get_data();
	if (value == nullptr || *value == '\0') {
		return false;
	}

	char *end = nullptr;
	unsigned long long parsed = std::strtoull(value, &end, 10);
	if (end == value || (end != nullptr && *end != '\0')) {
		return false;
	}

	*r_xuid = static_cast<uint64_t>(parsed);
	return true;
}

#else
void GDKMultiplayerActivity::set_owner(GDK *p_owner) {}

Ref<GDKResult> GDKMultiplayerActivity::on_runtime_initialized() {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

void GDKMultiplayerActivity::shutdown() {}

void GDKMultiplayerActivity::on_user_removed(const Ref<GDKUser> &p_user) {}

Signal GDKMultiplayerActivity::set_activity_async(
		const Ref<GDKUser> &p_user,
		const String &p_connection_string,
		const String &p_join_restriction,
		int64_t p_max_players,
		int64_t p_current_players,
		const String &p_group_id,
		bool p_allow_cross_platform_join) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKMultiplayerActivity::get_activities_async(const Ref<GDKUser> &p_user, const PackedStringArray &p_xuids) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Ref<GDKMultiplayerActivityInfo> GDKMultiplayerActivity::get_cached_activity(const String &p_xuid) const {
	return Ref<GDKMultiplayerActivityInfo>();
}

Signal GDKMultiplayerActivity::delete_activity_async(const Ref<GDKUser> &p_user) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKMultiplayerActivity::send_invites_async(
		const Ref<GDKUser> &p_user,
		const PackedStringArray &p_xuids,
		bool p_allow_cross_platform_join,
		const String &p_connection_string) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKMultiplayerActivity::show_invite_ui_async(const Ref<GDKUser> &p_user) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Ref<GDKResult> GDKMultiplayerActivity::update_recent_players(
		const Ref<GDKUser> &p_user,
		const PackedStringArray &p_xuids,
		const String &p_encounter_type) {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Signal GDKMultiplayerActivity::flush_recent_players_async(const Ref<GDKUser> &p_user) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Ref<GDKResult> GDKMultiplayerActivity::accept_pending_invite(const String &p_invite_uri) {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Signal GDKMultiplayerActivity::make_completed_signal_internal(const Ref<GDKResult> &p_result) const {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKMultiplayerActivity::make_error_signal_internal(
		HRESULT p_hresult,
		const String &p_code,
		const String &p_message,
		const Variant &p_data) const {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Ref<GDKResult> GDKMultiplayerActivity::duplicate_context_for_user_internal(
		const Ref<GDKUser> &p_user,
		XblContextHandle *r_context,
		uint64_t *r_xbox_user_id) const {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Ref<GDKMultiplayerActivityInfo> GDKMultiplayerActivity::cache_activity_internal(const Ref<GDKMultiplayerActivityInfo> &p_info) {
	return Ref<GDKMultiplayerActivityInfo>();
}

void GDKMultiplayerActivity::remove_cached_activity_internal(const String &p_xuid) {}

void GDKMultiplayerActivity::emit_activities_updated_internal(const std::vector<String> &p_xuids) {}

void GDKMultiplayerActivity::handle_activation_internal(const Dictionary &p_activation_info) {}

String GDKMultiplayerActivity::join_restriction_to_string_internal(XblMultiplayerActivityJoinRestriction p_join_restriction) {
	return String();
}

bool GDKMultiplayerActivity::try_parse_join_restriction_internal(
		const String &p_join_restriction,
		XblMultiplayerActivityJoinRestriction *r_join_restriction) {
	return false;
}

String GDKMultiplayerActivity::platform_to_string_internal(XblMultiplayerActivityPlatform p_platform) {
	return String();
}

String GDKMultiplayerActivity::encounter_type_to_string_internal(XblMultiplayerActivityEncounterType p_encounter_type) {
	return String();
}

bool GDKMultiplayerActivity::try_parse_encounter_type_internal(
		const String &p_encounter_type,
		XblMultiplayerActivityEncounterType *r_encounter_type) {
	return false;
}

Dictionary GDKMultiplayerActivity::parse_invite_uri_internal(const String &p_uri, const String &p_activation_type) {
	return Dictionary();
}

bool GDKMultiplayerActivity::try_parse_xuid_internal(const String &p_xuid, uint64_t *r_xuid) {
	return false;
}
#endif
