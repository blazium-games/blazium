/**************************************************************************/
/*  town_sdk_client.cpp                                                   */
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

#include "town_sdk_client.h"

#include "turnbattle/client.hpp"

#include "core/error/error_macros.h"
#include "core/math/math_funcs.h"
#include "core/string/print_string.h"
#include "core/variant/variant.h"

#include <vector>

namespace {
std::string string_to_std(const String &p_string) {
	CharString utf8 = p_string.utf8();
	return std::string(utf8.get_data(), utf8.length());
}

String std_to_string(const std::string &p_value) {
	return String::utf8(p_value.c_str());
}
} // namespace

TownSdkClient *TownSdkClient::singleton = nullptr;

TownSdkClient *TownSdkClient::get_singleton() {
	return singleton;
}

TownSdkClient::TownSdkClient() {
	ERR_FAIL_COND_MSG(singleton != nullptr, "Only one TownSdkClient singleton is allowed.");

	client = std::make_unique<turnbattle::Client>();

	if (client) {
		_attach_callbacks();
		client->set_debug_logging_enabled(true, 128);
	}

	singleton = this;
}

TownSdkClient::~TownSdkClient() {
	if (client) {
		client->on_snapshot({});
		client->on_move_state({});
		client->on_battle_start({});
		client->on_battle_state({});
		client->on_battle_log({});
		client->on_battle_end({});
		client->on_battle_indicator_spawn({});
		client->on_battle_indicator_despawn({});
		client->on_error({});
		client->on_disconnect({});
		client->on_reconnecting({});
		client->on_reconnected({});
		client->on_reconnect_failed({});
		client->on_admin_reload({});
		client->on_admin_kick({});
		client->on_admin_stats({});
		client->on_admin_broadcast({});
		client.reset();
	}

	if (singleton == this) {
		singleton = nullptr;
	}
}

void TownSdkClient::_attach_callbacks() {
	if (!client) {
		return;
	}

	client->on_snapshot([this](const Variant &p_snapshot) {
		emit_signal("snapshot_received", p_snapshot);
	});

	client->on_move_state([this](const Variant &p_state) {
		emit_signal("move_state", p_state);
	});

	client->on_battle_start([this](const Variant &p_battle) {
		emit_signal("battle_start", p_battle);
	});

	client->on_battle_state([this](const Variant &p_state) {
		emit_signal("battle_state", p_state);
	});

	client->on_battle_log([this](const std::string &p_log) {
		emit_signal("battle_log", std_to_string(p_log));
	});

	client->on_battle_end([this](const Variant &p_end) {
		emit_signal("battle_end", p_end);
	});

	client->on_battle_indicator_spawn([this](const Variant &p_indicator) {
		emit_signal("battle_indicator_spawn", p_indicator);
	});

	client->on_battle_indicator_despawn([this](const Variant &p_indicator) {
		emit_signal("battle_indicator_despawn", p_indicator);
	});

	client->on_error([this](const std::string &p_error) {
		emit_signal("error", std_to_string(p_error));
	});

	client->on_disconnect([this](const std::string &p_reason) {
		emit_signal("disconnected", std_to_string(p_reason));
	});

	client->on_reconnecting([this](int p_attempt, float p_next_delay) {
		emit_signal("reconnecting", p_attempt, p_next_delay);
	});

	client->on_reconnected([this](const Variant &p_resume_state) {
		emit_signal("reconnected", p_resume_state);
	});

	client->on_reconnect_failed([this](const std::string &p_reason) {
		emit_signal("reconnect_failed", std_to_string(p_reason));
	});

	client->on_admin_reload([this](const Variant &p_payload) {
		emit_signal("admin_reload_received", p_payload);
	});

	client->on_admin_kick([this](const Variant &p_payload) {
		emit_signal("admin_kick_received", p_payload);
	});

	client->on_admin_stats([this](const Variant &p_payload) {
		emit_signal("admin_stats_received", p_payload);
	});

	client->on_admin_broadcast([this](const Variant &p_payload) {
		emit_signal("admin_broadcast_received", p_payload);
	});
}

std::string TownSdkClient::_string_to_std(const String &p_string) {
	return string_to_std(p_string);
}

String TownSdkClient::_std_to_string(const std::string &p_value) {
	return std_to_string(p_value);
}

bool TownSdkClient::connect_to_server(const String &p_address, int p_port) {
	if (!client) {
		return false;
	}

	bool connected = client->connect(_string_to_std(p_address), (uint16_t)CLAMP(p_port, 0, 65535));
	if (connected) {
		emit_signal("connected");
	} else {
		emit_signal("connection_failed");
	}
	return connected;
}

void TownSdkClient::disconnect_from_server() {
	if (client) {
		client->disconnect();
	}
}

bool TownSdkClient::is_client_connected() const {
	return client ? client->is_connected() : false;
}

String TownSdkClient::get_server_version() const {
	if (!client) {
		return {};
	}
	return _std_to_string(client->get_server_version());
}

void TownSdkClient::authenticate(const String &p_jwt_token) {
	if (client) {
		client->auth(_string_to_std(p_jwt_token));
	}
}

void TownSdkClient::enter_region(const String &p_region_id) {
	if (client) {
		client->enter_region(_string_to_std(p_region_id));
	}
}

void TownSdkClient::leave_region() {
	if (client) {
		client->leave_region();
	}
}

void TownSdkClient::send_move(int p_held, double p_delta) {
	if (client) {
		uint8_t held = (uint8_t)CLAMP(p_held, 0, 255);
		client->send_move(held, (float)p_delta);
	}
}

void TownSdkClient::battle_action(const String &p_battle_id, BattleAction p_action, const String &p_target_id) {
	if (!client) {
		return;
	}

	turnbattle::Action action = static_cast<turnbattle::Action>(p_action);
	client->battle_action(_string_to_std(p_battle_id), action, _string_to_std(p_target_id));
}

void TownSdkClient::leave_battle(const String &p_battle_id) {
	if (client) {
		client->leave_battle(_string_to_std(p_battle_id));
	}
}

void TownSdkClient::admin_reload(const String &p_scope) {
	if (client) {
		client->admin_reload(_string_to_std(p_scope));
	}
}

void TownSdkClient::admin_kick(const String &p_username, const String &p_reason) {
	if (client) {
		client->admin_kick(_string_to_std(p_username), _string_to_std(p_reason));
	}
}

void TownSdkClient::admin_stats_request() {
	if (client) {
		client->admin_stats_request();
	}
}

void TownSdkClient::admin_broadcast(const String &p_message, bool p_is_alert) {
	if (client) {
		client->admin_broadcast(_string_to_std(p_message), p_is_alert);
	}
}

void TownSdkClient::set_auto_reconnect(bool p_enabled) {
	if (client) {
		client->set_auto_reconnect(p_enabled);
	}
}

void TownSdkClient::manual_reconnect() {
	if (client) {
		client->manual_reconnect();
	}
}

void TownSdkClient::poll(double p_delta) {
	if (client) {
		client->update((float)p_delta);
	}
}

void TownSdkClient::set_debug_logging_enabled(bool p_enabled, int p_history_limit) {
	if (client) {
		client->set_debug_logging_enabled(p_enabled, p_history_limit <= 0 ? 1 : (size_t)p_history_limit);
	}
}

bool TownSdkClient::is_debug_logging_enabled() const {
	return client && client->is_debug_logging_enabled();
}

PackedStringArray TownSdkClient::get_debug_log() const {
	PackedStringArray result;
	if (!client) {
		return result;
	}

	const std::vector<std::string> log_entries = client->get_debug_log();
	result.resize(log_entries.size());
	for (size_t i = 0; i < log_entries.size(); ++i) {
		result.set(i, String::utf8(log_entries[i].c_str()));
	}
	return result;
}

void TownSdkClient::clear_debug_log() {
	if (client) {
		client->clear_debug_log();
	}
}

String TownSdkClient::get_last_error_message() const {
	return client ? String::utf8(client->get_last_error_message().c_str()) : String();
}

String TownSdkClient::get_last_warning_message() const {
	return client ? String::utf8(client->get_last_warning_message().c_str()) : String();
}

String TownSdkClient::get_last_info_message() const {
	return client ? String::utf8(client->get_last_info_message().c_str()) : String();
}

void TownSdkClient::_bind_methods() {
	ClassDB::bind_static_method("TownSdkClient", D_METHOD("get_singleton"), &TownSdkClient::get_singleton);
	ClassDB::bind_method(D_METHOD("connect_to_server", "address", "port"), &TownSdkClient::connect_to_server);
	ClassDB::bind_method(D_METHOD("disconnect_from_server"), &TownSdkClient::disconnect_from_server);
	ClassDB::bind_method(D_METHOD("is_client_connected"), &TownSdkClient::is_client_connected);
	ClassDB::bind_method(D_METHOD("get_server_version"), &TownSdkClient::get_server_version);
	ClassDB::bind_method(D_METHOD("authenticate", "jwt_token"), &TownSdkClient::authenticate);
	ClassDB::bind_method(D_METHOD("enter_region", "region_id"), &TownSdkClient::enter_region);
	ClassDB::bind_method(D_METHOD("leave_region"), &TownSdkClient::leave_region);
	ClassDB::bind_method(D_METHOD("send_move", "held", "delta"), &TownSdkClient::send_move);
	ClassDB::bind_method(D_METHOD("battle_action", "battle_id", "action", "target_id"), &TownSdkClient::battle_action, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("leave_battle", "battle_id"), &TownSdkClient::leave_battle);
	ClassDB::bind_method(D_METHOD("admin_reload", "scope"), &TownSdkClient::admin_reload);
	ClassDB::bind_method(D_METHOD("admin_kick", "username", "reason"), &TownSdkClient::admin_kick, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("admin_stats_request"), &TownSdkClient::admin_stats_request);
	ClassDB::bind_method(D_METHOD("admin_broadcast", "message", "is_alert"), &TownSdkClient::admin_broadcast, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("set_auto_reconnect", "enabled"), &TownSdkClient::set_auto_reconnect);
	ClassDB::bind_method(D_METHOD("manual_reconnect"), &TownSdkClient::manual_reconnect);
	ClassDB::bind_method(D_METHOD("poll", "delta"), &TownSdkClient::poll);
	ClassDB::bind_method(D_METHOD("set_debug_logging_enabled", "enabled", "history_limit"), &TownSdkClient::set_debug_logging_enabled, DEFVAL(64));
	ClassDB::bind_method(D_METHOD("is_debug_logging_enabled"), &TownSdkClient::is_debug_logging_enabled);
	ClassDB::bind_method(D_METHOD("get_debug_log"), &TownSdkClient::get_debug_log);
	ClassDB::bind_method(D_METHOD("clear_debug_log"), &TownSdkClient::clear_debug_log);
	ClassDB::bind_method(D_METHOD("get_last_error_message"), &TownSdkClient::get_last_error_message);
	ClassDB::bind_method(D_METHOD("get_last_warning_message"), &TownSdkClient::get_last_warning_message);
	ClassDB::bind_method(D_METHOD("get_last_info_message"), &TownSdkClient::get_last_info_message);

	ADD_SIGNAL(MethodInfo("connected"));
	ADD_SIGNAL(MethodInfo("connection_failed"));
	ADD_SIGNAL(MethodInfo("snapshot_received", PropertyInfo(Variant::DICTIONARY, "snapshot")));
	ADD_SIGNAL(MethodInfo("move_state", PropertyInfo(Variant::DICTIONARY, "state")));
	ADD_SIGNAL(MethodInfo("battle_start", PropertyInfo(Variant::DICTIONARY, "battle")));
	ADD_SIGNAL(MethodInfo("battle_state", PropertyInfo(Variant::DICTIONARY, "state")));
	ADD_SIGNAL(MethodInfo("battle_log", PropertyInfo(Variant::STRING, "log")));
	ADD_SIGNAL(MethodInfo("battle_end", PropertyInfo(Variant::DICTIONARY, "battle")));
	ADD_SIGNAL(MethodInfo("battle_indicator_spawn", PropertyInfo(Variant::DICTIONARY, "indicator")));
	ADD_SIGNAL(MethodInfo("battle_indicator_despawn", PropertyInfo(Variant::DICTIONARY, "indicator")));
	ADD_SIGNAL(MethodInfo("error", PropertyInfo(Variant::STRING, "message")));
	ADD_SIGNAL(MethodInfo("disconnected", PropertyInfo(Variant::STRING, "reason")));
	ADD_SIGNAL(MethodInfo("reconnecting", PropertyInfo(Variant::INT, "attempt"), PropertyInfo(Variant::FLOAT, "next_delay")));
	ADD_SIGNAL(MethodInfo("reconnected", PropertyInfo(Variant::NIL, "resume_state")));
	ADD_SIGNAL(MethodInfo("reconnect_failed", PropertyInfo(Variant::STRING, "reason")));
	ADD_SIGNAL(MethodInfo("admin_reload_received", PropertyInfo(Variant::DICTIONARY, "payload")));
	ADD_SIGNAL(MethodInfo("admin_kick_received", PropertyInfo(Variant::DICTIONARY, "payload")));
	ADD_SIGNAL(MethodInfo("admin_stats_received", PropertyInfo(Variant::DICTIONARY, "payload")));
	ADD_SIGNAL(MethodInfo("admin_broadcast_received", PropertyInfo(Variant::DICTIONARY, "payload")));

	BIND_ENUM_CONSTANT(ACTION_ATTACK);
	BIND_ENUM_CONSTANT(ACTION_BLOCK);
	BIND_ENUM_CONSTANT(ACTION_DEFEND);
}
