/**************************************************************************/
/*  steam.cpp                                                             */
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

#include "steam.h"

#include "core/config/engine.h"
#include "core/io/image.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "steam_auth_client.h"
#include "steam_types.h"

Steam *Steam::singleton = nullptr;

void Steam::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_available"), &Steam::is_available);
	ClassDB::bind_method(D_METHOD("has_inventory_support"), &Steam::has_inventory_support);
	ClassDB::bind_method(D_METHOD("initialize", "app_id"), &Steam::initialize, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("shutdown"), &Steam::shutdown);
	ClassDB::bind_method(D_METHOD("is_initialized"), &Steam::is_initialized);
	ClassDB::bind_method(D_METHOD("set_debug_logging", "enabled"), &Steam::set_debug_logging);
	ClassDB::bind_method(D_METHOD("is_debug_logging_enabled"), &Steam::is_debug_logging_enabled);
	ClassDB::bind_method(D_METHOD("get_debug_log"), &Steam::get_debug_log);
	ClassDB::bind_method(D_METHOD("clear_debug_log"), &Steam::clear_debug_log);
	ClassDB::bind_method(D_METHOD("request_web_api_ticket", "identity"), &Steam::request_web_api_ticket, DEFVAL("blazium"));
	ClassDB::bind_method(D_METHOD("poll_callbacks"), &Steam::poll_callbacks);
	ClassDB::bind_method(D_METHOD("get_ticket_state"), &Steam::get_ticket_state);
	ClassDB::bind_method(D_METHOD("get_pending_hex_ticket"), &Steam::get_pending_hex_ticket);
	ClassDB::bind_method(D_METHOD("get_pending_auth_ticket_handle"), &Steam::get_pending_auth_ticket_handle);
	ClassDB::bind_method(D_METHOD("get_pending_ticket_error"), &Steam::get_pending_ticket_error);
	ClassDB::bind_method(D_METHOD("authenticate_with_server", "url", "ticket", "app_id"), &Steam::authenticate_with_server);
	ClassDB::bind_method(D_METHOD("cancel_auth_ticket", "handle"), &Steam::cancel_auth_ticket);
	ClassDB::bind_method(D_METHOD("get_local_steam_id"), &Steam::get_local_steam_id);
	ClassDB::bind_method(D_METHOD("is_logged_on"), &Steam::is_logged_on);

	ClassDB::bind_method(D_METHOD("request_current_stats"), &Steam::request_current_stats);
	ClassDB::bind_method(D_METHOD("refresh_current_stats"), &Steam::refresh_current_stats);
	ClassDB::bind_method(D_METHOD("store_stats"), &Steam::store_stats);
	ClassDB::bind_method(D_METHOD("set_achievement", "name"), &Steam::set_achievement);
	ClassDB::bind_method(D_METHOD("clear_achievement", "name"), &Steam::clear_achievement);
	ClassDB::bind_method(D_METHOD("get_achievement", "name"), &Steam::get_achievement);
	ClassDB::bind_method(D_METHOD("get_achievement_info", "name", "include_icon"), &Steam::get_achievement_info, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("get_all_achievements", "include_icons"), &Steam::get_all_achievements, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_stat_int", "name"), &Steam::get_stat_int);
	ClassDB::bind_method(D_METHOD("get_stat_float", "name"), &Steam::get_stat_float);
	ClassDB::bind_method(D_METHOD("set_stat_int", "name", "value"), &Steam::set_stat_int);
	ClassDB::bind_method(D_METHOD("set_stat_float", "name", "value"), &Steam::set_stat_float);
	ClassDB::bind_method(D_METHOD("increment_stat_int", "name", "delta"), &Steam::increment_stat_int);
	ClassDB::bind_method(D_METHOD("clear_stat", "name"), &Steam::clear_stat);
	ClassDB::bind_method(D_METHOD("get_persona_name"), &Steam::get_persona_name);
	ClassDB::bind_method(D_METHOD("get_avatar_image", "steam_id", "size"), &Steam::get_avatar_image, DEFVAL(0), DEFVAL(2));

	ClassDB::bind_method(D_METHOD("load_item_definitions"), &Steam::load_item_definitions);
	ClassDB::bind_method(D_METHOD("request_item_definitions", "timeout_sec"), &Steam::request_item_definitions, DEFVAL(15.0));
	ClassDB::bind_method(D_METHOD("get_item_definition_ids"), &Steam::get_item_definition_ids);
	ClassDB::bind_method(D_METHOD("get_item_definition", "def_id"), &Steam::get_item_definition);
	ClassDB::bind_method(D_METHOD("get_all_item_definitions"), &Steam::get_all_item_definitions);
	ClassDB::bind_method(D_METHOD("get_all_items"), &Steam::get_all_items);
	ClassDB::bind_method(D_METHOD("get_items_by_id", "instance_ids"), &Steam::get_items_by_id);
	ClassDB::bind_method(D_METHOD("find_items_by_def_id", "def_id"), &Steam::find_items_by_def_id);
	ClassDB::bind_method(D_METHOD("get_item_definition_icon", "def_id"), &Steam::get_item_definition_icon);
	ClassDB::bind_method(D_METHOD("consume_item", "instance_id", "quantity"), &Steam::consume_item);
	ClassDB::bind_method(D_METHOD("exchange_items", "generate_def_ids", "generate_quantities", "destroy_instance_ids", "destroy_quantities"), &Steam::exchange_items);
	ClassDB::bind_method(D_METHOD("trigger_item_drop", "drop_list_definition"), &Steam::trigger_item_drop);
	ClassDB::bind_method(D_METHOD("transfer_item_quantity", "source_instance_id", "quantity", "dest_instance_id"), &Steam::transfer_item_quantity);
	ClassDB::bind_method(D_METHOD("grant_promo_items"), &Steam::grant_promo_items);
	ClassDB::bind_method(D_METHOD("get_eligible_promo_item_definition_ids", "steam_id"), &Steam::get_eligible_promo_item_definition_ids, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_inventory_result_status", "result_handle"), &Steam::get_inventory_result_status);
	ClassDB::bind_method(D_METHOD("get_inventory_result_items", "result_handle"), &Steam::get_inventory_result_items);
	ClassDB::bind_method(D_METHOD("add_promo_item", "def_id"), &Steam::add_promo_item);
	ClassDB::bind_method(D_METHOD("add_promo_items", "def_ids"), &Steam::add_promo_items);
	ClassDB::bind_method(D_METHOD("start_property_update"), &Steam::start_property_update);
	ClassDB::bind_method(D_METHOD("set_property_string", "instance_id", "property_name", "value"), &Steam::set_property_string);
	ClassDB::bind_method(D_METHOD("set_property_bool", "instance_id", "property_name", "value"), &Steam::set_property_bool);
	ClassDB::bind_method(D_METHOD("set_property_int64", "instance_id", "property_name", "value"), &Steam::set_property_int64);
	ClassDB::bind_method(D_METHOD("set_property_float", "instance_id", "property_name", "value"), &Steam::set_property_float);
	ClassDB::bind_method(D_METHOD("remove_property", "instance_id", "property_name"), &Steam::remove_property);
	ClassDB::bind_method(D_METHOD("submit_property_update"), &Steam::submit_property_update);
	ClassDB::bind_method(D_METHOD("serialize_inventory_result", "result_handle"), &Steam::serialize_inventory_result);
	ClassDB::bind_method(D_METHOD("deserialize_inventory", "bytes", "expected_steam_id"), &Steam::deserialize_inventory, DEFVAL(0));

	BIND_ENUM_CONSTANT(TICKET_STATE_IDLE);
	BIND_ENUM_CONSTANT(TICKET_STATE_PENDING);
	BIND_ENUM_CONSTANT(TICKET_STATE_READY);
	BIND_ENUM_CONSTANT(TICKET_STATE_FAILED);

	BIND_ENUM_CONSTANT(AVATAR_SIZE_SMALL);
	BIND_ENUM_CONSTANT(AVATAR_SIZE_MEDIUM);
	BIND_ENUM_CONSTANT(AVATAR_SIZE_LARGE);

	ADD_SIGNAL(MethodInfo("web_api_ticket_ready", PropertyInfo(Variant::STRING, "hex_ticket"), PropertyInfo(Variant::INT, "auth_ticket_handle")));
	ADD_SIGNAL(MethodInfo("web_api_ticket_failed", PropertyInfo(Variant::STRING, "error_message")));
	ADD_SIGNAL(MethodInfo("inventory_result_ready", PropertyInfo(Variant::INT, "result_handle"), PropertyInfo(Variant::INT, "result_code")));
	ADD_SIGNAL(MethodInfo("inventory_full_update", PropertyInfo(Variant::INT, "result_handle")));
	ADD_SIGNAL(MethodInfo("inventory_definitions_updated"));
}

Steam *Steam::get_singleton() {
	return singleton;
}

Steam::Steam() {
	singleton = this;
	auth_client = memnew(SteamAuthClient);
}

Steam::~Steam() {
	if (initialized) {
		shutdown();
	}
	memdelete(auth_client);
	auth_client = nullptr;
	if (singleton == this) {
		singleton = nullptr;
	}
}

void Steam::_log_debug(const String &p_message) {
	if (!debug_logging) {
		return;
	}
	print_line(vformat("[Steam] %s", p_message));
	if (debug_log.size() >= kMaxDebugLogEntries) {
		debug_log.remove_at(0);
	}
	debug_log.push_back(p_message);
}

void Steam::_reset_ticket_state() {
	pending_auth_ticket = 0;
	pending_hex_ticket = String();
	pending_ticket_error = String();
	ticket_state = TICKET_STATE_IDLE;
}

void Steam::_handle_callback(int p_callback_id, const void *p_data, int p_size) {
	if (p_callback_id == SteamGetTicketForWebApiResponse::k_iCallback) {
		if (p_size < (int)sizeof(SteamGetTicketForWebApiResponse)) {
			_log_debug(vformat("GetTicketForWebApiResponse too small: %d", p_size));
			return;
		}

		const SteamGetTicketForWebApiResponse *response = (const SteamGetTicketForWebApiResponse *)p_data;
		if (response->m_eResult != STEAM_RESULT_OK) {
			ticket_state = TICKET_STATE_FAILED;
			pending_ticket_error = vformat("Steam ticket request failed (result=%d)", (int)response->m_eResult);
			_log_debug(pending_ticket_error);
			emit_signal("web_api_ticket_failed", pending_ticket_error);
			return;
		}

		if (response->m_cubTicket <= 0 || response->m_cubTicket > SteamGetTicketForWebApiResponse::k_nCubTicketMaxLength) {
			ticket_state = TICKET_STATE_FAILED;
			pending_ticket_error = "Steam returned invalid ticket size";
			_log_debug(pending_ticket_error);
			emit_signal("web_api_ticket_failed", pending_ticket_error);
			return;
		}

		pending_auth_ticket = response->m_hAuthTicket;
		pending_hex_ticket = SteamAPILoader::bytes_to_hex(response->m_rgubTicket, response->m_cubTicket);
		ticket_state = TICKET_STATE_READY;
		_log_debug(vformat("Web API ticket ready (%d bytes, handle=%d)", response->m_cubTicket, (int)pending_auth_ticket));
		emit_signal("web_api_ticket_ready", pending_hex_ticket, (int)pending_auth_ticket);
		return;
	}

	if (p_callback_id == SteamUserStatsReceived::k_iCallback) {
		if (p_size < (int)sizeof(SteamUserStatsReceived)) {
			return;
		}
		const SteamUserStatsReceived *response = (const SteamUserStatsReceived *)p_data;
		if (response->m_eResult == STEAM_RESULT_OK) {
			if (app_id > 0) {
				const uint32_t callback_app_id = (uint32_t)(response->m_nGameID & 0xFFFFFFFFULL);
				if (callback_app_id != (uint32_t)app_id) {
					return;
				}
			}
			stats_received = true;
			_log_debug("User stats received");
		} else {
			_log_debug(vformat("User stats receive failed (result=%d)", response->m_eResult));
		}
		return;
	}

	if (p_callback_id == SteamUserStatsStored::k_iCallback) {
		if (p_size < (int)sizeof(SteamUserStatsStored)) {
			return;
		}
		const SteamUserStatsStored *response = (const SteamUserStatsStored *)p_data;
		stats_store_pending = false;
		stats_store_succeeded = response->m_eResult == STEAM_RESULT_OK;
		if (stats_store_succeeded) {
			stats_received = true;
			_log_debug("User stats stored");
		} else {
			_log_debug(vformat("User stats store failed (result=%d)", response->m_eResult));
		}
		return;
	}

	if (p_callback_id == SteamInventoryResultReady::k_iCallback) {
		if (p_size < (int)sizeof(SteamInventoryResultReady)) {
			return;
		}
		const SteamInventoryResultReady *response = (const SteamInventoryResultReady *)p_data;
		inventory_pending_results[response->m_handle] = response->m_result;
		_log_debug(vformat("Inventory result ready handle=%d result=%d", (int)response->m_handle, response->m_result));
		emit_signal("inventory_result_ready", (int)response->m_handle, response->m_result);
		return;
	}

	if (p_callback_id == SteamInventoryFullUpdate::k_iCallback) {
		if (p_size < (int)sizeof(SteamInventoryFullUpdate)) {
			return;
		}
		const SteamInventoryFullUpdate *response = (const SteamInventoryFullUpdate *)p_data;
		_log_debug(vformat("Inventory full update handle=%d", (int)response->m_handle));
		emit_signal("inventory_full_update", (int)response->m_handle);
		return;
	}

	if (p_callback_id == SteamInventoryDefinitionUpdate::k_iCallback) {
		inventory_definitions_loaded = true;
		inventory_definitions_pending = false;
		_log_debug("Inventory definitions updated");
		emit_signal("inventory_definitions_updated");
	}
}

void Steam::poll_callbacks() {
	_dispatch_callbacks();
}

void Steam::_dispatch_callbacks() {
	if (!initialized || !loader.is_loaded()) {
		return;
	}

	loader.manual_dispatch_run_frame(steam_pipe);

	SteamCallbackMsg callback;
	while (loader.manual_dispatch_get_next_callback(steam_pipe, &callback)) {
		if (callback.m_pubParam && callback.m_cubParam > 0) {
			_handle_callback(callback.m_iCallback, callback.m_pubParam, callback.m_cubParam);
		}
		loader.manual_dispatch_free_last_callback(steam_pipe);
	}
}

bool Steam::is_available() {
	if (dll_available) {
		return true;
	}
	dll_available = loader.try_load();
	if (dll_available) {
		_log_debug("steam_api library loaded");
	} else {
		_log_debug("steam_api library not found; Steam features disabled");
	}
	return dll_available;
}

bool Steam::has_inventory_support() const {
	return loader.has_inventory_support();
}

Error Steam::initialize(int p_app_id) {
	if (!is_available()) {
		return ERR_UNAVAILABLE;
	}
	if (initialized) {
		return OK;
	}

	app_id = p_app_id;
	if (app_id > 0) {
		::OS::get_singleton()->set_environment("SteamAppId", itos(app_id));
		::OS::get_singleton()->set_environment("SteamGameId", itos(app_id));
		_log_debug(vformat("Set SteamAppId=%d", app_id));
	}

	String err_msg;
	int init_result = loader.init_flat(err_msg);
	if (init_result != 0) {
		_log_debug(vformat("SteamAPI_InitFlat failed (%d): %s", init_result, err_msg));
		return ERR_CANT_OPEN;
	}

	loader.manual_dispatch_init();
	steam_pipe = loader.get_h_steam_pipe();
	steam_user = loader.get_steam_user();
	if (!steam_user) {
		loader.shutdown();
		_log_debug("Failed to acquire ISteamUser interface");
		return ERR_CANT_CREATE;
	}

	if (loader.has_stats_support()) {
		steam_user_stats = loader.get_steam_user_stats();
		steam_friends = loader.get_steam_friends();
		steam_utils = loader.get_steam_utils();
		if (!steam_user_stats || !steam_friends || !steam_utils) {
			_log_debug("Steam stats/friends/utils interfaces unavailable");
			steam_user_stats = nullptr;
			steam_friends = nullptr;
			steam_utils = nullptr;
		}
	}

	if (loader.has_inventory_support()) {
		steam_inventory = loader.get_steam_inventory();
		if (!steam_inventory) {
			_log_debug("Steam inventory interface unavailable");
		}
	}

	initialized = true;
	_log_debug("Steam initialized");

	// Pump callbacks until logged on or timeout.
	const double start_usec = Time::get_singleton()->get_ticks_usec();
	while (!loader.is_logged_on(steam_user)) {
		_dispatch_callbacks();
		if ((Time::get_singleton()->get_ticks_usec() - start_usec) / 1000000.0 > 10.0) {
			_log_debug("Timed out waiting for Steam login");
			break;
		}
		::OS::get_singleton()->delay_usec(10000);
	}

	if (loader.is_logged_on(steam_user)) {
		_log_debug(vformat("Steam user logged on (steam_id=%s)", String::num_uint64(loader.get_steam_id(steam_user))));
		_wait_for_user_stats(10.0, false);
		if (steam_inventory) {
			request_item_definitions(15.0);
		}
	} else {
		_log_debug("Steam user not logged on yet");
	}

	return OK;
}

void Steam::shutdown() {
	if (!initialized) {
		return;
	}
	if (pending_auth_ticket != 0 && steam_user) {
		loader.cancel_auth_ticket(steam_user, pending_auth_ticket);
	}
	_reset_ticket_state();
	loader.shutdown();
	steam_user = nullptr;
	steam_user_stats = nullptr;
	steam_friends = nullptr;
	steam_utils = nullptr;
	steam_inventory = nullptr;
	steam_pipe = 0;
	stats_received = false;
	stats_store_pending = false;
	stats_store_succeeded = false;
	inventory_definitions_loaded = false;
	inventory_definitions_pending = false;
	inventory_pending_results.clear();
	inventory_property_update_handle = STEAM_INVENTORY_UPDATE_HANDLE_INVALID;
	initialized = false;
	_log_debug("Steam shutdown");
}

void Steam::set_debug_logging(bool p_enabled) {
	debug_logging = p_enabled;
}

Array Steam::get_debug_log() const {
	Array out;
	for (int i = 0; i < debug_log.size(); i++) {
		out.push_back(debug_log[i]);
	}
	return out;
}

void Steam::clear_debug_log() {
	debug_log.clear();
}

void Steam::request_web_api_ticket(const String &p_identity) {
	if (!initialized || !steam_user) {
		pending_ticket_error = "Steam not initialized";
		ticket_state = TICKET_STATE_FAILED;
		emit_signal("web_api_ticket_failed", pending_ticket_error);
		return;
	}

	_reset_ticket_state();
	ticket_state = TICKET_STATE_PENDING;

	CharString identity_utf8 = p_identity.utf8();
	pending_auth_ticket = loader.get_auth_ticket_for_web_api(steam_user, identity_utf8.get_data());
	_log_debug(vformat("Requested Web API ticket (identity=%s, handle=%d)", p_identity, (int)pending_auth_ticket));

	const double start_usec = Time::get_singleton()->get_ticks_usec();
	while (ticket_state == TICKET_STATE_PENDING) {
		_dispatch_callbacks();
		if ((Time::get_singleton()->get_ticks_usec() - start_usec) / 1000000.0 > 15.0) {
			ticket_state = TICKET_STATE_FAILED;
			pending_ticket_error = "Timed out waiting for Web API ticket callback";
			_log_debug(pending_ticket_error);
			emit_signal("web_api_ticket_failed", pending_ticket_error);
			break;
		}
		::OS::get_singleton()->delay_usec(1000);
	}
}

Ref<SteamAuthResult> Steam::authenticate_with_server(const String &p_url, const String &p_ticket, int p_app_id) {
	_log_debug(vformat("Authenticating ticket with server: %s", p_url));
	return auth_client->authenticate_sync(p_url, p_ticket, p_app_id, debug_logging);
}

void Steam::cancel_auth_ticket(int p_handle) {
	if (!initialized || !steam_user || p_handle == 0) {
		return;
	}
	loader.cancel_auth_ticket(steam_user, (SteamAPILoader::HAuthTicket)p_handle);
	if (pending_auth_ticket == p_handle) {
		_reset_ticket_state();
	}
}

uint64_t Steam::get_local_steam_id() const {
	if (!initialized || !steam_user) {
		return 0;
	}
	return loader.get_steam_id(steam_user);
}

bool Steam::is_logged_on() const {
	if (!initialized || !steam_user) {
		return false;
	}
	return loader.is_logged_on(steam_user);
}

bool Steam::_ensure_stats_ready() const {
	return initialized && steam_user_stats && steam_friends && steam_utils && loader.has_stats_support();
}

Ref<Image> Steam::_image_from_steam_handle(int p_image) const {
	if (!steam_utils) {
		return Ref<Image>();
	}
	return loader.image_from_steam_handle(steam_utils, p_image);
}

bool Steam::_probe_stats_loaded() {
	if (!_ensure_stats_ready()) {
		return false;
	}

	_dispatch_callbacks();
	if (stats_received) {
		return true;
	}

	const uint32_t count = loader.get_num_achievements(steam_user_stats);
	if (count == 0) {
		return false;
	}

	stats_received = true;
	_log_debug(vformat("User stats available (%d achievements)", count));
	return true;
}

bool Steam::_wait_for_user_stats(double p_timeout_sec, bool p_require_fresh) {
	if (!p_require_fresh && _probe_stats_loaded()) {
		return true;
	}

	if (p_require_fresh) {
		stats_received = false;
	}

	const double start_usec = Time::get_singleton()->get_ticks_usec();
	while (!stats_received) {
		_dispatch_callbacks();
		if ((Time::get_singleton()->get_ticks_usec() - start_usec) / 1000000.0 > p_timeout_sec) {
			if (p_require_fresh) {
				_log_debug("Timed out waiting for fresh user stats; using cached data");
			} else {
				_log_debug("Timed out waiting for user stats; proceeding with cached data");
			}
			return true;
		}
		::OS::get_singleton()->delay_usec(1000);
	}
	return true;
}

bool Steam::request_current_stats() {
	if (!_ensure_stats_ready()) {
		_log_debug("request_current_stats unavailable");
		return false;
	}

	return _wait_for_user_stats(10.0, false);
}

bool Steam::refresh_current_stats() {
	if (!_ensure_stats_ready()) {
		_log_debug("refresh_current_stats unavailable");
		return false;
	}

	return _wait_for_user_stats(10.0, true);
}

bool Steam::store_stats() {
	if (!_ensure_stats_ready()) {
		_log_debug("store_stats unavailable");
		return false;
	}

	stats_store_pending = true;
	stats_store_succeeded = false;
	if (!loader.store_stats(steam_user_stats)) {
		_log_debug("StoreStats call failed");
		stats_store_pending = false;
		return false;
	}

	const double start_usec = Time::get_singleton()->get_ticks_usec();
	while (stats_store_pending) {
		_dispatch_callbacks();
		if ((Time::get_singleton()->get_ticks_usec() - start_usec) / 1000000.0 > 10.0) {
			_log_debug("Timed out waiting for UserStatsStored");
			return false;
		}
		::OS::get_singleton()->delay_usec(1000);
	}
	return stats_store_succeeded;
}

bool Steam::set_achievement(const String &p_name) {
	if (!_ensure_stats_ready() || p_name.is_empty()) {
		return false;
	}
	CharString name_utf8 = p_name.utf8();
	const bool ok = loader.set_achievement(steam_user_stats, name_utf8.get_data());
	_log_debug(vformat("set_achievement(%s) -> %s", p_name, ok ? "true" : "false"));
	return ok;
}

bool Steam::clear_achievement(const String &p_name) {
	if (!_ensure_stats_ready() || p_name.is_empty()) {
		return false;
	}
	CharString name_utf8 = p_name.utf8();
	const bool ok = loader.clear_achievement(steam_user_stats, name_utf8.get_data());
	_log_debug(vformat("clear_achievement(%s) -> %s", p_name, ok ? "true" : "false"));
	return ok;
}

bool Steam::get_achievement(const String &p_name) const {
	if (!_ensure_stats_ready() || p_name.is_empty()) {
		return false;
	}
	CharString name_utf8 = p_name.utf8();
	bool achieved = false;
	if (!loader.get_achievement(steam_user_stats, name_utf8.get_data(), achieved)) {
		return false;
	}
	return achieved;
}

Ref<SteamAchievementInfo> Steam::_build_achievement_info(const String &p_name, bool p_include_icon) const {
	Ref<SteamAchievementInfo> info;
	if (!_ensure_stats_ready() || p_name.is_empty()) {
		return info;
	}

	info.instantiate();
	info->set_api_name(p_name);

	CharString name_utf8 = p_name.utf8();
	const char *name_cstr = name_utf8.get_data();

	bool achieved = false;
	uint32_t unlock_time = 0;
	if (loader.get_achievement_and_unlock_time(steam_user_stats, name_cstr, achieved, unlock_time)) {
		info->set_achieved(achieved);
		info->set_unlock_time((int)unlock_time);
	}

	info->set_display_name(loader.get_achievement_display_attribute(steam_user_stats, name_cstr, "name"));
	info->set_description(loader.get_achievement_display_attribute(steam_user_stats, name_cstr, "desc"));
	const String hidden_attr = loader.get_achievement_display_attribute(steam_user_stats, name_cstr, "hidden");
	info->set_hidden(hidden_attr == "1" || hidden_attr.to_lower() == "true");

	if (p_include_icon) {
		const int icon_handle = loader.get_achievement_icon(steam_user_stats, name_cstr);
		if (icon_handle > 0) {
			info->set_icon(_image_from_steam_handle(icon_handle));
		}
	}

	return info;
}

Ref<SteamAchievementInfo> Steam::get_achievement_info(const String &p_name, bool p_include_icon) {
	return _build_achievement_info(p_name, p_include_icon);
}

Array Steam::get_all_achievements(bool p_include_icons) {
	Array out;
	if (!_ensure_stats_ready()) {
		return out;
	}

	const uint32_t count = loader.get_num_achievements(steam_user_stats);
	for (uint32_t i = 0; i < count; i++) {
		const String name = loader.get_achievement_name(steam_user_stats, i);
		if (name.is_empty()) {
			continue;
		}
		Ref<SteamAchievementInfo> info = _build_achievement_info(name, p_include_icons);
		if (info.is_valid()) {
			out.push_back(info);
		}
	}
	return out;
}

bool Steam::_get_stat_int(const String &p_name, int &r_value) const {
	if (!_ensure_stats_ready() || p_name.is_empty()) {
		return false;
	}
	CharString name_utf8 = p_name.utf8();
	int32_t value = 0;
	if (!loader.get_stat_int32(steam_user_stats, name_utf8.get_data(), value)) {
		return false;
	}
	r_value = (int)value;
	return true;
}

int Steam::get_stat_int(const String &p_name) const {
	int value = 0;
	if (!_get_stat_int(p_name, value)) {
		return -1;
	}
	return value;
}

float Steam::get_stat_float(const String &p_name) const {
	if (!_ensure_stats_ready() || p_name.is_empty()) {
		return 0.0f;
	}
	CharString name_utf8 = p_name.utf8();
	float value = 0.0f;
	if (!loader.get_stat_float(steam_user_stats, name_utf8.get_data(), value)) {
		return 0.0f;
	}
	return value;
}

bool Steam::set_stat_int(const String &p_name, int p_value) {
	if (!_ensure_stats_ready() || p_name.is_empty()) {
		return false;
	}
	CharString name_utf8 = p_name.utf8();
	const bool ok = loader.set_stat_int32(steam_user_stats, name_utf8.get_data(), (int32_t)p_value);
	_log_debug(vformat("set_stat_int(%s, %d) -> %s", p_name, p_value, ok ? "true" : "false"));
	return ok;
}

bool Steam::set_stat_float(const String &p_name, float p_value) {
	if (!_ensure_stats_ready() || p_name.is_empty()) {
		return false;
	}
	CharString name_utf8 = p_name.utf8();
	const bool ok = loader.set_stat_float(steam_user_stats, name_utf8.get_data(), p_value);
	_log_debug(vformat("set_stat_float(%s, %f) -> %s", p_name, p_value, ok ? "true" : "false"));
	return ok;
}

bool Steam::increment_stat_int(const String &p_name, int p_delta) {
	if (p_delta != 1) {
		_log_debug(vformat("increment_stat_int(%s, %d) rejected: delta must be 1", p_name, p_delta));
		return false;
	}
	_dispatch_callbacks();
	int current = 0;
	if (!_get_stat_int(p_name, current)) {
		return false;
	}
	return set_stat_int(p_name, current + 1);
}

bool Steam::clear_stat(const String &p_name) {
	return set_stat_int(p_name, 0);
}

String Steam::get_persona_name() const {
	if (!_ensure_stats_ready()) {
		return String();
	}
	return loader.get_persona_name(steam_friends);
}

Ref<Image> Steam::get_avatar_image(uint64_t p_steam_id, AvatarSize p_size) {
	if (!_ensure_stats_ready()) {
		return Ref<Image>();
	}

	uint64_t steam_id = p_steam_id;
	if (steam_id == 0) {
		steam_id = get_local_steam_id();
	}
	if (steam_id == 0) {
		return Ref<Image>();
	}

	const int avatar_handle = loader.get_friend_avatar(steam_friends, steam_id, (int)p_size);
	if (avatar_handle <= 0) {
		// Avatar may load asynchronously; pump callbacks briefly.
		const double start_usec = Time::get_singleton()->get_ticks_usec();
		int handle = avatar_handle;
		while (handle <= 0 && (Time::get_singleton()->get_ticks_usec() - start_usec) / 1000000.0 < 3.0) {
			_dispatch_callbacks();
			handle = loader.get_friend_avatar(steam_friends, steam_id, (int)p_size);
			::OS::get_singleton()->delay_usec(10000);
		}
		if (handle <= 0) {
			return Ref<Image>();
		}
		return _image_from_steam_handle(handle);
	}
	return _image_from_steam_handle(avatar_handle);
}
