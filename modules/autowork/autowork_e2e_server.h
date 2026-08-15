/**************************************************************************/
/*  autowork_e2e_server.h                                                 */
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

#include "core/io/tcp_server.h"
#include "core/object/object.h"
#include "core/os/os.h"
#include "core/variant/dictionary.h"
#include "modules/websocket/websocket_peer.h"
#include "scene/main/node.h"

class AutoworkE2EServer : public Node {
	GDCLASS(AutoworkE2EServer, Node);

public:
	enum State {
		STATE_LISTENING,
		STATE_IDLE,
		STATE_EXECUTING,
		STATE_WAITING,
		STATE_DISCONNECTED,
	};

	enum WaitType {
		WAIT_NONE,
		WAIT_PROCESS_FRAMES,
		WAIT_PHYSICS_FRAMES,
		WAIT_SECONDS,
		WAIT_NODE_EXISTS,
		WAIT_SIGNAL_EMITTED,
		WAIT_PROPERTY_VALUE,
		WAIT_SCENE_CHANGE,
	};

	struct PeerContext {
		int peer_id = 0;
		Ref<WebSocketPeer> peer;

		State state = STATE_DISCONNECTED;
		bool handshake_sent = false;
		bool authenticated = false;
		PackedByteArray challenge_nonce; // For HMAC

		Variant pending_id;

		WaitType wait_type = WAIT_NONE;
		int wait_process_remaining = 0;
		int wait_physics_remaining = 0;
		float wait_seconds_target = 0.0f;
		float wait_seconds_elapsed = 0.0f;
		String wait_node_path;
		Node *wait_node_cache = nullptr; // pointer cache
		bool wait_signal_emitted = false;
		Callable wait_signal_connection;
		Object *wait_signal_source = nullptr;
		String wait_signal_name;
		String wait_property_node_path;
		String wait_property_name;
		Variant wait_property_value;
		String wait_scene_path;
		uint64_t wait_timeout_ms = 0;
		uint64_t wait_start_ms = 0;
	};

	// Networking
	Ref<TCPServer> server;
	Vector<PeerContext *> peers;

	BitField<MouseButtonMask> mouse_button_mask;
	bool shift_pressed = false;
	bool alt_pressed = false;
	bool ctrl_pressed = false;
	bool meta_pressed = false;

	int physics_frame_counter = 0;

	static const String SERVER_VERSION;
	static const int MAX_PAYLOAD_SIZE = 16777216; // 16 MB

	void _poll_listening();
	void _poll_connection_health(PeerContext *p_ctx);
	void _poll_recv(PeerContext *p_ctx);
	void _poll_wait(PeerContext *p_ctx, float p_delta);
	void _handle_disconnect(PeerContext *p_ctx);
	void _disconnect_peer(PeerContext *p_ctx);
	void _transition_idle(PeerContext *p_ctx);
	void _cleanup_wait(PeerContext *p_ctx);

	void _send_response(PeerContext *p_ctx, const Dictionary &p_data);
	void _dispatch(PeerContext *p_ctx, const Dictionary &p_cmd);
	void _handle_hello(PeerContext *p_ctx, const Dictionary &p_cmd);
	void _enter_wait(PeerContext *p_ctx, const Dictionary &p_params);

	int _listen_random_port(const String &p_host);
	void _write_port_file(const String &p_path, int p_port);
	void _log(const String &p_msg);

	// Command Handlers
	Dictionary _execute_command(const Dictionary &p_cmd);
	Dictionary _cmd_node_exists(const Dictionary &p_cmd, const Variant &p_id);
	Dictionary _cmd_get_property(const Dictionary &p_cmd, const Variant &p_id);
	Dictionary _cmd_set_property(const Dictionary &p_cmd, const Variant &p_id);
	Dictionary _cmd_call_method(const Dictionary &p_cmd, const Variant &p_id);
	Dictionary _cmd_find_by_group(const Dictionary &p_cmd, const Variant &p_id);
	Dictionary _cmd_query_nodes(const Dictionary &p_cmd, const Variant &p_id);
	Dictionary _cmd_get_tree(const Dictionary &p_cmd, const Variant &p_id);
	Dictionary _cmd_batch(const Dictionary &p_cmd, const Variant &p_id);
	Dictionary _cmd_input_key(const Dictionary &p_cmd, const Variant &p_id);
	Dictionary _cmd_input_action(const Dictionary &p_cmd, const Variant &p_id);
	Dictionary _cmd_input_mouse_button(const Dictionary &p_cmd, const Variant &p_id);
	Dictionary _cmd_input_mouse_motion(const Dictionary &p_cmd, const Variant &p_id);
	Dictionary _cmd_click_node(const Dictionary &p_cmd, const Variant &p_id);
	Dictionary _cmd_wait_process_frames(const Dictionary &p_cmd, const Variant &p_id);
	Dictionary _cmd_wait_physics_frames(const Dictionary &p_cmd, const Variant &p_id);
	Dictionary _cmd_wait_seconds(const Dictionary &p_cmd, const Variant &p_id);
	Dictionary _cmd_wait_for_node(const Dictionary &p_cmd, const Variant &p_id);
	Dictionary _cmd_wait_for_signal(const Dictionary &p_cmd, const Variant &p_id);
	Dictionary _cmd_wait_for_property(const Dictionary &p_cmd, const Variant &p_id);
	Dictionary _cmd_get_scene(const Dictionary &p_cmd, const Variant &p_id);

	Dictionary _cmd_capture_screenshot(const Dictionary &p_cmd, const Variant &p_id);
	Dictionary _cmd_set_time_scale(const Dictionary &p_cmd, const Variant &p_id);
	Dictionary _cmd_set_audio_mute(const Dictionary &p_cmd, const Variant &p_id);
	Dictionary _cmd_change_scene(const Dictionary &p_cmd, const Variant &p_id);
	Dictionary _cmd_reload_scene(const Dictionary &p_cmd, const Variant &p_id);
	Dictionary _cmd_screenshot(const Dictionary &p_cmd, const Variant &p_id);
	Dictionary _cmd_quit(const Dictionary &p_cmd, const Variant &p_id);

	// Helpers
	void _walk_tree_match(Node *p_node, const String &p_pattern, Array &r_results);
	Dictionary _build_tree_dict(Node *p_node, int p_max_depth, int p_current_depth);
	Array _get_property_list_names(Node *p_node);
	void _on_wait_signal_emitted(int p_peer_id);

	Variant _deserialize_variant(const Variant &p_val);
	Variant _serialize_variant(const Variant &p_val);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	AutoworkE2EServer();
	~AutoworkE2EServer();
};
