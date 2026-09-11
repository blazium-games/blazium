/**************************************************************************/
/*  town_sdk_client.h                                                     */
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

#include "turnbattle/types.hpp"

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/string/ustring.h"
#include "core/variant/typed_array.h"

#include <memory>

namespace turnbattle {
class Client;
}

class TownSdkClient : public Object {
	GDCLASS(TownSdkClient, Object);

public:
	enum GameType {
		GAME_TYPE_TURN_BASED = 0,
		GAME_TYPE_FPS = 1,
	};

	enum BattleAction {
		ACTION_ATTACK = (int)turnbattle::Action::ATTACK,
		ACTION_BLOCK = (int)turnbattle::Action::BLOCK,
		ACTION_DEFEND = (int)turnbattle::Action::DEFEND,
	};

	TownSdkClient();
	~TownSdkClient() override;

	static TownSdkClient *get_singleton();

	bool connect_to_server(const String &p_address, int p_port);
	void disconnect_from_server();
	bool is_client_connected() const;
	String get_server_version() const;

	void set_game_type(GameType p_type);
	GameType get_game_type() const;

	void authenticate(const String &p_jwt_token);
	void authenticate_username(const String &p_username);
	void enter_region(const String &p_region_id);
	void leave_region();
	void send_move(int p_held, double p_delta);
	void send_move_look(int p_held, double p_delta, double p_yaw, double p_pitch, bool p_flashlight = false);
	void send_interact(const String &p_interactable_id);
	void send_pickup(const String &p_pickup_id);
	void send_fire();
	void send_equip(int p_slot);
	void send_use(int p_slot);
	void send_craft(const String &p_recipe);
	void send_reload();
	void request_inventory();

	void battle_action(const String &p_battle_id, BattleAction p_action, const String &p_target_id = String());
	void leave_battle(const String &p_battle_id);

	void admin_reload(const String &p_scope);
	void admin_kick(const String &p_username, const String &p_reason = String());
	void admin_stats_request();
	void admin_broadcast(const String &p_message, bool p_is_alert = false);

	void set_auto_reconnect(bool p_enabled);
	void manual_reconnect();

	void poll(double p_delta);

	void set_debug_logging_enabled(bool p_enabled, int p_history_limit = 64);
	bool is_debug_logging_enabled() const;
	PackedStringArray get_debug_log() const;
	void clear_debug_log();
	String get_last_error_message() const;
	String get_last_warning_message() const;
	String get_last_info_message() const;

protected:
	static void _bind_methods();

private:
	static std::string _string_to_std(const String &p_string);
	static String _std_to_string(const std::string &p_value);

	void _attach_callbacks();

	static TownSdkClient *singleton;
	std::unique_ptr<turnbattle::Client> client;
	GameType game_type = GAME_TYPE_TURN_BASED;
};

VARIANT_ENUM_CAST(TownSdkClient::GameType);
VARIANT_ENUM_CAST(TownSdkClient::BattleAction);
