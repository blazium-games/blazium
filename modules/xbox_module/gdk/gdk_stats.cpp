/**************************************************************************/
/*  gdk_stats.cpp                                                         */
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

#include "gdk_stats.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "gdk_windows.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>

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

Ref<GDKResult> _parse_stat_names(
		const PackedStringArray &p_stat_names,
		std::vector<String> *r_stat_names,
		std::vector<CharString> *r_stat_name_utf8,
		std::vector<const char *> *r_stat_name_ptrs) {
	if (r_stat_names == nullptr || r_stat_name_utf8 == nullptr || r_stat_name_ptrs == nullptr) {
		return GDKResult::error_result(E_POINTER, "invalid_output", "Statistic name output storage is unavailable.");
	}

	r_stat_names->clear();
	r_stat_name_utf8->clear();
	r_stat_name_ptrs->clear();

	for (int64_t i = 0; i < p_stat_names.size(); ++i) {
		const String stat_name = String(p_stat_names[i]).strip_edges();
		if (stat_name.is_empty()) {
			return GDKResult::error_result(E_INVALIDARG, "invalid_stat_name", "Statistic names must be non-empty strings.");
		}
		if (std::find(r_stat_names->begin(), r_stat_names->end(), stat_name) == r_stat_names->end()) {
			r_stat_names->push_back(stat_name);
		}
	}

	if (r_stat_names->empty()) {
		return GDKResult::error_result(E_INVALIDARG, "invalid_stat_names", "At least one statistic name is required.");
	}

	r_stat_name_utf8->reserve(r_stat_names->size());
	r_stat_name_ptrs->reserve(r_stat_names->size());
	for (const String &stat_name : *r_stat_names) {
		r_stat_name_utf8->push_back(stat_name.utf8());
	}
	for (const CharString &stat_name_utf8 : *r_stat_name_utf8) {
		r_stat_name_ptrs->push_back(stat_name_utf8.get_data());
	}

	return GDKResult::ok_result();
}

Dictionary _make_stat_dictionary(const XblStatistic &p_statistic, const String &p_scid) {
	Dictionary stat;
	stat["name"] = _utf8_or_empty(p_statistic.statisticName);
	stat["type"] = _utf8_or_empty(p_statistic.statisticType);
	stat["value"] = _utf8_or_empty(p_statistic.value);
	stat["service_configuration_id"] = p_scid;
	return stat;
}

Dictionary _make_stats_dictionary(const XblUserStatisticsResult &p_result) {
	Dictionary stats;
	if (p_result.serviceConfigStatistics == nullptr) {
		return stats;
	}

	for (uint32_t scid_index = 0; scid_index < p_result.serviceConfigStatisticsCount; ++scid_index) {
		const XblServiceConfigurationStatistic &service_config = p_result.serviceConfigStatistics[scid_index];
		if (service_config.statistics == nullptr) {
			continue;
		}
		const String scid = _utf8_or_empty(service_config.serviceConfigurationId);
		for (uint32_t stat_index = 0; stat_index < service_config.statisticsCount; ++stat_index) {
			const XblStatistic &statistic = service_config.statistics[stat_index];
			const String stat_name = _utf8_or_empty(statistic.statisticName);
			if (!stat_name.is_empty()) {
				stats[stat_name] = _make_stat_dictionary(statistic, scid);
			}
		}
	}
	return stats;
}

class QueryStatsAsyncContext final : public GDKSignalXAsyncContext {
	GDKStats *m_stats = nullptr;
	Ref<GDKUser> m_user;
	XblContextHandle m_context = nullptr;
	CharString m_scid_utf8;
	std::vector<CharString> m_stat_name_utf8;
	std::vector<const char *> m_stat_name_ptrs;
	uint64_t m_xbox_user_id = 0;
	std::vector<uint64_t> m_xuids;
	bool m_single_user_payload = true;
	bool m_multiple_user_result = false;

protected:
	void finalize(XAsyncBlock *p_async_block) override {
		Ref<GDKResult> result;

		if (get_runtime()->is_shutting_down() || get_pending_signal()->was_cancel_requested()) {
			result = GDKResult::cancelled("Statistic query cancelled.");
			get_pending_signal()->complete(result);
			return;
		}

		size_t result_size = 0;
		HRESULT result_hr = m_multiple_user_result ? XblUserStatisticsGetMultipleUserStatisticsResultSize(p_async_block, &result_size) : XblUserStatisticsGetSingleUserStatisticsResultSize(p_async_block, &result_size);
		if (result_hr == E_ABORT) {
			result = GDKResult::cancelled("Statistic query cancelled.");
			get_pending_signal()->complete(result);
			return;
		}
		if (FAILED(result_hr)) {
			result = GDKResult::hresult_error(result_hr, "Failed to retrieve statistic query result size.", "stats_result_size_failed");
			get_pending_signal()->complete(result);
			return;
		}

		std::vector<uint8_t> buffer(result_size);
		XblUserStatisticsResult *results = nullptr;
		size_t results_count = 0;
		size_t buffer_used = 0;
		if (m_multiple_user_result) {
			result_hr = XblUserStatisticsGetMultipleUserStatisticsResult(
					p_async_block,
					buffer.size(),
					buffer.empty() ? nullptr : buffer.data(),
					&results,
					&results_count,
					&buffer_used);
		} else {
			result_hr = XblUserStatisticsGetSingleUserStatisticsResult(
					p_async_block,
					buffer.size(),
					buffer.empty() ? nullptr : buffer.data(),
					&results,
					&buffer_used);
			results_count = results != nullptr ? 1 : 0;
		}

		if (result_hr == E_ABORT) {
			result = GDKResult::cancelled("Statistic query cancelled.");
			get_pending_signal()->complete(result);
			return;
		}
		if (FAILED(result_hr)) {
			result = GDKResult::hresult_error(result_hr, "Failed to retrieve statistic query results.", "stats_results_failed");
			get_pending_signal()->complete(result);
			return;
		}

		Dictionary payload = m_stats->_make_query_payload(results, results_count, m_single_user_payload);
		if (m_single_user_payload) {
			m_stats->emit_signal("stats_updated", m_user, m_stats->get_cached_stats(m_user));
		}
		get_pending_signal()->complete(GDKResult::ok_result(payload));
	}

public:
	QueryStatsAsyncContext(
			GDKStats *p_stats,
			const Ref<GDKUser> &p_user,
			GDKRuntime *p_runtime,
			const Ref<GDKPendingSignal> &p_pending_signal,
			XblContextHandle p_context,
			const String &p_scid,
			const std::vector<String> &p_stat_names,
			uint64_t p_xbox_user_id,
			const std::vector<uint64_t> &p_xuids,
			bool p_single_user_payload,
			bool p_multiple_user_result) :
			GDKSignalXAsyncContext(p_runtime, p_pending_signal),
			m_stats(p_stats),
			m_user(p_user),
			m_context(p_context),
			m_scid_utf8(p_scid.utf8()),
			m_xbox_user_id(p_xbox_user_id),
			m_xuids(p_xuids),
			m_single_user_payload(p_single_user_payload),
			m_multiple_user_result(p_multiple_user_result) {
		m_stat_name_utf8.reserve(p_stat_names.size());
		m_stat_name_ptrs.reserve(p_stat_names.size());
		for (const String &stat_name : p_stat_names) {
			m_stat_name_utf8.push_back(stat_name.utf8());
		}
		for (const CharString &stat_name_utf8 : m_stat_name_utf8) {
			m_stat_name_ptrs.push_back(stat_name_utf8.get_data());
		}
	}

	~QueryStatsAsyncContext() override {
		if (m_context != nullptr) {
			XblContextCloseHandle(m_context);
			m_context = nullptr;
		}
	}

	XblContextHandle get_context() const {
		return m_context;
	}

	const char *get_scid() const {
		return m_scid_utf8.get_data();
	}

	const char **get_stat_name_ptrs() {
		return m_stat_name_ptrs.empty() ? nullptr : m_stat_name_ptrs.data();
	}

	size_t get_stat_name_count() const {
		return m_stat_name_ptrs.size();
	}

	uint64_t get_xbox_user_id() const {
		return m_xbox_user_id;
	}

	uint64_t *get_xuids() {
		return m_xuids.empty() ? nullptr : m_xuids.data();
	}

	size_t get_xuid_count() const {
		return m_xuids.size();
	}
};

class FlushStatsAsyncContext final : public GDKSignalXAsyncContext {
	GDKStats *m_stats = nullptr;
	Ref<GDKUser> m_user;
	XblContextHandle m_context = nullptr;
	std::vector<GDKStats::StagedStat> m_staged_stats;
	std::vector<CharString> m_stat_name_utf8;
	std::vector<XblTitleManagedStatistic> m_native_stats;

protected:
	void finalize(XAsyncBlock *p_async_block) override {
		Ref<GDKResult> result;

		if (get_runtime()->is_shutting_down() || get_pending_signal()->was_cancel_requested()) {
			result = GDKResult::cancelled("Statistic flush cancelled.");
			get_pending_signal()->complete(result);
			return;
		}

		HRESULT result_hr = XAsyncGetStatus(p_async_block, false);
		if (result_hr == E_ABORT) {
			result = GDKResult::cancelled("Statistic flush cancelled.");
			get_pending_signal()->complete(result);
			return;
		}
		if (FAILED(result_hr)) {
			result = GDKResult::hresult_error(result_hr, "Failed to flush title-managed statistics.", "stats_flush_failed");
			get_pending_signal()->complete(result);
			return;
		}

		const String xuid = m_user.is_valid() ? m_user->get_xuid() : String();
		m_stats->_apply_staged_stats_to_cache(m_user, m_staged_stats);
		m_stats->_clear_staged_stats(m_user);

		Dictionary data;
		data["xuid"] = xuid;
		data["count"] = static_cast<int64_t>(m_staged_stats.size());
		result = GDKResult::ok_result(data);
		m_stats->emit_signal("stats_flushed", m_user, result);
		get_pending_signal()->complete(result);
	}

public:
	FlushStatsAsyncContext(
			GDKStats *p_stats,
			const Ref<GDKUser> &p_user,
			GDKRuntime *p_runtime,
			const Ref<GDKPendingSignal> &p_pending_signal,
			XblContextHandle p_context,
			const std::vector<GDKStats::StagedStat> &p_staged_stats) :
			GDKSignalXAsyncContext(p_runtime, p_pending_signal),
			m_stats(p_stats),
			m_user(p_user),
			m_context(p_context),
			m_staged_stats(p_staged_stats) {
		m_stat_name_utf8.reserve(m_staged_stats.size());
		m_native_stats.reserve(m_staged_stats.size());

		for (const GDKStats::StagedStat &staged_stat : m_staged_stats) {
			m_stat_name_utf8.push_back(staged_stat.name.utf8());
		}
		for (size_t i = 0; i < m_staged_stats.size(); ++i) {
			XblTitleManagedStatistic native_stat = {};
			native_stat.statisticName = m_stat_name_utf8[i].get_data();
			native_stat.statisticType = XblTitleManagedStatType::Number;
			native_stat.numberValue = m_staged_stats[i].value;
			native_stat.stringValue = nullptr;
			m_native_stats.push_back(native_stat);
		}
	}

	~FlushStatsAsyncContext() override {
		if (m_context != nullptr) {
			XblContextCloseHandle(m_context);
			m_context = nullptr;
		}
	}

	XblContextHandle get_context() const {
		return m_context;
	}

	const XblTitleManagedStatistic *get_native_stats() const {
		return m_native_stats.empty() ? nullptr : m_native_stats.data();
	}

	size_t get_native_stats_count() const {
		return m_native_stats.size();
	}
};

} //namespace

#endif

void GDKStats::_bind_methods() {
	ClassDB::bind_method(D_METHOD("query_user_stats_async", "user", "stat_names"), &GDKStats::query_user_stats_async, DEFVAL(PackedStringArray()));
	ClassDB::bind_method(D_METHOD("query_users_stats_async", "user", "xuids", "stat_names"), &GDKStats::query_users_stats_async, DEFVAL(PackedStringArray()));
	ClassDB::bind_method(D_METHOD("set_stat_integer", "user", "stat_name", "value"), &GDKStats::set_stat_integer);
	ClassDB::bind_method(D_METHOD("set_stat_number", "user", "stat_name", "value"), &GDKStats::set_stat_number);
	ClassDB::bind_method(D_METHOD("flush_stats_async", "user"), &GDKStats::flush_stats_async);
	ClassDB::bind_method(D_METHOD("track_stats", "user", "stat_names"), &GDKStats::track_stats);
	ClassDB::bind_method(D_METHOD("stop_tracking_stats", "user", "stat_names"), &GDKStats::stop_tracking_stats, DEFVAL(PackedStringArray()));
	ClassDB::bind_method(D_METHOD("get_cached_stats", "user"), &GDKStats::get_cached_stats);

	ADD_SIGNAL(MethodInfo("stats_updated", PropertyInfo(Variant::OBJECT, "user", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "GDKUser"), PropertyInfo(Variant::DICTIONARY, "stats")));
	ADD_SIGNAL(MethodInfo("stat_changed", PropertyInfo(Variant::OBJECT, "user", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "GDKUser"), PropertyInfo(Variant::STRING, "stat_name"), PropertyInfo(Variant::NIL, "value")));
	ADD_SIGNAL(MethodInfo("stats_flushed", PropertyInfo(Variant::OBJECT, "user", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "GDKUser"), PropertyInfo(Variant::OBJECT, "result", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "GDKResult")));
}

#ifdef XBOX_MODULE_GDK_ENABLED
void GDKStats::set_owner(GDK *p_owner) {
	m_owner = p_owner;
}

GDKRuntime *GDKStats::_get_runtime() const {
	return m_owner == nullptr ? nullptr : m_owner->get_runtime();
}

GDKXboxServices *GDKStats::_get_xbox_services() const {
	return m_owner == nullptr ? nullptr : m_owner->get_xbox_services();
}

Signal GDKStats::_make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data) const {
	GDKRuntime *runtime = _get_runtime();
	ERR_FAIL_NULL_V(runtime, Signal());
	return runtime->make_error_signal(p_hresult, p_code, p_message, p_data);
}

Ref<GDKResult> GDKStats::_ensure_ready_user(const Ref<GDKUser> &p_user) const {
	if (!m_runtime_ready) {
		return GDKResult::error_result(E_FAIL, "runtime_unavailable", "GDK runtime is not initialized.");
	}
	if (!p_user.is_valid() || p_user->get_handle() == nullptr || !p_user->is_signed_in()) {
		return GDKResult::error_result(E_INVALIDARG, "invalid_user", "A signed-in GDKUser is required.");
	}
	return GDKResult::ok_result();
}

GDKStats::CachedStats *GDKStats::_find_cached_stats(const String &p_xuid) {
	for (CachedStats &cached_stats : m_cached_stats) {
		if (cached_stats.xuid == p_xuid) {
			return &cached_stats;
		}
	}
	return nullptr;
}

const GDKStats::CachedStats *GDKStats::_find_cached_stats(const String &p_xuid) const {
	for (const CachedStats &cached_stats : m_cached_stats) {
		if (cached_stats.xuid == p_xuid) {
			return &cached_stats;
		}
	}
	return nullptr;
}

GDKStats::StagedStats *GDKStats::_find_staged_stats(XUserLocalId p_local_id) {
	for (StagedStats &staged_stats : m_staged_stats) {
		if (staged_stats.local_id.value == p_local_id.value) {
			return &staged_stats;
		}
	}
	return nullptr;
}

GDKStats::TrackingState *GDKStats::_find_tracking_state(XUserLocalId p_local_id) {
	for (TrackingState &state : m_tracking_states) {
		if (state.local_id.value == p_local_id.value) {
			return &state;
		}
	}
	return nullptr;
}

void GDKStats::_cache_stat_value(const String &p_xuid, const String &p_stat_name, const String &p_stat_type, const String &p_value, const String &p_scid) {
	if (p_xuid.is_empty() || p_stat_name.is_empty()) {
		return;
	}

	CachedStats *cached_stats = _find_cached_stats(p_xuid);
	if (cached_stats == nullptr) {
		CachedStats new_cached_stats;
		new_cached_stats.xuid = p_xuid;
		m_cached_stats.push_back(new_cached_stats);
		cached_stats = &m_cached_stats.back();
	}

	Dictionary stat;
	stat["name"] = p_stat_name;
	stat["type"] = p_stat_type;
	stat["value"] = p_value;
	stat["service_configuration_id"] = p_scid;
	cached_stats->stats[p_stat_name] = stat;
}

void GDKStats::_cache_results(const XblUserStatisticsResult *p_results, size_t p_result_count) {
	if (p_results == nullptr) {
		return;
	}

	for (size_t result_index = 0; result_index < p_result_count; ++result_index) {
		const XblUserStatisticsResult &result = p_results[result_index];
		if (result.serviceConfigStatistics == nullptr) {
			continue;
		}
		const String xuid = String::num_uint64(result.xboxUserId);
		for (uint32_t scid_index = 0; scid_index < result.serviceConfigStatisticsCount; ++scid_index) {
			const XblServiceConfigurationStatistic &service_config = result.serviceConfigStatistics[scid_index];
			if (service_config.statistics == nullptr) {
				continue;
			}
			const String scid = _utf8_or_empty(service_config.serviceConfigurationId);
			for (uint32_t stat_index = 0; stat_index < service_config.statisticsCount; ++stat_index) {
				const XblStatistic &statistic = service_config.statistics[stat_index];
				_cache_stat_value(
						xuid,
						_utf8_or_empty(statistic.statisticName),
						_utf8_or_empty(statistic.statisticType),
						_utf8_or_empty(statistic.value),
						scid);
			}
		}
	}
}

Dictionary GDKStats::_make_query_payload(const XblUserStatisticsResult *p_results, size_t p_result_count, bool p_single_user_payload) {
	_cache_results(p_results, p_result_count);

	if (p_single_user_payload) {
		if (p_results == nullptr || p_result_count == 0) {
			return Dictionary();
		}
		return _make_stats_dictionary(p_results[0]);
	}

	Dictionary payload;
	if (p_results == nullptr) {
		return payload;
	}
	for (size_t result_index = 0; result_index < p_result_count; ++result_index) {
		const String xuid = String::num_uint64(p_results[result_index].xboxUserId);
		payload[xuid] = _make_stats_dictionary(p_results[result_index]);
	}
	return payload;
}

void GDKStats::_clear_staged_stats(const Ref<GDKUser> &p_user) {
	if (!p_user.is_valid()) {
		return;
	}

	XUserLocalId local_id = {};
	local_id.value = static_cast<uint64_t>(p_user->get_local_id());
	m_staged_stats.erase(
			std::remove_if(
					m_staged_stats.begin(),
					m_staged_stats.end(),
					[local_id](const StagedStats &staged_stats) {
						return staged_stats.local_id.value == local_id.value;
					}),
			m_staged_stats.end());
}

void GDKStats::_apply_staged_stats_to_cache(const Ref<GDKUser> &p_user, const std::vector<StagedStat> &p_stats) {
	const String xuid = p_user.is_valid() ? p_user->get_xuid() : String();
	for (const StagedStat &stat : p_stats) {
		_cache_stat_value(xuid, stat.name, "number", String::num(stat.value), _get_xbox_services() == nullptr ? String() : _get_xbox_services()->get_scid());
	}

	CachedStats *cached_stats = _find_cached_stats(xuid);
	emit_signal("stats_updated", p_user, cached_stats == nullptr ? Dictionary() : cached_stats->stats.duplicate(true));
}

void GDKStats::_close_tracking_state(TrackingState &p_state) {
	if (p_state.callback_context) {
		std::lock_guard<std::mutex> lock(p_state.callback_context->mutex);
		p_state.callback_context->active.store(false, std::memory_order_release);
		p_state.callback_context->stats = nullptr;
	}

	if (p_state.handler_registered && p_state.context != nullptr) {
		XblUserStatisticsRemoveStatisticChangedHandler(p_state.context, p_state.handler_token);
	}
	p_state.handler_registered = false;
	p_state.handler_token = {};

	if (p_state.context != nullptr) {
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

Ref<GDKResult> GDKStats::on_runtime_initialized() {
	m_runtime_ready = true;
	return GDKResult::ok_result();
}

void GDKStats::shutdown() {
	m_runtime_ready = false;

	for (TrackingState &state : m_tracking_states) {
		_close_tracking_state(state);
	}
	m_tracking_states.clear();
	m_staged_stats.clear();
	m_cached_stats.clear();

	{
		std::lock_guard<std::mutex> lock(m_pending_stat_changes_mutex);
		m_pending_stat_changes.clear();
	}
}

int GDKStats::dispatch() {
	if (!m_runtime_ready) {
		return 0;
	}

	std::vector<PendingStatChange> changes;
	{
		std::lock_guard<std::mutex> lock(m_pending_stat_changes_mutex);
		changes.swap(m_pending_stat_changes);
	}

	int dispatched = 0;
	for (const PendingStatChange &change : changes) {
		const String xuid = String::num_uint64(change.xbox_user_id);
		const String stat_name = String::utf8(change.statistic_name.c_str());
		const String stat_type = String::utf8(change.statistic_type.c_str());
		const String value = String::utf8(change.value.c_str());
		_cache_stat_value(xuid, stat_name, stat_type, value, _get_xbox_services() == nullptr ? String() : _get_xbox_services()->get_scid());

		Ref<GDKUser> user;
		for (const TrackingState &state : m_tracking_states) {
			if (state.xbox_user_id == change.xbox_user_id) {
				user = state.user;
				break;
			}
		}

		CachedStats *cached_stats = _find_cached_stats(xuid);
		emit_signal("stat_changed", user, stat_name, value);
		if (!m_runtime_ready) {
			break;
		}
		emit_signal("stats_updated", user, cached_stats == nullptr ? Dictionary() : cached_stats->stats.duplicate(true));
		++dispatched;
		if (!m_runtime_ready) {
			break;
		}
	}

	return dispatched;
}

Signal GDKStats::query_user_stats_async(const Ref<GDKUser> &p_user, const PackedStringArray &p_stat_names) {
	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr) {
		return Signal();
	}

	Ref<GDKResult> validation = _ensure_ready_user(p_user);
	if (!validation->is_ok()) {
		return _make_error_signal(static_cast<HRESULT>(validation->get_hresult()), validation->get_code(), validation->get_message());
	}

	std::vector<String> stat_names;
	std::vector<CharString> stat_name_utf8;
	std::vector<const char *> stat_name_ptrs;
	Ref<GDKResult> parse_result = _parse_stat_names(p_stat_names, &stat_names, &stat_name_utf8, &stat_name_ptrs);
	if (!parse_result->is_ok()) {
		return _make_error_signal(static_cast<HRESULT>(parse_result->get_hresult()), parse_result->get_code(), parse_result->get_message());
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
	QueryStatsAsyncContext *async_context = new QueryStatsAsyncContext(this, p_user, runtime, pending_signal, context, xbox_services->get_scid(), stat_names, xbox_user_id, std::vector<uint64_t>(), true, false);
	async_context->bind_cancel_handler();

	hr = XblUserStatisticsGetSingleUserStatisticsAsync(
			async_context->get_context(),
			async_context->get_xbox_user_id(),
			async_context->get_scid(),
			async_context->get_stat_name_ptrs(),
			async_context->get_stat_name_count(),
			async_context->get_async_block());
	if (FAILED(hr)) {
		async_context->clear_cancel_handler();
		delete async_context;
		return _make_error_signal(hr, "stats_query_failed", "Failed to start statistic query.");
	}

	return pending_signal->get_completed_signal();
}

Signal GDKStats::query_users_stats_async(const Ref<GDKUser> &p_user, const PackedStringArray &p_xuids, const PackedStringArray &p_stat_names) {
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
		if (!_try_parse_xuid(String(p_xuids[i]), &xuid)) {
			return _make_error_signal(E_INVALIDARG, "invalid_xuid", "XUID values must be non-empty decimal strings.");
		}
		xuids.push_back(xuid);
	}

	std::vector<String> stat_names;
	std::vector<CharString> stat_name_utf8;
	std::vector<const char *> stat_name_ptrs;
	Ref<GDKResult> parse_result = _parse_stat_names(p_stat_names, &stat_names, &stat_name_utf8, &stat_name_ptrs);
	if (!parse_result->is_ok()) {
		return _make_error_signal(static_cast<HRESULT>(parse_result->get_hresult()), parse_result->get_code(), parse_result->get_message());
	}

	GDKXboxServices *xbox_services = _get_xbox_services();
	if (xbox_services == nullptr || !xbox_services->is_initialized()) {
		return _make_error_signal(E_FAIL, "xbox_services_uninitialized", "Xbox services are not initialized.");
	}

	XblContextHandle context = nullptr;
	HRESULT hr = xbox_services->duplicate_context_for_user(p_user, &context);
	if (FAILED(hr)) {
		return _make_error_signal(hr, "xbox_context_unavailable", "Failed to create an Xbox services context for the user.");
	}

	Ref<GDKPendingSignal> pending_signal = runtime->make_pending_signal();
	QueryStatsAsyncContext *async_context = new QueryStatsAsyncContext(this, p_user, runtime, pending_signal, context, xbox_services->get_scid(), stat_names, 0, xuids, false, true);
	async_context->bind_cancel_handler();

	hr = XblUserStatisticsGetMultipleUserStatisticsAsync(
			async_context->get_context(),
			async_context->get_xuids(),
			async_context->get_xuid_count(),
			async_context->get_scid(),
			async_context->get_stat_name_ptrs(),
			async_context->get_stat_name_count(),
			async_context->get_async_block());
	if (FAILED(hr)) {
		async_context->clear_cancel_handler();
		delete async_context;
		return _make_error_signal(hr, "stats_query_failed", "Failed to start multi-user statistic query.");
	}

	return pending_signal->get_completed_signal();
}

Ref<GDKResult> GDKStats::set_stat_integer(const Ref<GDKUser> &p_user, const String &p_stat_name, int64_t p_value) {
	return set_stat_number(p_user, p_stat_name, static_cast<double>(p_value));
}

Ref<GDKResult> GDKStats::set_stat_number(const Ref<GDKUser> &p_user, const String &p_stat_name, double p_value) {
	Ref<GDKResult> validation = _ensure_ready_user(p_user);
	if (!validation->is_ok()) {
		return validation;
	}

	const String stat_name = p_stat_name.strip_edges();
	if (stat_name.is_empty()) {
		return GDKResult::error_result(E_INVALIDARG, "invalid_stat_name", "Statistic name must be a non-empty string.");
	}

	XUserLocalId local_id = {};
	local_id.value = static_cast<uint64_t>(p_user->get_local_id());
	StagedStats *staged_stats = _find_staged_stats(local_id);
	if (staged_stats == nullptr) {
		StagedStats new_staged_stats;
		new_staged_stats.user = p_user;
		new_staged_stats.local_id = local_id;
		m_staged_stats.push_back(new_staged_stats);
		staged_stats = &m_staged_stats.back();
	}

	for (StagedStat &staged_stat : staged_stats->stats) {
		if (staged_stat.name == stat_name) {
			staged_stat.value = p_value;
			Dictionary data;
			data["name"] = stat_name;
			data["value"] = p_value;
			return GDKResult::ok_result(data);
		}
	}

	StagedStat staged_stat;
	staged_stat.name = stat_name;
	staged_stat.value = p_value;
	staged_stats->stats.push_back(staged_stat);

	Dictionary data;
	data["name"] = stat_name;
	data["value"] = p_value;
	return GDKResult::ok_result(data);
}

Signal GDKStats::flush_stats_async(const Ref<GDKUser> &p_user) {
	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr) {
		return Signal();
	}

	Ref<GDKResult> validation = _ensure_ready_user(p_user);
	if (!validation->is_ok()) {
		return _make_error_signal(static_cast<HRESULT>(validation->get_hresult()), validation->get_code(), validation->get_message());
	}
	XUserLocalId local_id = {};
	local_id.value = static_cast<uint64_t>(p_user->get_local_id());
	StagedStats *staged_stats = _find_staged_stats(local_id);
	if (staged_stats == nullptr || staged_stats->stats.empty()) {
		return _make_error_signal(E_INVALIDARG, "no_staged_stats", "No staged statistics are available to flush.");
	}

	GDKXboxServices *xbox_services = _get_xbox_services();
	if (xbox_services == nullptr || !xbox_services->is_initialized()) {
		return _make_error_signal(E_FAIL, "xbox_services_uninitialized", "Xbox services are not initialized.");
	}

	XblContextHandle context = nullptr;
	HRESULT hr = xbox_services->duplicate_context_for_user(p_user, &context);
	if (FAILED(hr)) {
		return _make_error_signal(hr, "xbox_context_unavailable", "Failed to create an Xbox services context for the user.");
	}

	Ref<GDKPendingSignal> pending_signal = runtime->make_pending_signal();
	FlushStatsAsyncContext *async_context = new FlushStatsAsyncContext(this, p_user, runtime, pending_signal, context, staged_stats->stats);
	async_context->bind_cancel_handler();

	hr = XblTitleManagedStatsUpdateStatsAsync(
			async_context->get_context(),
			async_context->get_native_stats(),
			async_context->get_native_stats_count(),
			async_context->get_async_block());
	if (FAILED(hr)) {
		async_context->clear_cancel_handler();
		delete async_context;
		return _make_error_signal(hr, "stats_flush_start_failed", "Failed to start title-managed statistic flush.");
	}

	return pending_signal->get_completed_signal();
}

Ref<GDKResult> GDKStats::track_stats(const Ref<GDKUser> &p_user, const PackedStringArray &p_stat_names) {
	Ref<GDKResult> validation = _ensure_ready_user(p_user);
	if (!validation->is_ok()) {
		return validation;
	}

	std::vector<String> stat_names;
	std::vector<CharString> stat_name_utf8;
	std::vector<const char *> stat_name_ptrs;
	Ref<GDKResult> parse_result = _parse_stat_names(p_stat_names, &stat_names, &stat_name_utf8, &stat_name_ptrs);
	if (!parse_result->is_ok()) {
		return parse_result;
	}

	GDKXboxServices *xbox_services = _get_xbox_services();
	if (xbox_services == nullptr || !xbox_services->is_initialized()) {
		return GDKResult::error_result(E_FAIL, "xbox_services_uninitialized", "Xbox services are not initialized.");
	}

	XUserLocalId local_id = {};
	local_id.value = static_cast<uint64_t>(p_user->get_local_id());
	TrackingState *state = _find_tracking_state(local_id);
	if (state == nullptr) {
		TrackingState new_state;
		new_state.user = p_user;
		new_state.local_id = local_id;

		HRESULT context_hr = xbox_services->duplicate_context_for_user(p_user, &new_state.context, &new_state.xbox_user_id);
		if (FAILED(context_hr)) {
			return GDKResult::hresult_error(context_hr, "Failed to create an Xbox services context for statistics tracking.", "xbox_context_unavailable");
		}

		new_state.callback_context = std::make_shared<TrackingState::CallbackContext>();
		new_state.callback_context->stats = this;
		new_state.callback_token = std::make_shared<TrackingState::CallbackToken>();
		new_state.callback_token->context = new_state.callback_context;
		new_state.handler_token = XblUserStatisticsAddStatisticChangedHandler(new_state.context, _statistic_changed_handler, new_state.callback_token.get());
		new_state.handler_registered = true;
		m_tracking_states.push_back(new_state);
		state = &m_tracking_states.back();
	}

	const CharString scid_utf8 = xbox_services->get_scid().utf8();
	const uint64_t xuid = state->xbox_user_id;
	HRESULT hr = XblUserStatisticsTrackStatistics(
			state->context,
			&xuid,
			1,
			scid_utf8.get_data(),
			stat_name_ptrs.data(),
			stat_name_ptrs.size());
	if (FAILED(hr)) {
		return GDKResult::hresult_error(hr, "Failed to track statistic changes.", "stats_track_failed");
	}

	Dictionary data;
	data["xuid"] = String::num_uint64(xuid);
	data["stat_names"] = p_stat_names;
	return GDKResult::ok_result(data);
}

Ref<GDKResult> GDKStats::stop_tracking_stats(const Ref<GDKUser> &p_user, const PackedStringArray &p_stat_names) {
	Ref<GDKResult> validation = _ensure_ready_user(p_user);
	if (!validation->is_ok()) {
		return validation;
	}

	GDKXboxServices *xbox_services = _get_xbox_services();
	if (xbox_services == nullptr || !xbox_services->is_initialized()) {
		return GDKResult::error_result(E_FAIL, "xbox_services_uninitialized", "Xbox services are not initialized.");
	}

	XUserLocalId local_id = {};
	local_id.value = static_cast<uint64_t>(p_user->get_local_id());
	TrackingState *state = _find_tracking_state(local_id);
	if (state == nullptr) {
		return GDKResult::ok_result();
	}

	HRESULT hr = S_OK;
	const uint64_t xuid = state->xbox_user_id;
	if (p_stat_names.is_empty()) {
		hr = XblUserStatisticsStopTrackingUsers(state->context, &xuid, 1);
		_close_tracking_state(*state);
		m_tracking_states.erase(
				std::remove_if(
						m_tracking_states.begin(),
						m_tracking_states.end(),
						[local_id](const TrackingState &candidate) {
							return candidate.local_id.value == local_id.value;
						}),
				m_tracking_states.end());
	} else {
		std::vector<String> stat_names;
		std::vector<CharString> stat_name_utf8;
		std::vector<const char *> stat_name_ptrs;
		Ref<GDKResult> parse_result = _parse_stat_names(p_stat_names, &stat_names, &stat_name_utf8, &stat_name_ptrs);
		if (!parse_result->is_ok()) {
			return parse_result;
		}

		const CharString scid_utf8 = xbox_services->get_scid().utf8();
		hr = XblUserStatisticsStopTrackingStatistics(
				state->context,
				&xuid,
				1,
				scid_utf8.get_data(),
				stat_name_ptrs.data(),
				stat_name_ptrs.size());
	}

	if (FAILED(hr)) {
		return GDKResult::hresult_error(hr, "Failed to stop tracking statistic changes.", "stats_stop_tracking_failed");
	}

	return GDKResult::ok_result();
}

Dictionary GDKStats::get_cached_stats(const Ref<GDKUser> &p_user) const {
	if (!p_user.is_valid()) {
		return Dictionary();
	}

	const CachedStats *cached_stats = _find_cached_stats(p_user->get_xuid());
	return cached_stats == nullptr ? Dictionary() : cached_stats->stats.duplicate(true);
}

void GDKStats::on_user_removed(const Ref<GDKUser> &p_user) {
	if (!p_user.is_valid()) {
		return;
	}

	XUserLocalId local_id = {};
	local_id.value = static_cast<uint64_t>(p_user->get_local_id());
	m_staged_stats.erase(
			std::remove_if(
					m_staged_stats.begin(),
					m_staged_stats.end(),
					[local_id](const StagedStats &staged_stats) {
						return staged_stats.local_id.value == local_id.value;
					}),
			m_staged_stats.end());
	m_tracking_states.erase(
			std::remove_if(
					m_tracking_states.begin(),
					m_tracking_states.end(),
					[this, local_id](TrackingState &state) {
						if (state.local_id.value != local_id.value) {
							return false;
						}
						_close_tracking_state(state);
						return true;
					}),
			m_tracking_states.end());
}

void CALLBACK GDKStats::_statistic_changed_handler(XblStatisticChangeEventArgs p_args, void *p_context) {
	TrackingState::CallbackToken *token = static_cast<TrackingState::CallbackToken *>(p_context);
	std::shared_ptr<TrackingState::CallbackContext> callback_context = token != nullptr ? token->context.lock() : nullptr;
	if (!callback_context || !callback_context->active.load(std::memory_order_acquire)) {
		return;
	}

	std::lock_guard<std::mutex> context_lock(callback_context->mutex);
	GDKStats *stats = callback_context->stats;
	if (!callback_context->active.load(std::memory_order_acquire) || stats == nullptr) {
		return;
	}

	PendingStatChange change;
	change.xbox_user_id = p_args.xboxUserId;
	change.statistic_name = p_args.latestStatistic.statisticName == nullptr ? std::string() : std::string(p_args.latestStatistic.statisticName);
	change.statistic_type = p_args.latestStatistic.statisticType == nullptr ? std::string() : std::string(p_args.latestStatistic.statisticType);
	change.value = p_args.latestStatistic.value == nullptr ? std::string() : std::string(p_args.latestStatistic.value);

	std::lock_guard<std::mutex> lock(stats->m_pending_stat_changes_mutex);
	stats->m_pending_stat_changes.push_back(change);
}

#else
void GDKStats::set_owner(GDK *p_owner) {}

Signal GDKStats::_make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data) const {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Ref<GDKResult> GDKStats::_ensure_ready_user(const Ref<GDKUser> &p_user) const {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

void GDKStats::_cache_stat_value(const String &p_xuid, const String &p_stat_name, const String &p_stat_type, const String &p_value, const String &p_scid) {}

void GDKStats::_cache_results(const XblUserStatisticsResult *p_results, size_t p_result_count) {}

Dictionary GDKStats::_make_query_payload(const XblUserStatisticsResult *p_results, size_t p_result_count, bool p_single_user_payload) {
	return Dictionary();
}

void GDKStats::_clear_staged_stats(const Ref<GDKUser> &p_user) {}

void GDKStats::_apply_staged_stats_to_cache(const Ref<GDKUser> &p_user, const std::vector<StagedStat> &p_stats) {}

void GDKStats::_close_tracking_state(TrackingState &p_state) {}

Ref<GDKResult> GDKStats::on_runtime_initialized() {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

void GDKStats::shutdown() {}

int GDKStats::dispatch() {
	return 0;
}

Signal GDKStats::query_user_stats_async(const Ref<GDKUser> &p_user, const PackedStringArray &p_stat_names) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Signal GDKStats::query_users_stats_async(const Ref<GDKUser> &p_user, const PackedStringArray &p_xuids, const PackedStringArray &p_stat_names) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Ref<GDKResult> GDKStats::set_stat_integer(const Ref<GDKUser> &p_user, const String &p_stat_name, int64_t p_value) {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Ref<GDKResult> GDKStats::set_stat_number(const Ref<GDKUser> &p_user, const String &p_stat_name, double p_value) {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Signal GDKStats::flush_stats_async(const Ref<GDKUser> &p_user) {
	Ref<GDKPendingSignal> p;
	p.instantiate();
	p->complete(GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build."));
	return p->get_completed_signal();
}

Ref<GDKResult> GDKStats::track_stats(const Ref<GDKUser> &p_user, const PackedStringArray &p_stat_names) {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Ref<GDKResult> GDKStats::stop_tracking_stats(const Ref<GDKUser> &p_user, const PackedStringArray &p_stat_names) {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Dictionary GDKStats::get_cached_stats(const Ref<GDKUser> &p_user) const {
	return Dictionary();
}

void GDKStats::on_user_removed(const Ref<GDKUser> &p_user) {}
#endif
