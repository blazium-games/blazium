/**************************************************************************/
/*  inter_dvd_hotspot.h                                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#pragma once

#include "modules/inter_dvd/author/inter_dvd_project.h"
#include "scene/gui/control.h"

class InterDVDHotspot : public Control {
	GDCLASS(InterDVDHotspot, Control);

	InterDVDButton::Action action = InterDVDButton::ACTION_JUMP_TITLE;
	NodePath destination;
	int target = 1;
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
	Color select_color = Color(1, 0.92, 0.2, 1);
	Color action_color = Color(1, 0.35, 0.1, 1);
	PackedByteArray command;

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	InterDVDHotspot();
	void set_action(InterDVDButton::Action p_action);
	InterDVDButton::Action get_action() const { return action; }
	void set_destination(const NodePath &p_path) { destination = p_path; }
	NodePath get_destination() const { return destination; }
	void set_target(int p_target) { target = p_target; }
	int get_target() const { return target; }
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
	void set_select_color(const Color &p_color) { select_color = p_color; }
	Color get_select_color() const { return select_color; }
	void set_action_color(const Color &p_color) { action_color = p_color; }
	Color get_action_color() const { return action_color; }
	void set_command(const PackedByteArray &p_cmd) { command = p_cmd; }
	PackedByteArray get_command() const { return command; }
	Ref<InterDVDButton> compile_button() const;
};
