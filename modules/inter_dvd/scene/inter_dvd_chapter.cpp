/**************************************************************************/
/*  inter_dvd_chapter.cpp                                                 */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#include "inter_dvd_chapter.h"

#include "core/object/class_db.h"

void InterDVDChapter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_source", "source"), &InterDVDChapter::set_source);
	ClassDB::bind_method(D_METHOD("get_source"), &InterDVDChapter::get_source);
	ClassDB::bind_method(D_METHOD("set_source_path", "path"), &InterDVDChapter::set_source_path);
	ClassDB::bind_method(D_METHOD("get_source_path"), &InterDVDChapter::get_source_path);
	ClassDB::bind_method(D_METHOD("set_packed_scene", "scene"), &InterDVDChapter::set_packed_scene);
	ClassDB::bind_method(D_METHOD("get_packed_scene"), &InterDVDChapter::get_packed_scene);
	ClassDB::bind_method(D_METHOD("set_duration_sec", "seconds"), &InterDVDChapter::set_duration_sec);
	ClassDB::bind_method(D_METHOD("get_duration_sec"), &InterDVDChapter::get_duration_sec);
	ClassDB::bind_method(D_METHOD("set_pip_source_path", "path"), &InterDVDChapter::set_pip_source_path);
	ClassDB::bind_method(D_METHOD("get_pip_source_path"), &InterDVDChapter::get_pip_source_path);
	ClassDB::bind_method(D_METHOD("set_audio_path", "path"), &InterDVDChapter::set_audio_path);
	ClassDB::bind_method(D_METHOD("get_audio_path"), &InterDVDChapter::get_audio_path);
	ClassDB::bind_method(D_METHOD("set_pip_slot_path", "path"), &InterDVDChapter::set_pip_slot_path);
	ClassDB::bind_method(D_METHOD("get_pip_slot_path"), &InterDVDChapter::get_pip_slot_path);
	ClassDB::bind_method(D_METHOD("set_pip_rect", "rect"), &InterDVDChapter::set_pip_rect);
	ClassDB::bind_method(D_METHOD("get_pip_rect"), &InterDVDChapter::get_pip_rect);
	ClassDB::bind_method(D_METHOD("set_pip_lead_sec", "seconds"), &InterDVDChapter::set_pip_lead_sec);
	ClassDB::bind_method(D_METHOD("get_pip_lead_sec"), &InterDVDChapter::get_pip_lead_sec);
	ClassDB::bind_method(D_METHOD("set_bake_hold_sec", "seconds"), &InterDVDChapter::set_bake_hold_sec);
	ClassDB::bind_method(D_METHOD("get_bake_hold_sec"), &InterDVDChapter::get_bake_hold_sec);
	ClassDB::bind_method(D_METHOD("set_bake_camera_path", "path"), &InterDVDChapter::set_bake_camera_path);
	ClassDB::bind_method(D_METHOD("get_bake_camera_path"), &InterDVDChapter::get_bake_camera_path);
	ClassDB::bind_method(D_METHOD("set_include_audio", "include"), &InterDVDChapter::set_include_audio);
	ClassDB::bind_method(D_METHOD("get_include_audio"), &InterDVDChapter::get_include_audio);
	ClassDB::bind_method(D_METHOD("set_loop_pad_sec", "seconds"), &InterDVDChapter::set_loop_pad_sec);
	ClassDB::bind_method(D_METHOD("get_loop_pad_sec"), &InterDVDChapter::get_loop_pad_sec);
	ClassDB::bind_method(D_METHOD("set_angle", "angle"), &InterDVDChapter::set_angle);
	ClassDB::bind_method(D_METHOD("get_angle"), &InterDVDChapter::get_angle);
	ClassDB::bind_method(D_METHOD("set_streams", "streams"), &InterDVDChapter::set_streams);
	ClassDB::bind_method(D_METHOD("get_streams"), &InterDVDChapter::get_streams);
	ClassDB::bind_method(D_METHOD("compile_cell"), &InterDVDChapter::compile_cell);
	ClassDB::bind_method(D_METHOD("fill_cell", "cell"), &InterDVDChapter::fill_cell);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "source", PROPERTY_HINT_ENUM, "Video,Scene"), "set_source", "get_source");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "source_path", PROPERTY_HINT_FILE, "*.mp4,*.mkv,*.mov,*.avi,*.webm,*.mpg,*.mpeg,*.vob,*.m2v"), "set_source_path", "get_source_path");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "packed_scene", PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"), "set_packed_scene", "get_packed_scene");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "duration_sec", PROPERTY_HINT_RANGE, "0,3600,0.1"), "set_duration_sec", "get_duration_sec");
	ADD_GROUP("Picture in Picture", "");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "pip_source_path", PROPERTY_HINT_FILE, "*.mp4,*.mkv,*.mov,*.avi,*.webm,*.mpg,*.mpeg,*.vob"), "set_pip_source_path", "get_pip_source_path");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "audio_path", PROPERTY_HINT_FILE, "*.wav,*.mp3,*.ogg,*.flac,*.m4a,*.aac,*.ac3"), "set_audio_path", "get_audio_path");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "pip_slot_path"), "set_pip_slot_path", "get_pip_slot_path");
	ADD_PROPERTY(PropertyInfo(Variant::RECT2, "pip_rect"), "set_pip_rect", "get_pip_rect");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pip_lead_sec", PROPERTY_HINT_RANGE, "0,5,0.01"), "set_pip_lead_sec", "get_pip_lead_sec");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bake_hold_sec", PROPERTY_HINT_RANGE, "0,60,0.1"), "set_bake_hold_sec", "get_bake_hold_sec");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "bake_camera_path"), "set_bake_camera_path", "get_bake_camera_path");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "include_audio"), "set_include_audio", "get_include_audio");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "loop_pad_sec", PROPERTY_HINT_RANGE, "0,600,0.1"), "set_loop_pad_sec", "get_loop_pad_sec");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "angle", PROPERTY_HINT_RANGE, "1,9,1"), "set_angle", "get_angle");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "streams", PROPERTY_HINT_ARRAY_TYPE, "InterDVDStream"), "set_streams", "get_streams");
	BIND_ENUM_CONSTANT(SOURCE_VIDEO);
	BIND_ENUM_CONSTANT(SOURCE_SCENE);
}

void InterDVDChapter::set_source(Source p_source) {
	if (source == p_source) {
		return;
	}
	source = p_source;
	notify_property_list_changed();
}

void InterDVDChapter::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == StringName("source_path") && source != SOURCE_VIDEO) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
	if ((p_property.name == StringName("packed_scene") || p_property.name == StringName("duration_sec")) && source != SOURCE_SCENE) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
}

void InterDVDChapter::fill_source(Ref<InterDVDCell> p_cell, Source p_source, const String &p_path, const Ref<PackedScene> &p_scene, double p_duration) {
	ERR_FAIL_COND(p_cell.is_null());
	if (p_source == SOURCE_SCENE) {
		p_cell->set_packed_scene(p_scene);
		p_cell->set_duration_sec(p_duration > 0.0 ? p_duration : 4.0);
		p_cell->set_source_path(String());
	} else {
		p_cell->set_source_path(p_path);
		p_cell->set_packed_scene(Ref<PackedScene>());
	}
}

void InterDVDChapter::fill_cell(Ref<InterDVDCell> p_cell) const {
	ERR_FAIL_COND(p_cell.is_null());
	p_cell->set_name(get_name());
	fill_source(p_cell, source, source_path, packed_scene, duration_sec);
	p_cell->set_pip_source_path(pip_source_path);
	p_cell->set_audio_path(audio_path);
	p_cell->set_pip_slot_path(pip_slot_path);
	p_cell->set_pip_rect(pip_rect);
	p_cell->set_pip_lead_sec(pip_lead_sec);
	p_cell->set_bake_hold_sec(bake_hold_sec);
	p_cell->set_bake_camera_path(bake_camera_path);
	p_cell->set_include_audio(include_audio);
	p_cell->set_loop_pad_sec(loop_pad_sec);
	p_cell->set_angle(angle);
	p_cell->set_streams(streams);
}

Ref<InterDVDCell> InterDVDChapter::compile_cell() const {
	Ref<InterDVDCell> cell;
	cell.instantiate();
	fill_cell(cell);
	return cell;
}
