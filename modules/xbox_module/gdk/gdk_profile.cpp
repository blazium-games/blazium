/**************************************************************************/
/*  gdk_profile.cpp                                                       */
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

#include "gdk_profile.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "gdk_windows.h"

#include <cerrno>
#include <cstdlib>
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
	if (p_value == nullptr || p_value[0] == '\0') {
		return String();
	}
	return String::utf8(p_value);
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

Ref<GDKUserProfile> _make_user_profile(const XblUserProfile &p_profile) {
	Ref<GDKUserProfile> profile;
	profile.instantiate();
	profile->populate_from_native(p_profile);
	return profile;
}

Array _make_user_profile_array(const std::vector<XblUserProfile> &p_profiles) {
	Array result;
	for (const XblUserProfile &profile : p_profiles) {
		result.push_back(_make_user_profile(profile));
	}
	return result;
}

class ProfileAsyncContext final : public GDKSignalXAsyncContext {
public:
	enum Mode {
		MODE_SINGLE,
		MODE_MULTIPLE,
		MODE_SOCIAL_GROUP,
	};

private:
	XblContextHandle m_context = nullptr;
	Mode m_mode = MODE_SINGLE;
	uint64_t m_target_xuid = 0;
	std::vector<uint64_t> m_xuids;
	String m_social_group;
	CharString m_social_group_utf8;

protected:
	void finalize(XAsyncBlock *p_async_block) override {
		Ref<GDKResult> result;

		if (get_runtime()->is_shutting_down() || get_pending_signal()->was_cancel_requested()) {
			result = GDKResult::cancelled("Profile query cancelled.");
			get_pending_signal()->complete(result);
			return;
		}

		if (m_mode == MODE_SINGLE) {
			XblUserProfile profile = {};
			HRESULT result_hr = XblProfileGetUserProfileResult(p_async_block, &profile);
			if (result_hr == E_ABORT) {
				result = GDKResult::cancelled("Profile query cancelled.");
				get_pending_signal()->complete(result);
				return;
			}
			if (FAILED(result_hr)) {
				result = GDKResult::hresult_error(result_hr, "Failed to retrieve the profile result.", "profile_result_failed");
				get_pending_signal()->complete(result);
				return;
			}

			get_pending_signal()->complete(GDKResult::ok_result(_make_user_profile(profile)));
			return;
		}

		size_t profile_count = 0;
		HRESULT result_hr = m_mode == MODE_SOCIAL_GROUP ? XblProfileGetUserProfilesForSocialGroupResultCount(p_async_block, &profile_count) : XblProfileGetUserProfilesResultCount(p_async_block, &profile_count);
		if (result_hr == E_ABORT) {
			result = GDKResult::cancelled("Profile query cancelled.");
			get_pending_signal()->complete(result);
			return;
		}
		if (FAILED(result_hr)) {
			result = GDKResult::hresult_error(result_hr, "Failed to retrieve the profile result count.", "profile_result_count_failed");
			get_pending_signal()->complete(result);
			return;
		}

		std::vector<XblUserProfile> profiles(profile_count);
		if (profile_count > 0) {
			result_hr = m_mode == MODE_SOCIAL_GROUP ? XblProfileGetUserProfilesForSocialGroupResult(p_async_block, profiles.size(), profiles.data()) : XblProfileGetUserProfilesResult(p_async_block, profiles.size(), profiles.data());
			if (result_hr == E_ABORT) {
				result = GDKResult::cancelled("Profile query cancelled.");
				get_pending_signal()->complete(result);
				return;
			}
			if (FAILED(result_hr)) {
				result = GDKResult::hresult_error(result_hr, "Failed to retrieve profile results.", "profile_results_failed");
				get_pending_signal()->complete(result);
				return;
			}
		}

		get_pending_signal()->complete(GDKResult::ok_result(_make_user_profile_array(profiles)));
	}

public:
	ProfileAsyncContext(GDKRuntime *p_runtime, const Ref<GDKPendingSignal> &p_pending_signal, XblContextHandle p_context, uint64_t p_target_xuid) :
			GDKSignalXAsyncContext(p_runtime, p_pending_signal),
			m_context(p_context),
			m_mode(MODE_SINGLE),
			m_target_xuid(p_target_xuid) {}

	ProfileAsyncContext(GDKRuntime *p_runtime, const Ref<GDKPendingSignal> &p_pending_signal, XblContextHandle p_context, std::vector<uint64_t> p_xuids) :
			GDKSignalXAsyncContext(p_runtime, p_pending_signal),
			m_context(p_context),
			m_mode(MODE_MULTIPLE),
			m_xuids(std::move(p_xuids)) {}

	ProfileAsyncContext(GDKRuntime *p_runtime, const Ref<GDKPendingSignal> &p_pending_signal, XblContextHandle p_context, const String &p_social_group) :
			GDKSignalXAsyncContext(p_runtime, p_pending_signal),
			m_context(p_context),
			m_mode(MODE_SOCIAL_GROUP),
			m_social_group(p_social_group) {
		m_social_group_utf8 = m_social_group.utf8();
	}

	~ProfileAsyncContext() override {
		if (m_context != nullptr) {
			XblContextCloseHandle(m_context);
			m_context = nullptr;
		}
	}

	XblContextHandle get_context() const {
		return m_context;
	}

	uint64_t get_target_xuid() const {
		return m_target_xuid;
	}

	uint64_t *get_xuids_data() {
		return m_xuids.data();
	}

	size_t get_xuids_count() const {
		return m_xuids.size();
	}

	const char *get_social_group() const {
		return m_social_group_utf8.get_data();
	}
};

} //namespace

#endif

void GDKUserProfile::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_xuid"), &GDKUserProfile::get_xuid);
	ClassDB::bind_method(D_METHOD("get_app_display_name"), &GDKUserProfile::get_app_display_name);
	ClassDB::bind_method(D_METHOD("get_app_display_picture_resize_uri"), &GDKUserProfile::get_app_display_picture_resize_uri);
	ClassDB::bind_method(D_METHOD("get_game_display_name"), &GDKUserProfile::get_game_display_name);
	ClassDB::bind_method(D_METHOD("get_game_display_picture_resize_uri"), &GDKUserProfile::get_game_display_picture_resize_uri);
	ClassDB::bind_method(D_METHOD("get_gamerscore"), &GDKUserProfile::get_gamerscore);
	ClassDB::bind_method(D_METHOD("get_gamertag"), &GDKUserProfile::get_gamertag);
	ClassDB::bind_method(D_METHOD("get_modern_gamertag"), &GDKUserProfile::get_modern_gamertag);
	ClassDB::bind_method(D_METHOD("get_modern_gamertag_suffix"), &GDKUserProfile::get_modern_gamertag_suffix);
	ClassDB::bind_method(D_METHOD("get_unique_modern_gamertag"), &GDKUserProfile::get_unique_modern_gamertag);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "xuid"), "", "get_xuid");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "app_display_name"), "", "get_app_display_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "app_display_picture_resize_uri"), "", "get_app_display_picture_resize_uri");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "game_display_name"), "", "get_game_display_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "game_display_picture_resize_uri"), "", "get_game_display_picture_resize_uri");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "gamerscore"), "", "get_gamerscore");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "gamertag"), "", "get_gamertag");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "modern_gamertag"), "", "get_modern_gamertag");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "modern_gamertag_suffix"), "", "get_modern_gamertag_suffix");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "unique_modern_gamertag"), "", "get_unique_modern_gamertag");
}

String GDKUserProfile::get_xuid() const {
	return m_xuid;
}

String GDKUserProfile::get_app_display_name() const {
	return m_app_display_name;
}

String GDKUserProfile::get_app_display_picture_resize_uri() const {
	return m_app_display_picture_resize_uri;
}

String GDKUserProfile::get_game_display_name() const {
	return m_game_display_name;
}

String GDKUserProfile::get_game_display_picture_resize_uri() const {
	return m_game_display_picture_resize_uri;
}

String GDKUserProfile::get_gamerscore() const {
	return m_gamerscore;
}

String GDKUserProfile::get_gamertag() const {
	return m_gamertag;
}

String GDKUserProfile::get_modern_gamertag() const {
	return m_modern_gamertag;
}

String GDKUserProfile::get_modern_gamertag_suffix() const {
	return m_modern_gamertag_suffix;
}

String GDKUserProfile::get_unique_modern_gamertag() const {
	return m_unique_modern_gamertag;
}

#ifdef XBOX_MODULE_GDK_ENABLED
void GDKUserProfile::populate_from_native(const XblUserProfile &p_profile) {
	m_xuid = String::num_uint64(p_profile.xboxUserId);
	m_app_display_name = _utf8_or_empty(p_profile.appDisplayName);
	m_app_display_picture_resize_uri = _utf8_or_empty(p_profile.appDisplayPictureResizeUri);
	m_game_display_name = _utf8_or_empty(p_profile.gameDisplayName);
	m_game_display_picture_resize_uri = _utf8_or_empty(p_profile.gameDisplayPictureResizeUri);
	m_gamerscore = _utf8_or_empty(p_profile.gamerscore);
	m_gamertag = _utf8_or_empty(p_profile.gamertag);
	m_modern_gamertag = _utf8_or_empty(p_profile.modernGamertag);
	m_modern_gamertag_suffix = _utf8_or_empty(p_profile.modernGamertagSuffix);
	m_unique_modern_gamertag = _utf8_or_empty(p_profile.uniqueModernGamertag);
}
#else
void GDKUserProfile::populate_from_native(const XblUserProfile &p_profile) {
}
#endif

void GDKProfile::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_profile_async", "user", "xuid"), &GDKProfile::get_profile_async);
	ClassDB::bind_method(D_METHOD("get_profiles_async", "user", "xuids"), &GDKProfile::get_profiles_async);
	ClassDB::bind_method(D_METHOD("get_profiles_for_social_group_async", "user", "social_group"), &GDKProfile::get_profiles_for_social_group_async);
}

#ifdef XBOX_MODULE_GDK_ENABLED
void GDKProfile::set_owner(GDK *p_owner) {
	m_owner = p_owner;
}

GDKRuntime *GDKProfile::_get_runtime() const {
	return m_owner == nullptr ? nullptr : m_owner->get_runtime();
}

GDKXboxServices *GDKProfile::_get_xbox_services() const {
	return m_owner == nullptr ? nullptr : m_owner->get_xbox_services();
}

Signal GDKProfile::_make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data) const {
	GDKRuntime *runtime = _get_runtime();
	ERR_FAIL_NULL_V(runtime, Signal());
	return runtime->make_error_signal(p_hresult, p_code, p_message, p_data);
}

Ref<GDKResult> GDKProfile::_ensure_ready_user(const Ref<GDKUser> &p_user) const {
	if (!m_runtime_ready) {
		return GDKResult::error_result(E_FAIL, "runtime_unavailable", "GDK runtime is not initialized.");
	}
	if (!p_user.is_valid() || p_user->get_handle() == nullptr || !p_user->is_signed_in()) {
		return GDKResult::error_result(E_INVALIDARG, "invalid_user", "A signed-in GDKUser is required.");
	}
	return GDKResult::ok_result();
}

Ref<GDKResult> GDKProfile::on_runtime_initialized() {
	m_runtime_ready = true;
	return GDKResult::ok_result();
}

void GDKProfile::shutdown() {
	m_runtime_ready = false;
}

Signal GDKProfile::get_profile_async(const Ref<GDKUser> &p_user, const String &p_xuid) {
	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr) {
		return Signal();
	}

	Ref<GDKResult> validation = _ensure_ready_user(p_user);
	if (!validation->is_ok()) {
		return _make_error_signal(static_cast<HRESULT>(validation->get_hresult()), validation->get_code(), validation->get_message());
	}

	uint64_t target_xuid = 0;
	if (!_try_parse_xuid(p_xuid, &target_xuid)) {
		return _make_error_signal(E_INVALIDARG, "invalid_xuid", "xuid must be a non-empty decimal XUID string.");
	}

	GDKXboxServices *xbox_services = _get_xbox_services();
	if (xbox_services == nullptr || !xbox_services->is_initialized()) {
		return _make_error_signal(E_FAIL, "xbox_services_not_initialized", "Xbox services are unavailable. Ensure the title has a TitleId before using profile.");
	}

	XblContextHandle context = nullptr;
	HRESULT hr = xbox_services->duplicate_context_for_user(p_user, &context);
	if (FAILED(hr)) {
		return _make_error_signal(hr, "xbox_context_unavailable", "Failed to create an Xbox services context for the user.");
	}

	Ref<GDKPendingSignal> pending_signal = runtime->make_pending_signal();
	ProfileAsyncContext *async_context = new ProfileAsyncContext(runtime, pending_signal, context, target_xuid);
	async_context->bind_cancel_handler();
	hr = XblProfileGetUserProfileAsync(async_context->get_context(), async_context->get_target_xuid(), async_context->get_async_block());
	if (FAILED(hr)) {
		async_context->clear_cancel_handler();
		delete async_context;
		return _make_error_signal(hr, "profile_query_start_failed", "Failed to start the profile query.");
	}
	return pending_signal->get_completed_signal();
}

Signal GDKProfile::get_profiles_async(const Ref<GDKUser> &p_user, const PackedStringArray &p_xuids) {
	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr) {
		return Signal();
	}

	Ref<GDKResult> validation = _ensure_ready_user(p_user);
	if (!validation->is_ok()) {
		return _make_error_signal(static_cast<HRESULT>(validation->get_hresult()), validation->get_code(), validation->get_message());
	}

	if (p_xuids.is_empty()) {
		return _make_error_signal(E_INVALIDARG, "invalid_xuids", "At least one XUID is required.");
	}

	std::vector<uint64_t> xuids;
	xuids.reserve(static_cast<size_t>(p_xuids.size()));
	for (int64_t i = 0; i < p_xuids.size(); ++i) {
		uint64_t xuid = 0;
		if (!_try_parse_xuid(p_xuids[i], &xuid)) {
			return _make_error_signal(E_INVALIDARG, "invalid_xuid", "xuids must contain decimal XUID strings.");
		}
		xuids.push_back(xuid);
	}

	GDKXboxServices *xbox_services = _get_xbox_services();
	if (xbox_services == nullptr || !xbox_services->is_initialized()) {
		return _make_error_signal(E_FAIL, "xbox_services_not_initialized", "Xbox services are unavailable. Ensure the title has a TitleId before using profile.");
	}

	XblContextHandle context = nullptr;
	HRESULT hr = xbox_services->duplicate_context_for_user(p_user, &context);
	if (FAILED(hr)) {
		return _make_error_signal(hr, "xbox_context_unavailable", "Failed to create an Xbox services context for the user.");
	}

	Ref<GDKPendingSignal> pending_signal = runtime->make_pending_signal();
	ProfileAsyncContext *async_context = new ProfileAsyncContext(runtime, pending_signal, context, std::move(xuids));
	async_context->bind_cancel_handler();
	hr = XblProfileGetUserProfilesAsync(async_context->get_context(), async_context->get_xuids_data(), async_context->get_xuids_count(), async_context->get_async_block());
	if (FAILED(hr)) {
		async_context->clear_cancel_handler();
		delete async_context;
		return _make_error_signal(hr, "profile_query_start_failed", "Failed to start the profiles query.");
	}
	return pending_signal->get_completed_signal();
}

Signal GDKProfile::get_profiles_for_social_group_async(const Ref<GDKUser> &p_user, const String &p_social_group) {
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
		return _make_error_signal(E_INVALIDARG, "invalid_social_group", "Profile social-group queries require a non-empty social group name.");
	}

	GDKXboxServices *xbox_services = _get_xbox_services();
	if (xbox_services == nullptr || !xbox_services->is_initialized()) {
		return _make_error_signal(E_FAIL, "xbox_services_not_initialized", "Xbox services are unavailable. Ensure the title has a TitleId before using profile.");
	}

	XblContextHandle context = nullptr;
	HRESULT hr = xbox_services->duplicate_context_for_user(p_user, &context);
	if (FAILED(hr)) {
		return _make_error_signal(hr, "xbox_context_unavailable", "Failed to create an Xbox services context for the user.");
	}

	Ref<GDKPendingSignal> pending_signal = runtime->make_pending_signal();
	ProfileAsyncContext *async_context = new ProfileAsyncContext(runtime, pending_signal, context, social_group);
	async_context->bind_cancel_handler();
	hr = XblProfileGetUserProfilesForSocialGroupAsync(async_context->get_context(), async_context->get_social_group(), async_context->get_async_block());
	if (FAILED(hr)) {
		async_context->clear_cancel_handler();
		delete async_context;
		return _make_error_signal(hr, "profile_social_group_query_start_failed", "Failed to start the social-group profile query.");
	}
	return pending_signal->get_completed_signal();
}

void GDKProfile::on_user_removed(const Ref<GDKUser> &p_user) {
	(void)p_user;
}

#else
void GDKProfile::set_owner(GDK *p_owner) {}

Signal GDKProfile::_make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data) const {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Ref<GDKResult> GDKProfile::_ensure_ready_user(const Ref<GDKUser> &p_user) const {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Ref<GDKResult> GDKProfile::on_runtime_initialized() {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

void GDKProfile::shutdown() {}

Signal GDKProfile::get_profile_async(const Ref<GDKUser> &p_user, const String &p_xuid) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKProfile::get_profiles_async(const Ref<GDKUser> &p_user, const PackedStringArray &p_xuids) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKProfile::get_profiles_for_social_group_async(const Ref<GDKUser> &p_user, const String &p_social_group) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

void GDKProfile::on_user_removed(const Ref<GDKUser> &p_user) {}
#endif
