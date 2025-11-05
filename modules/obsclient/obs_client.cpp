/**************************************************************************/
/*  obs_client.cpp                                                        */
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

#include "obs_client.h"

#include "core/crypto/crypto_core.h"
#include "core/io/json.h"
#include "core/string/ustring.h"
#include "core/variant/variant.h"

OBSClient *OBSClient::singleton = nullptr;

OBSClient *OBSClient::get_singleton() {
	return singleton;
}

void OBSClient::_bind_methods() {
	// Connection management
	ClassDB::bind_method(D_METHOD("connect_to_obs", "url", "password", "event_subscriptions"), &OBSClient::connect_to_obs, DEFVAL(String()), DEFVAL(OBS_EVENT_SUBSCRIPTION_ALL));
	ClassDB::bind_method(D_METHOD("disconnect_from_obs"), &OBSClient::disconnect_from_obs);
	ClassDB::bind_method(D_METHOD("poll"), &OBSClient::poll);
	ClassDB::bind_method(D_METHOD("get_connection_state"), &OBSClient::get_connection_state);
	ClassDB::bind_method(D_METHOD("is_obs_connected"), &OBSClient::is_obs_connected);

	// Session info
	ClassDB::bind_method(D_METHOD("get_server_info"), &OBSClient::get_server_info);
	ClassDB::bind_method(D_METHOD("get_negotiated_rpc_version"), &OBSClient::get_negotiated_rpc_version);

	// Event subscription
	ClassDB::bind_method(D_METHOD("subscribe_to_events", "event_mask", "callback"), &OBSClient::subscribe_to_events);
	ClassDB::bind_method(D_METHOD("unsubscribe_from_events", "event_mask", "callback"), &OBSClient::unsubscribe_from_events);
	ClassDB::bind_method(D_METHOD("reidentify", "event_subscriptions"), &OBSClient::reidentify);

	// Request sending
	ClassDB::bind_method(D_METHOD("send_request", "request_type", "request_data", "callback"), &OBSClient::send_request, DEFVAL(Dictionary()), DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("send_request_batch", "requests", "halt_on_failure", "execution_type", "callback"), &OBSClient::send_request_batch, DEFVAL(false), DEFVAL(OBS_REQUEST_BATCH_EXECUTION_TYPE_SERIAL_REALTIME), DEFVAL(Callable()));

	// General Requests
	ClassDB::bind_method(D_METHOD("get_version", "callback"), &OBSClient::get_version, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("get_stats", "callback"), &OBSClient::get_stats, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("broadcast_custom_event", "event_data"), &OBSClient::broadcast_custom_event);
	ClassDB::bind_method(D_METHOD("get_hotkey_list", "callback"), &OBSClient::get_hotkey_list, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("trigger_hotkey_by_name", "hotkey_name", "context_name"), &OBSClient::trigger_hotkey_by_name, DEFVAL(String()));

	// Config Requests
	ClassDB::bind_method(D_METHOD("get_persistent_data", "realm", "slot_name", "callback"), &OBSClient::get_persistent_data, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("set_persistent_data", "realm", "slot_name", "slot_value"), &OBSClient::set_persistent_data);
	ClassDB::bind_method(D_METHOD("get_scene_collection_list", "callback"), &OBSClient::get_scene_collection_list, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("set_current_scene_collection", "scene_collection_name"), &OBSClient::set_current_scene_collection);
	ClassDB::bind_method(D_METHOD("create_scene_collection", "scene_collection_name"), &OBSClient::create_scene_collection);
	ClassDB::bind_method(D_METHOD("get_profile_list", "callback"), &OBSClient::get_profile_list, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("set_current_profile", "profile_name"), &OBSClient::set_current_profile);
	ClassDB::bind_method(D_METHOD("create_profile", "profile_name"), &OBSClient::create_profile);
	ClassDB::bind_method(D_METHOD("remove_profile", "profile_name"), &OBSClient::remove_profile);
	ClassDB::bind_method(D_METHOD("get_profile_parameter", "parameter_category", "parameter_name", "callback"), &OBSClient::get_profile_parameter, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("set_profile_parameter", "parameter_category", "parameter_name", "parameter_value"), &OBSClient::set_profile_parameter);
	ClassDB::bind_method(D_METHOD("get_video_settings", "callback"), &OBSClient::get_video_settings, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("set_video_settings", "video_settings"), &OBSClient::set_video_settings);
	ClassDB::bind_method(D_METHOD("get_stream_service_settings", "callback"), &OBSClient::get_stream_service_settings, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("set_stream_service_settings", "stream_service_type", "stream_service_settings"), &OBSClient::set_stream_service_settings);
	ClassDB::bind_method(D_METHOD("get_record_directory", "callback"), &OBSClient::get_record_directory, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("set_record_directory", "record_directory"), &OBSClient::set_record_directory);

	// Scenes Requests
	ClassDB::bind_method(D_METHOD("get_scene_list", "callback"), &OBSClient::get_scene_list, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("get_group_list", "callback"), &OBSClient::get_group_list, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("get_current_program_scene", "callback"), &OBSClient::get_current_program_scene, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("set_current_program_scene", "scene_name"), &OBSClient::set_current_program_scene);
	ClassDB::bind_method(D_METHOD("get_current_preview_scene", "callback"), &OBSClient::get_current_preview_scene, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("set_current_preview_scene", "scene_name"), &OBSClient::set_current_preview_scene);
	ClassDB::bind_method(D_METHOD("create_scene", "scene_name", "callback"), &OBSClient::create_scene, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("remove_scene", "scene_name"), &OBSClient::remove_scene);
	ClassDB::bind_method(D_METHOD("set_scene_name", "scene_name", "new_scene_name"), &OBSClient::set_scene_name);
	ClassDB::bind_method(D_METHOD("get_scene_scene_transition_override", "scene_name", "callback"), &OBSClient::get_scene_scene_transition_override, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("set_scene_scene_transition_override", "scene_name", "transition_name", "transition_duration"), &OBSClient::set_scene_scene_transition_override, DEFVAL(-1));

	// Stream Requests
	ClassDB::bind_method(D_METHOD("get_stream_status", "callback"), &OBSClient::get_stream_status, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("toggle_stream", "callback"), &OBSClient::toggle_stream, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("start_stream"), &OBSClient::start_stream);
	ClassDB::bind_method(D_METHOD("stop_stream"), &OBSClient::stop_stream);
	ClassDB::bind_method(D_METHOD("send_stream_caption", "caption_text"), &OBSClient::send_stream_caption);

	// Record Requests
	ClassDB::bind_method(D_METHOD("get_record_status", "callback"), &OBSClient::get_record_status, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("toggle_record", "callback"), &OBSClient::toggle_record, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("start_record"), &OBSClient::start_record);
	ClassDB::bind_method(D_METHOD("stop_record", "callback"), &OBSClient::stop_record, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("toggle_record_pause"), &OBSClient::toggle_record_pause);
	ClassDB::bind_method(D_METHOD("pause_record"), &OBSClient::pause_record);
	ClassDB::bind_method(D_METHOD("resume_record"), &OBSClient::resume_record);
	ClassDB::bind_method(D_METHOD("split_record_file"), &OBSClient::split_record_file);
	ClassDB::bind_method(D_METHOD("create_record_chapter", "chapter_name"), &OBSClient::create_record_chapter, DEFVAL(String()));

	// Output Requests
	ClassDB::bind_method(D_METHOD("get_virtual_cam_status", "callback"), &OBSClient::get_virtual_cam_status, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("toggle_virtual_cam", "callback"), &OBSClient::toggle_virtual_cam, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("start_virtual_cam"), &OBSClient::start_virtual_cam);
	ClassDB::bind_method(D_METHOD("stop_virtual_cam"), &OBSClient::stop_virtual_cam);
	ClassDB::bind_method(D_METHOD("get_replay_buffer_status", "callback"), &OBSClient::get_replay_buffer_status, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("toggle_replay_buffer", "callback"), &OBSClient::toggle_replay_buffer, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("start_replay_buffer"), &OBSClient::start_replay_buffer);
	ClassDB::bind_method(D_METHOD("stop_replay_buffer"), &OBSClient::stop_replay_buffer);
	ClassDB::bind_method(D_METHOD("save_replay_buffer"), &OBSClient::save_replay_buffer);
	ClassDB::bind_method(D_METHOD("get_last_replay_buffer_replay", "callback"), &OBSClient::get_last_replay_buffer_replay, DEFVAL(Callable()));

	// Inputs Requests
	ClassDB::bind_method(D_METHOD("get_input_list", "input_kind", "callback"), &OBSClient::get_input_list, DEFVAL(String()), DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("get_input_kind_list", "unversioned", "callback"), &OBSClient::get_input_kind_list, DEFVAL(false), DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("get_special_inputs", "callback"), &OBSClient::get_special_inputs, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("create_input", "scene_name", "input_name", "input_kind", "input_settings", "scene_item_enabled", "callback"), &OBSClient::create_input, DEFVAL(Dictionary()), DEFVAL(true), DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("remove_input", "input_name"), &OBSClient::remove_input);
	ClassDB::bind_method(D_METHOD("set_input_name", "input_name", "new_input_name"), &OBSClient::set_input_name);
	ClassDB::bind_method(D_METHOD("get_input_default_settings", "input_kind", "callback"), &OBSClient::get_input_default_settings, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("get_input_settings", "input_name", "callback"), &OBSClient::get_input_settings, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("set_input_settings", "input_name", "input_settings", "overlay"), &OBSClient::set_input_settings, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("get_input_mute", "input_name", "callback"), &OBSClient::get_input_mute, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("set_input_mute", "input_name", "input_muted"), &OBSClient::set_input_mute);
	ClassDB::bind_method(D_METHOD("toggle_input_mute", "input_name", "callback"), &OBSClient::toggle_input_mute, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("get_input_volume", "input_name", "callback"), &OBSClient::get_input_volume, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("set_input_volume", "input_name", "input_volume_mul", "input_volume_db"), &OBSClient::set_input_volume, DEFVAL(-1.0), DEFVAL(0.0));

	// Transitions Requests
	ClassDB::bind_method(D_METHOD("get_transition_kind_list", "callback"), &OBSClient::get_transition_kind_list, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("get_scene_transition_list", "callback"), &OBSClient::get_scene_transition_list, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("get_current_scene_transition", "callback"), &OBSClient::get_current_scene_transition, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("set_current_scene_transition", "transition_name"), &OBSClient::set_current_scene_transition);
	ClassDB::bind_method(D_METHOD("set_current_scene_transition_duration", "transition_duration"), &OBSClient::set_current_scene_transition_duration);
	ClassDB::bind_method(D_METHOD("set_current_scene_transition_settings", "transition_settings", "overlay"), &OBSClient::set_current_scene_transition_settings, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("trigger_studio_mode_transition"), &OBSClient::trigger_studio_mode_transition);
	ClassDB::bind_method(D_METHOD("set_tbar_position", "position", "release"), &OBSClient::set_tbar_position, DEFVAL(true));

	// Filters Requests
	ClassDB::bind_method(D_METHOD("get_source_filter_kind_list", "callback"), &OBSClient::get_source_filter_kind_list, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("get_source_filter_list", "source_name", "callback"), &OBSClient::get_source_filter_list, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("get_source_filter_default_settings", "filter_kind", "callback"), &OBSClient::get_source_filter_default_settings, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("create_source_filter", "source_name", "filter_name", "filter_kind", "filter_settings"), &OBSClient::create_source_filter, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("remove_source_filter", "source_name", "filter_name"), &OBSClient::remove_source_filter);
	ClassDB::bind_method(D_METHOD("set_source_filter_name", "source_name", "filter_name", "new_filter_name"), &OBSClient::set_source_filter_name);
	ClassDB::bind_method(D_METHOD("get_source_filter", "source_name", "filter_name", "callback"), &OBSClient::get_source_filter, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("set_source_filter_index", "source_name", "filter_name", "filter_index"), &OBSClient::set_source_filter_index);
	ClassDB::bind_method(D_METHOD("set_source_filter_settings", "source_name", "filter_name", "filter_settings", "overlay"), &OBSClient::set_source_filter_settings, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("set_source_filter_enabled", "source_name", "filter_name", "filter_enabled"), &OBSClient::set_source_filter_enabled);

	// Scene Items Requests
	ClassDB::bind_method(D_METHOD("get_scene_item_list", "scene_name", "callback"), &OBSClient::get_scene_item_list, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("get_group_scene_item_list", "scene_name", "callback"), &OBSClient::get_group_scene_item_list, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("get_scene_item_id", "scene_name", "source_name", "search_offset", "callback"), &OBSClient::get_scene_item_id, DEFVAL(0), DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("create_scene_item", "scene_name", "source_name", "scene_item_enabled", "callback"), &OBSClient::create_scene_item, DEFVAL(true), DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("remove_scene_item", "scene_name", "scene_item_id"), &OBSClient::remove_scene_item);
	ClassDB::bind_method(D_METHOD("duplicate_scene_item", "scene_name", "scene_item_id", "destination_scene_name", "callback"), &OBSClient::duplicate_scene_item, DEFVAL(String()), DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("get_scene_item_transform", "scene_name", "scene_item_id", "callback"), &OBSClient::get_scene_item_transform, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("set_scene_item_transform", "scene_name", "scene_item_id", "scene_item_transform"), &OBSClient::set_scene_item_transform);
	ClassDB::bind_method(D_METHOD("get_scene_item_enabled", "scene_name", "scene_item_id", "callback"), &OBSClient::get_scene_item_enabled, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("set_scene_item_enabled", "scene_name", "scene_item_id", "scene_item_enabled"), &OBSClient::set_scene_item_enabled);
	ClassDB::bind_method(D_METHOD("get_scene_item_locked", "scene_name", "scene_item_id", "callback"), &OBSClient::get_scene_item_locked, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("set_scene_item_locked", "scene_name", "scene_item_id", "scene_item_locked"), &OBSClient::set_scene_item_locked);
	ClassDB::bind_method(D_METHOD("get_scene_item_index", "scene_name", "scene_item_id", "callback"), &OBSClient::get_scene_item_index, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("set_scene_item_index", "scene_name", "scene_item_id", "scene_item_index"), &OBSClient::set_scene_item_index);
	ClassDB::bind_method(D_METHOD("get_scene_item_blend_mode", "scene_name", "scene_item_id", "callback"), &OBSClient::get_scene_item_blend_mode, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("set_scene_item_blend_mode", "scene_name", "scene_item_id", "scene_item_blend_mode"), &OBSClient::set_scene_item_blend_mode);

	// Media Inputs Requests
	ClassDB::bind_method(D_METHOD("get_media_input_status", "input_name", "callback"), &OBSClient::get_media_input_status, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("set_media_input_cursor", "input_name", "media_cursor"), &OBSClient::set_media_input_cursor);
	ClassDB::bind_method(D_METHOD("offset_media_input_cursor", "input_name", "media_cursor_offset"), &OBSClient::offset_media_input_cursor);
	ClassDB::bind_method(D_METHOD("trigger_media_input_action", "input_name", "media_action"), &OBSClient::trigger_media_input_action);

	// Sources Requests
	ClassDB::bind_method(D_METHOD("get_source_active", "source_name", "callback"), &OBSClient::get_source_active, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("get_source_screenshot", "source_name", "image_format", "image_width", "image_height", "image_compression_quality", "callback"), &OBSClient::get_source_screenshot, DEFVAL(-1), DEFVAL(-1), DEFVAL(-1), DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("save_source_screenshot", "source_name", "image_format", "image_file_path", "image_width", "image_height", "image_compression_quality"), &OBSClient::save_source_screenshot, DEFVAL(-1), DEFVAL(-1), DEFVAL(-1));

	// UI Requests
	ClassDB::bind_method(D_METHOD("get_studio_mode_enabled", "callback"), &OBSClient::get_studio_mode_enabled, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("set_studio_mode_enabled", "studio_mode_enabled"), &OBSClient::set_studio_mode_enabled);
	ClassDB::bind_method(D_METHOD("open_input_properties_dialog", "input_name"), &OBSClient::open_input_properties_dialog);
	ClassDB::bind_method(D_METHOD("open_input_filters_dialog", "input_name"), &OBSClient::open_input_filters_dialog);
	ClassDB::bind_method(D_METHOD("open_input_interact_dialog", "input_name"), &OBSClient::open_input_interact_dialog);
	ClassDB::bind_method(D_METHOD("get_monitor_list", "callback"), &OBSClient::get_monitor_list, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("open_video_mix_projector", "video_mix_type", "monitor_index", "projector_geometry"), &OBSClient::open_video_mix_projector, DEFVAL(-1), DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("open_source_projector", "source_name", "monitor_index", "projector_geometry"), &OBSClient::open_source_projector, DEFVAL(-1), DEFVAL(String()));

	// Signals - General Events
	ADD_SIGNAL(MethodInfo("exit_started"));
	ADD_SIGNAL(MethodInfo("vendor_event", PropertyInfo(Variant::STRING, "vendor_name"), PropertyInfo(Variant::STRING, "event_type"), PropertyInfo(Variant::DICTIONARY, "event_data")));
	ADD_SIGNAL(MethodInfo("custom_event", PropertyInfo(Variant::DICTIONARY, "event_data")));

	// Signals - Config Events
	ADD_SIGNAL(MethodInfo("current_scene_collection_changing", PropertyInfo(Variant::STRING, "scene_collection_name")));
	ADD_SIGNAL(MethodInfo("current_scene_collection_changed", PropertyInfo(Variant::STRING, "scene_collection_name")));
	ADD_SIGNAL(MethodInfo("scene_collection_list_changed", PropertyInfo(Variant::ARRAY, "scene_collections")));
	ADD_SIGNAL(MethodInfo("current_profile_changing", PropertyInfo(Variant::STRING, "profile_name")));
	ADD_SIGNAL(MethodInfo("current_profile_changed", PropertyInfo(Variant::STRING, "profile_name")));
	ADD_SIGNAL(MethodInfo("profile_list_changed", PropertyInfo(Variant::ARRAY, "profiles")));

	// Signals - Scenes Events
	ADD_SIGNAL(MethodInfo("scene_created", PropertyInfo(Variant::STRING, "scene_name"), PropertyInfo(Variant::STRING, "scene_uuid"), PropertyInfo(Variant::BOOL, "is_group")));
	ADD_SIGNAL(MethodInfo("scene_removed", PropertyInfo(Variant::STRING, "scene_name"), PropertyInfo(Variant::STRING, "scene_uuid"), PropertyInfo(Variant::BOOL, "is_group")));
	ADD_SIGNAL(MethodInfo("scene_name_changed", PropertyInfo(Variant::STRING, "scene_uuid"), PropertyInfo(Variant::STRING, "old_scene_name"), PropertyInfo(Variant::STRING, "scene_name")));
	ADD_SIGNAL(MethodInfo("current_program_scene_changed", PropertyInfo(Variant::STRING, "scene_name"), PropertyInfo(Variant::STRING, "scene_uuid")));
	ADD_SIGNAL(MethodInfo("current_preview_scene_changed", PropertyInfo(Variant::STRING, "scene_name"), PropertyInfo(Variant::STRING, "scene_uuid")));
	ADD_SIGNAL(MethodInfo("scene_list_changed", PropertyInfo(Variant::ARRAY, "scenes")));

	// Signals - Inputs Events
	ADD_SIGNAL(MethodInfo("input_created", PropertyInfo(Variant::STRING, "input_name"), PropertyInfo(Variant::STRING, "input_uuid"), PropertyInfo(Variant::STRING, "input_kind"), PropertyInfo(Variant::DICTIONARY, "input_settings")));
	ADD_SIGNAL(MethodInfo("input_removed", PropertyInfo(Variant::STRING, "input_name"), PropertyInfo(Variant::STRING, "input_uuid")));
	ADD_SIGNAL(MethodInfo("input_name_changed", PropertyInfo(Variant::STRING, "input_uuid"), PropertyInfo(Variant::STRING, "old_input_name"), PropertyInfo(Variant::STRING, "input_name")));
	ADD_SIGNAL(MethodInfo("input_settings_changed", PropertyInfo(Variant::STRING, "input_name"), PropertyInfo(Variant::STRING, "input_uuid"), PropertyInfo(Variant::DICTIONARY, "input_settings")));
	ADD_SIGNAL(MethodInfo("input_active_state_changed", PropertyInfo(Variant::STRING, "input_name"), PropertyInfo(Variant::STRING, "input_uuid"), PropertyInfo(Variant::BOOL, "video_active")));
	ADD_SIGNAL(MethodInfo("input_show_state_changed", PropertyInfo(Variant::STRING, "input_name"), PropertyInfo(Variant::STRING, "input_uuid"), PropertyInfo(Variant::BOOL, "video_showing")));
	ADD_SIGNAL(MethodInfo("input_mute_state_changed", PropertyInfo(Variant::STRING, "input_name"), PropertyInfo(Variant::STRING, "input_uuid"), PropertyInfo(Variant::BOOL, "input_muted")));
	ADD_SIGNAL(MethodInfo("input_volume_changed", PropertyInfo(Variant::STRING, "input_name"), PropertyInfo(Variant::STRING, "input_uuid"), PropertyInfo(Variant::FLOAT, "input_volume_mul"), PropertyInfo(Variant::FLOAT, "input_volume_db")));
	ADD_SIGNAL(MethodInfo("input_audio_balance_changed", PropertyInfo(Variant::STRING, "input_name"), PropertyInfo(Variant::STRING, "input_uuid"), PropertyInfo(Variant::FLOAT, "input_audio_balance")));
	ADD_SIGNAL(MethodInfo("input_audio_sync_offset_changed", PropertyInfo(Variant::STRING, "input_name"), PropertyInfo(Variant::STRING, "input_uuid"), PropertyInfo(Variant::INT, "input_audio_sync_offset")));
	ADD_SIGNAL(MethodInfo("input_audio_tracks_changed", PropertyInfo(Variant::STRING, "input_name"), PropertyInfo(Variant::STRING, "input_uuid"), PropertyInfo(Variant::DICTIONARY, "input_audio_tracks")));
	ADD_SIGNAL(MethodInfo("input_audio_monitor_type_changed", PropertyInfo(Variant::STRING, "input_name"), PropertyInfo(Variant::STRING, "input_uuid"), PropertyInfo(Variant::STRING, "monitor_type")));
	ADD_SIGNAL(MethodInfo("input_volume_meters", PropertyInfo(Variant::ARRAY, "inputs")));

	// Signals - Transitions Events
	ADD_SIGNAL(MethodInfo("current_scene_transition_changed", PropertyInfo(Variant::STRING, "transition_name"), PropertyInfo(Variant::STRING, "transition_uuid")));
	ADD_SIGNAL(MethodInfo("current_scene_transition_duration_changed", PropertyInfo(Variant::INT, "transition_duration")));
	ADD_SIGNAL(MethodInfo("scene_transition_started", PropertyInfo(Variant::STRING, "transition_name"), PropertyInfo(Variant::STRING, "transition_uuid")));
	ADD_SIGNAL(MethodInfo("scene_transition_ended", PropertyInfo(Variant::STRING, "transition_name"), PropertyInfo(Variant::STRING, "transition_uuid")));
	ADD_SIGNAL(MethodInfo("scene_transition_video_ended", PropertyInfo(Variant::STRING, "transition_name"), PropertyInfo(Variant::STRING, "transition_uuid")));

	// Signals - Filters Events
	ADD_SIGNAL(MethodInfo("source_filter_list_reindexed", PropertyInfo(Variant::STRING, "source_name"), PropertyInfo(Variant::ARRAY, "filters")));
	ADD_SIGNAL(MethodInfo("source_filter_created", PropertyInfo(Variant::STRING, "source_name"), PropertyInfo(Variant::STRING, "filter_name"), PropertyInfo(Variant::STRING, "filter_kind"), PropertyInfo(Variant::INT, "filter_index"), PropertyInfo(Variant::DICTIONARY, "filter_settings")));
	ADD_SIGNAL(MethodInfo("source_filter_removed", PropertyInfo(Variant::STRING, "source_name"), PropertyInfo(Variant::STRING, "filter_name")));
	ADD_SIGNAL(MethodInfo("source_filter_name_changed", PropertyInfo(Variant::STRING, "source_name"), PropertyInfo(Variant::STRING, "old_filter_name"), PropertyInfo(Variant::STRING, "filter_name")));
	ADD_SIGNAL(MethodInfo("source_filter_settings_changed", PropertyInfo(Variant::STRING, "source_name"), PropertyInfo(Variant::STRING, "filter_name"), PropertyInfo(Variant::DICTIONARY, "filter_settings")));
	ADD_SIGNAL(MethodInfo("source_filter_enable_state_changed", PropertyInfo(Variant::STRING, "source_name"), PropertyInfo(Variant::STRING, "filter_name"), PropertyInfo(Variant::BOOL, "filter_enabled")));

	// Signals - Scene Items Events
	ADD_SIGNAL(MethodInfo("scene_item_created", PropertyInfo(Variant::STRING, "scene_name"), PropertyInfo(Variant::STRING, "scene_uuid"), PropertyInfo(Variant::STRING, "source_name"), PropertyInfo(Variant::INT, "scene_item_id"), PropertyInfo(Variant::INT, "scene_item_index")));
	ADD_SIGNAL(MethodInfo("scene_item_removed", PropertyInfo(Variant::STRING, "scene_name"), PropertyInfo(Variant::STRING, "scene_uuid"), PropertyInfo(Variant::STRING, "source_name"), PropertyInfo(Variant::INT, "scene_item_id")));
	ADD_SIGNAL(MethodInfo("scene_item_list_reindexed", PropertyInfo(Variant::STRING, "scene_name"), PropertyInfo(Variant::STRING, "scene_uuid"), PropertyInfo(Variant::ARRAY, "scene_items")));
	ADD_SIGNAL(MethodInfo("scene_item_enable_state_changed", PropertyInfo(Variant::STRING, "scene_name"), PropertyInfo(Variant::STRING, "scene_uuid"), PropertyInfo(Variant::INT, "scene_item_id"), PropertyInfo(Variant::BOOL, "scene_item_enabled")));
	ADD_SIGNAL(MethodInfo("scene_item_lock_state_changed", PropertyInfo(Variant::STRING, "scene_name"), PropertyInfo(Variant::STRING, "scene_uuid"), PropertyInfo(Variant::INT, "scene_item_id"), PropertyInfo(Variant::BOOL, "scene_item_locked")));
	ADD_SIGNAL(MethodInfo("scene_item_selected", PropertyInfo(Variant::STRING, "scene_name"), PropertyInfo(Variant::STRING, "scene_uuid"), PropertyInfo(Variant::INT, "scene_item_id")));
	ADD_SIGNAL(MethodInfo("scene_item_transform_changed", PropertyInfo(Variant::STRING, "scene_name"), PropertyInfo(Variant::STRING, "scene_uuid"), PropertyInfo(Variant::INT, "scene_item_id"), PropertyInfo(Variant::DICTIONARY, "scene_item_transform")));

	// Signals - Outputs Events (additional)
	ADD_SIGNAL(MethodInfo("stream_state_changed", PropertyInfo(Variant::BOOL, "output_active"), PropertyInfo(Variant::STRING, "output_state")));
	ADD_SIGNAL(MethodInfo("record_state_changed", PropertyInfo(Variant::BOOL, "output_active"), PropertyInfo(Variant::STRING, "output_state"), PropertyInfo(Variant::STRING, "output_path")));
	ADD_SIGNAL(MethodInfo("record_file_changed", PropertyInfo(Variant::STRING, "new_output_path")));
	ADD_SIGNAL(MethodInfo("replay_buffer_state_changed", PropertyInfo(Variant::BOOL, "output_active"), PropertyInfo(Variant::STRING, "output_state")));
	ADD_SIGNAL(MethodInfo("virtualcam_state_changed", PropertyInfo(Variant::BOOL, "output_active"), PropertyInfo(Variant::STRING, "output_state")));
	ADD_SIGNAL(MethodInfo("replay_buffer_saved", PropertyInfo(Variant::STRING, "saved_replay_path")));

	// Signals - Media Inputs Events
	ADD_SIGNAL(MethodInfo("media_input_playback_started", PropertyInfo(Variant::STRING, "input_name"), PropertyInfo(Variant::STRING, "input_uuid")));
	ADD_SIGNAL(MethodInfo("media_input_playback_ended", PropertyInfo(Variant::STRING, "input_name"), PropertyInfo(Variant::STRING, "input_uuid")));
	ADD_SIGNAL(MethodInfo("media_input_action_triggered", PropertyInfo(Variant::STRING, "input_name"), PropertyInfo(Variant::STRING, "input_uuid"), PropertyInfo(Variant::STRING, "media_action")));

	// Signals - UI Events
	ADD_SIGNAL(MethodInfo("studio_mode_state_changed", PropertyInfo(Variant::BOOL, "studio_mode_enabled")));
	ADD_SIGNAL(MethodInfo("screenshot_saved", PropertyInfo(Variant::STRING, "saved_screenshot_path")));

	// Bind connection state enums
	BIND_ENUM_CONSTANT(STATE_DISCONNECTED);
	BIND_ENUM_CONSTANT(STATE_CONNECTING);
	BIND_ENUM_CONSTANT(STATE_IDENTIFYING);
	BIND_ENUM_CONSTANT(STATE_CONNECTED);

	// Bind WebSocket OpCodes
	BIND_CONSTANT(OBS_WEBSOCKET_OPCODE_HELLO);
	BIND_CONSTANT(OBS_WEBSOCKET_OPCODE_IDENTIFY);
	BIND_CONSTANT(OBS_WEBSOCKET_OPCODE_IDENTIFIED);
	BIND_CONSTANT(OBS_WEBSOCKET_OPCODE_REIDENTIFY);
	BIND_CONSTANT(OBS_WEBSOCKET_OPCODE_EVENT);
	BIND_CONSTANT(OBS_WEBSOCKET_OPCODE_REQUEST);
	BIND_CONSTANT(OBS_WEBSOCKET_OPCODE_REQUEST_RESPONSE);
	BIND_CONSTANT(OBS_WEBSOCKET_OPCODE_REQUEST_BATCH);
	BIND_CONSTANT(OBS_WEBSOCKET_OPCODE_REQUEST_BATCH_RESPONSE);

	// Bind Event Subscription flags
	BIND_CONSTANT(OBS_EVENT_SUBSCRIPTION_NONE);
	BIND_CONSTANT(OBS_EVENT_SUBSCRIPTION_GENERAL);
	BIND_CONSTANT(OBS_EVENT_SUBSCRIPTION_CONFIG);
	BIND_CONSTANT(OBS_EVENT_SUBSCRIPTION_SCENES);
	BIND_CONSTANT(OBS_EVENT_SUBSCRIPTION_INPUTS);
	BIND_CONSTANT(OBS_EVENT_SUBSCRIPTION_TRANSITIONS);
	BIND_CONSTANT(OBS_EVENT_SUBSCRIPTION_FILTERS);
	BIND_CONSTANT(OBS_EVENT_SUBSCRIPTION_OUTPUTS);
	BIND_CONSTANT(OBS_EVENT_SUBSCRIPTION_SCENE_ITEMS);
	BIND_CONSTANT(OBS_EVENT_SUBSCRIPTION_MEDIA_INPUTS);
	BIND_CONSTANT(OBS_EVENT_SUBSCRIPTION_VENDORS);
	BIND_CONSTANT(OBS_EVENT_SUBSCRIPTION_UI);
	BIND_CONSTANT(OBS_EVENT_SUBSCRIPTION_INPUT_VOLUME_METERS);
	BIND_CONSTANT(OBS_EVENT_SUBSCRIPTION_INPUT_ACTIVE_STATE_CHANGED);
	BIND_CONSTANT(OBS_EVENT_SUBSCRIPTION_INPUT_SHOW_STATE_CHANGED);
	BIND_CONSTANT(OBS_EVENT_SUBSCRIPTION_SCENE_ITEM_TRANSFORM_CHANGED);
	BIND_CONSTANT(OBS_EVENT_SUBSCRIPTION_ALL);

	// Bind Request Status codes
	BIND_CONSTANT(OBS_REQUEST_STATUS_UNKNOWN);
	BIND_CONSTANT(OBS_REQUEST_STATUS_NO_ERROR);
	BIND_CONSTANT(OBS_REQUEST_STATUS_SUCCESS);
	BIND_CONSTANT(OBS_REQUEST_STATUS_MISSING_REQUEST_TYPE);
	BIND_CONSTANT(OBS_REQUEST_STATUS_UNKNOWN_REQUEST_TYPE);
	BIND_CONSTANT(OBS_REQUEST_STATUS_GENERIC_ERROR);
	BIND_CONSTANT(OBS_REQUEST_STATUS_UNSUPPORTED_REQUEST_BATCH_EXECUTION_TYPE);
	BIND_CONSTANT(OBS_REQUEST_STATUS_NOT_READY);
	BIND_CONSTANT(OBS_REQUEST_STATUS_MISSING_REQUEST_FIELD);
	BIND_CONSTANT(OBS_REQUEST_STATUS_MISSING_REQUEST_DATA);
	BIND_CONSTANT(OBS_REQUEST_STATUS_INVALID_REQUEST_FIELD);
	BIND_CONSTANT(OBS_REQUEST_STATUS_INVALID_REQUEST_FIELD_TYPE);
	BIND_CONSTANT(OBS_REQUEST_STATUS_REQUEST_FIELD_OUT_OF_RANGE);
	BIND_CONSTANT(OBS_REQUEST_STATUS_REQUEST_FIELD_EMPTY);
	BIND_CONSTANT(OBS_REQUEST_STATUS_TOO_MANY_REQUEST_FIELDS);
	BIND_CONSTANT(OBS_REQUEST_STATUS_OUTPUT_RUNNING);
	BIND_CONSTANT(OBS_REQUEST_STATUS_OUTPUT_NOT_RUNNING);
	BIND_CONSTANT(OBS_REQUEST_STATUS_OUTPUT_PAUSED);
	BIND_CONSTANT(OBS_REQUEST_STATUS_OUTPUT_NOT_PAUSED);
	BIND_CONSTANT(OBS_REQUEST_STATUS_OUTPUT_DISABLED);
	BIND_CONSTANT(OBS_REQUEST_STATUS_STUDIO_MODE_ACTIVE);
	BIND_CONSTANT(OBS_REQUEST_STATUS_STUDIO_MODE_NOT_ACTIVE);
	BIND_CONSTANT(OBS_REQUEST_STATUS_RESOURCE_NOT_FOUND);
	BIND_CONSTANT(OBS_REQUEST_STATUS_RESOURCE_ALREADY_EXISTS);
	BIND_CONSTANT(OBS_REQUEST_STATUS_INVALID_RESOURCE_TYPE);
	BIND_CONSTANT(OBS_REQUEST_STATUS_NOT_ENOUGH_RESOURCES);
	BIND_CONSTANT(OBS_REQUEST_STATUS_INVALID_RESOURCE_STATE);
	BIND_CONSTANT(OBS_REQUEST_STATUS_INVALID_INPUT_KIND);
	BIND_CONSTANT(OBS_REQUEST_STATUS_RESOURCE_NOT_CONFIGURABLE);
	BIND_CONSTANT(OBS_REQUEST_STATUS_INVALID_FILTER_KIND);
	BIND_CONSTANT(OBS_REQUEST_STATUS_RESOURCE_CREATION_FAILED);
	BIND_CONSTANT(OBS_REQUEST_STATUS_RESOURCE_ACTION_FAILED);
	BIND_CONSTANT(OBS_REQUEST_STATUS_REQUEST_PROCESSING_FAILED);
	BIND_CONSTANT(OBS_REQUEST_STATUS_CANNOT_ACT);

	// Bind Request Batch Execution Types
	BIND_CONSTANT(OBS_REQUEST_BATCH_EXECUTION_TYPE_NONE);
	BIND_CONSTANT(OBS_REQUEST_BATCH_EXECUTION_TYPE_SERIAL_REALTIME);
	BIND_CONSTANT(OBS_REQUEST_BATCH_EXECUTION_TYPE_SERIAL_FRAME);
	BIND_CONSTANT(OBS_REQUEST_BATCH_EXECUTION_TYPE_PARALLEL);

	// Bind WebSocket Close Codes
	BIND_CONSTANT(OBS_WEBSOCKET_CLOSE_DONT_CLOSE);
	BIND_CONSTANT(OBS_WEBSOCKET_CLOSE_UNKNOWN_REASON);
	BIND_CONSTANT(OBS_WEBSOCKET_CLOSE_MESSAGE_DECODE_ERROR);
	BIND_CONSTANT(OBS_WEBSOCKET_CLOSE_MISSING_DATA_FIELD);
	BIND_CONSTANT(OBS_WEBSOCKET_CLOSE_INVALID_DATA_FIELD_TYPE);
	BIND_CONSTANT(OBS_WEBSOCKET_CLOSE_INVALID_DATA_FIELD_VALUE);
	BIND_CONSTANT(OBS_WEBSOCKET_CLOSE_UNKNOWN_OPCODE);
	BIND_CONSTANT(OBS_WEBSOCKET_CLOSE_NOT_IDENTIFIED);
	BIND_CONSTANT(OBS_WEBSOCKET_CLOSE_ALREADY_IDENTIFIED);
	BIND_CONSTANT(OBS_WEBSOCKET_CLOSE_AUTHENTICATION_FAILED);
	BIND_CONSTANT(OBS_WEBSOCKET_CLOSE_UNSUPPORTED_RPC_VERSION);
	BIND_CONSTANT(OBS_WEBSOCKET_CLOSE_SESSION_INVALIDATED);
	BIND_CONSTANT(OBS_WEBSOCKET_CLOSE_UNSUPPORTED_FEATURE);

	// Bind Media Input Actions
	BIND_CONSTANT(OBS_MEDIA_INPUT_ACTION_NONE);
	BIND_CONSTANT(OBS_MEDIA_INPUT_ACTION_PLAY);
	BIND_CONSTANT(OBS_MEDIA_INPUT_ACTION_PAUSE);
	BIND_CONSTANT(OBS_MEDIA_INPUT_ACTION_STOP);
	BIND_CONSTANT(OBS_MEDIA_INPUT_ACTION_RESTART);
	BIND_CONSTANT(OBS_MEDIA_INPUT_ACTION_NEXT);
	BIND_CONSTANT(OBS_MEDIA_INPUT_ACTION_PREVIOUS);

	// Bind Output States
	BIND_CONSTANT(OBS_OUTPUT_STATE_UNKNOWN);
	BIND_CONSTANT(OBS_OUTPUT_STATE_STARTING);
	BIND_CONSTANT(OBS_OUTPUT_STATE_STARTED);
	BIND_CONSTANT(OBS_OUTPUT_STATE_STOPPING);
	BIND_CONSTANT(OBS_OUTPUT_STATE_STOPPED);
	BIND_CONSTANT(OBS_OUTPUT_STATE_RECONNECTING);
	BIND_CONSTANT(OBS_OUTPUT_STATE_RECONNECTED);
	BIND_CONSTANT(OBS_OUTPUT_STATE_PAUSED);
	BIND_CONSTANT(OBS_OUTPUT_STATE_RESUMED);
}

OBSClient::OBSClient() {
	singleton = this;
}

OBSClient::~OBSClient() {
	disconnect_from_obs();
	singleton = nullptr;
}

// Connection management

Error OBSClient::connect_to_obs(const String &p_url, const String &p_password, int p_event_subscriptions) {
	if (connection_state != STATE_DISCONNECTED) {
		disconnect_from_obs();
	}

	obs_url = p_url;
	obs_password = p_password;
	event_subscriptions = p_event_subscriptions;

	ws = Ref<WebSocketPeer>(WebSocketPeer::create());
	ERR_FAIL_COND_V(ws.is_null(), ERR_CANT_CREATE);

	Error err = ws->connect_to_url(obs_url);
	if (err != OK) {
		ws.unref();
		return err;
	}

	connection_state = STATE_CONNECTING;
	return OK;
}

void OBSClient::disconnect_from_obs() {
	if (ws.is_valid()) {
		ws->close(1000, "Client disconnect");
		ws.unref();
	}

	connection_state = STATE_DISCONNECTED;
	pending_requests.clear();
	auth_salt = String();
	auth_challenge = String();
	server_info.clear();
	negotiated_rpc_version = 0;
}

void OBSClient::poll() {
	if (ws.is_null()) {
		return;
	}

	ws->poll();

	WebSocketPeer::State state = ws->get_ready_state();

	switch (state) {
		case WebSocketPeer::STATE_CONNECTING:
			// Still connecting
			break;

		case WebSocketPeer::STATE_OPEN:
			// Process incoming messages
			while (ws->get_available_packet_count() > 0) {
				const uint8_t *buffer;
				int buffer_size;

				if (ws->get_packet(&buffer, buffer_size) == OK) {
					if (ws->was_string_packet()) {
						String msg;
						msg.parse_utf8((const char *)buffer, buffer_size);
						process_message(msg);
					}
				}
			}
			break;

		case WebSocketPeer::STATE_CLOSING:
			// Closing handshake in progress
			break;

		case WebSocketPeer::STATE_CLOSED:
			// Connection closed
			int code = ws->get_close_code();
			String reason = ws->get_close_reason();
			print_line(vformat("OBS WebSocket closed: %d - %s", code, reason));
			connection_state = STATE_DISCONNECTED;
			break;
	}
}

OBSClient::ConnectionState OBSClient::get_connection_state() const {
	return connection_state;
}

bool OBSClient::is_obs_connected() const {
	return connection_state == STATE_CONNECTED;
}

Dictionary OBSClient::get_server_info() const {
	return server_info;
}

int OBSClient::get_negotiated_rpc_version() const {
	return negotiated_rpc_version;
}

// Helper methods

String OBSClient::generate_request_id() {
	return String::num_uint64(next_request_id++);
}

String OBSClient::generate_auth_string(const String &p_password, const String &p_salt, const String &p_challenge) {
	// Step 1: SHA256(password + salt) -> base64
	String combined1 = p_password + p_salt;
	CharString cs1 = combined1.utf8();
	unsigned char hash1[32];
	CryptoCore::sha256((unsigned char *)cs1.ptr(), cs1.length(), hash1);
	String base64_secret = CryptoCore::b64_encode_str(hash1, 32);

	// Step 2: SHA256(base64_secret + challenge) -> base64
	String combined2 = base64_secret + p_challenge;
	CharString cs2 = combined2.utf8();
	unsigned char hash2[32];
	CryptoCore::sha256((unsigned char *)cs2.ptr(), cs2.length(), hash2);
	String auth_string = CryptoCore::b64_encode_str(hash2, 32);

	return auth_string;
}

void OBSClient::send_message(int p_opcode, const Dictionary &p_data) {
	if (ws.is_null() || ws->get_ready_state() != WebSocketPeer::STATE_OPEN) {
		ERR_PRINT("Cannot send message: WebSocket not connected");
		return;
	}

	Dictionary message;
	message["op"] = p_opcode;
	message["d"] = p_data;

	String json_str = JSON::stringify(message);
	Error err = ws->send_text(json_str);
	if (err != OK) {
		ERR_PRINT("Failed to send WebSocket message");
	}
}

void OBSClient::process_message(const String &p_message) {
	Ref<JSON> json;
	json.instantiate();
	Error err = json->parse(p_message);
	if (err != OK) {
		ERR_PRINT(vformat("Failed to parse JSON message: %s", p_message));
		return;
	}

	Dictionary data = json->get_data();
	if (!data.has("op")) {
		ERR_PRINT("Message missing 'op' field");
		return;
	}

	int opcode = data["op"];
	Dictionary msg_data = data.get("d", Dictionary());

	switch (opcode) {
		case OBS_WEBSOCKET_OPCODE_HELLO:
			handle_hello(msg_data);
			break;
		case OBS_WEBSOCKET_OPCODE_IDENTIFIED:
			handle_identified(msg_data);
			break;
		case OBS_WEBSOCKET_OPCODE_EVENT:
			handle_event(msg_data);
			break;
		case OBS_WEBSOCKET_OPCODE_REQUEST_RESPONSE:
			handle_request_response(msg_data);
			break;
		case OBS_WEBSOCKET_OPCODE_REQUEST_BATCH_RESPONSE:
			handle_request_batch_response(msg_data);
			break;
		default:
			print_line(vformat("Unknown OpCode: %d", opcode));
			break;
	}
}

void OBSClient::handle_hello(const Dictionary &p_data) {
	if (connection_state != STATE_CONNECTING) {
		return;
	}

	// Store server info
	server_info = p_data;
	rpc_version = p_data.get("rpcVersion", 1);

	// Check for authentication requirement
	Dictionary identify_data;
	identify_data["rpcVersion"] = rpc_version;
	identify_data["eventSubscriptions"] = event_subscriptions;

	if (p_data.has("authentication")) {
		Dictionary auth_data = p_data["authentication"];
		auth_salt = auth_data.get("salt", String());
		auth_challenge = auth_data.get("challenge", String());

		if (!obs_password.is_empty()) {
			String auth_string = generate_auth_string(obs_password, auth_salt, auth_challenge);
			identify_data["authentication"] = auth_string;
		} else {
			ERR_PRINT("OBS requires authentication but no password provided");
			disconnect_from_obs();
			return;
		}
	}

	send_message(OBS_WEBSOCKET_OPCODE_IDENTIFY, identify_data);
	connection_state = STATE_IDENTIFYING;
}

void OBSClient::handle_identified(const Dictionary &p_data) {
	if (connection_state != STATE_IDENTIFYING) {
		return;
	}

	negotiated_rpc_version = p_data.get("negotiatedRpcVersion", 1);
	connection_state = STATE_CONNECTED;
	print_line("Successfully connected to OBS WebSocket");
}

void OBSClient::handle_event(const Dictionary &p_data) {
	if (!p_data.has("eventType")) {
		return;
	}

	String event_type = p_data["eventType"];
	int event_intent = p_data.get("eventIntent", 0);
	Dictionary event_data = p_data.get("eventData", Dictionary());

	// Emit signals based on event type
	if (event_type == "ExitStarted") {
		emit_signal("exit_started");
	} else if (event_type == "VendorEvent") {
		emit_signal("vendor_event", event_data.get("vendorName", ""), event_data.get("eventType", ""), event_data.get("eventData", Dictionary()));
	} else if (event_type == "CustomEvent") {
		emit_signal("custom_event", event_data);
	} else if (event_type == "CurrentSceneCollectionChanging") {
		emit_signal("current_scene_collection_changing", event_data.get("sceneCollectionName", ""));
	} else if (event_type == "CurrentSceneCollectionChanged") {
		emit_signal("current_scene_collection_changed", event_data.get("sceneCollectionName", ""));
	} else if (event_type == "SceneCollectionListChanged") {
		emit_signal("scene_collection_list_changed", event_data.get("sceneCollections", Array()));
	} else if (event_type == "CurrentProfileChanging") {
		emit_signal("current_profile_changing", event_data.get("profileName", ""));
	} else if (event_type == "CurrentProfileChanged") {
		emit_signal("current_profile_changed", event_data.get("profileName", ""));
	} else if (event_type == "ProfileListChanged") {
		emit_signal("profile_list_changed", event_data.get("profiles", Array()));
	} else if (event_type == "SceneCreated") {
		emit_signal("scene_created", event_data.get("sceneName", ""), event_data.get("sceneUuid", ""), event_data.get("isGroup", false));
	} else if (event_type == "SceneRemoved") {
		emit_signal("scene_removed", event_data.get("sceneName", ""), event_data.get("sceneUuid", ""), event_data.get("isGroup", false));
	} else if (event_type == "SceneNameChanged") {
		emit_signal("scene_name_changed", event_data.get("sceneUuid", ""), event_data.get("oldSceneName", ""), event_data.get("sceneName", ""));
	} else if (event_type == "CurrentProgramSceneChanged") {
		emit_signal("current_program_scene_changed", event_data.get("sceneName", ""), event_data.get("sceneUuid", ""));
	} else if (event_type == "CurrentPreviewSceneChanged") {
		emit_signal("current_preview_scene_changed", event_data.get("sceneName", ""), event_data.get("sceneUuid", ""));
	} else if (event_type == "SceneListChanged") {
		emit_signal("scene_list_changed", event_data.get("scenes", Array()));
	} else if (event_type == "StreamStateChanged") {
		emit_signal("stream_state_changed", event_data.get("outputActive", false), event_data.get("outputState", ""));
	} else if (event_type == "RecordStateChanged") {
		emit_signal("record_state_changed", event_data.get("outputActive", false), event_data.get("outputState", ""), event_data.get("outputPath", ""));
	} else if (event_type == "VirtualcamStateChanged") {
		emit_signal("virtualcam_state_changed", event_data.get("outputActive", false), event_data.get("outputState", ""));
	} else if (event_type == "ReplayBufferStateChanged") {
		emit_signal("replay_buffer_state_changed", event_data.get("outputActive", false), event_data.get("outputState", ""));
	} else if (event_type == "ReplayBufferSaved") {
		emit_signal("replay_buffer_saved", event_data.get("savedReplayPath", ""));
	} else if (event_type == "RecordFileChanged") {
		emit_signal("record_file_changed", event_data.get("newOutputPath", ""));
	}
	// Inputs Events
	else if (event_type == "InputCreated") {
		emit_signal("input_created", event_data.get("inputName", ""), event_data.get("inputUuid", ""), event_data.get("inputKind", ""), event_data.get("inputSettings", Dictionary()));
	} else if (event_type == "InputRemoved") {
		emit_signal("input_removed", event_data.get("inputName", ""), event_data.get("inputUuid", ""));
	} else if (event_type == "InputNameChanged") {
		emit_signal("input_name_changed", event_data.get("inputUuid", ""), event_data.get("oldInputName", ""), event_data.get("inputName", ""));
	} else if (event_type == "InputSettingsChanged") {
		emit_signal("input_settings_changed", event_data.get("inputName", ""), event_data.get("inputUuid", ""), event_data.get("inputSettings", Dictionary()));
	} else if (event_type == "InputActiveStateChanged") {
		emit_signal("input_active_state_changed", event_data.get("inputName", ""), event_data.get("inputUuid", ""), event_data.get("videoActive", false));
	} else if (event_type == "InputShowStateChanged") {
		emit_signal("input_show_state_changed", event_data.get("inputName", ""), event_data.get("inputUuid", ""), event_data.get("videoShowing", false));
	} else if (event_type == "InputMuteStateChanged") {
		emit_signal("input_mute_state_changed", event_data.get("inputName", ""), event_data.get("inputUuid", ""), event_data.get("inputMuted", false));
	} else if (event_type == "InputVolumeChanged") {
		emit_signal("input_volume_changed", event_data.get("inputName", ""), event_data.get("inputUuid", ""), event_data.get("inputVolumeMul", 0.0), event_data.get("inputVolumeDb", 0.0));
	} else if (event_type == "InputAudioBalanceChanged") {
		emit_signal("input_audio_balance_changed", event_data.get("inputName", ""), event_data.get("inputUuid", ""), event_data.get("inputAudioBalance", 0.0));
	} else if (event_type == "InputAudioSyncOffsetChanged") {
		emit_signal("input_audio_sync_offset_changed", event_data.get("inputName", ""), event_data.get("inputUuid", ""), event_data.get("inputAudioSyncOffset", 0));
	} else if (event_type == "InputAudioTracksChanged") {
		emit_signal("input_audio_tracks_changed", event_data.get("inputName", ""), event_data.get("inputUuid", ""), event_data.get("inputAudioTracks", Dictionary()));
	} else if (event_type == "InputAudioMonitorTypeChanged") {
		emit_signal("input_audio_monitor_type_changed", event_data.get("inputName", ""), event_data.get("inputUuid", ""), event_data.get("monitorType", ""));
	} else if (event_type == "InputVolumeMeters") {
		emit_signal("input_volume_meters", event_data.get("inputs", Array()));
	}
	// Transitions Events
	else if (event_type == "CurrentSceneTransitionChanged") {
		emit_signal("current_scene_transition_changed", event_data.get("transitionName", ""), event_data.get("transitionUuid", ""));
	} else if (event_type == "CurrentSceneTransitionDurationChanged") {
		emit_signal("current_scene_transition_duration_changed", event_data.get("transitionDuration", 0));
	} else if (event_type == "SceneTransitionStarted") {
		emit_signal("scene_transition_started", event_data.get("transitionName", ""), event_data.get("transitionUuid", ""));
	} else if (event_type == "SceneTransitionEnded") {
		emit_signal("scene_transition_ended", event_data.get("transitionName", ""), event_data.get("transitionUuid", ""));
	} else if (event_type == "SceneTransitionVideoEnded") {
		emit_signal("scene_transition_video_ended", event_data.get("transitionName", ""), event_data.get("transitionUuid", ""));
	}
	// Filters Events
	else if (event_type == "SourceFilterListReindexed") {
		emit_signal("source_filter_list_reindexed", event_data.get("sourceName", ""), event_data.get("filters", Array()));
	} else if (event_type == "SourceFilterCreated") {
		emit_signal("source_filter_created", event_data.get("sourceName", ""), event_data.get("filterName", ""), event_data.get("filterKind", ""), event_data.get("filterIndex", 0), event_data.get("filterSettings", Dictionary()));
	} else if (event_type == "SourceFilterRemoved") {
		emit_signal("source_filter_removed", event_data.get("sourceName", ""), event_data.get("filterName", ""));
	} else if (event_type == "SourceFilterNameChanged") {
		emit_signal("source_filter_name_changed", event_data.get("sourceName", ""), event_data.get("oldFilterName", ""), event_data.get("filterName", ""));
	} else if (event_type == "SourceFilterSettingsChanged") {
		emit_signal("source_filter_settings_changed", event_data.get("sourceName", ""), event_data.get("filterName", ""), event_data.get("filterSettings", Dictionary()));
	} else if (event_type == "SourceFilterEnableStateChanged") {
		emit_signal("source_filter_enable_state_changed", event_data.get("sourceName", ""), event_data.get("filterName", ""), event_data.get("filterEnabled", false));
	}
	// Scene Items Events
	else if (event_type == "SceneItemCreated") {
		emit_signal("scene_item_created", event_data.get("sceneName", ""), event_data.get("sceneUuid", ""), event_data.get("sourceName", ""), event_data.get("sceneItemId", 0), event_data.get("sceneItemIndex", 0));
	} else if (event_type == "SceneItemRemoved") {
		emit_signal("scene_item_removed", event_data.get("sceneName", ""), event_data.get("sceneUuid", ""), event_data.get("sourceName", ""), event_data.get("sceneItemId", 0));
	} else if (event_type == "SceneItemListReindexed") {
		emit_signal("scene_item_list_reindexed", event_data.get("sceneName", ""), event_data.get("sceneUuid", ""), event_data.get("sceneItems", Array()));
	} else if (event_type == "SceneItemEnableStateChanged") {
		emit_signal("scene_item_enable_state_changed", event_data.get("sceneName", ""), event_data.get("sceneUuid", ""), event_data.get("sceneItemId", 0), event_data.get("sceneItemEnabled", false));
	} else if (event_type == "SceneItemLockStateChanged") {
		emit_signal("scene_item_lock_state_changed", event_data.get("sceneName", ""), event_data.get("sceneUuid", ""), event_data.get("sceneItemId", 0), event_data.get("sceneItemLocked", false));
	} else if (event_type == "SceneItemSelected") {
		emit_signal("scene_item_selected", event_data.get("sceneName", ""), event_data.get("sceneUuid", ""), event_data.get("sceneItemId", 0));
	} else if (event_type == "SceneItemTransformChanged") {
		emit_signal("scene_item_transform_changed", event_data.get("sceneName", ""), event_data.get("sceneUuid", ""), event_data.get("sceneItemId", 0), event_data.get("sceneItemTransform", Dictionary()));
	}
	// Media Inputs Events
	else if (event_type == "MediaInputPlaybackStarted") {
		emit_signal("media_input_playback_started", event_data.get("inputName", ""), event_data.get("inputUuid", ""));
	} else if (event_type == "MediaInputPlaybackEnded") {
		emit_signal("media_input_playback_ended", event_data.get("inputName", ""), event_data.get("inputUuid", ""));
	} else if (event_type == "MediaInputActionTriggered") {
		emit_signal("media_input_action_triggered", event_data.get("inputName", ""), event_data.get("inputUuid", ""), event_data.get("mediaAction", ""));
	}
	// UI Events
	else if (event_type == "StudioModeStateChanged") {
		emit_signal("studio_mode_state_changed", event_data.get("studioModeEnabled", false));
	} else if (event_type == "ScreenshotSaved") {
		emit_signal("screenshot_saved", event_data.get("savedScreenshotPath", ""));
	}

	// Call registered callbacks
	for (KeyValue<int, Vector<Callable>> &E : event_callbacks) {
		if (event_intent & E.key) {
			for (const Callable &callback : E.value) {
				if (callback.is_valid()) {
					callback.call(event_type, event_data);
				}
			}
		}
	}
}

void OBSClient::handle_request_response(const Dictionary &p_data) {
	String request_id = p_data.get("requestId", "");
	if (request_id.is_empty()) {
		return;
	}

	if (pending_requests.has(request_id)) {
		Callable callback = pending_requests[request_id];
		pending_requests.erase(request_id);

		if (callback.is_valid()) {
			Dictionary request_status = p_data.get("requestStatus", Dictionary());
			Dictionary response_data = p_data.get("responseData", Dictionary());
			callback.call(request_status, response_data);
		}
	}
}

void OBSClient::handle_request_batch_response(const Dictionary &p_data) {
	String request_id = p_data.get("requestId", "");
	if (request_id.is_empty()) {
		return;
	}

	if (pending_requests.has(request_id)) {
		Callable callback = pending_requests[request_id];
		pending_requests.erase(request_id);

		if (callback.is_valid()) {
			Array results = p_data.get("results", Array());
			callback.call(results);
		}
	}
}

// Event subscription

void OBSClient::subscribe_to_events(int p_event_mask, const Callable &p_callback) {
	if (!event_callbacks.has(p_event_mask)) {
		event_callbacks[p_event_mask] = Vector<Callable>();
	}
	event_callbacks[p_event_mask].push_back(p_callback);
}

void OBSClient::unsubscribe_from_events(int p_event_mask, const Callable &p_callback) {
	if (event_callbacks.has(p_event_mask)) {
		event_callbacks[p_event_mask].erase(p_callback);
	}
}

void OBSClient::reidentify(int p_event_subscriptions) {
	Dictionary reidentify_data;
	reidentify_data["eventSubscriptions"] = p_event_subscriptions;
	send_message(OBS_WEBSOCKET_OPCODE_REIDENTIFY, reidentify_data);
	event_subscriptions = p_event_subscriptions;
}

// Request sending (generic)

String OBSClient::send_request(const String &p_request_type, const Dictionary &p_request_data, const Callable &p_callback) {
	ERR_FAIL_COND_V(!is_obs_connected(), String());

	String request_id = generate_request_id();

	Dictionary request_payload;
	request_payload["requestType"] = p_request_type;
	request_payload["requestId"] = request_id;
	if (!p_request_data.is_empty()) {
		request_payload["requestData"] = p_request_data;
	}

	send_message(OBS_WEBSOCKET_OPCODE_REQUEST, request_payload);

	if (p_callback.is_valid()) {
		pending_requests[request_id] = p_callback;
	}

	return request_id;
}

String OBSClient::send_request_batch(const Array &p_requests, bool p_halt_on_failure, int p_execution_type, const Callable &p_callback) {
	ERR_FAIL_COND_V(!is_obs_connected(), String());

	String request_id = generate_request_id();

	Dictionary batch_payload;
	batch_payload["requestId"] = request_id;
	batch_payload["haltOnFailure"] = p_halt_on_failure;
	batch_payload["executionType"] = p_execution_type;
	batch_payload["requests"] = p_requests;

	send_message(OBS_WEBSOCKET_OPCODE_REQUEST_BATCH, batch_payload);

	if (p_callback.is_valid()) {
		pending_requests[request_id] = p_callback;
	}

	return request_id;
}

// General Requests

void OBSClient::get_version(const Callable &p_callback) {
	send_request("GetVersion", Dictionary(), p_callback);
}

void OBSClient::get_stats(const Callable &p_callback) {
	send_request("GetStats", Dictionary(), p_callback);
}

void OBSClient::broadcast_custom_event(const Dictionary &p_event_data) {
	Dictionary request_data;
	request_data["eventData"] = p_event_data;
	send_request("BroadcastCustomEvent", request_data);
}

void OBSClient::call_vendor_request(const String &p_vendor_name, const String &p_request_type, const Dictionary &p_request_data, const Callable &p_callback) {
	Dictionary request_data;
	request_data["vendorName"] = p_vendor_name;
	request_data["requestType"] = p_request_type;
	if (!p_request_data.is_empty()) {
		request_data["requestData"] = p_request_data;
	}
	send_request("CallVendorRequest", request_data, p_callback);
}

void OBSClient::get_hotkey_list(const Callable &p_callback) {
	send_request("GetHotkeyList", Dictionary(), p_callback);
}

void OBSClient::trigger_hotkey_by_name(const String &p_hotkey_name, const String &p_context_name) {
	Dictionary request_data;
	request_data["hotkeyName"] = p_hotkey_name;
	if (!p_context_name.is_empty()) {
		request_data["contextName"] = p_context_name;
	}
	send_request("TriggerHotkeyByName", request_data);
}

void OBSClient::sleep(int p_sleep_millis, int p_sleep_frames) {
	Dictionary request_data;
	if (p_sleep_millis > 0) {
		request_data["sleepMillis"] = p_sleep_millis;
	}
	if (p_sleep_frames > 0) {
		request_data["sleepFrames"] = p_sleep_frames;
	}
	send_request("Sleep", request_data);
}

// Config Requests

void OBSClient::get_persistent_data(const String &p_realm, const String &p_slot_name, const Callable &p_callback) {
	Dictionary request_data;
	request_data["realm"] = p_realm;
	request_data["slotName"] = p_slot_name;
	send_request("GetPersistentData", request_data, p_callback);
}

void OBSClient::set_persistent_data(const String &p_realm, const String &p_slot_name, const Variant &p_slot_value) {
	Dictionary request_data;
	request_data["realm"] = p_realm;
	request_data["slotName"] = p_slot_name;
	request_data["slotValue"] = p_slot_value;
	send_request("SetPersistentData", request_data);
}

void OBSClient::get_scene_collection_list(const Callable &p_callback) {
	send_request("GetSceneCollectionList", Dictionary(), p_callback);
}

void OBSClient::set_current_scene_collection(const String &p_scene_collection_name) {
	Dictionary request_data;
	request_data["sceneCollectionName"] = p_scene_collection_name;
	send_request("SetCurrentSceneCollection", request_data);
}

void OBSClient::create_scene_collection(const String &p_scene_collection_name) {
	Dictionary request_data;
	request_data["sceneCollectionName"] = p_scene_collection_name;
	send_request("CreateSceneCollection", request_data);
}

void OBSClient::get_profile_list(const Callable &p_callback) {
	send_request("GetProfileList", Dictionary(), p_callback);
}

void OBSClient::set_current_profile(const String &p_profile_name) {
	Dictionary request_data;
	request_data["profileName"] = p_profile_name;
	send_request("SetCurrentProfile", request_data);
}

void OBSClient::create_profile(const String &p_profile_name) {
	Dictionary request_data;
	request_data["profileName"] = p_profile_name;
	send_request("CreateProfile", request_data);
}

void OBSClient::remove_profile(const String &p_profile_name) {
	Dictionary request_data;
	request_data["profileName"] = p_profile_name;
	send_request("RemoveProfile", request_data);
}

void OBSClient::get_profile_parameter(const String &p_parameter_category, const String &p_parameter_name, const Callable &p_callback) {
	Dictionary request_data;
	request_data["parameterCategory"] = p_parameter_category;
	request_data["parameterName"] = p_parameter_name;
	send_request("GetProfileParameter", request_data, p_callback);
}

void OBSClient::set_profile_parameter(const String &p_parameter_category, const String &p_parameter_name, const String &p_parameter_value) {
	Dictionary request_data;
	request_data["parameterCategory"] = p_parameter_category;
	request_data["parameterName"] = p_parameter_name;
	request_data["parameterValue"] = p_parameter_value;
	send_request("SetProfileParameter", request_data);
}

void OBSClient::get_video_settings(const Callable &p_callback) {
	send_request("GetVideoSettings", Dictionary(), p_callback);
}

void OBSClient::set_video_settings(const Dictionary &p_video_settings) {
	send_request("SetVideoSettings", p_video_settings);
}

void OBSClient::get_stream_service_settings(const Callable &p_callback) {
	send_request("GetStreamServiceSettings", Dictionary(), p_callback);
}

void OBSClient::set_stream_service_settings(const String &p_stream_service_type, const Dictionary &p_stream_service_settings) {
	Dictionary request_data;
	request_data["streamServiceType"] = p_stream_service_type;
	request_data["streamServiceSettings"] = p_stream_service_settings;
	send_request("SetStreamServiceSettings", request_data);
}

void OBSClient::get_record_directory(const Callable &p_callback) {
	send_request("GetRecordDirectory", Dictionary(), p_callback);
}

void OBSClient::set_record_directory(const String &p_record_directory) {
	Dictionary request_data;
	request_data["recordDirectory"] = p_record_directory;
	send_request("SetRecordDirectory", request_data);
}

// Scenes Requests

void OBSClient::get_scene_list(const Callable &p_callback) {
	send_request("GetSceneList", Dictionary(), p_callback);
}

void OBSClient::get_group_list(const Callable &p_callback) {
	send_request("GetGroupList", Dictionary(), p_callback);
}

void OBSClient::get_current_program_scene(const Callable &p_callback) {
	send_request("GetCurrentProgramScene", Dictionary(), p_callback);
}

void OBSClient::set_current_program_scene(const String &p_scene_name) {
	Dictionary request_data;
	request_data["sceneName"] = p_scene_name;
	send_request("SetCurrentProgramScene", request_data);
}

void OBSClient::get_current_preview_scene(const Callable &p_callback) {
	send_request("GetCurrentPreviewScene", Dictionary(), p_callback);
}

void OBSClient::set_current_preview_scene(const String &p_scene_name) {
	Dictionary request_data;
	request_data["sceneName"] = p_scene_name;
	send_request("SetCurrentPreviewScene", request_data);
}

void OBSClient::create_scene(const String &p_scene_name, const Callable &p_callback) {
	Dictionary request_data;
	request_data["sceneName"] = p_scene_name;
	send_request("CreateScene", request_data, p_callback);
}

void OBSClient::remove_scene(const String &p_scene_name) {
	Dictionary request_data;
	request_data["sceneName"] = p_scene_name;
	send_request("RemoveScene", request_data);
}

void OBSClient::set_scene_name(const String &p_scene_name, const String &p_new_scene_name) {
	Dictionary request_data;
	request_data["sceneName"] = p_scene_name;
	request_data["newSceneName"] = p_new_scene_name;
	send_request("SetSceneName", request_data);
}

void OBSClient::get_scene_scene_transition_override(const String &p_scene_name, const Callable &p_callback) {
	Dictionary request_data;
	request_data["sceneName"] = p_scene_name;
	send_request("GetSceneSceneTransitionOverride", request_data, p_callback);
}

void OBSClient::set_scene_scene_transition_override(const String &p_scene_name, const String &p_transition_name, int p_transition_duration) {
	Dictionary request_data;
	request_data["sceneName"] = p_scene_name;
	if (!p_transition_name.is_empty()) {
		request_data["transitionName"] = p_transition_name;
	}
	if (p_transition_duration >= 0) {
		request_data["transitionDuration"] = p_transition_duration;
	}
	send_request("SetSceneSceneTransitionOverride", request_data);
}

// Stream Requests

void OBSClient::get_stream_status(const Callable &p_callback) {
	send_request("GetStreamStatus", Dictionary(), p_callback);
}

void OBSClient::toggle_stream(const Callable &p_callback) {
	send_request("ToggleStream", Dictionary(), p_callback);
}

void OBSClient::start_stream() {
	send_request("StartStream", Dictionary());
}

void OBSClient::stop_stream() {
	send_request("StopStream", Dictionary());
}

void OBSClient::send_stream_caption(const String &p_caption_text) {
	Dictionary request_data;
	request_data["captionText"] = p_caption_text;
	send_request("SendStreamCaption", request_data);
}

// Record Requests

void OBSClient::get_record_status(const Callable &p_callback) {
	send_request("GetRecordStatus", Dictionary(), p_callback);
}

void OBSClient::toggle_record(const Callable &p_callback) {
	send_request("ToggleRecord", Dictionary(), p_callback);
}

void OBSClient::start_record() {
	send_request("StartRecord", Dictionary());
}

void OBSClient::stop_record(const Callable &p_callback) {
	send_request("StopRecord", Dictionary(), p_callback);
}

void OBSClient::toggle_record_pause() {
	send_request("ToggleRecordPause", Dictionary());
}

void OBSClient::pause_record() {
	send_request("PauseRecord", Dictionary());
}

void OBSClient::resume_record() {
	send_request("ResumeRecord", Dictionary());
}

void OBSClient::split_record_file() {
	send_request("SplitRecordFile", Dictionary());
}

void OBSClient::create_record_chapter(const String &p_chapter_name) {
	Dictionary request_data;
	if (!p_chapter_name.is_empty()) {
		request_data["chapterName"] = p_chapter_name;
	}
	send_request("CreateRecordChapter", request_data);
}

// Output Requests

void OBSClient::get_virtual_cam_status(const Callable &p_callback) {
	send_request("GetVirtualCamStatus", Dictionary(), p_callback);
}

void OBSClient::toggle_virtual_cam(const Callable &p_callback) {
	send_request("ToggleVirtualCam", Dictionary(), p_callback);
}

void OBSClient::start_virtual_cam() {
	send_request("StartVirtualCam", Dictionary());
}

void OBSClient::stop_virtual_cam() {
	send_request("StopVirtualCam", Dictionary());
}

void OBSClient::get_replay_buffer_status(const Callable &p_callback) {
	send_request("GetReplayBufferStatus", Dictionary(), p_callback);
}

void OBSClient::toggle_replay_buffer(const Callable &p_callback) {
	send_request("ToggleReplayBuffer", Dictionary(), p_callback);
}

void OBSClient::start_replay_buffer() {
	send_request("StartReplayBuffer", Dictionary());
}

void OBSClient::stop_replay_buffer() {
	send_request("StopReplayBuffer", Dictionary());
}

void OBSClient::save_replay_buffer() {
	send_request("SaveReplayBuffer", Dictionary());
}

void OBSClient::get_last_replay_buffer_replay(const Callable &p_callback) {
	send_request("GetLastReplayBufferReplay", Dictionary(), p_callback);
}

// Inputs Requests

void OBSClient::get_input_list(const String &p_input_kind, const Callable &p_callback) {
	Dictionary request_data;
	if (!p_input_kind.is_empty()) {
		request_data["inputKind"] = p_input_kind;
	}
	send_request("GetInputList", request_data, p_callback);
}

void OBSClient::get_input_kind_list(bool p_unversioned, const Callable &p_callback) {
	Dictionary request_data;
	request_data["unversioned"] = p_unversioned;
	send_request("GetInputKindList", request_data, p_callback);
}

void OBSClient::get_special_inputs(const Callable &p_callback) {
	send_request("GetSpecialInputs", Dictionary(), p_callback);
}

void OBSClient::create_input(const String &p_scene_name, const String &p_input_name, const String &p_input_kind, const Dictionary &p_input_settings, bool p_scene_item_enabled, const Callable &p_callback) {
	Dictionary request_data;
	request_data["sceneName"] = p_scene_name;
	request_data["inputName"] = p_input_name;
	request_data["inputKind"] = p_input_kind;
	if (!p_input_settings.is_empty()) {
		request_data["inputSettings"] = p_input_settings;
	}
	request_data["sceneItemEnabled"] = p_scene_item_enabled;
	send_request("CreateInput", request_data, p_callback);
}

void OBSClient::remove_input(const String &p_input_name) {
	Dictionary request_data;
	request_data["inputName"] = p_input_name;
	send_request("RemoveInput", request_data);
}

void OBSClient::set_input_name(const String &p_input_name, const String &p_new_input_name) {
	Dictionary request_data;
	request_data["inputName"] = p_input_name;
	request_data["newInputName"] = p_new_input_name;
	send_request("SetInputName", request_data);
}

void OBSClient::get_input_default_settings(const String &p_input_kind, const Callable &p_callback) {
	Dictionary request_data;
	request_data["inputKind"] = p_input_kind;
	send_request("GetInputDefaultSettings", request_data, p_callback);
}

void OBSClient::get_input_settings(const String &p_input_name, const Callable &p_callback) {
	Dictionary request_data;
	request_data["inputName"] = p_input_name;
	send_request("GetInputSettings", request_data, p_callback);
}

void OBSClient::set_input_settings(const String &p_input_name, const Dictionary &p_input_settings, bool p_overlay) {
	Dictionary request_data;
	request_data["inputName"] = p_input_name;
	request_data["inputSettings"] = p_input_settings;
	request_data["overlay"] = p_overlay;
	send_request("SetInputSettings", request_data);
}

void OBSClient::get_input_mute(const String &p_input_name, const Callable &p_callback) {
	Dictionary request_data;
	request_data["inputName"] = p_input_name;
	send_request("GetInputMute", request_data, p_callback);
}

void OBSClient::set_input_mute(const String &p_input_name, bool p_input_muted) {
	Dictionary request_data;
	request_data["inputName"] = p_input_name;
	request_data["inputMuted"] = p_input_muted;
	send_request("SetInputMute", request_data);
}

void OBSClient::toggle_input_mute(const String &p_input_name, const Callable &p_callback) {
	Dictionary request_data;
	request_data["inputName"] = p_input_name;
	send_request("ToggleInputMute", request_data, p_callback);
}

void OBSClient::get_input_volume(const String &p_input_name, const Callable &p_callback) {
	Dictionary request_data;
	request_data["inputName"] = p_input_name;
	send_request("GetInputVolume", request_data, p_callback);
}

void OBSClient::set_input_volume(const String &p_input_name, float p_input_volume_mul, float p_input_volume_db) {
	Dictionary request_data;
	request_data["inputName"] = p_input_name;
	if (p_input_volume_mul >= 0.0) {
		request_data["inputVolumeMul"] = p_input_volume_mul;
	}
	if (p_input_volume_db != 0.0 || p_input_volume_mul < 0.0) {
		request_data["inputVolumeDb"] = p_input_volume_db;
	}
	send_request("SetInputVolume", request_data);
}

// Transitions Requests

void OBSClient::get_transition_kind_list(const Callable &p_callback) {
	send_request("GetTransitionKindList", Dictionary(), p_callback);
}

void OBSClient::get_scene_transition_list(const Callable &p_callback) {
	send_request("GetSceneTransitionList", Dictionary(), p_callback);
}

void OBSClient::get_current_scene_transition(const Callable &p_callback) {
	send_request("GetCurrentSceneTransition", Dictionary(), p_callback);
}

void OBSClient::set_current_scene_transition(const String &p_transition_name) {
	Dictionary request_data;
	request_data["transitionName"] = p_transition_name;
	send_request("SetCurrentSceneTransition", request_data);
}

void OBSClient::set_current_scene_transition_duration(int p_transition_duration) {
	Dictionary request_data;
	request_data["transitionDuration"] = p_transition_duration;
	send_request("SetCurrentSceneTransitionDuration", request_data);
}

void OBSClient::set_current_scene_transition_settings(const Dictionary &p_transition_settings, bool p_overlay) {
	Dictionary request_data;
	request_data["transitionSettings"] = p_transition_settings;
	request_data["overlay"] = p_overlay;
	send_request("SetCurrentSceneTransitionSettings", request_data);
}

void OBSClient::trigger_studio_mode_transition() {
	send_request("TriggerStudioModeTransition", Dictionary());
}

void OBSClient::set_tbar_position(float p_position, bool p_release) {
	Dictionary request_data;
	request_data["position"] = p_position;
	request_data["release"] = p_release;
	send_request("SetTBarPosition", request_data);
}

// Filters Requests

void OBSClient::get_source_filter_kind_list(const Callable &p_callback) {
	send_request("GetSourceFilterKindList", Dictionary(), p_callback);
}

void OBSClient::get_source_filter_list(const String &p_source_name, const Callable &p_callback) {
	Dictionary request_data;
	request_data["sourceName"] = p_source_name;
	send_request("GetSourceFilterList", request_data, p_callback);
}

void OBSClient::get_source_filter_default_settings(const String &p_filter_kind, const Callable &p_callback) {
	Dictionary request_data;
	request_data["filterKind"] = p_filter_kind;
	send_request("GetSourceFilterDefaultSettings", request_data, p_callback);
}

void OBSClient::create_source_filter(const String &p_source_name, const String &p_filter_name, const String &p_filter_kind, const Dictionary &p_filter_settings) {
	Dictionary request_data;
	request_data["sourceName"] = p_source_name;
	request_data["filterName"] = p_filter_name;
	request_data["filterKind"] = p_filter_kind;
	if (!p_filter_settings.is_empty()) {
		request_data["filterSettings"] = p_filter_settings;
	}
	send_request("CreateSourceFilter", request_data);
}

void OBSClient::remove_source_filter(const String &p_source_name, const String &p_filter_name) {
	Dictionary request_data;
	request_data["sourceName"] = p_source_name;
	request_data["filterName"] = p_filter_name;
	send_request("RemoveSourceFilter", request_data);
}

void OBSClient::set_source_filter_name(const String &p_source_name, const String &p_filter_name, const String &p_new_filter_name) {
	Dictionary request_data;
	request_data["sourceName"] = p_source_name;
	request_data["filterName"] = p_filter_name;
	request_data["newFilterName"] = p_new_filter_name;
	send_request("SetSourceFilterName", request_data);
}

void OBSClient::get_source_filter(const String &p_source_name, const String &p_filter_name, const Callable &p_callback) {
	Dictionary request_data;
	request_data["sourceName"] = p_source_name;
	request_data["filterName"] = p_filter_name;
	send_request("GetSourceFilter", request_data, p_callback);
}

void OBSClient::set_source_filter_index(const String &p_source_name, const String &p_filter_name, int p_filter_index) {
	Dictionary request_data;
	request_data["sourceName"] = p_source_name;
	request_data["filterName"] = p_filter_name;
	request_data["filterIndex"] = p_filter_index;
	send_request("SetSourceFilterIndex", request_data);
}

void OBSClient::set_source_filter_settings(const String &p_source_name, const String &p_filter_name, const Dictionary &p_filter_settings, bool p_overlay) {
	Dictionary request_data;
	request_data["sourceName"] = p_source_name;
	request_data["filterName"] = p_filter_name;
	request_data["filterSettings"] = p_filter_settings;
	request_data["overlay"] = p_overlay;
	send_request("SetSourceFilterSettings", request_data);
}

void OBSClient::set_source_filter_enabled(const String &p_source_name, const String &p_filter_name, bool p_filter_enabled) {
	Dictionary request_data;
	request_data["sourceName"] = p_source_name;
	request_data["filterName"] = p_filter_name;
	request_data["filterEnabled"] = p_filter_enabled;
	send_request("SetSourceFilterEnabled", request_data);
}

// Scene Items Requests

void OBSClient::get_scene_item_list(const String &p_scene_name, const Callable &p_callback) {
	Dictionary request_data;
	request_data["sceneName"] = p_scene_name;
	send_request("GetSceneItemList", request_data, p_callback);
}

void OBSClient::get_group_scene_item_list(const String &p_scene_name, const Callable &p_callback) {
	Dictionary request_data;
	request_data["sceneName"] = p_scene_name;
	send_request("GetGroupSceneItemList", request_data, p_callback);
}

void OBSClient::get_scene_item_id(const String &p_scene_name, const String &p_source_name, int p_search_offset, const Callable &p_callback) {
	Dictionary request_data;
	request_data["sceneName"] = p_scene_name;
	request_data["sourceName"] = p_source_name;
	if (p_search_offset != 0) {
		request_data["searchOffset"] = p_search_offset;
	}
	send_request("GetSceneItemId", request_data, p_callback);
}

void OBSClient::create_scene_item(const String &p_scene_name, const String &p_source_name, bool p_scene_item_enabled, const Callable &p_callback) {
	Dictionary request_data;
	request_data["sceneName"] = p_scene_name;
	request_data["sourceName"] = p_source_name;
	request_data["sceneItemEnabled"] = p_scene_item_enabled;
	send_request("CreateSceneItem", request_data, p_callback);
}

void OBSClient::remove_scene_item(const String &p_scene_name, int p_scene_item_id) {
	Dictionary request_data;
	request_data["sceneName"] = p_scene_name;
	request_data["sceneItemId"] = p_scene_item_id;
	send_request("RemoveSceneItem", request_data);
}

void OBSClient::duplicate_scene_item(const String &p_scene_name, int p_scene_item_id, const String &p_destination_scene_name, const Callable &p_callback) {
	Dictionary request_data;
	request_data["sceneName"] = p_scene_name;
	request_data["sceneItemId"] = p_scene_item_id;
	if (!p_destination_scene_name.is_empty()) {
		request_data["destinationSceneName"] = p_destination_scene_name;
	}
	send_request("DuplicateSceneItem", request_data, p_callback);
}

void OBSClient::get_scene_item_transform(const String &p_scene_name, int p_scene_item_id, const Callable &p_callback) {
	Dictionary request_data;
	request_data["sceneName"] = p_scene_name;
	request_data["sceneItemId"] = p_scene_item_id;
	send_request("GetSceneItemTransform", request_data, p_callback);
}

void OBSClient::set_scene_item_transform(const String &p_scene_name, int p_scene_item_id, const Dictionary &p_scene_item_transform) {
	Dictionary request_data;
	request_data["sceneName"] = p_scene_name;
	request_data["sceneItemId"] = p_scene_item_id;
	request_data["sceneItemTransform"] = p_scene_item_transform;
	send_request("SetSceneItemTransform", request_data);
}

void OBSClient::get_scene_item_enabled(const String &p_scene_name, int p_scene_item_id, const Callable &p_callback) {
	Dictionary request_data;
	request_data["sceneName"] = p_scene_name;
	request_data["sceneItemId"] = p_scene_item_id;
	send_request("GetSceneItemEnabled", request_data, p_callback);
}

void OBSClient::set_scene_item_enabled(const String &p_scene_name, int p_scene_item_id, bool p_scene_item_enabled) {
	Dictionary request_data;
	request_data["sceneName"] = p_scene_name;
	request_data["sceneItemId"] = p_scene_item_id;
	request_data["sceneItemEnabled"] = p_scene_item_enabled;
	send_request("SetSceneItemEnabled", request_data);
}

void OBSClient::get_scene_item_locked(const String &p_scene_name, int p_scene_item_id, const Callable &p_callback) {
	Dictionary request_data;
	request_data["sceneName"] = p_scene_name;
	request_data["sceneItemId"] = p_scene_item_id;
	send_request("GetSceneItemLocked", request_data, p_callback);
}

void OBSClient::set_scene_item_locked(const String &p_scene_name, int p_scene_item_id, bool p_scene_item_locked) {
	Dictionary request_data;
	request_data["sceneName"] = p_scene_name;
	request_data["sceneItemId"] = p_scene_item_id;
	request_data["sceneItemLocked"] = p_scene_item_locked;
	send_request("SetSceneItemLocked", request_data);
}

void OBSClient::get_scene_item_index(const String &p_scene_name, int p_scene_item_id, const Callable &p_callback) {
	Dictionary request_data;
	request_data["sceneName"] = p_scene_name;
	request_data["sceneItemId"] = p_scene_item_id;
	send_request("GetSceneItemIndex", request_data, p_callback);
}

void OBSClient::set_scene_item_index(const String &p_scene_name, int p_scene_item_id, int p_scene_item_index) {
	Dictionary request_data;
	request_data["sceneName"] = p_scene_name;
	request_data["sceneItemId"] = p_scene_item_id;
	request_data["sceneItemIndex"] = p_scene_item_index;
	send_request("SetSceneItemIndex", request_data);
}

void OBSClient::get_scene_item_blend_mode(const String &p_scene_name, int p_scene_item_id, const Callable &p_callback) {
	Dictionary request_data;
	request_data["sceneName"] = p_scene_name;
	request_data["sceneItemId"] = p_scene_item_id;
	send_request("GetSceneItemBlendMode", request_data, p_callback);
}

void OBSClient::set_scene_item_blend_mode(const String &p_scene_name, int p_scene_item_id, const String &p_scene_item_blend_mode) {
	Dictionary request_data;
	request_data["sceneName"] = p_scene_name;
	request_data["sceneItemId"] = p_scene_item_id;
	request_data["sceneItemBlendMode"] = p_scene_item_blend_mode;
	send_request("SetSceneItemBlendMode", request_data);
}

// Media Inputs Requests

void OBSClient::get_media_input_status(const String &p_input_name, const Callable &p_callback) {
	Dictionary request_data;
	request_data["inputName"] = p_input_name;
	send_request("GetMediaInputStatus", request_data, p_callback);
}

void OBSClient::set_media_input_cursor(const String &p_input_name, int p_media_cursor) {
	Dictionary request_data;
	request_data["inputName"] = p_input_name;
	request_data["mediaCursor"] = p_media_cursor;
	send_request("SetMediaInputCursor", request_data);
}

void OBSClient::offset_media_input_cursor(const String &p_input_name, int p_media_cursor_offset) {
	Dictionary request_data;
	request_data["inputName"] = p_input_name;
	request_data["mediaCursorOffset"] = p_media_cursor_offset;
	send_request("OffsetMediaInputCursor", request_data);
}

void OBSClient::trigger_media_input_action(const String &p_input_name, const String &p_media_action) {
	Dictionary request_data;
	request_data["inputName"] = p_input_name;
	request_data["mediaAction"] = p_media_action;
	send_request("TriggerMediaInputAction", request_data);
}

// Sources Requests

void OBSClient::get_source_active(const String &p_source_name, const Callable &p_callback) {
	Dictionary request_data;
	request_data["sourceName"] = p_source_name;
	send_request("GetSourceActive", request_data, p_callback);
}

void OBSClient::get_source_screenshot(const String &p_source_name, const String &p_image_format, int p_image_width, int p_image_height, int p_image_compression_quality, const Callable &p_callback) {
	Dictionary request_data;
	request_data["sourceName"] = p_source_name;
	request_data["imageFormat"] = p_image_format;
	if (p_image_width >= 0) {
		request_data["imageWidth"] = p_image_width;
	}
	if (p_image_height >= 0) {
		request_data["imageHeight"] = p_image_height;
	}
	if (p_image_compression_quality >= 0) {
		request_data["imageCompressionQuality"] = p_image_compression_quality;
	}
	send_request("GetSourceScreenshot", request_data, p_callback);
}

void OBSClient::save_source_screenshot(const String &p_source_name, const String &p_image_format, const String &p_image_file_path, int p_image_width, int p_image_height, int p_image_compression_quality) {
	Dictionary request_data;
	request_data["sourceName"] = p_source_name;
	request_data["imageFormat"] = p_image_format;
	request_data["imageFilePath"] = p_image_file_path;
	if (p_image_width >= 0) {
		request_data["imageWidth"] = p_image_width;
	}
	if (p_image_height >= 0) {
		request_data["imageHeight"] = p_image_height;
	}
	if (p_image_compression_quality >= 0) {
		request_data["imageCompressionQuality"] = p_image_compression_quality;
	}
	send_request("SaveSourceScreenshot", request_data);
}

// UI Requests

void OBSClient::get_studio_mode_enabled(const Callable &p_callback) {
	send_request("GetStudioModeEnabled", Dictionary(), p_callback);
}

void OBSClient::set_studio_mode_enabled(bool p_studio_mode_enabled) {
	Dictionary request_data;
	request_data["studioModeEnabled"] = p_studio_mode_enabled;
	send_request("SetStudioModeEnabled", request_data);
}

void OBSClient::open_input_properties_dialog(const String &p_input_name) {
	Dictionary request_data;
	request_data["inputName"] = p_input_name;
	send_request("OpenInputPropertiesDialog", request_data);
}

void OBSClient::open_input_filters_dialog(const String &p_input_name) {
	Dictionary request_data;
	request_data["inputName"] = p_input_name;
	send_request("OpenInputFiltersDialog", request_data);
}

void OBSClient::open_input_interact_dialog(const String &p_input_name) {
	Dictionary request_data;
	request_data["inputName"] = p_input_name;
	send_request("OpenInputInteractDialog", request_data);
}

void OBSClient::get_monitor_list(const Callable &p_callback) {
	send_request("GetMonitorList", Dictionary(), p_callback);
}

void OBSClient::open_video_mix_projector(const String &p_video_mix_type, int p_monitor_index, const String &p_projector_geometry) {
	Dictionary request_data;
	request_data["videoMixType"] = p_video_mix_type;
	if (p_monitor_index >= 0) {
		request_data["monitorIndex"] = p_monitor_index;
	}
	if (!p_projector_geometry.is_empty()) {
		request_data["projectorGeometry"] = p_projector_geometry;
	}
	send_request("OpenVideoMixProjector", request_data);
}

void OBSClient::open_source_projector(const String &p_source_name, int p_monitor_index, const String &p_projector_geometry) {
	Dictionary request_data;
	request_data["sourceName"] = p_source_name;
	if (p_monitor_index >= 0) {
		request_data["monitorIndex"] = p_monitor_index;
	}
	if (!p_projector_geometry.is_empty()) {
		request_data["projectorGeometry"] = p_projector_geometry;
	}
	send_request("OpenSourceProjector", request_data);
}
