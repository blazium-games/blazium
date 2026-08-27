/**************************************************************************/
/*  test_screensaver_cmdline.h                                            */
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

#include "core/config/project_settings.h"
#include "core/math/rect2i.h"
#include "core/os/os.h"
#include "modules/screensaver/screensaver.h"
#include "modules/screensaver/screensaver_cmdline.h"
#include "tests/test_macros.h"

static void _apply_screensaver(DisplayServer::WindowMode &r_window_mode, uint32_t &r_flags, int64_t &r_embed, Vector2i &r_pos, Size2i &r_size, int &r_screen, bool &r_use_pos) {
	ScreensaverCmdline::apply_recorded(r_window_mode, r_flags, r_embed, r_pos, r_size, r_screen, r_use_pos);
}

TEST_CASE("[Modules][Screensaver] consume /s") {
	ScreensaverCmdline::reset();
	bool consumed_next = false;
	CHECK(ScreensaverCmdline::try_consume("/s", "", consumed_next));
	CHECK_FALSE(consumed_next);
	CHECK(ScreensaverCmdline::get_mode() == ScreensaverCmdline::MODE_RUN);

	ScreensaverCmdline::reset();
	consumed_next = false;
	CHECK(ScreensaverCmdline::try_consume("/s", "2", consumed_next));
	CHECK(consumed_next);
	CHECK(ScreensaverCmdline::get_mode() == ScreensaverCmdline::MODE_RUN);
	CHECK(ScreensaverCmdline::resolved_target_screen() == 2);
}

TEST_CASE("[Modules][Screensaver] cover_os_rect fills the monitor") {
	ScreensaverCmdline::reset();
	ScreensaverCmdline::set_monitor_rect_for_tests(1, Rect2i(1920, 0, 1920, 1080));
	const Rect2i cover = ScreensaverCmdline::cover_os_rect(1);
	CHECK(cover.position == Vector2i(1920, 0));
	CHECK(cover.size == Size2i(1920, 1080));
	ScreensaverCmdline::clear_monitor_rect_for_tests();
}

TEST_CASE("[Modules][Screensaver] normalize_screen_index") {
	CHECK(ScreensaverCmdline::normalize_screen_index(0, 3) == 0);
	CHECK(ScreensaverCmdline::normalize_screen_index(1, 3) == 1);
	CHECK(ScreensaverCmdline::normalize_screen_index(2, 3) == 2);
	CHECK(ScreensaverCmdline::normalize_screen_index(3, 3) == 2);
	CHECK(ScreensaverCmdline::normalize_screen_index(4, 3) == -1);
	CHECK(ScreensaverCmdline::normalize_screen_index(-1, 3) == -1);
}

TEST_CASE("[Modules][Screensaver] order_monitor_rects primary first then left-to-right") {
	Vector<Rect2i> raw;
	raw.push_back(Rect2i(-1920, 0, 1920, 1080));
	raw.push_back(Rect2i(-3840, 0, 1920, 1080));
	raw.push_back(Rect2i(0, 0, 1920, 1080));
	const Vector<Rect2i> ordered = ScreensaverCmdline::order_monitor_rects(raw, 2);
	REQUIRE(ordered.size() == 3);
	CHECK(ordered[0] == Rect2i(0, 0, 1920, 1080));
	CHECK(ordered[1] == Rect2i(-3840, 0, 1920, 1080));
	CHECK(ordered[2] == Rect2i(-1920, 0, 1920, 1080));
}

TEST_CASE("[Modules][Screensaver] host /S launch spans all monitors") {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	REQUIRE(ps != nullptr);
	const String prev_cover_mode = ps->get_setting("blazium/screensaver/cover_mode", String());
	ps->set_setting("blazium/screensaver/cover_mode", "single");
	ScreensaverCmdline::reset();
	ScreensaverCmdline::set_host_screensaver_launch_for_tests(true);
	bool consumed_next = false;
	CHECK(ScreensaverCmdline::try_consume("/s", "", consumed_next));
	CHECK(ScreensaverCmdline::resolved_cover_mode() == "virtual");

	ScreensaverCmdline::set_target_screen(1);
	CHECK(ScreensaverCmdline::resolved_cover_mode() == "single");

	ScreensaverCmdline::reset();
	ScreensaverCmdline::clear_host_screensaver_launch_for_tests();
	ps->set_setting("blazium/screensaver/cover_mode", prev_cover_mode);
}

TEST_CASE("[Modules][Screensaver] ingest BLAZIUM_SCREENSAVER env when cmdline has no --screen") {
	REQUIRE(OS::get_singleton() != nullptr);
	const String prev_screen = OS::get_singleton()->get_environment("BLAZIUM_SCREENSAVER_SCREEN");
	const String prev_cover = OS::get_singleton()->get_environment("BLAZIUM_SCREENSAVER_COVER");

	ScreensaverCmdline::reset();
	OS::get_singleton()->set_environment("BLAZIUM_SCREENSAVER_SCREEN", "2");
	OS::get_singleton()->unset_environment("BLAZIUM_SCREENSAVER_COVER");
	ScreensaverCmdline::ingest_from_os_command_line();
	CHECK(ScreensaverCmdline::get_target_screen_override() == 2);

	ScreensaverCmdline::reset();
	OS::get_singleton()->unset_environment("BLAZIUM_SCREENSAVER_SCREEN");
	OS::get_singleton()->set_environment("BLAZIUM_SCREENSAVER_COVER", "virtual");
	ScreensaverCmdline::ingest_from_os_command_line();
	CHECK(ScreensaverCmdline::resolved_cover_mode() == "virtual");

	ScreensaverCmdline::reset();
	if (prev_screen.is_empty()) {
		OS::get_singleton()->unset_environment("BLAZIUM_SCREENSAVER_SCREEN");
	} else {
		OS::get_singleton()->set_environment("BLAZIUM_SCREENSAVER_SCREEN", prev_screen);
	}
	if (prev_cover.is_empty()) {
		OS::get_singleton()->unset_environment("BLAZIUM_SCREENSAVER_COVER");
	} else {
		OS::get_singleton()->set_environment("BLAZIUM_SCREENSAVER_COVER", prev_cover);
	}
}

TEST_CASE("[Modules][Screensaver] os_monitor_count override") {
	ScreensaverCmdline::reset();
	ScreensaverCmdline::set_os_monitor_count_for_tests(3);
	CHECK(ScreensaverCmdline::os_monitor_count() == 3);
	CHECK(ScreensaverCmdline::normalize_screen_index(0, ScreensaverCmdline::os_monitor_count()) == 0);
	CHECK(ScreensaverCmdline::normalize_screen_index(2, ScreensaverCmdline::os_monitor_count()) == 2);
	CHECK(ScreensaverCmdline::normalize_screen_index(3, ScreensaverCmdline::os_monitor_count()) == 2);
	ScreensaverCmdline::clear_os_monitor_count_for_tests();
	CHECK(ScreensaverCmdline::os_monitor_count() >= 1);
}

TEST_CASE("[Modules][Screensaver] consume /s:N") {
	ScreensaverCmdline::reset();
	bool consumed_next = false;
	CHECK(ScreensaverCmdline::try_consume("/s:2", "", consumed_next));
	CHECK_FALSE(consumed_next);
	CHECK(ScreensaverCmdline::get_mode() == ScreensaverCmdline::MODE_RUN);
	CHECK(ScreensaverCmdline::resolved_target_screen() == 2);

	ScreensaverCmdline::reset();
	CHECK(ScreensaverCmdline::try_consume("/s:0", "", consumed_next));
	CHECK(ScreensaverCmdline::get_mode() == ScreensaverCmdline::MODE_RUN);
	CHECK(ScreensaverCmdline::resolved_target_screen() == 0);
}

TEST_CASE("[Modules][Screensaver] consume --screensaver-cover") {
	ScreensaverCmdline::reset();
	bool consumed_next = false;
	CHECK(ScreensaverCmdline::try_consume("--screensaver-cover", "virtual", consumed_next));
	CHECK(consumed_next);
	CHECK(ScreensaverCmdline::resolved_cover_mode() == "virtual");

	ScreensaverCmdline::reset();
	consumed_next = false;
	CHECK(ScreensaverCmdline::try_consume("--screensaver-cover", "clone", consumed_next));
	CHECK(consumed_next);
	CHECK(ScreensaverCmdline::resolved_cover_mode() == "clone");

	ScreensaverCmdline::reset();
	consumed_next = false;
	CHECK(ScreensaverCmdline::try_consume("--screensaver-cover", "single", consumed_next));
	CHECK(consumed_next);
	CHECK(ScreensaverCmdline::resolved_cover_mode() == "single");
}

TEST_CASE("[Modules][Screensaver] consume /p hwnd") {
	ScreensaverCmdline::reset();
	bool consumed_next = false;
	CHECK(ScreensaverCmdline::try_consume("/p", "1234", consumed_next));
	CHECK(consumed_next);
	CHECK(ScreensaverCmdline::get_mode() == ScreensaverCmdline::MODE_PREVIEW);
	CHECK(ScreensaverCmdline::get_parent_hwnd() == 1234);
}

TEST_CASE("[Modules][Screensaver] consume /c and /c:hwnd and /C") {
	ScreensaverCmdline::reset();
	bool consumed_next = false;
	CHECK(ScreensaverCmdline::try_consume("/c", "", consumed_next));
	CHECK_FALSE(consumed_next);
	CHECK(ScreensaverCmdline::get_mode() == ScreensaverCmdline::MODE_CONFIGURE);

	ScreensaverCmdline::reset();
	CHECK(ScreensaverCmdline::try_consume("/c:1234", "", consumed_next));
	CHECK_FALSE(consumed_next);
	CHECK(ScreensaverCmdline::get_mode() == ScreensaverCmdline::MODE_CONFIGURE);
	CHECK(ScreensaverCmdline::get_parent_hwnd() == 1234);

	ScreensaverCmdline::reset();
	CHECK(ScreensaverCmdline::try_consume("/C", "", consumed_next));
	CHECK(ScreensaverCmdline::get_mode() == ScreensaverCmdline::MODE_CONFIGURE);
}

TEST_CASE("[Modules][Screensaver] consume /a hwnd and /A") {
	ScreensaverCmdline::reset();
	bool consumed_next = false;
	CHECK(ScreensaverCmdline::try_consume("/a", "1234", consumed_next));
	CHECK(consumed_next);
	CHECK(ScreensaverCmdline::get_mode() == ScreensaverCmdline::MODE_CHANGE_PASSWORD);
	CHECK(ScreensaverCmdline::get_parent_hwnd() == 1234);

	ScreensaverCmdline::reset();
	consumed_next = false;
	CHECK(ScreensaverCmdline::try_consume("/A", "99", consumed_next));
	CHECK(ScreensaverCmdline::get_mode() == ScreensaverCmdline::MODE_CHANGE_PASSWORD);
	CHECK(ScreensaverCmdline::get_parent_hwnd() == 99);
}

TEST_CASE("[Modules][Screensaver] consume hex HWND and colon /p /a") {
	ScreensaverCmdline::reset();
	int64_t hwnd = 0;
	CHECK(ScreensaverCmdline::parse_hwnd_token("0x1A2B", hwnd));
	CHECK(hwnd == 0x1A2B);
	CHECK(ScreensaverCmdline::parse_hwnd_token("0X10", hwnd));
	CHECK(hwnd == 0x10);
	CHECK_FALSE(ScreensaverCmdline::parse_hwnd_token("", hwnd));
	CHECK_FALSE(ScreensaverCmdline::parse_hwnd_token("not-a-hwnd", hwnd));

	bool consumed_next = false;
	ScreensaverCmdline::reset();
	CHECK(ScreensaverCmdline::try_consume("/p:4321", "", consumed_next));
	CHECK_FALSE(consumed_next);
	CHECK(ScreensaverCmdline::get_mode() == ScreensaverCmdline::MODE_PREVIEW);
	CHECK(ScreensaverCmdline::get_parent_hwnd() == 4321);

	ScreensaverCmdline::reset();
	CHECK(ScreensaverCmdline::try_consume("/a:0x20", "", consumed_next));
	CHECK(ScreensaverCmdline::get_mode() == ScreensaverCmdline::MODE_CHANGE_PASSWORD);
	CHECK(ScreensaverCmdline::get_parent_hwnd() == 0x20);
}

TEST_CASE("[Modules][Screensaver] ignore Godot -s --script --screen and -p") {
	ScreensaverCmdline::reset();
	bool consumed_next = false;
	CHECK_FALSE(ScreensaverCmdline::try_consume("-s", "res://x.gd", consumed_next));
	CHECK_FALSE(ScreensaverCmdline::try_consume("--script", "res://x.gd", consumed_next));
	CHECK_FALSE(ScreensaverCmdline::try_consume("--screen", "1", consumed_next));
	CHECK_FALSE(consumed_next);
	CHECK_FALSE(ScreensaverCmdline::try_consume("-p", "", consumed_next));
	CHECK_FALSE(ScreensaverCmdline::try_consume("--project-manager", "", consumed_next));
	CHECK(ScreensaverCmdline::get_mode() == ScreensaverCmdline::MODE_NONE);
}

TEST_CASE("[Modules][Screensaver] apply_recorded modes and enabled gate") {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	REQUIRE(ps != nullptr);
	const bool prev_enabled = bool(ps->get_setting("blazium/screensaver/enabled", false));
	const bool prev_cover = bool(ps->get_setting("blazium/screensaver/cover_all_screens", true));
	const String prev_cover_mode = ps->get_setting("blazium/screensaver/cover_mode", String());
	const int prev_screen = int(ps->get_setting("blazium/screensaver/screen", -1));

	ps->set_setting("blazium/screensaver/enabled", false);
	ScreensaverCmdline::reset();
	ScreensaverCmdline::clear_virtual_rect_for_tests();
	bool consumed_next = false;
	CHECK(ScreensaverCmdline::try_consume("/s", "", consumed_next));
	DisplayServer::WindowMode window_mode = DisplayServer::WINDOW_MODE_WINDOWED;
	uint32_t flags = 0;
	int64_t embed = 99;
	Vector2i pos;
	Size2i size;
	int screen = DisplayServer::SCREEN_PRIMARY;
	bool use_pos = true;
	_apply_screensaver(window_mode, flags, embed, pos, size, screen, use_pos);
	CHECK(window_mode == DisplayServer::WINDOW_MODE_WINDOWED);
	CHECK(embed == 99);
	CHECK_FALSE(use_pos);

	ps->set_setting("blazium/screensaver/enabled", true);
	ps->set_setting("blazium/screensaver/cover_mode", String());
	ps->set_setting("blazium/screensaver/cover_all_screens", false);
	ScreensaverCmdline::reset();
	CHECK(ScreensaverCmdline::try_consume("/s", "", consumed_next));
	window_mode = DisplayServer::WINDOW_MODE_WINDOWED;
	flags = 0;
	embed = 7;
	screen = DisplayServer::SCREEN_PRIMARY;
	use_pos = true;
	_apply_screensaver(window_mode, flags, embed, pos, size, screen, use_pos);
	CHECK(window_mode == DisplayServer::WINDOW_MODE_WINDOWED);
	CHECK(window_mode != DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN);
	CHECK((flags & DisplayServer::WINDOW_FLAG_BORDERLESS_BIT) != 0);
	CHECK(embed == 0);
	CHECK_FALSE(use_pos);
	CHECK(ScreensaverCmdline::should_clear_create_position());

	ScreensaverCmdline::reset();
	CHECK(ScreensaverCmdline::try_consume("/p", "1234", consumed_next));
	window_mode = DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN;
	flags = 0;
	embed = 0;
	use_pos = true;
	_apply_screensaver(window_mode, flags, embed, pos, size, screen, use_pos);
	CHECK(window_mode == DisplayServer::WINDOW_MODE_WINDOWED);
	CHECK((flags & DisplayServer::WINDOW_FLAG_BORDERLESS_BIT) != 0);
	CHECK(embed == 1234);

	ScreensaverCmdline::reset();
	CHECK(ScreensaverCmdline::try_consume("/c", "", consumed_next));
	window_mode = DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN;
	flags = DisplayServer::WINDOW_FLAG_BORDERLESS_BIT;
	embed = 5;
	use_pos = true;
	_apply_screensaver(window_mode, flags, embed, pos, size, screen, use_pos);
	CHECK(window_mode == DisplayServer::WINDOW_MODE_WINDOWED);
	CHECK((flags & DisplayServer::WINDOW_FLAG_BORDERLESS_BIT) == 0);
	CHECK(embed == 0);

	ScreensaverCmdline::reset();
	CHECK(ScreensaverCmdline::try_consume("/a", "88", consumed_next));
	window_mode = DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN;
	flags = 0;
	embed = 0;
	use_pos = true;
	_apply_screensaver(window_mode, flags, embed, pos, size, screen, use_pos);
	CHECK(window_mode == DisplayServer::WINDOW_MODE_WINDOWED);
	CHECK((flags & DisplayServer::WINDOW_FLAG_BORDERLESS_BIT) != 0);
	CHECK(embed == 88);

	ScreensaverCmdline::reset();
	ScreensaverCmdline::clear_virtual_rect_for_tests();
	ps->set_setting("blazium/screensaver/enabled", prev_enabled);
	ps->set_setting("blazium/screensaver/cover_all_screens", prev_cover);
	ps->set_setting("blazium/screensaver/cover_mode", prev_cover_mode);
	ps->set_setting("blazium/screensaver/screen", prev_screen);
}

TEST_CASE("[Modules][Screensaver] cover_mode virtual records windowed run without create rect") {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	REQUIRE(ps != nullptr);
	const bool prev_enabled = bool(ps->get_setting("blazium/screensaver/enabled", false));
	const bool prev_cover = bool(ps->get_setting("blazium/screensaver/cover_all_screens", true));
	const String prev_cover_mode = ps->get_setting("blazium/screensaver/cover_mode", String());

	const Rect2i virt(-1920, 0, 5760, 1080);
	ScreensaverCmdline::reset();
	ScreensaverCmdline::set_virtual_rect_for_tests(virt);
	ps->set_setting("blazium/screensaver/enabled", true);
	ps->set_setting("blazium/screensaver/cover_mode", "virtual");
	bool consumed_next = false;
	CHECK(ScreensaverCmdline::try_consume("/s", "", consumed_next));

	DisplayServer::WindowMode window_mode = DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN;
	uint32_t flags = 0;
	int64_t embed = 7;
	Vector2i pos;
	Size2i size;
	int screen = DisplayServer::SCREEN_PRIMARY;
	bool use_pos = false;
	_apply_screensaver(window_mode, flags, embed, pos, size, screen, use_pos);
	CHECK(window_mode == DisplayServer::WINDOW_MODE_WINDOWED);
	CHECK(window_mode != DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN);
	CHECK((flags & DisplayServer::WINDOW_FLAG_BORDERLESS_BIT) != 0);
	CHECK((flags & DisplayServer::WINDOW_FLAG_ALWAYS_ON_TOP_BIT) != 0);
	CHECK_FALSE(use_pos);
	CHECK(ScreensaverCmdline::should_clear_create_position());
	CHECK(ScreensaverCmdline::virtual_os_rect().size == Size2i(5760, 1080));
	CHECK(embed == 0);

	ps->set_setting("blazium/screensaver/cover_mode", String());
	ps->set_setting("blazium/screensaver/cover_all_screens", true);
	ScreensaverCmdline::reset();
	ScreensaverCmdline::set_virtual_rect_for_tests(virt);
	CHECK(ScreensaverCmdline::try_consume("/s", "", consumed_next));
	window_mode = DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN;
	flags = 0;
	screen = DisplayServer::SCREEN_PRIMARY;
	use_pos = false;
	_apply_screensaver(window_mode, flags, embed, pos, size, screen, use_pos);
	CHECK(window_mode == DisplayServer::WINDOW_MODE_WINDOWED);
	CHECK(window_mode != DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN);
	CHECK_FALSE(use_pos);
	CHECK(ScreensaverCmdline::should_clear_create_position());

	ScreensaverCmdline::reset();
	ScreensaverCmdline::clear_virtual_rect_for_tests();
	ps->set_setting("blazium/screensaver/enabled", prev_enabled);
	ps->set_setting("blazium/screensaver/cover_all_screens", prev_cover);
	ps->set_setting("blazium/screensaver/cover_mode", prev_cover_mode);
}

TEST_CASE("[Modules][Screensaver] cover_mode single honors screen index") {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	REQUIRE(ps != nullptr);
	const bool prev_enabled = bool(ps->get_setting("blazium/screensaver/enabled", false));
	const String prev_cover_mode = ps->get_setting("blazium/screensaver/cover_mode", String());
	const int prev_screen = int(ps->get_setting("blazium/screensaver/screen", -1));

	ps->set_setting("blazium/screensaver/enabled", true);
	ps->set_setting("blazium/screensaver/cover_mode", "single");
	ps->set_setting("blazium/screensaver/screen", -1);
	const Rect2i mon2(1920, 0, 1920, 1080);
	ScreensaverCmdline::reset();
	ScreensaverCmdline::set_monitor_rect_for_tests(2, mon2);
	bool consumed_next = false;
	CHECK(ScreensaverCmdline::try_consume("/s:2", "", consumed_next));

	DisplayServer::WindowMode window_mode = DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN;
	uint32_t flags = 0;
	int64_t embed = 7;
	Vector2i pos;
	Size2i size;
	int screen = DisplayServer::SCREEN_PRIMARY;
	bool use_pos = false;
	_apply_screensaver(window_mode, flags, embed, pos, size, screen, use_pos);
	CHECK(window_mode == DisplayServer::WINDOW_MODE_WINDOWED);
	CHECK(window_mode != DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN);
	CHECK(window_mode != DisplayServer::WINDOW_MODE_FULLSCREEN);
	CHECK(screen == 2);
	CHECK_FALSE(use_pos);
	CHECK(embed == 0);

	ScreensaverCmdline::reset();
	ScreensaverCmdline::set_monitor_rect_for_tests(1, Rect2i(0, 0, 1280, 1024));
	ps->set_setting("blazium/screensaver/screen", 1);
	CHECK(ScreensaverCmdline::try_consume("/s", "", consumed_next));
	window_mode = DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN;
	screen = DisplayServer::SCREEN_PRIMARY;
	use_pos = false;
	_apply_screensaver(window_mode, flags, embed, pos, size, screen, use_pos);
	CHECK(window_mode == DisplayServer::WINDOW_MODE_WINDOWED);
	CHECK(screen == 1);
	CHECK_FALSE(use_pos);

	ScreensaverCmdline::clear_monitor_rect_for_tests();

	ScreensaverCmdline::reset();
	ps->set_setting("blazium/screensaver/enabled", prev_enabled);
	ps->set_setting("blazium/screensaver/cover_mode", prev_cover_mode);
	ps->set_setting("blazium/screensaver/screen", prev_screen);
}

TEST_CASE("[Modules][Screensaver] cover_mode clone records run without spawning") {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	REQUIRE(ps != nullptr);
	const bool prev_enabled = bool(ps->get_setting("blazium/screensaver/enabled", false));
	const String prev_cover_mode = ps->get_setting("blazium/screensaver/cover_mode", String());

	ps->set_setting("blazium/screensaver/enabled", true);
	ps->set_setting("blazium/screensaver/cover_mode", "clone");
	ScreensaverCmdline::reset();
	bool consumed_next = false;
	CHECK(ScreensaverCmdline::try_consume("/s", "", consumed_next));
	CHECK(ScreensaverCmdline::get_mode() == ScreensaverCmdline::MODE_RUN);
	CHECK(ScreensaverCmdline::resolved_cover_mode() == "clone");

	DisplayServer::WindowMode window_mode = DisplayServer::WINDOW_MODE_WINDOWED;
	uint32_t flags = 0;
	int64_t embed = 7;
	Vector2i pos;
	Size2i size;
	int screen = DisplayServer::SCREEN_PRIMARY;
	bool use_pos = true;
	_apply_screensaver(window_mode, flags, embed, pos, size, screen, use_pos);
	CHECK(window_mode == DisplayServer::WINDOW_MODE_WINDOWED);
	CHECK(window_mode != DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN);
	CHECK_FALSE(use_pos);

	CHECK_FALSE(ScreensaverCmdline::should_spawn_clone_windows());
	if (Screensaver::get_singleton()) {
		CHECK(Screensaver::get_singleton()->get_mode() == Screensaver::MODE_RUN);
	}

	ScreensaverCmdline::reset();
	ps->set_setting("blazium/screensaver/enabled", prev_enabled);
	ps->set_setting("blazium/screensaver/cover_mode", prev_cover_mode);
}

TEST_CASE("[Modules][Screensaver] Main --screen notify overrides virtual cover") {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	REQUIRE(ps != nullptr);
	const bool prev_enabled = bool(ps->get_setting("blazium/screensaver/enabled", false));
	const String prev_cover_mode = ps->get_setting("blazium/screensaver/cover_mode", String());

	const Rect2i mon2(1920, 0, 1920, 1080);
	ps->set_setting("blazium/screensaver/enabled", true);
	ps->set_setting("blazium/screensaver/cover_mode", "virtual");
	ScreensaverCmdline::reset();
	ScreensaverCmdline::set_target_screen(2);
	ScreensaverCmdline::set_monitor_rect_for_tests(2, mon2);
	bool consumed_next = false;
	CHECK(ScreensaverCmdline::try_consume("/s", "", consumed_next));
	CHECK(ScreensaverCmdline::resolved_cover_mode() == "single");

	DisplayServer::WindowMode window_mode = DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN;
	uint32_t flags = 0;
	int64_t embed = 7;
	Vector2i pos;
	Size2i size;
	int screen = DisplayServer::SCREEN_PRIMARY;
	bool use_pos = false;
	_apply_screensaver(window_mode, flags, embed, pos, size, screen, use_pos);
	CHECK(window_mode == DisplayServer::WINDOW_MODE_WINDOWED);
	CHECK(window_mode != DisplayServer::WINDOW_MODE_FULLSCREEN);
	CHECK(screen == 2);
	CHECK_FALSE(use_pos);
	CHECK(ScreensaverCmdline::should_clear_create_position());

	ScreensaverCmdline::reset();
	ScreensaverCmdline::clear_monitor_rect_for_tests();
	ps->set_setting("blazium/screensaver/enabled", prev_enabled);
	ps->set_setting("blazium/screensaver/cover_mode", prev_cover_mode);
}

TEST_CASE("[Modules][Screensaver] --screen N overrides virtual cover") {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	REQUIRE(ps != nullptr);
	const bool prev_enabled = bool(ps->get_setting("blazium/screensaver/enabled", false));
	const String prev_cover_mode = ps->get_setting("blazium/screensaver/cover_mode", String());

	const Rect2i mon2(1920, 0, 1920, 1080);
	ps->set_setting("blazium/screensaver/enabled", true);
	ps->set_setting("blazium/screensaver/cover_mode", "virtual");
	ScreensaverCmdline::reset();
	ScreensaverCmdline::set_monitor_rect_for_tests(2, mon2);
	bool consumed_next = false;
	CHECK(ScreensaverCmdline::try_consume("/s", "", consumed_next));

	DisplayServer::WindowMode window_mode = DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN;
	uint32_t flags = 0;
	int64_t embed = 7;
	Vector2i pos;
	Size2i size;
	int screen = 2;
	bool use_pos = false;
	_apply_screensaver(window_mode, flags, embed, pos, size, screen, use_pos);
	CHECK(ScreensaverCmdline::resolved_cover_mode() == "single");
	CHECK(window_mode == DisplayServer::WINDOW_MODE_WINDOWED);
	CHECK(window_mode != DisplayServer::WINDOW_MODE_FULLSCREEN);
	CHECK(window_mode != DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN);
	CHECK(screen == 2);
	CHECK_FALSE(use_pos);

	ScreensaverCmdline::reset();
	ScreensaverCmdline::clear_monitor_rect_for_tests();
	ps->set_setting("blazium/screensaver/enabled", prev_enabled);
	ps->set_setting("blazium/screensaver/cover_mode", prev_cover_mode);
}
