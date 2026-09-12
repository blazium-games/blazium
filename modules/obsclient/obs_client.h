/**************************************************************************/
/*  obs_client.h                                                          */
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
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include "modules/websocket/websocket_peer.h"
#include "obs_enums.h"

class OBSClient : public Object {
	GDCLASS(OBSClient, Object);

	static OBSClient *singleton;

public:
	enum ConnectionState {
		STATE_DISCONNECTED,
		STATE_CONNECTING,
		STATE_IDENTIFYING,
		STATE_CONNECTED,
	};

private:
	// WebSocket connection
	Ref<WebSocketPeer> ws;
	ConnectionState connection_state = STATE_DISCONNECTED;

	// Connection parameters
	String obs_url;
	String obs_password;
	int event_subscriptions = OBS_EVENT_SUBSCRIPTION_ALL;
	int rpc_version = 1;

	// Authentication
	String auth_salt;
	String auth_challenge;

	// Request tracking
	uint64_t next_request_id = 1;
	HashMap<String, Callable> pending_requests;

	// Event callbacks
	HashMap<int, Vector<Callable>> event_callbacks;

	// Session info
	Dictionary server_info;
	int negotiated_rpc_version = 0;

	// Helper methods
	String generate_request_id();
	String generate_auth_string(const String &p_password, const String &p_salt, const String &p_challenge);
	void send_message(int p_opcode, const Dictionary &p_data);
	void process_message(const String &p_message);
	void handle_hello(const Dictionary &p_data);
	void handle_identified(const Dictionary &p_data);
	void handle_event(const Dictionary &p_data);
	void handle_request_response(const Dictionary &p_data);
	void handle_request_batch_response(const Dictionary &p_data);

protected:
	static void _bind_methods();

public:
	static OBSClient *get_singleton();

	// Connection management
	Error connect_to_obs(const String &p_url, const String &p_password = String(), int p_event_subscriptions = OBS_EVENT_SUBSCRIPTION_ALL);
	void disconnect_from_obs();
	void poll();
	ConnectionState get_connection_state() const;
	bool is_obs_connected() const;

	// Session info
	Dictionary get_server_info() const;
	int get_negotiated_rpc_version() const;

	// Event subscription
	void subscribe_to_events(int p_event_mask, const Callable &p_callback);
	void unsubscribe_from_events(int p_event_mask, const Callable &p_callback);
	void reidentify(int p_event_subscriptions);

	// Request sending (generic)
	String send_request(const String &p_request_type, const Dictionary &p_request_data = Dictionary(), const Callable &p_callback = Callable());
	String send_request_batch(const Array &p_requests, bool p_halt_on_failure = false, int p_execution_type = OBS_REQUEST_BATCH_EXECUTION_TYPE_SERIAL_REALTIME, const Callable &p_callback = Callable());

	// General Requests
	void get_version(const Callable &p_callback = Callable());
	void get_stats(const Callable &p_callback = Callable());
	void broadcast_custom_event(const Dictionary &p_event_data);
	void call_vendor_request(const String &p_vendor_name, const String &p_request_type, const Dictionary &p_request_data = Dictionary(), const Callable &p_callback = Callable());
	void get_hotkey_list(const Callable &p_callback = Callable());
	void trigger_hotkey_by_name(const String &p_hotkey_name, const String &p_context_name = String());
	void sleep(int p_sleep_millis = 0, int p_sleep_frames = 0);

	// Config Requests
	void get_persistent_data(const String &p_realm, const String &p_slot_name, const Callable &p_callback = Callable());
	void set_persistent_data(const String &p_realm, const String &p_slot_name, const Variant &p_slot_value);
	void get_scene_collection_list(const Callable &p_callback = Callable());
	void set_current_scene_collection(const String &p_scene_collection_name);
	void create_scene_collection(const String &p_scene_collection_name);
	void get_profile_list(const Callable &p_callback = Callable());
	void set_current_profile(const String &p_profile_name);
	void create_profile(const String &p_profile_name);
	void remove_profile(const String &p_profile_name);
	void get_profile_parameter(const String &p_parameter_category, const String &p_parameter_name, const Callable &p_callback = Callable());
	void set_profile_parameter(const String &p_parameter_category, const String &p_parameter_name, const String &p_parameter_value);
	void get_video_settings(const Callable &p_callback = Callable());
	void set_video_settings(const Dictionary &p_video_settings);
	void get_stream_service_settings(const Callable &p_callback = Callable());
	void set_stream_service_settings(const String &p_stream_service_type, const Dictionary &p_stream_service_settings);
	void get_record_directory(const Callable &p_callback = Callable());
	void set_record_directory(const String &p_record_directory);

	// Scenes Requests
	void get_scene_list(const Callable &p_callback = Callable());
	void get_group_list(const Callable &p_callback = Callable());
	void get_current_program_scene(const Callable &p_callback = Callable());
	void set_current_program_scene(const String &p_scene_name);
	void get_current_preview_scene(const Callable &p_callback = Callable());
	void set_current_preview_scene(const String &p_scene_name);
	void create_scene(const String &p_scene_name, const Callable &p_callback = Callable());
	void remove_scene(const String &p_scene_name);
	void set_scene_name(const String &p_scene_name, const String &p_new_scene_name);
	void get_scene_scene_transition_override(const String &p_scene_name, const Callable &p_callback = Callable());
	void set_scene_scene_transition_override(const String &p_scene_name, const String &p_transition_name, int p_transition_duration = -1);

	// Stream Requests
	void get_stream_status(const Callable &p_callback = Callable());
	void toggle_stream(const Callable &p_callback = Callable());
	void start_stream();
	void stop_stream();
	void send_stream_caption(const String &p_caption_text);

	// Record Requests
	void get_record_status(const Callable &p_callback = Callable());
	void toggle_record(const Callable &p_callback = Callable());
	void start_record();
	void stop_record(const Callable &p_callback = Callable());
	void toggle_record_pause();
	void pause_record();
	void resume_record();
	void split_record_file();
	void create_record_chapter(const String &p_chapter_name = String());

	// Output Requests
	void get_virtual_cam_status(const Callable &p_callback = Callable());
	void toggle_virtual_cam(const Callable &p_callback = Callable());
	void start_virtual_cam();
	void stop_virtual_cam();
	void get_replay_buffer_status(const Callable &p_callback = Callable());
	void toggle_replay_buffer(const Callable &p_callback = Callable());
	void start_replay_buffer();
	void stop_replay_buffer();
	void save_replay_buffer();
	void get_last_replay_buffer_replay(const Callable &p_callback = Callable());

	// Inputs Requests
	void get_input_list(const String &p_input_kind = String(), const Callable &p_callback = Callable());
	void get_input_kind_list(bool p_unversioned = false, const Callable &p_callback = Callable());
	void get_special_inputs(const Callable &p_callback = Callable());
	void create_input(const String &p_scene_name, const String &p_input_name, const String &p_input_kind, const Dictionary &p_input_settings = Dictionary(), bool p_scene_item_enabled = true, const Callable &p_callback = Callable());
	void remove_input(const String &p_input_name);
	void set_input_name(const String &p_input_name, const String &p_new_input_name);
	void get_input_default_settings(const String &p_input_kind, const Callable &p_callback = Callable());
	void get_input_settings(const String &p_input_name, const Callable &p_callback = Callable());
	void set_input_settings(const String &p_input_name, const Dictionary &p_input_settings, bool p_overlay = true);
	void get_input_mute(const String &p_input_name, const Callable &p_callback = Callable());
	void set_input_mute(const String &p_input_name, bool p_input_muted);
	void toggle_input_mute(const String &p_input_name, const Callable &p_callback = Callable());
	void get_input_volume(const String &p_input_name, const Callable &p_callback = Callable());
	void set_input_volume(const String &p_input_name, float p_input_volume_mul = -1.0, float p_input_volume_db = 0.0);

	// Transitions Requests
	void get_transition_kind_list(const Callable &p_callback = Callable());
	void get_scene_transition_list(const Callable &p_callback = Callable());
	void get_current_scene_transition(const Callable &p_callback = Callable());
	void set_current_scene_transition(const String &p_transition_name);
	void set_current_scene_transition_duration(int p_transition_duration);
	void set_current_scene_transition_settings(const Dictionary &p_transition_settings, bool p_overlay = true);
	void trigger_studio_mode_transition();
	void set_tbar_position(float p_position, bool p_release = true);

	// Filters Requests
	void get_source_filter_kind_list(const Callable &p_callback = Callable());
	void get_source_filter_list(const String &p_source_name, const Callable &p_callback = Callable());
	void get_source_filter_default_settings(const String &p_filter_kind, const Callable &p_callback = Callable());
	void create_source_filter(const String &p_source_name, const String &p_filter_name, const String &p_filter_kind, const Dictionary &p_filter_settings = Dictionary());
	void remove_source_filter(const String &p_source_name, const String &p_filter_name);
	void set_source_filter_name(const String &p_source_name, const String &p_filter_name, const String &p_new_filter_name);
	void get_source_filter(const String &p_source_name, const String &p_filter_name, const Callable &p_callback = Callable());
	void set_source_filter_index(const String &p_source_name, const String &p_filter_name, int p_filter_index);
	void set_source_filter_settings(const String &p_source_name, const String &p_filter_name, const Dictionary &p_filter_settings, bool p_overlay = true);
	void set_source_filter_enabled(const String &p_source_name, const String &p_filter_name, bool p_filter_enabled);

	// Scene Items Requests
	void get_scene_item_list(const String &p_scene_name, const Callable &p_callback = Callable());
	void get_group_scene_item_list(const String &p_scene_name, const Callable &p_callback = Callable());
	void get_scene_item_id(const String &p_scene_name, const String &p_source_name, int p_search_offset = 0, const Callable &p_callback = Callable());
	void create_scene_item(const String &p_scene_name, const String &p_source_name, bool p_scene_item_enabled = true, const Callable &p_callback = Callable());
	void remove_scene_item(const String &p_scene_name, int p_scene_item_id);
	void duplicate_scene_item(const String &p_scene_name, int p_scene_item_id, const String &p_destination_scene_name = String(), const Callable &p_callback = Callable());
	void get_scene_item_transform(const String &p_scene_name, int p_scene_item_id, const Callable &p_callback = Callable());
	void set_scene_item_transform(const String &p_scene_name, int p_scene_item_id, const Dictionary &p_scene_item_transform);
	void get_scene_item_enabled(const String &p_scene_name, int p_scene_item_id, const Callable &p_callback = Callable());
	void set_scene_item_enabled(const String &p_scene_name, int p_scene_item_id, bool p_scene_item_enabled);
	void get_scene_item_locked(const String &p_scene_name, int p_scene_item_id, const Callable &p_callback = Callable());
	void set_scene_item_locked(const String &p_scene_name, int p_scene_item_id, bool p_scene_item_locked);
	void get_scene_item_index(const String &p_scene_name, int p_scene_item_id, const Callable &p_callback = Callable());
	void set_scene_item_index(const String &p_scene_name, int p_scene_item_id, int p_scene_item_index);
	void get_scene_item_blend_mode(const String &p_scene_name, int p_scene_item_id, const Callable &p_callback = Callable());
	void set_scene_item_blend_mode(const String &p_scene_name, int p_scene_item_id, const String &p_scene_item_blend_mode);

	// Media Inputs Requests
	void get_media_input_status(const String &p_input_name, const Callable &p_callback = Callable());
	void set_media_input_cursor(const String &p_input_name, int p_media_cursor);
	void offset_media_input_cursor(const String &p_input_name, int p_media_cursor_offset);
	void trigger_media_input_action(const String &p_input_name, const String &p_media_action);

	// Sources Requests
	void get_source_active(const String &p_source_name, const Callable &p_callback = Callable());
	void get_source_screenshot(const String &p_source_name, const String &p_image_format, int p_image_width = -1, int p_image_height = -1, int p_image_compression_quality = -1, const Callable &p_callback = Callable());
	void save_source_screenshot(const String &p_source_name, const String &p_image_format, const String &p_image_file_path, int p_image_width = -1, int p_image_height = -1, int p_image_compression_quality = -1);

	// UI Requests
	void get_studio_mode_enabled(const Callable &p_callback = Callable());
	void set_studio_mode_enabled(bool p_studio_mode_enabled);
	void open_input_properties_dialog(const String &p_input_name);
	void open_input_filters_dialog(const String &p_input_name);
	void open_input_interact_dialog(const String &p_input_name);
	void get_monitor_list(const Callable &p_callback = Callable());
	void open_video_mix_projector(const String &p_video_mix_type, int p_monitor_index = -1, const String &p_projector_geometry = String());
	void open_source_projector(const String &p_source_name, int p_monitor_index = -1, const String &p_projector_geometry = String());

	// Constructor/Destructor
	OBSClient();
	~OBSClient();
};

VARIANT_ENUM_CAST(OBSClient::ConnectionState);
