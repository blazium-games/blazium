/**************************************************************************/
/*  steam.h                                                               */
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

#include "core/object/object.h"
#include "steam_achievement_info.h"
#include "steam_api_loader.h"
#include "steam_auth_result.h"
#include "steam_inventory_item.h"
#include "steam_item_definition.h"

class SteamAuthClient;

class Steam : public Object {
	GDCLASS(Steam, Object);

public:
	enum TicketState {
		TICKET_STATE_IDLE,
		TICKET_STATE_PENDING,
		TICKET_STATE_READY,
		TICKET_STATE_FAILED,
	};

	enum AvatarSize {
		AVATAR_SIZE_SMALL,
		AVATAR_SIZE_MEDIUM,
		AVATAR_SIZE_LARGE,
	};

private:
	static Steam *singleton;

	SteamAPILoader loader;
	SteamAuthClient *auth_client = nullptr;

	bool dll_available = false;
	bool initialized = false;
	bool debug_logging = false;
	int app_id = 0;
	SteamAPILoader::HSteamPipe steam_pipe = 0;
	SteamAPILoader::ISteamUserPtr steam_user = nullptr;
	SteamAPILoader::ISteamUserStatsPtr steam_user_stats = nullptr;
	SteamAPILoader::ISteamFriendsPtr steam_friends = nullptr;
	SteamAPILoader::ISteamUtilsPtr steam_utils = nullptr;
	SteamAPILoader::ISteamInventoryPtr steam_inventory = nullptr;

	TicketState ticket_state = TICKET_STATE_IDLE;
	SteamAPILoader::HAuthTicket pending_auth_ticket = 0;
	String pending_hex_ticket;
	String pending_ticket_error;

	bool stats_received = false;
	bool stats_store_pending = false;
	bool stats_store_succeeded = false;

	bool inventory_definitions_loaded = false;
	bool inventory_definitions_pending = false;
	HashMap<int, int> inventory_pending_results;

	SteamInventoryUpdateHandle_t inventory_property_update_handle = STEAM_INVENTORY_UPDATE_HANDLE_INVALID;

	Vector<String> debug_log;
	static const int kMaxDebugLogEntries = 256;

	void _log_debug(const String &p_message);
	void _dispatch_callbacks();
	void _handle_callback(int p_callback_id, const void *p_data, int p_size);
	void _reset_ticket_state();
	bool _ensure_stats_ready() const;
	bool _wait_for_user_stats(double p_timeout_sec, bool p_require_fresh);
	bool _probe_stats_loaded();
	bool _get_stat_int(const String &p_name, int &r_value) const;
	Ref<SteamAchievementInfo> _build_achievement_info(const String &p_name, bool p_include_icon) const;
	Ref<Image> _image_from_steam_handle(int p_image) const;
	bool _ensure_inventory_ready() const;
	bool _wait_for_inventory_result(SteamInventoryResult_t p_result, double p_timeout_sec);
	Array _parse_inventory_result(SteamInventoryResult_t p_result);
	Ref<SteamInventoryItem> _build_inventory_item(const SteamItemDetails &p_details, SteamInventoryResult_t p_result, uint32_t p_item_index) const;
	Ref<SteamItemDefinition> _build_item_definition(int p_def_id) const;
	Ref<Image> _fetch_image_from_url(const String &p_url) const;
	bool _wait_for_inventory_definitions(double p_timeout_sec);

protected:
	static void _bind_methods();

public:
	static Steam *get_singleton();

	bool is_available();
	bool has_inventory_support() const;
	Error initialize(int p_app_id = 0);
	void shutdown();
	bool is_initialized() const { return initialized; }

	void set_debug_logging(bool p_enabled);
	bool is_debug_logging_enabled() const { return debug_logging; }
	Array get_debug_log() const;
	void clear_debug_log();

	void request_web_api_ticket(const String &p_identity = "blazium");
	void poll_callbacks();
	TicketState get_ticket_state() const { return ticket_state; }
	String get_pending_hex_ticket() const { return pending_hex_ticket; }
	int get_pending_auth_ticket_handle() const { return pending_auth_ticket; }
	String get_pending_ticket_error() const { return pending_ticket_error; }

	Ref<SteamAuthResult> authenticate_with_server(const String &p_url, const String &p_ticket, int p_app_id);
	void cancel_auth_ticket(int p_handle);

	uint64_t get_local_steam_id() const;
	bool is_logged_on() const;

	bool request_current_stats();
	bool refresh_current_stats();
	bool store_stats();
	bool set_achievement(const String &p_name);
	bool clear_achievement(const String &p_name);
	bool get_achievement(const String &p_name) const;
	Ref<SteamAchievementInfo> get_achievement_info(const String &p_name, bool p_include_icon = true);
	Array get_all_achievements(bool p_include_icons = false);

	int get_stat_int(const String &p_name) const;
	float get_stat_float(const String &p_name) const;
	bool set_stat_int(const String &p_name, int p_value);
	bool set_stat_float(const String &p_name, float p_value);
	bool increment_stat_int(const String &p_name, int p_delta);
	bool clear_stat(const String &p_name);

	String get_persona_name() const;
	Ref<Image> get_avatar_image(uint64_t p_steam_id = 0, AvatarSize p_size = AVATAR_SIZE_LARGE);

	bool load_item_definitions();
	bool request_item_definitions(double p_timeout_sec = 15.0);
	PackedInt32Array get_item_definition_ids();
	Ref<SteamItemDefinition> get_item_definition(int p_def_id);
	Array get_all_item_definitions();
	Array get_all_items();
	Array get_items_by_id(const PackedInt64Array &p_instance_ids);
	Array find_items_by_def_id(int p_def_id);
	Ref<Image> get_item_definition_icon(int p_def_id);
	bool consume_item(uint64_t p_instance_id, int p_quantity);
	bool exchange_items(const PackedInt32Array &p_generate_def_ids, const PackedInt32Array &p_generate_quantities, const PackedInt64Array &p_destroy_instance_ids, const PackedInt32Array &p_destroy_quantities);
	bool trigger_item_drop(int p_drop_list_definition);
	bool transfer_item_quantity(uint64_t p_source_instance_id, int p_quantity, uint64_t p_dest_instance_id);
	bool grant_promo_items();
	PackedInt32Array get_eligible_promo_item_definition_ids(uint64_t p_steam_id = 0);
	int get_inventory_result_status(int p_result_handle);
	Array get_inventory_result_items(int p_result_handle);
	bool add_promo_item(int p_def_id);
	bool add_promo_items(const PackedInt32Array &p_def_ids);
	bool start_property_update();
	bool set_property_string(uint64_t p_instance_id, const String &p_property_name, const String &p_value);
	bool set_property_bool(uint64_t p_instance_id, const String &p_property_name, bool p_value);
	bool set_property_int64(uint64_t p_instance_id, const String &p_property_name, int p_value);
	bool set_property_float(uint64_t p_instance_id, const String &p_property_name, float p_value);
	bool remove_property(uint64_t p_instance_id, const String &p_property_name);
	bool submit_property_update();
	PackedByteArray serialize_inventory_result(int p_result_handle);
	int deserialize_inventory(const PackedByteArray &p_bytes, uint64_t p_expected_steam_id = 0);

	Steam();
	~Steam();
};

VARIANT_ENUM_CAST(Steam::TicketState);
VARIANT_ENUM_CAST(Steam::AvatarSize);
