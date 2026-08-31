/**************************************************************************/
/*  inp_writer.cpp                                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* SPDX-License-Identifier: MIT                                           */
/**************************************************************************/

#include "inp_writer.h"

#include "cooker.h"

#include "core/input/input_event.h"
#include "core/input/input_map.h"
#include "core/string/ustring.h"
#include "core/templates/hash_set.h"

namespace n64_cook {

	static const int kNameLen = 24;

static const uint16_t kBtnA = 1 << 0;
static const uint16_t kBtnB = 1 << 1;
static const uint16_t kBtnZ = 1 << 2;
static const uint16_t kBtnStart = 1 << 3;
static const uint16_t kBtnL = 1 << 4;
static const uint16_t kBtnR = 1 << 5;
static const uint16_t kBtnDLeft = 1 << 6;
static const uint16_t kBtnDRight = 1 << 7;
static const uint16_t kBtnDUp = 1 << 8;
static const uint16_t kBtnDDown = 1 << 9;
static const uint16_t kBtnCLeft = 1 << 10;
static const uint16_t kBtnCRight = 1 << 11;
static const uint16_t kBtnCUp = 1 << 12;
static const uint16_t kBtnCDown = 1 << 13;

struct InpRow {
	uint8_t id = 0;
	uint16_t buttons = 0;
	uint8_t stick = 0;
	String name;
};

static void append_u16(Vector<uint8_t> &o, uint16_t v) {
	o.push_back(uint8_t(v & 0xff));
	o.push_back(uint8_t(v >> 8));
}

static uint16_t buttons_from_joy(JoyButton p_btn) {
	switch (p_btn) {
		case JoyButton::A:
			return kBtnA;
		case JoyButton::B:
			return kBtnB;
		case JoyButton::X:
			return kBtnZ;
		case JoyButton::START:
			return kBtnStart;
		case JoyButton::LEFT_SHOULDER:
			return kBtnL;
		case JoyButton::RIGHT_SHOULDER:
			return kBtnR;
		case JoyButton::DPAD_LEFT:
			return kBtnDLeft;
		case JoyButton::DPAD_RIGHT:
			return kBtnDRight;
		case JoyButton::DPAD_UP:
			return kBtnDUp;
		case JoyButton::DPAD_DOWN:
			return kBtnDDown;
		case JoyButton::Y:
			return kBtnCUp;
		case JoyButton::LEFT_STICK:
			return kBtnCLeft;
		case JoyButton::RIGHT_STICK:
			return kBtnCRight;
		case JoyButton::MISC1:
			return kBtnCDown;
		default:
			return 0;
	}
}

static void merge_events(const StringName &p_action, uint16_t &r_buttons, uint8_t &r_stick) {
	InputMap *im = InputMap::get_singleton();
	if (!im || !im->has_action(p_action)) {
		return;
	}
	const List<Ref<InputEvent>> *evs = im->action_get_events(p_action);
	if (!evs) {
		return;
	}
	for (const Ref<InputEvent> &e : *evs) {
		if (const InputEventJoypadButton *jb = Object::cast_to<InputEventJoypadButton>(e.ptr())) {
			r_buttons = uint16_t(r_buttons | buttons_from_joy(jb->get_button_index()));
		} else if (const InputEventJoypadMotion *jm = Object::cast_to<InputEventJoypadMotion>(e.ptr())) {
			const JoyAxis ax = jm->get_axis();
			if (ax == JoyAxis::LEFT_X) {
				r_stick = 1;
			} else if (ax == JoyAxis::LEFT_Y) {
				r_stick = 2;
			}
		}
	}
}

static int preferred_id(const String &p_name) {
	if (p_name == "ui_accept" || p_name == "accept") {
		return 0;
	}
	if (p_name == "ui_cancel" || p_name == "cancel") {
		return 1;
	}
	if (p_name == "ui_pause" || p_name == "pause") {
		return 2;
	}
	if (p_name == "ui_left") {
		return 3;
	}
	if (p_name == "ui_right") {
		return 4;
	}
	if (p_name == "ui_up") {
		return 5;
	}
	if (p_name == "ui_down") {
		return 6;
	}
	if (p_name == "c_left") {
		return 7;
	}
	if (p_name == "c_right") {
		return 8;
	}
	if (p_name == "c_up") {
		return 9;
	}
	if (p_name == "c_down") {
		return 10;
	}
	return -1;
}

Error write_inp_blob(Vector<uint8_t> &r_out, int &r_count) {
	Vector<InpRow> rows;
	const InpRow defaults[] = {
		{ 0, kBtnA, 0, "ui_accept" },
		{ 1, kBtnB, 0, "ui_cancel" },
		{ 2, kBtnStart, 0, "ui_pause" },
		{ 3, kBtnDLeft, 1, "ui_left" },
		{ 4, kBtnDRight, 1, "ui_right" },
		{ 5, kBtnDUp, 2, "ui_up" },
		{ 6, kBtnDDown, 2, "ui_down" },
		{ 7, kBtnCLeft, 0, "c_left" },
		{ 8, kBtnCRight, 0, "c_right" },
		{ 9, kBtnCUp, 0, "c_up" },
		{ 10, kBtnCDown, 0, "c_down" },
	};
	HashSet<String> seen;
	for (const InpRow &d : defaults) {
		InpRow row = d;
		merge_events(StringName(d.name), row.buttons, row.stick);
		if (row.buttons == 0 && row.stick == 0) {
			row.buttons = d.buttons;
			row.stick = d.stick;
		}
		rows.push_back(row);
		seen.insert(d.name);
	}

	InputMap *im = InputMap::get_singleton();
	if (im) {
		const List<StringName> actions = im->get_actions();
		for (const StringName &sn : actions) {
			const String name = String(sn);
			if (seen.has(name)) {
				continue;
			}
			uint16_t buttons = 0;
			uint8_t stick = 0;
			merge_events(sn, buttons, stick);
			if (!buttons && !stick) {
				continue;
			}
			if (rows.size() >= kMaxActions) {
				return ERR_OUT_OF_MEMORY;
			}
			InpRow row;
			const int pref = preferred_id(name);
			row.id = pref >= 0 ? uint8_t(pref) : uint8_t(rows.size());
			row.buttons = buttons;
			row.stick = stick;
			row.name = name;
			rows.push_back(row);
			seen.insert(name);
		}
	}

	if (rows.is_empty() || rows.size() > kMaxActions) {
		return ERR_OUT_OF_MEMORY;
	}

	r_out.clear();
	r_out.push_back('I');
	r_out.push_back('N');
	r_out.push_back('P');
	r_out.push_back('6');
	append_u16(r_out, uint16_t(kCookABI));
	append_u16(r_out, uint16_t(rows.size()));
	for (int i = 0; i < rows.size(); i++) {
		const InpRow &row = rows[i];
		r_out.push_back(row.id);
		append_u16(r_out, row.buttons);
		r_out.push_back(row.stick);
		const CharString utf = row.name.utf8();
		for (int c = 0; c < kNameLen; c++) {
			r_out.push_back(c < utf.length() ? uint8_t(utf[c]) : 0);
		}
		for (int z = 0; z < 4; z++) {
			r_out.push_back(0);
		}
	}
	r_count = rows.size();
	return OK;
}

} // namespace n64_cook
