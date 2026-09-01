/**************************************************************************/
/*  inter_dvd_hotspot.cpp                                                 */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#include "inter_dvd_hotspot.h"

#include "core/object/class_db.h"

void InterDVDHotspot::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_action", "action"), &InterDVDHotspot::set_action);
	ClassDB::bind_method(D_METHOD("get_action"), &InterDVDHotspot::get_action);
	ClassDB::bind_method(D_METHOD("set_destination", "path"), &InterDVDHotspot::set_destination);
	ClassDB::bind_method(D_METHOD("get_destination"), &InterDVDHotspot::get_destination);
	ClassDB::bind_method(D_METHOD("set_target", "target"), &InterDVDHotspot::set_target);
	ClassDB::bind_method(D_METHOD("get_target"), &InterDVDHotspot::get_target);
	ClassDB::bind_method(D_METHOD("set_stream", "stream"), &InterDVDHotspot::set_stream);
	ClassDB::bind_method(D_METHOD("get_stream"), &InterDVDHotspot::get_stream);
	ClassDB::bind_method(D_METHOD("set_subtitle_on", "on"), &InterDVDHotspot::set_subtitle_on);
	ClassDB::bind_method(D_METHOD("get_subtitle_on"), &InterDVDHotspot::get_subtitle_on);
	ClassDB::bind_method(D_METHOD("set_auto_action", "auto"), &InterDVDHotspot::set_auto_action);
	ClassDB::bind_method(D_METHOD("get_auto_action"), &InterDVDHotspot::get_auto_action);
	ClassDB::bind_method(D_METHOD("set_hidden", "hidden"), &InterDVDHotspot::set_hidden);
	ClassDB::bind_method(D_METHOD("is_hidden"), &InterDVDHotspot::is_hidden);
	ClassDB::bind_method(D_METHOD("set_forced_selected", "forced"), &InterDVDHotspot::set_forced_selected);
	ClassDB::bind_method(D_METHOD("is_forced_selected"), &InterDVDHotspot::is_forced_selected);
	ClassDB::bind_method(D_METHOD("set_forced_activated", "forced"), &InterDVDHotspot::set_forced_activated);
	ClassDB::bind_method(D_METHOD("is_forced_activated"), &InterDVDHotspot::is_forced_activated);
	ClassDB::bind_method(D_METHOD("set_numeric_select", "numeric"), &InterDVDHotspot::set_numeric_select);
	ClassDB::bind_method(D_METHOD("get_numeric_select"), &InterDVDHotspot::get_numeric_select);
	ClassDB::bind_method(D_METHOD("set_adjacent_up", "button"), &InterDVDHotspot::set_adjacent_up);
	ClassDB::bind_method(D_METHOD("get_adjacent_up"), &InterDVDHotspot::get_adjacent_up);
	ClassDB::bind_method(D_METHOD("set_adjacent_down", "button"), &InterDVDHotspot::set_adjacent_down);
	ClassDB::bind_method(D_METHOD("get_adjacent_down"), &InterDVDHotspot::get_adjacent_down);
	ClassDB::bind_method(D_METHOD("set_adjacent_left", "button"), &InterDVDHotspot::set_adjacent_left);
	ClassDB::bind_method(D_METHOD("get_adjacent_left"), &InterDVDHotspot::get_adjacent_left);
	ClassDB::bind_method(D_METHOD("set_adjacent_right", "button"), &InterDVDHotspot::set_adjacent_right);
	ClassDB::bind_method(D_METHOD("get_adjacent_right"), &InterDVDHotspot::get_adjacent_right);
	ClassDB::bind_method(D_METHOD("set_select_color", "color"), &InterDVDHotspot::set_select_color);
	ClassDB::bind_method(D_METHOD("get_select_color"), &InterDVDHotspot::get_select_color);
	ClassDB::bind_method(D_METHOD("set_action_color", "color"), &InterDVDHotspot::set_action_color);
	ClassDB::bind_method(D_METHOD("get_action_color"), &InterDVDHotspot::get_action_color);
	ClassDB::bind_method(D_METHOD("set_command", "command"), &InterDVDHotspot::set_command);
	ClassDB::bind_method(D_METHOD("get_command"), &InterDVDHotspot::get_command);
	ClassDB::bind_method(D_METHOD("compile_button"), &InterDVDHotspot::compile_button);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "action", PROPERTY_HINT_ENUM, "Jump Title,Jump Menu,Jump PGC,Custom,Jump Chapter,Jump Program,Jump Cell,Resume,Set Audio,Set Subtitle,Set Angle,Highlight Button,Exit"), "set_action", "get_action");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "destination", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "InterDVDTitle,InterDVDMenuPage,InterDVDChapter"), "set_destination", "get_destination");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "target", PROPERTY_HINT_RANGE, "1,99,1"), "set_target", "get_target");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "stream", PROPERTY_HINT_RANGE, "0,31,1"), "set_stream", "get_stream");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "subtitle_on"), "set_subtitle_on", "get_subtitle_on");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "command"), "set_command", "get_command");
	ADD_GROUP("Highlight", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_action"), "set_auto_action", "get_auto_action");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "hidden"), "set_hidden", "is_hidden");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "forced_selected"), "set_forced_selected", "is_forced_selected");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "forced_activated"), "set_forced_activated", "is_forced_activated");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "numeric_select"), "set_numeric_select", "get_numeric_select");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "adjacent_up", PROPERTY_HINT_RANGE, "0,36,1"), "set_adjacent_up", "get_adjacent_up");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "adjacent_down", PROPERTY_HINT_RANGE, "0,36,1"), "set_adjacent_down", "get_adjacent_down");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "adjacent_left", PROPERTY_HINT_RANGE, "0,36,1"), "set_adjacent_left", "get_adjacent_left");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "adjacent_right", PROPERTY_HINT_RANGE, "0,36,1"), "set_adjacent_right", "get_adjacent_right");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "select_color"), "set_select_color", "get_select_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "action_color"), "set_action_color", "get_action_color");
}

void InterDVDHotspot::_validate_property(PropertyInfo &p_property) const {
	const bool uses_dest = action == InterDVDButton::ACTION_JUMP_TITLE || action == InterDVDButton::ACTION_JUMP_CHAPTER || action == InterDVDButton::ACTION_JUMP_MENU || action == InterDVDButton::ACTION_JUMP_PGC;
	const bool uses_target = action == InterDVDButton::ACTION_JUMP_CHAPTER || action == InterDVDButton::ACTION_JUMP_PROGRAM || action == InterDVDButton::ACTION_JUMP_CELL || action == InterDVDButton::ACTION_JUMP_PGC || action == InterDVDButton::ACTION_JUMP_MENU || action == InterDVDButton::ACTION_HIGHLIGHT_BUTTON;
	const bool uses_stream = action == InterDVDButton::ACTION_SET_AUDIO || action == InterDVDButton::ACTION_SET_SUBTITLE || action == InterDVDButton::ACTION_SET_ANGLE;
	if (p_property.name == StringName("destination") && !uses_dest) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
	if (p_property.name == StringName("target")) {
		if (!uses_target) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		} else if (action == InterDVDButton::ACTION_JUMP_MENU) {
			p_property.hint = PROPERTY_HINT_ENUM;
			p_property.hint_string = "Title Menu:2,Root Menu:3,Subpicture Menu:4,Audio Menu:5,Angle Menu:6,Chapter Menu:7";
		} else if (action == InterDVDButton::ACTION_HIGHLIGHT_BUTTON) {
			p_property.hint = PROPERTY_HINT_RANGE;
			p_property.hint_string = "1,36,1";
		}
	}
	if (p_property.name == StringName("stream") && !uses_stream) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
	if (p_property.name == StringName("subtitle_on") && action != InterDVDButton::ACTION_SET_SUBTITLE) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
	if (p_property.name == StringName("command") && action != InterDVDButton::ACTION_CUSTOM) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
}

InterDVDHotspot::InterDVDHotspot() {
	set_position(Vector2(80, 200));
	set_size(Vector2(240, 60));
}

void InterDVDHotspot::set_action(InterDVDButton::Action p_action) {
	if (action == p_action) {
		return;
	}
	action = p_action;
	if (action == InterDVDButton::ACTION_JUMP_MENU && target < 2) {
		target = int(InterDVDMenu::MENU_TITLE);
	}
	notify_property_list_changed();
}

Ref<InterDVDButton> InterDVDHotspot::compile_button() const {
	Ref<InterDVDButton> btn;
	btn.instantiate();
	btn->set_name(get_name());
	btn->set_action(action);
	btn->set_highlight(get_rect());
	btn->set_control_path(NodePath(get_name()));
	int resolved_target = target;
	if (action == InterDVDButton::ACTION_JUMP_MENU && resolved_target < 2) {
		resolved_target = int(InterDVDMenu::MENU_TITLE);
	}
	btn->set_target(resolved_target);
	btn->set_stream(stream);
	btn->set_subtitle_on(subtitle_on);
	btn->set_auto_action(auto_action);
	btn->set_hidden(hidden);
	btn->set_forced_selected(forced_selected);
	btn->set_forced_activated(forced_activated);
	btn->set_numeric_select(numeric_select);
	btn->set_adjacent_up(adjacent_up);
	btn->set_adjacent_down(adjacent_down);
	btn->set_adjacent_left(adjacent_left);
	btn->set_adjacent_right(adjacent_right);
	btn->set_select_color(select_color);
	btn->set_action_color(action_color);
	if (action == InterDVDButton::ACTION_CUSTOM) {
		btn->set_command(command);
	}
	return btn;
}
