/**************************************************************************/
/*  inter_dvd_menu_page.h                                                 */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#pragma once

#include "modules/inter_dvd/author/inter_dvd_project.h"
#include "modules/inter_dvd/scene/inter_dvd_chapter.h"
#include "scene/gui/control.h"
#include "scene/resources/packed_scene.h"

class InterDVDHotspot;

class InterDVDMenuPage : public Control {
	GDCLASS(InterDVDMenuPage, Control);

	InterDVDMenu::MenuType menu_type = InterDVDMenu::MENU_TITLE;
	int still_time = 0;
	InterDVDChapter::Source source = InterDVDChapter::SOURCE_SCENE;
	String source_path;
	Ref<PackedScene> packed_scene;
	double duration_sec = 4.0;
	bool motion = false;
	int parental_id = 0;
	int uops = 0;
	int goup_pgc = 0;

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	InterDVDMenuPage();
	void set_menu_type(InterDVDMenu::MenuType p_type) { menu_type = p_type; }
	InterDVDMenu::MenuType get_menu_type() const { return menu_type; }
	void set_still_time(int p_sec) { still_time = p_sec; }
	int get_still_time() const { return still_time; }
	void set_source(InterDVDChapter::Source p_source);
	InterDVDChapter::Source get_source() const { return source; }
	void set_source_path(const String &p_path) { source_path = p_path; }
	String get_source_path() const { return source_path; }
	void set_packed_scene(const Ref<PackedScene> &p_scene) { packed_scene = p_scene; }
	Ref<PackedScene> get_packed_scene() const { return packed_scene; }
	void set_duration_sec(double p_sec) { duration_sec = p_sec; }
	double get_duration_sec() const { return duration_sec; }
	void set_motion(bool p_motion) { motion = p_motion; }
	bool is_motion() const { return motion; }
	void set_parental_id(int p_id) { parental_id = p_id; }
	int get_parental_id() const { return parental_id; }
	void set_uops(int p_uops) { uops = p_uops; }
	int get_uops() const { return uops; }
	void set_goup_pgc(int p_pgc) { goup_pgc = p_pgc; }
	int get_goup_pgc() const { return goup_pgc; }
	InterDVDHotspot *add_hotspot(const String &p_name = "Play");
	Ref<InterDVDMenu> compile_menu() const;
};
