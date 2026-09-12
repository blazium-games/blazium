/**************************************************************************/
/*  test_inter_dvd_ifo_writer.h                                           */
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

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "modules/inter_dvd/author/inter_dvd_ifo_writer.h"
#include "modules/inter_dvd/author/inter_dvd_project.h"
#include "modules/inter_dvd/author/inter_dvd_vob_mux.h"
#include "modules/inter_dvd/machine/inter_dvd_instruction.h"
#include "scene/resources/packed_scene.h"
#include "tests/test_macros.h"
#include "tests/test_utils.h"

TEST_CASE("[Modules][InterDVD] IFO writer smoke test") {
	const String root = TestUtils::get_temp_path("inter_dvd_video_ts");
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	da->make_dir_recursive(root);

	Ref<InterDVDProject> project;
	project.instantiate();
	Ref<InterDVDPGC> title;
	title.instantiate();
	TypedArray<InterDVDPGC> titles;
	titles.push_back(title);
	project->set_titles(titles);

	String err;
	CHECK(InterDVDIfoWriter::write_video_ts(root, project, true, "", &err) == OK);

	const String video = root.path_join("VIDEO_TS");
	CHECK(FileAccess::exists(video.path_join("VIDEO_TS.IFO")));
	CHECK(FileAccess::exists(video.path_join("VIDEO_TS.BUP")));
	CHECK(FileAccess::exists(video.path_join("VTS_01_0.IFO")));
	CHECK(FileAccess::exists(video.path_join("VTS_01_0.BUP")));
	CHECK(FileAccess::exists(video.path_join("VTS_01_1.VOB")));
	CHECK(DirAccess::exists(root.path_join("AUDIO_TS")));

	Ref<FileAccess> ifo = FileAccess::open(video.path_join("VIDEO_TS.IFO"), FileAccess::READ);
	REQUIRE(ifo.is_valid());
	const Vector<uint8_t> vmg = ifo->get_buffer(int(ifo->get_length()));
	REQUIRE(vmg.size() >= 0x500);
	CHECK(String::utf8((const char *)vmg.ptr(), 12) == "DVDVIDEO-VMG");
	const uint32_t vmgi_last = (uint32_t(vmg[0x80]) << 24) | (uint32_t(vmg[0x81]) << 16) | (uint32_t(vmg[0x82]) << 8) | uint32_t(vmg[0x83]);
	const uint32_t first_play = (uint32_t(vmg[0x84]) << 24) | (uint32_t(vmg[0x85]) << 16) | (uint32_t(vmg[0x86]) << 8) | uint32_t(vmg[0x87]);
	CHECK(first_play < vmgi_last);
	CHECK(vmg[int(first_play) + 0xEC] == 0);
	CHECK(vmg[int(first_play) + 0xED] == 1);
	CHECK(vmg[int(first_play) + 0xF2] == 0);
	CHECK(vmg[int(first_play) + 0xF3] == 15);
	CHECK(vmg[int(first_play) + 0xF4] == 0x30);
	CHECK(vmg[int(first_play) + 0xF5] == 0x02);
	CHECK(vmg[int(first_play) + 0xF9] == 1);

	Ref<FileAccess> vts = FileAccess::open(video.path_join("VTS_01_0.IFO"), FileAccess::READ);
	REQUIRE(vts.is_valid());
	const Vector<uint8_t> vtsi = vts->get_buffer(int(vts->get_length()));
	REQUIRE(vtsi.size() >= 2048 + 16);
	CHECK(String::utf8((const char *)vtsi.ptr(), 12) == "DVDVIDEO-VTS");
	CHECK(vtsi[0x100] == 0x40);
	CHECK(vtsi[0x101] == 0x00);
	const uint32_t ptt_last = (uint32_t(vtsi[2048 + 4]) << 24) | (uint32_t(vtsi[2048 + 5]) << 16) | (uint32_t(vtsi[2048 + 6]) << 8) | uint32_t(vtsi[2048 + 7]);
	CHECK(ptt_last == 15);
	CHECK(((ptt_last + 1 - 12) % 4) == 0);
}

TEST_CASE("[Modules][InterDVD] IFO writer authors a PCI highlight button") {
	const String root = TestUtils::get_temp_path("inter_dvd_button_menu");
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	da->make_dir_recursive(root);

	Ref<InterDVDButton> play;
	play.instantiate();
	play->set_highlight(Rect2(80, 200, 240, 60));
	TypedArray<InterDVDButton> buttons;
	buttons.push_back(play);

	Ref<InterDVDMenu> menu;
	menu.instantiate();
	menu->set_buttons(buttons);

	Ref<InterDVDPGC> title;
	title.instantiate();
	TypedArray<InterDVDPGC> titles;
	titles.push_back(title);
	TypedArray<InterDVDMenu> menus;
	menus.push_back(menu);

	Ref<InterDVDProject> project;
	project.instantiate();
	project->set_titles(titles);
	project->set_menus(menus);

	String err;
	CHECK(InterDVDIfoWriter::write_video_ts(root, project, true, "", &err) == OK);

	const String video = root.path_join("VIDEO_TS");
	Ref<FileAccess> ifo = FileAccess::open(video.path_join("VIDEO_TS.IFO"), FileAccess::READ);
	REQUIRE(ifo.is_valid());
	const Vector<uint8_t> vmg = ifo->get_buffer(int(ifo->get_length()));
	const uint32_t first_play = (uint32_t(vmg[0x84]) << 24) | (uint32_t(vmg[0x85]) << 16) | (uint32_t(vmg[0x86]) << 8) | uint32_t(vmg[0x87]);
	CHECK(vmg[int(first_play) + 0xF4] == 0x30);
	CHECK(vmg[int(first_play) + 0xF5] == 0x06);
	CHECK(vmg[int(first_play) + 0xF9] == 0x42);

	Ref<FileAccess> vob = FileAccess::open(video.path_join("VIDEO_TS.VOB"), FileAccess::READ);
	REQUIRE(vob.is_valid());
	const Vector<uint8_t> nav = vob->get_buffer(InterDVDIfoWriter::SECTOR_SIZE);
	REQUIRE(nav.size() >= 200);
	CHECK(nav[141] == 0);
	CHECK(nav[142] == 1);
	CHECK(nav[157] == 1);
	CHECK(nav[158] == 1);
	CHECK(nav[159] == 0);
	CHECK(nav[160] == 0);
	CHECK(nav[197] == 0x30);
	CHECK(nav[198] == 0x02);
	CHECK(nav[202] == 1);
	const int menu_pgc = 3 * 2048 + 32;
	REQUIRE(vmg.size() > menu_pgc + 0xEA);
	const uint16_t menu_cell_pb = (uint16_t(vmg[menu_pgc + 0xE8]) << 8) | uint16_t(vmg[menu_pgc + 0xE9]);
	CHECK(vmg[menu_pgc + 0x9D] == 1);
	CHECK(vmg[menu_pgc + menu_cell_pb + 2] == 0);
}

TEST_CASE("[Modules][InterDVD] IFO writer authors a second title and title-domain button") {
	const String root = TestUtils::get_temp_path("inter_dvd_two_scenes");
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	da->make_dir_recursive(root);

	Ref<InterDVDButton> next;
	next.instantiate();
	next->set_highlight(Rect2(400, 390, 280, 60));
	next->set_action(InterDVDButton::ACTION_JUMP_TITLE);
	next->set_target(2);
	TypedArray<InterDVDButton> title_buttons;
	title_buttons.push_back(next);

	Ref<InterDVDPGC> scene1;
	scene1.instantiate();
	scene1->set_buttons(title_buttons);
	Ref<InterDVDButton> back;
	back.instantiate();
	back->set_highlight(Rect2(80, 390, 200, 60));
	back->set_action(InterDVDButton::ACTION_JUMP_TITLE);
	back->set_target(1);
	Ref<InterDVDButton> replay;
	replay.instantiate();
	replay->set_highlight(Rect2(400, 390, 240, 60));
	replay->set_action(InterDVDButton::ACTION_JUMP_TITLE);
	replay->set_target(1);
	TypedArray<InterDVDButton> scene2_buttons;
	scene2_buttons.push_back(back);
	scene2_buttons.push_back(replay);
	Ref<InterDVDPGC> scene2;
	scene2.instantiate();
	scene2->set_buttons(scene2_buttons);
	TypedArray<InterDVDPGC> titles;
	titles.push_back(scene1);
	titles.push_back(scene2);

	Ref<InterDVDButton> play;
	play.instantiate();
	play->set_highlight(Rect2(80, 200, 240, 60));
	TypedArray<InterDVDButton> menu_buttons;
	menu_buttons.push_back(play);
	Ref<InterDVDMenu> menu;
	menu.instantiate();
	menu->set_buttons(menu_buttons);
	TypedArray<InterDVDMenu> menus;
	menus.push_back(menu);

	Ref<InterDVDProject> project;
	project.instantiate();
	project->set_titles(titles);
	project->set_menus(menus);

	String err;
	CHECK(InterDVDIfoWriter::write_video_ts(root, project, true, "", &err) == OK);

	const String video = root.path_join("VIDEO_TS");
	Ref<FileAccess> ifo = FileAccess::open(video.path_join("VIDEO_TS.IFO"), FileAccess::READ);
	REQUIRE(ifo.is_valid());
	const Vector<uint8_t> vmg = ifo->get_buffer(int(ifo->get_length()));
	REQUIRE(vmg.size() >= 2048 + 4);
	CHECK(vmg[2048] == 0);
	CHECK(vmg[2049] == 2);

	Ref<FileAccess> title = FileAccess::open(video.path_join("VTS_01_1.VOB"), FileAccess::READ);
	REQUIRE(title.is_valid());
	const Vector<uint8_t> nav = title->get_buffer(InterDVDIfoWriter::SECTOR_SIZE);
	REQUIRE(nav.size() >= 210);
	CHECK(nav[142] == 1);
	CHECK(nav[157] == 1);
	CHECK(nav[158] == 1);
	CHECK(nav[159] == 0);
	CHECK(nav[160] == 0);
	CHECK(nav[197] == 0x30);
	CHECK(nav[198] == 0x03);
	CHECK(nav[202] == 2);

	title->seek(InterDVDIfoWriter::SECTOR_SIZE);
	const Vector<uint8_t> nav2 = title->get_buffer(InterDVDIfoWriter::SECTOR_SIZE);
	REQUIRE(nav2.size() >= 210);
	CHECK(nav2[157] == 2);
	CHECK(nav2[158] == 2);
	CHECK(nav2[159] == 0);
	CHECK(nav2[160] == 0);
	CHECK(nav2[197] == 0x30);
	CHECK(nav2[198] == 0x03);
	CHECK(nav2[202] == 1);
	CHECK(nav2[215] == 0x30);
	CHECK(nav2[216] == 0x03);
	CHECK(nav2[220] == 1);

	Ref<FileAccess> vtsi = FileAccess::open(video.path_join("VTS_01_0.IFO"), FileAccess::READ);
	REQUIRE(vtsi.is_valid());
	const Vector<uint8_t> vts = vtsi->get_buffer(int(vtsi->get_length()));
	REQUIRE(vts.size() >= 4096 + 32);
	CHECK(vts[0x254] == 0);
	CHECK(vts[0x255] == 0);
	const int pgcit = 4096;
	const uint32_t pgc_rel = (uint32_t(vts[pgcit + 12]) << 24) | (uint32_t(vts[pgcit + 13]) << 16) | (uint32_t(vts[pgcit + 14]) << 8) | uint32_t(vts[pgcit + 15]);
	const int pgc_off = pgcit + int(pgc_rel);
	REQUIRE(pgc_off + 0xEC + 16 < vts.size());
	const uint16_t cmd_tbl = (uint16_t(vts[pgc_off + 0xE4]) << 8) | uint16_t(vts[pgc_off + 0xE5]);
	const uint16_t cell_pb = (uint16_t(vts[pgc_off + 0xE8]) << 8) | uint16_t(vts[pgc_off + 0xE9]);
	CHECK(vts[pgc_off + 0x1C] == 0);
	CHECK(vts[pgc_off + 0x9C] == 0);
	CHECK(vts[pgc_off + 0x9D] == 1);
	CHECK(vts[pgc_off + cmd_tbl + 3] == 0);
	CHECK(vts[pgc_off + cell_pb + 2] == 0);
}

TEST_CASE("[Modules][InterDVD] button action target authors JumpTT without raw command") {
	Ref<InterDVDButton> btn;
	btn.instantiate();
	btn->set_action(InterDVDButton::ACTION_JUMP_TITLE);
	btn->set_target(2);
	const PackedByteArray menu_cmd = btn->resolve_command(InterDVDButton::DOMAIN_VMGM);
	REQUIRE(menu_cmd.size() == 8);
	CHECK(menu_cmd[0] == 0x30);
	CHECK(menu_cmd[1] == 0x02);
	CHECK(menu_cmd[5] == 2);
	const PackedByteArray title_cmd = btn->resolve_command(InterDVDButton::DOMAIN_VTST);
	REQUIRE(title_cmd.size() == 8);
	CHECK(title_cmd[0] == 0x30);
	CHECK(title_cmd[1] == 0x03);
	CHECK(title_cmd[5] == 2);
	Ref<InterDVDButton> menu;
	menu.instantiate();
	menu->set_action(InterDVDButton::ACTION_JUMP_MENU);
	const PackedByteArray call = menu->resolve_command(InterDVDButton::DOMAIN_VTST);
	CHECK(call[1] == 0x08);
	CHECK(call[4] == 1);
	CHECK(call[5] == 0x42);
	Ref<InterDVDButton> pgc;
	pgc.instantiate();
	pgc->set_action(InterDVDButton::ACTION_JUMP_PGC);
	pgc->set_target(2);
	const PackedByteArray link = pgc->resolve_command(InterDVDButton::DOMAIN_VTST);
	CHECK(link[0] == 0x20);
	CHECK(link[1] == 0x04);
	CHECK(link[7] == 2);
	Ref<InterDVDButton> chapter;
	chapter.instantiate();
	chapter->set_action(InterDVDButton::ACTION_JUMP_CHAPTER);
	chapter->set_target(2);
	const PackedByteArray ptt = chapter->resolve_command(InterDVDButton::DOMAIN_VTST);
	CHECK(ptt[0] == 0x30);
	CHECK(ptt[1] == 0x05);
	CHECK(ptt[5] == 1);
	CHECK(ptt[7] == 2);
	const PackedByteArray vts_ptt = chapter->resolve_command(InterDVDButton::DOMAIN_VMGM);
	CHECK(vts_ptt[0] == 0x30);
	CHECK(vts_ptt[1] == 0x05);
	CHECK(vts_ptt[7] == 2);
	Ref<InterDVDButton> audio;
	audio.instantiate();
	audio->set_action(InterDVDButton::ACTION_JUMP_MENU);
	audio->set_target(5);
	const PackedByteArray audio_menu = audio->resolve_command(InterDVDButton::DOMAIN_VMGM);
	CHECK(audio_menu[0] == 0x30);
	CHECK(audio_menu[1] == 0x06);
	CHECK(audio_menu[5] == 0x45);
	Ref<InterDVDButton> exit_btn;
	exit_btn.instantiate();
	exit_btn->set_action(InterDVDButton::ACTION_EXIT);
	CHECK(exit_btn->resolve_command()[1] == 0x01);
}

TEST_CASE("[Modules][InterDVD] IFO writer uses cell duration as PGC playback time") {
	const String root = TestUtils::get_temp_path("inter_dvd_pgc_duration");
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	da->make_dir_recursive(root);

	Ref<InterDVDCell> cell;
	cell.instantiate();
	cell->set_duration_sec(8.0);
	TypedArray<InterDVDCell> cells;
	cells.push_back(cell);
	Ref<InterDVDPGC> title;
	title.instantiate();
	title->set_cells(cells);
	TypedArray<InterDVDPGC> titles;
	titles.push_back(title);
	Ref<InterDVDProject> project;
	project.instantiate();
	project->set_titles(titles);

	String err;
	CHECK(InterDVDIfoWriter::write_video_ts(root, project, true, "", &err) == OK);

	Ref<FileAccess> vts = FileAccess::open(root.path_join("VIDEO_TS").path_join("VTS_01_0.IFO"), FileAccess::READ);
	REQUIRE(vts.is_valid());
	const Vector<uint8_t> vtsi = vts->get_buffer(int(vts->get_length()));
	const int pgcit = 4096;
	REQUIRE(vtsi.size() > pgcit + 16);
	const uint32_t pgc_rel = (uint32_t(vtsi[pgcit + 12]) << 24) | (uint32_t(vtsi[pgcit + 13]) << 16) | (uint32_t(vtsi[pgcit + 14]) << 8) | uint32_t(vtsi[pgcit + 15]);
	const int pgc_off = pgcit + int(pgc_rel);
	REQUIRE(vtsi.size() > pgc_off + 8);
	CHECK(vtsi[pgc_off + 4] == 0);
	CHECK(vtsi[pgc_off + 5] == 0);
	CHECK(vtsi[pgc_off + 6] != 2);
	CHECK(vtsi[pgc_off + 6] == 0x08);
	CHECK((vtsi[pgc_off + 7] & 0xC0) == 0xC0);
	const int cell_pb = ((int(vtsi[pgc_off + 0xE8]) << 8) | int(vtsi[pgc_off + 0xE9]));
	REQUIRE(vtsi.size() > pgc_off + cell_pb);
	CHECK(vtsi[pgc_off + cell_pb] == 0x02);
}

TEST_CASE("[Modules][InterDVD] IFO writer muxes every cell as a chapter") {
	const String root = TestUtils::get_temp_path("inter_dvd_two_cells");
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	da->make_dir_recursive(root);

	Ref<InterDVDProject> project;
	project.instantiate();
	Ref<InterDVDCell> cell_a;
	cell_a.instantiate();
	cell_a->set_duration_sec(4.0);
	Ref<InterDVDCell> cell_b;
	cell_b.instantiate();
	cell_b->set_duration_sec(6.0);
	TypedArray<InterDVDCell> cells;
	cells.push_back(cell_a);
	cells.push_back(cell_b);
	Ref<InterDVDPGC> title;
	title.instantiate();
	title->set_cells(cells);
	TypedArray<InterDVDPGC> titles;
	titles.push_back(title);
	project->set_titles(titles);

	Ref<InterDVDButton> play_ch2;
	play_ch2.instantiate();
	play_ch2->set_action(InterDVDButton::ACTION_JUMP_CHAPTER);
	play_ch2->set_target(2);
	CHECK(play_ch2->resolve_command(InterDVDButton::DOMAIN_VTST)[1] == 0x05);
	CHECK(play_ch2->resolve_command(InterDVDButton::DOMAIN_VTST)[7] == 2);

	String err;
	CHECK(InterDVDIfoWriter::write_video_ts(root, project, true, "", &err) == OK);

	Ref<FileAccess> vts = FileAccess::open(root.path_join("VIDEO_TS").path_join("VTS_01_0.IFO"), FileAccess::READ);
	REQUIRE(vts.is_valid());
	const Vector<uint8_t> vtsi = vts->get_buffer(int(vts->get_length()));
	REQUIRE(vtsi.size() >= 2048 + 24);
	CHECK(vtsi[2048] == 0);
	CHECK(vtsi[2049] == 1);
	const uint32_t ptt_last = (uint32_t(vtsi[2048 + 4]) << 24) | (uint32_t(vtsi[2048 + 5]) << 16) | (uint32_t(vtsi[2048 + 6]) << 8) | uint32_t(vtsi[2048 + 7]);
	CHECK(ptt_last == 19);
	const uint32_t ptt0 = (uint32_t(vtsi[2048 + 8]) << 24) | (uint32_t(vtsi[2048 + 9]) << 16) | (uint32_t(vtsi[2048 + 10]) << 8) | uint32_t(vtsi[2048 + 11]);
	CHECK(vtsi[2048 + int(ptt0) + 3] == 1);
	CHECK(vtsi[2048 + int(ptt0) + 7] == 2);
	const int pgcit = 4096;
	const uint32_t pgc_rel = (uint32_t(vtsi[pgcit + 12]) << 24) | (uint32_t(vtsi[pgcit + 13]) << 16) | (uint32_t(vtsi[pgcit + 14]) << 8) | uint32_t(vtsi[pgcit + 15]);
	const int pgc_off = pgcit + int(pgc_rel);
	CHECK(vtsi[pgc_off + 2] == 2);
	CHECK(vtsi[pgc_off + 3] == 2);
}

TEST_CASE("[Modules][InterDVD] IFO writer grows VTS tables so eight titles do not overlap") {
	const String root = TestUtils::get_temp_path("inter_dvd_eight_titles");
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	da->make_dir_recursive(root);

	TypedArray<InterDVDPGC> titles;
	for (int t = 0; t < 8; t++) {
		TypedArray<InterDVDCell> cells;
		const int n = (t == 0) ? 4 : ((t == 1) ? 6 : 1);
		for (int c = 0; c < n; c++) {
			Ref<InterDVDCell> cell;
			cell.instantiate();
			cell->set_duration_sec(2.0);
			cells.push_back(cell);
		}
		Ref<InterDVDPGC> pgc;
		pgc.instantiate();
		pgc->set_cells(cells);
		titles.push_back(pgc);
	}
	Ref<InterDVDProject> project;
	project.instantiate();
	project->set_titles(titles);

	String err;
	CHECK(InterDVDIfoWriter::write_video_ts(root, project, true, "", &err) == OK);

	Ref<FileAccess> vts = FileAccess::open(root.path_join("VIDEO_TS").path_join("VTS_01_0.IFO"), FileAccess::READ);
	REQUIRE(vts.is_valid());
	const Vector<uint8_t> vtsi = vts->get_buffer(int(vts->get_length()));
	const uint32_t ptt_sec = (uint32_t(vtsi[0xC8]) << 24) | (uint32_t(vtsi[0xC9]) << 16) | (uint32_t(vtsi[0xCA]) << 8) | uint32_t(vtsi[0xCB]);
	const uint32_t pgcit_sec = (uint32_t(vtsi[0xCC]) << 24) | (uint32_t(vtsi[0xCD]) << 16) | (uint32_t(vtsi[0xCE]) << 8) | uint32_t(vtsi[0xCF]);
	const uint32_t cadt_sec = (uint32_t(vtsi[0xE0]) << 24) | (uint32_t(vtsi[0xE1]) << 16) | (uint32_t(vtsi[0xE2]) << 8) | uint32_t(vtsi[0xE3]);
	const uint32_t admap_sec = (uint32_t(vtsi[0xE4]) << 24) | (uint32_t(vtsi[0xE5]) << 16) | (uint32_t(vtsi[0xE6]) << 8) | uint32_t(vtsi[0xE7]);
	CHECK(ptt_sec == 1);
	CHECK(pgcit_sec == 2);
	CHECK(cadt_sec > pgcit_sec);
	CHECK(admap_sec > cadt_sec);
	const int pgcit = int(pgcit_sec) * 2048;
	const uint32_t pgc_last = (uint32_t(vtsi[pgcit + 4]) << 24) | (uint32_t(vtsi[pgcit + 5]) << 16) | (uint32_t(vtsi[pgcit + 6]) << 8) | uint32_t(vtsi[pgcit + 7]);
	CHECK(int(pgc_last) + 1 > 2048);
	CHECK(pgcit + int(pgc_last) + 1 <= int(cadt_sec) * 2048);
	CHECK(vtsi[pgcit] == 0);
	CHECK(vtsi[pgcit + 1] == 8);
	for (int i = 0; i < 8; i++) {
		const uint32_t rel = (uint32_t(vtsi[pgcit + 12 + i * 8]) << 24) | (uint32_t(vtsi[pgcit + 13 + i * 8]) << 16) | (uint32_t(vtsi[pgcit + 14 + i * 8]) << 8) | uint32_t(vtsi[pgcit + 15 + i * 8]);
		const int pgc_off = pgcit + int(rel);
		REQUIRE(pgc_off + 4 < vtsi.size());
		CHECK(vtsi[pgc_off + 3] >= 1);
	}
}

TEST_CASE("[Modules][InterDVD] cell picture-in-picture and loop pad defaults") {
	Ref<InterDVDCell> cell;
	cell.instantiate();
	CHECK(cell->get_pip_source_path().is_empty());
	CHECK(cell->get_pip_slot_path().is_empty());
	CHECK(cell->get_loop_pad_sec() == 0.0);
	cell->set_pip_source_path("D:/clips/preview.mp4");
	cell->set_pip_slot_path(NodePath("VideoSlot"));
	cell->set_loop_pad_sec(5.0);
	cell->set_audio_path("D:/music/menu.wav");
	CHECK(cell->get_pip_source_path() == "D:/clips/preview.mp4");
	CHECK(cell->get_audio_path() == "D:/music/menu.wav");
	CHECK(String(cell->get_pip_slot_path()) == "VideoSlot");
	CHECK(cell->get_loop_pad_sec() == 5.0);
}

TEST_CASE("[Modules][InterDVD] add_title_from_video wires a cell source") {
	Ref<InterDVDProject> project;
	project.instantiate();
	const Ref<InterDVDPGC> title = project->add_title_from_video("D:/clips/intro.mkv");
	REQUIRE(title.is_valid());
	CHECK(project->get_titles().size() == 1);
	REQUIRE(title->get_cells().size() == 1);
	const Ref<InterDVDCell> cell = title->get_cells()[0];
	CHECK(cell->get_source_path() == "D:/clips/intro.mkv");
	CHECK(cell->get_display_name() == "intro");
}

TEST_CASE("[Modules][InterDVD] authoring helpers add titles chapters menus and first play") {
	Ref<InterDVDProject> project;
	project.instantiate();
	const TypedArray<InterDVDPGC> added = project->add_titles_from_videos(PackedStringArray{ "D:/clips/a.mp4", "", "D:/clips/b.mkv" });
	CHECK(added.size() == 2);
	CHECK(project->get_titles().size() == 2);

	const Ref<InterDVDCell> chapter = project->add_chapter_from_video(1, "D:/clips/a2.mp4");
	REQUIRE(chapter.is_valid());
	CHECK(chapter->get_source_path() == "D:/clips/a2.mp4");
	const Ref<InterDVDPGC> title1 = project->get_titles()[0];
	REQUIRE(title1.is_valid());
	CHECK(title1->get_cells().size() == 2);

	const Ref<InterDVDCell> last_chapter = project->add_chapter_from_video(0, "D:/clips/b2.mov");
	REQUIRE(last_chapter.is_valid());
	const Ref<InterDVDPGC> title2 = project->get_titles()[1];
	REQUIRE(title2.is_valid());
	CHECK(title2->get_cells().size() == 2);

	Ref<PackedScene> scene;
	scene.instantiate();
	const Ref<InterDVDPGC> from_scene = project->add_title_from_scene(scene, 6.0);
	REQUIRE(from_scene.is_valid());
	REQUIRE(from_scene->get_cells().size() == 1);
	const Ref<InterDVDCell> scene_cell = from_scene->get_cells()[0];
	REQUIRE(scene_cell.is_valid());
	CHECK(scene_cell->get_duration_sec() == 6.0);
	CHECK(scene_cell->get_packed_scene() == scene);

	const Ref<InterDVDMenu> menu = project->add_title_menu();
	REQUIRE(menu.is_valid());
	CHECK(menu->get_name() == "Title Menu");
	CHECK(menu->get_menu_type() == InterDVDMenu::MENU_TITLE);
	CHECK(menu->get_still_time() == 0);
	CHECK(menu->get_buttons().size() == 1);
	CHECK(project->get_menus().size() == 1);
	const Ref<InterDVDButton> play = menu->get_buttons()[0];
	REQUIRE(play.is_valid());
	CHECK(play->get_action() == InterDVDButton::ACTION_JUMP_TITLE);
	CHECK(play->get_title_n() == 1);

	const Ref<InterDVDPGC> menu_title = project->add_menu_title();
	REQUIRE(menu_title.is_valid());
	CHECK(menu_title->get_name() == "Menu");
	CHECK(menu_title->get_still_time() == 255);
	CHECK(menu_title->get_buttons().size() == 1);

	const Ref<InterDVDPGC> fpc = project->ensure_first_play(2);
	REQUIRE(fpc.is_valid());
	CHECK(project->get_first_play() == fpc);
	CHECK(fpc->get_name() == "First Play");
	REQUIRE(fpc->get_pre_commands().size() == 1);
	const PackedByteArray expected = InterDVDInstruction::encode_link(InterDVDInstruction::JUMP_TT, InterDVDInstruction::DOMAIN_FPC, InterDVDInstruction::DOMAIN_VTST, 2, nullptr);
	const PackedByteArray cmd = fpc->get_pre_commands()[0];
	CHECK(cmd == expected);

	Ref<InterDVDProject> empty;
	empty.instantiate();
	const Ref<InterDVDCell> first = empty->add_chapter_from_video(0, "D:/clips/only.mov");
	REQUIRE(first.is_valid());
	CHECK(empty->get_titles().size() == 1);
	CHECK(first->get_source_path() == "D:/clips/only.mov");

	Ref<InterDVDPGC> pgc;
	pgc.instantiate();
	const Ref<InterDVDCell> extra = pgc->add_cell_from_video("D:/clips/extra.mpg");
	REQUIRE(extra.is_valid());
	CHECK(extra->get_source_path() == "D:/clips/extra.mpg");
	const Ref<InterDVDButton> jump = pgc->add_jump_title_button(3);
	REQUIRE(jump.is_valid());
	CHECK(jump->get_action() == InterDVDButton::ACTION_JUMP_TITLE);
	CHECK(jump->get_title_n() == 3);
	CHECK(pgc->get_cells().size() == 1);
	CHECK(pgc->get_buttons().size() == 1);
}

TEST_CASE("[Modules][InterDVD] menu type 4-7 is written into VTSM PGCI_UT") {
	const String root = TestUtils::get_temp_path("inter_dvd_audio_menu");
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	da->make_dir_recursive(root);

	Ref<InterDVDButton> play;
	play.instantiate();
	play->set_highlight(Rect2(80, 200, 240, 60));
	TypedArray<InterDVDButton> buttons;
	buttons.push_back(play);
	Ref<InterDVDMenu> menu;
	menu.instantiate();
	menu->set_buttons(buttons);
	menu->set_menu_type(InterDVDMenu::MENU_AUDIO);
	menu->set_still_time(10);
	TypedArray<InterDVDMenu> menus;
	menus.push_back(menu);
	Ref<InterDVDProject> project;
	project.instantiate();
	project->set_menus(menus);

	String err;
	CHECK(InterDVDIfoWriter::write_video_ts(root, project, true, "", &err) == OK);
	Ref<FileAccess> vts = FileAccess::open(root.path_join("VIDEO_TS").path_join("VTS_01_0.IFO"), FileAccess::READ);
	REQUIRE(vts.is_valid());
	const Vector<uint8_t> vtsi = vts->get_buffer(int(vts->get_length()));
	const uint32_t vtsm_sec = (uint32_t(vtsi[0xD0]) << 24) | (uint32_t(vtsi[0xD1]) << 16) | (uint32_t(vtsi[0xD2]) << 8) | uint32_t(vtsi[0xD3]);
	const int ut = int(vtsm_sec) * 2048;
	REQUIRE(vtsi.size() > ut + 32);
	CHECK(vtsi[ut + 24] == 0x85);
	const int menu_pgc = ut + 32;
	const uint16_t menu_cell_pb = (uint16_t(vtsi[menu_pgc + 0xE8]) << 8) | uint16_t(vtsi[menu_pgc + 0xE9]);
	CHECK(vtsi[menu_pgc + menu_cell_pb + 2] == 10);
}

TEST_CASE("[Modules][InterDVD] menu still stays on the HLI cell when an exit cell follows") {
	const String root = TestUtils::get_temp_path("inter_dvd_exit_cell_still");
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	da->make_dir_recursive(root);

	Ref<InterDVDButton> back;
	back.instantiate();
	back->set_highlight(Rect2(40, 400, 180, 36));
	back->set_action(InterDVDButton::ACTION_JUMP_CELL);
	back->set_target(2);
	TypedArray<InterDVDButton> buttons;
	buttons.push_back(back);

	Ref<InterDVDCell> menu_cell;
	menu_cell.instantiate();
	menu_cell->set_duration_sec(4.0);
	Ref<InterDVDCell> exit_cell;
	exit_cell.instantiate();
	exit_cell->set_duration_sec(0.4);
	const PackedByteArray jump = InterDVDInstruction::encode_link(InterDVDInstruction::JUMP_VTS_TT, InterDVDInstruction::DOMAIN_VTST, InterDVDInstruction::DOMAIN_VTST, 1, nullptr);
	TypedArray<PackedByteArray> post;
	post.push_back(jump);
	exit_cell->set_post_commands(post);
	TypedArray<InterDVDCell> cells;
	cells.push_back(menu_cell);
	cells.push_back(exit_cell);

	Ref<InterDVDPGC> title;
	title.instantiate();
	title->set_cells(cells);
	title->set_buttons(buttons);
	title->set_still_time(255);
	title->set_next_pgc(0);
	TypedArray<InterDVDPGC> titles;
	titles.push_back(title);
	Ref<InterDVDProject> project;
	project.instantiate();
	project->set_titles(titles);

	String err;
	CHECK(InterDVDIfoWriter::write_video_ts(root, project, true, "", &err) == OK);

	Ref<FileAccess> vts = FileAccess::open(root.path_join("VIDEO_TS").path_join("VTS_01_0.IFO"), FileAccess::READ);
	REQUIRE(vts.is_valid());
	const Vector<uint8_t> buf = vts->get_buffer(int(vts->get_length()));
	const int pgcit = 4096;
	const uint32_t pgc_rel = (uint32_t(buf[pgcit + 12]) << 24) | (uint32_t(buf[pgcit + 13]) << 16) | (uint32_t(buf[pgcit + 14]) << 8) | uint32_t(buf[pgcit + 15]);
	const int pgc_off = pgcit + int(pgc_rel);
	const uint16_t cell_pb = (uint16_t(buf[pgc_off + 0xE8]) << 8) | uint16_t(buf[pgc_off + 0xE9]);
	CHECK(buf[pgc_off + 3] == 2);
	CHECK(buf[pgc_off + cell_pb + 2] == 255);
	CHECK(buf[pgc_off + cell_pb + 24 + 2] == 0);
	CHECK(back->resolve_command(InterDVDButton::DOMAIN_VTST)[1] == 0x07);
	CHECK(back->resolve_command(InterDVDButton::DOMAIN_VTST)[7] == 2);
	const uint32_t exit_first = (uint32_t(buf[pgc_off + cell_pb + 24 + 8]) << 24) | (uint32_t(buf[pgc_off + cell_pb + 24 + 9]) << 16) | (uint32_t(buf[pgc_off + cell_pb + 24 + 10]) << 8) | uint32_t(buf[pgc_off + cell_pb + 24 + 11]);
	Ref<FileAccess> title_vob = FileAccess::open(root.path_join("VIDEO_TS").path_join("VTS_01_1.VOB"), FileAccess::READ);
	REQUIRE(title_vob.is_valid());
	title_vob->seek(int64_t(exit_first) * InterDVDIfoWriter::SECTOR_SIZE);
	const Vector<uint8_t> exit_nav = title_vob->get_buffer(InterDVDIfoWriter::SECTOR_SIZE);
	REQUIRE(exit_nav.size() >= 158);
	CHECK(exit_nav[157] == 0);
}

TEST_CASE("[Modules][InterDVD] sanitize_volume_id and ISO args") {
	CHECK(InterDVDProject::sanitize_volume_id("My Disc!") == "MY_DISC");
	CHECK(InterDVDProject::sanitize_volume_id("") == "BLAZIUM_DVD");
	CHECK(InterDVDProject::language_be16("en") == 0x656E);
	CHECK(InterDVDProject::language_be16("fr") == 0x6672);
	const Vector<String> args = InterDVDIfoWriter::toolchain_iso_args("D:/work", "D:/out.iso", "D:/work/disc.interdvd.json");
	CHECK(args.has("interdvd"));
	CHECK(args.has("iso"));
	CHECK(args.has("--dir"));
	CHECK(args.has("D:/work"));
	CHECK(args.has("--out"));
	CHECK(args.has("D:/out.iso"));
	CHECK(args.has("--meta"));
	CHECK(args.has("D:/work/disc.interdvd.json"));
	CHECK(args.has("--write-meta"));
	CHECK(args.has("--json"));
	CHECK(!args.has("-dvd-video"));
	CHECK(!args.has("-u2"));
	CHECK(!args.has("mkisofs"));
}

TEST_CASE("[Modules][InterDVD] disc meta JSON and extras copy") {
	const String root = TestUtils::get_temp_path("inter_dvd_meta_extras");
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	da->make_dir_recursive(root);
	const String extra_host = root.path_join("note.txt");
	{
		Ref<FileAccess> f = FileAccess::open(extra_host, FileAccess::WRITE);
		REQUIRE(f.is_valid());
		f->store_string("hello extra");
	}
	Ref<InterDVDProject> project;
	project.instantiate();
	project->set_volume_id("My Disc!");
	project->set_disc_title("Hello Disc");
	project->set_publisher("Studio");
	project->set_license("MIT");
	PackedStringArray extras;
	extras.push_back(extra_host + ":NOTES.TXT");
	project->set_extras(extras);
	const String disc = root.path_join("disc");
	da->make_dir_recursive(disc);
	const String meta = disc.path_join("disc.interdvd.json");
	CHECK(InterDVDIfoWriter::write_disc_meta(meta, project) == OK);
	const String text = FileAccess::get_file_as_string(meta);
	CHECK(text.contains("blazium.interdvd.meta/v1"));
	CHECK(text.contains("MY_DISC"));
	CHECK(text.contains("Hello Disc"));
	CHECK(text.contains("NOTES.TXT"));
	CHECK(text.contains("MIT"));
	String err;
	CHECK(InterDVDIfoWriter::copy_extras(disc, project, &err) == OK);
	CHECK(FileAccess::exists(disc.path_join("NOTES.TXT")));
	CHECK(FileAccess::get_file_as_string(disc.path_join("NOTES.TXT")).contains("hello extra"));
}

TEST_CASE("[Modules][InterDVD] IFO writer stores provider language and TXTDT") {
	const String root = TestUtils::get_temp_path("inter_dvd_identity");
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	da->make_dir_recursive(root);
	Ref<InterDVDProject> project;
	project.instantiate();
	project->set_provider_id("TEST PROVIDER");
	project->set_disc_title("Hello Disc");
	project->set_menu_language("fr");
	project->set_audio_language("de");
	String err;
	CHECK(InterDVDIfoWriter::write_video_ts(root, project, true, "", &err) == OK);
	Ref<FileAccess> ifo = FileAccess::open(root.path_join("VIDEO_TS").path_join("VIDEO_TS.IFO"), FileAccess::READ);
	REQUIRE(ifo.is_valid());
	const Vector<uint8_t> vmg = ifo->get_buffer(int(ifo->get_length()));
	REQUIRE(vmg.size() > 0x50);
	CHECK(String::utf8((const char *)vmg.ptr() + 0x40, 13) == "TEST PROVIDER");
	const uint32_t txtdt = (uint32_t(vmg[0xD4]) << 24) | (uint32_t(vmg[0xD5]) << 16) | (uint32_t(vmg[0xD6]) << 8) | uint32_t(vmg[0xD7]);
	CHECK(txtdt != 0);
	const int ut = 3 * 2048;
	REQUIRE(vmg.size() > ut + 10);
	CHECK(vmg[ut + 8] == 0x66);
	CHECK(vmg[ut + 9] == 0x72);
}

TEST_CASE("[Modules][InterDVD] two muxed AC3 streams become two audio attrs") {
	const String root = TestUtils::get_temp_path("inter_dvd_two_ac3");
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	da->make_dir_recursive(root);
	const String encoded = TestUtils::get_temp_path("inter_dvd_two_ac3_src.vob");
	CHECK(InterDVDVobMux::write_dummy_vob(encoded) == OK);
	Ref<FileAccess> navf = FileAccess::open(encoded, FileAccess::READ);
	REQUIRE(navf.is_valid());
	const Vector<uint8_t> nav = navf->get_buffer(InterDVDVobMux::SECTOR_SIZE);
	navf.unref();
	auto private_pack = [](uint8_t p_id) {
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
		sec.write[41] = 0xBD;
		sec.write[46] = 0;
		sec.write[47] = p_id;
		return sec;
	};
	Ref<FileAccess> w = FileAccess::open(encoded, FileAccess::WRITE);
	REQUIRE(w.is_valid());
	w->store_buffer(nav.ptr(), nav.size());
	const Vector<uint8_t> a0 = private_pack(0x80);
	const Vector<uint8_t> a1 = private_pack(0x81);
	w->store_buffer(a0.ptr(), a0.size());
	w->store_buffer(a1.ptr(), a1.size());
	w.unref();
	CHECK(InterDVDVobMux::count_ac3_streams(encoded) == 2);
	CHECK(InterDVDVobMux::count_spu_streams(encoded) == 0);

	Ref<InterDVDCell> cell;
	cell.instantiate();
	cell->set_encoded_path(encoded);
	TypedArray<InterDVDCell> cells;
	cells.push_back(cell);
	Ref<InterDVDPGC> title;
	title.instantiate();
	title->set_cells(cells);
	TypedArray<InterDVDPGC> titles;
	titles.push_back(title);
	Ref<InterDVDProject> project;
	project.instantiate();
	project->set_titles(titles);
	String err;
	CHECK(InterDVDIfoWriter::write_video_ts(root, project, true, "", &err) == OK);
	Ref<FileAccess> vts = FileAccess::open(root.path_join("VIDEO_TS").path_join("VTS_01_0.IFO"), FileAccess::READ);
	REQUIRE(vts.is_valid());
	const Vector<uint8_t> vtsi = vts->get_buffer(int(vts->get_length()));
	CHECK(vtsi[0x102] == 0);
	CHECK(vtsi[0x103] == 2);
	CHECK(vtsi[0x254] == 0);
	CHECK(vtsi[0x255] == 0);
	const int pgcit = 4096;
	const uint32_t pgc_rel = (uint32_t(vtsi[pgcit + 12]) << 24) | (uint32_t(vtsi[pgcit + 13]) << 16) | (uint32_t(vtsi[pgcit + 14]) << 8) | uint32_t(vtsi[pgcit + 15]);
	const int pgc_off = pgcit + int(pgc_rel);
	CHECK(vtsi[pgc_off + 0x0C] == 0x80);
	CHECK(vtsi[pgc_off + 0x0D] == 0x80);
	CHECK(vtsi[pgc_off + 0x1C] == 0);
}

TEST_CASE("[Modules][InterDVD] SPU attr is written only when SPU PES exists") {
	const String encoded = TestUtils::get_temp_path("inter_dvd_spu_src.vob");
	CHECK(InterDVDVobMux::write_dummy_vob(encoded) == OK);
	Ref<FileAccess> navf = FileAccess::open(encoded, FileAccess::READ);
	const Vector<uint8_t> nav = navf->get_buffer(InterDVDVobMux::SECTOR_SIZE);
	navf.unref();
	Vector<uint8_t> spu;
	spu.resize(InterDVDVobMux::SECTOR_SIZE);
	spu.fill(0);
	spu.write[0] = 0x00;
	spu.write[1] = 0x00;
	spu.write[2] = 0x01;
	spu.write[3] = 0xBA;
	spu.write[38] = 0x00;
	spu.write[39] = 0x00;
	spu.write[40] = 0x01;
	spu.write[41] = 0xBD;
	spu.write[46] = 0;
	spu.write[47] = 0x20;
	Ref<FileAccess> w = FileAccess::open(encoded, FileAccess::WRITE);
	w->store_buffer(nav.ptr(), nav.size());
	w->store_buffer(spu.ptr(), spu.size());
	w.unref();
	CHECK(InterDVDVobMux::contains_spu(encoded));
	CHECK(InterDVDVobMux::count_spu_streams(encoded) == 1);

	const String root = TestUtils::get_temp_path("inter_dvd_spu_ifo");
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	da->make_dir_recursive(root);
	Ref<InterDVDCell> cell;
	cell.instantiate();
	cell->set_encoded_path(encoded);
	TypedArray<InterDVDCell> cells;
	cells.push_back(cell);
	Ref<InterDVDPGC> title;
	title.instantiate();
	title->set_cells(cells);
	TypedArray<InterDVDPGC> titles;
	titles.push_back(title);
	Ref<InterDVDProject> project;
	project.instantiate();
	project->set_titles(titles);
	String err;
	CHECK(InterDVDIfoWriter::write_video_ts(root, project, true, "", &err) == OK);
	Ref<FileAccess> vts = FileAccess::open(root.path_join("VIDEO_TS").path_join("VTS_01_0.IFO"), FileAccess::READ);
	const Vector<uint8_t> vtsi = vts->get_buffer(int(vts->get_length()));
	CHECK(vtsi[0x255] == 1);
	const int pgcit = 4096;
	const uint32_t pgc_rel = (uint32_t(vtsi[pgcit + 12]) << 24) | (uint32_t(vtsi[pgcit + 13]) << 16) | (uint32_t(vtsi[pgcit + 14]) << 8) | uint32_t(vtsi[pgcit + 15]);
	const int pgc_off = pgcit + int(pgc_rel);
	CHECK(vtsi[pgc_off + 0x1C] == 0x80);
}

TEST_CASE("[Modules][InterDVD] VTSM IFO has Root SRP when a Root menu is authored") {
	const String root = TestUtils::get_temp_path("inter_dvd_vtsm_root");
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	da->make_dir_recursive(root);
	Ref<InterDVDButton> play;
	play.instantiate();
	play->set_highlight(Rect2(80, 200, 240, 60));
	TypedArray<InterDVDButton> buttons;
	buttons.push_back(play);
	Ref<InterDVDMenu> menu;
	menu.instantiate();
	menu->set_menu_type(InterDVDMenu::MENU_ROOT);
	menu->set_buttons(buttons);
	TypedArray<InterDVDMenu> menus;
	menus.push_back(menu);
	Ref<InterDVDProject> project;
	project.instantiate();
	project->set_menus(menus);
	String err;
	CHECK(InterDVDIfoWriter::write_video_ts(root, project, true, "", &err) == OK);
	Ref<FileAccess> vts = FileAccess::open(root.path_join("VIDEO_TS").path_join("VTS_01_0.IFO"), FileAccess::READ);
	const Vector<uint8_t> vtsi = vts->get_buffer(int(vts->get_length()));
	const uint32_t vtsm_sec = (uint32_t(vtsi[0xD0]) << 24) | (uint32_t(vtsi[0xD1]) << 16) | (uint32_t(vtsi[0xD2]) << 8) | uint32_t(vtsi[0xD3]);
	const int ut = int(vtsm_sec) * 2048;
	REQUIRE(vtsi.size() > ut + 25);
	CHECK(vtsi[ut + 24] == 0x83);
}

TEST_CASE("[Modules][InterDVD] IFO writer writes PTL_MAIT GoUp UOP and second VTS") {
	const String root = TestUtils::get_temp_path("inter_dvd_ptl_vts2");
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	da->make_dir_recursive(root);

	Ref<InterDVDCell> cell_a;
	cell_a.instantiate();
	cell_a->set_duration_sec(2.0);
	cell_a->set_angle(1);
	Ref<InterDVDCell> cell_b;
	cell_b.instantiate();
	cell_b->set_duration_sec(2.0);
	cell_b->set_angle(2);
	TypedArray<InterDVDCell> cells_a;
	cells_a.push_back(cell_a);
	cells_a.push_back(cell_b);
	Ref<InterDVDPGC> t1;
	t1.instantiate();
	t1->set_cells(cells_a);
	t1->set_title_set_nr(1);
	t1->set_goup_pgc(1);
	t1->set_uops(0x00010000);
	t1->set_parental_id(1);

	Ref<InterDVDCell> cell_c;
	cell_c.instantiate();
	cell_c->set_duration_sec(2.0);
	TypedArray<InterDVDCell> cells_c;
	cells_c.push_back(cell_c);
	Ref<InterDVDPGC> t2;
	t2.instantiate();
	t2->set_cells(cells_c);
	t2->set_title_set_nr(2);

	TypedArray<InterDVDPGC> titles;
	titles.push_back(t1);
	titles.push_back(t2);
	Ref<InterDVDProject> project;
	project.instantiate();
	project->set_titles(titles);
	project->set_parental_level(3);

	String err;
	CHECK(InterDVDIfoWriter::write_video_ts(root, project, true, "", &err) == OK);
	const String video = root.path_join("VIDEO_TS");
	CHECK(FileAccess::exists(video.path_join("VTS_01_0.IFO")));
	CHECK(FileAccess::exists(video.path_join("VTS_02_0.IFO")));
	CHECK(FileAccess::exists(video.path_join("VTS_02_1.VOB")));

	Ref<FileAccess> vmgf = FileAccess::open(video.path_join("VIDEO_TS.IFO"), FileAccess::READ);
	REQUIRE(vmgf.is_valid());
	const Vector<uint8_t> vmg = vmgf->get_buffer(int(vmgf->get_length()));
	const uint32_t ptl = (uint32_t(vmg[0xCC]) << 24) | (uint32_t(vmg[0xCD]) << 16) | (uint32_t(vmg[0xCE]) << 8) | uint32_t(vmg[0xCF]);
	CHECK(ptl != 0);
	CHECK(vmg[0x3E] == 0);
	CHECK(vmg[0x3F] == 2);
	CHECK(vmg[2048 + 8 + 1] >= 2);

	Ref<FileAccess> vts = FileAccess::open(video.path_join("VTS_01_0.IFO"), FileAccess::READ);
	REQUIRE(vts.is_valid());
	const Vector<uint8_t> vtsi = vts->get_buffer(int(vts->get_length()));
	const int pgcit = 4096;
	const uint32_t pgc_rel = (uint32_t(vtsi[pgcit + 12]) << 24) | (uint32_t(vtsi[pgcit + 13]) << 16) | (uint32_t(vtsi[pgcit + 14]) << 8) | uint32_t(vtsi[pgcit + 15]);
	const int pgc_off = pgcit + int(pgc_rel);
	REQUIRE(vtsi.size() > pgc_off + 0xA2);
	CHECK(vtsi[pgc_off + 0xA1] == 1);
	const int cell_pb = ((int(vtsi[pgc_off + 0xE8]) << 8) | int(vtsi[pgc_off + 0xE9]));
	REQUIRE(vtsi.size() > pgc_off + cell_pb + 24);
	CHECK((vtsi[pgc_off + cell_pb] & 0xF0) != 0);
}
