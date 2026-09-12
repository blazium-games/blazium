/**************************************************************************/
/*  inter_dvd_project.h                                                   */
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

#include "core/io/resource.h"
#include "core/math/color.h"
#include "core/math/math_funcs.h"
#include "core/math/rect2.h"
#include "core/string/node_path.h"
#include "core/variant/typed_array.h"
#include "scene/resources/packed_scene.h"

class Node;

class InterDVDStream : public Resource {
	GDCLASS(InterDVDStream, Resource);

public:
	enum Kind {
		KIND_AUDIO = 0,
		KIND_SUBTITLE = 1,
	};

private:
	Kind kind = KIND_AUDIO;
	String source_path;
	String language = "en";
	int code_extension = 1;
	bool enabled = true;

protected:
	static void _bind_methods();

public:
	void set_kind(Kind p_kind) { kind = p_kind; }
	Kind get_kind() const { return kind; }
	void set_source_path(const String &p_path) { source_path = p_path; }
	String get_source_path() const { return source_path; }
	void set_language(const String &p_lang) { language = p_lang; }
	String get_language() const { return language; }
	void set_code_extension(int p_ext) { code_extension = p_ext; }
	int get_code_extension() const { return code_extension; }
	void set_enabled(bool p_enabled) { enabled = p_enabled; }
	bool is_enabled() const { return enabled; }
};

VARIANT_ENUM_CAST(InterDVDStream::Kind);

class InterDVDCell : public Resource {
	GDCLASS(InterDVDCell, Resource);

	String encoded_path;
	String source_path;
	String pip_source_path;
	String audio_path;
	NodePath pip_slot_path;
	Rect2 pip_rect;
	double pip_lead_sec = 0.40;
	double bake_hold_sec = 0.0;
	NodePath bake_camera_path;
	Rect2 default_highlight;
	Ref<PackedScene> packed_scene;
	double duration_sec = 0.0;
	double loop_pad_sec = 0.0;
	bool include_audio = true;
	TypedArray<InterDVDStream> streams;
	TypedArray<PackedByteArray> post_commands;
	int angle = 1;

protected:
	static void _bind_methods();

public:
	void set_encoded_path(const String &p_path) { encoded_path = p_path; }
	String get_encoded_path() const { return encoded_path; }
	void set_source_path(const String &p_path) { source_path = p_path; }
	String get_source_path() const { return source_path; }
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
	void set_default_highlight(const Rect2 &p_rect) { default_highlight = p_rect; }
	Rect2 get_default_highlight() const { return default_highlight; }
	void set_packed_scene(const Ref<PackedScene> &p_scene) { packed_scene = p_scene; }
	Ref<PackedScene> get_packed_scene() const { return packed_scene; }
	void set_duration_sec(double p_sec) { duration_sec = p_sec; }
	double get_duration_sec() const { return duration_sec; }
	void set_loop_pad_sec(double p_sec) { loop_pad_sec = p_sec; }
	double get_loop_pad_sec() const { return loop_pad_sec; }
	void set_include_audio(bool p_include) { include_audio = p_include; }
	bool get_include_audio() const { return include_audio; }
	void set_streams(const TypedArray<InterDVDStream> &p_streams) { streams = p_streams; }
	TypedArray<InterDVDStream> get_streams() const { return streams; }
	void set_post_commands(const TypedArray<PackedByteArray> &p_cmds) { post_commands = p_cmds; }
	TypedArray<PackedByteArray> get_post_commands() const { return post_commands; }
	void set_angle(int p_angle) { angle = CLAMP(p_angle, 1, 9); }
	int get_angle() const { return angle; }
	String get_display_name() const;
};

class InterDVDButton : public Resource {
	GDCLASS(InterDVDButton, Resource);

public:
	enum Action {
		ACTION_JUMP_TITLE = 0,
		ACTION_JUMP_MENU = 1,
		ACTION_JUMP_PGC = 2,
		ACTION_CUSTOM = 3,
		ACTION_JUMP_CHAPTER = 4,
		ACTION_JUMP_PROGRAM = 5,
		ACTION_JUMP_CELL = 6,
		ACTION_RESUME = 7,
		ACTION_SET_AUDIO = 8,
		ACTION_SET_SUBTITLE = 9,
		ACTION_SET_ANGLE = 10,
		ACTION_HIGHLIGHT_BUTTON = 11,
		ACTION_EXIT = 12,
	};

	enum CommandDomain {
		DOMAIN_VMGM = 0,
		DOMAIN_VTST = 1,
	};

private:
	Rect2 highlight;
	PackedByteArray command;
	Action action = ACTION_JUMP_TITLE;
	int target = 1;
	int title_n = 1;
	int stream = 0;
	bool subtitle_on = true;
	bool auto_action = false;
	bool hidden = false;
	bool forced_selected = false;
	bool forced_activated = false;
	bool numeric_select = true;
	int adjacent_up = 0;
	int adjacent_down = 0;
	int adjacent_left = 0;
	int adjacent_right = 0;
	int button_number = 0;
	int color_group = 1;
	NodePath control_path;
	Color select_color = Color(1, 0.92, 0.2, 1);
	Color action_color = Color(1, 0.35, 0.1, 1);

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	void set_highlight(const Rect2 &p_rect) { highlight = p_rect; }
	Rect2 get_highlight() const { return highlight; }
	void set_command(const PackedByteArray &p_cmd) { command = p_cmd; }
	PackedByteArray get_command() const { return command; }
	void set_action(Action p_action);
	Action get_action() const { return action; }
	void set_target(int p_target);
	int get_target() const { return target; }
	void set_title_n(int p_title);
	int get_title_n() const;
	void set_stream(int p_stream) { stream = p_stream; }
	int get_stream() const { return stream; }
	void set_subtitle_on(bool p_on) { subtitle_on = p_on; }
	bool get_subtitle_on() const { return subtitle_on; }
	void set_auto_action(bool p_auto) { auto_action = p_auto; }
	bool get_auto_action() const { return auto_action; }
	void set_hidden(bool p_hidden) { hidden = p_hidden; }
	bool is_hidden() const { return hidden; }
	void set_forced_selected(bool p_forced) { forced_selected = p_forced; }
	bool is_forced_selected() const { return forced_selected; }
	void set_forced_activated(bool p_forced) { forced_activated = p_forced; }
	bool is_forced_activated() const { return forced_activated; }
	void set_numeric_select(bool p_numeric) { numeric_select = p_numeric; }
	bool get_numeric_select() const { return numeric_select; }
	void set_adjacent_up(int p_btn) { adjacent_up = p_btn; }
	int get_adjacent_up() const { return adjacent_up; }
	void set_adjacent_down(int p_btn) { adjacent_down = p_btn; }
	int get_adjacent_down() const { return adjacent_down; }
	void set_adjacent_left(int p_btn) { adjacent_left = p_btn; }
	int get_adjacent_left() const { return adjacent_left; }
	void set_adjacent_right(int p_btn) { adjacent_right = p_btn; }
	int get_adjacent_right() const { return adjacent_right; }
	void set_button_number(int p_n) { button_number = p_n; }
	int get_button_number() const { return button_number; }
	void set_color_group(int p_group) { color_group = p_group; }
	int get_color_group() const { return color_group; }
	void set_control_path(const NodePath &p_path) { control_path = p_path; }
	NodePath get_control_path() const { return control_path; }
	void set_select_color(const Color &p_color) { select_color = p_color; }
	Color get_select_color() const { return select_color; }
	void set_action_color(const Color &p_color) { action_color = p_color; }
	Color get_action_color() const { return action_color; }
	PackedByteArray resolve_command(CommandDomain p_domain = DOMAIN_VMGM) const;
	void sync_highlight_from_scene(Node *p_root, const Rect2 &p_default_highlight = Rect2(), int p_title_safe_bottom = -1);
};

VARIANT_ENUM_CAST(InterDVDButton::Action);
VARIANT_ENUM_CAST(InterDVDButton::CommandDomain);

class InterDVDMenu : public Resource {
	GDCLASS(InterDVDMenu, Resource);

public:
	enum MenuType {
		MENU_TITLE = 2,
		MENU_ROOT = 3,
		MENU_SUBPICTURE = 4,
		MENU_AUDIO = 5,
		MENU_ANGLE = 6,
		MENU_CHAPTER = 7,
	};

private:
	bool motion = false;
	TypedArray<InterDVDButton> buttons;
	Ref<InterDVDCell> cell;
	int still_time = 0;
	int next_pgc = 1;
	int prev_pgc = 1;
	int default_button = 1;
	int forced_selected_button = 0;
	int forced_activated_button = 0;
	MenuType menu_type = MENU_TITLE;
	int button_group_mask = 0x1000;
	PackedColorArray clut;
	int parental_id = 0;
	int uops = 0;
	int goup_pgc = 0;
	int title_set_nr = 0;

protected:
	static void _bind_methods();

public:
	void set_motion(bool p_motion) { motion = p_motion; }
	bool is_motion() const { return motion; }
	void set_buttons(const TypedArray<InterDVDButton> &p_buttons);
	TypedArray<InterDVDButton> get_buttons() const { return buttons; }
	void set_cell(const Ref<InterDVDCell> &p_cell) { cell = p_cell; }
	Ref<InterDVDCell> get_cell() const { return cell; }
	void set_still_time(int p_sec) { still_time = p_sec; }
	int get_still_time() const { return still_time; }
	void set_next_pgc(int p_pgc) { next_pgc = p_pgc; }
	int get_next_pgc() const { return next_pgc; }
	void set_prev_pgc(int p_pgc) { prev_pgc = p_pgc; }
	int get_prev_pgc() const { return prev_pgc; }
	void set_default_button(int p_btn) { default_button = p_btn; }
	int get_default_button() const { return default_button; }
	void set_forced_selected_button(int p_btn) { forced_selected_button = p_btn; }
	int get_forced_selected_button() const { return forced_selected_button; }
	void set_forced_activated_button(int p_btn) { forced_activated_button = p_btn; }
	int get_forced_activated_button() const { return forced_activated_button; }
	void set_menu_type(MenuType p_type) { menu_type = p_type; }
	MenuType get_menu_type() const { return menu_type; }
	void set_button_group_mask(int p_mask) { button_group_mask = p_mask; }
	int get_button_group_mask() const { return button_group_mask; }
	void set_clut(const PackedColorArray &p_clut) { clut = p_clut; }
	PackedColorArray get_clut() const { return clut; }
	void set_parental_id(int p_id) { parental_id = p_id; }
	int get_parental_id() const { return parental_id; }
	void set_uops(int p_uops) { uops = p_uops; }
	int get_uops() const { return uops; }
	void set_goup_pgc(int p_pgc) { goup_pgc = p_pgc; }
	int get_goup_pgc() const { return goup_pgc; }
	void set_title_set_nr(int p_n) { title_set_nr = p_n; }
	int get_title_set_nr() const { return title_set_nr; }
};

VARIANT_ENUM_CAST(InterDVDMenu::MenuType);

class InterDVDPGC : public Resource {
	GDCLASS(InterDVDPGC, Resource);

	TypedArray<InterDVDCell> cells;
	TypedArray<InterDVDButton> buttons;
	TypedArray<PackedByteArray> pre_commands;
	TypedArray<PackedByteArray> post_commands;
	int still_time = 0;
	int next_pgc = 0;
	int prev_pgc = 0;
	int default_button = 1;
	PackedColorArray clut;
	int parental_id = 0;
	int uops = 0;
	int goup_pgc = 0;
	int title_set_nr = 0;

protected:
	static void _bind_methods();

public:
	void set_cells(const TypedArray<InterDVDCell> &p_cells) { cells = p_cells; }
	TypedArray<InterDVDCell> get_cells() const { return cells; }
	void set_buttons(const TypedArray<InterDVDButton> &p_buttons);
	TypedArray<InterDVDButton> get_buttons() const { return buttons; }
	Ref<InterDVDCell> add_cell_from_video(const String &p_path);
	Ref<InterDVDButton> add_jump_title_button(int p_title_n = 1);
	void set_pre_commands(const TypedArray<PackedByteArray> &p_cmds) { pre_commands = p_cmds; }
	TypedArray<PackedByteArray> get_pre_commands() const { return pre_commands; }
	void set_post_commands(const TypedArray<PackedByteArray> &p_cmds) { post_commands = p_cmds; }
	TypedArray<PackedByteArray> get_post_commands() const { return post_commands; }
	void set_still_time(int p_sec) { still_time = p_sec; }
	int get_still_time() const { return still_time; }
	void set_next_pgc(int p_pgc) { next_pgc = p_pgc; }
	int get_next_pgc() const { return next_pgc; }
	void set_prev_pgc(int p_pgc) { prev_pgc = p_pgc; }
	int get_prev_pgc() const { return prev_pgc; }
	void set_default_button(int p_btn) { default_button = p_btn; }
	int get_default_button() const { return default_button; }
	void set_clut(const PackedColorArray &p_clut) { clut = p_clut; }
	PackedColorArray get_clut() const { return clut; }
	void set_parental_id(int p_id) { parental_id = p_id; }
	int get_parental_id() const { return parental_id; }
	void set_uops(int p_uops) { uops = p_uops; }
	int get_uops() const { return uops; }
	void set_goup_pgc(int p_pgc) { goup_pgc = p_pgc; }
	int get_goup_pgc() const { return goup_pgc; }
	void set_title_set_nr(int p_n) { title_set_nr = p_n; }
	int get_title_set_nr() const { return title_set_nr; }
};

class InterDVDProject : public Resource {
	GDCLASS(InterDVDProject, Resource);

	int region_mask = 0x01;
	int parental_level = 1;
	String volume_id = "BLAZIUM_DVD";
	String serial;
	String provider_id = "BLAZIUM INTER-DVD";
	String disc_title;
	String menu_language = "en";
	String audio_language = "en";
	String subtitle_language = "en";
	int ac3_bitrate_k = 192;
	int ac3_channels = 2;
	int gop_size = 15;
	int title_safe_bottom = 432;
	int bake_warmup_frames = 2;
	double pip_blackdetect_sec = 2.5;
	double pip_blackdetect_pix_th = 0.12;
	PackedStringArray extras;
	String extras_dir;
	String copyright;
	String copyright_file;
	String license;
	String license_file;
	String readme;
	String readme_file;
	String credits;
	String credits_file;
	String autorun_label;
	String autorun_open;
	String autorun_icon;
	String publisher;
	String author;
	String studio;
	String website;
	String version;
	Ref<InterDVDPGC> first_play;
	TypedArray<InterDVDPGC> titles;
	TypedArray<InterDVDMenu> menus;

protected:
	static void _bind_methods();

public:
	InterDVDProject();
	void reset_encode_defaults();
	void seed_from_project_settings();
	void set_region_mask(int p_mask) { region_mask = p_mask; }
	int get_region_mask() const { return region_mask; }
	void set_parental_level(int p_level) { parental_level = p_level; }
	int get_parental_level() const { return parental_level; }
	void set_volume_id(const String &p_id) { volume_id = p_id; }
	String get_volume_id() const { return volume_id; }
	void set_serial(const String &p_serial) { serial = p_serial; }
	String get_serial() const { return serial; }
	void set_provider_id(const String &p_id) { provider_id = p_id; }
	String get_provider_id() const { return provider_id; }
	void set_disc_title(const String &p_title) { disc_title = p_title; }
	String get_disc_title() const { return disc_title; }
	void set_menu_language(const String &p_lang) { menu_language = p_lang; }
	String get_menu_language() const { return menu_language; }
	void set_audio_language(const String &p_lang) { audio_language = p_lang; }
	String get_audio_language() const { return audio_language; }
	void set_subtitle_language(const String &p_lang) { subtitle_language = p_lang; }
	String get_subtitle_language() const { return subtitle_language; }
	void set_ac3_bitrate_k(int p_k) { ac3_bitrate_k = p_k; }
	int get_ac3_bitrate_k() const { return ac3_bitrate_k; }
	void set_ac3_channels(int p_ch) { ac3_channels = p_ch; }
	int get_ac3_channels() const { return ac3_channels; }
	void set_gop_size(int p_gop) { gop_size = p_gop; }
	int get_gop_size() const { return gop_size; }
	void set_pip_blackdetect_sec(double p_sec) { pip_blackdetect_sec = p_sec; }
	double get_pip_blackdetect_sec() const { return pip_blackdetect_sec; }
	void set_pip_blackdetect_pix_th(double p_th) { pip_blackdetect_pix_th = p_th; }
	double get_pip_blackdetect_pix_th() const { return pip_blackdetect_pix_th; }
	void set_title_safe_bottom(int p_y) { title_safe_bottom = p_y; }
	int get_title_safe_bottom() const { return title_safe_bottom; }
	void set_bake_warmup_frames(int p_frames) { bake_warmup_frames = p_frames; }
	int get_bake_warmup_frames() const { return bake_warmup_frames; }
	void set_extras(const PackedStringArray &p_extras) { extras = p_extras; }
	PackedStringArray get_extras() const { return extras; }
	void set_extras_dir(const String &p_dir) { extras_dir = p_dir; }
	String get_extras_dir() const { return extras_dir; }
	void set_copyright(const String &p_text) { copyright = p_text; }
	String get_copyright() const { return copyright; }
	void set_copyright_file(const String &p_path) { copyright_file = p_path; }
	String get_copyright_file() const { return copyright_file; }
	void set_license(const String &p_text) { license = p_text; }
	String get_license() const { return license; }
	void set_license_file(const String &p_path) { license_file = p_path; }
	String get_license_file() const { return license_file; }
	void set_readme(const String &p_text) { readme = p_text; }
	String get_readme() const { return readme; }
	void set_readme_file(const String &p_path) { readme_file = p_path; }
	String get_readme_file() const { return readme_file; }
	void set_credits(const String &p_text) { credits = p_text; }
	String get_credits() const { return credits; }
	void set_credits_file(const String &p_path) { credits_file = p_path; }
	String get_credits_file() const { return credits_file; }
	void set_autorun_label(const String &p_label) { autorun_label = p_label; }
	String get_autorun_label() const { return autorun_label; }
	void set_autorun_open(const String &p_open) { autorun_open = p_open; }
	String get_autorun_open() const { return autorun_open; }
	void set_autorun_icon(const String &p_icon) { autorun_icon = p_icon; }
	String get_autorun_icon() const { return autorun_icon; }
	void set_publisher(const String &p_publisher) { publisher = p_publisher; }
	String get_publisher() const { return publisher; }
	void set_author(const String &p_author) { author = p_author; }
	String get_author() const { return author; }
	void set_studio(const String &p_studio) { studio = p_studio; }
	String get_studio() const { return studio; }
	void set_website(const String &p_website) { website = p_website; }
	String get_website() const { return website; }
	void set_version(const String &p_version) { version = p_version; }
	String get_version() const { return version; }
	static void split_extra_spec(const String &p_spec, String &r_host, String &r_disc);
	static PackedStringArray split_extra_spec_bind(const String &p_spec);
	static String sanitize_volume_id(const String &p_id);
	static uint16_t language_be16(const String &p_lang);
	void set_first_play(const Ref<InterDVDPGC> &p_pgc) { first_play = p_pgc; }
	Ref<InterDVDPGC> get_first_play() const { return first_play; }
	void set_titles(const TypedArray<InterDVDPGC> &p_titles) { titles = p_titles; }
	TypedArray<InterDVDPGC> get_titles() const { return titles; }
	void set_menus(const TypedArray<InterDVDMenu> &p_menus) { menus = p_menus; }
	TypedArray<InterDVDMenu> get_menus() const { return menus; }
	Ref<InterDVDPGC> add_title_from_video(const String &p_path);
	TypedArray<InterDVDPGC> add_titles_from_videos(const PackedStringArray &p_paths);
	Ref<InterDVDCell> add_chapter_from_video(int p_title_n, const String &p_path);
	Ref<InterDVDPGC> add_title_from_scene(const Ref<PackedScene> &p_scene, double p_duration_sec = 4.0);
	Ref<InterDVDMenu> add_title_menu();
	Ref<InterDVDPGC> add_menu_title();
	Ref<InterDVDPGC> ensure_first_play(int p_title_n = 1);

private:
	void _notify_graph_changed();
};

namespace InterDVDSettings {
class ActiveProjectGuard {
	Ref<InterDVDProject> previous;

public:
	explicit ActiveProjectGuard(const Ref<InterDVDProject> &p_project);
	~ActiveProjectGuard();
};

void set_active_project(const Ref<InterDVDProject> &p_project);
Ref<InterDVDProject> active_project();
int setting_int(const char *p_name, int p_fallback);
int ac3_bitrate_k(const Ref<InterDVDProject> &p_project = Ref<InterDVDProject>());
int ac3_channels(const Ref<InterDVDProject> &p_project = Ref<InterDVDProject>());
int gop_size(const Ref<InterDVDProject> &p_project = Ref<InterDVDProject>());
int title_safe_bottom(const Ref<InterDVDProject> &p_project = Ref<InterDVDProject>());
int bake_warmup_frames(const Ref<InterDVDProject> &p_project = Ref<InterDVDProject>());
double pip_blackdetect_sec(const Ref<InterDVDProject> &p_project = Ref<InterDVDProject>());
double pip_blackdetect_pix_th(const Ref<InterDVDProject> &p_project = Ref<InterDVDProject>());
String cache_path();
} //namespace InterDVDSettings
