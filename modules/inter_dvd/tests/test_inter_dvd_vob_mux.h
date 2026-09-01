/**************************************************************************/
/*  test_inter_dvd_vob_mux.h                                              */
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

#include "core/io/file_access.h"
#include "modules/inter_dvd/author/inter_dvd_project.h"
#include "modules/inter_dvd/author/inter_dvd_vob_mux.h"
#include "tests/test_macros.h"
#include "tests/test_utils.h"

TEST_CASE("[Modules][InterDVD] finalize rewrites empty ffmpeg NAV vobu_ea") {
	const String path = TestUtils::get_temp_path("inter_dvd_rewrite_nav.vob");
	Ref<FileAccess> f = FileAccess::open(path, FileAccess::WRITE);
	REQUIRE(f.is_valid());
	Vector<uint8_t> nav;
	nav.resize(InterDVDVobMux::SECTOR_SIZE);
	nav.fill(0);
	nav.write[0] = 0x00;
	nav.write[1] = 0x00;
	nav.write[2] = 0x01;
	nav.write[3] = 0xBA;
	nav.write[38] = 0x00;
	nav.write[39] = 0x00;
	nav.write[40] = 0x01;
	nav.write[41] = 0xBF;
	nav.write[57] = 0x00;
	nav.write[58] = 0x01;
	nav.write[59] = 0x5F;
	nav.write[60] = 0x90;
	nav.write[61] = 0x00;
	nav.write[62] = 0x02;
	nav.write[63] = 0xBF;
	nav.write[64] = 0x20;
	f->store_buffer(nav.ptr(), nav.size());
	Vector<uint8_t> video;
	video.resize(InterDVDVobMux::SECTOR_SIZE);
	video.fill(0);
	video.write[0] = 0x00;
	video.write[1] = 0x00;
	video.write[2] = 0x01;
	video.write[3] = 0xBA;
	video.write[38] = 0x00;
	video.write[39] = 0x00;
	video.write[40] = 0x01;
	video.write[41] = 0xE0;
	f->store_buffer(video.ptr(), video.size());
	f.unref();

	CHECK(InterDVDVobMux::finalize_title_vob(path) == OK);
	Ref<FileAccess> in = FileAccess::open(path, FileAccess::READ);
	REQUIRE(in.is_valid());
	const Vector<uint8_t> out = in->get_buffer(InterDVDVobMux::SECTOR_SIZE);
	REQUIRE(out.size() >= 1043);
	CHECK(out[1039] == 0);
	CHECK(out[1040] == 0);
	CHECK(out[1041] == 0);
	CHECK(out[1042] == 1);
	CHECK(out[57] == 0);
	CHECK(out[58] == 1);
	CHECK(out[59] == 0x5F);
	CHECK(out[60] == 0x90);
	CHECK(out[69] == 0);
	CHECK(out[70] == 0);
	CHECK(out[71] == 1);
	CHECK((out[72] & 0xC0) == 0xC0);
	CHECK(out[4] == 0x44);

	CHECK(out[1031 + 0xEA + 80] == 0x3F);
	CHECK(out[1031 + 0xEA + 81] == 0xFF);
	CHECK(out[1031 + 0xEA + 82] == 0xFF);
	CHECK(out[1031 + 0xEA + 83] == 0xFF);
	CHECK(out[1031 + 0xEA + 168] == 0x3F);
	CHECK(out[1031 + 0xEA + 169] == 0xFF);
}

TEST_CASE("[Modules][InterDVD] finalize injects NAV with nonzero vobu_ea") {
	const String path = TestUtils::get_temp_path("inter_dvd_inject_nav.vob");
	Ref<FileAccess> f = FileAccess::open(path, FileAccess::WRITE);
	REQUIRE(f.is_valid());
	Vector<uint8_t> sec;
	sec.resize(InterDVDVobMux::SECTOR_SIZE);
	sec.fill(0);
	sec.write[0] = 0x00;
	sec.write[1] = 0x00;
	sec.write[2] = 0x01;
	sec.write[3] = 0xBA;
	sec.write[38] = 0x00;
	sec.write[39] = 0x00;
	sec.write[40] = 0x01;
	sec.write[41] = 0xE0;
	f->store_buffer(sec.ptr(), sec.size());
	f.unref();

	CHECK(InterDVDVobMux::finalize_title_vob(path) == OK);
	Ref<FileAccess> in = FileAccess::open(path, FileAccess::READ);
	REQUIRE(in.is_valid());
	CHECK(in->get_length() == InterDVDVobMux::SECTOR_SIZE * 2);
	const Vector<uint8_t> nav = in->get_buffer(InterDVDVobMux::SECTOR_SIZE);
	REQUIRE(nav.size() >= 1043);
	CHECK(nav[41] == 0xBF);
	CHECK(nav[1039] == 0);
	CHECK(nav[1040] == 0);
	CHECK(nav[1041] == 0);
	CHECK(nav[1042] == 1);
}

TEST_CASE("[Modules][InterDVD] apply_menu_buttons writes PCI HLI") {
	const String path = TestUtils::get_temp_path("inter_dvd_menu_button.vob");
	CHECK(InterDVDVobMux::write_dummy_vob(path) == OK);

	Ref<InterDVDButton> play;
	play.instantiate();
	play->set_highlight(Rect2(80, 200, 240, 60));
	TypedArray<InterDVDButton> buttons;
	buttons.push_back(play);
	CHECK(InterDVDVobMux::apply_menu_buttons(path, buttons) == OK);

	Ref<FileAccess> in = FileAccess::open(path, FileAccess::READ);
	REQUIRE(in.is_valid());
	const Vector<uint8_t> nav = in->get_buffer(InterDVDVobMux::SECTOR_SIZE);
	REQUIRE(nav.size() >= 210);
	CHECK(nav[142] == 1);
	CHECK(nav[155] == 0x10);
	CHECK(nav[156] == 0x00);
	CHECK(nav[157] == 1);
	CHECK(nav[158] == 1);
	CHECK(nav[159] == 0);
	CHECK(nav[160] == 0);
	CHECK(nav[147] == 0);
	CHECK(nav[148] == 1);
	CHECK(nav[149] == 0x5F);
	CHECK(nav[150] == 0x90);
	CHECK(nav[197] == 0x30);
	CHECK(nav[198] == 0x02);
	CHECK(nav[202] == 1);
}

TEST_CASE("[Modules][InterDVD] apply_menu_buttons can hold HLI past cell end for infinite still") {
	const String path = TestUtils::get_temp_path("inter_dvd_menu_hli_hold.vob");
	CHECK(InterDVDVobMux::write_dummy_vob(path) == OK);
	Ref<InterDVDButton> play;
	play.instantiate();
	play->set_highlight(Rect2(80, 200, 240, 60));
	TypedArray<InterDVDButton> buttons;
	buttons.push_back(play);
	CHECK(InterDVDVobMux::apply_menu_buttons(path, buttons, false, 0, 0, 0x1000, 0x3FFFFFFF) == OK);
	Ref<FileAccess> in = FileAccess::open(path, FileAccess::READ);
	REQUIRE(in.is_valid());
	const Vector<uint8_t> nav = in->get_buffer(InterDVDVobMux::SECTOR_SIZE);
	REQUIRE(nav.size() >= 151);
	CHECK(nav[147] == 0x3F);
	CHECK(nav[148] == 0xFF);
	CHECK(nav[149] == 0xFF);
	CHECK(nav[150] == 0xFF);
}

TEST_CASE("[Modules][InterDVD] apply_menu_buttons marks later NAV as same HLI") {
	const String path = TestUtils::get_temp_path("inter_dvd_hli_same.vob");
	CHECK(InterDVDVobMux::write_dummy_vob(path) == OK);
	Ref<FileAccess> first = FileAccess::open(path, FileAccess::READ);
	REQUIRE(first.is_valid());
	const Vector<uint8_t> nav0 = first->get_buffer(InterDVDVobMux::SECTOR_SIZE);
	first.unref();
	Ref<FileAccess> two = FileAccess::open(path, FileAccess::WRITE);
	REQUIRE(two.is_valid());
	two->store_buffer(nav0.ptr(), nav0.size());
	two->store_buffer(nav0.ptr(), nav0.size());
	two.unref();

	Ref<InterDVDButton> play;
	play.instantiate();
	play->set_highlight(Rect2(80, 200, 240, 60));
	TypedArray<InterDVDButton> buttons;
	buttons.push_back(play);
	CHECK(InterDVDVobMux::apply_menu_buttons(path, buttons) == OK);

	Ref<FileAccess> in = FileAccess::open(path, FileAccess::READ);
	REQUIRE(in.is_valid());
	const Vector<uint8_t> a = in->get_buffer(InterDVDVobMux::SECTOR_SIZE);
	const Vector<uint8_t> b = in->get_buffer(InterDVDVobMux::SECTOR_SIZE);
	REQUIRE(a.size() >= 143);
	REQUIRE(b.size() >= 143);
	CHECK(a[142] == 1);
	CHECK(b[142] == 2);
	CHECK(b[157] == 1);
}

TEST_CASE("[Modules][InterDVD] HLI auto-action and adjacent overrides") {
	const String path = TestUtils::get_temp_path("inter_dvd_hli_chrome.vob");
	CHECK(InterDVDVobMux::write_dummy_vob(path) == OK);

	Ref<InterDVDButton> play;
	play.instantiate();
	play->set_highlight(Rect2(80, 200, 240, 60));
	play->set_auto_action(true);
	play->set_adjacent_up(3);
	play->set_adjacent_down(4);
	Ref<InterDVDButton> other;
	other.instantiate();
	other->set_highlight(Rect2(80, 300, 240, 60));
	TypedArray<InterDVDButton> buttons;
	buttons.push_back(play);
	buttons.push_back(other);
	CHECK(InterDVDVobMux::apply_menu_buttons(path, buttons) == OK);

	Ref<FileAccess> in = FileAccess::open(path, FileAccess::READ);
	REQUIRE(in.is_valid());
	const Vector<uint8_t> nav = in->get_buffer(InterDVDVobMux::SECTOR_SIZE);
	REQUIRE(nav.size() >= 200);
	CHECK((nav[190] & 0x40) != 0);
	CHECK((nav[193] & 0x3F) == 3);
}

TEST_CASE("[Modules][InterDVD] HLI packs per-button color groups") {
	const String path = TestUtils::get_temp_path("inter_dvd_hli_color_group.vob");
	CHECK(InterDVDVobMux::write_dummy_vob(path) == OK);
	Ref<InterDVDButton> one;
	one.instantiate();
	one->set_highlight(Rect2(80, 200, 240, 60));
	one->set_color_group(1);
	one->set_select_color(Color(1, 0.92, 0.2));
	Ref<InterDVDButton> two;
	two.instantiate();
	two->set_highlight(Rect2(80, 300, 240, 60));
	two->set_color_group(2);
	two->set_select_color(Color(1, 1, 1));
	TypedArray<InterDVDButton> buttons;
	buttons.push_back(one);
	buttons.push_back(two);
	CHECK(InterDVDVobMux::apply_menu_buttons(path, buttons) == OK);
	Ref<FileAccess> in = FileAccess::open(path, FileAccess::READ);
	REQUIRE(in.is_valid());
	const Vector<uint8_t> nav = in->get_buffer(InterDVDVobMux::SECTOR_SIZE);
	REQUIRE(nav.size() >= 220);
	CHECK(nav[155] == 0x10);
	CHECK((nav[187] >> 6) == 1);
	CHECK((nav[205] >> 6) == 2);
}

TEST_CASE("[Modules][InterDVD] mux_cell rejects empty dummy when dummy disallowed") {
	const String dummy = TestUtils::get_temp_path("inter_dvd_dummy_src.vob");
	CHECK(InterDVDVobMux::write_dummy_vob(dummy) == OK);
	CHECK_FALSE(InterDVDVobMux::contains_mpeg_video(dummy));
	Ref<InterDVDCell> cell;
	cell.instantiate();
	cell->set_source_path(dummy);
	const String out = TestUtils::get_temp_path("inter_dvd_reject_empty.vob");
	String err;
	CHECK(InterDVDVobMux::mux_cell(cell, out, String(), false, &err, false) != OK);
	CHECK_FALSE(err.is_empty());
}
