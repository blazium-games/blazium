/**************************************************************************/
/*  inter_dvd_instruction.h                                               */
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

#pragma once

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/variant/typed_array.h"

class InterDVDInstruction : public Object {
	GDCLASS(InterDVDInstruction, Object);

public:
	enum Group {
		GROUP_SPECIAL = 0,
		GROUP_LINK = 1,
		GROUP_SET_SYSTEM = 2,
		GROUP_SET = 3,
		GROUP_SET_LINK = 4,
		GROUP_CMP_SET_SYSTEM = 5,
		GROUP_CMP_SET_LINK = 6,
	};

	enum Compare {
		CMP_ALWAYS = 0,
		CMP_AND = 1,
		CMP_EQ = 2,
		CMP_NE = 3,
		CMP_GE = 4,
		CMP_GT = 5,
		CMP_LE = 6,
		CMP_LT = 7,
	};

	enum SetOp {
		SET_NOP = 0,
		SET_ASSIGN = 1,
		SET_SWAP = 2,
		SET_ADD = 3,
		SET_SUB = 4,
		SET_MUL = 5,
		SET_DIV = 6,
		SET_MOD = 7,
		SET_RANDOM = 8,
		SET_AND = 9,
		SET_OR = 10,
		SET_XOR = 11,
	};

	enum LinkKind {
		LINK_NONE = 0,
		LINK_PGCN = 1,
		LINK_PGN = 2,
		LINK_PTT = 3,
		LINK_RSM = 4,
		JUMP_TT = 5,
		JUMP_VTS_TT = 6,
		JUMP_VTS_PTT = 7,
		CALL_SS = 8,
		JUMP_SS = 9,
		LINK_CN = 10,
	};

	enum Domain {
		DOMAIN_FPC = 0,
		DOMAIN_VMG = 1,
		DOMAIN_VTSM = 2,
		DOMAIN_VTST = 3,
	};

	enum SPRM {
		SPRM_M_ASN = 0,
		SPRM_ASTN = 1,
		SPRM_SPSTN = 2,
		SPRM_AGL_N = 3,
		SPRM_TT_N = 4,
		SPRM_VTS_TT_N = 5,
		SPRM_TT_VTS_N = 6,
		SPRM_PTT_N = 7,
		SPRM_HL_BTNN = 8,
		SPRM_NVTMR = 9,
		SPRM_NV_PGC = 10,
		SPRM_P_N = 11,
		SPRM_PTT_N2 = 12,
		SPRM_CN = 13,
		SPRM_VIDEO_CFG = 14,
		SPRM_AUDIO_CFG = 15,
		SPRM_INI_AUD_LANG = 16,
		SPRM_INI_AUD_EXT = 17,
		SPRM_INI_SP_LANG = 18,
		SPRM_INI_SP_EXT = 19,
		SPRM_REGION = 20,
		SPRM_RESERVED_21 = 21,
		SPRM_RESERVED_22 = 22,
		SPRM_RESERVED_23 = 23,
	};

	static bool validate_transfer(Domain p_from, Domain p_to, LinkKind p_kind, String *r_error = nullptr);
	static PackedByteArray encode(Group p_group, int p_op, Compare p_cmp, int p_dest_reg, bool p_dest_sprm, int p_src, bool p_src_reg, bool p_src_sprm, LinkKind p_link, Domain p_from, Domain p_to, int p_link_target, String *r_error = nullptr);

	static PackedByteArray encode_nop();
	static PackedByteArray encode_link(LinkKind p_kind, Domain p_from, Domain p_to, int p_target, String *r_error = nullptr);
	static PackedByteArray encode_set(SetOp p_op, int p_gprm, int p_src, bool p_src_reg, bool p_src_sprm, String *r_error = nullptr);
	static PackedByteArray encode_rsm(Domain p_from, String *r_error = nullptr);
	static PackedByteArray encode_jump_vts_ptt(Domain p_from, int p_title, int p_ptt, String *r_error = nullptr);
	static PackedByteArray encode_set_stn(int p_audio, int p_subtitle, bool p_subtitle_on, int p_angle, String *r_error = nullptr);
	static PackedByteArray encode_set_hl_btnn(int p_button, String *r_error = nullptr);
	static PackedByteArray encode_set_nvtmr(int p_seconds, int p_pgc, String *r_error = nullptr);
	static PackedByteArray encode_set_gprmmd(int p_gprm, int p_value, bool p_counter, String *r_error = nullptr);
	static PackedByteArray encode_exit();
	static PackedByteArray encode_goto(int p_line, String *r_error = nullptr);
	static PackedByteArray encode_break();

protected:
	static void _bind_methods();

private:
	static PackedByteArray _encode_script(int p_group, int p_op, int p_cmp, int p_dest_reg, bool p_dest_sprm, int p_src, bool p_src_reg, bool p_src_sprm, int p_link, int p_from, int p_to, int p_link_target);
};

VARIANT_ENUM_CAST(InterDVDInstruction::Group);
VARIANT_ENUM_CAST(InterDVDInstruction::Compare);
VARIANT_ENUM_CAST(InterDVDInstruction::SetOp);
VARIANT_ENUM_CAST(InterDVDInstruction::LinkKind);
VARIANT_ENUM_CAST(InterDVDInstruction::Domain);
VARIANT_ENUM_CAST(InterDVDInstruction::SPRM);
