/**************************************************************************/
/*  gdk_achievement.cpp                                                   */
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
#include "gdk_achievement.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "gdk_windows.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "gdk.h"
#include "gdk_pending_signal.h"
#include "gdk_result.h"
#include "gdk_runtime.h"
#include "gdk_user.h"
#include "gdk_xbox_services.h"

#ifdef XBOX_MODULE_GDK_ENABLED
namespace {

String _utf8_or_empty(const char *p_value) {
	return p_value != nullptr ? String::utf8(p_value) : String();
}

String _progress_state_to_string(XblAchievementProgressState p_state) {
	switch (p_state) {
		case XblAchievementProgressState::Achieved:
			return "Achieved";
		case XblAchievementProgressState::NotStarted:
			return "NotStarted";
		case XblAchievementProgressState::InProgress:
			return "InProgress";
		case XblAchievementProgressState::Unknown:
		default:
			return "Unknown";
	}
}

bool _try_parse_progress_value(const char *p_value, double &r_parsed_value) {
	if (p_value == nullptr || *p_value == '\0') {
		return false;
	}

	char *end = nullptr;
	double parsed_value = std::strtod(p_value, &end);
	if (end == p_value || (end != nullptr && *end != '\0')) {
		return false;
	}

	r_parsed_value = parsed_value;
	return true;
}

int64_t _compute_progress_percent(const XblAchievement &p_achievement) {
	if (p_achievement.progressState == XblAchievementProgressState::Achieved) {
		return 100;
	}

	if (p_achievement.progression.requirements == nullptr || p_achievement.progression.requirementsCount == 0) {
		return p_achievement.progressState == XblAchievementProgressState::InProgress ? 1 : 0;
	}

	double total_ratio = 0.0;
	size_t parsed_requirements = 0;
	for (size_t i = 0; i < p_achievement.progression.requirementsCount; ++i) {
		const XblAchievementRequirement &requirement = p_achievement.progression.requirements[i];

		double current_value = 0.0;
		double target_value = 0.0;
		if (!_try_parse_progress_value(requirement.currentProgressValue, current_value) ||
				!_try_parse_progress_value(requirement.targetProgressValue, target_value) ||
				target_value <= 0.0) {
			continue;
		}

		total_ratio += std::clamp(current_value / target_value, 0.0, 1.0);
		++parsed_requirements;
	}

	if (parsed_requirements == 0) {
		return p_achievement.progressState == XblAchievementProgressState::InProgress ? 1 : 0;
	}

	return static_cast<int64_t>(std::round((total_ratio / static_cast<double>(parsed_requirements)) * 100.0));
}

} //namespace
#endif

void GDKAchievement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_id"), &GDKAchievement::get_id);
	ClassDB::bind_method(D_METHOD("get_name"), &GDKAchievement::get_name);
	ClassDB::bind_method(D_METHOD("get_service_configuration_id"), &GDKAchievement::get_service_configuration_id);
	ClassDB::bind_method(D_METHOD("get_progress_state"), &GDKAchievement::get_progress_state);
	ClassDB::bind_method(D_METHOD("get_progress_percent"), &GDKAchievement::get_progress_percent);
	ClassDB::bind_method(D_METHOD("is_unlocked"), &GDKAchievement::is_unlocked);
	ClassDB::bind_method(D_METHOD("is_secret"), &GDKAchievement::is_secret);
	ClassDB::bind_method(D_METHOD("get_locked_description"), &GDKAchievement::get_locked_description);
	ClassDB::bind_method(D_METHOD("get_unlocked_description"), &GDKAchievement::get_unlocked_description);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "id"), "", "get_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "name"), "", "get_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "service_configuration_id"), "", "get_service_configuration_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "progress_state"), "", "get_progress_state");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "progress_percent"), "", "get_progress_percent");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "unlocked"), "", "is_unlocked");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "secret"), "", "is_secret");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "locked_description"), "", "get_locked_description");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "unlocked_description"), "", "get_unlocked_description");
}

String GDKAchievement::get_id() const {
	return m_id;
}

String GDKAchievement::get_name() const {
	return m_name;
}

String GDKAchievement::get_service_configuration_id() const {
	return m_service_configuration_id;
}

String GDKAchievement::get_progress_state() const {
	return m_progress_state;
}

int64_t GDKAchievement::get_progress_percent() const {
	return m_progress_percent;
}

bool GDKAchievement::is_unlocked() const {
	return m_unlocked;
}

bool GDKAchievement::is_secret() const {
	return m_secret;
}

String GDKAchievement::get_locked_description() const {
	return m_locked_description;
}

String GDKAchievement::get_unlocked_description() const {
	return m_unlocked_description;
}

bool GDKAchievement::matches_id(const String &p_id) const {
	return m_id == p_id;
}

#ifdef XBOX_MODULE_GDK_ENABLED
void GDKAchievement::populate_from_native(const XblAchievement &p_native_achievement) {
	m_id = _utf8_or_empty(p_native_achievement.id);
	m_name = _utf8_or_empty(p_native_achievement.name);
	m_service_configuration_id = _utf8_or_empty(p_native_achievement.serviceConfigurationId);
	m_progress_state = _progress_state_to_string(p_native_achievement.progressState);
	m_progress_percent = _compute_progress_percent(p_native_achievement);
	m_unlocked = p_native_achievement.progressState == XblAchievementProgressState::Achieved;
	m_secret = p_native_achievement.isSecret;
	m_locked_description = _utf8_or_empty(p_native_achievement.lockedDescription);
	m_unlocked_description = _utf8_or_empty(p_native_achievement.unlockedDescription);
}
#else
void GDKAchievement::populate_from_native(const XblAchievement &p_native_achievement) {
}
#endif

void GDKAchievements::_bind_methods() {
	ClassDB::bind_method(D_METHOD("query_player_achievements_async", "user"), &GDKAchievements::query_player_achievements_async);
	ClassDB::bind_method(D_METHOD("update_achievement_async", "user", "achievement_id", "percent_complete"), &GDKAchievements::update_achievement_async);
	ClassDB::bind_method(D_METHOD("get_cached_achievements", "user"), &GDKAchievements::get_cached_achievements);

	ADD_SIGNAL(MethodInfo("achievement_unlocked",
			PropertyInfo(Variant::OBJECT, "user"),
			PropertyInfo(Variant::STRING, "achievement_id")));
	ADD_SIGNAL(MethodInfo("achievements_updated", PropertyInfo(Variant::OBJECT, "user")));
	ADD_SIGNAL(MethodInfo("runtime_error", PropertyInfo(Variant::OBJECT, "result", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "GDKResult")));
}

#ifdef XBOX_MODULE_GDK_ENABLED
GDKRuntime *GDKAchievements::_get_runtime() const {
	return m_owner != nullptr ? m_owner->get_runtime() : nullptr;
}

GDKXboxServices *GDKAchievements::_get_xbox_services() const {
	return m_owner != nullptr ? m_owner->get_xbox_services() : nullptr;
}

Signal GDKAchievements::_make_completed_signal(const Ref<GDKResult> &p_result) const {
	GDKRuntime *runtime = _get_runtime();
	Ref<GDKPendingSignal> pending_signal = runtime != nullptr ? runtime->make_pending_signal() : Ref<GDKPendingSignal>();
	if (pending_signal.is_null()) {
		pending_signal.instantiate();
	}
	pending_signal->complete_deferred(p_result);
	return pending_signal->get_completed_signal();
}

Signal GDKAchievements::_make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message) const {
	GDKRuntime *runtime = _get_runtime();
	if (runtime != nullptr) {
		return runtime->make_error_signal(p_hresult, p_code, p_message);
	}

	Ref<GDKPendingSignal> pending_signal;
	pending_signal.instantiate();
	pending_signal->complete_deferred(GDKResult::error_result(p_hresult, p_code, p_message));
	return pending_signal->get_completed_signal();
}

GDKAchievements::UserState *GDKAchievements::_find_user_state_by_xuid(uint64_t p_xbox_user_id) {
	for (UserState &state : m_user_states) {
		if (state.xbox_user_id == p_xbox_user_id) {
			return &state;
		}
	}

	return nullptr;
}

GDKAchievements::UserState *GDKAchievements::_find_user_state_by_local_id(XUserLocalId p_local_id) {
	for (UserState &state : m_user_states) {
		if (state.user.is_valid() && state.user->matches_local_id(p_local_id)) {
			return &state;
		}
	}

	return nullptr;
}

Ref<GDKAchievement> GDKAchievements::_find_cached_achievement(const UserState &p_state, const String &p_achievement_id) const {
	for (const Ref<GDKAchievement> &achievement : p_state.achievements) {
		if (achievement.is_valid() && achievement->matches_id(p_achievement_id)) {
			return achievement;
		}
	}

	return Ref<GDKAchievement>();
}

Array GDKAchievements::_get_cached_achievements_array(const UserState &p_state) const {
	Array achievements;
	for (const Ref<GDKAchievement> &achievement : p_state.achievements) {
		achievements.push_back(achievement);
	}
	return achievements;
}

Ref<GDKResult> GDKAchievements::_ensure_user_state(const Ref<GDKUser> &p_user, UserState **r_state) {
	ERR_FAIL_COND_V(r_state == nullptr, GDKResult::error_result(E_POINTER, "internal_error", "Missing achievements user-state output."));

	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr || !runtime->is_initialized()) {
		return GDKResult::error_result(E_FAIL, "not_initialized", "GDK is not initialized. Call GDK.initialize() first.");
	}

	GDKXboxServices *xbox_services = _get_xbox_services();
	if (xbox_services == nullptr || !xbox_services->is_initialized()) {
		return GDKResult::error_result(
				E_FAIL,
				"xbox_services_not_initialized",
				"Xbox services are unavailable. Ensure the title has a TitleId before using achievements.");
	}

	if (!p_user.is_valid() || p_user->get_handle() == nullptr) {
		return GDKResult::error_result(E_INVALIDARG, "invalid_user", "A signed-in GDKUser is required for achievements.");
	}

	uint64_t xbox_user_id = 0;
	HRESULT hr = xbox_services->get_xbox_user_id(p_user, &xbox_user_id);
	if (FAILED(hr)) {
		return GDKResult::hresult_error(hr, "Failed to resolve the Xbox services context for the user.", "xbox_services_context_failed");
	}

	UserState *state = _find_user_state_by_xuid(xbox_user_id);
	if (state == nullptr) {
		UserState new_state;
		new_state.user = p_user;
		new_state.xbox_user_id = xbox_user_id;
		m_user_states.push_back(new_state);
		state = &m_user_states.back();
	} else {
		state->user = p_user;
	}

	if (!state->manager_added) {
		hr = XblAchievementsManagerAddLocalUser(p_user->get_handle(), runtime->get_task_queue());
		if (FAILED(hr)) {
			return GDKResult::hresult_error(hr, "Failed to register the user with Achievements Manager.", "achievement_manager_add_user_failed");
		}

		state->manager_added = true;
		state->initialized = SUCCEEDED(XblAchievementsManagerIsUserInitialized(state->xbox_user_id));
	}

	*r_state = state;
	return GDKResult::ok_result();
}

Ref<GDKResult> GDKAchievements::_refresh_user_cache(UserState &p_state) {
	XblAchievementsManagerResultHandle result_handle = nullptr;
	HRESULT hr = XblAchievementsManagerGetAchievements(
			p_state.xbox_user_id,
			XblAchievementOrderBy::DefaultOrder,
			XblAchievementsManagerSortOrder::Unsorted,
			&result_handle);
	if (FAILED(hr)) {
		return GDKResult::hresult_error(hr, "Failed to read the achievements cache for the user.", "achievement_cache_read_failed");
	}

	const XblAchievement *achievements = nullptr;
	uint64_t achievements_count = 0;
	hr = XblAchievementsManagerResultGetAchievements(result_handle, &achievements, &achievements_count);
	if (FAILED(hr)) {
		XblAchievementsManagerResultCloseHandle(result_handle);
		return GDKResult::hresult_error(hr, "Failed to translate the achievements cache.", "achievement_cache_translate_failed");
	}

	p_state.achievements.clear();
	for (uint64_t i = 0; i < achievements_count; ++i) {
		Ref<GDKAchievement> achievement;
		achievement.instantiate();
		achievement->populate_from_native(achievements[i]);
		p_state.achievements.push_back(achievement);
	}

	XblAchievementsManagerResultCloseHandle(result_handle);
	return GDKResult::ok_result(_get_cached_achievements_array(p_state));
}

Ref<GDKResult> GDKAchievements::_refresh_single_achievement(UserState &p_state, const String &p_achievement_id) {
	CharString achievement_id_utf8 = p_achievement_id.utf8();

	XblAchievementsManagerResultHandle result_handle = nullptr;
	HRESULT hr = XblAchievementsManagerGetAchievement(
			p_state.xbox_user_id,
			achievement_id_utf8.get_data(),
			&result_handle);
	if (FAILED(hr)) {
		return GDKResult::hresult_error(hr, "Failed to read the cached achievement state.", "achievement_read_failed");
	}

	const XblAchievement *achievements = nullptr;
	uint64_t achievements_count = 0;
	hr = XblAchievementsManagerResultGetAchievements(result_handle, &achievements, &achievements_count);
	if (FAILED(hr)) {
		XblAchievementsManagerResultCloseHandle(result_handle);
		return GDKResult::hresult_error(hr, "Failed to translate the cached achievement state.", "achievement_translate_failed");
	}

	if (achievements_count == 0) {
		XblAchievementsManagerResultCloseHandle(result_handle);
		return GDKResult::error_result(E_FAIL, "achievement_not_found", "The cached achievement state was empty.");
	}

	Ref<GDKAchievement> achievement;
	achievement.instantiate();
	achievement->populate_from_native(achievements[0]);

	bool updated = false;
	for (Ref<GDKAchievement> &existing : p_state.achievements) {
		if (existing.is_valid() && existing->matches_id(p_achievement_id)) {
			existing = achievement;
			updated = true;
			break;
		}
	}
	if (!updated) {
		p_state.achievements.push_back(achievement);
	}

	XblAchievementsManagerResultCloseHandle(result_handle);
	return GDKResult::ok_result(achievement);
}

Ref<GDKResult> GDKAchievements::_submit_update(PendingUpdateOp &p_pending_update) {
	HRESULT hr = XblAchievementsManagerUpdateAchievement(
			p_pending_update.xbox_user_id,
			p_pending_update.achievement_id.utf8().get_data(),
			static_cast<uint8_t>(p_pending_update.percent_complete));
	if (FAILED(hr)) {
		return GDKResult::hresult_error(hr, "Failed to submit the achievement update.", "achievement_update_submit_failed");
	}

	p_pending_update.submitted = true;
	return GDKResult::ok_result();
}

void GDKAchievements::_complete_pending_queries(UserState &p_state) {
	Ref<GDKResult> result = GDKResult::ok_result(_get_cached_achievements_array(p_state));
	GDKRuntime *runtime = _get_runtime();
	if (runtime != nullptr) {
	}

	m_pending_query_ops.erase(
			std::remove_if(
					m_pending_query_ops.begin(),
					m_pending_query_ops.end(),
					[&p_state, &result](PendingQueryOp &pending_query) {
						if (pending_query.xbox_user_id != p_state.xbox_user_id) {
							return false;
						}

						if (pending_query.request.is_valid()) {
							pending_query.request->complete(result);
						}
						return true;
					}),
			m_pending_query_ops.end());
}

void GDKAchievements::_fail_pending_queries(uint64_t p_xbox_user_id, const Ref<GDKResult> &p_result) {
	GDKRuntime *runtime = _get_runtime();
	if (runtime != nullptr) {
	}

	m_pending_query_ops.erase(
			std::remove_if(
					m_pending_query_ops.begin(),
					m_pending_query_ops.end(),
					[p_xbox_user_id, &p_result](PendingQueryOp &pending_query) {
						if (p_xbox_user_id != UINT64_MAX && pending_query.xbox_user_id != p_xbox_user_id) {
							return false;
						}

						if (pending_query.request.is_valid()) {
							pending_query.request->complete(p_result);
						}
						return true;
					}),
			m_pending_query_ops.end());
}

void GDKAchievements::_complete_pending_updates(UserState &p_state, const String &p_achievement_id) {
	Ref<GDKAchievement> achievement = _find_cached_achievement(p_state, p_achievement_id);
	if (!achievement.is_valid()) {
		return;
	}

	Ref<GDKResult> result = GDKResult::ok_result(achievement);
	GDKRuntime *runtime = _get_runtime();
	if (runtime != nullptr) {
	}

	m_pending_update_ops.erase(
			std::remove_if(
					m_pending_update_ops.begin(),
					m_pending_update_ops.end(),
					[&p_state, &p_achievement_id, &achievement, &result](PendingUpdateOp &pending_update) {
						if (pending_update.xbox_user_id != p_state.xbox_user_id ||
								pending_update.achievement_id != p_achievement_id ||
								!pending_update.submitted) {
							return false;
						}

						if (!achievement->is_unlocked() &&
								achievement->get_progress_percent() < static_cast<int64_t>(pending_update.percent_complete)) {
							return false;
						}

						if (pending_update.request.is_valid()) {
							pending_update.request->complete(result);
						}
						return true;
					}),
			m_pending_update_ops.end());
}

void GDKAchievements::_fail_pending_updates(uint64_t p_xbox_user_id, const Ref<GDKResult> &p_result) {
	GDKRuntime *runtime = _get_runtime();
	if (runtime != nullptr) {
	}

	m_pending_update_ops.erase(
			std::remove_if(
					m_pending_update_ops.begin(),
					m_pending_update_ops.end(),
					[p_xbox_user_id, &p_result](PendingUpdateOp &pending_update) {
						if (p_xbox_user_id != UINT64_MAX && pending_update.xbox_user_id != p_xbox_user_id) {
							return false;
						}

						if (pending_update.request.is_valid()) {
							pending_update.request->complete(p_result);
						}
						return true;
					}),
			m_pending_update_ops.end());
}

void GDKAchievements::_cancel_pending_query_signal(GDKPendingSignal *p_request) {
	if (p_request == nullptr) {
		return;
	}

	for (auto it = m_pending_query_ops.begin(); it != m_pending_query_ops.end(); ++it) {
		if (it->request.is_null() || it->request.ptr() != p_request) {
			continue;
		}

		Ref<GDKPendingSignal> pending_signal = it->request;
		m_pending_query_ops.erase(it);
		if (pending_signal.is_valid()) {
			pending_signal->clear_cancel_handler();
			pending_signal->complete(GDKResult::cancelled("Achievement query cancelled."));
		}
		return;
	}
}

void GDKAchievements::_cancel_pending_update_signal(GDKPendingSignal *p_request) {
	if (p_request == nullptr) {
		return;
	}

	for (auto it = m_pending_update_ops.begin(); it != m_pending_update_ops.end(); ++it) {
		if (it->request.is_null() || it->request.ptr() != p_request) {
			continue;
		}

		Ref<GDKPendingSignal> pending_signal = it->request;
		m_pending_update_ops.erase(it);
		if (pending_signal.is_valid()) {
			pending_signal->clear_cancel_handler();
			pending_signal->complete(GDKResult::cancelled("Achievement update cancelled."));
		}
		return;
	}
}

void GDKAchievements::_submit_waiting_updates(UserState &p_state) {
	GDKRuntime *runtime = _get_runtime();

	for (PendingUpdateOp &pending_update : m_pending_update_ops) {
		if (pending_update.xbox_user_id != p_state.xbox_user_id || pending_update.submitted || !pending_update.request.is_valid()) {
			continue;
		}

		Ref<GDKResult> submit_result = _submit_update(pending_update);
		if (!submit_result->is_ok()) {
			if (runtime != nullptr) {
			}
			pending_update.request->complete(submit_result);
		}
	}

	_erase_completed_updates();
}

void GDKAchievements::_erase_completed_updates() {
	m_pending_update_ops.erase(
			std::remove_if(
					m_pending_update_ops.begin(),
					m_pending_update_ops.end(),
					[](const PendingUpdateOp &pending_update) {
						return pending_update.request.is_null() || pending_update.request->is_done();
					}),
			m_pending_update_ops.end());
}

void GDKAchievements::_erase_user_state(uint64_t p_xbox_user_id) {
	m_user_states.erase(
			std::remove_if(
					m_user_states.begin(),
					m_user_states.end(),
					[p_xbox_user_id](const UserState &state) {
						return state.xbox_user_id == p_xbox_user_id;
					}),
			m_user_states.end());
}

void GDKAchievements::set_owner(GDK *p_owner) {
	m_owner = p_owner;
}

Ref<GDKResult> GDKAchievements::on_runtime_initialized() {
	m_runtime_ready = true;
	return GDKResult::ok_result();
}

void GDKAchievements::shutdown() {
	m_runtime_ready = false;

	Ref<GDKResult> cancelled = GDKResult::cancelled("Achievements operation cancelled during shutdown.");
	_fail_pending_queries(UINT64_MAX, cancelled);
	_fail_pending_updates(UINT64_MAX, cancelled);

	for (UserState &state : m_user_states) {
		if (state.manager_added && state.user.is_valid() && state.user->get_handle() != nullptr) {
			XblAchievementsManagerRemoveLocalUser(state.user->get_handle());
		}
	}

	m_pending_query_ops.clear();
	m_pending_update_ops.clear();
	m_user_states.clear();
}

int GDKAchievements::dispatch() {
	if (!m_runtime_ready) {
		return 0;
	}

	GDKXboxServices *xbox_services = _get_xbox_services();
	if (xbox_services == nullptr || !xbox_services->is_initialized()) {
		return 0;
	}

	const XblAchievementsManagerEvent *events = nullptr;
	size_t event_count = 0;
	HRESULT hr = XblAchievementsManagerDoWork(&events, &event_count);
	if (FAILED(hr)) {
		emit_signal("runtime_error", GDKResult::hresult_error(hr, "Failed to dispatch Achievements Manager state.", "achievement_manager_dispatch_failed"));
		return 0;
	}

	int handled_events = 0;
	for (size_t i = 0; i < event_count; ++i) {
		const XblAchievementsManagerEvent &event = events[i];
		UserState *state = _find_user_state_by_xuid(event.xboxUserId);
		if (state == nullptr || !state->user.is_valid()) {
			continue;
		}

		++handled_events;

		switch (event.eventType) {
			case XblAchievementsManagerEventType::LocalUserInitialStateSynced: {
				state->initialized = true;

				Ref<GDKResult> refresh_result = _refresh_user_cache(*state);
				if (!refresh_result->is_ok()) {
					_fail_pending_queries(state->xbox_user_id, refresh_result);
					_fail_pending_updates(state->xbox_user_id, refresh_result);
					continue;
				}

				emit_signal("achievements_updated", state->user);
				if (!m_runtime_ready) {
					return handled_events;
				}
				_complete_pending_queries(*state);
				_submit_waiting_updates(*state);
			} break;
			case XblAchievementsManagerEventType::AchievementProgressUpdated: {
				String achievement_id = _utf8_or_empty(event.progressInfo.achievementId);
				Ref<GDKResult> refresh_result = _refresh_single_achievement(*state, achievement_id);
				if (!refresh_result->is_ok()) {
					_fail_pending_updates(state->xbox_user_id, refresh_result);
					continue;
				}

				emit_signal("achievements_updated", state->user);
				if (!m_runtime_ready) {
					return handled_events;
				}
				_complete_pending_updates(*state, achievement_id);
			} break;
			case XblAchievementsManagerEventType::AchievementUnlocked: {
				String achievement_id = _utf8_or_empty(event.progressInfo.achievementId);
				Ref<GDKResult> refresh_result = _refresh_single_achievement(*state, achievement_id);
				if (!refresh_result->is_ok()) {
					_fail_pending_updates(state->xbox_user_id, refresh_result);
					continue;
				}

				emit_signal("achievements_updated", state->user);
				if (!m_runtime_ready) {
					return handled_events;
				}
				emit_signal("achievement_unlocked", state->user, achievement_id);
				if (!m_runtime_ready) {
					return handled_events;
				}
				_complete_pending_updates(*state, achievement_id);
			} break;
			default:
				break;
		}
	}

	return handled_events;
}

Signal GDKAchievements::query_player_achievements_async(const Ref<GDKUser> &p_user) {
	UserState *state = nullptr;
	Ref<GDKResult> ensure_result = _ensure_user_state(p_user, &state);
	if (!ensure_result->is_ok()) {
		return _make_error_signal(
				static_cast<HRESULT>(ensure_result->get_hresult()),
				ensure_result->get_code(),
				ensure_result->get_message());
	}

	if (state->initialized) {
		Ref<GDKResult> refresh_result = _refresh_user_cache(*state);
		if (!refresh_result->is_ok()) {
			return _make_error_signal(
					static_cast<HRESULT>(refresh_result->get_hresult()),
					refresh_result->get_code(),
					refresh_result->get_message());
		}
		return _make_completed_signal(refresh_result);
	}

	GDKRuntime *runtime = _get_runtime();
	Ref<GDKPendingSignal> pending_signal = runtime != nullptr ? runtime->make_pending_signal() : Ref<GDKPendingSignal>();
	ERR_FAIL_COND_V(pending_signal.is_null(), Signal());

	PendingQueryOp pending_query;
	pending_query.xbox_user_id = state->xbox_user_id;
	pending_query.request = pending_signal;
	m_pending_query_ops.push_back(pending_query);
	GDKPendingSignal *pending_signal_ptr = pending_signal.ptr();
	pending_signal->set_cancel_handler([this, pending_signal_ptr]() {
		_cancel_pending_query_signal(pending_signal_ptr);
	});

	return pending_signal->get_completed_signal();
}

Signal GDKAchievements::update_achievement_async(const Ref<GDKUser> &p_user, const String &p_achievement_id, int64_t p_percent_complete) {
	UserState *state = nullptr;
	Ref<GDKResult> ensure_result = _ensure_user_state(p_user, &state);
	if (!ensure_result->is_ok()) {
		return _make_error_signal(
				static_cast<HRESULT>(ensure_result->get_hresult()),
				ensure_result->get_code(),
				ensure_result->get_message());
	}

	if (p_achievement_id.is_empty()) {
		return _make_error_signal(E_INVALIDARG, "invalid_achievement_id", "Achievement updates require a non-empty achievement id.");
	}
	if (p_percent_complete < 1 || p_percent_complete > 100) {
		return _make_error_signal(E_INVALIDARG, "invalid_achievement_progress", "Achievement progress must be between 1 and 100.");
	}

	PendingUpdateOp pending_update;
	pending_update.xbox_user_id = state->xbox_user_id;
	pending_update.achievement_id = p_achievement_id;
	pending_update.percent_complete = static_cast<uint32_t>(p_percent_complete);

	if (state->initialized) {
		Ref<GDKResult> submit_result = _submit_update(pending_update);
		if (!submit_result->is_ok()) {
			return _make_error_signal(
					static_cast<HRESULT>(submit_result->get_hresult()),
					submit_result->get_code(),
					submit_result->get_message());
		}
	}

	GDKRuntime *runtime = _get_runtime();
	Ref<GDKPendingSignal> pending_signal = runtime != nullptr ? runtime->make_pending_signal() : Ref<GDKPendingSignal>();
	ERR_FAIL_COND_V(pending_signal.is_null(), Signal());
	pending_update.request = pending_signal;

	m_pending_update_ops.push_back(pending_update);
	GDKPendingSignal *pending_signal_ptr = pending_signal.ptr();
	pending_signal->set_cancel_handler([this, pending_signal_ptr]() {
		_cancel_pending_update_signal(pending_signal_ptr);
	});
	return pending_signal->get_completed_signal();
}

Array GDKAchievements::get_cached_achievements(const Ref<GDKUser> &p_user) const {
	if (!p_user.is_valid()) {
		return Array();
	}

	XUserLocalId local_id = {};
	local_id.value = static_cast<uint64_t>(p_user->get_local_id());

	for (const UserState &state : m_user_states) {
		if (state.user.is_valid() && state.user->matches_local_id(local_id)) {
			return _get_cached_achievements_array(state);
		}
	}

	return Array();
}

void GDKAchievements::on_user_removed(const Ref<GDKUser> &p_user) {
	if (!p_user.is_valid()) {
		return;
	}

	XUserLocalId local_id = {};
	local_id.value = static_cast<uint64_t>(p_user->get_local_id());

	UserState *state = _find_user_state_by_local_id(local_id);
	if (state == nullptr) {
		return;
	}

	Ref<GDKResult> cancelled = GDKResult::cancelled("Achievement operation cancelled because the user signed out.");
	_fail_pending_queries(state->xbox_user_id, cancelled);
	_fail_pending_updates(state->xbox_user_id, cancelled);

	if (state->manager_added && p_user->get_handle() != nullptr) {
		XblAchievementsManagerRemoveLocalUser(p_user->get_handle());
	}

	_erase_user_state(state->xbox_user_id);
}

#else
Signal GDKAchievements::_make_completed_signal(const Ref<GDKResult> &p_result) const {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKAchievements::_make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message) const {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Ref<GDKAchievement> GDKAchievements::_find_cached_achievement(const UserState &p_state, const String &p_achievement_id) const {
	return Ref<GDKAchievement>();
}

Array GDKAchievements::_get_cached_achievements_array(const UserState &p_state) const {
	return Array();
}

Ref<GDKResult> GDKAchievements::_ensure_user_state(const Ref<GDKUser> &p_user, UserState **r_state) {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Ref<GDKResult> GDKAchievements::_refresh_user_cache(UserState &p_state) {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Ref<GDKResult> GDKAchievements::_refresh_single_achievement(UserState &p_state, const String &p_achievement_id) {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Ref<GDKResult> GDKAchievements::_submit_update(PendingUpdateOp &p_pending_update) {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

void GDKAchievements::_complete_pending_queries(UserState &p_state) {}

void GDKAchievements::_fail_pending_queries(uint64_t p_xbox_user_id, const Ref<GDKResult> &p_result) {}

void GDKAchievements::_complete_pending_updates(UserState &p_state, const String &p_achievement_id) {}

void GDKAchievements::_fail_pending_updates(uint64_t p_xbox_user_id, const Ref<GDKResult> &p_result) {}

void GDKAchievements::_cancel_pending_query_signal(GDKPendingSignal *p_request) {}

void GDKAchievements::_cancel_pending_update_signal(GDKPendingSignal *p_request) {}

void GDKAchievements::_submit_waiting_updates(UserState &p_state) {}

void GDKAchievements::_erase_completed_updates() {}

void GDKAchievements::_erase_user_state(uint64_t p_xbox_user_id) {}

void GDKAchievements::set_owner(GDK *p_owner) {}

Ref<GDKResult> GDKAchievements::on_runtime_initialized() {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

void GDKAchievements::shutdown() {}

int GDKAchievements::dispatch() {
	return 0;
}

Signal GDKAchievements::query_player_achievements_async(const Ref<GDKUser> &p_user) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKAchievements::update_achievement_async(const Ref<GDKUser> &p_user, const String &p_achievement_id, int64_t p_percent_complete) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Array GDKAchievements::get_cached_achievements(const Ref<GDKUser> &p_user) const {
	return Array();
}

void GDKAchievements::on_user_removed(const Ref<GDKUser> &p_user) {}
#endif
