/**************************************************************************/
/*  inter_dvd_title_set.cpp                                               */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#include "inter_dvd_title_set.h"

#include "inter_dvd_disc.h"
#include "inter_dvd_menu_page.h"
#include "inter_dvd_title.h"

#include "core/object/class_db.h"

void InterDVDTitleSet::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_title", "name"), &InterDVDTitleSet::add_title, DEFVAL("Title"));
	ClassDB::bind_method(D_METHOD("add_root_menu", "name"), &InterDVDTitleSet::add_root_menu, DEFVAL("RootMenu"));
}

InterDVDTitle *InterDVDTitleSet::add_title(const String &p_name) {
	InterDVDTitle *title = memnew(InterDVDTitle);
	title->set_name(InterDVDDisc::unique_child_name(this, p_name.is_empty() ? String("Title") : p_name));
	add_child(title);
	return title;
}

InterDVDMenuPage *InterDVDTitleSet::add_root_menu(const String &p_name) {
	InterDVDMenuPage *menu = memnew(InterDVDMenuPage);
	menu->set_name(InterDVDDisc::unique_child_name(this, p_name.is_empty() ? String("RootMenu") : p_name));
	menu->set_menu_type(InterDVDMenu::MENU_ROOT);
	menu->set_still_time(0);
	add_child(menu);
	return menu;
}
