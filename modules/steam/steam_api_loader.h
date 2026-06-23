/**************************************************************************/
/*  steam_api_loader.h                                                    */
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

#include "core/error/error_list.h"
#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"

#include "core/io/image.h"

#include "steam_types.h"

class SteamAPILoader {
public:
	typedef int32_t HAuthTicket;
	typedef int32_t HSteamPipe;
	typedef void *ISteamUserPtr;
	typedef void *ISteamUserStatsPtr;
	typedef void *ISteamFriendsPtr;
	typedef void *ISteamUtilsPtr;
	typedef void *ISteamInventoryPtr;

private:
	void *library_handle = nullptr;
	bool loaded = false;
	bool stats_loaded = false;
	bool inventory_loaded = false;

	// Core API
	typedef int (*SteamAPI_InitFlatFn)(char *p_out_err_msg);
	typedef void (*SteamAPI_ShutdownFn)();
	typedef HSteamPipe (*SteamAPI_GetHSteamPipeFn)();
	typedef void (*SteamAPI_ManualDispatch_InitFn)();
	typedef void (*SteamAPI_ManualDispatch_RunFrameFn)(HSteamPipe p_pipe);
	typedef bool (*SteamAPI_ManualDispatch_GetNextCallbackFn)(HSteamPipe p_pipe, void *p_callback_msg);
	typedef void (*SteamAPI_ManualDispatch_FreeLastCallbackFn)(HSteamPipe p_pipe);
	typedef bool (*SteamAPI_ManualDispatch_GetAPICallResultFn)(HSteamPipe p_pipe, uint64_t p_async_call, void *p_callback, int p_cub_callback, int p_callback_expected, bool *p_failed);
	typedef bool (*SteamAPI_IsSteamRunningFn)();
	typedef ISteamUserPtr (*SteamAPI_SteamUserFn)();
	typedef ISteamUserStatsPtr (*SteamAPI_SteamUserStatsFn)();
	typedef ISteamFriendsPtr (*SteamAPI_SteamFriendsFn)();
	typedef ISteamUtilsPtr (*SteamAPI_SteamUtilsFn)();

	// ISteamUser flat API
	typedef HAuthTicket (*ISteamUser_GetAuthTicketForWebApiFn)(ISteamUserPtr p_self, const char *p_identity);
	typedef void (*ISteamUser_CancelAuthTicketFn)(ISteamUserPtr p_self, HAuthTicket p_ticket);
	typedef bool (*ISteamUser_BLoggedOnFn)(ISteamUserPtr p_self);
	typedef uint64_t (*ISteamUser_GetSteamIDFn)(ISteamUserPtr p_self);

	// ISteamUserStats flat API
	typedef bool (*ISteamUserStats_RequestCurrentStatsFn)(ISteamUserStatsPtr p_self);
	typedef bool (*ISteamUserStats_GetAchievementFn)(ISteamUserStatsPtr p_self, const char *p_name, bool *p_achieved);
	typedef bool (*ISteamUserStats_SetAchievementFn)(ISteamUserStatsPtr p_self, const char *p_name);
	typedef bool (*ISteamUserStats_ClearAchievementFn)(ISteamUserStatsPtr p_self, const char *p_name);
	typedef bool (*ISteamUserStats_StoreStatsFn)(ISteamUserStatsPtr p_self);
	typedef bool (*ISteamUserStats_GetAchievementAndUnlockTimeFn)(ISteamUserStatsPtr p_self, const char *p_name, bool *p_achieved, uint32_t *p_unlock_time);
	typedef int (*ISteamUserStats_GetAchievementIconFn)(ISteamUserStatsPtr p_self, const char *p_name);
	typedef const char *(*ISteamUserStats_GetAchievementDisplayAttributeFn)(ISteamUserStatsPtr p_self, const char *p_name, const char *p_key);
	typedef uint32_t (*ISteamUserStats_GetNumAchievementsFn)(ISteamUserStatsPtr p_self);
	typedef const char *(*ISteamUserStats_GetAchievementNameFn)(ISteamUserStatsPtr p_self, uint32_t p_index);
	typedef bool (*ISteamUserStats_GetStatInt32Fn)(ISteamUserStatsPtr p_self, const char *p_name, int32_t *p_data);
	typedef bool (*ISteamUserStats_GetStatFloatFn)(ISteamUserStatsPtr p_self, const char *p_name, float *p_data);
	typedef bool (*ISteamUserStats_SetStatInt32Fn)(ISteamUserStatsPtr p_self, const char *p_name, int32_t p_data);
	typedef bool (*ISteamUserStats_SetStatFloatFn)(ISteamUserStatsPtr p_self, const char *p_name, float p_data);
	typedef bool (*ISteamUserStats_UpdateAvgRateStatFn)(ISteamUserStatsPtr p_self, const char *p_name, float p_count_this_session, double p_session_length);

	// ISteamFriends flat API
	typedef const char *(*ISteamFriends_GetPersonaNameFn)(ISteamFriendsPtr p_self);
	typedef int (*ISteamFriends_GetSmallFriendAvatarFn)(ISteamFriendsPtr p_self, uint64_t p_steam_id);
	typedef int (*ISteamFriends_GetMediumFriendAvatarFn)(ISteamFriendsPtr p_self, uint64_t p_steam_id);
	typedef int (*ISteamFriends_GetLargeFriendAvatarFn)(ISteamFriendsPtr p_self, uint64_t p_steam_id);

	// ISteamUtils flat API
	typedef bool (*ISteamUtils_GetImageSizeFn)(ISteamUtilsPtr p_self, int p_image, uint32_t *p_width, uint32_t *p_height);
	typedef bool (*ISteamUtils_GetImageRGBAFn)(ISteamUtilsPtr p_self, int p_image, uint8_t *p_dest, int p_dest_size);

	// ISteamInventory flat API
	typedef ISteamInventoryPtr (*SteamAPI_SteamInventoryFn)();
	typedef int (*ISteamInventory_GetResultStatusFn)(ISteamInventoryPtr p_self, SteamInventoryResult_t p_result);
	typedef bool (*ISteamInventory_GetResultItemsFn)(ISteamInventoryPtr p_self, SteamInventoryResult_t p_result, SteamItemDetails *p_out_items, uint32_t *p_out_count);
	typedef bool (*ISteamInventory_GetResultItemPropertyFn)(ISteamInventoryPtr p_self, SteamInventoryResult_t p_result, uint32_t p_item_index, const char *p_property_name, char *p_value_buffer, uint32_t *p_value_buffer_size);
	typedef uint32_t (*ISteamInventory_GetResultTimestampFn)(ISteamInventoryPtr p_self, SteamInventoryResult_t p_result);
	typedef bool (*ISteamInventory_CheckResultSteamIDFn)(ISteamInventoryPtr p_self, SteamInventoryResult_t p_result, uint64_t p_steam_id);
	typedef void (*ISteamInventory_DestroyResultFn)(ISteamInventoryPtr p_self, SteamInventoryResult_t p_result);
	typedef bool (*ISteamInventory_GetAllItemsFn)(ISteamInventoryPtr p_self, SteamInventoryResult_t *p_result);
	typedef bool (*ISteamInventory_GetItemsByIDFn)(ISteamInventoryPtr p_self, SteamInventoryResult_t *p_result, const SteamItemInstanceID_t *p_instance_ids, uint32_t p_count);
	typedef bool (*ISteamInventory_SerializeResultFn)(ISteamInventoryPtr p_self, SteamInventoryResult_t p_result, void *p_out_buffer, uint32_t *p_out_buffer_size);
	typedef bool (*ISteamInventory_DeserializeResultFn)(ISteamInventoryPtr p_self, SteamInventoryResult_t *p_out_result, const void *p_buffer, uint32_t p_buffer_size, bool p_reserved);
	typedef bool (*ISteamInventory_GrantPromoItemsFn)(ISteamInventoryPtr p_self, SteamInventoryResult_t *p_result);
	typedef bool (*ISteamInventory_AddPromoItemFn)(ISteamInventoryPtr p_self, SteamInventoryResult_t *p_result, SteamItemDef_t p_item_def);
	typedef bool (*ISteamInventory_AddPromoItemsFn)(ISteamInventoryPtr p_self, SteamInventoryResult_t *p_result, const SteamItemDef_t *p_item_defs, uint32_t p_count);
	typedef bool (*ISteamInventory_ConsumeItemFn)(ISteamInventoryPtr p_self, SteamInventoryResult_t *p_result, SteamItemInstanceID_t p_item, uint32_t p_quantity);
	typedef bool (*ISteamInventory_ExchangeItemsFn)(ISteamInventoryPtr p_self, SteamInventoryResult_t *p_result, const SteamItemDef_t *p_generate_defs, const uint32_t *p_generate_quantities, uint32_t p_generate_count, const SteamItemInstanceID_t *p_destroy_ids, const uint32_t *p_destroy_quantities, uint32_t p_destroy_count);
	typedef bool (*ISteamInventory_TransferItemQuantityFn)(ISteamInventoryPtr p_self, SteamInventoryResult_t *p_result, SteamItemInstanceID_t p_source, uint32_t p_quantity, SteamItemInstanceID_t p_dest);
	typedef bool (*ISteamInventory_LoadItemDefinitionsFn)(ISteamInventoryPtr p_self);
	typedef bool (*ISteamInventory_GetItemDefinitionIDsFn)(ISteamInventoryPtr p_self, SteamItemDef_t *p_item_def_ids, uint32_t *p_count);
	typedef bool (*ISteamInventory_GetItemDefinitionPropertyFn)(ISteamInventoryPtr p_self, SteamItemDef_t p_definition, const char *p_property_name, char *p_value_buffer, uint32_t *p_value_buffer_size);
	typedef SteamInventoryUpdateHandle_t (*ISteamInventory_StartUpdatePropertiesFn)(ISteamInventoryPtr p_self);
	typedef bool (*ISteamInventory_RemovePropertyFn)(ISteamInventoryPtr p_self, SteamInventoryUpdateHandle_t p_handle, SteamItemInstanceID_t p_item_id, const char *p_property_name);
	typedef bool (*ISteamInventory_SetPropertyStringFn)(ISteamInventoryPtr p_self, SteamInventoryUpdateHandle_t p_handle, SteamItemInstanceID_t p_item_id, const char *p_property_name, const char *p_value);
	typedef bool (*ISteamInventory_SetPropertyBoolFn)(ISteamInventoryPtr p_self, SteamInventoryUpdateHandle_t p_handle, SteamItemInstanceID_t p_item_id, const char *p_property_name, bool p_value);
	typedef bool (*ISteamInventory_SetPropertyInt64Fn)(ISteamInventoryPtr p_self, SteamInventoryUpdateHandle_t p_handle, SteamItemInstanceID_t p_item_id, const char *p_property_name, int64_t p_value);
	typedef bool (*ISteamInventory_SetPropertyFloatFn)(ISteamInventoryPtr p_self, SteamInventoryUpdateHandle_t p_handle, SteamItemInstanceID_t p_item_id, const char *p_property_name, float p_value);
	typedef bool (*ISteamInventory_SubmitUpdatePropertiesFn)(ISteamInventoryPtr p_self, SteamInventoryUpdateHandle_t p_handle, SteamInventoryResult_t *p_result);
	typedef bool (*ISteamInventory_InspectItemFn)(ISteamInventoryPtr p_self, SteamInventoryResult_t *p_result, const char *p_item_token);
	typedef bool (*ISteamInventory_TriggerItemDropFn)(ISteamInventoryPtr p_self, SteamInventoryResult_t *p_result, SteamItemDef_t p_drop_list_definition);
	typedef bool (*ISteamInventory_GetEligiblePromoItemDefinitionIDsFn)(ISteamInventoryPtr p_self, uint64_t p_steam_id, SteamItemDef_t *p_item_def_ids, uint32_t *p_count);

	SteamAPI_InitFlatFn fn_init_flat = nullptr;
	SteamAPI_ShutdownFn fn_shutdown = nullptr;
	SteamAPI_GetHSteamPipeFn fn_get_h_steam_pipe = nullptr;
	SteamAPI_ManualDispatch_InitFn fn_manual_dispatch_init = nullptr;
	SteamAPI_ManualDispatch_RunFrameFn fn_manual_dispatch_run_frame = nullptr;
	SteamAPI_ManualDispatch_GetNextCallbackFn fn_manual_dispatch_get_next_callback = nullptr;
	SteamAPI_ManualDispatch_FreeLastCallbackFn fn_manual_dispatch_free_last_callback = nullptr;
	SteamAPI_ManualDispatch_GetAPICallResultFn fn_manual_dispatch_get_api_call_result = nullptr;
	SteamAPI_IsSteamRunningFn fn_is_steam_running = nullptr;
	SteamAPI_SteamUserFn fn_steam_user = nullptr;
	ISteamUser_GetAuthTicketForWebApiFn fn_get_auth_ticket_for_web_api = nullptr;
	ISteamUser_CancelAuthTicketFn fn_cancel_auth_ticket = nullptr;
	ISteamUser_BLoggedOnFn fn_b_logged_on = nullptr;
	ISteamUser_GetSteamIDFn fn_get_steam_id = nullptr;

	SteamAPI_SteamUserStatsFn fn_steam_user_stats = nullptr;
	ISteamUserStats_RequestCurrentStatsFn fn_request_current_stats = nullptr;
	ISteamUserStats_GetAchievementFn fn_get_achievement = nullptr;
	ISteamUserStats_SetAchievementFn fn_set_achievement = nullptr;
	ISteamUserStats_ClearAchievementFn fn_clear_achievement = nullptr;
	ISteamUserStats_StoreStatsFn fn_store_stats = nullptr;
	ISteamUserStats_GetAchievementAndUnlockTimeFn fn_get_achievement_and_unlock_time = nullptr;
	ISteamUserStats_GetAchievementIconFn fn_get_achievement_icon = nullptr;
	ISteamUserStats_GetAchievementDisplayAttributeFn fn_get_achievement_display_attribute = nullptr;
	ISteamUserStats_GetNumAchievementsFn fn_get_num_achievements = nullptr;
	ISteamUserStats_GetAchievementNameFn fn_get_achievement_name = nullptr;
	ISteamUserStats_GetStatInt32Fn fn_get_stat_int32 = nullptr;
	ISteamUserStats_GetStatFloatFn fn_get_stat_float = nullptr;
	ISteamUserStats_SetStatInt32Fn fn_set_stat_int32 = nullptr;
	ISteamUserStats_SetStatFloatFn fn_set_stat_float = nullptr;
	ISteamUserStats_UpdateAvgRateStatFn fn_update_avg_rate_stat = nullptr;

	SteamAPI_SteamFriendsFn fn_steam_friends = nullptr;
	ISteamFriends_GetPersonaNameFn fn_get_persona_name = nullptr;
	ISteamFriends_GetSmallFriendAvatarFn fn_get_small_friend_avatar = nullptr;
	ISteamFriends_GetMediumFriendAvatarFn fn_get_medium_friend_avatar = nullptr;
	ISteamFriends_GetLargeFriendAvatarFn fn_get_large_friend_avatar = nullptr;

	SteamAPI_SteamUtilsFn fn_steam_utils = nullptr;
	ISteamUtils_GetImageSizeFn fn_get_image_size = nullptr;
	ISteamUtils_GetImageRGBAFn fn_get_image_rgba = nullptr;

	SteamAPI_SteamInventoryFn fn_steam_inventory = nullptr;
	ISteamInventory_GetResultStatusFn fn_inventory_get_result_status = nullptr;
	ISteamInventory_GetResultItemsFn fn_inventory_get_result_items = nullptr;
	ISteamInventory_GetResultItemPropertyFn fn_inventory_get_result_item_property = nullptr;
	ISteamInventory_GetResultTimestampFn fn_inventory_get_result_timestamp = nullptr;
	ISteamInventory_CheckResultSteamIDFn fn_inventory_check_result_steam_id = nullptr;
	ISteamInventory_DestroyResultFn fn_inventory_destroy_result = nullptr;
	ISteamInventory_GetAllItemsFn fn_inventory_get_all_items = nullptr;
	ISteamInventory_GetItemsByIDFn fn_inventory_get_items_by_id = nullptr;
	ISteamInventory_SerializeResultFn fn_inventory_serialize_result = nullptr;
	ISteamInventory_DeserializeResultFn fn_inventory_deserialize_result = nullptr;
	ISteamInventory_GrantPromoItemsFn fn_inventory_grant_promo_items = nullptr;
	ISteamInventory_AddPromoItemFn fn_inventory_add_promo_item = nullptr;
	ISteamInventory_AddPromoItemsFn fn_inventory_add_promo_items = nullptr;
	ISteamInventory_ConsumeItemFn fn_inventory_consume_item = nullptr;
	ISteamInventory_ExchangeItemsFn fn_inventory_exchange_items = nullptr;
	ISteamInventory_TransferItemQuantityFn fn_inventory_transfer_item_quantity = nullptr;
	ISteamInventory_LoadItemDefinitionsFn fn_inventory_load_item_definitions = nullptr;
	ISteamInventory_GetItemDefinitionIDsFn fn_inventory_get_item_definition_ids = nullptr;
	ISteamInventory_GetItemDefinitionPropertyFn fn_inventory_get_item_definition_property = nullptr;
	ISteamInventory_StartUpdatePropertiesFn fn_inventory_start_update_properties = nullptr;
	ISteamInventory_RemovePropertyFn fn_inventory_remove_property = nullptr;
	ISteamInventory_SetPropertyStringFn fn_inventory_set_property_string = nullptr;
	ISteamInventory_SetPropertyBoolFn fn_inventory_set_property_bool = nullptr;
	ISteamInventory_SetPropertyInt64Fn fn_inventory_set_property_int64 = nullptr;
	ISteamInventory_SetPropertyFloatFn fn_inventory_set_property_float = nullptr;
	ISteamInventory_SubmitUpdatePropertiesFn fn_inventory_submit_update_properties = nullptr;
	ISteamInventory_InspectItemFn fn_inventory_inspect_item = nullptr;
	ISteamInventory_TriggerItemDropFn fn_inventory_trigger_item_drop = nullptr;
	ISteamInventory_GetEligiblePromoItemDefinitionIDsFn fn_inventory_get_eligible_promo_item_definition_ids = nullptr;

	bool _load_symbol(const char *p_name, void *&r_symbol);
	bool _load_stats_symbols();
	bool _load_inventory_symbols();

public:
	static constexpr int kGetTicketForWebApiResponseCallback = 168; // k_iSteamUserCallbacks(100) + 68

	bool try_load();
	void unload();
	bool is_loaded() const { return loaded; }
	bool has_stats_support() const { return stats_loaded; }
	bool has_inventory_support() const { return inventory_loaded; }

	int init_flat(String &r_err_msg);
	void shutdown();
	HSteamPipe get_h_steam_pipe() const;
	void manual_dispatch_init();
	void manual_dispatch_run_frame(HSteamPipe p_pipe);
	bool manual_dispatch_get_next_callback(HSteamPipe p_pipe, void *p_callback_msg);
	void manual_dispatch_free_last_callback(HSteamPipe p_pipe);
	bool manual_dispatch_get_api_call_result(HSteamPipe p_pipe, uint64_t p_async_call, void *p_callback, int p_cub_callback, int p_callback_expected, bool *p_failed);
	bool is_steam_running() const;

	ISteamUserPtr get_steam_user() const;
	HAuthTicket get_auth_ticket_for_web_api(ISteamUserPtr p_user, const char *p_identity) const;
	void cancel_auth_ticket(ISteamUserPtr p_user, HAuthTicket p_ticket) const;
	bool is_logged_on(ISteamUserPtr p_user) const;
	uint64_t get_steam_id(ISteamUserPtr p_user) const;

	ISteamUserStatsPtr get_steam_user_stats() const;
	bool request_current_stats(ISteamUserStatsPtr p_stats) const;
	bool get_achievement(ISteamUserStatsPtr p_stats, const char *p_name, bool &r_achieved) const;
	bool set_achievement(ISteamUserStatsPtr p_stats, const char *p_name) const;
	bool clear_achievement(ISteamUserStatsPtr p_stats, const char *p_name) const;
	bool store_stats(ISteamUserStatsPtr p_stats) const;
	bool get_achievement_and_unlock_time(ISteamUserStatsPtr p_stats, const char *p_name, bool &r_achieved, uint32_t &r_unlock_time) const;
	int get_achievement_icon(ISteamUserStatsPtr p_stats, const char *p_name) const;
	String get_achievement_display_attribute(ISteamUserStatsPtr p_stats, const char *p_name, const char *p_key) const;
	uint32_t get_num_achievements(ISteamUserStatsPtr p_stats) const;
	String get_achievement_name(ISteamUserStatsPtr p_stats, uint32_t p_index) const;
	bool get_stat_int32(ISteamUserStatsPtr p_stats, const char *p_name, int32_t &r_data) const;
	bool get_stat_float(ISteamUserStatsPtr p_stats, const char *p_name, float &r_data) const;
	bool set_stat_int32(ISteamUserStatsPtr p_stats, const char *p_name, int32_t p_data) const;
	bool set_stat_float(ISteamUserStatsPtr p_stats, const char *p_name, float p_data) const;
	bool update_avg_rate_stat(ISteamUserStatsPtr p_stats, const char *p_name, float p_count_this_session, double p_session_length) const;

	ISteamFriendsPtr get_steam_friends() const;
	String get_persona_name(ISteamFriendsPtr p_friends) const;
	int get_friend_avatar(ISteamFriendsPtr p_friends, uint64_t p_steam_id, int p_size) const;

	ISteamUtilsPtr get_steam_utils() const;
	bool get_image_size(ISteamUtilsPtr p_utils, int p_image, uint32_t &r_width, uint32_t &r_height) const;
	bool get_image_rgba(ISteamUtilsPtr p_utils, int p_image, uint8_t *p_dest, int p_dest_size) const;

	static String bytes_to_hex(const uint8_t *p_data, int p_size);
	static Ref<Image> image_from_rgba(const uint8_t *p_data, int p_width, int p_height);
	Ref<Image> image_from_steam_handle(ISteamUtilsPtr p_utils, int p_image) const;

	ISteamInventoryPtr get_steam_inventory() const;
	int inventory_get_result_status(ISteamInventoryPtr p_inventory, SteamInventoryResult_t p_result) const;
	bool inventory_get_result_items(ISteamInventoryPtr p_inventory, SteamInventoryResult_t p_result, Vector<SteamItemDetails> &r_items) const;
	String inventory_get_result_item_property(ISteamInventoryPtr p_inventory, SteamInventoryResult_t p_result, uint32_t p_item_index, const char *p_property_name) const;
	uint32_t inventory_get_result_timestamp(ISteamInventoryPtr p_inventory, SteamInventoryResult_t p_result) const;
	bool inventory_check_result_steam_id(ISteamInventoryPtr p_inventory, SteamInventoryResult_t p_result, uint64_t p_steam_id) const;
	void inventory_destroy_result(ISteamInventoryPtr p_inventory, SteamInventoryResult_t p_result) const;
	bool inventory_get_all_items(ISteamInventoryPtr p_inventory, SteamInventoryResult_t &r_result) const;
	bool inventory_get_items_by_id(ISteamInventoryPtr p_inventory, SteamInventoryResult_t &r_result, const Vector<uint64_t> &p_instance_ids) const;
	bool inventory_serialize_result(ISteamInventoryPtr p_inventory, SteamInventoryResult_t p_result, PackedByteArray &r_bytes) const;
	bool inventory_deserialize_result(ISteamInventoryPtr p_inventory, SteamInventoryResult_t &r_result, const PackedByteArray &p_bytes, bool p_reserved = false) const;
	bool inventory_grant_promo_items(ISteamInventoryPtr p_inventory, SteamInventoryResult_t &r_result) const;
	bool inventory_add_promo_item(ISteamInventoryPtr p_inventory, SteamInventoryResult_t &r_result, SteamItemDef_t p_item_def) const;
	bool inventory_add_promo_items(ISteamInventoryPtr p_inventory, SteamInventoryResult_t &r_result, const Vector<int32_t> &p_item_defs) const;
	bool inventory_consume_item(ISteamInventoryPtr p_inventory, SteamInventoryResult_t &r_result, SteamItemInstanceID_t p_item, uint32_t p_quantity) const;
	bool inventory_exchange_items(ISteamInventoryPtr p_inventory, SteamInventoryResult_t &r_result, const Vector<int32_t> &p_generate_defs, const Vector<int32_t> &p_generate_quantities, const Vector<uint64_t> &p_destroy_ids, const Vector<int32_t> &p_destroy_quantities) const;
	bool inventory_transfer_item_quantity(ISteamInventoryPtr p_inventory, SteamInventoryResult_t &r_result, SteamItemInstanceID_t p_source, uint32_t p_quantity, SteamItemInstanceID_t p_dest) const;
	bool inventory_load_item_definitions(ISteamInventoryPtr p_inventory) const;
	Vector<int32_t> inventory_get_item_definition_ids(ISteamInventoryPtr p_inventory) const;
	String inventory_get_item_definition_property(ISteamInventoryPtr p_inventory, SteamItemDef_t p_definition, const char *p_property_name) const;
	Dictionary inventory_get_item_definition_properties(ISteamInventoryPtr p_inventory, SteamItemDef_t p_definition) const;
	SteamInventoryUpdateHandle_t inventory_start_update_properties(ISteamInventoryPtr p_inventory) const;
	bool inventory_remove_property(ISteamInventoryPtr p_inventory, SteamInventoryUpdateHandle_t p_handle, SteamItemInstanceID_t p_item_id, const char *p_property_name) const;
	bool inventory_set_property_string(ISteamInventoryPtr p_inventory, SteamInventoryUpdateHandle_t p_handle, SteamItemInstanceID_t p_item_id, const char *p_property_name, const char *p_value) const;
	bool inventory_set_property_bool(ISteamInventoryPtr p_inventory, SteamInventoryUpdateHandle_t p_handle, SteamItemInstanceID_t p_item_id, const char *p_property_name, bool p_value) const;
	bool inventory_set_property_int64(ISteamInventoryPtr p_inventory, SteamInventoryUpdateHandle_t p_handle, SteamItemInstanceID_t p_item_id, const char *p_property_name, int64_t p_value) const;
	bool inventory_set_property_float(ISteamInventoryPtr p_inventory, SteamInventoryUpdateHandle_t p_handle, SteamItemInstanceID_t p_item_id, const char *p_property_name, float p_value) const;
	bool inventory_submit_update_properties(ISteamInventoryPtr p_inventory, SteamInventoryUpdateHandle_t p_handle, SteamInventoryResult_t &r_result) const;
	bool inventory_inspect_item(ISteamInventoryPtr p_inventory, SteamInventoryResult_t &r_result, const char *p_item_token) const;
	bool inventory_trigger_item_drop(ISteamInventoryPtr p_inventory, SteamInventoryResult_t &r_result, SteamItemDef_t p_drop_list_definition) const;
	Vector<int32_t> inventory_get_eligible_promo_item_definition_ids(ISteamInventoryPtr p_inventory, uint64_t p_steam_id) const;
};
