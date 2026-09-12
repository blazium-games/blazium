/**************************************************************************/
/*  gdk_leaderboards.cpp                                                  */
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

#include "gdk_leaderboards.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "gdk_windows.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>

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

String _stat_type_to_string(XblLeaderboardStatType p_type) {
	switch (p_type) {
		case XblLeaderboardStatType::Uint64:
			return "uint64";
		case XblLeaderboardStatType::Boolean:
			return "boolean";
		case XblLeaderboardStatType::Double:
			return "double";
		case XblLeaderboardStatType::String:
			return "string";
		case XblLeaderboardStatType::Other:
		default:
			return "other";
	}
}

Ref<GDKResult> _parse_stat_name(const String &p_stat_name, String *r_stat_name) {
	if (r_stat_name == nullptr) {
		return GDKResult::error_result(E_POINTER, "invalid_output", "Statistic name output storage is unavailable.");
	}

	const String stat_name = p_stat_name.strip_edges();
	if (stat_name.is_empty()) {
		return GDKResult::error_result(E_INVALIDARG, "invalid_stat_name", "Statistic name must be a non-empty string.");
	}

	*r_stat_name = stat_name;
	return GDKResult::ok_result();
}

Ref<GDKResult> _parse_max_items(int64_t p_max_items, uint32_t *r_max_items) {
	if (r_max_items == nullptr) {
		return GDKResult::error_result(E_POINTER, "invalid_output", "Max item output storage is unavailable.");
	}
	if (p_max_items < 0 || p_max_items > static_cast<int64_t>(std::numeric_limits<uint32_t>::max())) {
		return GDKResult::error_result(E_INVALIDARG, "invalid_max_items", "max_items must fit in a non-negative 32-bit unsigned integer.");
	}

	*r_max_items = static_cast<uint32_t>(p_max_items);
	return GDKResult::ok_result();
}

String _query_type_to_string(XblLeaderboardQueryType p_query_type, bool p_around_user) {
	if (p_around_user) {
		return "around_user";
	}

	switch (p_query_type) {
		case XblLeaderboardQueryType::TitleManagedStatBackedSocial:
			return "social";
		case XblLeaderboardQueryType::TitleManagedStatBackedGlobal:
			return "global";
		case XblLeaderboardQueryType::UserStatBacked:
		default:
			return "user_stat";
	}
}

class LeaderboardAsyncContext : public GDKSignalXAsyncContext {
	GDKLeaderboards *m_leaderboards = nullptr;
	Ref<GDKUser> m_user;
	XblContextHandle m_context = nullptr;
	String m_stat_name;
	String m_query_type;
	bool m_next_page = false;

protected:
	void finalize(XAsyncBlock *p_async_block) override {
		Ref<GDKResult> result;

		if (get_runtime()->is_shutting_down() || get_pending_signal()->was_cancel_requested()) {
			result = GDKResult::cancelled("Leaderboard query cancelled.");
			get_pending_signal()->complete(result);
			return;
		}

		size_t result_size = 0;
		HRESULT result_hr = m_next_page ? XblLeaderboardResultGetNextResultSize(p_async_block, &result_size) : XblLeaderboardGetLeaderboardResultSize(p_async_block, &result_size);
		if (result_hr == E_ABORT) {
			result = GDKResult::cancelled("Leaderboard query cancelled.");
			get_pending_signal()->complete(result);
			return;
		}
		if (FAILED(result_hr)) {
			result = GDKResult::hresult_error(result_hr, "Failed to retrieve leaderboard result size.", "leaderboard_result_size_failed");
			get_pending_signal()->complete(result);
			return;
		}

		std::vector<uint8_t> buffer(result_size);
		XblLeaderboardResult *native_result = nullptr;
		size_t buffer_used = 0;
		result_hr = m_next_page ? XblLeaderboardResultGetNextResult(
										  p_async_block,
										  buffer.size(),
										  buffer.empty() ? nullptr : buffer.data(),
										  &native_result,
										  &buffer_used)
								: XblLeaderboardGetLeaderboardResult(
										  p_async_block,
										  buffer.size(),
										  buffer.empty() ? nullptr : buffer.data(),
										  &native_result,
										  &buffer_used);

		if (result_hr == E_ABORT) {
			result = GDKResult::cancelled("Leaderboard query cancelled.");
			get_pending_signal()->complete(result);
			return;
		}
		if (FAILED(result_hr)) {
			result = GDKResult::hresult_error(result_hr, "Failed to retrieve leaderboard results.", "leaderboard_results_failed");
			get_pending_signal()->complete(result);
			return;
		}

		Ref<GDKLeaderboard> leaderboard;
		leaderboard.instantiate();
		leaderboard->populate_from_native(m_stat_name, m_query_type, m_user, std::move(buffer), native_result);
		m_leaderboards->cache_leaderboard_internal(leaderboard);

		m_leaderboards->emit_signal("leaderboard_updated", m_stat_name, leaderboard);
		get_pending_signal()->complete(GDKResult::ok_result(leaderboard));
	}

public:
	LeaderboardAsyncContext(
			GDKLeaderboards *p_leaderboards,
			const Ref<GDKUser> &p_user,
			GDKRuntime *p_runtime,
			const Ref<GDKPendingSignal> &p_pending_signal,
			XblContextHandle p_context,
			const String &p_stat_name,
			const String &p_query_type,
			bool p_next_page) :
			GDKSignalXAsyncContext(p_runtime, p_pending_signal),
			m_leaderboards(p_leaderboards),
			m_user(p_user),
			m_context(p_context),
			m_stat_name(p_stat_name),
			m_query_type(p_query_type),
			m_next_page(p_next_page) {}

	~LeaderboardAsyncContext() override {
		if (m_context != nullptr) {
			XblContextCloseHandle(m_context);
			m_context = nullptr;
		}
	}

	XblContextHandle get_context() const {
		return m_context;
	}
};

class LeaderboardQueryAsyncContext final : public LeaderboardAsyncContext {
	CharString m_stat_name_utf8;
	XblLeaderboardQuery m_query = {};

public:
	LeaderboardQueryAsyncContext(
			GDKLeaderboards *p_leaderboards,
			const Ref<GDKUser> &p_user,
			GDKRuntime *p_runtime,
			const Ref<GDKPendingSignal> &p_pending_signal,
			XblContextHandle p_context,
			const String &p_stat_name,
			const String &p_query_type,
			const String &p_scid,
			uint64_t p_xbox_user_id,
			uint32_t p_max_items,
			XblSocialGroupType p_social_group,
			XblLeaderboardQueryType p_native_query_type,
			bool p_around_user) :
			LeaderboardAsyncContext(p_leaderboards, p_user, p_runtime, p_pending_signal, p_context, p_stat_name, p_query_type, false),
			m_stat_name_utf8(p_stat_name.utf8()) {
		const CharString scid_utf8 = p_scid.utf8();
		std::snprintf(m_query.scid, sizeof(m_query.scid), "%s", scid_utf8.get_data());
		m_query.xboxUserId = p_native_query_type == XblLeaderboardQueryType::TitleManagedStatBackedGlobal && !p_around_user ? 0 : p_xbox_user_id;
		m_query.leaderboardName = nullptr;
		m_query.statName = m_stat_name_utf8.get_data();
		m_query.socialGroup = p_social_group;
		m_query.additionalColumnleaderboardNames = nullptr;
		m_query.additionalColumnleaderboardNamesCount = 0;
		m_query.order = XblLeaderboardSortOrder::Descending;
		m_query.maxItems = p_max_items;
		m_query.skipToXboxUserId = p_around_user ? p_xbox_user_id : 0;
		m_query.skipResultToRank = 0;
		m_query.continuationToken = nullptr;
		m_query.queryType = p_native_query_type;
	}

	XblLeaderboardQuery get_query() const {
		return m_query;
	}
};

class LeaderboardNextPageAsyncContext final : public LeaderboardAsyncContext {
	Ref<GDKLeaderboard> m_previous_leaderboard;

public:
	LeaderboardNextPageAsyncContext(
			GDKLeaderboards *p_leaderboards,
			const Ref<GDKLeaderboard> &p_previous_leaderboard,
			GDKRuntime *p_runtime,
			const Ref<GDKPendingSignal> &p_pending_signal,
			XblContextHandle p_context) :
			LeaderboardAsyncContext(
					p_leaderboards,
					p_previous_leaderboard->get_user_internal(),
					p_runtime,
					p_pending_signal,
					p_context,
					p_previous_leaderboard->get_stat_name(),
					p_previous_leaderboard->get_query_type(),
					true),
			m_previous_leaderboard(p_previous_leaderboard) {}

	XblLeaderboardResult *get_previous_native_result() const {
		return m_previous_leaderboard->get_native_result_internal();
	}
};

} //namespace

#endif

void GDKLeaderboardColumn::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_stat_name"), &GDKLeaderboardColumn::get_stat_name);
	ClassDB::bind_method(D_METHOD("get_stat_type"), &GDKLeaderboardColumn::get_stat_type);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "stat_name"), "", "get_stat_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "stat_type"), "", "get_stat_type");
}

String GDKLeaderboardColumn::get_stat_name() const {
	return m_stat_name;
}

String GDKLeaderboardColumn::get_stat_type() const {
	return m_stat_type;
}

#ifdef XBOX_MODULE_GDK_ENABLED
void GDKLeaderboardColumn::populate_from_native(const XblLeaderboardColumn &p_column) {
	m_stat_name = _utf8_or_empty(p_column.statName);
	m_stat_type = _stat_type_to_string(p_column.statType);
}
#else
void GDKLeaderboardColumn::populate_from_native(const XblLeaderboardColumn &p_column) {
}
#endif

void GDKLeaderboardRow::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_gamertag"), &GDKLeaderboardRow::get_gamertag);
	ClassDB::bind_method(D_METHOD("get_modern_gamertag"), &GDKLeaderboardRow::get_modern_gamertag);
	ClassDB::bind_method(D_METHOD("get_modern_gamertag_suffix"), &GDKLeaderboardRow::get_modern_gamertag_suffix);
	ClassDB::bind_method(D_METHOD("get_unique_modern_gamertag"), &GDKLeaderboardRow::get_unique_modern_gamertag);
	ClassDB::bind_method(D_METHOD("get_xuid"), &GDKLeaderboardRow::get_xuid);
	ClassDB::bind_method(D_METHOD("get_percentile"), &GDKLeaderboardRow::get_percentile);
	ClassDB::bind_method(D_METHOD("get_rank"), &GDKLeaderboardRow::get_rank);
	ClassDB::bind_method(D_METHOD("get_global_rank"), &GDKLeaderboardRow::get_global_rank);
	ClassDB::bind_method(D_METHOD("get_column_values"), &GDKLeaderboardRow::get_column_values);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "gamertag"), "", "get_gamertag");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "modern_gamertag"), "", "get_modern_gamertag");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "modern_gamertag_suffix"), "", "get_modern_gamertag_suffix");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "unique_modern_gamertag"), "", "get_unique_modern_gamertag");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "xuid"), "", "get_xuid");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "percentile"), "", "get_percentile");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "rank"), "", "get_rank");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "global_rank"), "", "get_global_rank");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "column_values"), "", "get_column_values");
}

String GDKLeaderboardRow::get_gamertag() const {
	return m_gamertag;
}

String GDKLeaderboardRow::get_modern_gamertag() const {
	return m_modern_gamertag;
}

String GDKLeaderboardRow::get_modern_gamertag_suffix() const {
	return m_modern_gamertag_suffix;
}

String GDKLeaderboardRow::get_unique_modern_gamertag() const {
	return m_unique_modern_gamertag;
}

String GDKLeaderboardRow::get_xuid() const {
	return m_xuid;
}

double GDKLeaderboardRow::get_percentile() const {
	return m_percentile;
}

int64_t GDKLeaderboardRow::get_rank() const {
	return m_rank;
}

int64_t GDKLeaderboardRow::get_global_rank() const {
	return m_global_rank;
}

PackedStringArray GDKLeaderboardRow::get_column_values() const {
	return m_column_values;
}

#ifdef XBOX_MODULE_GDK_ENABLED
void GDKLeaderboardRow::populate_from_native(const XblLeaderboardRow &p_row) {
	m_gamertag = _utf8_or_empty(p_row.gamertag);
	m_modern_gamertag = _utf8_or_empty(p_row.modernGamertag);
	m_modern_gamertag_suffix = _utf8_or_empty(p_row.modernGamertagSuffix);
	m_unique_modern_gamertag = _utf8_or_empty(p_row.uniqueModernGamertag);
	m_xuid = String::num_uint64(p_row.xboxUserId);
	m_percentile = p_row.percentile;
	m_rank = p_row.rank;
	m_global_rank = p_row.globalRank;
	m_column_values.clear();
	if (p_row.columnValues != nullptr) {
		for (size_t i = 0; i < p_row.columnValuesCount; ++i) {
			m_column_values.push_back(_utf8_or_empty(p_row.columnValues[i]));
		}
	}
}
#else
void GDKLeaderboardRow::populate_from_native(const XblLeaderboardRow &p_row) {
}
#endif

void GDKLeaderboard::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_stat_name"), &GDKLeaderboard::get_stat_name);
	ClassDB::bind_method(D_METHOD("get_query_type"), &GDKLeaderboard::get_query_type);
	ClassDB::bind_method(D_METHOD("get_total_row_count"), &GDKLeaderboard::get_total_row_count);
	ClassDB::bind_method(D_METHOD("has_next"), &GDKLeaderboard::has_next);
	ClassDB::bind_method(D_METHOD("get_columns"), &GDKLeaderboard::get_columns);
	ClassDB::bind_method(D_METHOD("get_rows"), &GDKLeaderboard::get_rows);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "stat_name"), "", "get_stat_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "query_type"), "", "get_query_type");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "total_row_count"), "", "get_total_row_count");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "has_next"), "", "has_next");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "columns"), "", "get_columns");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "rows"), "", "get_rows");
}

String GDKLeaderboard::get_stat_name() const {
	return m_stat_name;
}

String GDKLeaderboard::get_query_type() const {
	return m_query_type;
}

int64_t GDKLeaderboard::get_total_row_count() const {
	return m_total_row_count;
}

bool GDKLeaderboard::has_next() const {
	return m_has_next;
}

Array GDKLeaderboard::get_columns() const {
	return m_columns;
}

Array GDKLeaderboard::get_rows() const {
	return m_rows;
}

Ref<GDKUser> GDKLeaderboard::get_user_internal() const {
	return m_user;
}

XblLeaderboardResult *GDKLeaderboard::get_native_result_internal() const {
	return m_native_result;
}

#ifdef XBOX_MODULE_GDK_ENABLED
void GDKLeaderboard::populate_from_native(
		const String &p_stat_name,
		const String &p_query_type,
		const Ref<GDKUser> &p_user,
		std::vector<uint8_t> &&p_buffer,
		XblLeaderboardResult *p_native_result) {
	m_stat_name = p_stat_name;
	m_query_type = p_query_type;
	m_user = p_user;
	m_native_result_buffer = std::move(p_buffer);
	m_native_result = p_native_result;

	m_total_row_count = p_native_result == nullptr ? 0 : p_native_result->totalRowCount;
	m_has_next = p_native_result != nullptr && p_native_result->hasNext;
	m_columns.clear();
	m_rows.clear();

	if (p_native_result == nullptr) {
		return;
	}

	if (p_native_result->columns != nullptr) {
		for (size_t i = 0; i < p_native_result->columnsCount; ++i) {
			Ref<GDKLeaderboardColumn> column;
			column.instantiate();
			column->populate_from_native(p_native_result->columns[i]);
			m_columns.push_back(column);
		}
	}

	if (p_native_result->rows != nullptr) {
		for (size_t i = 0; i < p_native_result->rowsCount; ++i) {
			Ref<GDKLeaderboardRow> row;
			row.instantiate();
			row->populate_from_native(p_native_result->rows[i]);
			m_rows.push_back(row);
		}
	}
}
#else
void GDKLeaderboard::populate_from_native(
		const String &p_stat_name,
		const String &p_query_type,
		const Ref<GDKUser> &p_user,
		std::vector<uint8_t> &&p_buffer,
		XblLeaderboardResult *p_native_result) {
	m_stat_name = p_stat_name;
	m_query_type = p_query_type;
	m_user = p_user;
	m_native_result_buffer = std::move(p_buffer);
	m_native_result = p_native_result;
	m_total_row_count = 0;
	m_has_next = false;
	m_columns.clear();
	m_rows.clear();
}
#endif

void GDKLeaderboards::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_leaderboard_async", "user", "stat_name", "max_items"), &GDKLeaderboards::get_leaderboard_async, DEFVAL(25));
	ClassDB::bind_method(D_METHOD("get_leaderboard_around_user_async", "user", "stat_name", "max_items"), &GDKLeaderboards::get_leaderboard_around_user_async, DEFVAL(25));
	ClassDB::bind_method(D_METHOD("get_social_leaderboard_async", "user", "stat_name", "max_items"), &GDKLeaderboards::get_social_leaderboard_async, DEFVAL(25));
	ClassDB::bind_method(D_METHOD("get_next_page_async", "leaderboard"), &GDKLeaderboards::get_next_page_async);
	ClassDB::bind_method(D_METHOD("get_cached_leaderboard", "stat_name"), &GDKLeaderboards::get_cached_leaderboard);

	ADD_SIGNAL(MethodInfo("leaderboard_updated", PropertyInfo(Variant::STRING, "stat_name"), PropertyInfo(Variant::OBJECT, "leaderboard", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "GDKLeaderboard")));
}

#ifdef XBOX_MODULE_GDK_ENABLED
void GDKLeaderboards::set_owner(GDK *p_owner) {
	m_owner = p_owner;
}

GDKRuntime *GDKLeaderboards::_get_runtime() const {
	return m_owner == nullptr ? nullptr : m_owner->get_runtime();
}

GDKXboxServices *GDKLeaderboards::_get_xbox_services() const {
	return m_owner == nullptr ? nullptr : m_owner->get_xbox_services();
}

Signal GDKLeaderboards::_make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data) const {
	GDKRuntime *runtime = _get_runtime();
	ERR_FAIL_NULL_V(runtime, Signal());
	return runtime->make_error_signal(p_hresult, p_code, p_message, p_data);
}

Ref<GDKResult> GDKLeaderboards::_ensure_ready_user(const Ref<GDKUser> &p_user) const {
	if (!m_runtime_ready) {
		return GDKResult::error_result(E_FAIL, "runtime_unavailable", "GDK runtime is not initialized.");
	}
	if (!p_user.is_valid() || p_user->get_handle() == nullptr || !p_user->is_signed_in()) {
		return GDKResult::error_result(E_INVALIDARG, "invalid_user", "A signed-in GDKUser is required.");
	}
	return GDKResult::ok_result();
}

GDKLeaderboards::CachedLeaderboard *GDKLeaderboards::_find_cached_leaderboard(const String &p_stat_name) {
	for (CachedLeaderboard &cached : m_cached_leaderboards) {
		if (cached.stat_name == p_stat_name) {
			return &cached;
		}
	}
	return nullptr;
}

Ref<GDKLeaderboard> GDKLeaderboards::_cache_leaderboard(const Ref<GDKLeaderboard> &p_leaderboard) {
	if (!p_leaderboard.is_valid() || p_leaderboard->get_stat_name().is_empty()) {
		return p_leaderboard;
	}

	CachedLeaderboard *cached = _find_cached_leaderboard(p_leaderboard->get_stat_name());
	if (cached == nullptr) {
		CachedLeaderboard new_cached;
		new_cached.stat_name = p_leaderboard->get_stat_name();
		new_cached.leaderboard = p_leaderboard;
		m_cached_leaderboards.push_back(new_cached);
	} else {
		cached->leaderboard = p_leaderboard;
	}

	return p_leaderboard;
}

Ref<GDKResult> GDKLeaderboards::on_runtime_initialized() {
	m_runtime_ready = true;
	return GDKResult::ok_result();
}

void GDKLeaderboards::shutdown() {
	m_cached_leaderboards.clear();
	m_runtime_ready = false;
}

Signal GDKLeaderboards::_start_leaderboard_async(
		const Ref<GDKUser> &p_user,
		const String &p_stat_name,
		int64_t p_max_items,
		XblSocialGroupType p_social_group,
		XblLeaderboardQueryType p_native_query_type,
		bool p_around_user) {
	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr) {
		return Signal();
	}

	Ref<GDKResult> validation = _ensure_ready_user(p_user);
	if (!validation->is_ok()) {
		return _make_error_signal(static_cast<HRESULT>(validation->get_hresult()), validation->get_code(), validation->get_message());
	}

	String stat_name;
	Ref<GDKResult> stat_name_result = _parse_stat_name(p_stat_name, &stat_name);
	if (!stat_name_result->is_ok()) {
		return _make_error_signal(static_cast<HRESULT>(stat_name_result->get_hresult()), stat_name_result->get_code(), stat_name_result->get_message());
	}

	uint32_t max_items = 0;
	Ref<GDKResult> max_items_result = _parse_max_items(p_max_items, &max_items);
	if (!max_items_result->is_ok()) {
		return _make_error_signal(static_cast<HRESULT>(max_items_result->get_hresult()), max_items_result->get_code(), max_items_result->get_message());
	}

	GDKXboxServices *xbox_services = _get_xbox_services();
	if (xbox_services == nullptr || !xbox_services->is_initialized()) {
		return _make_error_signal(E_FAIL, "xbox_services_uninitialized", "Xbox services are not initialized.");
	}

	XblContextHandle context = nullptr;
	uint64_t xbox_user_id = 0;
	HRESULT hr = xbox_services->duplicate_context_for_user(p_user, &context, &xbox_user_id);
	if (FAILED(hr)) {
		return _make_error_signal(hr, "xbox_context_unavailable", "Failed to create an Xbox services context for the user.");
	}

	Ref<GDKPendingSignal> pending_signal = runtime->make_pending_signal();
	LeaderboardQueryAsyncContext *async_context = new LeaderboardQueryAsyncContext(
			this,
			p_user,
			runtime,
			pending_signal,
			context,
			stat_name,
			_query_type_to_string(p_native_query_type, p_around_user),
			xbox_services->get_scid(),
			xbox_user_id,
			max_items,
			p_social_group,
			p_native_query_type,
			p_around_user);
	async_context->bind_cancel_handler();

	hr = XblLeaderboardGetLeaderboardAsync(
			async_context->get_context(),
			async_context->get_query(),
			async_context->get_async_block());
	if (FAILED(hr)) {
		async_context->clear_cancel_handler();
		delete async_context;
		return _make_error_signal(hr, "leaderboard_query_start_failed", "Failed to start leaderboard query.");
	}

	return pending_signal->get_completed_signal();
}

Signal GDKLeaderboards::get_leaderboard_async(const Ref<GDKUser> &p_user, const String &p_stat_name, int64_t p_max_items) {
	return _start_leaderboard_async(p_user, p_stat_name, p_max_items, XblSocialGroupType::None, XblLeaderboardQueryType::TitleManagedStatBackedGlobal, false);
}

Signal GDKLeaderboards::get_leaderboard_around_user_async(const Ref<GDKUser> &p_user, const String &p_stat_name, int64_t p_max_items) {
	return _start_leaderboard_async(p_user, p_stat_name, p_max_items, XblSocialGroupType::None, XblLeaderboardQueryType::TitleManagedStatBackedGlobal, true);
}

Signal GDKLeaderboards::get_social_leaderboard_async(const Ref<GDKUser> &p_user, const String &p_stat_name, int64_t p_max_items) {
	return _start_leaderboard_async(p_user, p_stat_name, p_max_items, XblSocialGroupType::People, XblLeaderboardQueryType::TitleManagedStatBackedSocial, false);
}

Signal GDKLeaderboards::get_next_page_async(const Ref<GDKLeaderboard> &p_leaderboard) {
	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr) {
		return Signal();
	}
	if (!m_runtime_ready) {
		return _make_error_signal(E_FAIL, "runtime_unavailable", "GDK runtime is not initialized.");
	}
	if (!p_leaderboard.is_valid() || p_leaderboard->get_native_result_internal() == nullptr) {
		return _make_error_signal(E_INVALIDARG, "invalid_leaderboard", "A leaderboard returned by this service is required.");
	}
	if (!p_leaderboard->has_next()) {
		return _make_error_signal(E_INVALIDARG, "no_next_page", "Leaderboard has no next page.");
	}

	Ref<GDKUser> user = p_leaderboard->get_user_internal();
	Ref<GDKResult> validation = _ensure_ready_user(user);
	if (!validation->is_ok()) {
		return _make_error_signal(static_cast<HRESULT>(validation->get_hresult()), validation->get_code(), validation->get_message());
	}

	GDKXboxServices *xbox_services = _get_xbox_services();
	if (xbox_services == nullptr || !xbox_services->is_initialized()) {
		return _make_error_signal(E_FAIL, "xbox_services_uninitialized", "Xbox services are not initialized.");
	}

	XblContextHandle context = nullptr;
	HRESULT hr = xbox_services->duplicate_context_for_user(user, &context);
	if (FAILED(hr)) {
		return _make_error_signal(hr, "xbox_context_unavailable", "Failed to create an Xbox services context for the user.");
	}

	Ref<GDKPendingSignal> pending_signal = runtime->make_pending_signal();
	LeaderboardNextPageAsyncContext *async_context = new LeaderboardNextPageAsyncContext(this, p_leaderboard, runtime, pending_signal, context);
	async_context->bind_cancel_handler();

	hr = XblLeaderboardResultGetNextAsync(
			async_context->get_context(),
			async_context->get_previous_native_result(),
			0,
			async_context->get_async_block());
	if (FAILED(hr)) {
		async_context->clear_cancel_handler();
		delete async_context;
		return _make_error_signal(hr, "leaderboard_next_page_start_failed", "Failed to start leaderboard next-page query.");
	}

	return pending_signal->get_completed_signal();
}

Ref<GDKLeaderboard> GDKLeaderboards::get_cached_leaderboard(const String &p_stat_name) const {
	const String stat_name = p_stat_name.strip_edges();
	if (stat_name.is_empty()) {
		return Ref<GDKLeaderboard>();
	}

	for (const CachedLeaderboard &cached : m_cached_leaderboards) {
		if (cached.stat_name == stat_name) {
			return cached.leaderboard;
		}
	}
	return Ref<GDKLeaderboard>();
}

Ref<GDKLeaderboard> GDKLeaderboards::cache_leaderboard_internal(const Ref<GDKLeaderboard> &p_leaderboard) {
	return _cache_leaderboard(p_leaderboard);
}

void GDKLeaderboards::on_user_removed(const Ref<GDKUser> &p_user) {
	if (!p_user.is_valid()) {
		return;
	}

	const int64_t local_id = p_user->get_local_id();
	m_cached_leaderboards.erase(
			std::remove_if(
					m_cached_leaderboards.begin(),
					m_cached_leaderboards.end(),
					[local_id](const CachedLeaderboard &cached) {
						return cached.leaderboard.is_valid() &&
								cached.leaderboard->get_user_internal().is_valid() &&
								cached.leaderboard->get_user_internal()->get_local_id() == local_id;
					}),
			m_cached_leaderboards.end());
}

#else
void GDKLeaderboards::set_owner(GDK *p_owner) {}

Signal GDKLeaderboards::_make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data) const {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Ref<GDKResult> GDKLeaderboards::_ensure_ready_user(const Ref<GDKUser> &p_user) const {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Ref<GDKLeaderboard> GDKLeaderboards::_cache_leaderboard(const Ref<GDKLeaderboard> &p_leaderboard) {
	return Ref<GDKLeaderboard>();
}

Ref<GDKResult> GDKLeaderboards::on_runtime_initialized() {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

void GDKLeaderboards::shutdown() {}

Signal GDKLeaderboards::_start_leaderboard_async(
		const Ref<GDKUser> &p_user,
		const String &p_stat_name,
		int64_t p_max_items,
		XblSocialGroupType p_social_group,
		XblLeaderboardQueryType p_native_query_type,
		bool p_around_user) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKLeaderboards::get_leaderboard_async(const Ref<GDKUser> &p_user, const String &p_stat_name, int64_t p_max_items) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKLeaderboards::get_leaderboard_around_user_async(const Ref<GDKUser> &p_user, const String &p_stat_name, int64_t p_max_items) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKLeaderboards::get_social_leaderboard_async(const Ref<GDKUser> &p_user, const String &p_stat_name, int64_t p_max_items) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKLeaderboards::get_next_page_async(const Ref<GDKLeaderboard> &p_leaderboard) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Ref<GDKLeaderboard> GDKLeaderboards::get_cached_leaderboard(const String &p_stat_name) const {
	return Ref<GDKLeaderboard>();
}

Ref<GDKLeaderboard> GDKLeaderboards::cache_leaderboard_internal(const Ref<GDKLeaderboard> &p_leaderboard) {
	return Ref<GDKLeaderboard>();
}

void GDKLeaderboards::on_user_removed(const Ref<GDKUser> &p_user) {}
#endif
