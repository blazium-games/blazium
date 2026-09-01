/**************************************************************************/
/*  screensaver_cmdline.h                                                 */
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
#include "core/math/vector2i.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "servers/display_server.h"

class ScreensaverCmdline {
public:
	enum Mode {
		MODE_NONE,
		MODE_RUN,
		MODE_PREVIEW,
		MODE_CONFIGURE,
		MODE_CHANGE_PASSWORD,
	};

	static Mode get_mode();
	static int64_t get_parent_hwnd();
	static void reset();

	static void set_target_screen(int p_screen);
	static int get_target_screen_override();

	static void ingest_from_os_command_line();

	static bool try_consume(const String &p_arg, const String &p_next, bool &r_consumed_next);

	static void apply_recorded(DisplayServer::WindowMode &r_window_mode, uint32_t &r_window_flags, int64_t &r_embed_parent_hwnd, Vector2i &r_window_position, Size2i &r_window_size, int &r_screen, bool &r_use_position);

	static bool parse_hwnd_token(const String &p_token, int64_t &r_hwnd);
	static bool is_hwnd_valid(int64_t p_hwnd);
	static void set_hwnd_valid_for_tests(int64_t p_hwnd);
	static void clear_hwnd_valid_for_tests();
	static Size2i parent_client_size();

	static String resolved_cover_mode();

	static bool is_host_screensaver_launch();
	static void set_host_screensaver_launch_for_tests(bool p_host);
	static void clear_host_screensaver_launch_for_tests();

	static int resolved_target_screen(int p_current_screen = DisplayServer::SCREEN_PRIMARY);

	static int normalize_screen_index(int p_screen, int p_count);

	static int os_monitor_count();
	static void set_os_monitor_count_for_tests(int p_count);
	static void clear_os_monitor_count_for_tests();

	static Vector<Rect2i> order_monitor_rects(const Vector<Rect2i> &p_rects, int p_primary);

	static bool should_clear_create_position();

	static Rect2i virtual_screen_rect();

	static Rect2i virtual_os_rect();
	static Rect2i monitor_rect(int p_screen);

	static Rect2i monitor_os_rect(int p_screen);

	static Rect2i cover_os_rect(int p_screen);

	static Rect2i cover_rect(int p_screen);
	static void set_virtual_rect_for_tests(const Rect2i &p_rect);
	static void clear_virtual_rect_for_tests();
	static void set_monitor_rect_for_tests(int p_screen, const Rect2i &p_rect);
	static void clear_monitor_rect_for_tests();

	static bool should_spawn_clone_windows();

	static void apply_default_mode_if_scr();
	static void set_executable_is_scr_for_tests(bool p_scr);
	static void clear_executable_is_scr_for_tests();

private:
	static Mode mode;
	static int64_t parent_hwnd;
	static String cover_override;
	static int screen_override;
	static bool virtual_rect_overridden;
	static Rect2i virtual_rect_override;
	static bool monitor_rect_overridden;
	static int monitor_rect_override_index;
	static Rect2i monitor_rect_override;
	static bool os_monitor_count_overridden;
	static int os_monitor_count_override;
	static bool host_launch_overridden;
	static bool host_launch_override;
	static bool executable_is_scr_overridden;
	static bool executable_is_scr_override;
	static bool hwnd_valid_overridden;
	static int64_t hwnd_valid_override;
};
