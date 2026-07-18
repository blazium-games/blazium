/**************************************************************************/
/*  dddbrowser_volume.cpp                                                 */
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

#include "dddbrowser_volume.h"

void DDDBrowserVolume::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_volume_type", "type"), &DDDBrowserVolume::set_volume_type);
	ClassDB::bind_method(D_METHOD("get_volume_type"), &DDDBrowserVolume::get_volume_type);
	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &DDDBrowserVolume::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &DDDBrowserVolume::get_radius);
	ClassDB::bind_method(D_METHOD("set_event_name", "name"), &DDDBrowserVolume::set_event_name);
	ClassDB::bind_method(D_METHOD("get_event_name"), &DDDBrowserVolume::get_event_name);
	ClassDB::bind_method(D_METHOD("set_single_use", "v"), &DDDBrowserVolume::set_single_use);
	ClassDB::bind_method(D_METHOD("get_single_use"), &DDDBrowserVolume::get_single_use);
	ClassDB::bind_method(D_METHOD("set_cooldown_seconds", "v"), &DDDBrowserVolume::set_cooldown_seconds);
	ClassDB::bind_method(D_METHOD("get_cooldown_seconds"), &DDDBrowserVolume::get_cooldown_seconds);
	ClassDB::bind_method(D_METHOD("set_fov_degrees", "v"), &DDDBrowserVolume::set_fov_degrees);
	ClassDB::bind_method(D_METHOD("get_fov_degrees"), &DDDBrowserVolume::get_fov_degrees);
	ClassDB::bind_method(D_METHOD("set_target_instance_id", "id"), &DDDBrowserVolume::set_target_instance_id);
	ClassDB::bind_method(D_METHOD("get_target_instance_id"), &DDDBrowserVolume::get_target_instance_id);
	ClassDB::bind_method(D_METHOD("set_max_fires", "v"), &DDDBrowserVolume::set_max_fires);
	ClassDB::bind_method(D_METHOD("get_max_fires"), &DDDBrowserVolume::get_max_fires);
	ClassDB::bind_method(D_METHOD("set_auto_remove_on_fire", "v"), &DDDBrowserVolume::set_auto_remove_on_fire);
	ClassDB::bind_method(D_METHOD("get_auto_remove_on_fire"), &DDDBrowserVolume::get_auto_remove_on_fire);
	ClassDB::bind_method(D_METHOD("set_stay_interval", "v"), &DDDBrowserVolume::set_stay_interval);
	ClassDB::bind_method(D_METHOD("get_stay_interval"), &DDDBrowserVolume::get_stay_interval);
	ClassDB::bind_method(D_METHOD("set_required_stay_time", "v"), &DDDBrowserVolume::set_required_stay_time);
	ClassDB::bind_method(D_METHOD("get_required_stay_time"), &DDDBrowserVolume::get_required_stay_time);
	ClassDB::bind_method(D_METHOD("set_counter_word", "v"), &DDDBrowserVolume::set_counter_word);
	ClassDB::bind_method(D_METHOD("get_counter_word"), &DDDBrowserVolume::get_counter_word);
	ClassDB::bind_method(D_METHOD("set_required_count", "v"), &DDDBrowserVolume::set_required_count);
	ClassDB::bind_method(D_METHOD("get_required_count"), &DDDBrowserVolume::get_required_count);
	ClassDB::bind_method(D_METHOD("set_auto_reset_after_fire", "v"), &DDDBrowserVolume::set_auto_reset_after_fire);
	ClassDB::bind_method(D_METHOD("get_auto_reset_after_fire"), &DDDBrowserVolume::get_auto_reset_after_fire);
	ClassDB::bind_method(D_METHOD("set_fire_once_when_reached", "v"), &DDDBrowserVolume::set_fire_once_when_reached);
	ClassDB::bind_method(D_METHOD("get_fire_once_when_reached"), &DDDBrowserVolume::get_fire_once_when_reached);
	ClassDB::bind_method(D_METHOD("set_sequence_group_id", "v"), &DDDBrowserVolume::set_sequence_group_id);
	ClassDB::bind_method(D_METHOD("get_sequence_group_id"), &DDDBrowserVolume::get_sequence_group_id);
	ClassDB::bind_method(D_METHOD("set_sequence_index", "v"), &DDDBrowserVolume::set_sequence_index);
	ClassDB::bind_method(D_METHOD("get_sequence_index"), &DDDBrowserVolume::get_sequence_index);
	ClassDB::bind_method(D_METHOD("set_reset_if_wrong", "v"), &DDDBrowserVolume::set_reset_if_wrong);
	ClassDB::bind_method(D_METHOD("get_reset_if_wrong"), &DDDBrowserVolume::get_reset_if_wrong);
	ClassDB::bind_method(D_METHOD("set_is_on", "v"), &DDDBrowserVolume::set_is_on);
	ClassDB::bind_method(D_METHOD("get_is_on"), &DDDBrowserVolume::get_is_on);
	ClassDB::bind_method(D_METHOD("set_on_activate_event", "v"), &DDDBrowserVolume::set_on_activate_event);
	ClassDB::bind_method(D_METHOD("get_on_activate_event"), &DDDBrowserVolume::get_on_activate_event);
	ClassDB::bind_method(D_METHOD("set_on_deactivate_event", "v"), &DDDBrowserVolume::set_on_deactivate_event);
	ClassDB::bind_method(D_METHOD("get_on_deactivate_event"), &DDDBrowserVolume::get_on_deactivate_event);
	ClassDB::bind_method(D_METHOD("set_action", "v"), &DDDBrowserVolume::set_action);
	ClassDB::bind_method(D_METHOD("get_action"), &DDDBrowserVolume::get_action);
	ClassDB::bind_method(D_METHOD("set_target_position", "v"), &DDDBrowserVolume::set_target_position);
	ClassDB::bind_method(D_METHOD("get_target_position"), &DDDBrowserVolume::get_target_position);
	ClassDB::bind_method(D_METHOD("set_keep_velocity", "v"), &DDDBrowserVolume::set_keep_velocity);
	ClassDB::bind_method(D_METHOD("get_keep_velocity"), &DDDBrowserVolume::get_keep_velocity);
	ClassDB::bind_method(D_METHOD("set_extra_props", "props"), &DDDBrowserVolume::set_extra_props);
	ClassDB::bind_method(D_METHOD("get_extra_props"), &DDDBrowserVolume::get_extra_props);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "volume_type", PROPERTY_HINT_ENUM, "Autosave,LookAt,LookedAt,SingleTrigger,MultiTrigger,CooldownTrigger,ExitTrigger,StayTrigger,TimedEntryTrigger,CounterTrigger,SequenceTrigger,ToggleTrigger,Teleport,Interaction"), "set_volume_type", "get_volume_type");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_RANGE, "0,100,0.01"), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "event_name"), "set_event_name", "get_event_name");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "single_use"), "set_single_use", "get_single_use");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cooldown_seconds", PROPERTY_HINT_RANGE, "0,3600,0.01"), "set_cooldown_seconds", "get_cooldown_seconds");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fov_degrees", PROPERTY_HINT_RANGE, "0,180,0.1"), "set_fov_degrees", "get_fov_degrees");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "target_instance_id"), "set_target_instance_id", "get_target_instance_id");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_fires", PROPERTY_HINT_RANGE, "0,100000,1"), "set_max_fires", "get_max_fires");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_remove_on_fire"), "set_auto_remove_on_fire", "get_auto_remove_on_fire");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "stay_interval", PROPERTY_HINT_RANGE, "0.01,3600,0.01"), "set_stay_interval", "get_stay_interval");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "required_stay_time", PROPERTY_HINT_RANGE, "0.01,3600,0.01"), "set_required_stay_time", "get_required_stay_time");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "counter_word"), "set_counter_word", "get_counter_word");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "required_count", PROPERTY_HINT_RANGE, "1,100000,1"), "set_required_count", "get_required_count");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_reset_after_fire"), "set_auto_reset_after_fire", "get_auto_reset_after_fire");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "fire_once_when_reached"), "set_fire_once_when_reached", "get_fire_once_when_reached");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "sequence_group_id"), "set_sequence_group_id", "get_sequence_group_id");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sequence_index", PROPERTY_HINT_RANGE, "0,100000,1"), "set_sequence_index", "get_sequence_index");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "reset_if_wrong"), "set_reset_if_wrong", "get_reset_if_wrong");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_on"), "set_is_on", "get_is_on");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "on_activate_event"), "set_on_activate_event", "get_on_activate_event");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "on_deactivate_event"), "set_on_deactivate_event", "get_on_deactivate_event");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "action"), "set_action", "get_action");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "target_position"), "set_target_position", "get_target_position");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "keep_velocity"), "set_keep_velocity", "get_keep_velocity");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "extra_props"), "set_extra_props", "get_extra_props");

	BIND_ENUM_CONSTANT(VOLUME_AUTOSAVE);
	BIND_ENUM_CONSTANT(VOLUME_LOOK_AT);
	BIND_ENUM_CONSTANT(VOLUME_LOOKED_AT);
	BIND_ENUM_CONSTANT(VOLUME_SINGLE_TRIGGER);
	BIND_ENUM_CONSTANT(VOLUME_MULTI_TRIGGER);
	BIND_ENUM_CONSTANT(VOLUME_COOLDOWN_TRIGGER);
	BIND_ENUM_CONSTANT(VOLUME_EXIT_TRIGGER);
	BIND_ENUM_CONSTANT(VOLUME_STAY_TRIGGER);
	BIND_ENUM_CONSTANT(VOLUME_TIMED_ENTRY_TRIGGER);
	BIND_ENUM_CONSTANT(VOLUME_COUNTER_TRIGGER);
	BIND_ENUM_CONSTANT(VOLUME_SEQUENCE_TRIGGER);
	BIND_ENUM_CONSTANT(VOLUME_TOGGLE_TRIGGER);
	BIND_ENUM_CONSTANT(VOLUME_TELEPORT);
	BIND_ENUM_CONSTANT(VOLUME_INTERACTION);
}

void DDDBrowserVolume::_validate_property(PropertyInfo &p_property) const {
	const String n = p_property.name;
	if (n == "volume_type" || n == "radius" || n == "extra_props") {
		return;
	}
	bool show = false;
	switch (volume_type) {
		case VOLUME_AUTOSAVE:
			show = (n == "single_use");
			break;
		case VOLUME_LOOK_AT:
			show = (n == "event_name" || n == "single_use" || n == "cooldown_seconds" || n == "fov_degrees");
			break;
		case VOLUME_LOOKED_AT:
			show = (n == "event_name" || n == "target_instance_id" || n == "single_use" || n == "cooldown_seconds" || n == "fov_degrees");
			break;
		case VOLUME_SINGLE_TRIGGER:
		case VOLUME_EXIT_TRIGGER:
			show = (n == "event_name");
			break;
		case VOLUME_MULTI_TRIGGER:
			show = (n == "event_name" || n == "max_fires" || n == "auto_remove_on_fire");
			break;
		case VOLUME_COOLDOWN_TRIGGER:
			show = (n == "event_name" || n == "cooldown_seconds" || n == "max_fires");
			break;
		case VOLUME_STAY_TRIGGER:
			show = (n == "event_name" || n == "stay_interval");
			break;
		case VOLUME_TIMED_ENTRY_TRIGGER:
			show = (n == "event_name" || n == "required_stay_time");
			break;
		case VOLUME_COUNTER_TRIGGER:
			show = (n == "counter_word" || n == "required_count" || n == "auto_reset_after_fire" || n == "fire_once_when_reached");
			break;
		case VOLUME_SEQUENCE_TRIGGER:
			show = (n == "sequence_group_id" || n == "sequence_index" || n == "reset_if_wrong" || n == "event_name");
			break;
		case VOLUME_TOGGLE_TRIGGER:
			show = (n == "is_on" || n == "on_activate_event" || n == "on_deactivate_event");
			break;
		case VOLUME_TELEPORT:
			show = (n == "target_position" || n == "keep_velocity");
			break;
		case VOLUME_INTERACTION:
			show = (n == "event_name" || n == "action");
			break;
	}
	if (!show) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
}

void DDDBrowserVolume::set_volume_type(VolumeType p_type) {
	volume_type = p_type;
	notify_property_list_changed();
}
DDDBrowserVolume::VolumeType DDDBrowserVolume::get_volume_type() const {
	return volume_type;
}
void DDDBrowserVolume::set_radius(float p_radius) {
	radius = p_radius;
}
float DDDBrowserVolume::get_radius() const {
	return radius;
}
void DDDBrowserVolume::set_event_name(const String &p_name) {
	event_name = p_name;
}
String DDDBrowserVolume::get_event_name() const {
	return event_name;
}
void DDDBrowserVolume::set_single_use(bool p_v) {
	single_use = p_v;
}
bool DDDBrowserVolume::get_single_use() const {
	return single_use;
}
void DDDBrowserVolume::set_cooldown_seconds(float p_v) {
	cooldown_seconds = p_v;
}
float DDDBrowserVolume::get_cooldown_seconds() const {
	return cooldown_seconds;
}
void DDDBrowserVolume::set_fov_degrees(float p_v) {
	fov_degrees = p_v;
}
float DDDBrowserVolume::get_fov_degrees() const {
	return fov_degrees;
}
void DDDBrowserVolume::set_target_instance_id(const String &p_id) {
	target_instance_id = p_id;
}
String DDDBrowserVolume::get_target_instance_id() const {
	return target_instance_id;
}
void DDDBrowserVolume::set_max_fires(int p_v) {
	max_fires = p_v;
}
int DDDBrowserVolume::get_max_fires() const {
	return max_fires;
}
void DDDBrowserVolume::set_auto_remove_on_fire(bool p_v) {
	auto_remove_on_fire = p_v;
}
bool DDDBrowserVolume::get_auto_remove_on_fire() const {
	return auto_remove_on_fire;
}
void DDDBrowserVolume::set_stay_interval(float p_v) {
	stay_interval = p_v;
}
float DDDBrowserVolume::get_stay_interval() const {
	return stay_interval;
}
void DDDBrowserVolume::set_required_stay_time(float p_v) {
	required_stay_time = p_v;
}
float DDDBrowserVolume::get_required_stay_time() const {
	return required_stay_time;
}
void DDDBrowserVolume::set_counter_word(const String &p_v) {
	counter_word = p_v;
}
String DDDBrowserVolume::get_counter_word() const {
	return counter_word;
}
void DDDBrowserVolume::set_required_count(int p_v) {
	required_count = p_v;
}
int DDDBrowserVolume::get_required_count() const {
	return required_count;
}
void DDDBrowserVolume::set_auto_reset_after_fire(bool p_v) {
	auto_reset_after_fire = p_v;
}
bool DDDBrowserVolume::get_auto_reset_after_fire() const {
	return auto_reset_after_fire;
}
void DDDBrowserVolume::set_fire_once_when_reached(bool p_v) {
	fire_once_when_reached = p_v;
}
bool DDDBrowserVolume::get_fire_once_when_reached() const {
	return fire_once_when_reached;
}
void DDDBrowserVolume::set_sequence_group_id(const String &p_v) {
	sequence_group_id = p_v;
}
String DDDBrowserVolume::get_sequence_group_id() const {
	return sequence_group_id;
}
void DDDBrowserVolume::set_sequence_index(int p_v) {
	sequence_index = p_v;
}
int DDDBrowserVolume::get_sequence_index() const {
	return sequence_index;
}
void DDDBrowserVolume::set_reset_if_wrong(bool p_v) {
	reset_if_wrong = p_v;
}
bool DDDBrowserVolume::get_reset_if_wrong() const {
	return reset_if_wrong;
}
void DDDBrowserVolume::set_is_on(bool p_v) {
	is_on = p_v;
}
bool DDDBrowserVolume::get_is_on() const {
	return is_on;
}
void DDDBrowserVolume::set_on_activate_event(const String &p_v) {
	on_activate_event = p_v;
}
String DDDBrowserVolume::get_on_activate_event() const {
	return on_activate_event;
}
void DDDBrowserVolume::set_on_deactivate_event(const String &p_v) {
	on_deactivate_event = p_v;
}
String DDDBrowserVolume::get_on_deactivate_event() const {
	return on_deactivate_event;
}
void DDDBrowserVolume::set_action(const String &p_v) {
	action = p_v;
}
String DDDBrowserVolume::get_action() const {
	return action;
}
void DDDBrowserVolume::set_target_position(const Vector3 &p_v) {
	target_position = p_v;
}
Vector3 DDDBrowserVolume::get_target_position() const {
	return target_position;
}
void DDDBrowserVolume::set_keep_velocity(bool p_v) {
	keep_velocity = p_v;
}
bool DDDBrowserVolume::get_keep_velocity() const {
	return keep_velocity;
}
void DDDBrowserVolume::set_extra_props(const Dictionary &p_props) {
	extra_props = p_props;
}
Dictionary DDDBrowserVolume::get_extra_props() const {
	return extra_props;
}

String DDDBrowserVolume::type_string() const {
	return properties_key();
}

String DDDBrowserVolume::properties_key() const {
	switch (volume_type) {
		case VOLUME_AUTOSAVE:
			return "autosaveVolume";
		case VOLUME_LOOK_AT:
			return "lookAtVolume";
		case VOLUME_LOOKED_AT:
			return "lookedAtVolume";
		case VOLUME_MULTI_TRIGGER:
			return "multiTriggerVolume";
		case VOLUME_COOLDOWN_TRIGGER:
			return "cooldownTriggerVolume";
		case VOLUME_EXIT_TRIGGER:
			return "exitTriggerVolume";
		case VOLUME_STAY_TRIGGER:
			return "stayTriggerVolume";
		case VOLUME_TIMED_ENTRY_TRIGGER:
			return "timedEntryTriggerVolume";
		case VOLUME_COUNTER_TRIGGER:
			return "counterTriggerVolume";
		case VOLUME_SEQUENCE_TRIGGER:
			return "sequenceTriggerVolume";
		case VOLUME_TOGGLE_TRIGGER:
			return "toggleTriggerVolume";
		case VOLUME_TELEPORT:
			return "teleportVolume";
		case VOLUME_INTERACTION:
			return "interactionVolume";
		default:
			return "singleTriggerVolume";
	}
}

Dictionary DDDBrowserVolume::build_properties_dictionary() const {
	Dictionary props = extra_props.duplicate();
	switch (volume_type) {
		case VOLUME_AUTOSAVE:
			props["singleUse"] = single_use;
			break;
		case VOLUME_LOOK_AT:
			props["eventName"] = event_name;
			props["singleUse"] = single_use;
			props["cooldownSeconds"] = cooldown_seconds;
			props["fovDegrees"] = fov_degrees;
			break;
		case VOLUME_LOOKED_AT:
			props["eventName"] = event_name;
			props["targetInstanceId"] = target_instance_id;
			props["singleUse"] = single_use;
			props["cooldownSeconds"] = cooldown_seconds;
			props["fovDegrees"] = fov_degrees;
			break;
		case VOLUME_SINGLE_TRIGGER:
		case VOLUME_EXIT_TRIGGER:
			props["eventName"] = event_name;
			break;
		case VOLUME_MULTI_TRIGGER:
			props["eventName"] = event_name;
			props["maxFires"] = max_fires;
			props["autoRemoveOnFire"] = auto_remove_on_fire;
			break;
		case VOLUME_COOLDOWN_TRIGGER:
			props["eventName"] = event_name;
			props["cooldownSeconds"] = MAX(0.01, cooldown_seconds);
			props["maxFires"] = max_fires;
			break;
		case VOLUME_STAY_TRIGGER:
			props["eventName"] = event_name;
			props["stayInterval"] = MAX(0.01, stay_interval);
			break;
		case VOLUME_TIMED_ENTRY_TRIGGER:
			props["eventName"] = event_name;
			props["requiredStayTime"] = MAX(0.01, required_stay_time);
			break;
		case VOLUME_COUNTER_TRIGGER:
			props["counter_word"] = counter_word;
			props["required_count"] = MAX(1, required_count);
			props["auto_reset_after_fire"] = auto_reset_after_fire;
			props["fire_once_when_reached"] = fire_once_when_reached;
			break;
		case VOLUME_SEQUENCE_TRIGGER:
			props["sequence_group_id"] = sequence_group_id;
			props["sequence_index"] = sequence_index;
			props["reset_if_wrong"] = reset_if_wrong;
			if (!event_name.is_empty()) {
				props["eventName"] = event_name;
			}
			break;
		case VOLUME_TOGGLE_TRIGGER:
			props["is_on"] = is_on;
			if (!on_activate_event.is_empty()) {
				props["on_activate_event"] = on_activate_event;
			}
			if (!on_deactivate_event.is_empty()) {
				props["on_deactivate_event"] = on_deactivate_event;
			}
			break;
		case VOLUME_TELEPORT: {
			Dictionary tp;
			tp["x"] = target_position.x;
			tp["y"] = target_position.y;
			tp["z"] = target_position.z;
			props["target_position"] = tp;
			props["keep_velocity"] = keep_velocity;
		} break;
		case VOLUME_INTERACTION:
			props["eventName"] = event_name;
			props["action"] = action;
			break;
	}
	return props;
}
