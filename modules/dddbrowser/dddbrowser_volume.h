/**************************************************************************/
/*  dddbrowser_volume.h                                                   */
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

#include "scene/3d/node_3d.h"

class DDDBrowserVolume : public Node3D {
	GDCLASS(DDDBrowserVolume, Node3D);

public:
	enum VolumeType {
		VOLUME_AUTOSAVE,
		VOLUME_LOOK_AT,
		VOLUME_LOOKED_AT,
		VOLUME_SINGLE_TRIGGER,
		VOLUME_MULTI_TRIGGER,
		VOLUME_COOLDOWN_TRIGGER,
		VOLUME_EXIT_TRIGGER,
		VOLUME_STAY_TRIGGER,
		VOLUME_TIMED_ENTRY_TRIGGER,
		VOLUME_COUNTER_TRIGGER,
		VOLUME_SEQUENCE_TRIGGER,
		VOLUME_TOGGLE_TRIGGER,
		VOLUME_TELEPORT,
		VOLUME_INTERACTION,
	};

private:
	VolumeType volume_type = VOLUME_SINGLE_TRIGGER;
	float radius = 1.0f;
	String event_name = "on_trigger";
	bool single_use = false;
	float cooldown_seconds = 1.0f;
	float fov_degrees = 30.0f;
	String target_instance_id;
	int max_fires = 0;
	bool auto_remove_on_fire = false;
	float stay_interval = 1.0f;
	float required_stay_time = 1.0f;
	String counter_word = "count";
	int required_count = 1;
	bool auto_reset_after_fire = false;
	bool fire_once_when_reached = false;
	String sequence_group_id = "seq";
	int sequence_index = 0;
	bool reset_if_wrong = true;
	bool is_on = false;
	String on_activate_event;
	String on_deactivate_event;
	String action = "interact";
	Vector3 target_position;
	bool keep_velocity = false;
	Dictionary extra_props;

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	void set_volume_type(VolumeType p_type);
	VolumeType get_volume_type() const;
	void set_radius(float p_radius);
	float get_radius() const;
	void set_event_name(const String &p_name);
	String get_event_name() const;
	void set_single_use(bool p_v);
	bool get_single_use() const;
	void set_cooldown_seconds(float p_v);
	float get_cooldown_seconds() const;
	void set_fov_degrees(float p_v);
	float get_fov_degrees() const;
	void set_target_instance_id(const String &p_id);
	String get_target_instance_id() const;
	void set_max_fires(int p_v);
	int get_max_fires() const;
	void set_auto_remove_on_fire(bool p_v);
	bool get_auto_remove_on_fire() const;
	void set_stay_interval(float p_v);
	float get_stay_interval() const;
	void set_required_stay_time(float p_v);
	float get_required_stay_time() const;
	void set_counter_word(const String &p_v);
	String get_counter_word() const;
	void set_required_count(int p_v);
	int get_required_count() const;
	void set_auto_reset_after_fire(bool p_v);
	bool get_auto_reset_after_fire() const;
	void set_fire_once_when_reached(bool p_v);
	bool get_fire_once_when_reached() const;
	void set_sequence_group_id(const String &p_v);
	String get_sequence_group_id() const;
	void set_sequence_index(int p_v);
	int get_sequence_index() const;
	void set_reset_if_wrong(bool p_v);
	bool get_reset_if_wrong() const;
	void set_is_on(bool p_v);
	bool get_is_on() const;
	void set_on_activate_event(const String &p_v);
	String get_on_activate_event() const;
	void set_on_deactivate_event(const String &p_v);
	String get_on_deactivate_event() const;
	void set_action(const String &p_v);
	String get_action() const;
	void set_target_position(const Vector3 &p_v);
	Vector3 get_target_position() const;
	void set_keep_velocity(bool p_v);
	bool get_keep_velocity() const;
	void set_extra_props(const Dictionary &p_props);
	Dictionary get_extra_props() const;

	String type_string() const;
	String properties_key() const;
	Dictionary build_properties_dictionary() const;
};

VARIANT_ENUM_CAST(DDDBrowserVolume::VolumeType);
