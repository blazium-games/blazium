/**************************************************************************/
/*  gdk_privacy.cpp                                                       */
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

#include "gdk_privacy.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "gdk_windows.h"

#include <cerrno>
#include <cstdlib>
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

bool _try_parse_permission(const String &p_permission, XblPermission *r_permission) {
	if (r_permission == nullptr) {
		return false;
	}

	const String token = _normalize_token(p_permission);
	if (token == "communicate_using_text") {
		*r_permission = XblPermission::CommunicateUsingText;
	} else if (token == "communicate_using_video") {
		*r_permission = XblPermission::CommunicateUsingVideo;
	} else if (token == "communicate_using_voice") {
		*r_permission = XblPermission::CommunicateUsingVoice;
	} else if (token == "view_target_profile") {
		*r_permission = XblPermission::ViewTargetProfile;
	} else if (token == "view_target_game_history") {
		*r_permission = XblPermission::ViewTargetGameHistory;
	} else if (token == "view_target_video_history") {
		*r_permission = XblPermission::ViewTargetVideoHistory;
	} else if (token == "view_target_music_history") {
		*r_permission = XblPermission::ViewTargetMusicHistory;
	} else if (token == "view_target_exercise_info") {
		*r_permission = XblPermission::ViewTargetExerciseInfo;
	} else if (token == "view_target_presence") {
		*r_permission = XblPermission::ViewTargetPresence;
	} else if (token == "view_target_video_status") {
		*r_permission = XblPermission::ViewTargetVideoStatus;
	} else if (token == "view_target_music_status") {
		*r_permission = XblPermission::ViewTargetMusicStatus;
	} else if (token == "play_multiplayer") {
		*r_permission = XblPermission::PlayMultiplayer;
	} else if (token == "view_target_user_created_content") {
		*r_permission = XblPermission::ViewTargetUserCreatedContent;
	} else if (token == "broadcast_with_twitch") {
		*r_permission = XblPermission::BroadcastWithTwitch;
	} else if (token == "write_comment") {
		*r_permission = XblPermission::WriteComment;
	} else if (token == "share_item") {
		*r_permission = XblPermission::ShareItem;
	} else if (token == "share_target_content_to_external_networks") {
		*r_permission = XblPermission::ShareTargetContentToExternalNetworks;
	} else {
		return false;
	}

	return true;
}

bool _try_parse_anonymous_user_type(const String &p_user_type, XblAnonymousUserType *r_user_type) {
	if (r_user_type == nullptr) {
		return false;
	}

	const String token = _normalize_token(p_user_type);
	if (token == "cross_network_user") {
		*r_user_type = XblAnonymousUserType::CrossNetworkUser;
	} else if (token == "cross_network_friend") {
		*r_user_type = XblAnonymousUserType::CrossNetworkFriend;
	} else {
		return false;
	}

	return true;
}

String _permission_to_string(XblPermission p_permission) {
	switch (p_permission) {
		case XblPermission::CommunicateUsingText:
			return "communicate_using_text";
		case XblPermission::CommunicateUsingVideo:
			return "communicate_using_video";
		case XblPermission::CommunicateUsingVoice:
			return "communicate_using_voice";
		case XblPermission::ViewTargetProfile:
			return "view_target_profile";
		case XblPermission::ViewTargetGameHistory:
			return "view_target_game_history";
		case XblPermission::ViewTargetVideoHistory:
			return "view_target_video_history";
		case XblPermission::ViewTargetMusicHistory:
			return "view_target_music_history";
		case XblPermission::ViewTargetExerciseInfo:
			return "view_target_exercise_info";
		case XblPermission::ViewTargetPresence:
			return "view_target_presence";
		case XblPermission::ViewTargetVideoStatus:
			return "view_target_video_status";
		case XblPermission::ViewTargetMusicStatus:
			return "view_target_music_status";
		case XblPermission::PlayMultiplayer:
			return "play_multiplayer";
		case XblPermission::ViewTargetUserCreatedContent:
			return "view_target_user_created_content";
		case XblPermission::BroadcastWithTwitch:
			return "broadcast_with_twitch";
		case XblPermission::WriteComment:
			return "write_comment";
		case XblPermission::ShareItem:
			return "share_item";
		case XblPermission::ShareTargetContentToExternalNetworks:
			return "share_target_content_to_external_networks";
		case XblPermission::Unknown:
		default:
			return "unknown";
	}
}

String _anonymous_user_type_to_string(XblAnonymousUserType p_user_type) {
	switch (p_user_type) {
		case XblAnonymousUserType::CrossNetworkUser:
			return "cross_network_user";
		case XblAnonymousUserType::CrossNetworkFriend:
			return "cross_network_friend";
		case XblAnonymousUserType::Unknown:
		default:
			return "unknown";
	}
}

String _deny_reason_to_string(XblPermissionDenyReason p_reason) {
	switch (p_reason) {
		case XblPermissionDenyReason::NotAllowed:
			return "not_allowed";
		case XblPermissionDenyReason::MissingPrivilege:
			return "missing_privilege";
		case XblPermissionDenyReason::PrivilegeRestrictsTarget:
			return "privilege_restricts_target";
		case XblPermissionDenyReason::BlockListRestrictsTarget:
			return "block_list_restricts_target";
		case XblPermissionDenyReason::MuteListRestrictsTarget:
			return "mute_list_restricts_target";
		case XblPermissionDenyReason::PrivacySettingRestrictsTarget:
			return "privacy_setting_restricts_target";
		case XblPermissionDenyReason::CrossNetworkUserMustBeFriend:
			return "cross_network_user_must_be_friend";
		case XblPermissionDenyReason::Unknown:
		default:
			return "unknown";
	}
}

String _privilege_to_string(XblPrivilege p_privilege) {
	switch (p_privilege) {
		case XblPrivilege::AllowIngameVoiceCommunications:
			return "allow_ingame_voice_communications";
		case XblPrivilege::AllowVideoCommunications:
			return "allow_video_communications";
		case XblPrivilege::AllowProfileViewing:
			return "allow_profile_viewing";
		case XblPrivilege::AllowCommunications:
			return "allow_communications";
		case XblPrivilege::AllowMultiplayer:
			return "allow_multiplayer";
		case XblPrivilege::AllowAddFriend:
			return "allow_add_friend";
		case XblPrivilege::Unknown:
		default:
			return "unknown";
	}
}

String _privacy_setting_to_string(XblPrivacySetting p_setting) {
	switch (p_setting) {
		case XblPrivacySetting::ShareFriendList:
			return "share_friend_list";
		case XblPrivacySetting::ShareGameHistory:
			return "share_game_history";
		case XblPrivacySetting::CommunicateUsingTextAndVoice:
			return "communicate_using_text_and_voice";
		case XblPrivacySetting::SharePresence:
			return "share_presence";
		case XblPrivacySetting::ShareProfile:
			return "share_profile";
		case XblPrivacySetting::CommunicateDuringCrossNetworkPlay:
			return "communicate_during_cross_network_play";
		case XblPrivacySetting::Unknown:
		default:
			return "unknown";
	}
}

Dictionary _make_permission_result_dictionary(const XblPermissionCheckResult &p_result) {
	Dictionary result;
	result["allowed"] = p_result.isAllowed;
	result["target_xuid"] = p_result.targetXuid == 0 ? String() : String::num_uint64(p_result.targetXuid);
	result["target_user_type"] = _anonymous_user_type_to_string(p_result.targetUserType);
	result["permission"] = _permission_to_string(p_result.permissionRequested);

	Array reasons;
	if (p_result.reasons != nullptr) {
		for (size_t i = 0; i < p_result.reasonsCount; ++i) {
			const XblPermissionDenyReasonDetails &native_reason = p_result.reasons[i];
			Dictionary reason;
			reason["reason"] = _deny_reason_to_string(native_reason.reason);
			reason["restricted_privilege"] = _privilege_to_string(native_reason.restrictedPrivilege);
			reason["restricted_privacy_setting"] = _privacy_setting_to_string(native_reason.restrictedPrivacySetting);
			reasons.push_back(reason);
		}
	}
	result["reasons"] = reasons;
	return result;
}

PackedStringArray _make_xuid_array(const std::vector<uint64_t> &p_xuids) {
	PackedStringArray result;
	for (uint64_t xuid : p_xuids) {
		result.push_back(String::num_uint64(xuid));
	}
	return result;
}

class PrivacyPermissionAsyncContext final : public GDKSignalXAsyncContext {
public:
	enum Mode {
		MODE_SINGLE,
		MODE_ANONYMOUS,
		MODE_BATCH,
	};

private:
	XblContextHandle m_context = nullptr;
	Mode m_mode = MODE_SINGLE;

protected:
	void finalize(XAsyncBlock *p_async_block) override {
		Ref<GDKResult> result;

		if (get_runtime()->is_shutting_down() || get_pending_signal()->was_cancel_requested()) {
			result = GDKResult::cancelled("Privacy permission check cancelled.");
			get_pending_signal()->complete(result);
			return;
		}

		size_t result_size = 0;
		HRESULT result_hr = m_mode == MODE_BATCH ? XblPrivacyBatchCheckPermissionResultSize(p_async_block, &result_size) : (m_mode == MODE_ANONYMOUS ? XblPrivacyCheckPermissionForAnonymousUserResultSize(p_async_block, &result_size) : XblPrivacyCheckPermissionResultSize(p_async_block, &result_size));
		if (result_hr == E_ABORT) {
			result = GDKResult::cancelled("Privacy permission check cancelled.");
			get_pending_signal()->complete(result);
			return;
		}
		if (FAILED(result_hr)) {
			result = GDKResult::hresult_error(result_hr, "Failed to retrieve privacy permission result size.", "privacy_result_size_failed");
			get_pending_signal()->complete(result);
			return;
		}

		std::vector<uint8_t> buffer(result_size);
		size_t buffer_used = 0;
		if (m_mode == MODE_BATCH) {
			XblPermissionCheckResult *results = nullptr;
			size_t result_count = 0;
			result_hr = XblPrivacyBatchCheckPermissionResult(
					p_async_block,
					buffer.size(),
					buffer.empty() ? nullptr : buffer.data(),
					&results,
					&result_count,
					&buffer_used);
			if (FAILED(result_hr)) {
				result = result_hr == E_ABORT ? GDKResult::cancelled("Privacy permission check cancelled.") : GDKResult::hresult_error(result_hr, "Failed to retrieve privacy permission results.", "privacy_results_failed");
				get_pending_signal()->complete(result);
				return;
			}

			Array payload;
			for (size_t i = 0; i < result_count; ++i) {
				payload.push_back(_make_permission_result_dictionary(results[i]));
			}
			get_pending_signal()->complete(GDKResult::ok_result(payload));
			return;
		}

		XblPermissionCheckResult *permission_result = nullptr;
		result_hr = m_mode == MODE_ANONYMOUS ? XblPrivacyCheckPermissionForAnonymousUserResult(
													   p_async_block,
													   buffer.size(),
													   buffer.empty() ? nullptr : buffer.data(),
													   &permission_result,
													   &buffer_used)
											 : XblPrivacyCheckPermissionResult(
													   p_async_block,
													   buffer.size(),
													   buffer.empty() ? nullptr : buffer.data(),
													   &permission_result,
													   &buffer_used);
		if (FAILED(result_hr)) {
			result = result_hr == E_ABORT ? GDKResult::cancelled("Privacy permission check cancelled.") : GDKResult::hresult_error(result_hr, "Failed to retrieve privacy permission result.", "privacy_result_failed");
			get_pending_signal()->complete(result);
			return;
		}

		Dictionary payload = permission_result == nullptr ? Dictionary() : _make_permission_result_dictionary(*permission_result);
		get_pending_signal()->complete(GDKResult::ok_result(payload));
	}

public:
	PrivacyPermissionAsyncContext(GDKRuntime *p_runtime, const Ref<GDKPendingSignal> &p_pending_signal, XblContextHandle p_context, Mode p_mode) :
			GDKSignalXAsyncContext(p_runtime, p_pending_signal),
			m_context(p_context),
			m_mode(p_mode) {}

	~PrivacyPermissionAsyncContext() override {
		if (m_context != nullptr) {
			XblContextCloseHandle(m_context);
			m_context = nullptr;
		}
	}

	XblContextHandle get_context() const {
		return m_context;
	}
};

class PrivacyListAsyncContext final : public GDKSignalXAsyncContext {
	XblContextHandle m_context = nullptr;
	bool m_mute_list = false;

protected:
	void finalize(XAsyncBlock *p_async_block) override {
		Ref<GDKResult> result;

		if (get_runtime()->is_shutting_down() || get_pending_signal()->was_cancel_requested()) {
			result = GDKResult::cancelled("Privacy list query cancelled.");
			get_pending_signal()->complete(result);
			return;
		}

		size_t xuid_count = 0;
		HRESULT result_hr = m_mute_list ? XblPrivacyGetMuteListResultCount(p_async_block, &xuid_count) : XblPrivacyGetAvoidListResultCount(p_async_block, &xuid_count);
		if (result_hr == E_ABORT) {
			result = GDKResult::cancelled("Privacy list query cancelled.");
			get_pending_signal()->complete(result);
			return;
		}
		if (FAILED(result_hr)) {
			result = GDKResult::hresult_error(result_hr, "Failed to retrieve privacy list result count.", "privacy_list_count_failed");
			get_pending_signal()->complete(result);
			return;
		}

		std::vector<uint64_t> xuids(xuid_count);
		result_hr = m_mute_list ? XblPrivacyGetMuteListResult(p_async_block, xuids.size(), xuids.empty() ? nullptr : xuids.data()) : XblPrivacyGetAvoidListResult(p_async_block, xuids.size(), xuids.empty() ? nullptr : xuids.data());
		if (FAILED(result_hr)) {
			result = result_hr == E_ABORT ? GDKResult::cancelled("Privacy list query cancelled.") : GDKResult::hresult_error(result_hr, "Failed to retrieve privacy list results.", "privacy_list_result_failed");
			get_pending_signal()->complete(result);
			return;
		}

		get_pending_signal()->complete(GDKResult::ok_result(_make_xuid_array(xuids)));
	}

public:
	PrivacyListAsyncContext(GDKRuntime *p_runtime, const Ref<GDKPendingSignal> &p_pending_signal, XblContextHandle p_context, bool p_mute_list) :
			GDKSignalXAsyncContext(p_runtime, p_pending_signal),
			m_context(p_context),
			m_mute_list(p_mute_list) {}

	~PrivacyListAsyncContext() override {
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

void GDKPrivacy::_bind_methods() {
	ClassDB::bind_method(D_METHOD("check_permission_async", "user", "permission", "target_xuid"), &GDKPrivacy::check_permission_async);
	ClassDB::bind_method(D_METHOD("check_permission_for_anonymous_user_async", "user", "permission", "anonymous_user_type"), &GDKPrivacy::check_permission_for_anonymous_user_async);
	ClassDB::bind_method(D_METHOD("batch_check_permission_async", "user", "permission", "target_xuids"), &GDKPrivacy::batch_check_permission_async);
	ClassDB::bind_method(D_METHOD("get_avoid_list_async", "user"), &GDKPrivacy::get_avoid_list_async);
	ClassDB::bind_method(D_METHOD("get_mute_list_async", "user"), &GDKPrivacy::get_mute_list_async);
}

#ifdef XBOX_MODULE_GDK_ENABLED
void GDKPrivacy::set_owner(GDK *p_owner) {
	m_owner = p_owner;
}

GDKRuntime *GDKPrivacy::_get_runtime() const {
	return m_owner == nullptr ? nullptr : m_owner->get_runtime();
}

GDKXboxServices *GDKPrivacy::_get_xbox_services() const {
	return m_owner == nullptr ? nullptr : m_owner->get_xbox_services();
}

Signal GDKPrivacy::_make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data) const {
	GDKRuntime *runtime = _get_runtime();
	ERR_FAIL_NULL_V(runtime, Signal());
	return runtime->make_error_signal(p_hresult, p_code, p_message, p_data);
}

Ref<GDKResult> GDKPrivacy::_ensure_ready_user(const Ref<GDKUser> &p_user) const {
	if (!m_runtime_ready) {
		return GDKResult::error_result(E_FAIL, "runtime_unavailable", "GDK runtime is not initialized.");
	}
	if (!p_user.is_valid() || p_user->get_handle() == nullptr || !p_user->is_signed_in()) {
		return GDKResult::error_result(E_INVALIDARG, "invalid_user", "A signed-in GDKUser is required.");
	}
	return GDKResult::ok_result();
}

Ref<GDKResult> GDKPrivacy::on_runtime_initialized() {
	m_runtime_ready = true;
	return GDKResult::ok_result();
}

void GDKPrivacy::shutdown() {
	m_runtime_ready = false;
}

int GDKPrivacy::dispatch() {
	return 0;
}

Signal GDKPrivacy::check_permission_async(const Ref<GDKUser> &p_user, const String &p_permission, const String &p_target_xuid) {
	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr) {
		return Signal();
	}

	Ref<GDKResult> validation = _ensure_ready_user(p_user);
	if (!validation->is_ok()) {
		return _make_error_signal(static_cast<HRESULT>(validation->get_hresult()), validation->get_code(), validation->get_message());
	}

	XblPermission permission = XblPermission::Unknown;
	if (!_try_parse_permission(p_permission, &permission)) {
		return _make_error_signal(E_INVALIDARG, "invalid_permission", "Unknown privacy permission.");
	}

	uint64_t target_xuid = 0;
	if (!_try_parse_xuid(p_target_xuid, &target_xuid)) {
		return _make_error_signal(E_INVALIDARG, "invalid_xuid", "target_xuid must be a non-empty decimal XUID string.");
	}

	GDKXboxServices *xbox_services = _get_xbox_services();
	if (xbox_services == nullptr || !xbox_services->is_initialized()) {
		return _make_error_signal(E_FAIL, "xbox_services_not_initialized", "Xbox services are unavailable. Ensure the title has a TitleId before using privacy.");
	}

	XblContextHandle context = nullptr;
	HRESULT hr = xbox_services->duplicate_context_for_user(p_user, &context);
	if (FAILED(hr)) {
		return _make_error_signal(hr, "xbox_context_unavailable", "Failed to create an Xbox services context for the user.");
	}

	Ref<GDKPendingSignal> pending_signal = runtime->make_pending_signal();
	PrivacyPermissionAsyncContext *async_context = new PrivacyPermissionAsyncContext(runtime, pending_signal, context, PrivacyPermissionAsyncContext::MODE_SINGLE);
	async_context->bind_cancel_handler();
	hr = XblPrivacyCheckPermissionAsync(async_context->get_context(), permission, target_xuid, async_context->get_async_block());
	if (FAILED(hr)) {
		async_context->clear_cancel_handler();
		delete async_context;
		return _make_error_signal(hr, "privacy_check_start_failed", "Failed to start privacy permission check.");
	}
	return pending_signal->get_completed_signal();
}

Signal GDKPrivacy::check_permission_for_anonymous_user_async(const Ref<GDKUser> &p_user, const String &p_permission, const String &p_anonymous_user_type) {
	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr) {
		return Signal();
	}

	Ref<GDKResult> validation = _ensure_ready_user(p_user);
	if (!validation->is_ok()) {
		return _make_error_signal(static_cast<HRESULT>(validation->get_hresult()), validation->get_code(), validation->get_message());
	}

	XblPermission permission = XblPermission::Unknown;
	if (!_try_parse_permission(p_permission, &permission)) {
		return _make_error_signal(E_INVALIDARG, "invalid_permission", "Unknown privacy permission.");
	}

	XblAnonymousUserType user_type = XblAnonymousUserType::Unknown;
	if (!_try_parse_anonymous_user_type(p_anonymous_user_type, &user_type)) {
		return _make_error_signal(E_INVALIDARG, "invalid_anonymous_user_type", "Unknown anonymous user type.");
	}

	GDKXboxServices *xbox_services = _get_xbox_services();
	if (xbox_services == nullptr || !xbox_services->is_initialized()) {
		return _make_error_signal(E_FAIL, "xbox_services_not_initialized", "Xbox services are unavailable. Ensure the title has a TitleId before using privacy.");
	}

	XblContextHandle context = nullptr;
	HRESULT hr = xbox_services->duplicate_context_for_user(p_user, &context);
	if (FAILED(hr)) {
		return _make_error_signal(hr, "xbox_context_unavailable", "Failed to create an Xbox services context for the user.");
	}

	Ref<GDKPendingSignal> pending_signal = runtime->make_pending_signal();
	PrivacyPermissionAsyncContext *async_context = new PrivacyPermissionAsyncContext(runtime, pending_signal, context, PrivacyPermissionAsyncContext::MODE_ANONYMOUS);
	async_context->bind_cancel_handler();
	hr = XblPrivacyCheckPermissionForAnonymousUserAsync(async_context->get_context(), permission, user_type, async_context->get_async_block());
	if (FAILED(hr)) {
		async_context->clear_cancel_handler();
		delete async_context;
		return _make_error_signal(hr, "privacy_check_start_failed", "Failed to start anonymous privacy permission check.");
	}
	return pending_signal->get_completed_signal();
}

Signal GDKPrivacy::batch_check_permission_async(const Ref<GDKUser> &p_user, const String &p_permission, const PackedStringArray &p_target_xuids) {
	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr) {
		return Signal();
	}

	Ref<GDKResult> validation = _ensure_ready_user(p_user);
	if (!validation->is_ok()) {
		return _make_error_signal(static_cast<HRESULT>(validation->get_hresult()), validation->get_code(), validation->get_message());
	}

	XblPermission permission = XblPermission::Unknown;
	if (!_try_parse_permission(p_permission, &permission)) {
		return _make_error_signal(E_INVALIDARG, "invalid_permission", "Unknown privacy permission.");
	}
	if (p_target_xuids.is_empty()) {
		return _make_error_signal(E_INVALIDARG, "invalid_xuids", "At least one target XUID is required.");
	}

	std::vector<uint64_t> xuids;
	xuids.reserve(static_cast<size_t>(p_target_xuids.size()));
	for (int64_t i = 0; i < p_target_xuids.size(); ++i) {
		uint64_t xuid = 0;
		if (!_try_parse_xuid(p_target_xuids[i], &xuid)) {
			return _make_error_signal(E_INVALIDARG, "invalid_xuid", "target_xuids must contain decimal XUID strings.");
		}
		xuids.push_back(xuid);
	}

	GDKXboxServices *xbox_services = _get_xbox_services();
	if (xbox_services == nullptr || !xbox_services->is_initialized()) {
		return _make_error_signal(E_FAIL, "xbox_services_not_initialized", "Xbox services are unavailable. Ensure the title has a TitleId before using privacy.");
	}

	XblContextHandle context = nullptr;
	HRESULT hr = xbox_services->duplicate_context_for_user(p_user, &context);
	if (FAILED(hr)) {
		return _make_error_signal(hr, "xbox_context_unavailable", "Failed to create an Xbox services context for the user.");
	}

	Ref<GDKPendingSignal> pending_signal = runtime->make_pending_signal();
	PrivacyPermissionAsyncContext *async_context = new PrivacyPermissionAsyncContext(runtime, pending_signal, context, PrivacyPermissionAsyncContext::MODE_BATCH);
	async_context->bind_cancel_handler();
	hr = XblPrivacyBatchCheckPermissionAsync(async_context->get_context(), &permission, 1, xuids.data(), xuids.size(), nullptr, 0, async_context->get_async_block());
	if (FAILED(hr)) {
		async_context->clear_cancel_handler();
		delete async_context;
		return _make_error_signal(hr, "privacy_batch_check_start_failed", "Failed to start batch privacy permission check.");
	}
	return pending_signal->get_completed_signal();
}

Signal GDKPrivacy::get_avoid_list_async(const Ref<GDKUser> &p_user) {
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
		return _make_error_signal(E_FAIL, "xbox_services_not_initialized", "Xbox services are unavailable. Ensure the title has a TitleId before using privacy.");
	}

	XblContextHandle context = nullptr;
	HRESULT hr = xbox_services->duplicate_context_for_user(p_user, &context);
	if (FAILED(hr)) {
		return _make_error_signal(hr, "xbox_context_unavailable", "Failed to create an Xbox services context for the user.");
	}

	Ref<GDKPendingSignal> pending_signal = runtime->make_pending_signal();
	PrivacyListAsyncContext *async_context = new PrivacyListAsyncContext(runtime, pending_signal, context, false);
	async_context->bind_cancel_handler();
	hr = XblPrivacyGetAvoidListAsync(async_context->get_context(), async_context->get_async_block());
	if (FAILED(hr)) {
		async_context->clear_cancel_handler();
		delete async_context;
		return _make_error_signal(hr, "privacy_avoid_list_start_failed", "Failed to start privacy avoid-list query.");
	}
	return pending_signal->get_completed_signal();
}

Signal GDKPrivacy::get_mute_list_async(const Ref<GDKUser> &p_user) {
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
		return _make_error_signal(E_FAIL, "xbox_services_not_initialized", "Xbox services are unavailable. Ensure the title has a TitleId before using privacy.");
	}

	XblContextHandle context = nullptr;
	HRESULT hr = xbox_services->duplicate_context_for_user(p_user, &context);
	if (FAILED(hr)) {
		return _make_error_signal(hr, "xbox_context_unavailable", "Failed to create an Xbox services context for the user.");
	}

	Ref<GDKPendingSignal> pending_signal = runtime->make_pending_signal();
	PrivacyListAsyncContext *async_context = new PrivacyListAsyncContext(runtime, pending_signal, context, true);
	async_context->bind_cancel_handler();
	hr = XblPrivacyGetMuteListAsync(async_context->get_context(), async_context->get_async_block());
	if (FAILED(hr)) {
		async_context->clear_cancel_handler();
		delete async_context;
		return _make_error_signal(hr, "privacy_mute_list_start_failed", "Failed to start privacy mute-list query.");
	}
	return pending_signal->get_completed_signal();
}

void GDKPrivacy::on_user_removed(const Ref<GDKUser> &p_user) {
	(void)p_user;
}

#else
void GDKPrivacy::set_owner(GDK *p_owner) {}

Signal GDKPrivacy::_make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data) const {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Ref<GDKResult> GDKPrivacy::_ensure_ready_user(const Ref<GDKUser> &p_user) const {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Ref<GDKResult> GDKPrivacy::on_runtime_initialized() {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

void GDKPrivacy::shutdown() {}

int GDKPrivacy::dispatch() {
	return 0;
}

Signal GDKPrivacy::check_permission_async(const Ref<GDKUser> &p_user, const String &p_permission, const String &p_target_xuid) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKPrivacy::check_permission_for_anonymous_user_async(const Ref<GDKUser> &p_user, const String &p_permission, const String &p_anonymous_user_type) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKPrivacy::batch_check_permission_async(const Ref<GDKUser> &p_user, const String &p_permission, const PackedStringArray &p_target_xuids) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKPrivacy::get_avoid_list_async(const Ref<GDKUser> &p_user) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKPrivacy::get_mute_list_async(const Ref<GDKUser> &p_user) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

void GDKPrivacy::on_user_removed(const Ref<GDKUser> &p_user) {}
#endif
