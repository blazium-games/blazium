/**************************************************************************/
/*  test_livewallpaper_workerw.h                                          */
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

#include "modules/livewallpaper/livewallpaper_workerw.h"
#include "tests/test_macros.h"

TEST_CASE("[Modules][LiveWallpaper] workerw helpers and test inject") {
	LiveWallpaperWorkerW::reset_for_tests();
	LiveWallpaperWorkerW::find_workerw();

	LiveWallpaperWorkerW::set_workerw_for_tests(4242);
	CHECK(LiveWallpaperWorkerW::find_workerw() == 4242);

	LiveWallpaperWorkerW::set_virtual_rect_for_tests(Rect2i(5, 6, 700, 500));
	CHECK(LiveWallpaperWorkerW::virtual_screen_rect() == Rect2i(5, 6, 700, 500));

	LiveWallpaperWorkerW::set_primary_rect_for_tests(Rect2i(0, 0, 320, 240));
	CHECK(LiveWallpaperWorkerW::primary_screen_rect() == Rect2i(0, 0, 320, 240));

	const Rect2i live = LiveWallpaperWorkerW::virtual_screen_rect();
	CHECK(live.size.x > 0);
	CHECK(live.size.y > 0);

	LiveWallpaperWorkerW::reset_for_tests();
	LiveWallpaperWorkerW::find_workerw();

	CHECK_FALSE(LiveWallpaperWorkerW::attach_window(0, 1, true));
	CHECK_FALSE(LiveWallpaperWorkerW::attach_window(1, 0, true));
	CHECK_FALSE(LiveWallpaperWorkerW::is_child_of(0, 1));
	CHECK_FALSE(LiveWallpaperWorkerW::is_fully_attached(0, 1, true));
	CHECK_FALSE(LiveWallpaperWorkerW::is_fully_attached(1, 0, false));
	CHECK_FALSE(LiveWallpaperWorkerW::present_bgra(0, true, 4, 4, nullptr));

	LiveWallpaperWorkerW::set_virtual_rect_for_tests(Rect2i(-100, -50, 2560, 1440));
	LiveWallpaperWorkerW::set_primary_rect_for_tests(Rect2i(0, 0, 1920, 1080));
	CHECK(LiveWallpaperWorkerW::attach_rect(0, true) == Rect2i(-100, -50, 2560, 1440));
	CHECK(LiveWallpaperWorkerW::attach_rect(0, false) == Rect2i(0, 0, 1920, 1080));

	LiveWallpaperWorkerW::reset_for_tests();
}
