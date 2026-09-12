/**************************************************************************/
/*  inter_dvd_instruction.cpp                                             */
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

#include "inter_dvd_instruction.h"

#include "core/math/math_funcs.h"
#include "core/object/class_db.h"

bool InterDVDInstruction::validate_transfer(Domain p_from, Domain p_to, LinkKind p_kind, String *r_error) {
	if (p_kind == LINK_NONE) {
		return true;
	}
	if (p_kind == LINK_RSM) {
		if (p_from != DOMAIN_VMG && p_from != DOMAIN_VTSM) {
			if (r_error) {
				*r_error = "RSM is only legal from VMG or VTSM menu domains.";
			}
			return false;
		}
		return true;
	}

	if (p_kind == JUMP_SS && p_from == DOMAIN_VTST) {
		if (r_error) {
			*r_error = "JumpSS is illegal from VTST; use CallSS so RSM can return.";
		}
		return false;
	}

	const bool is_link = p_kind == LINK_PGCN || p_kind == LINK_PGN || p_kind == LINK_PTT || p_kind == LINK_CN;
	const bool is_jump = p_kind == JUMP_TT || p_kind == JUMP_VTS_TT || p_kind == JUMP_VTS_PTT || p_kind == JUMP_SS;
	const bool is_call = p_kind == CALL_SS;

	if (is_link && p_from != p_to) {
		if (r_error) {
			*r_error = "Link stays in the current domain.";
		}
		return false;
	}
	if (is_call && p_from != DOMAIN_VTST) {
		if (r_error) {
			*r_error = "Call is only legal from VTST (one-deep resume).";
		}
		return false;
	}
	if (is_call && p_to != DOMAIN_VMG && p_to != DOMAIN_VTSM) {
		if (r_error) {
			*r_error = "Call target must be VMG or VTSM.";
		}
		return false;
	}
	if (is_jump && p_from == DOMAIN_VTST && p_to == DOMAIN_VTST && p_kind == JUMP_TT) {
		if (r_error) {
			*r_error = "JumpTT is illegal from VTST; use JumpVTS_TT in the same title set.";
		}
		return false;
	}
	if (is_jump && p_from == DOMAIN_VTST && (p_to == DOMAIN_VMG || p_to == DOMAIN_VTSM)) {
		if (r_error) {
			*r_error = "VTST must Call (not Jump) into a menu domain so RSM can return.";
		}
		return false;
	}
	return true;
}

PackedByteArray InterDVDInstruction::encode(Group p_group, int p_op, Compare p_cmp, int p_dest_reg, bool p_dest_sprm, int p_src, bool p_src_reg, bool p_src_sprm, LinkKind p_link, Domain p_from, Domain p_to, int p_link_target, String *r_error) {
	if (p_group < GROUP_SPECIAL || p_group > GROUP_CMP_SET_LINK) {
		if (r_error) {
			*r_error = "Instruction group must be 0-6.";
		}
		return PackedByteArray();
	}
	if (p_dest_sprm) {
		if (p_dest_reg < 0 || p_dest_reg > 23) {
			if (r_error) {
				*r_error = "SPRM index must be 0-23.";
			}
			return PackedByteArray();
		}
	} else if (p_dest_reg < 0 || p_dest_reg > 15) {
		if (r_error) {
			*r_error = "GPRM index must be 0-15.";
		}
		return PackedByteArray();
	}
	if (!validate_transfer(p_from, p_to, p_link, r_error)) {
		return PackedByteArray();
	}

	PackedByteArray bytes;
	bytes.resize(8);
	bytes.write[0] = uint8_t((int(p_group) << 5) | (p_op & 0x1F));
	uint8_t flags = uint8_t((int(p_cmp) & 0x07) << 5);
	if (p_dest_sprm) {
		flags |= 0x10;
	}
	if (p_src_reg) {
		flags |= 0x08;
	}
	if (p_src_sprm) {
		flags |= 0x04;
	}
	bytes.write[1] = flags;
	bytes.write[2] = uint8_t((p_dest_reg >> 8) & 0xFF);
	bytes.write[3] = uint8_t(p_dest_reg & 0xFF);
	bytes.write[4] = uint8_t((p_src >> 8) & 0xFF);
	bytes.write[5] = uint8_t(p_src & 0xFF);
	bytes.write[6] = uint8_t(((int(p_link) & 0x0F) << 4) | ((p_link_target >> 8) & 0x0F));
	bytes.write[7] = uint8_t(p_link_target & 0xFF);
	return bytes;
}

PackedByteArray InterDVDInstruction::encode_nop() {
	PackedByteArray bytes;
	bytes.resize(8);
	bytes.fill(0);
	return bytes;
}

static PackedByteArray _hw8() {
	PackedByteArray bytes;
	bytes.resize(8);
	bytes.fill(0);
	return bytes;
}

static uint8_t _menu_id(int p_target) {
	if (p_target >= 2 && p_target <= 7) {
		return uint8_t(p_target);
	}
	return 2;
}

PackedByteArray InterDVDInstruction::encode_link(LinkKind p_kind, Domain p_from, Domain p_to, int p_target, String *r_error) {
	if (!validate_transfer(p_from, p_to, p_kind, r_error)) {
		return PackedByteArray();
	}
	PackedByteArray out = _hw8();
	const int t = p_target > 0 ? p_target : 1;
	switch (p_kind) {
		case LINK_PGCN: {
			const uint16_t pgcn = uint16_t(CLAMP(t, 1, 999));
			out.write[0] = 0x20;
			out.write[1] = 0x04;
			out.write[6] = uint8_t((pgcn >> 8) & 0xFF);
			out.write[7] = uint8_t(pgcn & 0xFF);
			return out;
		}
		case LINK_PTT: {
			const uint16_t ptt = uint16_t(CLAMP(t, 1, 999));
			out.write[0] = 0x20;
			out.write[1] = 0x05;
			out.write[6] = uint8_t((ptt >> 8) & 0xFF);
			out.write[7] = uint8_t(ptt & 0xFF);
			return out;
		}
		case LINK_PGN:
			out.write[0] = 0x20;
			out.write[1] = 0x06;
			out.write[7] = uint8_t(CLAMP(t, 1, 99));
			return out;
		case LINK_CN:
			out.write[0] = 0x20;
			out.write[1] = 0x07;
			out.write[7] = uint8_t(CLAMP(t, 1, 255));
			return out;
		case LINK_RSM:
			out.write[0] = 0x20;
			out.write[1] = 0x10;
			return out;
		case JUMP_TT:
			out.write[0] = 0x30;
			out.write[1] = 0x02;
			out.write[5] = uint8_t(CLAMP(t, 1, 99));
			return out;
		case JUMP_VTS_TT:
			out.write[0] = 0x30;
			out.write[1] = 0x03;
			out.write[5] = uint8_t(CLAMP(t, 1, 99));
			return out;
		case JUMP_VTS_PTT: {
			const uint16_t ptt = uint16_t(CLAMP(t, 1, 999));
			out.write[0] = 0x30;
			out.write[1] = 0x05;
			out.write[5] = 1;
			out.write[6] = uint8_t((ptt >> 8) & 0xFF);
			out.write[7] = uint8_t(ptt & 0xFF);
			return out;
		}
		case JUMP_SS:
			out.write[0] = 0x30;
			out.write[1] = 0x06;
			out.write[5] = uint8_t(0x40 | _menu_id(p_target));
			return out;
		case CALL_SS:
			out.write[0] = 0x30;
			out.write[1] = 0x08;
			out.write[4] = 1;
			out.write[5] = uint8_t(0x40 | _menu_id(p_target));
			return out;
		default:
			return encode(GROUP_LINK, int(p_kind), CMP_ALWAYS, 0, false, 0, false, false, p_kind, p_from, p_to, p_target, r_error);
	}
}

PackedByteArray InterDVDInstruction::encode_set(SetOp p_op, int p_gprm, int p_src, bool p_src_reg, bool p_src_sprm, String *r_error) {
	return encode(GROUP_SET, int(p_op), CMP_ALWAYS, p_gprm, false, p_src, p_src_reg, p_src_sprm, LINK_NONE, DOMAIN_FPC, DOMAIN_FPC, 0, r_error);
}

PackedByteArray InterDVDInstruction::encode_rsm(Domain p_from, String *r_error) {
	return encode_link(LINK_RSM, p_from, p_from, 0, r_error);
}

PackedByteArray InterDVDInstruction::encode_jump_vts_ptt(Domain p_from, int p_title, int p_ptt, String *r_error) {
	if (!validate_transfer(p_from, DOMAIN_VTST, JUMP_VTS_PTT, r_error)) {
		return PackedByteArray();
	}
	PackedByteArray out = _hw8();
	const uint16_t ptt = uint16_t(CLAMP(p_ptt > 0 ? p_ptt : 1, 1, 999));
	out.write[0] = 0x30;
	out.write[1] = 0x05;
	out.write[5] = uint8_t(CLAMP(p_title > 0 ? p_title : 1, 1, 99));
	out.write[6] = uint8_t((ptt >> 8) & 0xFF);
	out.write[7] = uint8_t(ptt & 0xFF);
	return out;
}

PackedByteArray InterDVDInstruction::encode_set_stn(int p_audio, int p_subtitle, bool p_subtitle_on, int p_angle, String *r_error) {
	(void)r_error;
	PackedByteArray out = _hw8();
	out.write[0] = 0x51;
	if (p_audio >= 0) {
		out.write[4] = uint8_t(0x80 | (CLAMP(p_audio, 0, 7) & 0x07));
	}
	if (p_subtitle >= 0) {
		uint8_t sp = uint8_t(0x80 | (CLAMP(p_subtitle, 0, 31) & 0x1F));
		if (p_subtitle_on) {
			sp |= 0x40;
		}
		out.write[5] = sp;
	}
	if (p_angle >= 0) {
		out.write[6] = uint8_t(0x80 | (CLAMP(p_angle, 1, 9) & 0x0F));
	}
	return out;
}

PackedByteArray InterDVDInstruction::encode_set_hl_btnn(int p_button, String *r_error) {
	if (p_button < 1 || p_button > 36) {
		if (r_error) {
			*r_error = "Highlighted button must be 1-36.";
		}
		return PackedByteArray();
	}
	PackedByteArray out = _hw8();
	out.write[0] = 0x46;
	out.write[5] = uint8_t(p_button);
	return out;
}

PackedByteArray InterDVDInstruction::encode_set_nvtmr(int p_seconds, int p_pgc, String *r_error) {
	if (p_seconds < 0 || p_seconds > 0xFFFF) {
		if (r_error) {
			*r_error = "Navigation timer must be 0-65535 seconds.";
		}
		return PackedByteArray();
	}
	PackedByteArray out = _hw8();
	out.write[0] = 0x42;
	out.write[4] = uint8_t((p_seconds >> 8) & 0xFF);
	out.write[5] = uint8_t(p_seconds & 0xFF);
	const uint16_t pgc = uint16_t(CLAMP(p_pgc > 0 ? p_pgc : 1, 1, 999));
	out.write[6] = uint8_t((pgc >> 8) & 0xFF);
	out.write[7] = uint8_t(pgc & 0xFF);
	return out;
}

PackedByteArray InterDVDInstruction::encode_set_gprmmd(int p_gprm, int p_value, bool p_counter, String *r_error) {
	if (p_gprm < 0 || p_gprm > 15) {
		if (r_error) {
			*r_error = "GPRM index must be 0-15.";
		}
		return PackedByteArray();
	}
	PackedByteArray out = _hw8();
	out.write[0] = 0x47;
	out.write[4] = p_counter ? 1 : 0;
	out.write[5] = uint8_t(p_gprm);
	out.write[6] = uint8_t((p_value >> 8) & 0xFF);
	out.write[7] = uint8_t(p_value & 0xFF);
	return out;
}

PackedByteArray InterDVDInstruction::encode_exit() {
	PackedByteArray out = _hw8();
	out.write[0] = 0x30;
	out.write[1] = 0x01;
	return out;
}

PackedByteArray InterDVDInstruction::encode_goto(int p_line, String *r_error) {
	if (p_line < 1 || p_line > 128) {
		if (r_error) {
			*r_error = "Goto line must be 1-128.";
		}
		return PackedByteArray();
	}
	PackedByteArray out = _hw8();
	out.write[1] = 0x01;
	out.write[6] = uint8_t((p_line >> 8) & 0xFF);
	out.write[7] = uint8_t(p_line & 0xFF);
	return out;
}

PackedByteArray InterDVDInstruction::encode_break() {
	PackedByteArray out = _hw8();
	out.write[1] = 0x02;
	return out;
}

PackedByteArray InterDVDInstruction::_encode_script(int p_group, int p_op, int p_cmp, int p_dest_reg, bool p_dest_sprm, int p_src, bool p_src_reg, bool p_src_sprm, int p_link, int p_from, int p_to, int p_link_target) {
	return encode(Group(p_group), p_op, Compare(p_cmp), p_dest_reg, p_dest_sprm, p_src, p_src_reg, p_src_sprm, LinkKind(p_link), Domain(p_from), Domain(p_to), p_link_target, nullptr);
}

static PackedByteArray _encode_link_bind(int p_kind, int p_from, int p_to, int p_target) {
	return InterDVDInstruction::encode_link(InterDVDInstruction::LinkKind(p_kind), InterDVDInstruction::Domain(p_from), InterDVDInstruction::Domain(p_to), p_target, nullptr);
}

static PackedByteArray _encode_set_bind(int p_op, int p_gprm, int p_src, bool p_src_reg, bool p_src_sprm) {
	return InterDVDInstruction::encode_set(InterDVDInstruction::SetOp(p_op), p_gprm, p_src, p_src_reg, p_src_sprm, nullptr);
}

static PackedByteArray _encode_rsm_bind(int p_from) {
	return InterDVDInstruction::encode_rsm(InterDVDInstruction::Domain(p_from), nullptr);
}

static PackedByteArray _encode_jump_vts_ptt_bind(int p_from, int p_title, int p_ptt) {
	return InterDVDInstruction::encode_jump_vts_ptt(InterDVDInstruction::Domain(p_from), p_title, p_ptt, nullptr);
}

static PackedByteArray _encode_set_stn_bind(int p_audio, int p_subtitle, bool p_subtitle_on, int p_angle) {
	return InterDVDInstruction::encode_set_stn(p_audio, p_subtitle, p_subtitle_on, p_angle, nullptr);
}

static PackedByteArray _encode_set_hl_btnn_bind(int p_button) {
	return InterDVDInstruction::encode_set_hl_btnn(p_button, nullptr);
}

static PackedByteArray _encode_set_nvtmr_bind(int p_seconds, int p_pgc) {
	return InterDVDInstruction::encode_set_nvtmr(p_seconds, p_pgc, nullptr);
}

static PackedByteArray _encode_set_gprmmd_bind(int p_gprm, int p_value, bool p_counter) {
	return InterDVDInstruction::encode_set_gprmmd(p_gprm, p_value, p_counter, nullptr);
}

static PackedByteArray _encode_goto_bind(int p_line) {
	return InterDVDInstruction::encode_goto(p_line, nullptr);
}

void InterDVDInstruction::_bind_methods() {
	ClassDB::bind_static_method("InterDVDInstruction", D_METHOD("encode", "group", "op", "cmp", "dest_reg", "dest_sprm", "src", "src_reg", "src_sprm", "link", "from_domain", "to_domain", "link_target"), &InterDVDInstruction::_encode_script);
	ClassDB::bind_static_method("InterDVDInstruction", D_METHOD("encode_nop"), &InterDVDInstruction::encode_nop);
	ClassDB::bind_static_method("InterDVDInstruction", D_METHOD("encode_link", "kind", "from_domain", "to_domain", "target"), &_encode_link_bind);
	ClassDB::bind_static_method("InterDVDInstruction", D_METHOD("encode_set", "op", "gprm", "src", "src_reg", "src_sprm"), &_encode_set_bind);
	ClassDB::bind_static_method("InterDVDInstruction", D_METHOD("encode_rsm", "from_domain"), &_encode_rsm_bind);
	ClassDB::bind_static_method("InterDVDInstruction", D_METHOD("encode_jump_vts_ptt", "from_domain", "title", "ptt"), &_encode_jump_vts_ptt_bind);
	ClassDB::bind_static_method("InterDVDInstruction", D_METHOD("encode_set_stn", "audio", "subtitle", "subtitle_on", "angle"), &_encode_set_stn_bind);
	ClassDB::bind_static_method("InterDVDInstruction", D_METHOD("encode_set_hl_btnn", "button"), &_encode_set_hl_btnn_bind);
	ClassDB::bind_static_method("InterDVDInstruction", D_METHOD("encode_set_nvtmr", "seconds", "pgc"), &_encode_set_nvtmr_bind);
	ClassDB::bind_static_method("InterDVDInstruction", D_METHOD("encode_set_gprmmd", "gprm", "value", "counter"), &_encode_set_gprmmd_bind);
	ClassDB::bind_static_method("InterDVDInstruction", D_METHOD("encode_exit"), &InterDVDInstruction::encode_exit);
	ClassDB::bind_static_method("InterDVDInstruction", D_METHOD("encode_goto", "line"), &_encode_goto_bind);
	ClassDB::bind_static_method("InterDVDInstruction", D_METHOD("encode_break"), &InterDVDInstruction::encode_break);

	BIND_ENUM_CONSTANT(GROUP_SPECIAL);
	BIND_ENUM_CONSTANT(GROUP_LINK);
	BIND_ENUM_CONSTANT(GROUP_SET_SYSTEM);
	BIND_ENUM_CONSTANT(GROUP_SET);
	BIND_ENUM_CONSTANT(GROUP_SET_LINK);
	BIND_ENUM_CONSTANT(GROUP_CMP_SET_SYSTEM);
	BIND_ENUM_CONSTANT(GROUP_CMP_SET_LINK);

	BIND_ENUM_CONSTANT(CMP_ALWAYS);
	BIND_ENUM_CONSTANT(CMP_AND);
	BIND_ENUM_CONSTANT(CMP_EQ);
	BIND_ENUM_CONSTANT(CMP_NE);
	BIND_ENUM_CONSTANT(CMP_GE);
	BIND_ENUM_CONSTANT(CMP_GT);
	BIND_ENUM_CONSTANT(CMP_LE);
	BIND_ENUM_CONSTANT(CMP_LT);

	BIND_ENUM_CONSTANT(SET_NOP);
	BIND_ENUM_CONSTANT(SET_ASSIGN);
	BIND_ENUM_CONSTANT(SET_SWAP);
	BIND_ENUM_CONSTANT(SET_ADD);
	BIND_ENUM_CONSTANT(SET_SUB);
	BIND_ENUM_CONSTANT(SET_MUL);
	BIND_ENUM_CONSTANT(SET_DIV);
	BIND_ENUM_CONSTANT(SET_MOD);
	BIND_ENUM_CONSTANT(SET_RANDOM);
	BIND_ENUM_CONSTANT(SET_AND);
	BIND_ENUM_CONSTANT(SET_OR);
	BIND_ENUM_CONSTANT(SET_XOR);

	BIND_ENUM_CONSTANT(LINK_NONE);
	BIND_ENUM_CONSTANT(LINK_PGCN);
	BIND_ENUM_CONSTANT(LINK_PGN);
	BIND_ENUM_CONSTANT(LINK_PTT);
	BIND_ENUM_CONSTANT(LINK_RSM);
	BIND_ENUM_CONSTANT(JUMP_TT);
	BIND_ENUM_CONSTANT(JUMP_VTS_TT);
	BIND_ENUM_CONSTANT(JUMP_VTS_PTT);
	BIND_ENUM_CONSTANT(CALL_SS);
	BIND_ENUM_CONSTANT(JUMP_SS);
	BIND_ENUM_CONSTANT(LINK_CN);

	BIND_ENUM_CONSTANT(DOMAIN_FPC);
	BIND_ENUM_CONSTANT(DOMAIN_VMG);
	BIND_ENUM_CONSTANT(DOMAIN_VTSM);
	BIND_ENUM_CONSTANT(DOMAIN_VTST);

	BIND_ENUM_CONSTANT(SPRM_M_ASN);
	BIND_ENUM_CONSTANT(SPRM_ASTN);
	BIND_ENUM_CONSTANT(SPRM_SPSTN);
	BIND_ENUM_CONSTANT(SPRM_AGL_N);
	BIND_ENUM_CONSTANT(SPRM_TT_N);
	BIND_ENUM_CONSTANT(SPRM_VTS_TT_N);
	BIND_ENUM_CONSTANT(SPRM_TT_VTS_N);
	BIND_ENUM_CONSTANT(SPRM_PTT_N);
	BIND_ENUM_CONSTANT(SPRM_HL_BTNN);
	BIND_ENUM_CONSTANT(SPRM_NVTMR);
	BIND_ENUM_CONSTANT(SPRM_NV_PGC);
	BIND_ENUM_CONSTANT(SPRM_P_N);
	BIND_ENUM_CONSTANT(SPRM_PTT_N2);
	BIND_ENUM_CONSTANT(SPRM_CN);
	BIND_ENUM_CONSTANT(SPRM_VIDEO_CFG);
	BIND_ENUM_CONSTANT(SPRM_AUDIO_CFG);
	BIND_ENUM_CONSTANT(SPRM_INI_AUD_LANG);
	BIND_ENUM_CONSTANT(SPRM_INI_AUD_EXT);
	BIND_ENUM_CONSTANT(SPRM_INI_SP_LANG);
	BIND_ENUM_CONSTANT(SPRM_INI_SP_EXT);
	BIND_ENUM_CONSTANT(SPRM_REGION);
	BIND_ENUM_CONSTANT(SPRM_RESERVED_21);
	BIND_ENUM_CONSTANT(SPRM_RESERVED_22);
	BIND_ENUM_CONSTANT(SPRM_RESERVED_23);
}
