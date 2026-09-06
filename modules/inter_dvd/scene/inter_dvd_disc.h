/**************************************************************************/
/*  inter_dvd_disc.h                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#pragma once

#include "modules/inter_dvd/author/inter_dvd_project.h"
#include "scene/main/node.h"

class InterDVDTitle;
class InterDVDMenuPage;
class InterDVDChapter;
class InterDVDTitleSet;

class InterDVDDisc : public Node {
	GDCLASS(InterDVDDisc, Node);

public:
	enum FirstPlay {
		FIRST_PLAY_TITLE_MENU = 0,
		FIRST_PLAY_MAIN_TITLE = 1,
		FIRST_PLAY_PATH = 2,
	};

private:
	String disc_title;
	String volume_id = "BLAZIUM_DVD";
	String serial;
	String provider_id = "BLAZIUM INTER-DVD";
	int region_mask = 1;
	int parental_level = 1;
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
	FirstPlay first_play = FIRST_PLAY_TITLE_MENU;
	NodePath first_play_path;

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	void set_disc_title(const String &p_title) { disc_title = p_title; }
	String get_disc_title() const { return disc_title; }
	void set_volume_id(const String &p_id) { volume_id = p_id; }
	String get_volume_id() const { return volume_id; }
	void set_serial(const String &p_serial) { serial = p_serial; }
	String get_serial() const { return serial; }
	void set_provider_id(const String &p_id) { provider_id = p_id; }
	String get_provider_id() const { return provider_id; }
	void set_region_mask(int p_mask) { region_mask = p_mask; }
	int get_region_mask() const { return region_mask; }
	void set_parental_level(int p_level) { parental_level = p_level; }
	int get_parental_level() const { return parental_level; }
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
	void set_title_safe_bottom(int p_y) { title_safe_bottom = p_y; }
	int get_title_safe_bottom() const { return title_safe_bottom; }
	void set_bake_warmup_frames(int p_frames) { bake_warmup_frames = p_frames; }
	int get_bake_warmup_frames() const { return bake_warmup_frames; }
	void set_pip_blackdetect_sec(double p_sec) { pip_blackdetect_sec = p_sec; }
	double get_pip_blackdetect_sec() const { return pip_blackdetect_sec; }
	void set_pip_blackdetect_pix_th(double p_th) { pip_blackdetect_pix_th = p_th; }
	double get_pip_blackdetect_pix_th() const { return pip_blackdetect_pix_th; }
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
	void set_first_play(FirstPlay p_play);
	FirstPlay get_first_play() const { return first_play; }
	void set_first_play_path(const NodePath &p_path) { first_play_path = p_path; }
	NodePath get_first_play_path() const { return first_play_path; }

	void apply_settings_to(const Ref<InterDVDProject> &p_project) const;
	InterDVDTitle *add_title(const String &p_name = "Title");
	InterDVDChapter *add_chapter_to_last(const String &p_name = "Chapter");
	InterDVDMenuPage *add_title_menu(const String &p_name = "TitleMenu");
	InterDVDMenuPage *add_root_menu(const String &p_name = "RootMenu");
	InterDVDTitleSet *add_title_set(const String &p_name = "TitleSet");
	InterDVDTitle *add_menu_title(const String &p_name = "Menu");
	void apply_scene_owner(Node *p_owner);
	Ref<InterDVDProject> build_project() const;
	static InterDVDDisc *create_starter();
	static InterDVDDisc *find_in_tree(Node *p_root);
	static String unique_child_name(Node *p_parent, const String &p_base);
};

VARIANT_ENUM_CAST(InterDVDDisc::FirstPlay);
