/**************************************************************************/
/*  gdk_leaderboards.h                                                    */
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

#include "gdk_gdk_stubs.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifdef XBOX_MODULE_GDK_ENABLED
#include "gdk_windows.h"
#endif

#include <vector>

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/variant/array.h"
#include "core/variant/callable.h"
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

class GDKLeaderboardColumn : public RefCounted {
	GDCLASS(GDKLeaderboardColumn, RefCounted);

	String m_stat_name;
	String m_stat_type;

protected:
	static void _bind_methods();

public:
	String get_stat_name() const;
	String get_stat_type() const;

	void populate_from_native(const XblLeaderboardColumn &p_column);
};

class GDKLeaderboardRow : public RefCounted {
	GDCLASS(GDKLeaderboardRow, RefCounted);

	String m_gamertag;
	String m_modern_gamertag;
	String m_modern_gamertag_suffix;
	String m_unique_modern_gamertag;
	String m_xuid;
	double m_percentile = 0.0;
	int64_t m_rank = 0;
	int64_t m_global_rank = 0;
	PackedStringArray m_column_values;

protected:
	static void _bind_methods();

public:
	String get_gamertag() const;
	String get_modern_gamertag() const;
	String get_modern_gamertag_suffix() const;
	String get_unique_modern_gamertag() const;
	String get_xuid() const;
	double get_percentile() const;
	int64_t get_rank() const;
	int64_t get_global_rank() const;
	PackedStringArray get_column_values() const;

	void populate_from_native(const XblLeaderboardRow &p_row);
};

class GDKLeaderboard : public RefCounted {
	GDCLASS(GDKLeaderboard, RefCounted);

	String m_stat_name;
	String m_query_type;
	int64_t m_total_row_count = 0;
	bool m_has_next = false;
	Array m_columns;
	Array m_rows;
	Ref<GDKUser> m_user;
	std::vector<uint8_t> m_native_result_buffer;
	XblLeaderboardResult *m_native_result = nullptr;

protected:
	static void _bind_methods();

public:
	String get_stat_name() const;
	String get_query_type() const;
	int64_t get_total_row_count() const;
	bool has_next() const;
	Array get_columns() const;
	Array get_rows() const;

	Ref<GDKUser> get_user_internal() const;
	XblLeaderboardResult *get_native_result_internal() const;
	void populate_from_native(
			const String &p_stat_name,
			const String &p_query_type,
			const Ref<GDKUser> &p_user,
			std::vector<uint8_t> &&p_buffer,
			XblLeaderboardResult *p_native_result);
};

class GDKLeaderboards : public RefCounted {
	GDCLASS(GDKLeaderboards, RefCounted);

	struct CachedLeaderboard {
		String stat_name;
		Ref<GDKLeaderboard> leaderboard;
	};

	GDK *m_owner = nullptr;
	bool m_runtime_ready = false;
	std::vector<CachedLeaderboard> m_cached_leaderboards;

	GDKRuntime *_get_runtime() const;
	GDKXboxServices *_get_xbox_services() const;
	Signal _make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data = Variant()) const;
	Ref<GDKResult> _ensure_ready_user(const Ref<GDKUser> &p_user) const;
	CachedLeaderboard *_find_cached_leaderboard(const String &p_stat_name);
	Ref<GDKLeaderboard> _cache_leaderboard(const Ref<GDKLeaderboard> &p_leaderboard);
	Signal _start_leaderboard_async(
			const Ref<GDKUser> &p_user,
			const String &p_stat_name,
			int64_t p_max_items,
			XblSocialGroupType p_social_group,
			XblLeaderboardQueryType p_native_query_type,
			bool p_around_user);

protected:
	static void _bind_methods();

public:
	void set_owner(GDK *p_owner);

	Ref<GDKResult> on_runtime_initialized();
	void shutdown();

	Signal get_leaderboard_async(const Ref<GDKUser> &p_user, const String &p_stat_name, int64_t p_max_items = 25);
	Signal get_leaderboard_around_user_async(const Ref<GDKUser> &p_user, const String &p_stat_name, int64_t p_max_items = 25);
	Signal get_social_leaderboard_async(const Ref<GDKUser> &p_user, const String &p_stat_name, int64_t p_max_items = 25);
	Signal get_next_page_async(const Ref<GDKLeaderboard> &p_leaderboard);
	Ref<GDKLeaderboard> get_cached_leaderboard(const String &p_stat_name) const;

	Ref<GDKLeaderboard> cache_leaderboard_internal(const Ref<GDKLeaderboard> &p_leaderboard);
	void on_user_removed(const Ref<GDKUser> &p_user);
};
