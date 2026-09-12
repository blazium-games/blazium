/**************************************************************************/
/*  test_inter_dvd_instruction.h                                          */
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

#include "modules/inter_dvd/machine/inter_dvd_instruction.h"
#include "modules/inter_dvd/machine/inter_dvd_machine.h"
#include "tests/test_macros.h"

TEST_CASE("[Modules][InterDVD] groups 0-6 encode 8-byte commands") {
	for (int g = 0; g <= 6; g++) {
		String err;
		const PackedByteArray bytes = InterDVDInstruction::encode(InterDVDInstruction::Group(g), InterDVDInstruction::SET_ASSIGN, InterDVDInstruction::CMP_EQ, 1, false, 4, false, false, InterDVDInstruction::LINK_NONE, InterDVDInstruction::DOMAIN_FPC, InterDVDInstruction::DOMAIN_FPC, 0, &err);
		CHECK(bytes.size() == InterDVDMachine::INSTRUCTION_SIZE);
		CHECK((bytes[0] >> 5) == g);
		CHECK(((bytes[1] >> 5) & 0x07) == InterDVDInstruction::CMP_EQ);
	}
	CHECK(InterDVDInstruction::encode_nop().size() == 8);
	CHECK(InterDVDInstruction::encode_nop()[0] == 0);
}

TEST_CASE("[Modules][InterDVD] set-ops and conditionals") {
	const PackedByteArray set = InterDVDInstruction::encode_set(InterDVDInstruction::SET_ADD, 3, 10, false, false, nullptr);
	CHECK(set.size() == 8);
	CHECK((set[0] & 0x1F) == InterDVDInstruction::SET_ADD);
	CHECK(set[3] == 3);
	CHECK(set[5] == 10);

	String err;
	const PackedByteArray cmp = InterDVDInstruction::encode(InterDVDInstruction::GROUP_CMP_SET_LINK, InterDVDInstruction::SET_ASSIGN, InterDVDInstruction::CMP_GT, 0, false, InterDVDInstruction::SPRM_HL_BTNN, true, true, InterDVDInstruction::LINK_PGCN, InterDVDInstruction::DOMAIN_VMG, InterDVDInstruction::DOMAIN_VMG, 2, &err);
	CHECK(cmp.size() == 8);
	CHECK(((cmp[1] >> 5) & 0x07) == InterDVDInstruction::CMP_GT);
	CHECK((cmp[1] & 0x08) != 0);
	CHECK((cmp[1] & 0x04) != 0);
}

TEST_CASE("[Modules][InterDVD] Link Jump Call RSM and illegal transfers") {
	CHECK(InterDVDInstruction::encode_link(InterDVDInstruction::LINK_PGCN, InterDVDInstruction::DOMAIN_VMG, InterDVDInstruction::DOMAIN_VMG, 1, nullptr).size() == 8);
	CHECK(InterDVDInstruction::encode_link(InterDVDInstruction::JUMP_TT, InterDVDInstruction::DOMAIN_FPC, InterDVDInstruction::DOMAIN_VTST, 1, nullptr).size() == 8);
	CHECK(InterDVDInstruction::encode_link(InterDVDInstruction::CALL_SS, InterDVDInstruction::DOMAIN_VTST, InterDVDInstruction::DOMAIN_VMG, 1, nullptr).size() == 8);
	CHECK(InterDVDInstruction::encode_rsm(InterDVDInstruction::DOMAIN_VMG, nullptr).size() == 8);

	String err;
	CHECK(InterDVDInstruction::encode_link(InterDVDInstruction::LINK_PGCN, InterDVDInstruction::DOMAIN_VMG, InterDVDInstruction::DOMAIN_VTST, 1, &err).is_empty());
	CHECK(err.contains("Link"));
	err = String();
	CHECK(InterDVDInstruction::encode_link(InterDVDInstruction::JUMP_VTS_TT, InterDVDInstruction::DOMAIN_VTST, InterDVDInstruction::DOMAIN_VTST, 2, &err).size() == 8);
	err = String();
	CHECK(InterDVDInstruction::encode_link(InterDVDInstruction::JUMP_TT, InterDVDInstruction::DOMAIN_VTST, InterDVDInstruction::DOMAIN_VTST, 2, &err).is_empty());
	CHECK(err.contains("JumpTT"));
	err = String();
	CHECK(InterDVDInstruction::encode_link(InterDVDInstruction::CALL_SS, InterDVDInstruction::DOMAIN_VMG, InterDVDInstruction::DOMAIN_VTSM, 1, &err).is_empty());
	CHECK(err.contains("VTST"));
	err = String();
	CHECK(InterDVDInstruction::encode_rsm(InterDVDInstruction::DOMAIN_VTST, &err).is_empty());
	CHECK(err.contains("RSM"));
	err = String();
	CHECK(InterDVDInstruction::encode_link(InterDVDInstruction::JUMP_SS, InterDVDInstruction::DOMAIN_VTST, InterDVDInstruction::DOMAIN_VMG, 2, &err).is_empty());
	CHECK(err.contains("JumpSS"));
	const PackedByteArray jump_ss = InterDVDInstruction::encode_link(InterDVDInstruction::JUMP_SS, InterDVDInstruction::DOMAIN_VMG, InterDVDInstruction::DOMAIN_VMG, 5, nullptr);
	REQUIRE(jump_ss.size() == 8);
	CHECK(jump_ss[0] == 0x30);
	CHECK(jump_ss[1] == 0x06);
	CHECK(jump_ss[5] == 0x45);
	const PackedByteArray link_ptt = InterDVDInstruction::encode_link(InterDVDInstruction::LINK_PTT, InterDVDInstruction::DOMAIN_VTST, InterDVDInstruction::DOMAIN_VTST, 2, nullptr);
	REQUIRE(link_ptt.size() == 8);
	CHECK(link_ptt[0] == 0x20);
	CHECK(link_ptt[1] == 0x05);
	CHECK(link_ptt[7] == 2);
	const PackedByteArray link_cn = InterDVDInstruction::encode_link(InterDVDInstruction::LINK_CN, InterDVDInstruction::DOMAIN_VTST, InterDVDInstruction::DOMAIN_VTST, 3, nullptr);
	CHECK(link_cn[0] == 0x20);
	CHECK(link_cn[1] == 0x07);
	CHECK(link_cn[7] == 3);
}

TEST_CASE("[Modules][InterDVD] SetSTN Exit Goto Break and SPRM helpers") {
	const PackedByteArray stn = InterDVDInstruction::encode_set_stn(1, 2, true, 1, nullptr);
	REQUIRE(stn.size() == 8);
	CHECK(stn[0] == 0x51);
	CHECK(stn[4] == 0x81);
	CHECK(stn[5] == 0xC2);
	CHECK(stn[6] == 0x81);
	const PackedByteArray hl = InterDVDInstruction::encode_set_hl_btnn(4, nullptr);
	CHECK(hl[0] == 0x46);
	CHECK(hl[5] == 4);
	CHECK(InterDVDInstruction::encode_set_hl_btnn(0, nullptr).is_empty());
	const PackedByteArray exit_cmd = InterDVDInstruction::encode_exit();
	CHECK(exit_cmd[0] == 0x30);
	CHECK(exit_cmd[1] == 0x01);
	const PackedByteArray go = InterDVDInstruction::encode_goto(3, nullptr);
	CHECK(go[1] == 0x01);
	CHECK(go[7] == 3);
	CHECK(InterDVDInstruction::encode_break()[1] == 0x02);
	const PackedByteArray timer = InterDVDInstruction::encode_set_nvtmr(12, 1, nullptr);
	CHECK(timer[0] == 0x42);
	CHECK(timer[5] == 12);
	const PackedByteArray md = InterDVDInstruction::encode_set_gprmmd(2, 7, true, nullptr);
	CHECK(md[0] == 0x47);
	CHECK(md[4] == 1);
	CHECK(md[5] == 2);
	CHECK(int(InterDVDInstruction::SPRM_TT_N) == 4);
	CHECK(int(InterDVDInstruction::SPRM_PTT_N) == 7);
	CHECK(int(InterDVDInstruction::SPRM_NVTMR) == 9);
	CHECK(int(InterDVDInstruction::SPRM_REGION) == 20);
}
