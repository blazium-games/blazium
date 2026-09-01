/**************************************************************************/
/*  inter_dvd_title.cpp                                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#include "inter_dvd_title.h"

#include "inter_dvd_chapter.h"
#include "inter_dvd_disc.h"
#include "inter_dvd_hotspot.h"
#include "inter_dvd_menu_page.h"
#include "inter_dvd_title_set.h"

#include "core/object/class_db.h"

void InterDVDTitle::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_menu_title", "menu_title"), &InterDVDTitle::set_menu_title);
	ClassDB::bind_method(D_METHOD("is_menu_title"), &InterDVDTitle::is_menu_title);
	ClassDB::bind_method(D_METHOD("set_still_time", "seconds"), &InterDVDTitle::set_still_time);
	ClassDB::bind_method(D_METHOD("get_still_time"), &InterDVDTitle::get_still_time);
	ClassDB::bind_method(D_METHOD("set_parental_id", "id"), &InterDVDTitle::set_parental_id);
	ClassDB::bind_method(D_METHOD("get_parental_id"), &InterDVDTitle::get_parental_id);
	ClassDB::bind_method(D_METHOD("set_uops", "uops"), &InterDVDTitle::set_uops);
	ClassDB::bind_method(D_METHOD("get_uops"), &InterDVDTitle::get_uops);
	ClassDB::bind_method(D_METHOD("set_goup_pgc", "pgc"), &InterDVDTitle::set_goup_pgc);
	ClassDB::bind_method(D_METHOD("get_goup_pgc"), &InterDVDTitle::get_goup_pgc);
	ClassDB::bind_method(D_METHOD("add_chapter", "name"), &InterDVDTitle::add_chapter, DEFVAL("Chapter"));
	ClassDB::bind_method(D_METHOD("add_hotspot", "name"), &InterDVDTitle::add_hotspot, DEFVAL("Play"));
	ClassDB::bind_method(D_METHOD("add_menu_hotspot", "name"), &InterDVDTitle::add_menu_hotspot, DEFVAL("Menu"));
	ClassDB::bind_method(D_METHOD("add_resume_hotspot", "name"), &InterDVDTitle::add_resume_hotspot, DEFVAL("Resume"));
	ClassDB::bind_method(D_METHOD("compile_pgc"), &InterDVDTitle::compile_pgc);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "menu_title"), "set_menu_title", "is_menu_title");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "still_time", PROPERTY_HINT_RANGE, "0,255,1"), "set_still_time", "get_still_time");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "parental_id", PROPERTY_HINT_RANGE, "0,32767,1"), "set_parental_id", "get_parental_id");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "uops"), "set_uops", "get_uops");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "goup_pgc", PROPERTY_HINT_RANGE, "0,99,1"), "set_goup_pgc", "get_goup_pgc");
}

InterDVDTitle::InterDVDTitle() {
	set_custom_minimum_size(Size2(720, 480));
	set_size(Size2(720, 480));
}

void InterDVDTitle::_validate_property(PropertyInfo &p_property) const {
	(void)p_property;
}

void InterDVDTitle::set_menu_title(bool p_menu) {
	menu_title = p_menu;
}

InterDVDChapter *InterDVDTitle::add_chapter(const String &p_name) {
	InterDVDChapter *chapter = memnew(InterDVDChapter);
	chapter->set_name(InterDVDDisc::unique_child_name(this, p_name.is_empty() ? String("Chapter") : p_name));
	add_child(chapter);
	return chapter;
}

InterDVDHotspot *InterDVDTitle::add_hotspot(const String &p_name) {
	InterDVDHotspot *spot = memnew(InterDVDHotspot);
	spot->set_name(InterDVDDisc::unique_child_name(this, p_name.is_empty() ? String("Play") : p_name));
	add_child(spot);
	return spot;
}

InterDVDHotspot *InterDVDTitle::add_menu_hotspot(const String &p_name) {
	InterDVDHotspot *spot = add_hotspot(p_name.is_empty() ? String("Menu") : p_name);
	spot->set_action(InterDVDButton::ACTION_JUMP_TITLE);
	Node *disc = get_parent();
	while (disc && !Object::cast_to<InterDVDDisc>(disc)) {
		disc = disc->get_parent();
	}
	if (disc) {
		InterDVDTitle *menu_pgc = nullptr;
		InterDVDMenuPage *title_page = nullptr;
		for (int i = 0; i < disc->get_child_count(); i++) {
			if (InterDVDTitle *title = Object::cast_to<InterDVDTitle>(disc->get_child(i))) {
				if (title->is_menu_title() && !menu_pgc) {
					menu_pgc = title;
				}
			} else if (InterDVDTitleSet *set = Object::cast_to<InterDVDTitleSet>(disc->get_child(i))) {
				for (int t = 0; t < set->get_child_count(); t++) {
					if (InterDVDTitle *title = Object::cast_to<InterDVDTitle>(set->get_child(t))) {
						if (title->is_menu_title() && !menu_pgc) {
							menu_pgc = title;
						}
					}
				}
			} else if (InterDVDMenuPage *page = Object::cast_to<InterDVDMenuPage>(disc->get_child(i))) {
				if (page->get_menu_type() == InterDVDMenu::MENU_TITLE && !title_page) {
					title_page = page;
				}
			}
		}
		if (menu_pgc) {
			spot->set_destination(spot->get_path_to(menu_pgc));
			return spot;
		}
		if (title_page) {
			spot->set_destination(spot->get_path_to(title_page));
			return spot;
		}
	}
	return spot;
}

InterDVDHotspot *InterDVDTitle::add_resume_hotspot(const String &p_name) {
	InterDVDHotspot *spot = add_hotspot(p_name.is_empty() ? String("Resume") : p_name);
	spot->set_action(InterDVDButton::ACTION_RESUME);
	spot->set_destination(NodePath());
	return spot;
}

Ref<InterDVDPGC> InterDVDTitle::compile_pgc() const {
	Ref<InterDVDPGC> pgc;
	pgc.instantiate();
	pgc->set_name(get_name());
	pgc->set_still_time(still_time);
	pgc->set_parental_id(parental_id);
	pgc->set_uops(uops);
	pgc->set_goup_pgc(goup_pgc);
	TypedArray<InterDVDCell> cells;
	TypedArray<InterDVDButton> buttons;
	for (int i = 0; i < get_child_count(); i++) {
		if (const InterDVDChapter *chapter = Object::cast_to<InterDVDChapter>(get_child(i))) {
			cells.push_back(chapter->compile_cell());
		} else if (const InterDVDHotspot *spot = Object::cast_to<InterDVDHotspot>(get_child(i))) {
			buttons.push_back(spot->compile_button());
		}
	}
	pgc->set_cells(cells);
	pgc->set_buttons(buttons);
	return pgc;
}
