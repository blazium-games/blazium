/**************************************************************************/
/*  test_livewallpaper_cmdline.h                                          */
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
#include "modules/livewallpaper/livewallpaper_cmdline.h"
#include "modules/livewallpaper/livewallpaper_workerw.h"
#include "tests/test_macros.h"

TEST_CASE("[Modules][LiveWallpaper] consume preview and quit") {
	LiveWallpaperCmdline::reset();
	bool consumed_next = false;
	CHECK(LiveWallpaperCmdline::try_consume("--livewallpaper-preview", "", consumed_next));
	CHECK_FALSE(consumed_next);
	CHECK(LiveWallpaperCmdline::get_mode() == LiveWallpaperCmdline::MODE_PREVIEW);

	LiveWallpaperCmdline::reset();
	CHECK(LiveWallpaperCmdline::try_consume("/p", "", consumed_next));
	CHECK(LiveWallpaperCmdline::get_mode() == LiveWallpaperCmdline::MODE_PREVIEW);

	LiveWallpaperCmdline::reset();
	CHECK(LiveWallpaperCmdline::try_consume("--livewallpaper-quit", "", consumed_next));
	CHECK(LiveWallpaperCmdline::get_mode() == LiveWallpaperCmdline::MODE_QUIT);
}

TEST_CASE("[Modules][LiveWallpaper] ignore Godot -s --script and -p") {
	LiveWallpaperCmdline::reset();
	bool consumed_next = false;
	CHECK_FALSE(LiveWallpaperCmdline::try_consume("-s", "res://x.gd", consumed_next));
	CHECK_FALSE(LiveWallpaperCmdline::try_consume("--script", "res://x.gd", consumed_next));
	CHECK_FALSE(LiveWallpaperCmdline::try_consume("-p", "", consumed_next));
	CHECK_FALSE(LiveWallpaperCmdline::try_consume("--project-manager", "", consumed_next));
	CHECK(LiveWallpaperCmdline::get_mode() == LiveWallpaperCmdline::MODE_NONE);
}

TEST_CASE("[Modules][LiveWallpaper] apply_recorded modes and enabled gate") {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	REQUIRE(ps != nullptr);
	const bool prev_enabled = bool(ps->get_setting("blazium/livewallpaper/enabled", false));
	const bool prev_cover = bool(ps->get_setting("blazium/livewallpaper/cover_all_screens", true));

	LiveWallpaperWorkerW::reset_for_tests();
	LiveWallpaperWorkerW::set_workerw_for_tests(12345);
	LiveWallpaperWorkerW::set_virtual_rect_for_tests(Rect2i(10, 20, 800, 600));
	LiveWallpaperWorkerW::set_primary_rect_for_tests(Rect2i(0, 0, 640, 480));

	ps->set_setting("blazium/livewallpaper/enabled", false);
	LiveWallpaperCmdline::reset();
	bool consumed_next = false;
	CHECK(LiveWallpaperCmdline::try_consume("--livewallpaper-preview", "", consumed_next));
	DisplayServer::WindowMode window_mode = DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN;
	uint32_t flags = 0;
	Vector2i pos(1, 1);
	Size2i size(100, 100);
	int64_t embed = 99;
	bool use_pos = true;
	LiveWallpaperCmdline::apply_recorded(window_mode, flags, pos, size, embed, use_pos);
	CHECK(window_mode == DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN);
	CHECK(embed == 99);
	CHECK_FALSE(use_pos);

	ps->set_setting("blazium/livewallpaper/enabled", true);
	LiveWallpaperCmdline::reset();
	CHECK(LiveWallpaperCmdline::try_consume("--livewallpaper-preview", "", consumed_next));
	window_mode = DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN;
	flags = 0;
	pos = Vector2i(1, 1);
	size = Size2i(100, 100);
	embed = 99;
	use_pos = true;
	LiveWallpaperCmdline::apply_recorded(window_mode, flags, pos, size, embed, use_pos);
	CHECK(window_mode == DisplayServer::WINDOW_MODE_WINDOWED);
	CHECK((flags & DisplayServer::WINDOW_FLAG_BORDERLESS_BIT) != 0);
	CHECK(embed == 0);
	CHECK(LiveWallpaperCmdline::get_embed_hwnd() == 0);
	CHECK(size == Size2i(640, 360));
	CHECK_FALSE(use_pos);

	ps->set_setting("blazium/livewallpaper/cover_all_screens", true);
	LiveWallpaperCmdline::reset();
	window_mode = DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN;
	flags = 0;
	pos = Vector2i();
	size = Size2i();
	embed = 0;
	use_pos = false;
	LiveWallpaperCmdline::apply_recorded(window_mode, flags, pos, size, embed, use_pos);
	CHECK(LiveWallpaperCmdline::get_mode() == LiveWallpaperCmdline::MODE_RUN);
	CHECK(window_mode == DisplayServer::WINDOW_MODE_WINDOWED);
	CHECK((flags & DisplayServer::WINDOW_FLAG_BORDERLESS_BIT) != 0);
	CHECK((flags & DisplayServer::WINDOW_FLAG_NO_FOCUS_BIT) != 0);
	CHECK(embed == 0);
	CHECK(LiveWallpaperCmdline::get_embed_hwnd() == 12345);
	CHECK(size == Size2i(256, 256));
	CHECK(use_pos);

	ps->set_setting("blazium/livewallpaper/cover_all_screens", false);
	LiveWallpaperCmdline::reset();
	window_mode = DisplayServer::WINDOW_MODE_WINDOWED;
	flags = 0;
	pos = Vector2i();
	size = Size2i();
	embed = 7;
	use_pos = false;
	LiveWallpaperCmdline::apply_recorded(window_mode, flags, pos, size, embed, use_pos);
	CHECK(embed == 0);
	CHECK(LiveWallpaperCmdline::get_embed_hwnd() == 12345);
	CHECK(size == Size2i(256, 256));

	CHECK(LiveWallpaperCmdline::mutex_name().contains("BlaziumLiveWallpaper-"));
	CHECK(LiveWallpaperCmdline::quit_event_name().contains("BlaziumLiveWallpaper-Quit-"));

	LiveWallpaperCmdline::reset();
	LiveWallpaperWorkerW::reset_for_tests();
	ps->set_setting("blazium/livewallpaper/enabled", prev_enabled);
	ps->set_setting("blazium/livewallpaper/cover_all_screens", prev_cover);
}
