/**************************************************************************/
/*  inter_dvd_project.cpp                                                 */
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

#include "inter_dvd_project.h"

#include "modules/inter_dvd/machine/inter_dvd_instruction.h"

#include "core/config/project_settings.h"
#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "scene/2d/node_2d.h"
#include "scene/gui/control.h"
#include "scene/main/canvas_item.h"
#include "scene/main/node.h"

void InterDVDStream::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_kind", "kind"), &InterDVDStream::set_kind);
	ClassDB::bind_method(D_METHOD("get_kind"), &InterDVDStream::get_kind);
	ClassDB::bind_method(D_METHOD("set_source_path", "path"), &InterDVDStream::set_source_path);
	ClassDB::bind_method(D_METHOD("get_source_path"), &InterDVDStream::get_source_path);
	ClassDB::bind_method(D_METHOD("set_language", "language"), &InterDVDStream::set_language);
	ClassDB::bind_method(D_METHOD("get_language"), &InterDVDStream::get_language);
	ClassDB::bind_method(D_METHOD("set_code_extension", "extension"), &InterDVDStream::set_code_extension);
	ClassDB::bind_method(D_METHOD("get_code_extension"), &InterDVDStream::get_code_extension);
	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &InterDVDStream::set_enabled);
	ClassDB::bind_method(D_METHOD("is_enabled"), &InterDVDStream::is_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "kind", PROPERTY_HINT_ENUM, "Audio,Subtitle"), "set_kind", "get_kind");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "source_path", PROPERTY_HINT_FILE, "*.wav,*.mp3,*.ogg,*.flac,*.m4a,*.aac,*.ac3,*.srt,*.sup,*.sub,*.idx"), "set_source_path", "get_source_path");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "language"), "set_language", "get_language");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "code_extension", PROPERTY_HINT_RANGE, "0,15,1"), "set_code_extension", "get_code_extension");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "is_enabled");
	BIND_ENUM_CONSTANT(KIND_AUDIO);
	BIND_ENUM_CONSTANT(KIND_SUBTITLE);
}

void InterDVDCell::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_encoded_path", "path"), &InterDVDCell::set_encoded_path);
	ClassDB::bind_method(D_METHOD("get_encoded_path"), &InterDVDCell::get_encoded_path);
	ClassDB::bind_method(D_METHOD("set_source_path", "path"), &InterDVDCell::set_source_path);
	ClassDB::bind_method(D_METHOD("get_source_path"), &InterDVDCell::get_source_path);
	ClassDB::bind_method(D_METHOD("set_pip_source_path", "path"), &InterDVDCell::set_pip_source_path);
	ClassDB::bind_method(D_METHOD("get_pip_source_path"), &InterDVDCell::get_pip_source_path);
	ClassDB::bind_method(D_METHOD("set_audio_path", "path"), &InterDVDCell::set_audio_path);
	ClassDB::bind_method(D_METHOD("get_audio_path"), &InterDVDCell::get_audio_path);
	ClassDB::bind_method(D_METHOD("set_pip_slot_path", "path"), &InterDVDCell::set_pip_slot_path);
	ClassDB::bind_method(D_METHOD("get_pip_slot_path"), &InterDVDCell::get_pip_slot_path);
	ClassDB::bind_method(D_METHOD("set_pip_rect", "rect"), &InterDVDCell::set_pip_rect);
	ClassDB::bind_method(D_METHOD("get_pip_rect"), &InterDVDCell::get_pip_rect);
	ClassDB::bind_method(D_METHOD("set_pip_lead_sec", "seconds"), &InterDVDCell::set_pip_lead_sec);
	ClassDB::bind_method(D_METHOD("get_pip_lead_sec"), &InterDVDCell::get_pip_lead_sec);
	ClassDB::bind_method(D_METHOD("set_bake_hold_sec", "seconds"), &InterDVDCell::set_bake_hold_sec);
	ClassDB::bind_method(D_METHOD("get_bake_hold_sec"), &InterDVDCell::get_bake_hold_sec);
	ClassDB::bind_method(D_METHOD("set_bake_camera_path", "path"), &InterDVDCell::set_bake_camera_path);
	ClassDB::bind_method(D_METHOD("get_bake_camera_path"), &InterDVDCell::get_bake_camera_path);
	ClassDB::bind_method(D_METHOD("set_default_highlight", "rect"), &InterDVDCell::set_default_highlight);
	ClassDB::bind_method(D_METHOD("get_default_highlight"), &InterDVDCell::get_default_highlight);
	ClassDB::bind_method(D_METHOD("set_packed_scene", "scene"), &InterDVDCell::set_packed_scene);
	ClassDB::bind_method(D_METHOD("get_packed_scene"), &InterDVDCell::get_packed_scene);
	ClassDB::bind_method(D_METHOD("set_duration_sec", "seconds"), &InterDVDCell::set_duration_sec);
	ClassDB::bind_method(D_METHOD("get_duration_sec"), &InterDVDCell::get_duration_sec);
	ClassDB::bind_method(D_METHOD("set_loop_pad_sec", "seconds"), &InterDVDCell::set_loop_pad_sec);
	ClassDB::bind_method(D_METHOD("get_loop_pad_sec"), &InterDVDCell::get_loop_pad_sec);
	ClassDB::bind_method(D_METHOD("set_include_audio", "include"), &InterDVDCell::set_include_audio);
	ClassDB::bind_method(D_METHOD("get_include_audio"), &InterDVDCell::get_include_audio);
	ClassDB::bind_method(D_METHOD("set_streams", "streams"), &InterDVDCell::set_streams);
	ClassDB::bind_method(D_METHOD("get_streams"), &InterDVDCell::get_streams);
	ClassDB::bind_method(D_METHOD("set_post_commands", "commands"), &InterDVDCell::set_post_commands);
	ClassDB::bind_method(D_METHOD("get_post_commands"), &InterDVDCell::get_post_commands);
	ClassDB::bind_method(D_METHOD("set_angle", "angle"), &InterDVDCell::set_angle);
	ClassDB::bind_method(D_METHOD("get_angle"), &InterDVDCell::get_angle);
	ClassDB::bind_method(D_METHOD("get_display_name"), &InterDVDCell::get_display_name);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "encoded_path", PROPERTY_HINT_FILE, "*.vob,*.m2v,*.mpg,*.mpeg"), "set_encoded_path", "get_encoded_path");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "source_path", PROPERTY_HINT_FILE, "*.mp4,*.mkv,*.mov,*.avi,*.webm,*.mpg,*.mpeg,*.vob,*.m2v"), "set_source_path", "get_source_path");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "pip_source_path", PROPERTY_HINT_FILE, "*.mp4,*.mkv,*.mov,*.avi,*.webm,*.mpg,*.mpeg,*.vob"), "set_pip_source_path", "get_pip_source_path");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "audio_path", PROPERTY_HINT_FILE, "*.wav,*.mp3,*.ogg,*.flac,*.m4a,*.aac,*.ac3,*.aif,*.aiff"), "set_audio_path", "get_audio_path");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "pip_slot_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Control,Node2D"), "set_pip_slot_path", "get_pip_slot_path");
	ADD_PROPERTY(PropertyInfo(Variant::RECT2, "pip_rect"), "set_pip_rect", "get_pip_rect");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pip_lead_sec", PROPERTY_HINT_RANGE, "0,5,0.01"), "set_pip_lead_sec", "get_pip_lead_sec");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bake_hold_sec", PROPERTY_HINT_RANGE, "0,60,0.1"), "set_bake_hold_sec", "get_bake_hold_sec");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "bake_camera_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Camera2D,Camera3D"), "set_bake_camera_path", "get_bake_camera_path");
	ADD_PROPERTY(PropertyInfo(Variant::RECT2, "default_highlight"), "set_default_highlight", "get_default_highlight");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "packed_scene", PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"), "set_packed_scene", "get_packed_scene");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "duration_sec", PROPERTY_HINT_RANGE, "0,3600,0.1"), "set_duration_sec", "get_duration_sec");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "loop_pad_sec", PROPERTY_HINT_RANGE, "0,600,0.1"), "set_loop_pad_sec", "get_loop_pad_sec");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "include_audio"), "set_include_audio", "get_include_audio");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "streams", PROPERTY_HINT_ARRAY_TYPE, "InterDVDStream"), "set_streams", "get_streams");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "post_commands", PROPERTY_HINT_ARRAY_TYPE, "PackedByteArray"), "set_post_commands", "get_post_commands");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "angle", PROPERTY_HINT_RANGE, "1,9,1"), "set_angle", "get_angle");
}

String InterDVDCell::get_display_name() const {
	if (!get_name().is_empty()) {
		return get_name();
	}
	if (!source_path.is_empty()) {
		return source_path.get_file();
	}
	if (!encoded_path.is_empty()) {
		return encoded_path.get_file();
	}
	return String();
}

void InterDVDButton::set_action(Action p_action) {
	if (action == p_action) {
		return;
	}
	if (p_action == ACTION_JUMP_TITLE && action == ACTION_JUMP_CHAPTER) {
		target = title_n;
	}
	action = p_action;
	notify_property_list_changed();
}

void InterDVDButton::set_target(int p_target) {
	if (target == p_target) {
		return;
	}
	target = p_target;
	if (action == ACTION_JUMP_TITLE) {
		title_n = p_target;
	}
}

void InterDVDButton::set_title_n(int p_title) {
	if (action == ACTION_JUMP_TITLE) {
		if (target == p_title && title_n == p_title) {
			return;
		}
		target = p_title;
		title_n = p_title;
		notify_property_list_changed();
		return;
	}
	if (title_n == p_title) {
		return;
	}
	title_n = p_title;
	notify_property_list_changed();
}

int InterDVDButton::get_title_n() const {
	// Jump Title stores the destination in `target` on existing resources.
	return action == ACTION_JUMP_TITLE ? target : title_n;
}

void InterDVDButton::_validate_property(PropertyInfo &p_property) const {
	const bool uses_title = action == ACTION_JUMP_TITLE || action == ACTION_JUMP_CHAPTER;
	const bool uses_target = action == ACTION_JUMP_MENU || action == ACTION_JUMP_PGC || action == ACTION_JUMP_CHAPTER || action == ACTION_JUMP_PROGRAM || action == ACTION_JUMP_CELL || action == ACTION_HIGHLIGHT_BUTTON;
	const bool uses_stream = action == ACTION_SET_AUDIO || action == ACTION_SET_SUBTITLE || action == ACTION_SET_ANGLE;
	if (p_property.name == StringName("command")) {
		if (action != ACTION_CUSTOM) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		}
		return;
	}
	if (p_property.name == StringName("target")) {
		if (!uses_target) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
			return;
		}
		if (action == ACTION_JUMP_MENU) {
			p_property.hint = PROPERTY_HINT_ENUM;
			p_property.hint_string = "Title Menu:2,Root Menu:3,Subpicture Menu:4,Audio Menu:5,Angle Menu:6,Chapter Menu:7";
		} else if (action == ACTION_HIGHLIGHT_BUTTON) {
			p_property.hint = PROPERTY_HINT_RANGE;
			p_property.hint_string = "1,36,1";
		} else {
			p_property.hint = PROPERTY_HINT_RANGE;
			p_property.hint_string = "1,99,1";
		}
		return;
	}
	if (p_property.name == StringName("title_n")) {
		if (!uses_title) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		}
		return;
	}
	if (p_property.name == StringName("stream")) {
		if (!uses_stream) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		}
		return;
	}
	if (p_property.name == StringName("subtitle_on") && action != ACTION_SET_SUBTITLE) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
}

PackedByteArray InterDVDButton::resolve_command(CommandDomain p_domain) const {
	if (action == ACTION_CUSTOM && command.size() == 8) {
		return command;
	}
	const InterDVDInstruction::Domain from = (p_domain == DOMAIN_VTST) ? InterDVDInstruction::DOMAIN_VTST : InterDVDInstruction::DOMAIN_VMG;
	const int dest = target > 0 ? target : 1;
	switch (action) {
		case ACTION_JUMP_PGC:
			return InterDVDInstruction::encode_link(InterDVDInstruction::LINK_PGCN, from, from, dest, nullptr);
		case ACTION_JUMP_TITLE:
			if (p_domain == DOMAIN_VTST) {
				return InterDVDInstruction::encode_link(InterDVDInstruction::JUMP_VTS_TT, InterDVDInstruction::DOMAIN_VTST, InterDVDInstruction::DOMAIN_VTST, dest, nullptr);
			}
			return InterDVDInstruction::encode_link(InterDVDInstruction::JUMP_TT, InterDVDInstruction::DOMAIN_VMG, InterDVDInstruction::DOMAIN_VTST, dest, nullptr);
		case ACTION_JUMP_MENU:
			if (p_domain == DOMAIN_VTST) {
				return InterDVDInstruction::encode_link(InterDVDInstruction::CALL_SS, InterDVDInstruction::DOMAIN_VTST, InterDVDInstruction::DOMAIN_VMG, dest, nullptr);
			}
			return InterDVDInstruction::encode_link(InterDVDInstruction::JUMP_SS, InterDVDInstruction::DOMAIN_VMG, InterDVDInstruction::DOMAIN_VMG, dest, nullptr);
		case ACTION_JUMP_CHAPTER:

			return InterDVDInstruction::encode_jump_vts_ptt(
					p_domain == DOMAIN_VTST ? InterDVDInstruction::DOMAIN_VTST : InterDVDInstruction::DOMAIN_VMG,
					title_n > 0 ? title_n : dest,
					dest,
					nullptr);
		case ACTION_JUMP_PROGRAM:
			return InterDVDInstruction::encode_link(InterDVDInstruction::LINK_PGN, from, from, dest, nullptr);
		case ACTION_JUMP_CELL:
			return InterDVDInstruction::encode_link(InterDVDInstruction::LINK_CN, from, from, dest, nullptr);
		case ACTION_RESUME: {
			const PackedByteArray rsm = InterDVDInstruction::encode_rsm(from, nullptr);
			return rsm.size() == 8 ? rsm : InterDVDInstruction::encode_nop();
		}
		case ACTION_SET_AUDIO:
			return InterDVDInstruction::encode_set_stn(stream, -1, false, -1, nullptr);
		case ACTION_SET_SUBTITLE:
			return InterDVDInstruction::encode_set_stn(-1, stream, subtitle_on, -1, nullptr);
		case ACTION_SET_ANGLE:
			return InterDVDInstruction::encode_set_stn(-1, -1, false, stream > 0 ? stream : dest, nullptr);
		case ACTION_HIGHLIGHT_BUTTON:
			return InterDVDInstruction::encode_set_hl_btnn(dest, nullptr);
		case ACTION_EXIT:
			return InterDVDInstruction::encode_exit();
		default:
			break;
	}
	return InterDVDInstruction::encode_link(InterDVDInstruction::JUMP_TT, InterDVDInstruction::DOMAIN_VMG, InterDVDInstruction::DOMAIN_VTST, dest, nullptr);
}

static Rect2 clamp_hli_rect(const Rect2 &p_rect, int p_title_safe_bottom) {
	if (p_rect.size.x <= 0 || p_rect.size.y <= 0) {
		return Rect2();
	}
	const int safe_y = CLAMP(p_title_safe_bottom > 0 ? p_title_safe_bottom : 432, 2, 480);
	int x0 = CLAMP(int(p_rect.position.x), 0, 719);
	int y0 = CLAMP(int(p_rect.position.y), 0, 479);
	int x1 = CLAMP(int(p_rect.position.x + p_rect.size.x), x0, 719);
	int y1 = CLAMP(int(p_rect.position.y + p_rect.size.y), y0, 479);
	if (y1 > safe_y) {
		const int bh = MAX(y1 - y0, 2);
		y1 = safe_y;
		y0 = MAX(0, y1 - bh);
	}
	x0 &= ~1;
	y0 &= ~1;
	x1 &= ~1;
	y1 &= ~1;
	if (x1 <= x0) {
		x1 = MIN(x0 + 2, 718);
	}
	if (y1 <= y0) {
		y1 = MIN(y0 + 2, 478);
	}
	return Rect2(x0, y0, x1 - x0, y1 - y0);
}

void InterDVDButton::sync_highlight_from_scene(Node *p_root, const Rect2 &p_default_highlight, int p_title_safe_bottom) {
	Rect2 found_rect;
	Node *found = nullptr;
	if (p_root && !control_path.is_empty()) {
		found = p_root->get_node_or_null(control_path);
	}
	if (!found && p_root && !get_name().is_empty()) {
		found = p_root->find_child(get_name(), true, false);
		if (found && !Object::cast_to<CanvasItem>(found)) {
			found = nullptr;
		}
	}
	if (Control *ctrl = Object::cast_to<Control>(found)) {
		found_rect = ctrl->is_inside_tree() ? ctrl->get_global_rect() : Rect2(ctrl->get_position(), ctrl->get_size());
	} else if (Node2D *n2 = Object::cast_to<Node2D>(found)) {
		if (n2->has_method("get_rect")) {
			const Rect2 local = n2->call("get_rect");
			found_rect = n2->get_global_transform().xform(local);
		}
	} else if (CanvasItem *ci = Object::cast_to<CanvasItem>(found)) {
		if (ci->has_method("get_rect")) {
			const Rect2 local = ci->call("get_rect");
			found_rect = ci->get_global_transform().xform(local);
		}
	}
	if (found_rect.size.x <= 0 || found_rect.size.y <= 0) {
		if (p_default_highlight.size.x <= 0 || p_default_highlight.size.y <= 0) {
			return;
		}
		found_rect = p_default_highlight;
	}
	const int safe = p_title_safe_bottom > 0 ? p_title_safe_bottom : InterDVDSettings::title_safe_bottom();
	highlight = clamp_hli_rect(found_rect, safe);
}

void InterDVDButton::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_highlight", "rect"), &InterDVDButton::set_highlight);
	ClassDB::bind_method(D_METHOD("get_highlight"), &InterDVDButton::get_highlight);
	ClassDB::bind_method(D_METHOD("set_command", "command"), &InterDVDButton::set_command);
	ClassDB::bind_method(D_METHOD("get_command"), &InterDVDButton::get_command);
	ClassDB::bind_method(D_METHOD("set_action", "action"), &InterDVDButton::set_action);
	ClassDB::bind_method(D_METHOD("get_action"), &InterDVDButton::get_action);
	ClassDB::bind_method(D_METHOD("set_target", "target"), &InterDVDButton::set_target);
	ClassDB::bind_method(D_METHOD("get_target"), &InterDVDButton::get_target);
	ClassDB::bind_method(D_METHOD("set_title_n", "title"), &InterDVDButton::set_title_n);
	ClassDB::bind_method(D_METHOD("get_title_n"), &InterDVDButton::get_title_n);
	ClassDB::bind_method(D_METHOD("set_stream", "stream"), &InterDVDButton::set_stream);
	ClassDB::bind_method(D_METHOD("get_stream"), &InterDVDButton::get_stream);
	ClassDB::bind_method(D_METHOD("set_subtitle_on", "on"), &InterDVDButton::set_subtitle_on);
	ClassDB::bind_method(D_METHOD("get_subtitle_on"), &InterDVDButton::get_subtitle_on);
	ClassDB::bind_method(D_METHOD("set_auto_action", "enabled"), &InterDVDButton::set_auto_action);
	ClassDB::bind_method(D_METHOD("get_auto_action"), &InterDVDButton::get_auto_action);
	ClassDB::bind_method(D_METHOD("set_hidden", "hidden"), &InterDVDButton::set_hidden);
	ClassDB::bind_method(D_METHOD("is_hidden"), &InterDVDButton::is_hidden);
	ClassDB::bind_method(D_METHOD("set_forced_selected", "forced"), &InterDVDButton::set_forced_selected);
	ClassDB::bind_method(D_METHOD("is_forced_selected"), &InterDVDButton::is_forced_selected);
	ClassDB::bind_method(D_METHOD("set_forced_activated", "forced"), &InterDVDButton::set_forced_activated);
	ClassDB::bind_method(D_METHOD("is_forced_activated"), &InterDVDButton::is_forced_activated);
	ClassDB::bind_method(D_METHOD("set_numeric_select", "enabled"), &InterDVDButton::set_numeric_select);
	ClassDB::bind_method(D_METHOD("get_numeric_select"), &InterDVDButton::get_numeric_select);
	ClassDB::bind_method(D_METHOD("set_adjacent_up", "button"), &InterDVDButton::set_adjacent_up);
	ClassDB::bind_method(D_METHOD("get_adjacent_up"), &InterDVDButton::get_adjacent_up);
	ClassDB::bind_method(D_METHOD("set_adjacent_down", "button"), &InterDVDButton::set_adjacent_down);
	ClassDB::bind_method(D_METHOD("get_adjacent_down"), &InterDVDButton::get_adjacent_down);
	ClassDB::bind_method(D_METHOD("set_adjacent_left", "button"), &InterDVDButton::set_adjacent_left);
	ClassDB::bind_method(D_METHOD("get_adjacent_left"), &InterDVDButton::get_adjacent_left);
	ClassDB::bind_method(D_METHOD("set_adjacent_right", "button"), &InterDVDButton::set_adjacent_right);
	ClassDB::bind_method(D_METHOD("get_adjacent_right"), &InterDVDButton::get_adjacent_right);
	ClassDB::bind_method(D_METHOD("set_button_number", "number"), &InterDVDButton::set_button_number);
	ClassDB::bind_method(D_METHOD("get_button_number"), &InterDVDButton::get_button_number);
	ClassDB::bind_method(D_METHOD("set_color_group", "group"), &InterDVDButton::set_color_group);
	ClassDB::bind_method(D_METHOD("get_color_group"), &InterDVDButton::get_color_group);
	ClassDB::bind_method(D_METHOD("set_control_path", "path"), &InterDVDButton::set_control_path);
	ClassDB::bind_method(D_METHOD("get_control_path"), &InterDVDButton::get_control_path);
	ClassDB::bind_method(D_METHOD("set_select_color", "color"), &InterDVDButton::set_select_color);
	ClassDB::bind_method(D_METHOD("get_select_color"), &InterDVDButton::get_select_color);
	ClassDB::bind_method(D_METHOD("set_action_color", "color"), &InterDVDButton::set_action_color);
	ClassDB::bind_method(D_METHOD("get_action_color"), &InterDVDButton::get_action_color);
	ClassDB::bind_method(D_METHOD("resolve_command", "domain"), &InterDVDButton::resolve_command, DEFVAL(DOMAIN_VMGM));
	ClassDB::bind_method(D_METHOD("sync_highlight_from_scene", "root", "default_highlight", "title_safe_bottom"), &InterDVDButton::sync_highlight_from_scene, DEFVAL(Rect2()), DEFVAL(-1));
	ADD_PROPERTY(PropertyInfo(Variant::INT, "action", PROPERTY_HINT_ENUM, "Jump Title,Jump Menu,Jump PGC,Custom,Jump Chapter,Jump Program,Jump Cell,Resume,Set Audio,Set Subtitle,Set Angle,Highlight Button,Exit"), "set_action", "get_action");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "target", PROPERTY_HINT_RANGE, "1,99,1"), "set_target", "get_target");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "title_n", PROPERTY_HINT_RANGE, "1,99,1"), "set_title_n", "get_title_n");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "stream", PROPERTY_HINT_RANGE, "0,31,1"), "set_stream", "get_stream");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "subtitle_on"), "set_subtitle_on", "get_subtitle_on");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_action"), "set_auto_action", "get_auto_action");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "hidden"), "set_hidden", "is_hidden");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "forced_selected"), "set_forced_selected", "is_forced_selected");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "forced_activated"), "set_forced_activated", "is_forced_activated");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "numeric_select"), "set_numeric_select", "get_numeric_select");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "adjacent_up", PROPERTY_HINT_RANGE, "0,36,1"), "set_adjacent_up", "get_adjacent_up");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "adjacent_down", PROPERTY_HINT_RANGE, "0,36,1"), "set_adjacent_down", "get_adjacent_down");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "adjacent_left", PROPERTY_HINT_RANGE, "0,36,1"), "set_adjacent_left", "get_adjacent_left");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "adjacent_right", PROPERTY_HINT_RANGE, "0,36,1"), "set_adjacent_right", "get_adjacent_right");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "button_number", PROPERTY_HINT_RANGE, "0,36,1"), "set_button_number", "get_button_number");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "color_group", PROPERTY_HINT_RANGE, "1,3,1"), "set_color_group", "get_color_group");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "control_path"), "set_control_path", "get_control_path");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "select_color"), "set_select_color", "get_select_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "action_color"), "set_action_color", "get_action_color");
	ADD_PROPERTY(PropertyInfo(Variant::RECT2, "highlight"), "set_highlight", "get_highlight");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "command"), "set_command", "get_command");
	BIND_ENUM_CONSTANT(ACTION_JUMP_TITLE);
	BIND_ENUM_CONSTANT(ACTION_JUMP_MENU);
	BIND_ENUM_CONSTANT(ACTION_JUMP_PGC);
	BIND_ENUM_CONSTANT(ACTION_CUSTOM);
	BIND_ENUM_CONSTANT(ACTION_JUMP_CHAPTER);
	BIND_ENUM_CONSTANT(ACTION_JUMP_PROGRAM);
	BIND_ENUM_CONSTANT(ACTION_JUMP_CELL);
	BIND_ENUM_CONSTANT(ACTION_RESUME);
	BIND_ENUM_CONSTANT(ACTION_SET_AUDIO);
	BIND_ENUM_CONSTANT(ACTION_SET_SUBTITLE);
	BIND_ENUM_CONSTANT(ACTION_SET_ANGLE);
	BIND_ENUM_CONSTANT(ACTION_HIGHLIGHT_BUTTON);
	BIND_ENUM_CONSTANT(ACTION_EXIT);
	BIND_ENUM_CONSTANT(DOMAIN_VMGM);
	BIND_ENUM_CONSTANT(DOMAIN_VTST);
}

void InterDVDMenu::set_buttons(const TypedArray<InterDVDButton> &p_buttons) {
	buttons = p_buttons;
	if (buttons.size() > 36) {
		buttons.resize(36);
	}
}

void InterDVDMenu::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_motion", "motion"), &InterDVDMenu::set_motion);
	ClassDB::bind_method(D_METHOD("is_motion"), &InterDVDMenu::is_motion);
	ClassDB::bind_method(D_METHOD("set_buttons", "buttons"), &InterDVDMenu::set_buttons);
	ClassDB::bind_method(D_METHOD("get_buttons"), &InterDVDMenu::get_buttons);
	ClassDB::bind_method(D_METHOD("set_cell", "cell"), &InterDVDMenu::set_cell);
	ClassDB::bind_method(D_METHOD("get_cell"), &InterDVDMenu::get_cell);
	ClassDB::bind_method(D_METHOD("set_still_time", "seconds"), &InterDVDMenu::set_still_time);
	ClassDB::bind_method(D_METHOD("get_still_time"), &InterDVDMenu::get_still_time);
	ClassDB::bind_method(D_METHOD("set_next_pgc", "pgc"), &InterDVDMenu::set_next_pgc);
	ClassDB::bind_method(D_METHOD("get_next_pgc"), &InterDVDMenu::get_next_pgc);
	ClassDB::bind_method(D_METHOD("set_prev_pgc", "pgc"), &InterDVDMenu::set_prev_pgc);
	ClassDB::bind_method(D_METHOD("get_prev_pgc"), &InterDVDMenu::get_prev_pgc);
	ClassDB::bind_method(D_METHOD("set_default_button", "button"), &InterDVDMenu::set_default_button);
	ClassDB::bind_method(D_METHOD("get_default_button"), &InterDVDMenu::get_default_button);
	ClassDB::bind_method(D_METHOD("set_forced_selected_button", "button"), &InterDVDMenu::set_forced_selected_button);
	ClassDB::bind_method(D_METHOD("get_forced_selected_button"), &InterDVDMenu::get_forced_selected_button);
	ClassDB::bind_method(D_METHOD("set_forced_activated_button", "button"), &InterDVDMenu::set_forced_activated_button);
	ClassDB::bind_method(D_METHOD("get_forced_activated_button"), &InterDVDMenu::get_forced_activated_button);
	ClassDB::bind_method(D_METHOD("set_menu_type", "type"), &InterDVDMenu::set_menu_type);
	ClassDB::bind_method(D_METHOD("get_menu_type"), &InterDVDMenu::get_menu_type);
	ClassDB::bind_method(D_METHOD("set_button_group_mask", "mask"), &InterDVDMenu::set_button_group_mask);
	ClassDB::bind_method(D_METHOD("get_button_group_mask"), &InterDVDMenu::get_button_group_mask);
	ClassDB::bind_method(D_METHOD("set_clut", "clut"), &InterDVDMenu::set_clut);
	ClassDB::bind_method(D_METHOD("get_clut"), &InterDVDMenu::get_clut);
	ClassDB::bind_method(D_METHOD("set_parental_id", "id"), &InterDVDMenu::set_parental_id);
	ClassDB::bind_method(D_METHOD("get_parental_id"), &InterDVDMenu::get_parental_id);
	ClassDB::bind_method(D_METHOD("set_uops", "uops"), &InterDVDMenu::set_uops);
	ClassDB::bind_method(D_METHOD("get_uops"), &InterDVDMenu::get_uops);
	ClassDB::bind_method(D_METHOD("set_goup_pgc", "pgc"), &InterDVDMenu::set_goup_pgc);
	ClassDB::bind_method(D_METHOD("get_goup_pgc"), &InterDVDMenu::get_goup_pgc);
	ClassDB::bind_method(D_METHOD("set_title_set_nr", "n"), &InterDVDMenu::set_title_set_nr);
	ClassDB::bind_method(D_METHOD("get_title_set_nr"), &InterDVDMenu::get_title_set_nr);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "motion"), "set_motion", "is_motion");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "menu_type", PROPERTY_HINT_ENUM, "Title Menu:2,Root Menu:3,Subpicture Menu:4,Audio Menu:5,Angle Menu:6,Chapter Menu:7"), "set_menu_type", "get_menu_type");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "still_time", PROPERTY_HINT_RANGE, "0,255,1"), "set_still_time", "get_still_time");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "next_pgc", PROPERTY_HINT_RANGE, "0,99,1"), "set_next_pgc", "get_next_pgc");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "prev_pgc", PROPERTY_HINT_RANGE, "0,99,1"), "set_prev_pgc", "get_prev_pgc");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "default_button", PROPERTY_HINT_RANGE, "1,36,1"), "set_default_button", "get_default_button");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "forced_selected_button", PROPERTY_HINT_RANGE, "0,36,1"), "set_forced_selected_button", "get_forced_selected_button");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "forced_activated_button", PROPERTY_HINT_RANGE, "0,36,1"), "set_forced_activated_button", "get_forced_activated_button");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "button_group_mask"), "set_button_group_mask", "get_button_group_mask");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_COLOR_ARRAY, "clut"), "set_clut", "get_clut");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "buttons", PROPERTY_HINT_ARRAY_TYPE, "InterDVDButton"), "set_buttons", "get_buttons");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "cell", PROPERTY_HINT_RESOURCE_TYPE, "InterDVDCell"), "set_cell", "get_cell");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "parental_id", PROPERTY_HINT_RANGE, "0,32767,1"), "set_parental_id", "get_parental_id");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "uops"), "set_uops", "get_uops");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "goup_pgc", PROPERTY_HINT_RANGE, "0,99,1"), "set_goup_pgc", "get_goup_pgc");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "title_set_nr", PROPERTY_HINT_RANGE, "0,99,1"), "set_title_set_nr", "get_title_set_nr");
	BIND_ENUM_CONSTANT(MENU_TITLE);
	BIND_ENUM_CONSTANT(MENU_ROOT);
	BIND_ENUM_CONSTANT(MENU_SUBPICTURE);
	BIND_ENUM_CONSTANT(MENU_AUDIO);
	BIND_ENUM_CONSTANT(MENU_ANGLE);
	BIND_ENUM_CONSTANT(MENU_CHAPTER);
}

void InterDVDPGC::set_buttons(const TypedArray<InterDVDButton> &p_buttons) {
	buttons = p_buttons;
	if (buttons.size() > 36) {
		buttons.resize(36);
	}
}

Ref<InterDVDCell> InterDVDPGC::add_cell_from_video(const String &p_path) {
	Ref<InterDVDCell> cell;
	cell.instantiate();
	cell->set_source_path(p_path);
	const String clip_name = p_path.get_file().get_basename();
	if (!clip_name.is_empty()) {
		cell->set_name(clip_name);
	}
	cells.push_back(cell);
	emit_changed();
	notify_property_list_changed();
	return cell;
}

Ref<InterDVDButton> InterDVDPGC::add_jump_title_button(int p_title_n) {
	Ref<InterDVDButton> btn;
	btn.instantiate();
	btn->set_action(InterDVDButton::ACTION_JUMP_TITLE);
	btn->set_title_n(CLAMP(p_title_n, 1, 99));
	btn->set_highlight(Rect2(80, 200, 240, 60));
	buttons.push_back(btn);
	if (buttons.size() > 36) {
		buttons.resize(36);
	}
	emit_changed();
	notify_property_list_changed();
	return btn;
}

void InterDVDPGC::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_cells", "cells"), &InterDVDPGC::set_cells);
	ClassDB::bind_method(D_METHOD("get_cells"), &InterDVDPGC::get_cells);
	ClassDB::bind_method(D_METHOD("set_buttons", "buttons"), &InterDVDPGC::set_buttons);
	ClassDB::bind_method(D_METHOD("get_buttons"), &InterDVDPGC::get_buttons);
	ClassDB::bind_method(D_METHOD("set_pre_commands", "commands"), &InterDVDPGC::set_pre_commands);
	ClassDB::bind_method(D_METHOD("get_pre_commands"), &InterDVDPGC::get_pre_commands);
	ClassDB::bind_method(D_METHOD("set_post_commands", "commands"), &InterDVDPGC::set_post_commands);
	ClassDB::bind_method(D_METHOD("get_post_commands"), &InterDVDPGC::get_post_commands);
	ClassDB::bind_method(D_METHOD("set_still_time", "seconds"), &InterDVDPGC::set_still_time);
	ClassDB::bind_method(D_METHOD("get_still_time"), &InterDVDPGC::get_still_time);
	ClassDB::bind_method(D_METHOD("set_next_pgc", "pgc"), &InterDVDPGC::set_next_pgc);
	ClassDB::bind_method(D_METHOD("get_next_pgc"), &InterDVDPGC::get_next_pgc);
	ClassDB::bind_method(D_METHOD("set_prev_pgc", "pgc"), &InterDVDPGC::set_prev_pgc);
	ClassDB::bind_method(D_METHOD("get_prev_pgc"), &InterDVDPGC::get_prev_pgc);
	ClassDB::bind_method(D_METHOD("set_default_button", "button"), &InterDVDPGC::set_default_button);
	ClassDB::bind_method(D_METHOD("get_default_button"), &InterDVDPGC::get_default_button);
	ClassDB::bind_method(D_METHOD("set_clut", "clut"), &InterDVDPGC::set_clut);
	ClassDB::bind_method(D_METHOD("get_clut"), &InterDVDPGC::get_clut);
	ClassDB::bind_method(D_METHOD("set_parental_id", "id"), &InterDVDPGC::set_parental_id);
	ClassDB::bind_method(D_METHOD("get_parental_id"), &InterDVDPGC::get_parental_id);
	ClassDB::bind_method(D_METHOD("set_uops", "uops"), &InterDVDPGC::set_uops);
	ClassDB::bind_method(D_METHOD("get_uops"), &InterDVDPGC::get_uops);
	ClassDB::bind_method(D_METHOD("set_goup_pgc", "pgc"), &InterDVDPGC::set_goup_pgc);
	ClassDB::bind_method(D_METHOD("get_goup_pgc"), &InterDVDPGC::get_goup_pgc);
	ClassDB::bind_method(D_METHOD("set_title_set_nr", "n"), &InterDVDPGC::set_title_set_nr);
	ClassDB::bind_method(D_METHOD("get_title_set_nr"), &InterDVDPGC::get_title_set_nr);
	ClassDB::bind_method(D_METHOD("add_cell_from_video", "path"), &InterDVDPGC::add_cell_from_video);
	ClassDB::bind_method(D_METHOD("add_jump_title_button", "title_n"), &InterDVDPGC::add_jump_title_button, DEFVAL(1));
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "cells", PROPERTY_HINT_ARRAY_TYPE, "InterDVDCell"), "set_cells", "get_cells");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "buttons", PROPERTY_HINT_ARRAY_TYPE, "InterDVDButton"), "set_buttons", "get_buttons");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "pre_commands", PROPERTY_HINT_ARRAY_TYPE, "PackedByteArray"), "set_pre_commands", "get_pre_commands");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "post_commands", PROPERTY_HINT_ARRAY_TYPE, "PackedByteArray"), "set_post_commands", "get_post_commands");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "still_time", PROPERTY_HINT_RANGE, "0,255,1"), "set_still_time", "get_still_time");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "next_pgc", PROPERTY_HINT_RANGE, "0,99,1"), "set_next_pgc", "get_next_pgc");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "prev_pgc", PROPERTY_HINT_RANGE, "0,99,1"), "set_prev_pgc", "get_prev_pgc");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "default_button", PROPERTY_HINT_RANGE, "1,36,1"), "set_default_button", "get_default_button");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_COLOR_ARRAY, "clut"), "set_clut", "get_clut");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "parental_id", PROPERTY_HINT_RANGE, "0,32767,1"), "set_parental_id", "get_parental_id");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "uops"), "set_uops", "get_uops");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "goup_pgc", PROPERTY_HINT_RANGE, "0,99,1"), "set_goup_pgc", "get_goup_pgc");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "title_set_nr", PROPERTY_HINT_RANGE, "0,99,1"), "set_title_set_nr", "get_title_set_nr");
}

void InterDVDProject::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_region_mask", "mask"), &InterDVDProject::set_region_mask);
	ClassDB::bind_method(D_METHOD("get_region_mask"), &InterDVDProject::get_region_mask);
	ClassDB::bind_method(D_METHOD("set_parental_level", "level"), &InterDVDProject::set_parental_level);
	ClassDB::bind_method(D_METHOD("get_parental_level"), &InterDVDProject::get_parental_level);
	ClassDB::bind_method(D_METHOD("set_volume_id", "id"), &InterDVDProject::set_volume_id);
	ClassDB::bind_method(D_METHOD("get_volume_id"), &InterDVDProject::get_volume_id);
	ClassDB::bind_method(D_METHOD("set_serial", "serial"), &InterDVDProject::set_serial);
	ClassDB::bind_method(D_METHOD("get_serial"), &InterDVDProject::get_serial);
	ClassDB::bind_method(D_METHOD("set_provider_id", "id"), &InterDVDProject::set_provider_id);
	ClassDB::bind_method(D_METHOD("get_provider_id"), &InterDVDProject::get_provider_id);
	ClassDB::bind_method(D_METHOD("set_disc_title", "title"), &InterDVDProject::set_disc_title);
	ClassDB::bind_method(D_METHOD("get_disc_title"), &InterDVDProject::get_disc_title);
	ClassDB::bind_method(D_METHOD("set_menu_language", "language"), &InterDVDProject::set_menu_language);
	ClassDB::bind_method(D_METHOD("get_menu_language"), &InterDVDProject::get_menu_language);
	ClassDB::bind_method(D_METHOD("set_audio_language", "language"), &InterDVDProject::set_audio_language);
	ClassDB::bind_method(D_METHOD("get_audio_language"), &InterDVDProject::get_audio_language);
	ClassDB::bind_method(D_METHOD("set_subtitle_language", "language"), &InterDVDProject::set_subtitle_language);
	ClassDB::bind_method(D_METHOD("get_subtitle_language"), &InterDVDProject::get_subtitle_language);
	ClassDB::bind_method(D_METHOD("set_ac3_bitrate_k", "kbps"), &InterDVDProject::set_ac3_bitrate_k);
	ClassDB::bind_method(D_METHOD("get_ac3_bitrate_k"), &InterDVDProject::get_ac3_bitrate_k);
	ClassDB::bind_method(D_METHOD("set_ac3_channels", "channels"), &InterDVDProject::set_ac3_channels);
	ClassDB::bind_method(D_METHOD("get_ac3_channels"), &InterDVDProject::get_ac3_channels);
	ClassDB::bind_method(D_METHOD("set_pip_blackdetect_sec", "seconds"), &InterDVDProject::set_pip_blackdetect_sec);
	ClassDB::bind_method(D_METHOD("get_pip_blackdetect_sec"), &InterDVDProject::get_pip_blackdetect_sec);
	ClassDB::bind_method(D_METHOD("set_pip_blackdetect_pix_th", "threshold"), &InterDVDProject::set_pip_blackdetect_pix_th);
	ClassDB::bind_method(D_METHOD("get_pip_blackdetect_pix_th"), &InterDVDProject::get_pip_blackdetect_pix_th);
	ClassDB::bind_method(D_METHOD("set_gop_size", "frames"), &InterDVDProject::set_gop_size);
	ClassDB::bind_method(D_METHOD("get_gop_size"), &InterDVDProject::get_gop_size);
	ClassDB::bind_method(D_METHOD("set_title_safe_bottom", "y"), &InterDVDProject::set_title_safe_bottom);
	ClassDB::bind_method(D_METHOD("get_title_safe_bottom"), &InterDVDProject::get_title_safe_bottom);
	ClassDB::bind_method(D_METHOD("set_bake_warmup_frames", "frames"), &InterDVDProject::set_bake_warmup_frames);
	ClassDB::bind_method(D_METHOD("get_bake_warmup_frames"), &InterDVDProject::get_bake_warmup_frames);
	ClassDB::bind_method(D_METHOD("set_extras", "extras"), &InterDVDProject::set_extras);
	ClassDB::bind_method(D_METHOD("get_extras"), &InterDVDProject::get_extras);
	ClassDB::bind_method(D_METHOD("set_extras_dir", "dir"), &InterDVDProject::set_extras_dir);
	ClassDB::bind_method(D_METHOD("get_extras_dir"), &InterDVDProject::get_extras_dir);
	ClassDB::bind_method(D_METHOD("set_copyright", "text"), &InterDVDProject::set_copyright);
	ClassDB::bind_method(D_METHOD("get_copyright"), &InterDVDProject::get_copyright);
	ClassDB::bind_method(D_METHOD("set_copyright_file", "path"), &InterDVDProject::set_copyright_file);
	ClassDB::bind_method(D_METHOD("get_copyright_file"), &InterDVDProject::get_copyright_file);
	ClassDB::bind_method(D_METHOD("set_license", "text"), &InterDVDProject::set_license);
	ClassDB::bind_method(D_METHOD("get_license"), &InterDVDProject::get_license);
	ClassDB::bind_method(D_METHOD("set_license_file", "path"), &InterDVDProject::set_license_file);
	ClassDB::bind_method(D_METHOD("get_license_file"), &InterDVDProject::get_license_file);
	ClassDB::bind_method(D_METHOD("set_readme", "text"), &InterDVDProject::set_readme);
	ClassDB::bind_method(D_METHOD("get_readme"), &InterDVDProject::get_readme);
	ClassDB::bind_method(D_METHOD("set_readme_file", "path"), &InterDVDProject::set_readme_file);
	ClassDB::bind_method(D_METHOD("get_readme_file"), &InterDVDProject::get_readme_file);
	ClassDB::bind_method(D_METHOD("set_credits", "text"), &InterDVDProject::set_credits);
	ClassDB::bind_method(D_METHOD("get_credits"), &InterDVDProject::get_credits);
	ClassDB::bind_method(D_METHOD("set_credits_file", "path"), &InterDVDProject::set_credits_file);
	ClassDB::bind_method(D_METHOD("get_credits_file"), &InterDVDProject::get_credits_file);
	ClassDB::bind_method(D_METHOD("set_autorun_label", "label"), &InterDVDProject::set_autorun_label);
	ClassDB::bind_method(D_METHOD("get_autorun_label"), &InterDVDProject::get_autorun_label);
	ClassDB::bind_method(D_METHOD("set_autorun_open", "open"), &InterDVDProject::set_autorun_open);
	ClassDB::bind_method(D_METHOD("get_autorun_open"), &InterDVDProject::get_autorun_open);
	ClassDB::bind_method(D_METHOD("set_autorun_icon", "icon"), &InterDVDProject::set_autorun_icon);
	ClassDB::bind_method(D_METHOD("get_autorun_icon"), &InterDVDProject::get_autorun_icon);
	ClassDB::bind_method(D_METHOD("set_publisher", "publisher"), &InterDVDProject::set_publisher);
	ClassDB::bind_method(D_METHOD("get_publisher"), &InterDVDProject::get_publisher);
	ClassDB::bind_method(D_METHOD("set_author", "author"), &InterDVDProject::set_author);
	ClassDB::bind_method(D_METHOD("get_author"), &InterDVDProject::get_author);
	ClassDB::bind_method(D_METHOD("set_studio", "studio"), &InterDVDProject::set_studio);
	ClassDB::bind_method(D_METHOD("get_studio"), &InterDVDProject::get_studio);
	ClassDB::bind_method(D_METHOD("set_website", "website"), &InterDVDProject::set_website);
	ClassDB::bind_method(D_METHOD("get_website"), &InterDVDProject::get_website);
	ClassDB::bind_method(D_METHOD("set_version", "version"), &InterDVDProject::set_version);
	ClassDB::bind_method(D_METHOD("get_version"), &InterDVDProject::get_version);
	ClassDB::bind_method(D_METHOD("reset_encode_defaults"), &InterDVDProject::reset_encode_defaults);
	ClassDB::bind_method(D_METHOD("seed_from_project_settings"), &InterDVDProject::seed_from_project_settings);
	ClassDB::bind_static_method("InterDVDProject", D_METHOD("sanitize_volume_id", "id"), &InterDVDProject::sanitize_volume_id);
	ClassDB::bind_static_method("InterDVDProject", D_METHOD("language_be16", "language"), &InterDVDProject::language_be16);
	ClassDB::bind_static_method("InterDVDProject", D_METHOD("split_extra_spec", "spec"), &InterDVDProject::split_extra_spec_bind);
	ClassDB::bind_method(D_METHOD("set_first_play", "pgc"), &InterDVDProject::set_first_play);
	ClassDB::bind_method(D_METHOD("get_first_play"), &InterDVDProject::get_first_play);
	ClassDB::bind_method(D_METHOD("set_titles", "titles"), &InterDVDProject::set_titles);
	ClassDB::bind_method(D_METHOD("get_titles"), &InterDVDProject::get_titles);
	ClassDB::bind_method(D_METHOD("set_menus", "menus"), &InterDVDProject::set_menus);
	ClassDB::bind_method(D_METHOD("get_menus"), &InterDVDProject::get_menus);
	ClassDB::bind_method(D_METHOD("add_title_from_video", "path"), &InterDVDProject::add_title_from_video);
	ClassDB::bind_method(D_METHOD("add_titles_from_videos", "paths"), &InterDVDProject::add_titles_from_videos);
	ClassDB::bind_method(D_METHOD("add_chapter_from_video", "title_n", "path"), &InterDVDProject::add_chapter_from_video);
	ClassDB::bind_method(D_METHOD("add_title_from_scene", "scene", "duration_sec"), &InterDVDProject::add_title_from_scene, DEFVAL(4.0));
	ClassDB::bind_method(D_METHOD("add_title_menu"), &InterDVDProject::add_title_menu);
	ClassDB::bind_method(D_METHOD("add_menu_title"), &InterDVDProject::add_menu_title);
	ClassDB::bind_method(D_METHOD("ensure_first_play", "title_n"), &InterDVDProject::ensure_first_play, DEFVAL(1));
	ADD_GROUP("Disc", "");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "disc_title"), "set_disc_title", "get_disc_title");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "volume_id"), "set_volume_id", "get_volume_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "serial"), "set_serial", "get_serial");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "provider_id"), "set_provider_id", "get_provider_id");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "region_mask"), "set_region_mask", "get_region_mask");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "parental_level"), "set_parental_level", "get_parental_level");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "menu_language"), "set_menu_language", "get_menu_language");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "audio_language"), "set_audio_language", "get_audio_language");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "subtitle_language"), "set_subtitle_language", "get_subtitle_language");
	ADD_GROUP("Authoring", "");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "first_play", PROPERTY_HINT_RESOURCE_TYPE, "InterDVDPGC"), "set_first_play", "get_first_play");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "titles", PROPERTY_HINT_ARRAY_TYPE, "InterDVDPGC"), "set_titles", "get_titles");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "menus", PROPERTY_HINT_ARRAY_TYPE, "InterDVDMenu"), "set_menus", "get_menus");
	ADD_GROUP("Encode", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "ac3_bitrate_k", PROPERTY_HINT_RANGE, "64,448,32"), "set_ac3_bitrate_k", "get_ac3_bitrate_k");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "ac3_channels", PROPERTY_HINT_RANGE, "1,6,1"), "set_ac3_channels", "get_ac3_channels");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "gop_size", PROPERTY_HINT_RANGE, "1,30,1"), "set_gop_size", "get_gop_size");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pip_blackdetect_sec", PROPERTY_HINT_RANGE, "0,10,0.1"), "set_pip_blackdetect_sec", "get_pip_blackdetect_sec");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pip_blackdetect_pix_th", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_pip_blackdetect_pix_th", "get_pip_blackdetect_pix_th");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "title_safe_bottom", PROPERTY_HINT_RANGE, "2,480,2"), "set_title_safe_bottom", "get_title_safe_bottom");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "bake_warmup_frames", PROPERTY_HINT_RANGE, "0,30,1"), "set_bake_warmup_frames", "get_bake_warmup_frames");
	ADD_GROUP("Extras", "");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "extras"), "set_extras", "get_extras");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "extras_dir", PROPERTY_HINT_DIR), "set_extras_dir", "get_extras_dir");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "copyright", PROPERTY_HINT_MULTILINE_TEXT), "set_copyright", "get_copyright");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "copyright_file", PROPERTY_HINT_FILE), "set_copyright_file", "get_copyright_file");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "license", PROPERTY_HINT_MULTILINE_TEXT), "set_license", "get_license");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "license_file", PROPERTY_HINT_FILE), "set_license_file", "get_license_file");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "readme", PROPERTY_HINT_MULTILINE_TEXT), "set_readme", "get_readme");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "readme_file", PROPERTY_HINT_FILE), "set_readme_file", "get_readme_file");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "credits", PROPERTY_HINT_MULTILINE_TEXT), "set_credits", "get_credits");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "credits_file", PROPERTY_HINT_FILE), "set_credits_file", "get_credits_file");
	ADD_GROUP("Autorun", "");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "autorun_label"), "set_autorun_label", "get_autorun_label");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "autorun_open"), "set_autorun_open", "get_autorun_open");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "autorun_icon", PROPERTY_HINT_FILE), "set_autorun_icon", "get_autorun_icon");
	ADD_GROUP("Metadata", "");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "publisher"), "set_publisher", "get_publisher");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "author"), "set_author", "get_author");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "studio"), "set_studio", "get_studio");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "website"), "set_website", "get_website");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "version"), "set_version", "get_version");
}

void InterDVDProject::_notify_graph_changed() {
	emit_changed();
	notify_property_list_changed();
}

Ref<InterDVDPGC> InterDVDProject::add_title_from_video(const String &p_path) {
	Ref<InterDVDCell> cell;
	cell.instantiate();
	cell->set_source_path(p_path);
	const String clip_name = p_path.get_file().get_basename();
	if (!clip_name.is_empty()) {
		cell->set_name(clip_name);
	}
	Ref<InterDVDPGC> title;
	title.instantiate();
	if (!clip_name.is_empty()) {
		title->set_name(clip_name);
	}
	TypedArray<InterDVDCell> cells;
	cells.push_back(cell);
	title->set_cells(cells);
	titles.push_back(title);
	_notify_graph_changed();
	return title;
}

TypedArray<InterDVDPGC> InterDVDProject::add_titles_from_videos(const PackedStringArray &p_paths) {
	TypedArray<InterDVDPGC> added;
	for (int i = 0; i < p_paths.size(); i++) {
		const String path = p_paths[i].strip_edges();
		if (path.is_empty()) {
			continue;
		}
		const Ref<InterDVDPGC> title = add_title_from_video(path);
		if (title.is_valid()) {
			added.push_back(title);
		}
	}
	return added;
}

Ref<InterDVDCell> InterDVDProject::add_chapter_from_video(int p_title_n, const String &p_path) {
	if (titles.is_empty()) {
		const Ref<InterDVDPGC> title = add_title_from_video(p_path);
		if (title.is_valid() && title->get_cells().size() > 0) {
			return title->get_cells()[0];
		}
		return Ref<InterDVDCell>();
	}
	const int idx = (p_title_n <= 0) ? titles.size() - 1 : CLAMP(p_title_n - 1, 0, titles.size() - 1);
	Ref<InterDVDPGC> title = titles[idx];
	if (title.is_null()) {
		title.instantiate();
		titles[idx] = title;
	}
	const Ref<InterDVDCell> cell = title->add_cell_from_video(p_path);
	_notify_graph_changed();
	return cell;
}

Ref<InterDVDPGC> InterDVDProject::add_title_from_scene(const Ref<PackedScene> &p_scene, double p_duration_sec) {
	Ref<InterDVDCell> cell;
	cell.instantiate();
	cell->set_packed_scene(p_scene);
	cell->set_duration_sec(p_duration_sec > 0.0 ? p_duration_sec : 4.0);
	if (p_scene.is_valid()) {
		const String scene_name = p_scene->get_path().get_file().get_basename();
		if (!scene_name.is_empty()) {
			cell->set_name(scene_name);
		}
	}
	Ref<InterDVDPGC> title;
	title.instantiate();
	if (!cell->get_name().is_empty()) {
		title->set_name(cell->get_name());
	}
	TypedArray<InterDVDCell> cells;
	cells.push_back(cell);
	title->set_cells(cells);
	titles.push_back(title);
	_notify_graph_changed();
	return title;
}

Ref<InterDVDMenu> InterDVDProject::add_title_menu() {
	Ref<InterDVDButton> play;
	play.instantiate();
	play->set_name("Play");
	play->set_action(InterDVDButton::ACTION_JUMP_TITLE);
	play->set_title_n(1);
	play->set_highlight(Rect2(80, 200, 240, 60));
	TypedArray<InterDVDButton> buttons;
	buttons.push_back(play);
	Ref<InterDVDMenu> menu;
	menu.instantiate();
	menu->set_name("Title Menu");
	menu->set_menu_type(InterDVDMenu::MENU_TITLE);
	menu->set_still_time(0);
	menu->set_buttons(buttons);
	menus.push_back(menu);
	_notify_graph_changed();
	return menu;
}

Ref<InterDVDPGC> InterDVDProject::add_menu_title() {
	Ref<InterDVDPGC> title;
	title.instantiate();
	title->set_name("Menu");
	title->set_still_time(255);
	title->add_jump_title_button(1);
	titles.push_back(title);
	_notify_graph_changed();
	return title;
}

Ref<InterDVDPGC> InterDVDProject::ensure_first_play(int p_title_n) {
	const int dest = CLAMP(p_title_n, 1, 99);
	if (first_play.is_null()) {
		first_play.instantiate();
		first_play->set_name("First Play");
	}
	TypedArray<PackedByteArray> cmds;
	cmds.push_back(InterDVDInstruction::encode_link(InterDVDInstruction::JUMP_TT, InterDVDInstruction::DOMAIN_FPC, InterDVDInstruction::DOMAIN_VTST, dest, nullptr));
	first_play->set_pre_commands(cmds);
	_notify_graph_changed();
	return first_play;
}

void InterDVDProject::split_extra_spec(const String &p_spec, String &r_host, String &r_disc) {
	const String spec = p_spec.strip_edges();
	r_host = spec;
	r_disc = String();
	if (spec.is_empty()) {
		return;
	}
	const int colon = spec.rfind(":");
	if (colon <= 0) {
		return;
	}
	if (colon == 1 && spec.length() > 2 && (spec[2] == '\\' || spec[2] == '/') && ((spec[0] >= 'A' && spec[0] <= 'Z') || (spec[0] >= 'a' && spec[0] <= 'z'))) {
		return;
	}
	r_host = spec.substr(0, colon);
	r_disc = spec.substr(colon + 1).replace("\\", "/").trim_prefix("/");
}

PackedStringArray InterDVDProject::split_extra_spec_bind(const String &p_spec) {
	String host;
	String disc;
	split_extra_spec(p_spec, host, disc);
	PackedStringArray out;
	out.push_back(host);
	out.push_back(disc);
	return out;
}

String InterDVDProject::sanitize_volume_id(const String &p_id) {
	String out;
	const String src = p_id.strip_edges().to_upper();
	for (int i = 0; i < src.length() && out.length() < 32; i++) {
		const char32_t c = src[i];
		if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
			out += c;
		} else if (c == '_' || c == '-' || c == ' ') {
			out += '_';
		}
	}
	while (out.contains("__")) {
		out = out.replace("__", "_");
	}
	out = out.trim_prefix("_").trim_suffix("_");
	return out.is_empty() ? String("BLAZIUM_DVD") : out;
}

uint16_t InterDVDProject::language_be16(const String &p_lang) {
	String lang = p_lang.strip_edges().to_lower();
	if (lang.length() < 2) {
		lang = "en";
	}
	return uint16_t((uint16_t(lang.unicode_at(0) & 0x7F) << 8) | uint16_t(lang.unicode_at(1) & 0x7F));
}

InterDVDProject::InterDVDProject() {
	reset_encode_defaults();
}

void InterDVDProject::reset_encode_defaults() {
	region_mask = 1;
	parental_level = 1;
	ac3_bitrate_k = 192;
	ac3_channels = 2;
	gop_size = 15;
	title_safe_bottom = 432;
	bake_warmup_frames = 2;
	pip_blackdetect_sec = 2.5;
	pip_blackdetect_pix_th = 0.12;
}

void InterDVDProject::seed_from_project_settings() {
	reset_encode_defaults();
}

namespace InterDVDSettings {

static Ref<InterDVDProject> _active_project;

void set_active_project(const Ref<InterDVDProject> &p_project) {
	_active_project = p_project;
}

Ref<InterDVDProject> active_project() {
	return _active_project;
}

ActiveProjectGuard::ActiveProjectGuard(const Ref<InterDVDProject> &p_project) {
	previous = _active_project;
	_active_project = p_project;
}

ActiveProjectGuard::~ActiveProjectGuard() {
	_active_project = previous;
}

static Ref<InterDVDProject> resolved_project(const Ref<InterDVDProject> &p_project) {
	return p_project.is_valid() ? p_project : _active_project;
}

int setting_int(const char *p_name, int p_fallback) {
	if (!ProjectSettings::get_singleton() || !ProjectSettings::get_singleton()->has_setting(p_name)) {
		return p_fallback;
	}
	return int(ProjectSettings::get_singleton()->get_setting(p_name));
}

int ac3_bitrate_k(const Ref<InterDVDProject> &p_project) {
	const Ref<InterDVDProject> project = resolved_project(p_project);
	if (project.is_valid() && project->get_ac3_bitrate_k() > 0) {
		return project->get_ac3_bitrate_k();
	}
	return 192;
}

int ac3_channels(const Ref<InterDVDProject> &p_project) {
	const Ref<InterDVDProject> project = resolved_project(p_project);
	if (project.is_valid() && project->get_ac3_channels() > 0) {
		return CLAMP(project->get_ac3_channels(), 1, 6);
	}
	return 2;
}

int gop_size(const Ref<InterDVDProject> &p_project) {
	const Ref<InterDVDProject> project = resolved_project(p_project);
	if (project.is_valid() && project->get_gop_size() > 0) {
		return project->get_gop_size();
	}
	return 15;
}

int title_safe_bottom(const Ref<InterDVDProject> &p_project) {
	const Ref<InterDVDProject> project = resolved_project(p_project);
	if (project.is_valid() && project->get_title_safe_bottom() > 0) {
		return project->get_title_safe_bottom();
	}
	return 432;
}

int bake_warmup_frames(const Ref<InterDVDProject> &p_project) {
	const Ref<InterDVDProject> project = resolved_project(p_project);
	if (project.is_valid()) {
		return MAX(project->get_bake_warmup_frames(), 0);
	}
	return 2;
}

double pip_blackdetect_sec(const Ref<InterDVDProject> &p_project) {
	const Ref<InterDVDProject> project = resolved_project(p_project);
	if (project.is_valid() && project->get_pip_blackdetect_sec() > 0.0) {
		return project->get_pip_blackdetect_sec();
	}
	return 2.5;
}

double pip_blackdetect_pix_th(const Ref<InterDVDProject> &p_project) {
	const Ref<InterDVDProject> project = resolved_project(p_project);
	if (project.is_valid() && project->get_pip_blackdetect_pix_th() > 0.0) {
		return project->get_pip_blackdetect_pix_th();
	}
	return 0.12;
}

String cache_path() {
	String path = "res://.inter_dvd_cache";
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/inter_dvd/cache_path")) {
		const String set = ProjectSettings::get_singleton()->get_setting("blazium/inter_dvd/cache_path");
		if (!set.is_empty()) {
			path = set;
		}
	}
	if (path.begins_with("res://") && ProjectSettings::get_singleton() && !ProjectSettings::get_singleton()->get_resource_path().is_empty()) {
		return ProjectSettings::get_singleton()->globalize_path(path);
	}
	if (path.is_empty() && OS::get_singleton()) {
		return OS::get_singleton()->get_cache_path().path_join("inter_dvd_cache");
	}
	return path;
}

} //namespace InterDVDSettings
