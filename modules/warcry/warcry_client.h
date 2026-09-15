/**************************************************************************/
/*  warcry_client.h                                                       */
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

#include "src/warcry_opus.h"
#include "src/warcry_protocol.h"

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/templates/hash_map.h"
#include "core/variant/typed_array.h"
#include "modules/enet/enet_connection.h"
#ifdef VISIBLE
#undef VISIBLE
#endif
#ifdef IGNORE
#undef IGNORE
#endif
#ifdef ERROR
#undef ERROR
#endif
#include "servers/audio/audio_stream.h"
#include "servers/audio/effects/audio_effect_capture.h"
#include "servers/audio/effects/audio_stream_generator.h"

class Node;

class WarcryClient : public Object {
	GDCLASS(WarcryClient, Object);

public:
	WarcryClient();
	~WarcryClient() override;

	static WarcryClient *get_singleton();

	bool connect_to_server(const String &p_host, int p_port, const String &p_username);
	void disconnect_from_server();
	bool is_client_connected() const;

	bool authenticate(const String &p_username, const String &p_password);
	bool join_channel(int p_channel_id);
	bool leave_channel(int p_channel_id);

	void set_muted(bool p_muted);
	bool is_muted() const;
	void set_deaf(bool p_deaf);
	bool is_deaf() const;
	void set_master_volume(float p_volume);
	float get_master_volume() const;
	bool set_user_preference(int p_user_id, float p_volume, bool p_muted);

	void start_speaking();
	void stop_speaking();
	bool is_speaking() const;

	void poll();

	TypedArray<Dictionary> get_channels() const;
	TypedArray<Dictionary> get_users() const;
	int get_local_user_id() const;
	int get_current_channel() const;

	void set_capture_effect(const Ref<AudioEffectCapture> &p_capture);
	void set_playback(const Ref<AudioStreamGeneratorPlayback> &p_playback);
	Ref<AudioStream> get_playback_stream() const;
	bool setup_audio_io(Node *p_parent);

protected:
	static void _bind_methods();

private:
	struct RemoteUser {
		int id = 0;
		String username;
		int channel_id = 0;
		bool muted = false;
		bool deaf = false;
	};

	struct RemoteChannel {
		int id = 0;
		String name;
		int member_count = 0;
	};

	void _send_control(WarcryProtocol::MsgType p_type, const Dictionary &p_data);
	void _send_user_state();
	void _send_voice(const Vector<uint8_t> &p_opus);
	void _handle_control(WarcryProtocol::MsgType p_type, const Dictionary &p_data);
	void _handle_voice(const uint8_t *p_data, int p_size);
	void _capture_and_send();
	void _apply_server_state(const Dictionary &p_data);
	void _reset_session_state();
	RemoteUser _user_from_dict(const Dictionary &p_user, int p_fallback_id);
	RemoteChannel _channel_from_dict(const Dictionary &p_channel, int p_fallback_id);
	void _ensure_frame_hook();

	static WarcryClient *singleton;

	Ref<ENetConnection> host;
	Ref<ENetPacketPeer> peer;
	WarcryOpusCodec codec;

	String host_name;
	int port = 27015;
	String username;
	int local_user_id = 0;
	int current_channel = 0;
	int downlink_channels = 1;
	bool muted = false;
	bool deaf = false;
	bool speaking = false;
	bool hello_sent = false;
	float master_volume = 1.0f;
	uint32_t voice_sequence = 0;

	HashMap<int, RemoteUser> users;
	HashMap<int, RemoteChannel> channels;

	Ref<AudioEffectCapture> capture;
	Ref<AudioStreamGenerator> playback_stream;
	Ref<AudioStreamGeneratorPlayback> playback;
};
