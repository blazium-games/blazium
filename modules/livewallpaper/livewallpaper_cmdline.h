/**************************************************************************/
/*  livewallpaper_cmdline.h                                               */
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

#include "core/math/vector2i.h"
#include "core/string/ustring.h"
#include "servers/display_server.h"

class LiveWallpaperCmdline {
public:
	enum Mode {
		MODE_NONE,
		MODE_RUN,
		MODE_PREVIEW,
		MODE_QUIT,
	};

	static Mode get_mode();
	static int64_t get_embed_hwnd();
	static void set_embed_hwnd(int64_t p_hwnd);
	static void reset();

	static bool try_consume(const String &p_arg, const String &p_next, bool &r_consumed_next);
	static void apply_recorded(DisplayServer::WindowMode &r_window_mode, uint32_t &r_window_flags, Vector2i &r_window_position, Size2i &r_window_size, int64_t &r_embed_parent_hwnd, bool &r_use_position);

	static String mutex_name();
	static String quit_event_name();

private:
	static Mode mode;
	static int64_t embed_hwnd;
};
