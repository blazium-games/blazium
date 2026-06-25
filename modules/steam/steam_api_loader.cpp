/**************************************************************************/
/*  steam_api_loader.cpp                                                  */
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

#include "steam_api_loader.h"

#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/os/os.h"
#include "core/variant/dictionary.h"

#include <cstring>

bool SteamAPILoader::_load_symbol(const char *p_name, void *&r_symbol) {
	r_symbol = nullptr;
	if (!library_handle) {
		return false;
	}
	Error err = OS::get_singleton()->get_dynamic_library_symbol_handle(library_handle, p_name, r_symbol);
	return err == OK && r_symbol != nullptr;
}

bool SteamAPILoader::try_load() {
	if (loaded) {
		return true;
	}

	Vector<String> candidates;
#ifdef WINDOWS_ENABLED
	candidates.push_back("steam_api64.dll");
	candidates.push_back("steam_api.dll");
#else
	candidates.push_back("libsteam_api.so");
#endif

	String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
	for (int i = 0; i < candidates.size(); i++) {
		String path = exe_dir.path_join(candidates[i]);
		if (!FileAccess::exists(path)) {
			path = candidates[i];
		}
		if (!FileAccess::exists(path)) {
			continue;
		}
		Error err = OS::get_singleton()->open_dynamic_library(path, library_handle);
		if (err == OK) {
			break;
		}
	}

	if (!library_handle) {
		return false;
	}

	void *symbol = nullptr;
	if (!_load_symbol("SteamAPI_InitFlat", symbol)) {
		unload();
		return false;
	}
	fn_init_flat = (SteamAPI_InitFlatFn)symbol;

	if (!_load_symbol("SteamAPI_Shutdown", symbol)) {
		unload();
		return false;
	}
	fn_shutdown = (SteamAPI_ShutdownFn)symbol;

	if (!_load_symbol("SteamAPI_GetHSteamPipe", symbol)) {
		unload();
		return false;
	}
	fn_get_h_steam_pipe = (SteamAPI_GetHSteamPipeFn)symbol;

	_load_symbol("SteamAPI_RunCallbacks", symbol);
	fn_run_callbacks = (SteamAPI_RunCallbacksFn)symbol;

	if (!_load_symbol("SteamAPI_ManualDispatch_Init", symbol)) {
		unload();
		return false;
	}
	fn_manual_dispatch_init = (SteamAPI_ManualDispatch_InitFn)symbol;

	if (!_load_symbol("SteamAPI_ManualDispatch_RunFrame", symbol)) {
		unload();
		return false;
	}
	fn_manual_dispatch_run_frame = (SteamAPI_ManualDispatch_RunFrameFn)symbol;

	if (!_load_symbol("SteamAPI_ManualDispatch_GetNextCallback", symbol)) {
		unload();
		return false;
	}
	fn_manual_dispatch_get_next_callback = (SteamAPI_ManualDispatch_GetNextCallbackFn)symbol;

	if (!_load_symbol("SteamAPI_ManualDispatch_FreeLastCallback", symbol)) {
		unload();
		return false;
	}
	fn_manual_dispatch_free_last_callback = (SteamAPI_ManualDispatch_FreeLastCallbackFn)symbol;

	if (!_load_symbol("SteamAPI_ManualDispatch_GetAPICallResult", symbol)) {
		unload();
		return false;
	}
	fn_manual_dispatch_get_api_call_result = (SteamAPI_ManualDispatch_GetAPICallResultFn)symbol;

	_load_symbol("SteamAPI_IsSteamRunning", symbol);
	fn_is_steam_running = (SteamAPI_IsSteamRunningFn)symbol;

#ifdef WINDOWS_ENABLED
	if (!_load_symbol("SteamAPI_SteamUser_v023", symbol)) {
		unload();
		return false;
	}
#else
	if (!_load_symbol("SteamAPI_SteamUser_v023", symbol)) {
		unload();
		return false;
	}
#endif
	fn_steam_user = (SteamAPI_SteamUserFn)symbol;

	if (!_load_symbol("SteamAPI_ISteamUser_GetAuthTicketForWebApi", symbol)) {
		unload();
		return false;
	}
	fn_get_auth_ticket_for_web_api = (ISteamUser_GetAuthTicketForWebApiFn)symbol;

	if (!_load_symbol("SteamAPI_ISteamUser_CancelAuthTicket", symbol)) {
		unload();
		return false;
	}
	fn_cancel_auth_ticket = (ISteamUser_CancelAuthTicketFn)symbol;

	if (!_load_symbol("SteamAPI_ISteamUser_BLoggedOn", symbol)) {
		unload();
		return false;
	}
	fn_b_logged_on = (ISteamUser_BLoggedOnFn)symbol;

	if (!_load_symbol("SteamAPI_ISteamUser_GetSteamID", symbol)) {
		unload();
		return false;
	}
	fn_get_steam_id = (ISteamUser_GetSteamIDFn)symbol;

	loaded = true;
	stats_loaded = _load_stats_symbols();
	inventory_loaded = _load_inventory_symbols();
	return true;
}

bool SteamAPILoader::_load_stats_symbols() {
	void *symbol = nullptr;

	if (!_load_symbol("SteamAPI_SteamUserStats_v013", symbol)) {
		return false;
	}
	fn_steam_user_stats = (SteamAPI_SteamUserStatsFn)symbol;

	if (!_load_symbol("SteamAPI_ISteamUserStats_GetAchievement", symbol)) {
		return false;
	}
	fn_get_achievement = (ISteamUserStats_GetAchievementFn)symbol;

	if (!_load_symbol("SteamAPI_ISteamUserStats_SetAchievement", symbol)) {
		return false;
	}
	fn_set_achievement = (ISteamUserStats_SetAchievementFn)symbol;

	if (!_load_symbol("SteamAPI_ISteamUserStats_ClearAchievement", symbol)) {
		return false;
	}
	fn_clear_achievement = (ISteamUserStats_ClearAchievementFn)symbol;

	if (!_load_symbol("SteamAPI_ISteamUserStats_StoreStats", symbol)) {
		return false;
	}
	fn_store_stats = (ISteamUserStats_StoreStatsFn)symbol;

	if (!_load_symbol("SteamAPI_ISteamUserStats_GetAchievementAndUnlockTime", symbol)) {
		return false;
	}
	fn_get_achievement_and_unlock_time = (ISteamUserStats_GetAchievementAndUnlockTimeFn)symbol;

	if (!_load_symbol("SteamAPI_ISteamUserStats_GetAchievementIcon", symbol)) {
		return false;
	}
	fn_get_achievement_icon = (ISteamUserStats_GetAchievementIconFn)symbol;

	if (!_load_symbol("SteamAPI_ISteamUserStats_GetAchievementDisplayAttribute", symbol)) {
		return false;
	}
	fn_get_achievement_display_attribute = (ISteamUserStats_GetAchievementDisplayAttributeFn)symbol;

	if (!_load_symbol("SteamAPI_ISteamUserStats_GetNumAchievements", symbol)) {
		return false;
	}
	fn_get_num_achievements = (ISteamUserStats_GetNumAchievementsFn)symbol;

	if (!_load_symbol("SteamAPI_ISteamUserStats_GetAchievementName", symbol)) {
		return false;
	}
	fn_get_achievement_name = (ISteamUserStats_GetAchievementNameFn)symbol;

	if (!_load_symbol("SteamAPI_ISteamUserStats_GetStatInt32", symbol)) {
		return false;
	}
	fn_get_stat_int32 = (ISteamUserStats_GetStatInt32Fn)symbol;

	if (!_load_symbol("SteamAPI_ISteamUserStats_GetStatFloat", symbol)) {
		return false;
	}
	fn_get_stat_float = (ISteamUserStats_GetStatFloatFn)symbol;

	if (!_load_symbol("SteamAPI_ISteamUserStats_SetStatInt32", symbol)) {
		return false;
	}
	fn_set_stat_int32 = (ISteamUserStats_SetStatInt32Fn)symbol;

	if (!_load_symbol("SteamAPI_ISteamUserStats_SetStatFloat", symbol)) {
		return false;
	}
	fn_set_stat_float = (ISteamUserStats_SetStatFloatFn)symbol;

	if (!_load_symbol("SteamAPI_ISteamUserStats_UpdateAvgRateStat", symbol)) {
		return false;
	}
	fn_update_avg_rate_stat = (ISteamUserStats_UpdateAvgRateStatFn)symbol;

	if (!_load_symbol("SteamAPI_SteamFriends_v018", symbol)) {
		return false;
	}
	fn_steam_friends = (SteamAPI_SteamFriendsFn)symbol;

	if (!_load_symbol("SteamAPI_ISteamFriends_GetPersonaName", symbol)) {
		return false;
	}
	fn_get_persona_name = (ISteamFriends_GetPersonaNameFn)symbol;

	if (!_load_symbol("SteamAPI_ISteamFriends_GetSmallFriendAvatar", symbol)) {
		return false;
	}
	fn_get_small_friend_avatar = (ISteamFriends_GetSmallFriendAvatarFn)symbol;

	if (!_load_symbol("SteamAPI_ISteamFriends_GetMediumFriendAvatar", symbol)) {
		return false;
	}
	fn_get_medium_friend_avatar = (ISteamFriends_GetMediumFriendAvatarFn)symbol;

	if (!_load_symbol("SteamAPI_ISteamFriends_GetLargeFriendAvatar", symbol)) {
		return false;
	}
	fn_get_large_friend_avatar = (ISteamFriends_GetLargeFriendAvatarFn)symbol;

	if (!_load_symbol("SteamAPI_SteamUtils_v010", symbol)) {
		return false;
	}
	fn_steam_utils = (SteamAPI_SteamUtilsFn)symbol;

	if (!_load_symbol("SteamAPI_ISteamUtils_GetImageSize", symbol)) {
		return false;
	}
	fn_get_image_size = (ISteamUtils_GetImageSizeFn)symbol;

	if (!_load_symbol("SteamAPI_ISteamUtils_GetImageRGBA", symbol)) {
		return false;
	}
	fn_get_image_rgba = (ISteamUtils_GetImageRGBAFn)symbol;

	return true;
}

bool SteamAPILoader::_load_inventory_symbols() {
	void *symbol = nullptr;

	if (!_load_symbol("SteamAPI_SteamInventory_v003", symbol)) {
		return false;
	}
	fn_steam_inventory = (SteamAPI_SteamInventoryFn)symbol;

#define LOAD_INV_SYM(name, field, type) \
	if (!_load_symbol(name, symbol)) {  \
		return false;                   \
	}                                   \
	field = (type)symbol;

	LOAD_INV_SYM("SteamAPI_ISteamInventory_GetResultStatus", fn_inventory_get_result_status, ISteamInventory_GetResultStatusFn);
	LOAD_INV_SYM("SteamAPI_ISteamInventory_GetResultItems", fn_inventory_get_result_items, ISteamInventory_GetResultItemsFn);
	LOAD_INV_SYM("SteamAPI_ISteamInventory_GetResultItemProperty", fn_inventory_get_result_item_property, ISteamInventory_GetResultItemPropertyFn);
	LOAD_INV_SYM("SteamAPI_ISteamInventory_GetResultTimestamp", fn_inventory_get_result_timestamp, ISteamInventory_GetResultTimestampFn);
	LOAD_INV_SYM("SteamAPI_ISteamInventory_CheckResultSteamID", fn_inventory_check_result_steam_id, ISteamInventory_CheckResultSteamIDFn);
	LOAD_INV_SYM("SteamAPI_ISteamInventory_DestroyResult", fn_inventory_destroy_result, ISteamInventory_DestroyResultFn);
	LOAD_INV_SYM("SteamAPI_ISteamInventory_GetAllItems", fn_inventory_get_all_items, ISteamInventory_GetAllItemsFn);
	LOAD_INV_SYM("SteamAPI_ISteamInventory_GetItemsByID", fn_inventory_get_items_by_id, ISteamInventory_GetItemsByIDFn);
	LOAD_INV_SYM("SteamAPI_ISteamInventory_SerializeResult", fn_inventory_serialize_result, ISteamInventory_SerializeResultFn);
	LOAD_INV_SYM("SteamAPI_ISteamInventory_DeserializeResult", fn_inventory_deserialize_result, ISteamInventory_DeserializeResultFn);
	LOAD_INV_SYM("SteamAPI_ISteamInventory_GrantPromoItems", fn_inventory_grant_promo_items, ISteamInventory_GrantPromoItemsFn);
	LOAD_INV_SYM("SteamAPI_ISteamInventory_AddPromoItem", fn_inventory_add_promo_item, ISteamInventory_AddPromoItemFn);
	LOAD_INV_SYM("SteamAPI_ISteamInventory_AddPromoItems", fn_inventory_add_promo_items, ISteamInventory_AddPromoItemsFn);
	LOAD_INV_SYM("SteamAPI_ISteamInventory_ConsumeItem", fn_inventory_consume_item, ISteamInventory_ConsumeItemFn);
	LOAD_INV_SYM("SteamAPI_ISteamInventory_ExchangeItems", fn_inventory_exchange_items, ISteamInventory_ExchangeItemsFn);
	LOAD_INV_SYM("SteamAPI_ISteamInventory_TransferItemQuantity", fn_inventory_transfer_item_quantity, ISteamInventory_TransferItemQuantityFn);
	LOAD_INV_SYM("SteamAPI_ISteamInventory_LoadItemDefinitions", fn_inventory_load_item_definitions, ISteamInventory_LoadItemDefinitionsFn);
	LOAD_INV_SYM("SteamAPI_ISteamInventory_GetItemDefinitionIDs", fn_inventory_get_item_definition_ids, ISteamInventory_GetItemDefinitionIDsFn);
	LOAD_INV_SYM("SteamAPI_ISteamInventory_GetItemDefinitionProperty", fn_inventory_get_item_definition_property, ISteamInventory_GetItemDefinitionPropertyFn);
	LOAD_INV_SYM("SteamAPI_ISteamInventory_StartUpdateProperties", fn_inventory_start_update_properties, ISteamInventory_StartUpdatePropertiesFn);
	LOAD_INV_SYM("SteamAPI_ISteamInventory_RemoveProperty", fn_inventory_remove_property, ISteamInventory_RemovePropertyFn);
	LOAD_INV_SYM("SteamAPI_ISteamInventory_SetPropertyString", fn_inventory_set_property_string, ISteamInventory_SetPropertyStringFn);
	LOAD_INV_SYM("SteamAPI_ISteamInventory_SetPropertyBool", fn_inventory_set_property_bool, ISteamInventory_SetPropertyBoolFn);
	LOAD_INV_SYM("SteamAPI_ISteamInventory_SetPropertyInt64", fn_inventory_set_property_int64, ISteamInventory_SetPropertyInt64Fn);
	LOAD_INV_SYM("SteamAPI_ISteamInventory_SetPropertyFloat", fn_inventory_set_property_float, ISteamInventory_SetPropertyFloatFn);
	LOAD_INV_SYM("SteamAPI_ISteamInventory_SubmitUpdateProperties", fn_inventory_submit_update_properties, ISteamInventory_SubmitUpdatePropertiesFn);
	LOAD_INV_SYM("SteamAPI_ISteamInventory_InspectItem", fn_inventory_inspect_item, ISteamInventory_InspectItemFn);
	LOAD_INV_SYM("SteamAPI_ISteamInventory_TriggerItemDrop", fn_inventory_trigger_item_drop, ISteamInventory_TriggerItemDropFn);
	LOAD_INV_SYM("SteamAPI_ISteamInventory_GetEligiblePromoItemDefinitionIDs", fn_inventory_get_eligible_promo_item_definition_ids, ISteamInventory_GetEligiblePromoItemDefinitionIDsFn);

#undef LOAD_INV_SYM
	return true;
}

void SteamAPILoader::unload() {
	if (library_handle) {
		OS::get_singleton()->close_dynamic_library(library_handle);
		library_handle = nullptr;
	}
	loaded = false;
	fn_init_flat = nullptr;
	fn_shutdown = nullptr;
	fn_get_h_steam_pipe = nullptr;
	fn_run_callbacks = nullptr;
	fn_manual_dispatch_init = nullptr;
	fn_manual_dispatch_run_frame = nullptr;
	fn_manual_dispatch_get_next_callback = nullptr;
	fn_manual_dispatch_free_last_callback = nullptr;
	fn_manual_dispatch_get_api_call_result = nullptr;
	fn_is_steam_running = nullptr;
	fn_steam_user = nullptr;
	fn_get_auth_ticket_for_web_api = nullptr;
	fn_cancel_auth_ticket = nullptr;
	fn_b_logged_on = nullptr;
	fn_get_steam_id = nullptr;
	stats_loaded = false;
	inventory_loaded = false;
	fn_steam_user_stats = nullptr;
	fn_request_current_stats = nullptr;
	fn_get_achievement = nullptr;
	fn_set_achievement = nullptr;
	fn_clear_achievement = nullptr;
	fn_store_stats = nullptr;
	fn_get_achievement_and_unlock_time = nullptr;
	fn_get_achievement_icon = nullptr;
	fn_get_achievement_display_attribute = nullptr;
	fn_get_num_achievements = nullptr;
	fn_get_achievement_name = nullptr;
	fn_get_stat_int32 = nullptr;
	fn_get_stat_float = nullptr;
	fn_set_stat_int32 = nullptr;
	fn_set_stat_float = nullptr;
	fn_update_avg_rate_stat = nullptr;
	fn_steam_friends = nullptr;
	fn_get_persona_name = nullptr;
	fn_get_small_friend_avatar = nullptr;
	fn_get_medium_friend_avatar = nullptr;
	fn_get_large_friend_avatar = nullptr;
	fn_steam_utils = nullptr;
	fn_get_image_size = nullptr;
	fn_get_image_rgba = nullptr;
	fn_steam_inventory = nullptr;
	fn_inventory_get_result_status = nullptr;
	fn_inventory_get_result_items = nullptr;
	fn_inventory_get_result_item_property = nullptr;
	fn_inventory_get_result_timestamp = nullptr;
	fn_inventory_check_result_steam_id = nullptr;
	fn_inventory_destroy_result = nullptr;
	fn_inventory_get_all_items = nullptr;
	fn_inventory_get_items_by_id = nullptr;
	fn_inventory_serialize_result = nullptr;
	fn_inventory_deserialize_result = nullptr;
	fn_inventory_grant_promo_items = nullptr;
	fn_inventory_add_promo_item = nullptr;
	fn_inventory_add_promo_items = nullptr;
	fn_inventory_consume_item = nullptr;
	fn_inventory_exchange_items = nullptr;
	fn_inventory_transfer_item_quantity = nullptr;
	fn_inventory_load_item_definitions = nullptr;
	fn_inventory_get_item_definition_ids = nullptr;
	fn_inventory_get_item_definition_property = nullptr;
	fn_inventory_start_update_properties = nullptr;
	fn_inventory_remove_property = nullptr;
	fn_inventory_set_property_string = nullptr;
	fn_inventory_set_property_bool = nullptr;
	fn_inventory_set_property_int64 = nullptr;
	fn_inventory_set_property_float = nullptr;
	fn_inventory_submit_update_properties = nullptr;
	fn_inventory_inspect_item = nullptr;
	fn_inventory_trigger_item_drop = nullptr;
	fn_inventory_get_eligible_promo_item_definition_ids = nullptr;
}

int SteamAPILoader::init_flat(String &r_err_msg) {
	if (!fn_init_flat) {
		r_err_msg = "Steam API not loaded";
		return 1;
	}
	char err_msg[1024];
	err_msg[0] = '\0';
	int result = fn_init_flat(err_msg);
	if (result != 0) {
		r_err_msg = String::utf8(err_msg);
	}
	return result;
}

void SteamAPILoader::shutdown() {
	if (fn_shutdown) {
		fn_shutdown();
	}
}

SteamAPILoader::HSteamPipe SteamAPILoader::get_h_steam_pipe() const {
	return fn_get_h_steam_pipe ? fn_get_h_steam_pipe() : 0;
}

void SteamAPILoader::run_callbacks() {
	if (fn_run_callbacks) {
		fn_run_callbacks();
	}
}

void SteamAPILoader::manual_dispatch_init() {
	if (fn_manual_dispatch_init) {
		fn_manual_dispatch_init();
	}
}

void SteamAPILoader::manual_dispatch_run_frame(HSteamPipe p_pipe) {
	if (fn_manual_dispatch_run_frame) {
		fn_manual_dispatch_run_frame(p_pipe);
	}
}

bool SteamAPILoader::manual_dispatch_get_next_callback(HSteamPipe p_pipe, void *p_callback_msg) {
	return fn_manual_dispatch_get_next_callback && fn_manual_dispatch_get_next_callback(p_pipe, p_callback_msg);
}

void SteamAPILoader::manual_dispatch_free_last_callback(HSteamPipe p_pipe) {
	if (fn_manual_dispatch_free_last_callback) {
		fn_manual_dispatch_free_last_callback(p_pipe);
	}
}

bool SteamAPILoader::manual_dispatch_get_api_call_result(HSteamPipe p_pipe, uint64_t p_async_call, void *p_callback, int p_cub_callback, int p_callback_expected, bool *p_failed) {
	return fn_manual_dispatch_get_api_call_result && fn_manual_dispatch_get_api_call_result(p_pipe, p_async_call, p_callback, p_cub_callback, p_callback_expected, p_failed);
}

bool SteamAPILoader::is_steam_running() const {
	return fn_is_steam_running && fn_is_steam_running();
}

SteamAPILoader::ISteamUserPtr SteamAPILoader::get_steam_user() const {
	return fn_steam_user ? fn_steam_user() : nullptr;
}

SteamAPILoader::HAuthTicket SteamAPILoader::get_auth_ticket_for_web_api(ISteamUserPtr p_user, const char *p_identity) const {
	return fn_get_auth_ticket_for_web_api ? fn_get_auth_ticket_for_web_api(p_user, p_identity) : 0;
}

void SteamAPILoader::cancel_auth_ticket(ISteamUserPtr p_user, HAuthTicket p_ticket) const {
	if (fn_cancel_auth_ticket) {
		fn_cancel_auth_ticket(p_user, p_ticket);
	}
}

bool SteamAPILoader::is_logged_on(ISteamUserPtr p_user) const {
	return fn_b_logged_on && fn_b_logged_on(p_user);
}

uint64_t SteamAPILoader::get_steam_id(ISteamUserPtr p_user) const {
	return fn_get_steam_id ? fn_get_steam_id(p_user) : 0;
}

String SteamAPILoader::bytes_to_hex(const uint8_t *p_data, int p_size) {
	String hex;
	for (int i = 0; i < p_size; i++) {
		hex += vformat("%02x", p_data[i]);
	}
	return hex;
}

SteamAPILoader::ISteamUserStatsPtr SteamAPILoader::get_steam_user_stats() const {
	return fn_steam_user_stats ? fn_steam_user_stats() : nullptr;
}

bool SteamAPILoader::request_current_stats(ISteamUserStatsPtr p_stats) const {
	return fn_request_current_stats && fn_request_current_stats(p_stats);
}

bool SteamAPILoader::get_achievement(ISteamUserStatsPtr p_stats, const char *p_name, bool &r_achieved) const {
	if (!fn_get_achievement) {
		return false;
	}
	return fn_get_achievement(p_stats, p_name, &r_achieved);
}

bool SteamAPILoader::set_achievement(ISteamUserStatsPtr p_stats, const char *p_name) const {
	return fn_set_achievement && fn_set_achievement(p_stats, p_name);
}

bool SteamAPILoader::clear_achievement(ISteamUserStatsPtr p_stats, const char *p_name) const {
	return fn_clear_achievement && fn_clear_achievement(p_stats, p_name);
}

bool SteamAPILoader::store_stats(ISteamUserStatsPtr p_stats) const {
	return fn_store_stats && fn_store_stats(p_stats);
}

bool SteamAPILoader::get_achievement_and_unlock_time(ISteamUserStatsPtr p_stats, const char *p_name, bool &r_achieved, uint32_t &r_unlock_time) const {
	if (!fn_get_achievement_and_unlock_time) {
		return false;
	}
	return fn_get_achievement_and_unlock_time(p_stats, p_name, &r_achieved, &r_unlock_time);
}

int SteamAPILoader::get_achievement_icon(ISteamUserStatsPtr p_stats, const char *p_name) const {
	return fn_get_achievement_icon ? fn_get_achievement_icon(p_stats, p_name) : 0;
}

String SteamAPILoader::get_achievement_display_attribute(ISteamUserStatsPtr p_stats, const char *p_name, const char *p_key) const {
	if (!fn_get_achievement_display_attribute) {
		return String();
	}
	const char *value = fn_get_achievement_display_attribute(p_stats, p_name, p_key);
	return value ? String::utf8(value) : String();
}

uint32_t SteamAPILoader::get_num_achievements(ISteamUserStatsPtr p_stats) const {
	return fn_get_num_achievements ? fn_get_num_achievements(p_stats) : 0;
}

String SteamAPILoader::get_achievement_name(ISteamUserStatsPtr p_stats, uint32_t p_index) const {
	if (!fn_get_achievement_name) {
		return String();
	}
	const char *name = fn_get_achievement_name(p_stats, p_index);
	return name ? String::utf8(name) : String();
}

bool SteamAPILoader::get_stat_int32(ISteamUserStatsPtr p_stats, const char *p_name, int32_t &r_data) const {
	if (!fn_get_stat_int32) {
		return false;
	}
	return fn_get_stat_int32(p_stats, p_name, &r_data);
}

bool SteamAPILoader::get_stat_float(ISteamUserStatsPtr p_stats, const char *p_name, float &r_data) const {
	if (!fn_get_stat_float) {
		return false;
	}
	return fn_get_stat_float(p_stats, p_name, &r_data);
}

bool SteamAPILoader::set_stat_int32(ISteamUserStatsPtr p_stats, const char *p_name, int32_t p_data) const {
	return fn_set_stat_int32 && fn_set_stat_int32(p_stats, p_name, p_data);
}

bool SteamAPILoader::set_stat_float(ISteamUserStatsPtr p_stats, const char *p_name, float p_data) const {
	return fn_set_stat_float && fn_set_stat_float(p_stats, p_name, p_data);
}

bool SteamAPILoader::update_avg_rate_stat(ISteamUserStatsPtr p_stats, const char *p_name, float p_count_this_session, double p_session_length) const {
	return fn_update_avg_rate_stat && fn_update_avg_rate_stat(p_stats, p_name, p_count_this_session, p_session_length);
}

SteamAPILoader::ISteamFriendsPtr SteamAPILoader::get_steam_friends() const {
	return fn_steam_friends ? fn_steam_friends() : nullptr;
}

String SteamAPILoader::get_persona_name(ISteamFriendsPtr p_friends) const {
	if (!fn_get_persona_name) {
		return String();
	}
	const char *name = fn_get_persona_name(p_friends);
	return name ? String::utf8(name) : String();
}

int SteamAPILoader::get_friend_avatar(ISteamFriendsPtr p_friends, uint64_t p_steam_id, int p_size) const {
	if (!p_friends) {
		return 0;
	}
	switch (p_size) {
		case 0:
			return fn_get_small_friend_avatar ? fn_get_small_friend_avatar(p_friends, p_steam_id) : 0;
		case 1:
			return fn_get_medium_friend_avatar ? fn_get_medium_friend_avatar(p_friends, p_steam_id) : 0;
		case 2:
		default:
			return fn_get_large_friend_avatar ? fn_get_large_friend_avatar(p_friends, p_steam_id) : 0;
	}
}

SteamAPILoader::ISteamUtilsPtr SteamAPILoader::get_steam_utils() const {
	return fn_steam_utils ? fn_steam_utils() : nullptr;
}

bool SteamAPILoader::get_image_size(ISteamUtilsPtr p_utils, int p_image, uint32_t &r_width, uint32_t &r_height) const {
	if (!fn_get_image_size || p_image <= 0) {
		return false;
	}
	return fn_get_image_size(p_utils, p_image, &r_width, &r_height);
}

bool SteamAPILoader::get_image_rgba(ISteamUtilsPtr p_utils, int p_image, uint8_t *p_dest, int p_dest_size) const {
	return fn_get_image_rgba && fn_get_image_rgba(p_utils, p_image, p_dest, p_dest_size);
}

Ref<Image> SteamAPILoader::image_from_rgba(const uint8_t *p_data, int p_width, int p_height) {
	if (!p_data || p_width <= 0 || p_height <= 0) {
		return Ref<Image>();
	}
	Vector<uint8_t> bytes;
	bytes.resize(p_width * p_height * 4);
	memcpy(bytes.ptrw(), p_data, bytes.size());
	Ref<Image> image;
	image.instantiate();
	image->set_data(p_width, p_height, false, Image::FORMAT_RGBA8, bytes);
	return image;
}

Ref<Image> SteamAPILoader::image_from_steam_handle(ISteamUtilsPtr p_utils, int p_image) const {
	if (!p_utils || p_image <= 0) {
		return Ref<Image>();
	}
	uint32_t width = 0;
	uint32_t height = 0;
	if (!get_image_size(p_utils, p_image, width, height) || width == 0 || height == 0) {
		return Ref<Image>();
	}
	const int64_t buffer_size = int64_t(width) * int64_t(height) * 4;
	Vector<uint8_t> rgba;
	rgba.resize((int)buffer_size);
	if (!get_image_rgba(p_utils, p_image, rgba.ptrw(), (int)buffer_size)) {
		return Ref<Image>();
	}
	return image_from_rgba(rgba.ptr(), (int)width, (int)height);
}

SteamAPILoader::ISteamInventoryPtr SteamAPILoader::get_steam_inventory() const {
	return fn_steam_inventory ? fn_steam_inventory() : nullptr;
}

int SteamAPILoader::inventory_get_result_status(ISteamInventoryPtr p_inventory, SteamInventoryResult_t p_result) const {
	return fn_inventory_get_result_status ? fn_inventory_get_result_status(p_inventory, p_result) : 0;
}

bool SteamAPILoader::inventory_get_result_items(ISteamInventoryPtr p_inventory, SteamInventoryResult_t p_result, Vector<SteamItemDetails> &r_items) const {
	r_items.clear();
	if (!fn_inventory_get_result_items) {
		return false;
	}
	uint32_t count = 0;
	if (!fn_inventory_get_result_items(p_inventory, p_result, nullptr, &count)) {
		return false;
	}
	if (count == 0) {
		return true;
	}
	r_items.resize((int)count);
	if (!fn_inventory_get_result_items(p_inventory, p_result, r_items.ptrw(), &count)) {
		r_items.clear();
		return false;
	}
	r_items.resize((int)count);
	return true;
}

String SteamAPILoader::inventory_get_result_item_property(ISteamInventoryPtr p_inventory, SteamInventoryResult_t p_result, uint32_t p_item_index, const char *p_property_name) const {
	if (!fn_inventory_get_result_item_property) {
		return String();
	}
	uint32_t buffer_size = 0;
	fn_inventory_get_result_item_property(p_inventory, p_result, p_item_index, p_property_name, nullptr, &buffer_size);
	if (buffer_size == 0) {
		return String();
	}
	Vector<char> buffer;
	buffer.resize((int)buffer_size);
	if (!fn_inventory_get_result_item_property(p_inventory, p_result, p_item_index, p_property_name, buffer.ptrw(), &buffer_size) || buffer_size == 0) {
		return String();
	}
	return String::utf8(buffer.ptr(), (int)buffer_size);
}

uint32_t SteamAPILoader::inventory_get_result_timestamp(ISteamInventoryPtr p_inventory, SteamInventoryResult_t p_result) const {
	return fn_inventory_get_result_timestamp ? fn_inventory_get_result_timestamp(p_inventory, p_result) : 0;
}

bool SteamAPILoader::inventory_check_result_steam_id(ISteamInventoryPtr p_inventory, SteamInventoryResult_t p_result, uint64_t p_steam_id) const {
	return fn_inventory_check_result_steam_id && fn_inventory_check_result_steam_id(p_inventory, p_result, p_steam_id);
}

void SteamAPILoader::inventory_destroy_result(ISteamInventoryPtr p_inventory, SteamInventoryResult_t p_result) const {
	if (fn_inventory_destroy_result) {
		fn_inventory_destroy_result(p_inventory, p_result);
	}
}

bool SteamAPILoader::inventory_get_all_items(ISteamInventoryPtr p_inventory, SteamInventoryResult_t &r_result) const {
	r_result = STEAM_INVENTORY_RESULT_INVALID;
	return fn_inventory_get_all_items && fn_inventory_get_all_items(p_inventory, &r_result);
}

bool SteamAPILoader::inventory_get_items_by_id(ISteamInventoryPtr p_inventory, SteamInventoryResult_t &r_result, const Vector<uint64_t> &p_instance_ids) const {
	r_result = STEAM_INVENTORY_RESULT_INVALID;
	if (!fn_inventory_get_items_by_id || p_instance_ids.is_empty()) {
		return false;
	}
	return fn_inventory_get_items_by_id(p_inventory, &r_result, (const SteamItemInstanceID_t *)p_instance_ids.ptr(), (uint32_t)p_instance_ids.size());
}

bool SteamAPILoader::inventory_serialize_result(ISteamInventoryPtr p_inventory, SteamInventoryResult_t p_result, PackedByteArray &r_bytes) const {
	r_bytes.clear();
	if (!fn_inventory_serialize_result) {
		return false;
	}
	uint32_t size = 0;
	if (!fn_inventory_serialize_result(p_inventory, p_result, nullptr, &size) || size == 0) {
		return false;
	}
	r_bytes.resize((int)size);
	return fn_inventory_serialize_result(p_inventory, p_result, r_bytes.ptrw(), &size);
}

bool SteamAPILoader::inventory_deserialize_result(ISteamInventoryPtr p_inventory, SteamInventoryResult_t &r_result, const PackedByteArray &p_bytes, bool p_reserved) const {
	r_result = STEAM_INVENTORY_RESULT_INVALID;
	if (!fn_inventory_deserialize_result || p_bytes.is_empty()) {
		return false;
	}
	return fn_inventory_deserialize_result(p_inventory, &r_result, p_bytes.ptr(), (uint32_t)p_bytes.size(), p_reserved);
}

bool SteamAPILoader::inventory_grant_promo_items(ISteamInventoryPtr p_inventory, SteamInventoryResult_t &r_result) const {
	r_result = STEAM_INVENTORY_RESULT_INVALID;
	return fn_inventory_grant_promo_items && fn_inventory_grant_promo_items(p_inventory, &r_result);
}

bool SteamAPILoader::inventory_add_promo_item(ISteamInventoryPtr p_inventory, SteamInventoryResult_t &r_result, SteamItemDef_t p_item_def) const {
	r_result = STEAM_INVENTORY_RESULT_INVALID;
	return fn_inventory_add_promo_item && fn_inventory_add_promo_item(p_inventory, &r_result, p_item_def);
}

bool SteamAPILoader::inventory_add_promo_items(ISteamInventoryPtr p_inventory, SteamInventoryResult_t &r_result, const Vector<int32_t> &p_item_defs) const {
	r_result = STEAM_INVENTORY_RESULT_INVALID;
	if (!fn_inventory_add_promo_items || p_item_defs.is_empty()) {
		return false;
	}
	return fn_inventory_add_promo_items(p_inventory, &r_result, (const SteamItemDef_t *)p_item_defs.ptr(), (uint32_t)p_item_defs.size());
}

bool SteamAPILoader::inventory_consume_item(ISteamInventoryPtr p_inventory, SteamInventoryResult_t &r_result, SteamItemInstanceID_t p_item, uint32_t p_quantity) const {
	r_result = STEAM_INVENTORY_RESULT_INVALID;
	return fn_inventory_consume_item && fn_inventory_consume_item(p_inventory, &r_result, p_item, p_quantity);
}

bool SteamAPILoader::inventory_exchange_items(ISteamInventoryPtr p_inventory, SteamInventoryResult_t &r_result, const Vector<int32_t> &p_generate_defs, const Vector<int32_t> &p_generate_quantities, const Vector<uint64_t> &p_destroy_ids, const Vector<int32_t> &p_destroy_quantities) const {
	r_result = STEAM_INVENTORY_RESULT_INVALID;
	if (!fn_inventory_exchange_items || p_generate_defs.is_empty() || p_generate_defs.size() != p_generate_quantities.size() || p_destroy_ids.size() != p_destroy_quantities.size()) {
		return false;
	}
	return fn_inventory_exchange_items(
			p_inventory,
			&r_result,
			(const SteamItemDef_t *)p_generate_defs.ptr(),
			(const uint32_t *)p_generate_quantities.ptr(),
			(uint32_t)p_generate_defs.size(),
			(const SteamItemInstanceID_t *)p_destroy_ids.ptr(),
			(const uint32_t *)p_destroy_quantities.ptr(),
			(uint32_t)p_destroy_ids.size());
}

bool SteamAPILoader::inventory_transfer_item_quantity(ISteamInventoryPtr p_inventory, SteamInventoryResult_t &r_result, SteamItemInstanceID_t p_source, uint32_t p_quantity, SteamItemInstanceID_t p_dest) const {
	r_result = STEAM_INVENTORY_RESULT_INVALID;
	return fn_inventory_transfer_item_quantity && fn_inventory_transfer_item_quantity(p_inventory, &r_result, p_source, p_quantity, p_dest);
}

bool SteamAPILoader::inventory_load_item_definitions(ISteamInventoryPtr p_inventory) const {
	return fn_inventory_load_item_definitions && fn_inventory_load_item_definitions(p_inventory);
}

Vector<int32_t> SteamAPILoader::inventory_get_item_definition_ids(ISteamInventoryPtr p_inventory) const {
	Vector<int32_t> ids;
	if (!fn_inventory_get_item_definition_ids) {
		return ids;
	}
	uint32_t count = 0;
	if (!fn_inventory_get_item_definition_ids(p_inventory, nullptr, &count) || count == 0) {
		return ids;
	}
	ids.resize((int)count);
	if (!fn_inventory_get_item_definition_ids(p_inventory, (SteamItemDef_t *)ids.ptrw(), &count)) {
		ids.clear();
		return ids;
	}
	ids.resize((int)count);
	return ids;
}

String SteamAPILoader::inventory_get_item_definition_property(ISteamInventoryPtr p_inventory, SteamItemDef_t p_definition, const char *p_property_name) const {
	if (!fn_inventory_get_item_definition_property) {
		return String();
	}
	uint32_t buffer_size = 0;
	fn_inventory_get_item_definition_property(p_inventory, p_definition, p_property_name, nullptr, &buffer_size);
	if (buffer_size == 0) {
		return String();
	}
	Vector<char> buffer;
	buffer.resize((int)buffer_size);
	if (!fn_inventory_get_item_definition_property(p_inventory, p_definition, p_property_name, buffer.ptrw(), &buffer_size) || buffer_size == 0) {
		return String();
	}
	return String::utf8(buffer.ptr(), (int)buffer_size);
}

Dictionary SteamAPILoader::inventory_get_item_definition_properties(ISteamInventoryPtr p_inventory, SteamItemDef_t p_definition) const {
	Dictionary props;
	const String property_list = inventory_get_item_definition_property(p_inventory, p_definition, nullptr);
	if (property_list.is_empty()) {
		return props;
	}
	PackedStringArray names = property_list.split(",");
	for (int i = 0; i < names.size(); i++) {
		const String name = names[i].strip_edges();
		if (name.is_empty()) {
			continue;
		}
		CharString name_utf8 = name.utf8();
		props[name] = inventory_get_item_definition_property(p_inventory, p_definition, name_utf8.get_data());
	}
	return props;
}

SteamInventoryUpdateHandle_t SteamAPILoader::inventory_start_update_properties(ISteamInventoryPtr p_inventory) const {
	return fn_inventory_start_update_properties ? fn_inventory_start_update_properties(p_inventory) : STEAM_INVENTORY_UPDATE_HANDLE_INVALID;
}

bool SteamAPILoader::inventory_remove_property(ISteamInventoryPtr p_inventory, SteamInventoryUpdateHandle_t p_handle, SteamItemInstanceID_t p_item_id, const char *p_property_name) const {
	return fn_inventory_remove_property && fn_inventory_remove_property(p_inventory, p_handle, p_item_id, p_property_name);
}

bool SteamAPILoader::inventory_set_property_string(ISteamInventoryPtr p_inventory, SteamInventoryUpdateHandle_t p_handle, SteamItemInstanceID_t p_item_id, const char *p_property_name, const char *p_value) const {
	return fn_inventory_set_property_string && fn_inventory_set_property_string(p_inventory, p_handle, p_item_id, p_property_name, p_value);
}

bool SteamAPILoader::inventory_set_property_bool(ISteamInventoryPtr p_inventory, SteamInventoryUpdateHandle_t p_handle, SteamItemInstanceID_t p_item_id, const char *p_property_name, bool p_value) const {
	return fn_inventory_set_property_bool && fn_inventory_set_property_bool(p_inventory, p_handle, p_item_id, p_property_name, p_value);
}

bool SteamAPILoader::inventory_set_property_int64(ISteamInventoryPtr p_inventory, SteamInventoryUpdateHandle_t p_handle, SteamItemInstanceID_t p_item_id, const char *p_property_name, int64_t p_value) const {
	return fn_inventory_set_property_int64 && fn_inventory_set_property_int64(p_inventory, p_handle, p_item_id, p_property_name, p_value);
}

bool SteamAPILoader::inventory_set_property_float(ISteamInventoryPtr p_inventory, SteamInventoryUpdateHandle_t p_handle, SteamItemInstanceID_t p_item_id, const char *p_property_name, float p_value) const {
	return fn_inventory_set_property_float && fn_inventory_set_property_float(p_inventory, p_handle, p_item_id, p_property_name, p_value);
}

bool SteamAPILoader::inventory_submit_update_properties(ISteamInventoryPtr p_inventory, SteamInventoryUpdateHandle_t p_handle, SteamInventoryResult_t &r_result) const {
	r_result = STEAM_INVENTORY_RESULT_INVALID;
	return fn_inventory_submit_update_properties && fn_inventory_submit_update_properties(p_inventory, p_handle, &r_result);
}

bool SteamAPILoader::inventory_inspect_item(ISteamInventoryPtr p_inventory, SteamInventoryResult_t &r_result, const char *p_item_token) const {
	r_result = STEAM_INVENTORY_RESULT_INVALID;
	return fn_inventory_inspect_item && fn_inventory_inspect_item(p_inventory, &r_result, p_item_token);
}

bool SteamAPILoader::inventory_trigger_item_drop(ISteamInventoryPtr p_inventory, SteamInventoryResult_t &r_result, SteamItemDef_t p_drop_list_definition) const {
	r_result = STEAM_INVENTORY_RESULT_INVALID;
	if (!fn_inventory_trigger_item_drop || p_drop_list_definition <= 0) {
		return false;
	}
	return fn_inventory_trigger_item_drop(p_inventory, &r_result, p_drop_list_definition);
}

Vector<int32_t> SteamAPILoader::inventory_get_eligible_promo_item_definition_ids(ISteamInventoryPtr p_inventory, uint64_t p_steam_id) const {
	Vector<int32_t> ids;
	if (!fn_inventory_get_eligible_promo_item_definition_ids || p_steam_id == 0) {
		return ids;
	}
	uint32_t count = 0;
	if (!fn_inventory_get_eligible_promo_item_definition_ids(p_inventory, p_steam_id, nullptr, &count) || count == 0) {
		return ids;
	}
	ids.resize(count);
	if (!fn_inventory_get_eligible_promo_item_definition_ids(p_inventory, p_steam_id, (SteamItemDef_t *)ids.ptrw(), &count)) {
		ids.clear();
		return ids;
	}
	ids.resize(count);
	return ids;
}
