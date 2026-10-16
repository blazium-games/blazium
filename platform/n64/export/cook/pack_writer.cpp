/**************************************************************************/
/*  pack_writer.cpp                                                       */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* SPDX-License-Identifier: MIT                                           */
/**************************************************************************/

#include "pack_writer.h"

#include "cooker.h"

namespace n64_cook {

static void append_u16(Vector<uint8_t> &o, uint16_t v) {
	o.push_back(uint8_t(v & 0xff));
	o.push_back(uint8_t(v >> 8));
}

static void append_u32(Vector<uint8_t> &o, uint32_t v) {
	o.push_back(uint8_t(v & 0xff));
	o.push_back(uint8_t((v >> 8) & 0xff));
	o.push_back(uint8_t((v >> 16) & 0xff));
	o.push_back(uint8_t((v >> 24) & 0xff));
}

Error write_pack_blob(int p_pack_id, const String &p_name, Vector<uint8_t> &r_out, uint32_t p_node_sz, uint32_t p_mesh_sz, uint32_t p_ntex_sz) {
	if (p_pack_id < 1 || p_pack_id > 23) {
		return ERR_OUT_OF_MEMORY;
	}
	const CharString utf = p_name.utf8();
	const int name_n = utf.length() < 16 ? utf.length() : 16;
	r_out.clear();
	r_out.push_back('P');
	r_out.push_back('A');
	r_out.push_back('C');
	r_out.push_back('K');
	append_u16(r_out, uint16_t(kCookABI));
	append_u16(r_out, uint16_t(p_pack_id));
	append_u32(r_out, uint32_t(16));
	for (int i = 0; i < 16; i++) {
		r_out.push_back(i < name_n ? uint8_t(utf[i]) : 0);
	}
	append_u32(r_out, p_node_sz);
	append_u32(r_out, p_mesh_sz);
	append_u32(r_out, p_ntex_sz);
	if (r_out.size() >= 2 && r_out[0] == 0x4D && r_out[1] == 0x5A) {
		return FAILED;
	}
	return OK;
}

} // namespace n64_cook
