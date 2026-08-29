/**************************************************************************/
/*  livewallpaper_workerw.h                                               */
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

#include "core/math/rect2i.h"

class LiveWallpaperWorkerW {
public:
	static int64_t find_workerw();
	static bool attach_window(int64_t p_hwnd, int64_t p_workerw, bool p_cover_all);
	static bool is_child_of(int64_t p_hwnd, int64_t p_workerw);
	static bool is_fully_attached(int64_t p_hwnd, int64_t p_workerw, bool p_cover_all);
	static Rect2i attach_rect(int64_t p_workerw, bool p_cover_all);
	static bool present_bgra(int64_t p_workerw, bool p_cover_all, int p_src_width, int p_src_height, const uint8_t *p_bgra);
	static Rect2i virtual_screen_rect();
	static Rect2i primary_screen_rect();

	static void reset_for_tests();
	static bool has_test_override();
	static void set_workerw_for_tests(int64_t p_hwnd);
	static void set_virtual_rect_for_tests(const Rect2i &p_rect);
	static void set_primary_rect_for_tests(const Rect2i &p_rect);

private:
	static bool test_override;
	static int64_t test_hwnd;
	static bool test_virtual_override;
	static Rect2i test_virtual;
	static bool test_primary_override;
	static Rect2i test_primary;
};
