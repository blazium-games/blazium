/**************************************************************************/
/*  gdk_stats.h                                                           */
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
#include <string>
#include <vector>

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
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

class GDKStats : public RefCounted {
	GDCLASS(GDKStats, RefCounted);

public:
	struct StagedStat {
		String name;
		double value = 0.0;
	};

private:
	struct StagedStats {
		Ref<GDKUser> user;
		XUserLocalId local_id = {};
		std::vector<StagedStat> stats;
	};

	struct CachedStats {
		String xuid;
		Dictionary stats;
	};

	struct TrackingState {
		struct CallbackContext {
			GDKStats *stats = nullptr;
			std::atomic_bool active = true;
			std::mutex mutex;
		};

		struct CallbackToken {
			std::weak_ptr<CallbackContext> context;
		};

		Ref<GDKUser> user;
		XUserLocalId local_id = {};
		uint64_t xbox_user_id = 0;
		XblContextHandle context = nullptr;
		XblFunctionContext handler_token = {};
		bool handler_registered = false;
		std::shared_ptr<CallbackContext> callback_context;
		std::shared_ptr<CallbackToken> callback_token;
	};

	struct PendingStatChange {
		uint64_t xbox_user_id = 0;
		std::string statistic_name;
		std::string statistic_type;
		std::string value;
	};

	GDK *m_owner = nullptr;
	bool m_runtime_ready = false;
	std::vector<CachedStats> m_cached_stats;
	std::vector<StagedStats> m_staged_stats;
	std::vector<TrackingState> m_tracking_states;
	std::vector<PendingStatChange> m_pending_stat_changes;
	mutable std::mutex m_pending_stat_changes_mutex;
	std::vector<std::shared_ptr<TrackingState::CallbackToken>> m_retired_callback_tokens;

	static void CALLBACK _statistic_changed_handler(XblStatisticChangeEventArgs p_args, void *p_context);

	GDKRuntime *_get_runtime() const;
	GDKXboxServices *_get_xbox_services() const;
	Signal _make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data = Variant()) const;
	Ref<GDKResult> _ensure_ready_user(const Ref<GDKUser> &p_user) const;
	StagedStats *_find_staged_stats(XUserLocalId p_local_id);
	CachedStats *_find_cached_stats(const String &p_xuid);
	const CachedStats *_find_cached_stats(const String &p_xuid) const;
	TrackingState *_find_tracking_state(XUserLocalId p_local_id);
	void _cache_stat_value(const String &p_xuid, const String &p_stat_name, const String &p_stat_type, const String &p_value, const String &p_scid);
	void _cache_results(const XblUserStatisticsResult *p_results, size_t p_result_count);
	void _close_tracking_state(TrackingState &p_state);

public:
	Dictionary _make_query_payload(const XblUserStatisticsResult *p_results, size_t p_result_count, bool p_single_user_payload);
	void _clear_staged_stats(const Ref<GDKUser> &p_user);
	void _apply_staged_stats_to_cache(const Ref<GDKUser> &p_user, const std::vector<StagedStat> &p_stats);

protected:
	static void _bind_methods();

public:
	void set_owner(GDK *p_owner);

	Ref<GDKResult> on_runtime_initialized();
	void shutdown();
	int dispatch();

	Signal query_user_stats_async(const Ref<GDKUser> &p_user, const PackedStringArray &p_stat_names = PackedStringArray());
	Signal query_users_stats_async(const Ref<GDKUser> &p_user, const PackedStringArray &p_xuids, const PackedStringArray &p_stat_names = PackedStringArray());
	Ref<GDKResult> set_stat_integer(const Ref<GDKUser> &p_user, const String &p_stat_name, int64_t p_value);
	Ref<GDKResult> set_stat_number(const Ref<GDKUser> &p_user, const String &p_stat_name, double p_value);
	Signal flush_stats_async(const Ref<GDKUser> &p_user);
	Ref<GDKResult> track_stats(const Ref<GDKUser> &p_user, const PackedStringArray &p_stat_names);
	Ref<GDKResult> stop_tracking_stats(const Ref<GDKUser> &p_user, const PackedStringArray &p_stat_names = PackedStringArray());
	Dictionary get_cached_stats(const Ref<GDKUser> &p_user) const;

	void on_user_removed(const Ref<GDKUser> &p_user);
};
