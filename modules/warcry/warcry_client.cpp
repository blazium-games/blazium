/**************************************************************************/
/*  warcry_client.cpp                                                     */
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

#include "warcry_client.h"

#include "core/config/engine.h"
#include "core/math/math_funcs.h"
#include "core/os/time.h"
#include "modules/enet/enet_packet_peer.h"
#include "scene/audio/audio_stream_player.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"
#include "servers/audio_server.h"

#include <enet/enet.h>

WarcryClient *WarcryClient::singleton = nullptr;

WarcryClient *WarcryClient::get_singleton() {
	return singleton;
}

WarcryClient::WarcryClient() {
	if (!singleton) {
		singleton = this;
	}
	playback_stream.instantiate();
	playback_stream->set_mix_rate((float)WarcryOpusCodec::SAMPLE_RATE);
	playback_stream->set_mix_rate_mode(AudioStreamGenerator::MIX_RATE_CUSTOM);
	playback_stream->set_buffer_length(0.2f);
	codec.init();
}

WarcryClient::~WarcryClient() {
	disconnect_from_server();
	if (singleton == this) {
		singleton = nullptr;
	}
}

void WarcryClient::_ensure_frame_hook() {
	SceneTree *tree = SceneTree::get_singleton();
	if (!tree) {
		return;
	}
	const Callable cb = callable_mp(this, &WarcryClient::poll);
	if (!tree->is_connected(SNAME("process_frame"), cb)) {
		tree->connect(SNAME("process_frame"), cb);
	}
}

bool WarcryClient::connect_to_server(const String &p_host, int p_port, const String &p_username) {
	disconnect_from_server();
	host_name = p_host;
	port = p_port;
	username = p_username;
	hello_sent = false;

	host.instantiate();
	if (host->create_host(1, 2) != OK) {
		host.unref();
		emit_signal(SNAME("error"), String("Failed to create ENet host"));
		return false;
	}
	peer = host->connect_to_host(p_host, p_port, 2);
	if (peer.is_null()) {
		host->destroy();
		host.unref();
		emit_signal(SNAME("error"), String("Failed to start ENet connect"));
		return false;
	}
	_ensure_frame_hook();
	return true;
}

void WarcryClient::_reset_session_state() {
	local_user_id = 0;
	current_channel = 0;
	hello_sent = false;
	if (downlink_channels != 1) {
		downlink_channels = 1;
		codec.init_decoder(1);
	}
	users.clear();
	channels.clear();
}

void WarcryClient::disconnect_from_server() {
	const bool was_connected = is_client_connected() || hello_sent;
	if (peer.is_valid()) {
		peer->peer_disconnect_now();
		peer.unref();
	}
	if (host.is_valid()) {
		host->destroy();
		host.unref();
	}
	_reset_session_state();
	if (was_connected) {
		emit_signal(SNAME("disconnected"));
	}
}

bool WarcryClient::is_client_connected() const {
	return peer.is_valid() && peer->is_active() && peer->get_state() == ENetPacketPeer::STATE_CONNECTED;
}

void WarcryClient::_send_control(WarcryProtocol::MsgType p_type, const Dictionary &p_data) {
	if (!is_client_connected()) {
		return;
	}
	const Vector<uint8_t> bytes = WarcryProtocol::serialize_control(p_type, p_data);
	ENetPacket *packet = enet_packet_create(bytes.ptr(), bytes.size(), ENET_PACKET_FLAG_RELIABLE);
	peer->send(0, packet);
}

void WarcryClient::_send_user_state() {
	Dictionary data;
	data["muted"] = muted;
	data["deaf"] = deaf;
	_send_control(WarcryProtocol::MsgType::USER_STATE, data);
}

void WarcryClient::_send_voice(const Vector<uint8_t> &p_opus) {
	if (!is_client_connected() || muted || p_opus.is_empty()) {
		return;
	}
	WarcryProtocol::VoiceFrameHeader header;
	header.channel_id = (uint32_t)current_channel;
	header.sequence = ++voice_sequence;
	header.timestamp = (uint32_t)Time::get_singleton()->get_ticks_msec();
	const Vector<uint8_t> bytes = WarcryProtocol::serialize_voice(header, p_opus.ptr(), p_opus.size());
	ENetPacket *packet = enet_packet_create(bytes.ptr(), bytes.size(), 0);
	peer->send(1, packet);
}

bool WarcryClient::authenticate(const String &p_username, const String &p_password) {
	if (!is_client_connected()) {
		return false;
	}
	Dictionary data;
	data["username"] = p_username;
	data["password"] = p_password;
	_send_control(WarcryProtocol::MsgType::AUTH, data);
	return true;
}

bool WarcryClient::join_channel(int p_channel_id) {
	Dictionary data;
	data["channelId"] = p_channel_id;
	_send_control(WarcryProtocol::MsgType::JOIN_CHANNEL, data);
	return is_client_connected();
}

bool WarcryClient::leave_channel(int p_channel_id) {
	Dictionary data;
	data["channelId"] = p_channel_id;
	_send_control(WarcryProtocol::MsgType::LEAVE_CHANNEL, data);
	return is_client_connected();
}

void WarcryClient::set_muted(bool p_muted) {
	muted = p_muted;
	_send_user_state();
}

bool WarcryClient::is_muted() const {
	return muted;
}

void WarcryClient::set_deaf(bool p_deaf) {
	deaf = p_deaf;
	_send_user_state();
}

bool WarcryClient::is_deaf() const {
	return deaf;
}

void WarcryClient::set_master_volume(float p_volume) {
	master_volume = CLAMP(p_volume, 0.0f, 2.0f);
}

float WarcryClient::get_master_volume() const {
	return master_volume;
}

bool WarcryClient::set_user_preference(int p_user_id, float p_volume, bool p_muted) {
	Dictionary data;
	data["targetUserId"] = p_user_id;
	data["volume"] = p_volume;
	data["muted"] = p_muted;
	_send_control(WarcryProtocol::MsgType::USER_PREFERENCE, data);
	return is_client_connected();
}

void WarcryClient::start_speaking() {
	speaking = true;
}

void WarcryClient::stop_speaking() {
	speaking = false;
}

bool WarcryClient::is_speaking() const {
	return speaking;
}

void WarcryClient::set_capture_effect(const Ref<AudioEffectCapture> &p_capture) {
	capture = p_capture;
}

void WarcryClient::set_playback(const Ref<AudioStreamGeneratorPlayback> &p_playback) {
	playback = p_playback;
}

Ref<AudioStream> WarcryClient::get_playback_stream() const {
	return playback_stream;
}

bool WarcryClient::setup_audio_io(Node *p_parent) {
	AudioServer *audio = AudioServer::get_singleton();
	if (!audio) {
		return false;
	}

	const StringName cap_bus = SNAME("WarcryCap");
	if (audio->get_bus_index(cap_bus) == -1) {
		audio->add_bus();
		const int idx = audio->get_bus_count() - 1;
		audio->set_bus_name(idx, cap_bus);
		audio->set_bus_mute(idx, true);
		Ref<AudioEffectCapture> cap;
		cap.instantiate();
		cap->set_buffer_length(0.2f);
		audio->add_bus_effect(idx, cap);
		capture = cap;
	} else if (capture.is_null()) {
		const int idx = audio->get_bus_index(cap_bus);
		if (audio->get_bus_effect_count(idx) > 0) {
			capture = audio->get_bus_effect(idx, 0);
		}
	}

	if (!p_parent) {
		return capture.is_valid();
	}

	if (!p_parent->has_node(NodePath("WarcryPlayback"))) {
		AudioStreamPlayer *player = memnew(AudioStreamPlayer);
		player->set_name("WarcryPlayback");
		player->set_stream(playback_stream);
		p_parent->add_child(player);
		player->play();
		playback = player->get_stream_playback();
	} else {
		AudioStreamPlayer *player = Object::cast_to<AudioStreamPlayer>(p_parent->get_node(NodePath("WarcryPlayback")));
		if (player) {
			if (player->get_stream() != playback_stream) {
				player->set_stream(playback_stream);
			}
			if (!player->is_playing()) {
				player->play();
			}
			playback = player->get_stream_playback();
		}
	}

	if (!p_parent->has_node(NodePath("WarcryMic"))) {
		AudioStreamPlayer *mic_player = memnew(AudioStreamPlayer);
		mic_player->set_name("WarcryMic");
		Ref<AudioStreamMicrophone> mic;
		mic.instantiate();
		mic_player->set_stream(mic);
		mic_player->set_bus(cap_bus);
		p_parent->add_child(mic_player);
		mic_player->play();
	}

	return playback.is_valid() || capture.is_valid();
}

TypedArray<Dictionary> WarcryClient::get_channels() const {
	TypedArray<Dictionary> out;
	for (const KeyValue<int, RemoteChannel> &E : channels) {
		Dictionary d;
		d["id"] = E.value.id;
		d["name"] = E.value.name;
		d["member_count"] = E.value.member_count;
		out.push_back(d);
	}
	return out;
}

TypedArray<Dictionary> WarcryClient::get_users() const {
	TypedArray<Dictionary> out;
	for (const KeyValue<int, RemoteUser> &E : users) {
		Dictionary d;
		d["id"] = E.value.id;
		d["username"] = E.value.username;
		d["channel_id"] = E.value.channel_id;
		d["muted"] = E.value.muted;
		d["deaf"] = E.value.deaf;
		out.push_back(d);
	}
	return out;
}

int WarcryClient::get_local_user_id() const {
	return local_user_id;
}

int WarcryClient::get_current_channel() const {
	return current_channel;
}

void WarcryClient::poll() {
	if (host.is_null()) {
		return;
	}

	while (true) {
		ENetConnection::Event event;
		const ENetConnection::EventType type = host->service(0, event);
		if (type == ENetConnection::EVENT_NONE || type == ENetConnection::EVENT_ERROR) {
			break;
		}

		if (event.peer.is_valid()) {
			peer = event.peer;
		}

		if (type == ENetConnection::EVENT_CONNECT) {
			if (!hello_sent) {
				Dictionary hello;
				hello["username"] = username;
				_send_control(WarcryProtocol::MsgType::HELLO, hello);
				hello_sent = true;
			}
		} else if (type == ENetConnection::EVENT_DISCONNECT) {
			peer.unref();
			_reset_session_state();
			emit_signal(SNAME("disconnected"));
		} else if (type == ENetConnection::EVENT_RECEIVE && event.packet) {
			const uint8_t *data = event.packet->data;
			const int size = (int)event.packet->dataLength;
			if (event.channel_id == 1) {
				_handle_voice(data, size);
			} else {
				WarcryProtocol::MsgType msg_type = WarcryProtocol::MsgType::INVALID;
				Dictionary payload;
				if (WarcryProtocol::deserialize_control(data, size, msg_type, payload)) {
					_handle_control(msg_type, payload);
				}
			}
			enet_packet_destroy(event.packet);
		}
	}

	_capture_and_send();
}

void WarcryClient::_handle_control(WarcryProtocol::MsgType p_type, const Dictionary &p_data) {
	switch (p_type) {
		case WarcryProtocol::MsgType::HELLO_ACK: {
			local_user_id = (int)p_data.get("userId", 0);
			if (p_data.has("channelId")) {
				current_channel = (int)p_data.get("channelId", 0);
			}
			const int ack_channels = ((int)p_data.get("downlinkChannels", 1) == 2) ? 2 : 1;
			if (ack_channels != downlink_channels) {
				if (codec.init_decoder(ack_channels)) {
					downlink_channels = ack_channels;
				} else {
					ERR_PRINT("Warcry failed to init " + itos(ack_channels) + "-channel decoder");
				}
			}
			emit_signal(SNAME("connected"));
			break;
		}
		case WarcryProtocol::MsgType::SERVER_STATE: {
			_apply_server_state(p_data);
			emit_signal(SNAME("server_state"));
			break;
		}
		case WarcryProtocol::MsgType::AUTH_ACK: {
			emit_signal(SNAME("authenticated"), p_data.get("role", String()));
			break;
		}
		case WarcryProtocol::MsgType::ERROR_MSG: {
			emit_signal(SNAME("error"), p_data.get("message", String("Warcry error")));
			break;
		}
		default:
			break;
	}
}

WarcryClient::RemoteUser WarcryClient::_user_from_dict(const Dictionary &p_user, int p_fallback_id) {
	RemoteUser user;
	user.id = (int)p_user.get("id", p_fallback_id);
	user.username = p_user.get("username", String());
	user.channel_id = (int)p_user.get("channelId", 0);
	user.muted = p_user.get("muted", false);
	user.deaf = p_user.get("deaf", false);
	return user;
}

WarcryClient::RemoteChannel WarcryClient::_channel_from_dict(const Dictionary &p_channel, int p_fallback_id) {
	RemoteChannel ch;
	ch.id = (int)p_channel.get("id", p_fallback_id);
	ch.name = p_channel.get("name", String());
	if (p_channel.has("members") && p_channel["members"].get_type() == Variant::ARRAY) {
		ch.member_count = ((Array)p_channel["members"]).size();
	} else {
		ch.member_count = (int)p_channel.get("memberCount", 0);
	}
	return ch;
}

void WarcryClient::_apply_server_state(const Dictionary &p_data) {
	if (p_data.has("users")) {
		const Variant users_var = p_data["users"];
		if (users_var.get_type() == Variant::ARRAY) {
			HashMap<int, RemoteUser> incoming;
			const Array users_arr = users_var;
			for (int i = 0; i < users_arr.size(); i++) {
				if (users_arr[i].get_type() != Variant::DICTIONARY) {
					continue;
				}
				const RemoteUser user = _user_from_dict(users_arr[i], 0);
				if (user.id == 0) {
					continue;
				}
				incoming[user.id] = user;
			}
			Vector<int> gone;
			for (const KeyValue<int, RemoteUser> &E : users) {
				if (!incoming.has(E.key)) {
					gone.push_back(E.key);
				}
			}
			for (int id : gone) {
				users.erase(id);
				emit_signal(SNAME("user_left"), id);
			}
			for (const KeyValue<int, RemoteUser> &E : incoming) {
				const bool existed = users.has(E.key);
				users[E.key] = E.value;
				if (!existed) {
					emit_signal(SNAME("user_joined"), E.value.id, E.value.username);
				}
				if (E.value.id == local_user_id) {
					current_channel = E.value.channel_id;
				}
			}
		} else if (users_var.get_type() == Variant::DICTIONARY) {
			const Dictionary users_dict = users_var;
			const Array keys = users_dict.keys();
			for (int i = 0; i < keys.size(); i++) {
				const Variant raw = users_dict[keys[i]];
				const int id = String(keys[i]).to_int();
				if (raw.get_type() != Variant::DICTIONARY) {
					if (users.has(id)) {
						users.erase(id);
						emit_signal(SNAME("user_left"), id);
					}
					continue;
				}
				const RemoteUser user = _user_from_dict(raw, id);
				const bool existed = users.has(user.id);
				users[user.id] = user;
				if (!existed) {
					emit_signal(SNAME("user_joined"), user.id, user.username);
				}
				if (user.id == local_user_id) {
					current_channel = user.channel_id;
				}
			}
		}
	}

	if (p_data.has("channels")) {
		const Variant channels_var = p_data["channels"];
		if (channels_var.get_type() == Variant::ARRAY) {
			HashMap<int, RemoteChannel> incoming;
			const Array channels_arr = channels_var;
			for (int i = 0; i < channels_arr.size(); i++) {
				if (channels_arr[i].get_type() != Variant::DICTIONARY) {
					continue;
				}
				const RemoteChannel ch = _channel_from_dict(channels_arr[i], 0);
				if (ch.id == 0) {
					continue;
				}
				incoming[ch.id] = ch;
			}
			channels = incoming;
		} else if (channels_var.get_type() == Variant::DICTIONARY) {
			const Dictionary ch_dict = channels_var;
			const Array keys = ch_dict.keys();
			for (int i = 0; i < keys.size(); i++) {
				const Variant raw = ch_dict[keys[i]];
				const int id = String(keys[i]).to_int();
				if (raw.get_type() != Variant::DICTIONARY) {
					channels.erase(id);
					continue;
				}
				channels[id] = _channel_from_dict(raw, id);
			}
		}
	}
}

void WarcryClient::_handle_voice(const uint8_t *p_data, int p_size) {
	if (deaf) {
		return;
	}
	WarcryProtocol::VoiceFrameHeader header;
	Vector<uint8_t> opus;
	if (!WarcryProtocol::deserialize_voice(p_data, p_size, header, opus)) {
		return;
	}
	if (current_channel != 0 && header.channel_id != (uint32_t)current_channel) {
		return;
	}
	if (codec.decoder_channels() != downlink_channels) {
		return;
	}
	Vector<int16_t> pcm;
	if (!codec.decode_frame(opus.ptr(), opus.size(), pcm)) {
		return;
	}
	if (playback.is_valid()) {
		PackedVector2Array frames;
		if (downlink_channels == 2 && pcm.size() >= 2) {
			const int frame_count = pcm.size() / 2;
			frames.resize(frame_count);
			Vector2 *out = frames.ptrw();
			for (int i = 0; i < frame_count; i++) {
				const float left = CLAMP((float)pcm[i * 2] / 32768.0f * master_volume, -1.0f, 1.0f);
				const float right = CLAMP((float)pcm[i * 2 + 1] / 32768.0f * master_volume, -1.0f, 1.0f);
				out[i] = Vector2(left, right);
			}
		} else {
			frames.resize(pcm.size());
			Vector2 *out = frames.ptrw();
			for (int i = 0; i < pcm.size(); i++) {
				const float s = CLAMP((float)pcm[i] / 32768.0f * master_volume, -1.0f, 1.0f);
				out[i] = Vector2(s, s);
			}
		}
		if (playback->can_push_buffer(frames.size())) {
			playback->push_buffer(frames);
		}
	}
}

void WarcryClient::_capture_and_send() {
	if (!speaking || muted || capture.is_null() || !is_client_connected()) {
		return;
	}
	const int needed = WarcryOpusCodec::FRAME_SAMPLES;
	if (capture->get_frames_available() < needed) {
		return;
	}
	const PackedVector2Array frames = capture->get_buffer(needed);
	Vector<int16_t> pcm;
	pcm.resize(needed);
	int16_t *dst = pcm.ptrw();
	for (int i = 0; i < needed && i < frames.size(); i++) {
		const float s = CLAMP((float)frames[i].x, -1.0f, 1.0f);
		dst[i] = (int16_t)(s * 32767.0f);
	}
	Vector<uint8_t> opus;
	if (codec.encode_frame(pcm.ptr(), needed, opus)) {
		_send_voice(opus);
	}
}

void WarcryClient::_bind_methods() {
	ClassDB::bind_method(D_METHOD("connect_to_server", "host", "port", "username"), &WarcryClient::connect_to_server);
	ClassDB::bind_method(D_METHOD("disconnect_from_server"), &WarcryClient::disconnect_from_server);
	ClassDB::bind_method(D_METHOD("is_client_connected"), &WarcryClient::is_client_connected);
	ClassDB::bind_method(D_METHOD("authenticate", "username", "password"), &WarcryClient::authenticate);
	ClassDB::bind_method(D_METHOD("join_channel", "channel_id"), &WarcryClient::join_channel);
	ClassDB::bind_method(D_METHOD("leave_channel", "channel_id"), &WarcryClient::leave_channel);
	ClassDB::bind_method(D_METHOD("set_muted", "muted"), &WarcryClient::set_muted);
	ClassDB::bind_method(D_METHOD("is_muted"), &WarcryClient::is_muted);
	ClassDB::bind_method(D_METHOD("set_deaf", "deaf"), &WarcryClient::set_deaf);
	ClassDB::bind_method(D_METHOD("is_deaf"), &WarcryClient::is_deaf);
	ClassDB::bind_method(D_METHOD("set_master_volume", "volume"), &WarcryClient::set_master_volume);
	ClassDB::bind_method(D_METHOD("get_master_volume"), &WarcryClient::get_master_volume);
	ClassDB::bind_method(D_METHOD("set_user_preference", "user_id", "volume", "muted"), &WarcryClient::set_user_preference);
	ClassDB::bind_method(D_METHOD("start_speaking"), &WarcryClient::start_speaking);
	ClassDB::bind_method(D_METHOD("stop_speaking"), &WarcryClient::stop_speaking);
	ClassDB::bind_method(D_METHOD("is_speaking"), &WarcryClient::is_speaking);
	ClassDB::bind_method(D_METHOD("poll"), &WarcryClient::poll);
	ClassDB::bind_method(D_METHOD("get_channels"), &WarcryClient::get_channels);
	ClassDB::bind_method(D_METHOD("get_users"), &WarcryClient::get_users);
	ClassDB::bind_method(D_METHOD("get_local_user_id"), &WarcryClient::get_local_user_id);
	ClassDB::bind_method(D_METHOD("get_current_channel"), &WarcryClient::get_current_channel);
	ClassDB::bind_method(D_METHOD("_apply_server_state", "data"), &WarcryClient::_apply_server_state);
	ClassDB::bind_method(D_METHOD("set_capture_effect", "capture"), &WarcryClient::set_capture_effect);
	ClassDB::bind_method(D_METHOD("set_playback", "playback"), &WarcryClient::set_playback);
	ClassDB::bind_method(D_METHOD("get_playback_stream"), &WarcryClient::get_playback_stream);
	ClassDB::bind_method(D_METHOD("setup_audio_io", "parent"), &WarcryClient::setup_audio_io);

	ADD_SIGNAL(MethodInfo("connected"));
	ADD_SIGNAL(MethodInfo("disconnected"));
	ADD_SIGNAL(MethodInfo("server_state"));
	ADD_SIGNAL(MethodInfo("user_joined", PropertyInfo(Variant::INT, "user_id"), PropertyInfo(Variant::STRING, "username")));
	ADD_SIGNAL(MethodInfo("user_left", PropertyInfo(Variant::INT, "user_id")));
	ADD_SIGNAL(MethodInfo("error", PropertyInfo(Variant::STRING, "message")));
	ADD_SIGNAL(MethodInfo("authenticated", PropertyInfo(Variant::STRING, "role")));
}
