/**************************************************************************/
/*  inter_dvd_title.h                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#pragma once

#include "modules/inter_dvd/author/inter_dvd_project.h"
#include "scene/gui/control.h"

class InterDVDChapter;
class InterDVDHotspot;

class InterDVDTitle : public Control {
	GDCLASS(InterDVDTitle, Control);

	bool menu_title = false;
	int still_time = 0;
	int parental_id = 0;
	int uops = 0;
	int goup_pgc = 0;

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	InterDVDTitle();
	void set_menu_title(bool p_menu);
	bool is_menu_title() const { return menu_title; }
	void set_still_time(int p_sec) { still_time = p_sec; }
	int get_still_time() const { return still_time; }
	void set_parental_id(int p_id) { parental_id = p_id; }
	int get_parental_id() const { return parental_id; }
	void set_uops(int p_uops) { uops = p_uops; }
	int get_uops() const { return uops; }
	void set_goup_pgc(int p_pgc) { goup_pgc = p_pgc; }
	int get_goup_pgc() const { return goup_pgc; }
	InterDVDChapter *add_chapter(const String &p_name = "Chapter");
	InterDVDHotspot *add_hotspot(const String &p_name = "Play");
	InterDVDHotspot *add_menu_hotspot(const String &p_name = "Menu");
	InterDVDHotspot *add_resume_hotspot(const String &p_name = "Resume");
	Ref<InterDVDPGC> compile_pgc() const;
};
