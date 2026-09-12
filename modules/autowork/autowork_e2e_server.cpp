/**************************************************************************/
/*  autowork_e2e_server.cpp                                               */
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

#include "autowork_e2e_server.h"

#include "autowork_e2e_config.h"
#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/crypto/crypto.h"
#include "core/crypto/crypto_core.h"
#include "core/input/input.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/json.h"
#include "core/math/expression.h"
#include "core/math/random_number_generator.h"
#include "core/os/time.h"
#include "scene/2d/node_2d.h"
#include "scene/gui/control.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "servers/audio_server.h"
#include "servers/display_server.h"

const String AutoworkE2EServer::SERVER_VERSION = "1.1.0";

void AutoworkE2EServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_wait_signal_emitted"), &AutoworkE2EServer::_on_wait_signal_emitted);
}

AutoworkE2EServer::AutoworkE2EServer() {
	if (!AutoworkE2EConfig::is_enabled()) {
		set_process(false);
		set_physics_process(false);
		return;
	}

	server.instantiate();
	int port = AutoworkE2EConfig::get_port();
	String host = AutoworkE2EConfig::get_host();

	if (port == 0) {
		port = _listen_random_port(host);
		if (port == -1) {
			return;
		}
	} else {
		Error err = server->listen(port, host);
		if (err != OK) {
			ERR_PRINT(vformat("blazium-e2e: failed to listen on port %d (error %d)", port, err));
			set_process(false);
			set_physics_process(false);
			return;
		}
	}

	String port_file = AutoworkE2EConfig::get_port_file();
	if (!port_file.is_empty()) {
		_write_port_file(port_file, port);
	}

	_log(vformat("server listening on %s:%d", host, port));

	set_process(true);
	set_physics_process(true);
}

AutoworkE2EServer::~AutoworkE2EServer() {
}

void AutoworkE2EServer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PROCESS: {
			if (!AutoworkE2EConfig::is_enabled()) {
				return;
			}
			float delta = get_process_delta_time();
			_poll_listening();
			for (int i = 0; i < peers.size(); i++) {
				PeerContext *ctx = peers[i];
				switch (ctx->state) {
					case STATE_IDLE:
						_poll_connection_health(ctx);
						if (ctx->state == STATE_DISCONNECTED) {
							break;
						}
						_poll_recv(ctx);
						break;
					case STATE_WAITING:
						_poll_connection_health(ctx);
						if (ctx->state == STATE_DISCONNECTED) {
							break;
						}
						_poll_wait(ctx, delta);
						break;
					case STATE_DISCONNECTED:
						_handle_disconnect(ctx);
						break;
					default:
						break;
				}
			}

			// Cleanup disconnected peers
			for (int i = peers.size() - 1; i >= 0; i--) {
				if (peers[i]->state == STATE_DISCONNECTED) {
					memdelete(peers[i]);
					peers.remove_at(i);
				}
			}
		} break;

		case NOTIFICATION_PHYSICS_PROCESS: {
			if (!AutoworkE2EConfig::is_enabled()) {
				return;
			}
			physics_frame_counter++;
			for (int i = 0; i < peers.size(); i++) {
				PeerContext *ctx = peers[i];
				if (ctx->state == STATE_WAITING && ctx->wait_type == WAIT_PHYSICS_FRAMES) {
					ctx->wait_physics_remaining--;
					if (ctx->wait_physics_remaining <= 0) {
						Dictionary res;
						res["id"] = ctx->pending_id;
						res["ok"] = true;
						_send_response(ctx, res);
						_transition_idle(ctx);
					}
				}
			}
		} break;
	}
}

// ---------------------------------------------------------------------------
// Networking helpers
// ---------------------------------------------------------------------------

void AutoworkE2EServer::_poll_listening() {
	if (server.is_null()) {
		return;
	}
	static int next_peer_id = 1;
	while (server->is_connection_available()) {
		PeerContext *ctx = memnew(PeerContext);
		ctx->peer_id = next_peer_id++;

		Ref<WebSocketPeer> ws = Ref<WebSocketPeer>(WebSocketPeer::create());
		ws->accept_stream(server->take_connection());
		ctx->peer = ws;

		ctx->state = STATE_IDLE;

		Ref<Crypto> crypto = Ref<Crypto>(Crypto::create());
		if (crypto.is_valid()) {
			ctx->challenge_nonce = crypto->generate_random_bytes(32);
		} else {
			ctx->challenge_nonce.resize(32);
			for (int i = 0; i < 32; i++) {
				ctx->challenge_nonce.write[i] = (uint8_t)rand();
			}
		}

		peers.push_back(ctx);
		_log("client connected, waiting for web socket upgrade handshake");
	}
}

void AutoworkE2EServer::_poll_connection_health(PeerContext *p_ctx) {
	if (p_ctx->peer.is_null()) {
		p_ctx->state = STATE_DISCONNECTED;
		return;
	}
	p_ctx->peer->poll();
	WebSocketPeer::State ws_state = p_ctx->peer->get_ready_state();
	if (ws_state == WebSocketPeer::STATE_CLOSED) {
		_log(vformat("connection lost (state %d)", ws_state));
		p_ctx->state = STATE_DISCONNECTED;
	} else if (ws_state == WebSocketPeer::STATE_OPEN && !p_ctx->handshake_sent) {
		p_ctx->handshake_sent = true;
		Dictionary challenge;
		challenge["action"] = "auth_challenge";
		challenge["nonce"] = String::hex_encode_buffer(p_ctx->challenge_nonce.ptr(), p_ctx->challenge_nonce.size());
		_send_response(p_ctx, challenge);
		_log("websocket opened, auth challenge sent");
	}
}

void AutoworkE2EServer::_poll_recv(PeerContext *p_ctx) {
	if (p_ctx->peer->get_ready_state() != WebSocketPeer::STATE_OPEN) {
		return; // Wait for handshake completion
	}

	uint64_t start_time = Time::get_singleton()->get_ticks_msec();

	while (p_ctx->peer->get_available_packet_count() > 0) {
		if (Time::get_singleton()->get_ticks_msec() - start_time > 10) {
			_log("Yielding dispatch queue processing due to frame time budget (10ms)");
			break;
		}

		int len = 0;
		const uint8_t *packet;
		Error err = p_ctx->peer->get_packet(&packet, len);

		if (err != OK) {
			_log(vformat("recv error %d", err));
			p_ctx->state = STATE_DISCONNECTED;
			return;
		}

		String json_str;
		json_str.parse_utf8((const char *)packet, len);

		Variant parsed = JSON::parse_string(json_str);
		if (parsed.get_type() == Variant::NIL) {
			_log(vformat("invalid JSON: %s", json_str));
			Dictionary err_res;
			err_res["error"] = "invalid_json";
			err_res["message"] = "Could not parse JSON";
			_send_response(p_ctx, err_res);
			continue;
		}

		if (parsed.get_type() != Variant::DICTIONARY) {
			Dictionary err_res;
			err_res["error"] = "invalid_message";
			err_res["message"] = "Expected JSON object";
			_send_response(p_ctx, err_res);
			continue;
		}

		Dictionary cmd = parsed;
		_dispatch(p_ctx, cmd);

		if (p_ctx->state != STATE_IDLE) {
			break;
		}
	}
}

// ---------------------------------------------------------------------------
// Framing
// ---------------------------------------------------------------------------

void AutoworkE2EServer::_send_response(PeerContext *p_ctx, const Dictionary &p_data) {
	if (p_ctx->peer.is_null() || p_ctx->peer->get_ready_state() != WebSocketPeer::STATE_OPEN) {
		return;
	}
	Dictionary payload = p_data.duplicate();
	payload["frame_stamp"] = Engine::get_singleton()->get_process_frames();

	String json_str = JSON::stringify(payload);
	CharString utf8 = json_str.utf8();

	uint32_t size = utf8.length();
	if (size > 0) {
		Error err = p_ctx->peer->send((const uint8_t *)utf8.get_data(), size, WebSocketPeer::WRITE_MODE_TEXT);
		if (err != OK) {
			_log(vformat("Failed to send WebSocket response, error %d", err));
		}
	}
	_log(vformat(">> %s", json_str));
}

// ---------------------------------------------------------------------------
// Command dispatch
// ---------------------------------------------------------------------------

void AutoworkE2EServer::_dispatch(PeerContext *p_ctx, const Dictionary &p_cmd) {
	String action = p_cmd.get("action", "");
	Variant cmd_id = p_cmd.get("id", Variant());

	_log(vformat("<< %s (id=%s)", action, cmd_id));

	if (!p_ctx->authenticated) {
		if (action != "hello") {
			Dictionary err_res;
			err_res["id"] = cmd_id;
			err_res["error"] = "not_authenticated";
			err_res["message"] = "First command must be 'hello'";
			_send_response(p_ctx, err_res);
			_disconnect_peer(p_ctx);
			return;
		}
		_handle_hello(p_ctx, p_cmd);
		return;
	}

	Dictionary result = _execute_command(p_cmd);

	if (result.has("_deferred")) {
		_enter_wait(p_ctx, result);
	} else {
		_send_response(p_ctx, result);
	}
}

// ---------------------------------------------------------------------------
// Handshake
// ---------------------------------------------------------------------------

void AutoworkE2EServer::_handle_hello(PeerContext *p_ctx, const Dictionary &p_cmd) {
	Variant cmd_id = p_cmd.get("id", Variant());
	String hmac_provided = p_cmd.get("hmac", "");
	String expected_token = AutoworkE2EConfig::get_token();

	if (!expected_token.is_empty()) {
		Ref<HMACContext> hmac = Ref<HMACContext>(HMACContext::create());
		PackedByteArray key = expected_token.to_utf8_buffer();
		hmac->start(HashingContext::HASH_SHA256, key);
		hmac->update(p_ctx->challenge_nonce);
		PackedByteArray expected_hmac = hmac->finish();
		String expected_hmac_hex = String::hex_encode_buffer(expected_hmac.ptr(), expected_hmac.size());

		if (hmac_provided != expected_hmac_hex) {
			Dictionary err_res;
			err_res["id"] = cmd_id;
			err_res["error"] = "auth_failed";
			err_res["message"] = "HMAC Token mismatch";
			_send_response(p_ctx, err_res);
			_disconnect_peer(p_ctx);
			return;
		}
	}

	p_ctx->authenticated = true;
	Dictionary version_info = Engine::get_singleton()->get_version_info();
	String godot_version = vformat("%d.%d.%d", version_info["major"], version_info["minor"], version_info["patch"]);

	Dictionary res;
	res["id"] = cmd_id;
	res["ok"] = true;
	res["godot_version"] = godot_version;
	res["server_version"] = SERVER_VERSION;
	_send_response(p_ctx, res);
	_log("authenticated");
}

// ---------------------------------------------------------------------------
// Wait state management
// ---------------------------------------------------------------------------

void AutoworkE2EServer::_enter_wait(PeerContext *p_ctx, const Dictionary &p_params) {
	p_ctx->pending_id = p_params.get("id", Variant());
	p_ctx->state = STATE_WAITING;
	p_ctx->wait_start_ms = Time::get_singleton()->get_ticks_msec();

	float timeout_sec = p_params.get("timeout", 0.0f);
	p_ctx->wait_timeout_ms = (uint64_t)(timeout_sec * 1000.0f);

	String wait_type_str = p_params.get("wait_type", "");

	if (wait_type_str == "process_frames") {
		p_ctx->wait_type = WAIT_PROCESS_FRAMES;
		p_ctx->wait_process_remaining = p_params.get("count", 1);
	} else if (wait_type_str == "physics_frames") {
		p_ctx->wait_type = WAIT_PHYSICS_FRAMES;
		p_ctx->wait_physics_remaining = p_params.get("count", 1);
	} else if (wait_type_str == "seconds") {
		p_ctx->wait_type = WAIT_SECONDS;
		p_ctx->wait_seconds_elapsed = 0.0f;
		p_ctx->wait_seconds_target = p_params.get("duration", 1.0f);
	} else if (wait_type_str == "node_exists") {
		p_ctx->wait_type = WAIT_NODE_EXISTS;
		p_ctx->wait_node_path = p_params.get("path", "");
		p_ctx->wait_node_cache = nullptr;
	} else if (wait_type_str == "signal") {
		p_ctx->wait_type = WAIT_SIGNAL_EMITTED;
		p_ctx->wait_signal_emitted = false;
		String source_path = p_params.get("path", "");
		String sig_name = p_params.get("signal_name", "");

		Node *source = get_tree()->get_root()->get_node_or_null(source_path);
		if (!source) {
			Dictionary err_res;
			err_res["id"] = p_ctx->pending_id;
			err_res["error"] = "node_not_found";
			err_res["message"] = vformat("Signal source '%s' not found", source_path);
			_send_response(p_ctx, err_res);
			_transition_idle(p_ctx);
			return;
		}

		p_ctx->wait_signal_source = source;
		p_ctx->wait_signal_name = sig_name;
		p_ctx->wait_signal_connection = callable_mp(this, &AutoworkE2EServer::_on_wait_signal_emitted).bind(p_ctx->peer_id);

		if (source->has_signal(sig_name)) {
			source->connect(sig_name, p_ctx->wait_signal_connection, CONNECT_ONE_SHOT);
		} else {
			Dictionary err_res;
			err_res["id"] = p_ctx->pending_id;
			err_res["error"] = "signal_not_found";
			err_res["message"] = vformat("Signal '%s' not found on '%s'", sig_name, source_path);
			_send_response(p_ctx, err_res);
			_transition_idle(p_ctx);
			return;
		}
	} else if (wait_type_str == "property") {
		p_ctx->wait_type = WAIT_PROPERTY_VALUE;
		p_ctx->wait_property_node_path = p_params.get("path", "");
		p_ctx->wait_property_name = p_params.get("property", "");
		p_ctx->wait_property_value = p_params.get("value", Variant());
		p_ctx->wait_node_cache = nullptr;
	} else if (wait_type_str == "scene_change") {
		p_ctx->wait_type = WAIT_SCENE_CHANGE;
		p_ctx->wait_scene_path = p_params.get("scene_path", "");
	} else {
		Dictionary err_res;
		err_res["id"] = p_ctx->pending_id;
		err_res["error"] = "unknown_wait_type";
		err_res["message"] = vformat("Unknown wait type '%s'", wait_type_str);
		_send_response(p_ctx, err_res);
		_transition_idle(p_ctx);
	}
}

void AutoworkE2EServer::_on_wait_signal_emitted(int p_peer_id) {
	for (int i = 0; i < peers.size(); i++) {
		if (peers[i]->peer_id == p_peer_id) {
			peers[i]->wait_signal_emitted = true;
			break;
		}
	}
}

void AutoworkE2EServer::_poll_wait(PeerContext *p_ctx, float p_delta) {
	if (p_ctx->wait_timeout_ms > 0) {
		uint64_t elapsed_ms = Time::get_singleton()->get_ticks_msec() - p_ctx->wait_start_ms;
		if (elapsed_ms >= p_ctx->wait_timeout_ms) {
			Dictionary err_res;
			err_res["id"] = p_ctx->pending_id;
			err_res["error"] = "timeout";
			err_res["message"] = vformat("Wait timed out after %d ms", p_ctx->wait_timeout_ms);
			_send_response(p_ctx, err_res);
			_cleanup_wait(p_ctx);
			_transition_idle(p_ctx);
			return;
		}
	}

	switch (p_ctx->wait_type) {
		case WAIT_PROCESS_FRAMES: {
			p_ctx->wait_process_remaining--;
			if (p_ctx->wait_process_remaining <= 0) {
				Dictionary res;
				res["id"] = p_ctx->pending_id;
				res["ok"] = true;
				_send_response(p_ctx, res);
				_transition_idle(p_ctx);
			}
		} break;
		case WAIT_SECONDS: {
			p_ctx->wait_seconds_elapsed += p_delta;
			if (p_ctx->wait_seconds_elapsed >= p_ctx->wait_seconds_target) {
				Dictionary res;
				res["id"] = p_ctx->pending_id;
				res["ok"] = true;
				_send_response(p_ctx, res);
				_transition_idle(p_ctx);
			}
		} break;
		case WAIT_NODE_EXISTS: {
			if (!p_ctx->wait_node_cache) {
				p_ctx->wait_node_cache = get_tree()->get_root()->get_node_or_null(p_ctx->wait_node_path);
			}
			if (p_ctx->wait_node_cache != nullptr) {
				Dictionary res;
				res["id"] = p_ctx->pending_id;
				res["ok"] = true;
				_send_response(p_ctx, res);
				_transition_idle(p_ctx);
			}
		} break;
		case WAIT_SIGNAL_EMITTED: {
			if (p_ctx->wait_signal_emitted) {
				Dictionary res;
				res["id"] = p_ctx->pending_id;
				res["ok"] = true;
				_send_response(p_ctx, res);
				_transition_idle(p_ctx);
			}
		} break;
		case WAIT_PROPERTY_VALUE: {
			if (!p_ctx->wait_node_cache) {
				p_ctx->wait_node_cache = get_tree()->get_root()->get_node_or_null(p_ctx->wait_property_node_path);
			}
			if (p_ctx->wait_node_cache != nullptr) {
				Variant val = p_ctx->wait_node_cache->get(p_ctx->wait_property_name);

				Variant ret;
				bool is_valid = false;
				Variant::evaluate(Variant::OP_EQUAL, val, p_ctx->wait_property_value, ret, is_valid);

				if (is_valid && ret.operator bool()) {
					Dictionary res;
					res["id"] = p_ctx->pending_id;
					res["ok"] = true;
					_send_response(p_ctx, res);
					_transition_idle(p_ctx);
				}
			}
		} break;
		case WAIT_SCENE_CHANGE: {
			Node *current_scene = get_tree()->get_current_scene();
			if (current_scene != nullptr) {
				String scene_file = current_scene->get_scene_file_path();
				if (p_ctx->wait_scene_path.is_empty() || scene_file == p_ctx->wait_scene_path) {
					Dictionary res;
					res["id"] = p_ctx->pending_id;
					res["ok"] = true;
					_send_response(p_ctx, res);
					_transition_idle(p_ctx);
				}
			}
		} break;
		case WAIT_PHYSICS_FRAMES: {
			// Handled in _notification
		} break;
		default:
			break;
	}
}

void AutoworkE2EServer::_cleanup_wait(PeerContext *p_ctx) {
	if (p_ctx->wait_type == WAIT_SIGNAL_EMITTED && p_ctx->wait_signal_source != nullptr) {
		if (!p_ctx->wait_signal_emitted && p_ctx->wait_signal_source->is_connected(p_ctx->wait_signal_name, p_ctx->wait_signal_connection)) {
			p_ctx->wait_signal_source->disconnect(p_ctx->wait_signal_name, p_ctx->wait_signal_connection);
		}
		p_ctx->wait_signal_source = nullptr;
		p_ctx->wait_signal_connection = Callable();
		p_ctx->wait_signal_name = "";
	}
	p_ctx->wait_node_cache = nullptr;
}

void AutoworkE2EServer::_transition_idle(PeerContext *p_ctx) {
	_cleanup_wait(p_ctx);
	p_ctx->state = STATE_IDLE;
	p_ctx->pending_id = Variant();
	p_ctx->wait_type = WAIT_NONE;
}

void AutoworkE2EServer::_disconnect_peer(PeerContext *p_ctx) {
	if (p_ctx->peer.is_valid()) {
		p_ctx->peer->close(1000, "Normal Closure");
		p_ctx->peer.unref();
	}
	p_ctx->state = STATE_DISCONNECTED;
}

void AutoworkE2EServer::_handle_disconnect(PeerContext *p_ctx) {
	_cleanup_wait(p_ctx);
	p_ctx->peer.unref();
	p_ctx->handshake_sent = false;
	p_ctx->authenticated = false;
	p_ctx->pending_id = Variant();
	p_ctx->wait_type = WAIT_NONE;
	p_ctx->state = STATE_DISCONNECTED;
	_log("peer disconnected and cleaned up");
}

int AutoworkE2EServer::_listen_random_port(const String &p_host) {
	Ref<RandomNumberGenerator> rng;
	rng.instantiate();
	rng->randomize();
	for (int i = 0; i < 100; i++) {
		int candidate = rng->randi_range(10000, 60000);
		if (server->listen(candidate, p_host) == OK) {
			return candidate;
		}
	}
	ERR_PRINT("blazium-e2e: failed to find a free port after 100 attempts");
	set_process(false);
	set_physics_process(false);
	return -1;
}

void AutoworkE2EServer::_write_port_file(const String &p_path, int p_port) {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE);
	if (file.is_null()) {
		ERR_PRINT(vformat("blazium-e2e: failed to write port file '%s' (error %d)", p_path, FileAccess::get_open_error()));
		return;
	}
	file->store_string(itos(p_port));
	file->close();
	_log(vformat("wrote port %d to '%s'", p_port, p_path));
}

void AutoworkE2EServer::_log(const String &p_msg) {
	if (AutoworkE2EConfig::is_logging()) {
		print_line("[blazium-e2e] " + p_msg);
	}
}

// ---------------------------------------------------------------------------
// Command Handler
// ---------------------------------------------------------------------------

Dictionary AutoworkE2EServer::_execute_command(const Dictionary &p_cmd) {
	String action = p_cmd.get("action", "");
	Variant id = p_cmd.get("id", Variant());

	if (action == "node_exists") {
		return _cmd_node_exists(p_cmd, id);
	}
	if (action == "get_property") {
		return _cmd_get_property(p_cmd, id);
	}
	if (action == "set_property") {
		return _cmd_set_property(p_cmd, id);
	}
	if (action == "call_method") {
		return _cmd_call_method(p_cmd, id);
	}
	if (action == "find_by_group") {
		return _cmd_find_by_group(p_cmd, id);
	}
	if (action == "query_nodes") {
		return _cmd_query_nodes(p_cmd, id);
	}
	if (action == "get_tree") {
		return _cmd_get_tree(p_cmd, id);
	}
	if (action == "batch") {
		return _cmd_batch(p_cmd, id);
	}
	if (action == "input_key") {
		return _cmd_input_key(p_cmd, id);
	}
	if (action == "input_action") {
		return _cmd_input_action(p_cmd, id);
	}
	if (action == "input_mouse_button") {
		return _cmd_input_mouse_button(p_cmd, id);
	}
	if (action == "input_mouse_motion") {
		return _cmd_input_mouse_motion(p_cmd, id);
	}
	if (action == "click_node") {
		return _cmd_click_node(p_cmd, id);
	}
	if (action == "wait_process_frames") {
		return _cmd_wait_process_frames(p_cmd, id);
	}
	if (action == "wait_physics_frames") {
		return _cmd_wait_physics_frames(p_cmd, id);
	}
	if (action == "wait_seconds") {
		return _cmd_wait_seconds(p_cmd, id);
	}
	if (action == "wait_for_node") {
		return _cmd_wait_for_node(p_cmd, id);
	}
	if (action == "wait_for_signal") {
		return _cmd_wait_for_signal(p_cmd, id);
	}
	if (action == "wait_for_property") {
		return _cmd_wait_for_property(p_cmd, id);
	}
	if (action == "get_scene") {
		return _cmd_get_scene(p_cmd, id);
	}
	if (action == "change_scene") {
		return _cmd_change_scene(p_cmd, id);
	}
	if (action == "reload_scene") {
		return _cmd_reload_scene(p_cmd, id);
	}
	if (action == "set_time_scale") {
		return _cmd_set_time_scale(p_cmd, id);
	}
	if (action == "set_audio_mute") {
		return _cmd_set_audio_mute(p_cmd, id);
	}
	if (action == "screenshot" || action == "capture_screenshot") {
		return _cmd_screenshot(p_cmd, id);
	}
	if (action == "quit") {
		return _cmd_quit(p_cmd, id);
	}

	Dictionary res;
	res["id"] = id;
	res["error"] = "Unknown command: " + action;
	return res;
}

Variant AutoworkE2EServer::_deserialize_variant(const Variant &p_val) {
	return p_val; // For C++ we directly use Variant parsing built into JSON.
}

Variant AutoworkE2EServer::_serialize_variant(const Variant &p_val) {
	return p_val;
}

Dictionary AutoworkE2EServer::_cmd_node_exists(const Dictionary &p_cmd, const Variant &p_id) {
	String path = p_cmd.get("path", "");
	Node *node = get_tree()->get_root()->get_node_or_null(path);
	Dictionary res;
	res["id"] = p_id;
	res["exists"] = (node != nullptr);
	return res;
}

Dictionary AutoworkE2EServer::_cmd_get_property(const Dictionary &p_cmd, const Variant &p_id) {
	String path = p_cmd.get("path", "");
	String property = p_cmd.get("property", "");
	Node *node = get_tree()->get_root()->get_node_or_null(path);
	Dictionary res;

	if (!node) {
		res["id"] = p_id;
		res["error"] = "Node not found: " + path;
		return res;
	}

	Variant value = node->get(property);
	if (value.get_type() == Variant::NIL && !_get_property_list_names(node).has(property)) {
		String base_prop = property.split(":")[0];
		if (node->get(base_prop).get_type() == Variant::NIL && !_get_property_list_names(node).has(base_prop)) {
			res["id"] = p_id;
			res["error"] = "Property not found: " + property + " on " + path;
			return res;
		}
	}
	res["id"] = p_id;
	res["result"] = _serialize_variant(value);
	return res;
}

Dictionary AutoworkE2EServer::_cmd_set_property(const Dictionary &p_cmd, const Variant &p_id) {
	String path = p_cmd.get("path", "");
	String property = p_cmd.get("property", "");
	Variant raw_value = p_cmd.get("value", Variant());
	Node *node = get_tree()->get_root()->get_node_or_null(path);
	Dictionary res;

	if (!node) {
		res["id"] = p_id;
		res["error"] = "Node not found: " + path;
		return res;
	}

	Variant value = _deserialize_variant(raw_value);
	node->set(property, value);
	res["id"] = p_id;
	res["ok"] = true;
	return res;
}

Dictionary AutoworkE2EServer::_cmd_call_method(const Dictionary &p_cmd, const Variant &p_id) {
	String path = p_cmd.get("path", "");
	String method = p_cmd.get("method", "");
	Array raw_args = p_cmd.get("args", Array());
	Node *node = get_tree()->get_root()->get_node_or_null(path);
	Dictionary res;

	if (!node) {
		res["id"] = p_id;
		res["error"] = "Node not found: " + path;
		return res;
	}

	if (!node->has_method(method)) {
		res["id"] = p_id;
		res["error"] = "Method call failed: " + method + " not found on " + path;
		return res;
	}

	Vector<Variant> var_args;
	for (int i = 0; i < raw_args.size(); i++) {
		var_args.push_back(_deserialize_variant(raw_args[i]));
	}

	Callable::CallError err;
	const Variant **argptrs = nullptr;
	if (var_args.size() > 0) {
		argptrs = (const Variant **)alloca(sizeof(Variant *) * var_args.size());
		for (int i = 0; i < var_args.size(); i++) {
			argptrs[i] = &var_args[i];
		}
	}

	Variant result = node->callp(method, argptrs, var_args.size(), err);

	res["id"] = p_id;
	res["result"] = _serialize_variant(result);
	return res;
}

Dictionary AutoworkE2EServer::_cmd_find_by_group(const Dictionary &p_cmd, const Variant &p_id) {
	String group = p_cmd.get("group", "");
	List<Node *> nodes;
	get_tree()->get_nodes_in_group(group, &nodes);

	Array paths;
	for (Node *node : nodes) {
		paths.push_back(node->get_path());
	}

	Dictionary res;
	res["id"] = p_id;
	res["nodes"] = paths;
	return res;
}

void AutoworkE2EServer::_walk_tree_match(Node *p_node, const String &p_pattern, Array &r_results) {
	if (String(p_node->get_name()).match(p_pattern)) {
		r_results.push_back(p_node->get_path());
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		_walk_tree_match(p_node->get_child(i), p_pattern, r_results);
	}
}

Dictionary AutoworkE2EServer::_cmd_query_nodes(const Dictionary &p_cmd, const Variant &p_id) {
	String pattern = p_cmd.get("pattern", "");
	String group = p_cmd.get("group", "");
	Array results;

	if (!group.is_empty()) {
		List<Node *> group_nodes;
		get_tree()->get_nodes_in_group(group, &group_nodes);

		if (pattern.is_empty()) {
			for (Node *node : group_nodes) {
				results.push_back(node->get_path());
			}
		} else {
			for (Node *node : group_nodes) {
				if (String(node->get_name()).match(pattern)) {
					results.push_back(node->get_path());
				}
			}
		}
	} else if (!pattern.is_empty()) {
		_walk_tree_match(get_tree()->get_root(), pattern, results);
	}

	if (p_cmd.has("expression")) {
		String expr_str = p_cmd["expression"];
		Ref<Expression> expr;
		expr.instantiate();
		Error err = expr->parse(expr_str, Vector<String>({ "node" }));

		if (err == OK) {
			Array paths;
			for (int i = 0; i < results.size(); i++) {
				Node *n = get_tree()->get_root()->get_node_or_null(results[i]);
				if (n) {
					Array inputs;
					inputs.push_back(n);
					bool matched = expr->execute(inputs, n);
					if (!expr->has_execute_failed() && matched) {
						paths.push_back(results[i]);
					}
				}
			}
			results = paths;
		} else {
			_log("Failed to parse expression: " + expr_str);
		}
	}

	Dictionary res;
	res["id"] = p_id;
	res["nodes"] = results;
	return res;
}

Dictionary AutoworkE2EServer::_build_tree_dict(Node *p_node, int p_max_depth, int p_current_depth) {
	Dictionary result;
	result["name"] = p_node->get_name();
	result["type"] = p_node->get_class();
	result["path"] = p_node->get_path();
	Array children;

	if (p_current_depth < p_max_depth) {
		for (int i = 0; i < p_node->get_child_count(); i++) {
			children.push_back(_build_tree_dict(p_node->get_child(i), p_max_depth, p_current_depth + 1));
		}
	}
	result["children"] = children;
	return result;
}

Dictionary AutoworkE2EServer::_cmd_get_tree(const Dictionary &p_cmd, const Variant &p_id) {
	String path = p_cmd.get("path", "/root");
	int max_depth = p_cmd.get("depth", 10);
	Node *root_node = get_tree()->get_root()->get_node_or_null(path);

	Dictionary res;
	if (!root_node) {
		res["id"] = p_id;
		res["error"] = "Node not found: " + path;
		return res;
	}

	res["id"] = p_id;
	res["tree"] = _build_tree_dict(root_node, max_depth, 0);
	return res;
}

Dictionary AutoworkE2EServer::_cmd_batch(const Dictionary &p_cmd, const Variant &p_id) {
	Array commands = p_cmd.get("commands", Array());
	Array results;

	for (int i = 0; i < commands.size(); i++) {
		Dictionary sub_cmd = commands[i];
		Dictionary sub_result = _execute_command(sub_cmd);
		if (sub_result.has("_deferred")) {
			Dictionary err_res;
			err_res["id"] = sub_cmd.get("id", Variant());
			err_res["error"] = "Deferred commands not supported in batch";
			results.push_back(err_res);
		} else {
			results.push_back(sub_result);
		}
	}

	Dictionary res;
	res["id"] = p_id;
	res["results"] = results;
	return res;
}

// Input Simulation

Dictionary AutoworkE2EServer::_cmd_input_key(const Dictionary &p_cmd, const Variant &p_id) {
	int keycode = p_cmd.get("keycode", 0);
	bool pressed = p_cmd.get("pressed", true);
	bool physical = p_cmd.get("physical", false);

	Ref<InputEventKey> event;
	event.instantiate();
	if (physical) {
		event->set_physical_keycode((Key)keycode);
	} else {
		event->set_keycode((Key)keycode);
	}
	event->set_pressed(pressed);

	if (keycode == (int)Key::SHIFT) {
		shift_pressed = pressed;
	}
	if (keycode == (int)Key::ALT) {
		alt_pressed = pressed;
	}
	if (keycode == (int)Key::CTRL) {
		ctrl_pressed = pressed;
	}
	if (keycode == (int)Key::META) {
		meta_pressed = pressed;
	}

	event->set_shift_pressed(shift_pressed);
	event->set_alt_pressed(alt_pressed);
	event->set_ctrl_pressed(ctrl_pressed);
	event->set_meta_pressed(meta_pressed);

	if (get_tree() && get_tree()->get_root()) {
		get_tree()->get_root()->push_input(event);
	}

	Dictionary res, resp;
	resp["id"] = p_id;
	resp["ok"] = true;
	res["_deferred"] = true;
	res["wait_type"] = "physics_frames";
	res["count"] = 2;
	res["id"] = p_id;
	res["response"] = resp;
	return res;
}

Dictionary AutoworkE2EServer::_cmd_input_action(const Dictionary &p_cmd, const Variant &p_id) {
	String action_name = p_cmd.get("action_name", "");
	bool pressed = p_cmd.get("pressed", true);
	float strength = p_cmd.get("strength", 1.0f);

	Ref<InputEventAction> event;
	event.instantiate();
	event->set_action(action_name);
	event->set_pressed(pressed);
	event->set_strength(strength);

	if (get_tree() && get_tree()->get_root()) {
		get_tree()->get_root()->push_input(event);
	}

	Dictionary res, resp;
	resp["id"] = p_id;
	resp["ok"] = true;
	res["_deferred"] = true;
	res["wait_type"] = "physics_frames";
	res["count"] = 2;
	res["id"] = p_id;
	res["response"] = resp;
	return res;
}

Dictionary AutoworkE2EServer::_cmd_input_mouse_button(const Dictionary &p_cmd, const Variant &p_id) {
	float x = p_cmd.get("x", 0.0f);
	float y = p_cmd.get("y", 0.0f);
	int button_index = p_cmd.get("button", 1);
	bool pressed = p_cmd.get("pressed", true);

	Ref<InputEventMouseButton> event;
	event.instantiate();
	event->set_position(Vector2(x, y));
	event->set_global_position(Vector2(x, y));
	event->set_button_index((MouseButton)button_index);
	event->set_pressed(pressed);

	if (pressed) {
		mouse_button_mask.set_flag((MouseButtonMask)(1 << (button_index - 1)));
	} else {
		mouse_button_mask.clear_flag((MouseButtonMask)(1 << (button_index - 1)));
	}

	event->set_button_mask(mouse_button_mask);
	event->set_shift_pressed(shift_pressed);
	event->set_alt_pressed(alt_pressed);
	event->set_ctrl_pressed(ctrl_pressed);
	event->set_meta_pressed(meta_pressed);

	if (get_tree() && get_tree()->get_root()) {
		get_tree()->get_root()->push_input(event);
	}

	Dictionary res, resp;
	resp["id"] = p_id;
	resp["ok"] = true;
	res["_deferred"] = true;
	res["wait_type"] = "physics_frames";
	res["count"] = 2;
	res["id"] = p_id;
	res["response"] = resp;
	return res;
}

Dictionary AutoworkE2EServer::_cmd_input_mouse_motion(const Dictionary &p_cmd, const Variant &p_id) {
	float x = p_cmd.get("x", 0.0f);
	float y = p_cmd.get("y", 0.0f);
	float rel_x = p_cmd.get("relative_x", 0.0f);
	float rel_y = p_cmd.get("relative_y", 0.0f);

	Ref<InputEventMouseMotion> event;
	event.instantiate();
	event->set_position(Vector2(x, y));
	event->set_global_position(Vector2(x, y));
	event->set_relative(Vector2(rel_x, rel_y));

	event->set_button_mask(mouse_button_mask);
	event->set_shift_pressed(shift_pressed);
	event->set_alt_pressed(alt_pressed);
	event->set_ctrl_pressed(ctrl_pressed);
	event->set_meta_pressed(meta_pressed);

	if (get_tree() && get_tree()->get_root()) {
		get_tree()->get_root()->push_input(event);
	}

	Dictionary res, resp;
	resp["id"] = p_id;
	resp["ok"] = true;
	res["_deferred"] = true;
	res["wait_type"] = "physics_frames";
	res["count"] = 2;
	res["id"] = p_id;
	res["response"] = resp;
	return res;
}

Dictionary AutoworkE2EServer::_cmd_click_node(const Dictionary &p_cmd, const Variant &p_id) {
	String path = p_cmd.get("path", "");
	Node *node = get_tree()->get_root()->get_node_or_null(path);

	Dictionary res;
	if (!node) {
		res["id"] = p_id;
		res["error"] = "Node not found: " + path;
		return res;
	}

	Vector2 screen_pos = Vector2();
	Control *c = Object::cast_to<Control>(node);
	Node2D *n2d = Object::cast_to<Node2D>(node);

	if (DisplayServer::get_singleton() && DisplayServer::get_singleton()->get_name() == "headless") {
		// Bypassing default matrix transform misses in headless UI layers
		if (c) {
			screen_pos = c->get_size() / 2.0;
			screen_pos += c->get_global_position(); // Fallback map
		}
	}

	if (c) {
		screen_pos = c->get_global_rect().get_center();
	} else if (n2d) {
		Transform2D vt = n2d->get_viewport_transform() * n2d->get_global_transform();
		screen_pos = vt.xform(Vector2());
	} else {
		res["id"] = p_id;
		res["error"] = "Cannot determine screen position for node: " + path;
		return res;
	}

	Ref<InputEventMouseButton> press_event;
	press_event.instantiate();
	press_event->set_position(screen_pos);
	press_event->set_global_position(screen_pos);
	press_event->set_button_index(MouseButton::LEFT);
	press_event->set_pressed(true);
	press_event->set_button_mask(mouse_button_mask);
	press_event->set_shift_pressed(shift_pressed);
	press_event->set_alt_pressed(alt_pressed);
	press_event->set_ctrl_pressed(ctrl_pressed);
	press_event->set_meta_pressed(meta_pressed);

	if (get_tree() && get_tree()->get_root()) {
		get_tree()->get_root()->push_input(press_event);
	}

	Ref<InputEventMouseButton> release_event;
	release_event.instantiate();
	release_event->set_position(screen_pos);
	release_event->set_global_position(screen_pos);
	release_event->set_button_index(MouseButton::LEFT);
	release_event->set_pressed(false);
	if (get_tree() && get_tree()->get_root()) {
		get_tree()->get_root()->push_input(release_event);
	}

	Dictionary resp;
	resp["id"] = p_id;
	resp["ok"] = true;
	res["_deferred"] = true;
	res["wait_type"] = "physics_frames";
	res["count"] = 2;
	res["id"] = p_id;
	res["response"] = resp;
	return res;
}

Dictionary AutoworkE2EServer::_cmd_wait_process_frames(const Dictionary &p_cmd, const Variant &p_id) {
	int count = p_cmd.get("count", 1);
	Dictionary res, resp;
	resp["id"] = p_id;
	resp["ok"] = true;
	res["_deferred"] = true;
	res["wait_type"] = "process_frames";
	res["count"] = count;
	res["id"] = p_id;
	res["response"] = resp;
	return res;
}

Dictionary AutoworkE2EServer::_cmd_wait_physics_frames(const Dictionary &p_cmd, const Variant &p_id) {
	int count = p_cmd.get("count", 1);
	Dictionary res, resp;
	resp["id"] = p_id;
	resp["ok"] = true;
	res["_deferred"] = true;
	res["wait_type"] = "physics_frames";
	res["count"] = count;
	res["id"] = p_id;
	res["response"] = resp;
	return res;
}

Dictionary AutoworkE2EServer::_cmd_wait_seconds(const Dictionary &p_cmd, const Variant &p_id) {
	float duration = p_cmd.get("seconds", 1.0f);
	Dictionary res, resp;
	resp["id"] = p_id;
	resp["ok"] = true;
	res["_deferred"] = true;
	res["wait_type"] = "seconds";
	res["duration"] = duration;
	res["id"] = p_id;
	res["response"] = resp;
	return res;
}

Dictionary AutoworkE2EServer::_cmd_wait_for_node(const Dictionary &p_cmd, const Variant &p_id) {
	String path = p_cmd.get("path", "");
	float timeout = p_cmd.get("timeout", 5.0f);
	Dictionary res, resp;
	resp["id"] = p_id;
	resp["ok"] = true;
	res["_deferred"] = true;
	res["wait_type"] = "node_exists";
	res["path"] = path;
	res["timeout"] = timeout;
	res["id"] = p_id;
	res["response"] = resp;
	return res;
}

Dictionary AutoworkE2EServer::_cmd_wait_for_signal(const Dictionary &p_cmd, const Variant &p_id) {
	String path = p_cmd.get("path", "");
	String signal_name = p_cmd.get("signal_name", "");
	float timeout = p_cmd.get("timeout", 5.0f);
	Dictionary res, resp;
	resp["id"] = p_id;
	resp["ok"] = true;
	res["_deferred"] = true;
	res["wait_type"] = "signal";
	res["path"] = path;
	res["signal_name"] = signal_name;
	res["timeout"] = timeout;
	res["id"] = p_id;
	res["response"] = resp;
	return res;
}

Dictionary AutoworkE2EServer::_cmd_wait_for_property(const Dictionary &p_cmd, const Variant &p_id) {
	String path = p_cmd.get("path", "");
	String property = p_cmd.get("property", "");
	Variant value = p_cmd.get("value", Variant());
	float timeout = p_cmd.get("timeout", 5.0f);
	Dictionary res, resp;
	resp["id"] = p_id;
	resp["ok"] = true;
	res["_deferred"] = true;
	res["wait_type"] = "property";
	res["path"] = path;
	res["property"] = property;
	res["value"] = value;
	res["timeout"] = timeout;
	res["id"] = p_id;
	res["response"] = resp;
	return res;
}

Dictionary AutoworkE2EServer::_cmd_get_scene(const Dictionary &p_cmd, const Variant &p_id) {
	Node *current_scene = get_tree()->get_current_scene();
	Dictionary res;
	if (!current_scene) {
		res["id"] = p_id;
		res["error"] = "No current scene";
		return res;
	}
	res["id"] = p_id;
	res["scene"] = current_scene->get_scene_file_path();
	return res;
}

Dictionary AutoworkE2EServer::_cmd_change_scene(const Dictionary &p_cmd, const Variant &p_id) {
	String scene_path = p_cmd.get("scene_path", "");
	get_tree()->change_scene_to_file(scene_path);

	Dictionary res, resp;
	resp["id"] = p_id;
	resp["ok"] = true;
	res["_deferred"] = true;
	res["wait_type"] = "scene_change";
	res["scene_path"] = scene_path;
	res["id"] = p_id;
	res["response"] = resp;
	return res;
}

Dictionary AutoworkE2EServer::_cmd_reload_scene(const Dictionary &p_cmd, const Variant &p_id) {
	Node *current_scene = get_tree()->get_current_scene();
	Dictionary res;
	if (!current_scene) {
		res["id"] = p_id;
		res["error"] = "No current scene to reload";
		return res;
	}
	String scene_path = current_scene->get_scene_file_path();
	get_tree()->change_scene_to_file(scene_path);

	Dictionary resp;
	resp["id"] = p_id;
	resp["ok"] = true;
	res["_deferred"] = true;
	res["wait_type"] = "scene_change";
	res["scene_path"] = scene_path;
	res["id"] = p_id;
	res["response"] = resp;
	return res;
}

Dictionary AutoworkE2EServer::_cmd_screenshot(const Dictionary &p_cmd, const Variant &p_id) {
	Dictionary res;
	Window *window = get_tree()->get_root();
	if (!window) {
		res["id"] = p_id;
		res["error"] = "Failed to capture screenshot: no root viewport";
		return res;
	}

	Ref<Image> image = window->get_texture()->get_image();
	if (image.is_null() || image->is_empty()) {
		res["id"] = p_id;
		res["error"] = "Failed to capture screenshot: empty rendering buffer";
		return res;
	}

	PackedByteArray png_data = image->save_png_to_buffer();
	String b64_encoded = CryptoCore::b64_encode_str(png_data.ptr(), png_data.size());

	res["id"] = p_id;
	res["ok"] = true;
	res["b64_data"] = b64_encoded;
	return res;
}

Dictionary AutoworkE2EServer::_cmd_set_time_scale(const Dictionary &p_cmd, const Variant &p_id) {
	float scale = p_cmd.get("scale", 1.0f);
	Engine::get_singleton()->set_time_scale(scale);

	Dictionary res;
	res["id"] = p_id;
	res["ok"] = true;
	return res;
}

Dictionary AutoworkE2EServer::_cmd_set_audio_mute(const Dictionary &p_cmd, const Variant &p_id) {
	bool mute = p_cmd.get("mute", true);
	int bus = p_cmd.get("bus", 0);
	AudioServer::get_singleton()->set_bus_mute(bus, mute);

	Dictionary res;
	res["id"] = p_id;
	res["ok"] = true;
	return res;
}

Dictionary AutoworkE2EServer::_cmd_quit(const Dictionary &p_cmd, const Variant &p_id) {
	int exit_code = p_cmd.get("exit_code", 0);
	get_tree()->quit(exit_code);
	Dictionary res;
	res["id"] = p_id;
	res["ok"] = true;
	return res;
}

Array AutoworkE2EServer::_get_property_list_names(Node *p_node) {
	Array names;
	List<PropertyInfo> plist;
	p_node->get_property_list(&plist);
	for (const PropertyInfo &E : plist) {
		names.push_back(E.name);
	}
	return names;
}
