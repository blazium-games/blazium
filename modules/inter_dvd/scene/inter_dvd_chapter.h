/**************************************************************************/
/*  inter_dvd_chapter.h                                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#pragma once

#include "modules/inter_dvd/author/inter_dvd_project.h"
#include "scene/main/node.h"
#include "scene/resources/packed_scene.h"

class InterDVDChapter : public Node {
	GDCLASS(InterDVDChapter, Node);

public:
	enum Source {
		SOURCE_VIDEO = 0,
		SOURCE_SCENE = 1,
	};

private:
	Source source = SOURCE_VIDEO;
	String source_path;
	Ref<PackedScene> packed_scene;
	double duration_sec = 4.0;
	String pip_source_path;
	String audio_path;
	NodePath pip_slot_path;
	Rect2 pip_rect;
	double pip_lead_sec = 0.40;
	double bake_hold_sec = 0.0;
	NodePath bake_camera_path;
	bool include_audio = true;
	double loop_pad_sec = 0.0;
	int angle = 1;
	TypedArray<InterDVDStream> streams;

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	void set_source(Source p_source);
	Source get_source() const { return source; }
	void set_source_path(const String &p_path) { source_path = p_path; }
	String get_source_path() const { return source_path; }
	void set_packed_scene(const Ref<PackedScene> &p_scene) { packed_scene = p_scene; }
	Ref<PackedScene> get_packed_scene() const { return packed_scene; }
	void set_duration_sec(double p_sec) { duration_sec = p_sec; }
	double get_duration_sec() const { return duration_sec; }
	void set_pip_source_path(const String &p_path) { pip_source_path = p_path; }
	String get_pip_source_path() const { return pip_source_path; }
	void set_audio_path(const String &p_path) { audio_path = p_path; }
	String get_audio_path() const { return audio_path; }
	void set_pip_slot_path(const NodePath &p_path) { pip_slot_path = p_path; }
	NodePath get_pip_slot_path() const { return pip_slot_path; }
	void set_pip_rect(const Rect2 &p_rect) { pip_rect = p_rect; }
	Rect2 get_pip_rect() const { return pip_rect; }
	void set_pip_lead_sec(double p_sec) { pip_lead_sec = p_sec; }
	double get_pip_lead_sec() const { return pip_lead_sec; }
	void set_bake_hold_sec(double p_sec) { bake_hold_sec = p_sec; }
	double get_bake_hold_sec() const { return bake_hold_sec; }
	void set_bake_camera_path(const NodePath &p_path) { bake_camera_path = p_path; }
	NodePath get_bake_camera_path() const { return bake_camera_path; }
	void set_include_audio(bool p_include) { include_audio = p_include; }
	bool get_include_audio() const { return include_audio; }
	void set_loop_pad_sec(double p_sec) { loop_pad_sec = p_sec; }
	double get_loop_pad_sec() const { return loop_pad_sec; }
	void set_angle(int p_angle) { angle = CLAMP(p_angle, 1, 9); }
	int get_angle() const { return angle; }
	void set_streams(const TypedArray<InterDVDStream> &p_streams) { streams = p_streams; }
	TypedArray<InterDVDStream> get_streams() const { return streams; }
	static void fill_source(Ref<InterDVDCell> p_cell, Source p_source, const String &p_path, const Ref<PackedScene> &p_scene, double p_duration);
	void fill_cell(Ref<InterDVDCell> p_cell) const;
	Ref<InterDVDCell> compile_cell() const;
};

VARIANT_ENUM_CAST(InterDVDChapter::Source);
