/**************************************************************************/
/*  gdk_achievement.h                                                     */
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

#include "core/variant/callable.h"
#include "gdk_gdk_stubs.h"

#include <vector>

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/variant/array.h"

#ifdef XBOX_MODULE_GDK_ENABLED
#include <XUser.h>
#include <xsapi-c/services_c.h>
#endif

#include "gdk_pending_signal.h"

class GDK;
class GDKResult;
class GDKRuntime;
class GDKUser;
class GDKXboxServices;

class GDKAchievement : public RefCounted {
	GDCLASS(GDKAchievement, RefCounted);

	String m_id;
	String m_name;
	String m_service_configuration_id;
	String m_progress_state;
	int64_t m_progress_percent = 0;
	bool m_unlocked = false;
	bool m_secret = false;
	String m_locked_description;
	String m_unlocked_description;

protected:
	static void _bind_methods();

public:
	String get_id() const;
	String get_name() const;
	String get_service_configuration_id() const;
	String get_progress_state() const;
	int64_t get_progress_percent() const;
	bool is_unlocked() const;
	bool is_secret() const;
	String get_locked_description() const;
	String get_unlocked_description() const;

	bool matches_id(const String &p_id) const;
	void populate_from_native(const XblAchievement &p_native_achievement);
};

class GDKAchievements : public RefCounted {
	GDCLASS(GDKAchievements, RefCounted);

	struct UserState {
		Ref<GDKUser> user;
		uint64_t xbox_user_id = 0;
		bool manager_added = false;
		bool initialized = false;
		std::vector<Ref<GDKAchievement>> achievements;
	};

	struct PendingQueryOp {
		uint64_t xbox_user_id = 0;
		Ref<GDKPendingSignal> request;
	};

	struct PendingUpdateOp {
		uint64_t xbox_user_id = 0;
		String achievement_id;
		uint32_t percent_complete = 0;
		bool submitted = false;
		Ref<GDKPendingSignal> request;
	};

	GDK *m_owner = nullptr;
	bool m_runtime_ready = false;
	std::vector<UserState> m_user_states;
	std::vector<PendingQueryOp> m_pending_query_ops;
	std::vector<PendingUpdateOp> m_pending_update_ops;

	GDKRuntime *_get_runtime() const;
	GDKXboxServices *_get_xbox_services() const;
	Signal _make_completed_signal(const Ref<GDKResult> &p_result) const;
	Signal _make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message) const;
	UserState *_find_user_state_by_xuid(uint64_t p_xbox_user_id);
	UserState *_find_user_state_by_local_id(XUserLocalId p_local_id);
	Ref<GDKAchievement> _find_cached_achievement(const UserState &p_state, const String &p_achievement_id) const;
	Array _get_cached_achievements_array(const UserState &p_state) const;
	Ref<GDKResult> _ensure_user_state(const Ref<GDKUser> &p_user, UserState **r_state);
	Ref<GDKResult> _refresh_user_cache(UserState &p_state);
	Ref<GDKResult> _refresh_single_achievement(UserState &p_state, const String &p_achievement_id);
	Ref<GDKResult> _submit_update(PendingUpdateOp &p_pending_update);
	void _complete_pending_queries(UserState &p_state);
	void _fail_pending_queries(uint64_t p_xbox_user_id, const Ref<GDKResult> &p_result);
	void _complete_pending_updates(UserState &p_state, const String &p_achievement_id);
	void _fail_pending_updates(uint64_t p_xbox_user_id, const Ref<GDKResult> &p_result);
	void _cancel_pending_query_signal(GDKPendingSignal *p_request);
	void _cancel_pending_update_signal(GDKPendingSignal *p_request);
	void _submit_waiting_updates(UserState &p_state);
	void _erase_completed_updates();
	void _erase_user_state(uint64_t p_xbox_user_id);

protected:
	static void _bind_methods();

public:
	void set_owner(GDK *p_owner);

	Ref<GDKResult> on_runtime_initialized();
	void shutdown();
	int dispatch();

	Signal query_player_achievements_async(const Ref<GDKUser> &p_user);
	Signal update_achievement_async(const Ref<GDKUser> &p_user, const String &p_achievement_id, int64_t p_percent_complete);
	Array get_cached_achievements(const Ref<GDKUser> &p_user) const;

	void on_user_removed(const Ref<GDKUser> &p_user);
};
