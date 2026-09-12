/**************************************************************************/
/*  inter_dvd_menu_page.cpp                                               */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#include "inter_dvd_menu_page.h"

#include "inter_dvd_disc.h"
#include "inter_dvd_hotspot.h"

#include "core/object/class_db.h"

void InterDVDMenuPage::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_menu_type", "type"), &InterDVDMenuPage::set_menu_type);
	ClassDB::bind_method(D_METHOD("get_menu_type"), &InterDVDMenuPage::get_menu_type);
	ClassDB::bind_method(D_METHOD("set_still_time", "seconds"), &InterDVDMenuPage::set_still_time);
	ClassDB::bind_method(D_METHOD("get_still_time"), &InterDVDMenuPage::get_still_time);
	ClassDB::bind_method(D_METHOD("set_source", "source"), &InterDVDMenuPage::set_source);
	ClassDB::bind_method(D_METHOD("get_source"), &InterDVDMenuPage::get_source);
	ClassDB::bind_method(D_METHOD("set_source_path", "path"), &InterDVDMenuPage::set_source_path);
	ClassDB::bind_method(D_METHOD("get_source_path"), &InterDVDMenuPage::get_source_path);
	ClassDB::bind_method(D_METHOD("set_packed_scene", "scene"), &InterDVDMenuPage::set_packed_scene);
	ClassDB::bind_method(D_METHOD("get_packed_scene"), &InterDVDMenuPage::get_packed_scene);
	ClassDB::bind_method(D_METHOD("set_duration_sec", "seconds"), &InterDVDMenuPage::set_duration_sec);
	ClassDB::bind_method(D_METHOD("get_duration_sec"), &InterDVDMenuPage::get_duration_sec);
	ClassDB::bind_method(D_METHOD("set_motion", "motion"), &InterDVDMenuPage::set_motion);
	ClassDB::bind_method(D_METHOD("is_motion"), &InterDVDMenuPage::is_motion);
	ClassDB::bind_method(D_METHOD("set_parental_id", "id"), &InterDVDMenuPage::set_parental_id);
	ClassDB::bind_method(D_METHOD("get_parental_id"), &InterDVDMenuPage::get_parental_id);
	ClassDB::bind_method(D_METHOD("set_uops", "uops"), &InterDVDMenuPage::set_uops);
	ClassDB::bind_method(D_METHOD("get_uops"), &InterDVDMenuPage::get_uops);
	ClassDB::bind_method(D_METHOD("set_goup_pgc", "pgc"), &InterDVDMenuPage::set_goup_pgc);
	ClassDB::bind_method(D_METHOD("get_goup_pgc"), &InterDVDMenuPage::get_goup_pgc);
	ClassDB::bind_method(D_METHOD("add_hotspot", "name"), &InterDVDMenuPage::add_hotspot, DEFVAL("Play"));
	ClassDB::bind_method(D_METHOD("compile_menu"), &InterDVDMenuPage::compile_menu);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "menu_type", PROPERTY_HINT_ENUM, "Title Menu:2,Root Menu:3,Subpicture Menu:4,Audio Menu:5,Angle Menu:6,Chapter Menu:7"), "set_menu_type", "get_menu_type");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "still_time", PROPERTY_HINT_RANGE, "0,255,1"), "set_still_time", "get_still_time");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "source", PROPERTY_HINT_ENUM, "Video,Scene"), "set_source", "get_source");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "source_path", PROPERTY_HINT_FILE, "*.mp4,*.mkv,*.mov,*.avi,*.webm,*.mpg,*.mpeg,*.vob,*.m2v"), "set_source_path", "get_source_path");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "packed_scene", PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"), "set_packed_scene", "get_packed_scene");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "duration_sec", PROPERTY_HINT_RANGE, "0,3600,0.1"), "set_duration_sec", "get_duration_sec");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "motion"), "set_motion", "is_motion");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "parental_id", PROPERTY_HINT_RANGE, "0,32767,1"), "set_parental_id", "get_parental_id");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "uops"), "set_uops", "get_uops");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "goup_pgc", PROPERTY_HINT_RANGE, "0,99,1"), "set_goup_pgc", "get_goup_pgc");
}

InterDVDMenuPage::InterDVDMenuPage() {
	set_custom_minimum_size(Size2(720, 480));
	set_size(Size2(720, 480));
}

void InterDVDMenuPage::set_source(InterDVDChapter::Source p_source) {
	if (source == p_source) {
		return;
	}
	source = p_source;
	notify_property_list_changed();
}

void InterDVDMenuPage::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == StringName("source_path") && source != InterDVDChapter::SOURCE_VIDEO) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
	if ((p_property.name == StringName("packed_scene") || p_property.name == StringName("duration_sec")) && source != InterDVDChapter::SOURCE_SCENE) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
}

InterDVDHotspot *InterDVDMenuPage::add_hotspot(const String &p_name) {
	InterDVDHotspot *spot = memnew(InterDVDHotspot);
	spot->set_name(InterDVDDisc::unique_child_name(this, p_name.is_empty() ? String("Play") : p_name));
	add_child(spot);
	return spot;
}

Ref<InterDVDMenu> InterDVDMenuPage::compile_menu() const {
	Ref<InterDVDMenu> menu;
	menu.instantiate();
	menu->set_name(get_name());
	menu->set_menu_type(menu_type);
	menu->set_still_time(still_time);
	menu->set_motion(motion);
	menu->set_parental_id(parental_id);
	menu->set_uops(uops);
	menu->set_goup_pgc(goup_pgc);
	Ref<InterDVDCell> cell;
	cell.instantiate();
	cell->set_name(get_name());
	InterDVDChapter::fill_source(cell, source, source_path, packed_scene, duration_sec);
	menu->set_cell(cell);
	TypedArray<InterDVDButton> buttons;
	for (int i = 0; i < get_child_count(); i++) {
		if (const InterDVDHotspot *spot = Object::cast_to<InterDVDHotspot>(get_child(i))) {
			buttons.push_back(spot->compile_button());
		}
	}
	menu->set_buttons(buttons);
	return menu;
}
