/**************************************************************************/
/*  webrtc_enet_session.cpp                                               */
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

#include "webrtc_enet_session.h"

#include "enet_webrtc_socket_factory.h"
#include "protocol.h"

#include "core/os/os.h"

void WebRTCEnetSession::_bind_methods() {
	ClassDB::bind_method(D_METHOD("configure", "signal_url", "game_id", "auth_token"), &WebRTCEnetSession::configure);
	ClassDB::bind_method(D_METHOD("create_room", "name", "max_clients", "password", "hidden", "tags"), &WebRTCEnetSession::create_room, DEFVAL(8), DEFVAL(String()), DEFVAL(false), DEFVAL(PackedStringArray()));
	ClassDB::bind_method(D_METHOD("join_room", "room_code", "password", "display_name"), &WebRTCEnetSession::join_room, DEFVAL(String()), DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("list_rooms"), &WebRTCEnetSession::list_rooms);
	ClassDB::bind_method(D_METHOD("leave"), &WebRTCEnetSession::leave);
	ClassDB::bind_method(D_METHOD("seal"), &WebRTCEnetSession::seal);
	ClassDB::bind_method(D_METHOD("kick", "signal_id"), &WebRTCEnetSession::kick);
	ClassDB::bind_method(D_METHOD("get_peer"), &WebRTCEnetSession::get_peer);
	ClassDB::bind_method(D_METHOD("close"), &WebRTCEnetSession::close);
	ClassDB::bind_method(D_METHOD("is_in_room"), &WebRTCEnetSession::is_in_room);
	ClassDB::bind_method(D_METHOD("set_force_relay", "enable"), &WebRTCEnetSession::set_force_relay);
	ClassDB::bind_method(D_METHOD("get_force_relay"), &WebRTCEnetSession::get_force_relay);
	ClassDB::bind_method(D_METHOD("set_ice_timeout_msec", "msec"), &WebRTCEnetSession::set_ice_timeout_msec);
	ClassDB::bind_method(D_METHOD("get_ice_timeout_msec"), &WebRTCEnetSession::get_ice_timeout_msec);
	ClassDB::bind_method(D_METHOD("get_room_code"), &WebRTCEnetSession::get_room_code);
	ClassDB::bind_method(D_METHOD("get_local_fake_ip"), &WebRTCEnetSession::get_local_fake_ip);
	ClassDB::bind_method(D_METHOD("get_local_fake_port"), &WebRTCEnetSession::get_local_fake_port);
	ClassDB::bind_method(D_METHOD("get_local_signal_id"), &WebRTCEnetSession::get_local_signal_id);

	ADD_SIGNAL(MethodInfo("session_ready", PropertyInfo(Variant::STRING, "local_fake_ip"), PropertyInfo(Variant::INT, "local_fake_port"), PropertyInfo(Variant::STRING, "room_code")));
	ADD_SIGNAL(MethodInfo("peer_link_up", PropertyInfo(Variant::STRING, "fake_ip"), PropertyInfo(Variant::INT, "fake_port"), PropertyInfo(Variant::INT, "signal_id")));
	ADD_SIGNAL(MethodInfo("peer_link_down", PropertyInfo(Variant::STRING, "fake_ip"), PropertyInfo(Variant::INT, "fake_port"), PropertyInfo(Variant::INT, "signal_id")));
	ADD_SIGNAL(MethodInfo("using_turn", PropertyInfo(Variant::INT, "signal_id")));
	ADD_SIGNAL(MethodInfo("using_relay", PropertyInfo(Variant::INT, "signal_id")));
	ADD_SIGNAL(MethodInfo("failed", PropertyInfo(Variant::STRING, "reason")));
	ADD_SIGNAL(MethodInfo("rooms_listed", PropertyInfo(Variant::ARRAY, "rooms")));
}

WebRTCEnetSession::WebRTCEnetSession() {
	signal_client.instantiate();
	signal_client->connect("connected", callable_mp(this, &WebRTCEnetSession::_on_signal_connected));
	signal_client->connect("disconnected", callable_mp(this, &WebRTCEnetSession::_on_signal_disconnected));
	signal_client->connect("message_received", callable_mp(this, &WebRTCEnetSession::_on_signal_message));
	relay.set_signal_client(signal_client.ptr());
	set_process(false);
}

WebRTCEnetSession::~WebRTCEnetSession() {
	close();
}

void WebRTCEnetSession::_notification(int p_what) {
	if (p_what == NOTIFICATION_PROCESS) {
		_poll();
	}
}

void WebRTCEnetSession::_ensure_process() {
	set_process(true);
}

Error WebRTCEnetSession::configure(const String &p_signal_url, const String &p_game_id, const String &p_auth_token) {
	const Error err = signal_client->configure(p_signal_url, p_game_id, p_auth_token);
	if (err != OK) {
		signal_url = String();
		game_id = String();
		auth_token = String();
		return err;
	}
	signal_url = p_signal_url;
	game_id = p_game_id;
	auth_token = p_auth_token;
	return OK;
}

void WebRTCEnetSession::set_force_relay(bool p_enable) {
	force_relay = p_enable;
}

void WebRTCEnetSession::set_ice_timeout_msec(int p_msec) {
	ice_timeout_msec = CLAMP(p_msec, 0, 60000);
}

void WebRTCEnetSession::_fail(const String &p_reason) {
	pending = OP_NONE;
	emit_signal("failed", p_reason);
}

void WebRTCEnetSession::_poll() {
	if (signal_client.is_valid()) {
		signal_client->poll();
		if (signal_client->is_open()) {
			const uint64_t now = OS::get_singleton()->get_ticks_msec();
			if (now - last_ping_msec >= 15000) {
				last_ping_msec = now;
				Dictionary ping;
				ping["t"] = "ping";
				signal_client->send_message(GamesEnetWebrtcProtocol::MSG_PING, ping);
			}
		}
	}
	for (KeyValue<int, Ref<WebRTCLink>> &E : links) {
		if (E.value.is_valid()) {
			E.value->poll();
			if (E.value->is_open() && !notified_open.has(E.key)) {
				_on_channel_open(E.key);
			}
		}
	}
	if (peer.is_valid()) {
		if (peer->get_connection_status() != MultiplayerPeer::CONNECTION_DISCONNECTED) {
			peer->poll();
		}
		if (peer.is_valid() && peer->get_connection_status() == MultiplayerPeer::CONNECTION_DISCONNECTED && !is_host && session_emitted) {
			FakePeerEntry host_entry;
			if (map.lookup_signal(1, host_entry)) {
				emit_signal("peer_link_down", String(host_entry.fake_ip), host_entry.fake_port, 1);
			}
			close();
			return;
		}
	}
	if (ice_deadline_msec > 0 && OS::get_singleton()->get_ticks_msec() >= ice_deadline_msec) {
		ice_deadline_msec = 0;
		for (KeyValue<int, Ref<WebRTCLink>> &E : links) {
			if (E.value.is_valid() && E.value->get_mode() != WebRTCLink::MODE_RELAY && !E.value->is_open()) {
				E.value->set_mode(WebRTCLink::MODE_RELAY);
				map.set_relay(E.key, true);
				emit_signal("using_relay", E.key);
			}
		}
		_maybe_create_enet();
		_maybe_emit_ready();
	}
}

bool WebRTCEnetSession::is_in_room() const {
	return session_emitted || peer.is_valid();
}

Signal WebRTCEnetSession::create_room(const String &p_name, int p_max_clients, const String &p_password, bool p_hidden, const PackedStringArray &p_tags) {
	ERR_FAIL_COND_V_MSG(pending != OP_NONE, Signal(this, "failed"), "A session operation is already in progress.");
	if (is_in_room()) {
		_fail("already in a room");
		return Signal(this, "failed");
	}
	const String name = GamesEnetWebrtcProtocol::strip_controls(p_name);
	if (!GamesEnetWebrtcProtocol::is_valid_room_name(name) || !GamesEnetWebrtcProtocol::is_valid_password(p_password)) {
		_fail("invalid room fields");
		return Signal(this, "failed");
	}
	pending = OP_CREATE;
	session_emitted = false;
	is_host = true;
	host_transport_ready = true;
	_ensure_process();

	Dictionary payload;
	payload["name"] = name;
	payload["max"] = GamesEnetWebrtcProtocol::clamp_max_clients(p_max_clients);
	if (!p_password.is_empty()) {
		payload["password"] = p_password;
	}
	payload["hidden"] = p_hidden;
	PackedStringArray tags;
	for (int i = 0; i < p_tags.size() && tags.size() < GamesEnetWebrtcProtocol::MAX_TAGS; i++) {
		const String t = p_tags[i];
		if (GamesEnetWebrtcProtocol::is_valid_game_id(t) && t.length() <= GamesEnetWebrtcProtocol::MAX_TAG_LEN) {
			tags.push_back(t);
		}
	}
	payload["tags"] = tags;
	payload["force_relay"] = force_relay;

	if (signal_client->is_open()) {
		signal_client->send_message(GamesEnetWebrtcProtocol::MSG_CREATE, payload);
	} else {
		set_meta("_pending_create", payload);
		signal_client->connect_to_signal();
	}
	return Signal(this, "session_ready");
}

Signal WebRTCEnetSession::join_room(const String &p_room_code, const String &p_password, const String &p_display_name) {
	ERR_FAIL_COND_V_MSG(pending != OP_NONE, Signal(this, "failed"), "A session operation is already in progress.");
	if (is_in_room()) {
		_fail("already in a room");
		return Signal(this, "failed");
	}
	if (!GamesEnetWebrtcProtocol::is_valid_room_code(p_room_code) || !GamesEnetWebrtcProtocol::is_valid_password(p_password)) {
		_fail("invalid join fields");
		return Signal(this, "failed");
	}
	pending = OP_JOIN;
	session_emitted = false;
	is_host = false;
	host_transport_ready = false;
	room_code = p_room_code;
	_ensure_process();

	Dictionary payload;
	payload["room_code"] = p_room_code;
	if (!p_password.is_empty()) {
		payload["password"] = p_password;
	}
	payload["force_relay"] = force_relay;
	const String display = GamesEnetWebrtcProtocol::strip_controls(p_display_name);
	if (GamesEnetWebrtcProtocol::is_valid_room_name(display)) {
		payload["name"] = display;
	}

	if (signal_client->is_open()) {
		signal_client->send_message(GamesEnetWebrtcProtocol::MSG_JOIN, payload);
	} else {
		set_meta("_pending_join", payload);
		signal_client->connect_to_signal();
	}
	return Signal(this, "session_ready");
}

Signal WebRTCEnetSession::list_rooms() {
	_ensure_process();
	if (signal_client->is_open()) {
		signal_client->send_message(GamesEnetWebrtcProtocol::MSG_LIST, Dictionary());
	} else {
		set_meta("_pending_list", true);
		signal_client->connect_to_signal();
	}
	return Signal(this, "rooms_listed");
}

Error WebRTCEnetSession::leave() {
	const bool notify = signal_client.is_valid() && signal_client->is_open() && is_in_room();
	if (notify) {
		signal_client->send_message(GamesEnetWebrtcProtocol::MSG_LEAVE, Dictionary());
	}
	session_emitted = false;
	pending = OP_NONE;
	close();
	return notify ? OK : ERR_UNCONFIGURED;
}

Error WebRTCEnetSession::seal() {
	ERR_FAIL_COND_V(!is_host || !signal_client.is_valid() || !signal_client->is_open(), ERR_UNCONFIGURED);
	return signal_client->send_message(GamesEnetWebrtcProtocol::MSG_SEAL, Dictionary());
}

Error WebRTCEnetSession::kick(int p_signal_id) {
	ERR_FAIL_COND_V(!is_host || !signal_client.is_valid() || !signal_client->is_open(), ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(!GamesEnetWebrtcProtocol::is_valid_signal_id(p_signal_id) || p_signal_id == local_signal_id, ERR_INVALID_PARAMETER);
	Dictionary payload;
	payload["signal_id"] = p_signal_id;
	return signal_client->send_message(GamesEnetWebrtcProtocol::MSG_KICK, payload, p_signal_id);
}

Ref<ENetMultiplayerPeer> WebRTCEnetSession::get_peer() const {
	return peer;
}

void WebRTCEnetSession::_on_signal_connected() {
	if (has_meta("_pending_create")) {
		Dictionary payload = get_meta("_pending_create");
		remove_meta("_pending_create");
		signal_client->send_message(GamesEnetWebrtcProtocol::MSG_CREATE, payload);
	}
	if (has_meta("_pending_join")) {
		Dictionary payload = get_meta("_pending_join");
		remove_meta("_pending_join");
		signal_client->send_message(GamesEnetWebrtcProtocol::MSG_JOIN, payload);
	}
	if (has_meta("_pending_list")) {
		remove_meta("_pending_list");
		signal_client->send_message(GamesEnetWebrtcProtocol::MSG_LIST, Dictionary());
	}
}

void WebRTCEnetSession::_on_signal_disconnected() {
	if (pending != OP_NONE) {
		_fail("signal disconnected");
	}
	if (is_in_room()) {
		FakePeerEntry host_entry;
		if (!is_host && map.lookup_signal(1, host_entry)) {
			emit_signal("peer_link_down", String(host_entry.fake_ip), host_entry.fake_port, 1);
		}
		close();
	}
}

void WebRTCEnetSession::_on_signal_message(int p_type, int p_id, Dictionary p_data) {
	using namespace GamesEnetWebrtcProtocol;
	if (!payload_within_limits(p_type, p_data)) {
		return;
	}
	switch (p_type) {
		case MSG_HELLO:
			_handle_hello(p_data);
			break;
		case MSG_PEER_INFO:
			_handle_peer_info(p_data);
			break;
		case MSG_PEER_CONNECT:
			_handle_peer_info(p_data);
			break;
		case MSG_PEER_DISCONNECT: {
			const int sid = int(p_data.get("signal_id", p_id));
			FakePeerEntry entry;
			if (map.lookup_signal(sid, entry)) {
				emit_signal("peer_link_down", String(entry.fake_ip), entry.fake_port, sid);
			}
			if (links.has(sid)) {
				map.set_link(sid, nullptr);
				links[sid]->close();
				links.erase(sid);
			}
			map.remove_signal(sid);
		} break;
		case MSG_OFFER:
		case MSG_ANSWER: {
			const int sid = int(p_data.get("signal_id", p_id));
			if (!is_valid_signal_id(sid) || !links.has(sid)) {
				break;
			}
			links[sid]->set_remote_description(String(p_data.get("sdp_type", p_type == MSG_OFFER ? "offer" : "answer")), String(p_data.get("sdp", "")));
		} break;
		case MSG_CANDIDATE: {
			const int sid = int(p_data.get("signal_id", p_id));
			if (!is_valid_signal_id(sid) || !links.has(sid)) {
				break;
			}
			links[sid]->add_ice_candidate(String(p_data.get("mid", p_data.get("media", "0"))), int(p_data.get("index", 0)), String(p_data.get("candidate", "")));
		} break;
		case MSG_USE_RELAY: {
			const int sid = int(p_data.get("signal_id", p_id));
			FakePeerEntry relay_entry;
			if (!is_valid_signal_id(sid) || !map.lookup_signal(sid, relay_entry)) {
				break;
			}
			if (links.has(sid)) {
				links[sid]->set_mode(WebRTCLink::MODE_RELAY);
			}
			map.set_relay(sid, true);
			emit_signal("using_relay", sid);
			if (sid == 1) {
				host_transport_ready = true;
			}
			_maybe_create_enet();
			_maybe_emit_ready();
		} break;
		case MSG_RELAY_DATAGRAM: {
			int from = 0;
			Vector<uint8_t> payload;
			if (!RelayClient::decode_datagram(p_data, from, payload)) {
				break;
			}
			if (from == 0) {
				from = p_id;
			}
			FakePeerEntry entry;
			if (map.lookup_signal(from, entry)) {
				map.push_inbound(payload, entry.fake_ip, entry.fake_port);
			}
		} break;
		case MSG_LIST: {
			cached_rooms = GamesEnetWebrtcProtocol::sanitize_room_list(p_data.get("rooms", Array()));
			emit_signal("rooms_listed", cached_rooms);
		} break;
		case MSG_ICE_CONFIG: {
			ice_servers = GamesEnetWebrtcProtocol::sanitize_ice_servers(p_data.get("ice_servers", Array()));
		} break;
		case MSG_ERROR: {
			String msg = GamesEnetWebrtcProtocol::strip_controls(String(p_data.get("message", "signal error")));
			if (msg.length() > 256) {
				msg = msg.substr(0, 256);
			}
			if (msg.is_empty()) {
				msg = "signal error";
			}
			_fail(msg);
		} break;
		case MSG_PING:

			break;
		default:
			break;
	}
}

void WebRTCEnetSession::_handle_hello(const Dictionary &p_data) {
	local_signal_id = int(p_data.get("signal_id", 0));
	local_fake_ip = String(p_data.get("fake_ip", ""));
	local_fake_port = int(p_data.get("fake_port", 1));
	room_code = String(p_data.get("room_code", room_code));
	if (!GamesEnetWebrtcProtocol::is_valid_signal_id(local_signal_id) || !GamesEnetWebrtcProtocol::is_valid_fake_ip(local_fake_ip) || !GamesEnetWebrtcProtocol::is_valid_fake_port(local_fake_port) || !GamesEnetWebrtcProtocol::is_valid_room_code(room_code)) {
		_fail("invalid HELLO");
		return;
	}
	ice_servers = GamesEnetWebrtcProtocol::sanitize_ice_servers(p_data.get("ice_servers", Array()));

	FakePeerEntry self;
	self.signal_id = local_signal_id;
	self.fake_ip = IPAddress(local_fake_ip);
	self.fake_port = (uint16_t)local_fake_port;
	self.role = is_host ? "host" : "joiner";
	map.add_peer(self);

	if (force_relay) {
		ice_deadline_msec = 0;
	} else {
		ice_deadline_msec = OS::get_singleton()->get_ticks_msec() + ice_timeout_msec;
	}

	if (is_host) {
		_maybe_create_enet();
		_maybe_emit_ready();
	}
}

void WebRTCEnetSession::_handle_peer_info(const Dictionary &p_data) {
	const int sid = int(p_data.get("signal_id", 0));
	if (sid == 0 || sid == local_signal_id) {
		return;
	}
	const String fip = String(p_data.get("fake_ip", ""));
	const int fport = int(p_data.get("fake_port", 1));
	if (!GamesEnetWebrtcProtocol::is_valid_signal_id(sid) || !GamesEnetWebrtcProtocol::is_valid_fake_ip(fip) || !GamesEnetWebrtcProtocol::is_valid_fake_port(fport)) {
		return;
	}
	FakePeerEntry entry;
	entry.signal_id = sid;
	entry.fake_ip = IPAddress(fip);
	entry.fake_port = (uint16_t)fport;
	const String role = String(p_data.get("role", ""));
	entry.role = GamesEnetWebrtcProtocol::is_valid_role(role) ? role : String();
	entry.name = GamesEnetWebrtcProtocol::strip_controls(String(p_data.get("name", "")));
	map.add_peer(entry);

	if (!GamesEnetWebrtcProtocol::is_star_pair(local_signal_id, sid)) {
		return;
	}

	const bool we_offer = local_signal_id < sid;
	if (force_relay) {
		entry.relay = true;
		map.set_relay(sid, true);
		Ref<WebRTCLink> link;
		link.instantiate();
		link->set_mode(WebRTCLink::MODE_RELAY);
		links[sid] = link;
		if (sid == 1) {
			host_transport_ready = true;
		}
		notified_open.insert(sid);
		emit_signal("using_relay", sid);
		emit_signal("peer_link_up", String(entry.fake_ip), entry.fake_port, sid);
		_maybe_create_enet();
		_maybe_emit_ready();
		return;
	}

	_ensure_link(sid, entry.fake_ip, entry.fake_port, we_offer);
}

Ref<WebRTCLink> WebRTCEnetSession::_ensure_link(int p_signal_id, const IPAddress &p_ip, uint16_t p_port, bool p_offerer) {
	if (links.has(p_signal_id)) {
		return links[p_signal_id];
	}
	Ref<WebRTCLink> link;
	link.instantiate();
	link->connect("local_description", callable_mp(this, &WebRTCEnetSession::_on_local_description).bind(p_signal_id));
	link->connect("local_candidate", callable_mp(this, &WebRTCEnetSession::_on_local_candidate).bind(p_signal_id));
	link->connect("using_turn", callable_mp(this, &WebRTCEnetSession::_on_using_turn).bind(p_signal_id));
	link->setup(p_signal_id, p_ip, p_port, &map, p_offerer, ice_servers);
	links[p_signal_id] = link;
	map.set_link(p_signal_id, link.ptr());
	return link;
}

void WebRTCEnetSession::_on_local_description(const String &p_type, const String &p_sdp, int p_signal_id) {
	Dictionary payload;
	payload["sdp"] = p_sdp;
	payload["sdp_type"] = p_type;
	const int msg = p_type == "offer" ? GamesEnetWebrtcProtocol::MSG_OFFER : GamesEnetWebrtcProtocol::MSG_ANSWER;
	signal_client->send_message(msg, payload, p_signal_id);
}

void WebRTCEnetSession::_on_local_candidate(const String &p_media, int p_index, const String &p_name, int p_signal_id) {
	Dictionary payload;
	payload["mid"] = p_media;
	payload["index"] = p_index;
	payload["candidate"] = p_name;
	signal_client->send_message(GamesEnetWebrtcProtocol::MSG_CANDIDATE, payload, p_signal_id);
}

void WebRTCEnetSession::_on_channel_open(int p_signal_id) {
	if (notified_open.has(p_signal_id)) {
		return;
	}
	notified_open.insert(p_signal_id);
	FakePeerEntry entry;
	if (!map.lookup_signal(p_signal_id, entry)) {
		return;
	}
	if (p_signal_id == 1) {
		host_transport_ready = true;
	}
	signal_client->send_message(GamesEnetWebrtcProtocol::MSG_PEER_READY, Dictionary(), p_signal_id);
	emit_signal("peer_link_up", String(entry.fake_ip), entry.fake_port, p_signal_id);
	_maybe_create_enet();
	_maybe_emit_ready();
}

void WebRTCEnetSession::_on_using_turn(int p_signal_id) {
	emit_signal("using_turn", p_signal_id);
}

void WebRTCEnetSession::_maybe_create_enet() {
	if (peer.is_valid()) {
		return;
	}
	if (local_fake_ip.is_empty()) {
		return;
	}
	if (!is_host && !host_transport_ready) {
		return;
	}

	peer.instantiate();
	peer->set_bind_ip(IPAddress(local_fake_ip));
	ENetWebRTCSocketFactory::Guard guard(&map, this);
	Error err;
	if (is_host) {
		err = peer->create_server(local_fake_port, GamesEnetWebrtcProtocol::ADVERTISED_ROOM_CAP);
	} else {
		err = peer->create_client("10.66.0.1", 1, 0, 0, 0, local_fake_port);
	}
	if (err != OK) {
		peer.unref();
		_fail("failed to create ENet peer");
	}
}

void WebRTCEnetSession::_maybe_emit_ready() {
	if (session_emitted || peer.is_null()) {
		return;
	}
	if (!is_host && !host_transport_ready) {
		return;
	}
	session_emitted = true;
	pending = OP_NONE;
	emit_signal("session_ready", local_fake_ip, local_fake_port, room_code);
}

Error WebRTCEnetSession::send_datagram(const IPAddress &p_ip, uint16_t p_port, const uint8_t *p_buffer, int p_len) {
	ERR_FAIL_COND_V(p_buffer == nullptr || p_len <= 0 || p_len > GamesEnetWebrtcProtocol::MAX_RELAY_BYTES, ERR_INVALID_PARAMETER);
	FakePeerEntry entry;
	if (!map.lookup_addr(p_ip, p_port, entry)) {
		return ERR_BUSY;
	}
	if (!GamesEnetWebrtcProtocol::is_star_pair(local_signal_id, entry.signal_id)) {
		return ERR_BUSY;
	}
	if (entry.link && entry.link->is_open() && entry.link->get_mode() != WebRTCLink::MODE_RELAY) {
		return entry.link->send(p_buffer, p_len);
	}
	if (entry.relay || force_relay || (entry.link && entry.link->get_mode() == WebRTCLink::MODE_RELAY)) {
		return relay.send_datagram(entry.signal_id, p_buffer, p_len);
	}
	if (entry.link) {
		const Error e = entry.link->send(p_buffer, p_len);
		if (e == OK) {
			return OK;
		}
	}
	return relay.send_datagram(entry.signal_id, p_buffer, p_len);
}

void WebRTCEnetSession::close() {
	if (signal_client.is_valid() && signal_client->is_open() && (session_emitted || pending != OP_NONE)) {
		signal_client->send_message(GamesEnetWebrtcProtocol::MSG_LEAVE, Dictionary());
	}
	pending = OP_NONE;
	ice_deadline_msec = 0;
	session_emitted = false;
	host_transport_ready = false;
	is_host = false;
	local_fake_ip = String();
	local_fake_port = 1;
	local_signal_id = 0;
	room_code = String();
	last_ping_msec = 0;
	for (KeyValue<int, Ref<WebRTCLink>> &E : links) {
		map.set_link(E.key, nullptr);
		if (E.value.is_valid()) {
			E.value->close();
		}
	}
	links.clear();
	notified_open.clear();
	map.clear();
	if (peer.is_valid()) {
		peer->close();
		peer.unref();
	}
	if (signal_client.is_valid()) {
		signal_client->close();
	}
	set_process(false);
}
