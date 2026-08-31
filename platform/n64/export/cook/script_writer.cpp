/**************************************************************************/
/*  script_writer.cpp                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* SPDX-License-Identifier: MIT                                           */
/**************************************************************************/

#include "script_writer.h"

#include "cooker.h"

namespace n64_cook {

enum {
	OP_END = 0,
	OP_LOAD_DELTA = 2,
	OP_CALL_ROTATE_Y = 4,
};

static void append_u16(Vector<uint8_t> &o, uint16_t v) {
	o.push_back(uint8_t(v & 0xff));
	o.push_back(uint8_t(v >> 8));
}

bool source_is_forbidden(const String &p_src) {
	return p_src.contains("eval(") || p_src.contains("load(") || p_src.contains("HTTPClient") || p_src.contains("add_child") || p_src.contains("ClassDB");
}

Error write_script_blob(const Vector<String> &p_sources, Vector<uint8_t> &r_out) {
	for (int i = 0; i < p_sources.size(); i++) {
		if (source_is_forbidden(p_sources[i])) {
			return FAILED;
		}
	}
	Vector<uint8_t> code;
	bool rotate_y = false;
	for (int i = 0; i < p_sources.size(); i++) {
		if (p_sources[i].contains("rotate_y")) {
			rotate_y = true;
			break;
		}
	}
	if (rotate_y) {
		code.push_back(uint8_t(OP_LOAD_DELTA));
		code.push_back(uint8_t(OP_CALL_ROTATE_Y));
	}
	code.push_back(uint8_t(OP_END));
	r_out.clear();
	r_out.push_back('S');
	r_out.push_back('C');
	r_out.push_back('R');
	r_out.push_back('P');
	append_u16(r_out, uint16_t(kCookABI));
	append_u16(r_out, uint16_t(code.size()));
	for (int i = 0; i < code.size(); i++) {
		r_out.push_back(code[i]);
	}
	if (r_out.size() >= 2 && r_out[0] == 0x4D && r_out[1] == 0x5A) {
		return FAILED;
	}
	return OK;
}

} // namespace n64_cook
