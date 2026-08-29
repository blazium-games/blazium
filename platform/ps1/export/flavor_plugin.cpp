/**************************************************************************/
/*  flavor_plugin.cpp                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* SPDX-License-Identifier: MIT                                           */
/**************************************************************************/

#include "flavor_plugin.h"

#include <cstring>

#include "cook/cooker.h"
#include "cook/script_cook.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/templates/hash_set.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "scene/2d/sprite_2d.h"
#include "scene/2d/tile_map.h"
#include "scene/2d/tile_map_layer.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/gui/control.h"
#include "scene/resources/mesh.h"
#include "servers/display_server.h"

#include <cstdio>
#include <ctime>

void PS1::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load_scene", "path"), &PS1::load_scene);
	ClassDB::bind_method(D_METHOD("unload_scene", "path"), &PS1::unload_scene);
	ClassDB::bind_method(D_METHOD("is_scene_loaded", "path"), &PS1::is_scene_loaded);
	ClassDB::bind_method(D_METHOD("get_loaded_scene_count"), &PS1::get_loaded_scene_count);
	ClassDB::bind_method(D_METHOD("get_loaded_scene", "index"), &PS1::get_loaded_scene);
	ClassDB::bind_method(D_METHOD("get_static_memory_usage"), &PS1::get_static_memory_usage);
	ClassDB::bind_method(D_METHOD("get_static_memory_peak_usage"), &PS1::get_static_memory_peak_usage);
	ClassDB::bind_method(D_METHOD("get_ram_budget"), &PS1::get_ram_budget);
	ClassDB::bind_method(D_METHOD("get_free_ram"), &PS1::get_free_ram);
	ClassDB::bind_method(D_METHOD("can_load_scene", "path"), &PS1::can_load_scene);
	ClassDB::bind_method(D_METHOD("can_instantiate", "path_or_pack"), &PS1::can_instantiate);
	ClassDB::bind_method(D_METHOD("get_node_count"), &PS1::get_node_count);
	ClassDB::bind_method(D_METHOD("get_node_budget"), &PS1::get_node_budget);
	ClassDB::bind_method(D_METHOD("get_triangle_count"), &PS1::get_triangle_count);
	ClassDB::bind_method(D_METHOD("get_triangle_budget"), &PS1::get_triangle_budget);
	ClassDB::bind_method(D_METHOD("get_free_nodes"), &PS1::get_free_nodes);
	ClassDB::bind_method(D_METHOD("get_free_hud"), &PS1::get_free_hud);
	ClassDB::bind_method(D_METHOD("get_free_tiles"), &PS1::get_free_tiles);
	ClassDB::bind_method(D_METHOD("set_camera", "index"), &PS1::set_camera);
	ClassDB::bind_method(D_METHOD("set_camera_node", "name"), &PS1::set_camera_node);
	ClassDB::bind_method(D_METHOD("set_default_camera", "index_or_name"), &PS1::set_default_camera);
	ClassDB::bind_method(D_METHOD("get_camera"), &PS1::get_camera);
	ClassDB::bind_method(D_METHOD("get_camera_count"), &PS1::get_camera_count);
	ClassDB::bind_method(D_METHOD("get_camera_name", "index"), &PS1::get_camera_name);
	ClassDB::bind_method(D_METHOD("is_camera_current", "index"), &PS1::is_camera_current);
	ClassDB::bind_method(D_METHOD("next_camera"), &PS1::next_camera);
	ClassDB::bind_method(D_METHOD("prev_camera"), &PS1::prev_camera);
	ClassDB::bind_method(D_METHOD("overlaps", "a", "b"), &PS1::overlaps);
	ClassDB::bind_method(D_METHOD("overlaps_entered", "a", "b"), &PS1::overlaps_entered);
	ClassDB::bind_method(D_METHOD("has_overlapping_areas"), &PS1::has_overlapping_areas);
	ClassDB::bind_method(D_METHOD("get_overlapping_area_count"), &PS1::get_overlapping_area_count);
	ClassDB::bind_method(D_METHOD("get_overlapping_area", "index"), &PS1::get_overlapping_area);
	ClassDB::bind_method(D_METHOD("intersects_ray", "from", "dir"), &PS1::intersects_ray);
	ClassDB::bind_method(D_METHOD("hitbox_kind", "node"), &PS1::hitbox_kind);
	ClassDB::bind_method(D_METHOD("set_hitbox_enabled", "node", "on"), &PS1::set_hitbox_enabled);
	ClassDB::bind_method(D_METHOD("load_animations", "path"), &PS1::load_animations);
	ClassDB::bind_method(D_METHOD("unload_animations", "path"), &PS1::unload_animations);
	ClassDB::bind_method(D_METHOD("can_load_animations", "path"), &PS1::can_load_animations);
	ClassDB::bind_method(D_METHOD("is_animations_loaded", "path"), &PS1::is_animations_loaded);
	ClassDB::bind_method(D_METHOD("load_sprites", "path"), &PS1::load_sprites);
	ClassDB::bind_method(D_METHOD("unload_sprites", "path"), &PS1::unload_sprites);
	ClassDB::bind_method(D_METHOD("can_load_sprites", "path"), &PS1::can_load_sprites);
	ClassDB::bind_method(D_METHOD("is_sprites_loaded", "path"), &PS1::is_sprites_loaded);
	ClassDB::bind_method(D_METHOD("load_hitboxes", "path"), &PS1::load_hitboxes);
	ClassDB::bind_method(D_METHOD("unload_hitboxes", "path"), &PS1::unload_hitboxes);
	ClassDB::bind_method(D_METHOD("can_load_hitboxes", "path"), &PS1::can_load_hitboxes);
	ClassDB::bind_method(D_METHOD("is_hitboxes_loaded", "path"), &PS1::is_hitboxes_loaded);
	ClassDB::bind_method(D_METHOD("load_cameras", "path"), &PS1::load_cameras);
	ClassDB::bind_method(D_METHOD("unload_cameras", "path"), &PS1::unload_cameras);
	ClassDB::bind_method(D_METHOD("is_cameras_loaded", "path"), &PS1::is_cameras_loaded);
	ClassDB::bind_method(D_METHOD("can_load_cameras", "path"), &PS1::can_load_cameras);
	ClassDB::bind_method(D_METHOD("add_to_group", "node", "name"), &PS1::add_to_group);
	ClassDB::bind_method(D_METHOD("remove_from_group", "node", "name"), &PS1::remove_from_group);
	ClassDB::bind_method(D_METHOD("is_in_group", "node", "name"), &PS1::is_in_group);
	ClassDB::bind_method(D_METHOD("count_in_group", "name"), &PS1::count_in_group);
	ClassDB::bind_method(D_METHOD("get_in_group", "name", "index"), &PS1::get_in_group);
	ClassDB::bind_method(D_METHOD("get_first_in_group", "name"), &PS1::get_first_in_group);
	ClassDB::bind_method(D_METHOD("find_nearest", "from", "group"), &PS1::find_nearest);
	ClassDB::bind_method(D_METHOD("set_billboard", "node", "on"), &PS1::set_billboard);
	ClassDB::bind_method(D_METHOD("get_billboard", "node"), &PS1::get_billboard);
	ClassDB::bind_method(D_METHOD("get_floor_node"), &PS1::get_floor_node);
	ClassDB::bind_method(D_METHOD("get_floor_normal"), &PS1::get_floor_normal);
	ClassDB::bind_method(D_METHOD("make_current", "node"), &PS1::make_current);
	ClassDB::bind_method(D_METHOD("is_action_pressed_on", "device", "action"), &PS1::is_action_pressed_on);
	ClassDB::bind_method(D_METHOD("is_action_just_pressed_on", "device", "action"), &PS1::is_action_just_pressed_on);
	ClassDB::bind_method(D_METHOD("is_action_just_released_on", "device", "action"), &PS1::is_action_just_released_on);
	ClassDB::bind_method(D_METHOD("memcard_present", "slot"), &PS1::memcard_present);
	ClassDB::bind_method(D_METHOD("memcard_ready", "slot"), &PS1::memcard_ready);
	ClassDB::bind_method(D_METHOD("memcard_format", "slot"), &PS1::memcard_format);
	ClassDB::bind_method(D_METHOD("memcard_exists", "slot", "name"), &PS1::memcard_exists);
	ClassDB::bind_method(D_METHOD("memcard_delete", "slot", "name"), &PS1::memcard_delete);
	ClassDB::bind_method(D_METHOD("memcard_save", "slot", "name", "bytes"), &PS1::memcard_save, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("memcard_load", "slot", "name"), &PS1::memcard_load);
	ClassDB::bind_method(D_METHOD("memcard_count", "slot"), &PS1::memcard_count);
	ClassDB::bind_method(D_METHOD("memcard_name", "slot", "index"), &PS1::memcard_name);
	ClassDB::bind_method(D_METHOD("memcard_wrap", "payload", "title"), &PS1::memcard_wrap, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("memcard_unwrap", "framed"), &PS1::memcard_unwrap);
	ClassDB::bind_method(D_METHOD("get_stick", "device", "stick"), &PS1::get_stick);
	ClassDB::bind_method(D_METHOD("set_rumble", "device", "small", "large"), &PS1::set_rumble);
	ClassDB::bind_method(D_METHOD("stop_rumble", "device"), &PS1::stop_rumble);
	ClassDB::bind_method(D_METHOD("set_camera_transform", "pos", "rot"), &PS1::set_camera_transform);
	ClassDB::bind_method(D_METHOD("get_camera_transform"), &PS1::get_camera_transform);
	ClassDB::bind_method(D_METHOD("get_camera_rotation"), &PS1::get_camera_rotation);
	ClassDB::bind_method(D_METHOD("attach_camera", "node", "offset"), &PS1::attach_camera, DEFVAL(Vector3()));
	ClassDB::bind_method(D_METHOD("look_camera", "yaw", "pitch", "roll"), &PS1::look_camera, DEFVAL(0.0f));
	ClassDB::bind_method(D_METHOD("orbit_camera", "target", "yaw", "pitch", "dist", "clip"), &PS1::orbit_camera, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("set_camera_scale", "h"), &PS1::set_camera_scale);
	ClassDB::bind_method(D_METHOD("raycast", "from", "dir", "dist", "mask"), &PS1::raycast, DEFVAL(255));
	ClassDB::bind_method(D_METHOD("raycast_point", "from", "dir", "dist", "mask"), &PS1::raycast_point, DEFVAL(255));
	ClassDB::bind_method(D_METHOD("move_and_slide", "node", "velocity", "mask"), &PS1::move_and_slide, DEFVAL(255));
	ClassDB::bind_method(D_METHOD("is_on_floor"), &PS1::is_on_floor);
	ClassDB::bind_method(D_METHOD("is_on_wall"), &PS1::is_on_wall);
	ClassDB::bind_method(D_METHOD("is_on_ceiling"), &PS1::is_on_ceiling);
	ClassDB::bind_method(D_METHOD("set_hitbox_layer", "node", "layer"), &PS1::set_hitbox_layer);
	ClassDB::bind_method(D_METHOD("get_hitbox_layer", "node"), &PS1::get_hitbox_layer);
	ClassDB::bind_method(D_METHOD("shake_camera", "amp", "ms"), &PS1::shake_camera);
	ClassDB::bind_method(D_METHOD("set_paused", "on"), &PS1::set_paused);
	ClassDB::bind_method(D_METHOD("is_paused"), &PS1::is_paused);
	ClassDB::bind_method(D_METHOD("set_music_volume", "vol"), &PS1::set_music_volume);
	ClassDB::bind_method(D_METHOD("load_text", "path"), &PS1::load_text);
	ClassDB::bind_method(D_METHOD("unload_text", "path"), &PS1::unload_text);
	ClassDB::bind_method(D_METHOD("can_load_text", "path"), &PS1::can_load_text);
	ClassDB::bind_method(D_METHOD("is_text_loaded", "path"), &PS1::is_text_loaded);
	ClassDB::bind_method(D_METHOD("set_line", "hud", "name"), &PS1::set_line);
	ClassDB::bind_method(D_METHOD("set_line_chars", "hud", "name", "n"), &PS1::set_line_chars);
	ClassDB::bind_method(D_METHOD("get_action_strength", "action"), &PS1::get_action_strength);
	ClassDB::bind_method(D_METHOD("set_stick_deadzone", "n"), &PS1::set_stick_deadzone);
	ClassDB::bind_method(D_METHOD("get_velocity", "node"), &PS1::get_velocity, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("poke", "off", "n"), &PS1::poke);
	ClassDB::bind_method(D_METHOD("peek", "off"), &PS1::peek);
	ClassDB::bind_method(D_METHOD("set_node_meta", "node", "n"), &PS1::set_node_meta);
	ClassDB::bind_method(D_METHOD("get_node_meta", "node"), &PS1::get_node_meta);
	ClassDB::bind_method(D_METHOD("set_angular_velocity", "node", "v"), &PS1::set_angular_velocity);
	ClassDB::bind_method(D_METHOD("get_angular_velocity", "node"), &PS1::get_angular_velocity);
	ClassDB::bind_method(D_METHOD("set_camera_limits", "l", "t", "r", "b"), &PS1::set_camera_limits);
	ClassDB::bind_method(D_METHOD("set_camera_drag", "n"), &PS1::set_camera_drag);
	ClassDB::bind_method(D_METHOD("move_and_drive", "node", "throttle", "steer"), &PS1::move_and_drive);
	ClassDB::bind_method(D_METHOD("get_speed", "node"), &PS1::get_speed);
	ClassDB::bind_method(D_METHOD("set_drive_friction", "node", "f"), &PS1::set_drive_friction);
	ClassDB::bind_method(D_METHOD("start_stream", "node", "rate", "tex"), &PS1::start_stream);
	ClassDB::bind_method(D_METHOD("stop_stream", "node"), &PS1::stop_stream);
	ClassDB::bind_method(D_METHOD("set_stream_vel", "node", "v"), &PS1::set_stream_vel);
	ClassDB::bind_method(D_METHOD("set_stream_spread", "node", "n"), &PS1::set_stream_spread);
	ClassDB::bind_method(D_METHOD("set_stream_mode", "node", "m"), &PS1::set_stream_mode);
	ClassDB::bind_method(D_METHOD("set_particle_mode", "m"), &PS1::set_particle_mode);
	ClassDB::bind_method(D_METHOD("set_particle_flipbook", "tex", "frames", "fps"), &PS1::set_particle_flipbook);
	ClassDB::bind_method(D_METHOD("set_particle_gravity", "g"), &PS1::set_particle_gravity);
	ClassDB::bind_method(D_METHOD("set_particle_color", "c"), &PS1::set_particle_color);
	ClassDB::bind_method(D_METHOD("emit_mesh", "node", "n", "tex"), &PS1::emit_mesh);
	ClassDB::bind_method(D_METHOD("world_to_screen", "pos"), &PS1::world_to_screen);
	ClassDB::bind_method(D_METHOD("seek_animation", "sec"), &PS1::seek_animation);
	ClassDB::bind_method(D_METHOD("set_animation_speed", "s"), &PS1::set_animation_speed);
	ClassDB::bind_method(D_METHOD("format_int", "n", "width"), &PS1::format_int);
	ClassDB::bind_method(D_METHOD("set_hud_text", "hud", "text"), &PS1::set_hud_text);
	ClassDB::bind_method(D_METHOD("load_music", "path"), &PS1::load_music);
	ClassDB::bind_method(D_METHOD("unload_music", "path"), &PS1::unload_music);
	ClassDB::bind_method(D_METHOD("can_load_music", "path"), &PS1::can_load_music);
	ClassDB::bind_method(D_METHOD("is_music_loaded", "path"), &PS1::is_music_loaded);
	ClassDB::bind_method(D_METHOD("play_fmv", "path"), &PS1::play_fmv);
	ClassDB::bind_method(D_METHOD("play_xa", "path"), &PS1::play_xa);
	ClassDB::bind_method(D_METHOD("stop_xa"), &PS1::stop_xa);
	ClassDB::bind_method(D_METHOD("tile_solid_at", "x", "y"), &PS1::tile_solid_at);
	ClassDB::bind_method(D_METHOD("tile_at", "x", "y"), &PS1::tile_at);
	ClassDB::bind_method(D_METHOD("load_audio", "path"), &PS1::load_audio);
	ClassDB::bind_method(D_METHOD("unload_audio", "path"), &PS1::unload_audio);
	ClassDB::bind_method(D_METHOD("can_load_audio", "path"), &PS1::can_load_audio);
	ClassDB::bind_method(D_METHOD("is_audio_loaded", "path"), &PS1::is_audio_loaded);
	ClassDB::bind_method(D_METHOD("play_sfx", "name"), &PS1::play_sfx);
	ClassDB::bind_method(D_METHOD("stop_sfx", "name"), &PS1::stop_sfx);
	ClassDB::bind_method(D_METHOD("set_sfx_volume", "name", "vol"), &PS1::set_sfx_volume);
	ClassDB::bind_method(D_METHOD("emit", "pos", "count", "sprite_or_tim"), &PS1::emit);
	ClassDB::bind_method(D_METHOD("set_fog", "on", "start", "end", "color"), &PS1::set_fog);
	ClassDB::bind_method(D_METHOD("set_fade", "alpha", "color"), &PS1::set_fade, DEFVAL(Color()));
	ClassDB::bind_method(D_METHOD("set_light", "index", "dir", "color"), &PS1::set_light);
	ClassDB::bind_method(D_METHOD("get_forward", "node"), &PS1::get_forward, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("get_right", "node"), &PS1::get_right, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("get_up", "node"), &PS1::get_up, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("translate_local", "node", "v"), &PS1::translate_local);
	ClassDB::bind_method(D_METHOD("move_and_fly", "node", "throttle", "yaw", "pitch", "roll"), &PS1::move_and_fly);
	ClassDB::bind_method(D_METHOD("apply_gravity", "node"), &PS1::apply_gravity);
	ClassDB::bind_method(D_METHOD("set_gravity", "g"), &PS1::set_gravity);
	ClassDB::bind_method(D_METHOD("add_velocity", "node", "v"), &PS1::add_velocity);
	ClassDB::bind_method(D_METHOD("follow_node", "self", "target", "speed"), &PS1::follow_node);
	ClassDB::bind_method(D_METHOD("raycast_group", "from", "dir", "dist", "group"), &PS1::raycast_group);
	ClassDB::bind_method(D_METHOD("get_ray_point"), &PS1::get_ray_point);
	ClassDB::bind_method(D_METHOD("get_ray_node"), &PS1::get_ray_node);
	ClassDB::bind_method(D_METHOD("spawn_shot", "from", "vel", "life", "tex"), &PS1::spawn_shot);
	ClassDB::bind_method(D_METHOD("clear_shots"), &PS1::clear_shots);
	ClassDB::bind_method(D_METHOD("get_shot_count"), &PS1::get_shot_count);
	ClassDB::bind_method(D_METHOD("get_free_shots"), &PS1::get_free_shots);
	ClassDB::bind_method(D_METHOD("shot_hit", "i"), &PS1::shot_hit);
	ClassDB::bind_method(D_METHOD("clip_orbit", "on"), &PS1::clip_orbit);
	ClassDB::bind_method(D_METHOD("set_y_sort", "node", "on"), &PS1::set_y_sort);
	ClassDB::bind_method(D_METHOD("wrap_position", "node", "min", "max"), &PS1::wrap_position);
	ClassDB::bind_method(D_METHOD("set_cull_dist", "d"), &PS1::set_cull_dist);
	ClassDB::bind_method(D_METHOD("create_tween"), &PS1::create_tween);
	ClassDB::bind_method(D_METHOD("tween_property", "tween", "node", "prop", "to", "sec"), &PS1::tween_property);
	ClassDB::bind_method(D_METHOD("kill_tweens", "node"), &PS1::kill_tweens);
	ClassDB::bind_method(D_METHOD("tween_count"), &PS1::tween_count);
	ClassDB::bind_method(D_METHOD("count_waypoints", "path"), &PS1::count_waypoints, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("get_waypoint", "i"), &PS1::get_waypoint);
	ClassDB::bind_method(D_METHOD("get_waypoint_pos", "i"), &PS1::get_waypoint_pos);
	ClassDB::bind_method(D_METHOD("nav_nearest", "from"), &PS1::nav_nearest);
	ClassDB::bind_method(D_METHOD("nav_next", "from", "to"), &PS1::nav_next);
	ClassDB::bind_method(D_METHOD("load_nav", "path"), &PS1::load_nav);
	ClassDB::bind_method(D_METHOD("unload_nav", "path"), &PS1::unload_nav);
	ClassDB::bind_method(D_METHOD("can_load_nav", "path"), &PS1::can_load_nav);
	ClassDB::bind_method(D_METHOD("is_nav_loaded", "path"), &PS1::is_nav_loaded);
	ClassDB::bind_method(D_METHOD("load_paths", "path"), &PS1::load_paths);
	ClassDB::bind_method(D_METHOD("unload_paths", "path"), &PS1::unload_paths);
	ClassDB::bind_method(D_METHOD("can_load_paths", "path"), &PS1::can_load_paths);
	ClassDB::bind_method(D_METHOD("is_paths_loaded", "path"), &PS1::is_paths_loaded);
	ClassDB::bind_method(D_METHOD("load_waypoints", "path"), &PS1::load_waypoints);
	ClassDB::bind_method(D_METHOD("unload_waypoints", "path"), &PS1::unload_waypoints);
	ClassDB::bind_method(D_METHOD("can_load_waypoints", "path"), &PS1::can_load_waypoints);
	ClassDB::bind_method(D_METHOD("is_waypoints_loaded", "path"), &PS1::is_waypoints_loaded);
	ClassDB::bind_method(D_METHOD("unload_pack", "path"), &PS1::unload_pack);
	ClassDB::bind_method(D_METHOD("change_scene", "path"), &PS1::change_scene);
	ClassDB::bind_method(D_METHOD("set_coyote", "ms"), &PS1::set_coyote);
	ClassDB::bind_method(D_METHOD("set_jump_buffer", "ms"), &PS1::set_jump_buffer);
	ClassDB::bind_method(D_METHOD("can_jump", "node"), &PS1::can_jump);
	ClassDB::bind_method(D_METHOD("consume_jump", "node"), &PS1::consume_jump);
	ClassDB::bind_method(D_METHOD("set_one_way_pass", "node", "on"), &PS1::set_one_way_pass);
	ClassDB::bind_method(D_METHOD("change_scene_fade", "path", "seconds"), &PS1::change_scene_fade);
	ClassDB::bind_method(D_METHOD("set_invuln", "node", "ms"), &PS1::set_invuln);
	ClassDB::bind_method(D_METHOD("is_invuln", "node"), &PS1::is_invuln);
	ClassDB::bind_method(D_METHOD("set_air_jumps", "n"), &PS1::set_air_jumps);
	ClassDB::bind_method(D_METHOD("set_wall_jump", "ms"), &PS1::set_wall_jump);
	ClassDB::bind_method(D_METHOD("set_checkpoint", "node"), &PS1::set_checkpoint);
	ClassDB::bind_method(D_METHOD("respawn", "node"), &PS1::respawn);
	ClassDB::bind_method(D_METHOD("set_frame", "node", "i"), &PS1::set_frame);
	ClassDB::bind_method(D_METHOD("set_flip", "node", "h", "v"), &PS1::set_flip);
	ClassDB::bind_method(D_METHOD("set_hitstop", "ms"), &PS1::set_hitstop);
	ClassDB::bind_method(D_METHOD("knockback", "node", "vx", "vy", "vz"), &PS1::knockback);
	ClassDB::bind_method(D_METHOD("stop_music"), &PS1::stop_music);
	ClassDB::bind_method(D_METHOD("set_music_fade", "to_vol", "ms"), &PS1::set_music_fade);
	ClassDB::bind_method(D_METHOD("say", "hud", "line", "cps"), &PS1::say);
	ClassDB::bind_method(D_METHOD("say_done"), &PS1::say_done);
	ClassDB::bind_method(D_METHOD("swap_pack", "unload_path", "load_path"), &PS1::swap_pack);
	ClassDB::bind_method(D_METHOD("set_checkpoint_scene", "path"), &PS1::set_checkpoint_scene);
	ClassDB::bind_method(D_METHOD("shot_hit_any"), &PS1::shot_hit_any);
	ClassDB::bind_method(D_METHOD("last_shot_hit"), &PS1::last_shot_hit);
	ClassDB::bind_method(D_METHOD("set_hp", "node", "n"), &PS1::set_hp);
	ClassDB::bind_method(D_METHOD("get_hp", "node"), &PS1::get_hp);
	ClassDB::bind_method(D_METHOD("hurt", "node", "amount", "vx", "vy", "vz"), &PS1::hurt);
	ClassDB::bind_method(D_METHOD("portal", "unload_path", "load_path", "fade"), &PS1::portal);
	ClassDB::bind_method(D_METHOD("nav_follow", "node", "goal", "speed"), &PS1::nav_follow);
	ClassDB::bind_method(D_METHOD("move_planar", "node", "ax", "ay", "speed"), &PS1::move_planar);
	ClassDB::bind_method(D_METHOD("save_slot", "slot"), &PS1::save_slot);
	ClassDB::bind_method(D_METHOD("load_slot", "slot"), &PS1::load_slot);
	ClassDB::bind_method(D_METHOD("set_spawn", "node", "x", "y", "z"), &PS1::set_spawn);
	ClassDB::bind_method(D_METHOD("unload_extras"), &PS1::unload_extras);
	ClassDB::bind_method(D_METHOD("set_global_position", "node", "pos"), &PS1::set_global_position);
	ClassDB::bind_method(D_METHOD("set_process_when_paused", "node", "on"), &PS1::set_process_when_paused);
	ClassDB::bind_method(D_METHOD("tween_is_done", "tw"), &PS1::tween_is_done);
	ClassDB::bind_method(D_METHOD("overlaps_group", "node", "group"), &PS1::overlaps_group);
	ClassDB::bind_method(D_METHOD("overlaps_group_entered", "node", "group"), &PS1::overlaps_group_entered);
	ClassDB::bind_method(D_METHOD("unload_pack_of", "node"), &PS1::unload_pack_of);
	ClassDB::bind_method(D_METHOD("quit"), &PS1::quit);
	ClassDB::bind_method(D_METHOD("set_path_progress", "node", "t"), &PS1::set_path_progress);
	ClassDB::bind_method(D_METHOD("get_path_progress", "node"), &PS1::get_path_progress);
	ClassDB::bind_method(D_METHOD("get_path_point", "path", "i"), &PS1::get_path_point);
	ClassDB::bind_method(D_METHOD("count_path_points", "path"), &PS1::count_path_points);
	ClassDB::bind_method(D_METHOD("nav_next_node", "from", "to"), &PS1::nav_next_node);
}

static PackedByteArray _mc_wrap(const PackedByteArray &p_payload, const String &p_title) {
	if (p_payload.size() >= 2 && p_payload[0] == 'S' && p_payload[1] == 'C') {
		return p_payload;
	}
	if (p_payload.size() == 128 * 1024 && p_payload.size() >= 2 && p_payload[0] == 'M' && p_payload[1] == 'C') {
		return p_payload;
	}
	const int max_payload = 3 * 8192 - 512;
	if (p_payload.size() > max_payload) {
		return PackedByteArray();
	}
	const int blocks = 1 + (p_payload.size() + 511) / 8192;
	PackedByteArray out;
	out.resize(512 + p_payload.size());
	uint8_t *w = out.ptrw();
	memset(w, 0, out.size());
	w[0] = 'S';
	w[1] = 'C';
	w[2] = 0x11;
	w[3] = uint8_t(CLAMP(blocks, 1, 3));
	const CharString title = p_title.utf8();
	const int n = MIN(title.length(), 32);
	for (int i = 0; i < n; i++) {
		w[4 + i] = uint8_t(title[i]);
	}
	for (int i = 0; i < p_payload.size(); i++) {
		w[512 + i] = p_payload[i];
	}
	return out;
}

static PackedByteArray _mc_unwrap(const PackedByteArray &p_framed) {
	if (p_framed.size() == 128 * 1024 && p_framed.size() >= 2 && p_framed[0] == 'M' && p_framed[1] == 'C') {
		for (int e = 0; e < 15; e++) {
			const int off = 0x80 + e * 0x80;
			if (off + 0x80 > p_framed.size()) {
				break;
			}
			if (p_framed[off] == 0x51) {
				const int first = 1 + e;
				const int start = first * 8192;
				if (start + 512 < p_framed.size() && p_framed[start] == 'S' && p_framed[start + 1] == 'C') {
					PackedByteArray payload;
					const int avail = p_framed.size() - start - 512;
					payload.resize(avail);
					memcpy(payload.ptrw(), p_framed.ptr() + start + 512, avail);
					return payload;
				}
			}
		}
		return PackedByteArray();
	}
	if (p_framed.size() >= 2 && p_framed[0] == 'S' && p_framed[1] == 'C') {
		PackedByteArray payload;
		const int n = p_framed.size() - 512;
		if (n <= 0) {
			return PackedByteArray();
		}
		payload.resize(n);
		memcpy(payload.ptrw(), p_framed.ptr() + 512, n);
		return payload;
	}
	return p_framed;
}

PackedByteArray PS1::memcard_wrap(const Variant &p_payload, const String &p_title) {
	if (p_payload.get_type() == Variant::PACKED_BYTE_ARRAY) {
		return _mc_wrap(p_payload, p_title);
	}
	PackedByteArray raw;
	if (p_payload.get_type() == Variant::STRING) {
		const CharString cs = String(p_payload).utf8();
		raw.resize(cs.length());
		if (cs.length()) {
			memcpy(raw.ptrw(), cs.get_data(), cs.length());
		}
	}
	return _mc_wrap(raw, p_title);
}

PackedByteArray PS1::memcard_unwrap(const PackedByteArray &p_framed) {
	return _mc_unwrap(p_framed);
}

static void _count_scene(Node *p_node, int &r_nodes, int &r_tris, int &r_sprites, int &r_hud, int &r_tiles) {
	if (!p_node) {
		return;
	}
	r_nodes++;
	if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(p_node)) {
		const Ref<Mesh> mesh = mi->get_mesh();
		if (mesh.is_valid()) {
			r_tris += mesh->get_faces().size();
		}
	}
	if (Object::cast_to<Sprite2D>(p_node)) {
		r_sprites++;
	}
	if (Object::cast_to<Control>(p_node)) {
		r_hud++;
	}
	if (TileMap *tm = Object::cast_to<TileMap>(p_node)) {
		r_tiles += tm->get_used_cells(0).size();
	}
	if (TileMapLayer *tl = Object::cast_to<TileMapLayer>(p_node)) {
		r_tiles += tl->get_used_cells().size();
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		_count_scene(p_node->get_child(i), r_nodes, r_tris, r_sprites, r_hud, r_tiles);
	}
}

static int _count_literal_packs() {
	int n = 1;
	Vector<String> stack;
	stack.push_back("res://");
	HashSet<String> seen;
	while (!stack.is_empty()) {
		const String dir = stack[stack.size() - 1];
		stack.resize(stack.size() - 1);
		Ref<DirAccess> da = DirAccess::open(dir);
		if (da.is_null()) {
			continue;
		}
		da->list_dir_begin();
		for (String name = da->get_next(); !name.is_empty(); name = da->get_next()) {
			if (name == "." || name == "..") {
				continue;
			}
			const String path = dir.path_join(name);
			if (da->current_is_dir()) {
				if (name == "export" || name == ".godot" || name == "addons" || name == "tests" || name == "tools") {
					continue;
				}
				stack.push_back(path);
				continue;
			}
			if (!(name.ends_with(".gd") || name.ends_with(".luau"))) {
				continue;
			}
			if (name == "run_tests.gd" || name.begins_with("test_")) {
				continue;
			}
			const String src = FileAccess::get_file_as_string(path);
			Vector<String> scenes;
			ps1_cook::collect_literal_scenes(src, scenes);
			for (int i = 0; i < scenes.size(); i++) {
				if (!seen.has(scenes[i])) {
					seen.insert(scenes[i]);
					n++;
				}
			}
		}
	}
	return n;
}

void PS1FlavorEditorPlugin::_sync_singleton(bool p_enabled) {
	Engine *eng = Engine::get_singleton();
	if (!eng) {
		return;
	}
	if (p_enabled) {
		if (!ps1_stub) {
			ps1_stub = memnew(PS1);
		}
		if (!eng->has_singleton("PS1")) {
			eng->add_singleton(Engine::Singleton("PS1", ps1_stub));
		}
	} else {
		// Keep the process-lifetime stub. GDScript only treats PS1.* as
		// callable when Engine::has_singleton("PS1") is true at analyze time.
	}
	// #region agent log
	{
		FILE *df = fopen("D:/projects/ps1_blazium/debug-9aa74d.log", "a");
		if (df) {
			fprintf(df,
					"{\"sessionId\":\"9aa74d\",\"runId\":\"post-fix\",\"hypothesisId\":\"H2\",\"location\":\"flavor_plugin.cpp:_sync_singleton\",\"message\":\"PS1 singleton sync\",\"data\":{\"want\":%s,\"has\":%s},\"timestamp\":%lld}\n",
					p_enabled ? "true" : "false",
					eng->has_singleton("PS1") ? "true" : "false",
					(long long)time(nullptr) * 1000);
			fclose(df);
		}
	}
	// #endregion
}

void PS1FlavorEditorPlugin::_refresh() {
	if (!status || !warn) {
		return;
	}
	const bool enabled = ProjectSettings::get_singleton() && bool(ProjectSettings::get_singleton()->get("ps1/enabled"));
	_sync_singleton(true);
	last_enabled = enabled;
	if (!enabled) {
		status->set_text("Turn on Project Settings → ps1/enabled for live PS1 budget warnings.\nPS1.* is PS1 export only; use OS.has_feature(\"ps1\").");
		warn->set_text("");
		return;
	}
	Node *root = EditorInterface::get_singleton() ? EditorInterface::get_singleton()->get_edited_scene_root() : nullptr;
	int nodes = 0, tris = 0, sprites = 0, hud = 0, tiles = 0;
	_count_scene(root, nodes, tris, sprites, hud, tiles);
	const int packs = _count_literal_packs();
	const int est_ram = nodes * 56 + tris * 32 + hud * 80 + tiles * 8;
	status->set_text(vformat(
			"nodes %d/%d\ntris %d/%d\nHUD %d/%d\ntiles %d/%d\nSprite2D %d/%d\npacks %d/%d\nest RAM %d/%d\nPS1.* is PS1 export only; use OS.has_feature(\"ps1\").\nABI 20: 6DoF/fly, per-node floor, shot pool, orbit clip, y-sort/wrap/cull, RayCast, NAV/PATH/WAY, 8 tweens, Timer/RemoteTransform/notifiers.",
			nodes, ps1_cook::kMaxNodes, tris, ps1_cook::kMaxTris, hud, ps1_cook::kMaxHud, tiles, ps1_cook::kMaxTiles,
			sprites, ps1_cook::kMaxSprites, packs, ps1_cook::kMaxScenePacks, est_ram, 2 * 1024 * 1024));
	PackedStringArray reasons;
	if (nodes > ps1_cook::kMaxNodes) {
		reasons.push_back(vformat("too many nodes (%d/%d)", nodes, ps1_cook::kMaxNodes));
	}
	if (tris > ps1_cook::kMaxTris) {
		reasons.push_back(vformat("too many tris (%d/%d)", tris, ps1_cook::kMaxTris));
	}
	if (hud > ps1_cook::kMaxHud) {
		reasons.push_back(vformat("too many HUD widgets (%d/%d)", hud, ps1_cook::kMaxHud));
	}
	if (tiles > ps1_cook::kMaxTiles) {
		reasons.push_back(vformat("too many tiles (%d/%d)", tiles, ps1_cook::kMaxTiles));
	}
	if (sprites > ps1_cook::kMaxSprites) {
		reasons.push_back(vformat("too many Sprite2D (%d/%d)", sprites, ps1_cook::kMaxSprites));
	}
	if (packs > ps1_cook::kMaxScenePacks) {
		reasons.push_back(vformat("too many packs (%d/%d)", packs, ps1_cook::kMaxScenePacks));
	}
	if (est_ram > 2 * 1024 * 1024) {
		reasons.push_back("est RAM blob exceeds 2 MiB");
	}
	if (root && nodes == 0) {
		reasons.push_back("empty scene");
	}
	if (reasons.is_empty()) {
		warn->set_text("");
	} else {
		const String msg = "PS1 budget: " + String(", ").join(reasons);
		warn->set_text(msg);
		WARN_PRINT(msg);
	}
}

void PS1FlavorEditorPlugin::_notification(int p_what) {
	if (p_what == NOTIFICATION_PROCESS && dock) {
		Node *root = EditorInterface::get_singleton() ? EditorInterface::get_singleton()->get_edited_scene_root() : nullptr;
		const ObjectID id = root ? root->get_instance_id() : ObjectID();
		int nodes = 0, tris = 0, sprites = 0, hud = 0, tiles = 0;
		_count_scene(root, nodes, tris, sprites, hud, tiles);
		const bool enabled = ProjectSettings::get_singleton() && bool(ProjectSettings::get_singleton()->get("ps1/enabled"));
		if (id != last_root || nodes != last_nodes || enabled != last_enabled) {
			last_root = id;
			last_nodes = nodes;
			_refresh();
		}
	}
}

PS1FlavorEditorPlugin::PS1FlavorEditorPlugin() {
	const bool headless = DisplayServer::get_singleton() && String(DisplayServer::get_singleton()->get_name()) == "headless";
	if (!headless) {
		dock = memnew(VBoxContainer);
		status = memnew(Label);
		status->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
		dock->add_child(status);
		warn = memnew(Label);
		warn->add_theme_color_override("font_color", Color(1, 0.35f, 0.35f));
		warn->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
		dock->add_child(warn);
		add_control_to_dock(DOCK_SLOT_RIGHT_UL, dock);
		set_process(true);
	}
	_sync_singleton(true);
	if (!headless) {
		_refresh();
	}
}

PS1FlavorEditorPlugin::~PS1FlavorEditorPlugin() {
	// Do not drop the Engine singleton or delete a stub the Engine still owns.
	// Headless --export-release tears the plugin down before script cook.
	ps1_stub = nullptr;
	if (dock) {
		remove_control_from_docks(dock);
		dock = nullptr;
	}
}
