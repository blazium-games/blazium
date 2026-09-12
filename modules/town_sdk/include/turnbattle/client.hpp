/**************************************************************************/
/*  client.hpp                                                            */
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
#include "handlers.hpp"
#include "types.hpp"
#include <memory>
#include <string>
#include <vector>

namespace turnbattle {

class Client {
public:
	Client();
	~Client();

	// Disable copy/move
	Client(const Client &) = delete;
	Client &operator=(const Client &) = delete;

	// Connection
	bool connect(const std::string &address, uint16_t port);
	void disconnect();
	bool is_connected() const;
	std::string get_server_version() const;

	// Auth
	void auth(const std::string &jwt_token);

	// Region
	void enter_region(const std::string &region_id);
	void leave_region();
	void send_move(uint8_t held, float dt);

	// Battle
	void battle_action(const std::string &battle_id, Action action,
			const std::string &target_id = "");
	void leave_battle(const std::string &battle_id);

	// Admin Commands (requires admin JWT token)
	void admin_reload(const std::string &scope); // scope: all|data|scripts|config
	void admin_kick(const std::string &username, const std::string &reason = "");
	void admin_stats_request();
	void admin_broadcast(const std::string &message, bool is_alert = false);

	// Callbacks
	void on_snapshot(OnSnapshotCallback cb);
	void on_move_state(OnMoveStateCallback cb);
	void on_battle_start(OnBattleStartCallback cb);
	void on_battle_state(OnBattleStateCallback cb);
	void on_battle_log(OnBattleLogCallback cb);
	void on_battle_end(OnBattleEndCallback cb);
	void on_battle_indicator_spawn(OnBattleIndicatorSpawnCallback cb);
	void on_battle_indicator_despawn(OnBattleIndicatorDespawnCallback cb);
	void on_error(OnErrorCallback cb);
	void on_disconnect(OnDisconnectCallback cb);

	// Reconnection
	void set_auto_reconnect(bool enabled);
	void manual_reconnect();
	void on_reconnecting(OnReconnectingCallback cb);
	void on_reconnected(OnReconnectedCallback cb);
	void on_reconnect_failed(OnReconnectFailedCallback cb);

	// Admin callbacks
	void on_admin_reload(OnAdminReloadCallback cb);
	void on_admin_kick(OnAdminKickCallback cb);
	void on_admin_stats(OnAdminStatsCallback cb);
	void on_admin_broadcast(OnAdminBroadcastCallback cb);

	// Update (call each frame)
	void update(float dt);

	// Debug utilities
	void set_debug_logging_enabled(bool enabled, size_t history_limit = 64);
	bool is_debug_logging_enabled() const;
	std::vector<std::string> get_debug_log() const;
	void clear_debug_log();
	std::string get_last_error_message() const;
	std::string get_last_warning_message() const;
	std::string get_last_info_message() const;

private:
	bool perform_version_check();
	void handle_message(uint16_t type, const std::string &payload);
	void send_message(uint16_t type, const std::string &payload, uint8_t channel);
	bool is_ready() const;
	void append_log_entry(const char *p_level, const std::string &p_message);
	void log_info(const std::string &p_message);
	void log_warning(const std::string &p_message);
	void log_error(const std::string &p_message);

	struct Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace turnbattle
