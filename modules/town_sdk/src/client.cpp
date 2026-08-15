/**************************************************************************/
/*  client.cpp                                                            */
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

#include "turnbattle/client.hpp"
#include "turnbattle/protocol.hpp"
#include "turnbattle/version.hpp"

#include "core/error/error_macros.h"
#include "core/io/json.h"
#include "core/string/print_string.h"
#include "core/string/ustring.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

#include <enet/enet.h>
#include <chrono>
#include <cstring>
#include <functional>
#include <vector>

namespace turnbattle {
namespace {

static std::string to_std_string(const String &p_string) {
	CharString utf8 = p_string.utf8();
	return std::string(utf8.get_data(), utf8.length());
}

static std::string to_std_string(const Variant &p_value) {
	if (p_value.get_type() == Variant::STRING || p_value.get_type() == Variant::STRING_NAME || p_value.get_type() == Variant::NODE_PATH) {
		return to_std_string((String)p_value);
	}
	return std::string();
}

static std::string variant_to_json_string(const Variant &p_variant) {
	String json_text = JSON::stringify(p_variant, "", false, true);
	CharString utf8 = json_text.utf8();
	return std::string(utf8.get_data(), utf8.length());
}

static bool parse_json_payload(const std::string &p_payload, Variant &r_result, String &r_error) {
	Ref<JSON> json;
	json.instantiate();
	String payload = String::utf8(p_payload.c_str());
	Error err = json->parse(payload);
	if (err != OK) {
		r_error = json->get_error_message();
		return false;
	}
	r_result = json->get_data();
	return true;
}

static bool ensure_dictionary(const Variant &p_value, Dictionary &r_dict) {
	if (p_value.get_type() != Variant::DICTIONARY) {
		return false;
	}
	r_dict = p_value;
	return true;
}

} // namespace

// Private implementation
struct Client::Impl {
	ENetHost *host = nullptr;
	ENetPeer *peer = nullptr;
	bool connected = false;
	bool version_checked = false;
	std::string server_version;
	bool enet_initialized = false;

	// Callbacks
	OnSnapshotCallback on_snapshot;
	OnMoveStateCallback on_move_state;
	OnBattleStartCallback on_battle_start;
	OnBattleStateCallback on_battle_state;
	OnBattleLogCallback on_battle_log;
	OnBattleEndCallback on_battle_end;
	OnBattleIndicatorSpawnCallback on_battle_indicator_spawn;
	OnBattleIndicatorDespawnCallback on_battle_indicator_despawn;
	OnErrorCallback on_error;
	OnDisconnectCallback on_disconnect;

	// Reconnection state
	std::string last_jwt_token;
	std::string last_address;
	uint16_t last_port = 0;
	std::string reconnection_token;
	std::string session_id;
	bool auto_reconnect_enabled = true;
	int reconnect_attempts = 0;
	float reconnect_delay = 0.0f;
	bool is_reconnecting = false;

	// Reconnection callbacks
	OnReconnectingCallback on_reconnecting;
	OnReconnectedCallback on_reconnected;
	OnReconnectFailedCallback on_reconnect_failed;

	// Admin callbacks
	OnAdminReloadCallback on_admin_reload;
	OnAdminKickCallback on_admin_kick;
	OnAdminStatsCallback on_admin_stats;
	OnAdminBroadcastCallback on_admin_broadcast;

	bool debug_capture = false;
	size_t debug_history_limit = 64;
	std::vector<std::string> log_history;
	std::string last_error_message;
	std::string last_warning_message;
	std::string last_info_message;
};

void Client::append_log_entry(const char *p_level, const std::string &p_message) {
	if (!impl_ || !impl_->debug_capture) {
		return;
	}

	std::string entry = "[TownSDK] ";
	entry += p_level;
	entry += ": ";
	entry += p_message;
	impl_->log_history.push_back(entry);

	if (impl_->log_history.size() > impl_->debug_history_limit) {
		impl_->log_history.erase(impl_->log_history.begin());
	}
}

void Client::log_info(const std::string &p_message) {
	print_line("[TownSDK] " + String::utf8(p_message.c_str()));
	if (!impl_) {
		return;
	}
	impl_->last_info_message = p_message;
	append_log_entry("INFO", p_message);
}

void Client::log_warning(const std::string &p_message) {
	WARN_PRINT("[TownSDK] " + String::utf8(p_message.c_str()));
	if (!impl_) {
		return;
	}
	impl_->last_warning_message = p_message;
	append_log_entry("WARN", p_message);
}

void Client::log_error(const std::string &p_message) {
	ERR_PRINT("[TownSDK] " + String::utf8(p_message.c_str()));
	if (!impl_) {
		return;
	}
	impl_->last_error_message = p_message;
	append_log_entry("ERROR", p_message);
}

Client::Client() :
		impl_(std::make_unique<Impl>()) {
	if (enet_initialize() != 0) {
		log_error("Failed to initialize ENet");
		return;
	}
	impl_->enet_initialized = true;
}

Client::~Client() {
	disconnect();
	if (impl_ && impl_->enet_initialized) {
		enet_deinitialize();
		impl_->enet_initialized = false;
	}
}

bool Client::connect(const std::string &address, uint16_t port) {
	if (!is_ready()) {
		log_error("Cannot connect: ENet not initialized");
		return false;
	}
	if (impl_->connected) {
		return true; // Already connected
	}

	if (impl_->host) {
		disconnect(); // Clean up leaking host from previous session if disconnected abruptly
	}

	// Store for reconnection
	impl_->last_address = address;
	impl_->last_port = port;

	String address_str = String::utf8(address.c_str());
	log_info(to_std_string(vformat("Connecting to %s:%d...", address_str, port)));

	// Create client host (1 peer, 4 channels, no bandwidth limits)
	impl_->host = enet_host_create(
			nullptr, // Client (no specific bind address)
			1, // Max 1 outgoing connection
			4, // 4 channels (CONTROL, REGION, BATTLE, NOTIFICATIONS)
			0, // No download bandwidth limit
			0 // No upload bandwidth limit
	);

	if (!impl_->host) {
		log_error("Failed to create ENet host");
		return false;
	}

	// Resolve server address
	log_info(to_std_string(vformat("Resolving hostname '%s'...", address_str)));
	ENetAddress enet_address;
	if (enet_address_set_host(&enet_address, address.c_str()) != 0) {
		log_error(to_std_string(vformat("DNS resolution failed for '%s'", address_str)));
		enet_host_destroy(impl_->host);
		impl_->host = nullptr;
		return false;
	}
	enet_address.port = port;
	log_info("DNS resolved successfully");

	// Initiate connection (4 channels, no user data)
	log_info("Initiating ENet connection...");
	impl_->peer = enet_host_connect(impl_->host, &enet_address, 4, 0);

	if (!impl_->peer) {
		log_error("enet_host_connect returned null");
		enet_host_destroy(impl_->host);
		impl_->host = nullptr;
		return false;
	}

	// Async connection
	log_info("Connection initiated (async). Waiting for CONNECT event in poll...");
	return true;
}

bool Client::perform_version_check() {
	if (!is_ready()) {
		log_error("Cannot perform version check: ENet not initialized");
		return false;
	}
	if (!impl_->connected || !impl_->peer) {
		log_error("Cannot perform version check - not connected");
		return false;
	}

	// Send VERSION_CHECK with client version
	std::string client_ver = turnbattle::version::get_version_string();
	String client_ver_str = String::utf8(client_ver.c_str());
	log_info(to_std_string(vformat("Sending VERSION_CHECK (client version: %s)...", client_ver_str)));

	Dictionary version_msg;
	version_msg["version"] = client_ver_str;
	send_message(protocol::MessageType::VERSION_CHECK, variant_to_json_string(version_msg), protocol::Channel::CONTROL);

	// Wait for VERSION_CHECK response or VERSION_MISMATCH (3 second timeout)
	impl_->version_checked = false;

	log_info("Waiting for VERSION_CHECK response (3 second timeout)...");
	auto start_time = std::chrono::steady_clock::now();
	const auto timeout = std::chrono::seconds(3);

	while (std::chrono::steady_clock::now() - start_time < timeout) {
		ENetEvent event;
		if (enet_host_service(impl_->host, &event, 100) > 0) {
			if (event.type == ENET_EVENT_TYPE_RECEIVE) {
				// Parse message
				const char *data = reinterpret_cast<const char *>(event.packet->data);
				size_t len = event.packet->dataLength;

				if (len >= 4) {
					auto [type, payload_len] = protocol::decode_header(data, len);

					if (len >= 4 + static_cast<size_t>(payload_len)) {
						std::string payload(data + 4, payload_len);

						Variant parsed;
						String parse_error;
						if (!parse_json_payload(payload, parsed, parse_error)) {
							log_warning(to_std_string(vformat("Failed to parse VERSION_CHECK response: %s", parse_error)));
							enet_packet_destroy(event.packet);
							continue;
						}

						Dictionary data_dict;
						if (!ensure_dictionary(parsed, data_dict)) {
							log_warning("VERSION_CHECK response payload is not a dictionary");
							enet_packet_destroy(event.packet);
							continue;
						}

						// Check for VERSION_CHECK OK response
						if (type == protocol::MessageType::VERSION_CHECK) {
							String status = data_dict.get("status", "");
							if (status == "ok") {
								impl_->version_checked = true;
								if (data_dict.has("version")) {
									impl_->server_version = to_std_string((Variant)data_dict["version"]);
								}
								enet_packet_destroy(event.packet);
								return true;
							}
						}

						// Check for VERSION_MISMATCH
						if (type == protocol::MessageType::VERSION_MISMATCH) {
							std::string server_ver = to_std_string(data_dict.get("server_version", String("unknown")));
							std::string client_ver_reported = to_std_string(data_dict.get("client_version", String("unknown")));

							log_error(to_std_string(vformat("Version mismatch! Server version: %s Client version: %s",
									String::utf8(server_ver.c_str()), String::utf8(client_ver_reported.c_str()))));

							if (impl_->on_error) {
								impl_->on_error("VERSION_MISMATCH: server=" + server_ver +
										", client=" + client_ver_reported);
							}

							enet_packet_destroy(event.packet);
							return false;
						}
					}
				}

				enet_packet_destroy(event.packet);
			}
		}
	}

	// Timeout waiting for version check response
	log_error("Version check timeout (no response from server)");
	log_warning("Server may not be running or not processing VERSION_CHECK messages");
	if (impl_->on_error) {
		impl_->on_error("VERSION_CHECK timeout");
	}

	return false;
}

std::string Client::get_server_version() const {
	return impl_->server_version;
}

void Client::disconnect() {
	if (!is_ready()) {
		return;
	}
	if (!impl_->connected || !impl_->peer) {
		return;
	}

	enet_peer_disconnect(impl_->peer, 0);

	// Wait for disconnect event (up to 3 seconds)
	ENetEvent event;
	while (enet_host_service(impl_->host, &event, 3000) > 0) {
		if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
			break;
		}
	}

	if (impl_->host) {
		enet_host_destroy(impl_->host);
		impl_->host = nullptr;
	}

	impl_->peer = nullptr;
	impl_->connected = false;
}

bool Client::is_connected() const {
	return impl_->connected;
}

void Client::auth(const std::string &jwt_token) {
	// Store for reconnection
	impl_->last_jwt_token = jwt_token;

	Dictionary payload;
	payload["jwt"] = String::utf8(jwt_token.c_str());
	send_message(protocol::MessageType::HELLO, variant_to_json_string(payload), protocol::Channel::CONTROL);
}

void Client::enter_region(const std::string &region_id) {
	Dictionary payload;
	payload["region_id"] = String::utf8(region_id.c_str());
	send_message(protocol::MessageType::REGION_ENTER, variant_to_json_string(payload), protocol::Channel::CONTROL);
}

void Client::leave_region() {
	Dictionary payload;
	send_message(protocol::MessageType::REGION_LEAVE, variant_to_json_string(payload), protocol::Channel::CONTROL);
}

void Client::send_move(uint8_t held, float dt) {
	Dictionary payload;
	payload["held"] = (int)held;
	payload["dt"] = dt;
	send_message(protocol::MessageType::MOVE_INPUT, variant_to_json_string(payload), protocol::Channel::REGION);
}

void Client::battle_action(const std::string &battle_id, Action action, const std::string &target_id) {
	std::string action_str;
	switch (action) {
		case Action::ATTACK:
			action_str = "attack";
			break;
		case Action::BLOCK:
			action_str = "block";
			break;
		case Action::DEFEND:
			action_str = "defend";
			break;
	}

	Dictionary payload;
	payload["battle_id"] = String::utf8(battle_id.c_str());
	payload["action_type"] = String::utf8(action_str.c_str());

	if (!target_id.empty()) {
		payload["target_id"] = String::utf8(target_id.c_str());
	}

	send_message(protocol::MessageType::BATTLE_ACTION, variant_to_json_string(payload), protocol::Channel::BATTLE);
}

void Client::leave_battle(const std::string &battle_id) {
	Dictionary payload;
	payload["battle_id"] = String::utf8(battle_id.c_str());
	send_message(protocol::MessageType::BATTLE_LEAVE, variant_to_json_string(payload), protocol::Channel::BATTLE);
}

// Admin Commands
void Client::admin_reload(const std::string &scope) {
	Dictionary payload;
	payload["scope"] = String::utf8(scope.c_str());
	send_message(protocol::MessageType::ADMIN_RELOAD, variant_to_json_string(payload), protocol::Channel::CONTROL);
}

void Client::admin_kick(const std::string &username, const std::string &reason) {
	Dictionary payload;
	payload["username"] = String::utf8(username.c_str());
	payload["reason"] = String::utf8(reason.c_str());
	send_message(protocol::MessageType::ADMIN_KICK, variant_to_json_string(payload), protocol::Channel::CONTROL);
}

void Client::admin_stats_request() {
	Dictionary payload;
	send_message(protocol::MessageType::ADMIN_STATS_REQUEST, variant_to_json_string(payload), protocol::Channel::CONTROL);
}

void Client::admin_broadcast(const std::string &message, bool is_alert) {
	Dictionary payload;
	payload["message"] = String::utf8(message.c_str());
	payload["is_alert"] = is_alert;
	send_message(protocol::MessageType::ADMIN_BROADCAST, variant_to_json_string(payload), protocol::Channel::CONTROL);
}

// Callback setters
void Client::on_snapshot(OnSnapshotCallback cb) {
	impl_->on_snapshot = cb;
}
void Client::on_move_state(OnMoveStateCallback cb) {
	impl_->on_move_state = cb;
}
void Client::on_battle_start(OnBattleStartCallback cb) {
	impl_->on_battle_start = cb;
}
void Client::on_battle_state(OnBattleStateCallback cb) {
	impl_->on_battle_state = cb;
}
void Client::on_battle_log(OnBattleLogCallback cb) {
	impl_->on_battle_log = cb;
}
void Client::on_battle_end(OnBattleEndCallback cb) {
	impl_->on_battle_end = cb;
}
void Client::on_battle_indicator_spawn(OnBattleIndicatorSpawnCallback cb) {
	impl_->on_battle_indicator_spawn = cb;
}
void Client::on_battle_indicator_despawn(OnBattleIndicatorDespawnCallback cb) {
	impl_->on_battle_indicator_despawn = cb;
}
void Client::on_error(OnErrorCallback cb) {
	impl_->on_error = cb;
}
void Client::on_disconnect(OnDisconnectCallback cb) {
	impl_->on_disconnect = cb;
}

// Reconnection callback setters
void Client::on_reconnecting(OnReconnectingCallback cb) {
	impl_->on_reconnecting = cb;
}
void Client::on_reconnected(OnReconnectedCallback cb) {
	impl_->on_reconnected = cb;
}
void Client::on_reconnect_failed(OnReconnectFailedCallback cb) {
	impl_->on_reconnect_failed = cb;
}

// Admin callback setters
void Client::on_admin_reload(OnAdminReloadCallback cb) {
	impl_->on_admin_reload = cb;
}
void Client::on_admin_kick(OnAdminKickCallback cb) {
	impl_->on_admin_kick = cb;
}
void Client::on_admin_stats(OnAdminStatsCallback cb) {
	impl_->on_admin_stats = cb;
}
void Client::on_admin_broadcast(OnAdminBroadcastCallback cb) {
	impl_->on_admin_broadcast = cb;
}

void Client::set_auto_reconnect(bool enabled) {
	impl_->auto_reconnect_enabled = enabled;
}

void Client::manual_reconnect() {
	if (!impl_->is_reconnecting && !impl_->reconnection_token.empty()) {
		impl_->is_reconnecting = true;
		impl_->reconnect_attempts = 0;
		impl_->reconnect_delay = 0.0f;
	}
}

void Client::update(float dt) {
	if (!is_ready()) {
		return;
	}
	// Handle reconnection delay
	if (impl_->is_reconnecting && impl_->reconnect_delay > 0.0f) {
		impl_->reconnect_delay -= dt;
		if (impl_->reconnect_delay <= 0.0f) {
			// Time to attempt reconnection
			if (impl_->reconnect_attempts >= 3) {
				// Max attempts reached
				impl_->is_reconnecting = false;
				if (impl_->on_reconnect_failed) {
					impl_->on_reconnect_failed("Max reconnection attempts reached");
				}
			} else {
				// Attempt reconnection
				impl_->reconnect_attempts++;
				float delays[] = { 1.0f, 2.0f, 4.0f };
				impl_->reconnect_delay = delays[impl_->reconnect_attempts - 1];

				if (impl_->on_reconnecting) {
					impl_->on_reconnecting(impl_->reconnect_attempts, impl_->reconnect_delay);
				}

				// Use reconnection token if available, otherwise last JWT
				std::string token = !impl_->reconnection_token.empty()
						? impl_->reconnection_token
						: impl_->last_jwt_token;

				if (!token.empty() && !impl_->last_address.empty()) {
					disconnect(); // Clean up old connection
					if (connect(impl_->last_address, impl_->last_port)) {
						auth(token);
					}
				}
			}
		}
	}

	if (!impl_->host) {
		return;
	}

	ENetEvent event;
	while (impl_->host && enet_host_service(impl_->host, &event, 0) > 0) {
		switch (event.type) {
			case ENET_EVENT_TYPE_CONNECT:
				impl_->connected = true;
				impl_->is_reconnecting = false;
				log_info("Async connection established. Performing version check.");
				if (!perform_version_check()) {
					log_error("Version check failed to send");
					disconnect();
				} else {
					if (impl_->on_reconnected) {
						impl_->on_reconnected(Variant());
					}
				}
				break;

			case ENET_EVENT_TYPE_RECEIVE: {
				// Parse message
				const char *data = reinterpret_cast<const char *>(event.packet->data);
				size_t len = event.packet->dataLength;

				if (len >= 4) {
					auto [type, payload_len] = protocol::decode_header(data, len);

					if (len >= 4 + static_cast<size_t>(payload_len)) {
						std::string payload(data + 4, payload_len);
						handle_message(type, payload);
					}
				}

				enet_packet_destroy(event.packet);
				break;
			}

			case ENET_EVENT_TYPE_DISCONNECT:
				impl_->connected = false;
				if (impl_->on_disconnect) {
					impl_->on_disconnect("Disconnected from server");
				}

				// Trigger auto-reconnect if enabled and we have a reconnection token
				if (impl_->auto_reconnect_enabled && !impl_->reconnection_token.empty()) {
					impl_->is_reconnecting = true;
					impl_->reconnect_attempts = 0;
					impl_->reconnect_delay = 1.0f; // Start with 1 second delay

					if (impl_->on_reconnecting) {
						impl_->on_reconnecting(0, impl_->reconnect_delay);
					}
				}
				break;

			default:
				break;
		}
	}
}

void Client::handle_message(uint16_t type, const std::string &payload) {
	Variant parsed;
	String parse_error;
	if (!parse_json_payload(payload, parsed, parse_error)) {
		log_warning(to_std_string(vformat("Failed to parse message type %d: %s", type, parse_error)));
		if (impl_->on_error) {
			impl_->on_error("JSON parse error: " + to_std_string(parse_error));
		}
		return;
	}

	switch (type) {
		case protocol::MessageType::HELLO_ACK: {
			Dictionary data;
			if (!ensure_dictionary(parsed, data)) {
				log_warning("HELLO_ACK payload is not a dictionary");
				return;
			}

			if (data.has("session_id")) {
				impl_->session_id = to_std_string(data["session_id"]);
			}

			bool resumed = (bool)data.get("resumed", false);
			if (resumed) {
				impl_->is_reconnecting = false;
				impl_->reconnect_attempts = 0;

				if (impl_->on_reconnected) {
					impl_->on_reconnected(data.get("resume_state", Variant()));
				}
			}

			if (data.has("reconnection_token")) {
				impl_->reconnection_token = to_std_string(data["reconnection_token"]);
			}
			break;
		}

		case protocol::MessageType::REGION_SNAPSHOT:
			if (impl_->on_snapshot) {
				impl_->on_snapshot(parsed);
			}
			break;

		case protocol::MessageType::MOVE_STATE:
			if (impl_->on_move_state) {
				impl_->on_move_state(parsed);
			}
			break;

		case protocol::MessageType::BATTLE_INDICATOR_SPAWN:
			if (impl_->on_battle_indicator_spawn) {
				impl_->on_battle_indicator_spawn(parsed);
			}
			break;

		case protocol::MessageType::BATTLE_INDICATOR_DESPAWN:
			if (impl_->on_battle_indicator_despawn) {
				impl_->on_battle_indicator_despawn(parsed);
			}
			break;

		case protocol::MessageType::BATTLE_START:
			if (impl_->on_battle_start) {
				impl_->on_battle_start(parsed);
			}
			break;

		case protocol::MessageType::BATTLE_STATE:
			if (impl_->on_battle_state) {
				impl_->on_battle_state(parsed);
			}
			break;

		case protocol::MessageType::BATTLE_LOG:
			if (impl_->on_battle_log) {
				Dictionary data;
				if (ensure_dictionary(parsed, data) && data.has("log")) {
					impl_->on_battle_log(to_std_string(data["log"]));
				}
			}
			break;

		case protocol::MessageType::BATTLE_END:
		case protocol::MessageType::BATTLE_RESULT:
			if (impl_->on_battle_end) {
				impl_->on_battle_end(parsed);
			}
			break;

		case protocol::MessageType::RELOAD_OK:
			if (impl_->on_admin_reload) {
				impl_->on_admin_reload(parsed);
			}
			break;

		case protocol::MessageType::ADMIN_KICK:
			if (impl_->on_admin_kick) {
				impl_->on_admin_kick(parsed);
			}
			break;

		case protocol::MessageType::ADMIN_STATS_RESPONSE:
			if (impl_->on_admin_stats) {
				impl_->on_admin_stats(parsed);
			}
			break;

		case protocol::MessageType::ADMIN_BROADCAST:
			if (impl_->on_admin_broadcast) {
				impl_->on_admin_broadcast(parsed);
			}
			break;

		case protocol::MessageType::ERROR_MSG:
			if (impl_->on_error) {
				Dictionary data;
				std::string error_message = "unknown_error";
				if (ensure_dictionary(parsed, data) && data.has("error")) {
					error_message = to_std_string(data["error"]);
				}
				impl_->on_error(error_message);
			}
			break;

		case protocol::MessageType::DISCONNECT: {
			Dictionary data;
			std::string reason = "unknown";
			if (ensure_dictionary(parsed, data) && data.has("reason")) {
				reason = to_std_string(data["reason"]);
			}
			if (impl_->on_disconnect) {
				impl_->on_disconnect(reason);
			}
			impl_->connected = false;

			// Trigger auto-reconnect if enabled and we have a reconnection token
			if (impl_->auto_reconnect_enabled && !impl_->reconnection_token.empty()) {
				impl_->is_reconnecting = true;
				impl_->reconnect_attempts = 0;
				impl_->reconnect_delay = 1.0f; // Start with 1 second delay

				if (impl_->on_reconnecting) {
					impl_->on_reconnecting(0, impl_->reconnect_delay);
				}
			}
			break;
		}

		default:
			// Unknown message type, ignore
			break;
	}
}

void Client::send_message(uint16_t type, const std::string &payload, uint8_t channel) {
	if (!is_ready()) {
		return;
	}
	if (!impl_->connected || !impl_->peer) {
		return;
	}

	std::string encoded = protocol::encode_message(type, payload);

	// Create ENet packet (reliable, no fragmentation for now)
	ENetPacket *packet = enet_packet_create(
			encoded.data(),
			encoded.size(),
			ENET_PACKET_FLAG_RELIABLE);

	if (packet) {
		enet_peer_send(impl_->peer, channel, packet);
		enet_host_flush(impl_->host);
	}
}

void Client::set_debug_logging_enabled(bool enabled, size_t history_limit) {
	if (!impl_) {
		return;
	}

	impl_->debug_capture = enabled;
	impl_->debug_history_limit = history_limit == 0 ? 1 : history_limit;

	if (!enabled) {
		impl_->log_history.clear();
	}
}

bool Client::is_debug_logging_enabled() const {
	return impl_ && impl_->debug_capture;
}

std::vector<std::string> Client::get_debug_log() const {
	if (!impl_) {
		return {};
	}
	return impl_->log_history;
}

void Client::clear_debug_log() {
	if (!impl_) {
		return;
	}
	impl_->log_history.clear();
}

std::string Client::get_last_error_message() const {
	return impl_ ? impl_->last_error_message : std::string();
}

std::string Client::get_last_warning_message() const {
	return impl_ ? impl_->last_warning_message : std::string();
}

std::string Client::get_last_info_message() const {
	return impl_ ? impl_->last_info_message : std::string();
}

bool Client::is_ready() const {
	return impl_ && impl_->enet_initialized;
}

} // namespace turnbattle
