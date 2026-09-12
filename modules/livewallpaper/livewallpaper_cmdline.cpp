/**************************************************************************/
/*  livewallpaper_cmdline.cpp                                             */
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

#include "livewallpaper_cmdline.h"

#include "core/config/project_settings.h"
#include "core/os/os.h"
#include "modules/livewallpaper/livewallpaper_workerw.h"

static bool _is_non_editor_binary() {
	if (!OS::get_singleton()) {
		return false;
	}
	const String exe = OS::get_singleton()->get_executable_path().get_file().to_lower();
	return !exe.contains("editor");
}

LiveWallpaperCmdline::Mode LiveWallpaperCmdline::mode = LiveWallpaperCmdline::MODE_NONE;
int64_t LiveWallpaperCmdline::embed_hwnd = 0;

LiveWallpaperCmdline::Mode LiveWallpaperCmdline::get_mode() {
	return mode;
}

int64_t LiveWallpaperCmdline::get_embed_hwnd() {
	return embed_hwnd;
}

void LiveWallpaperCmdline::set_embed_hwnd(int64_t p_hwnd) {
	embed_hwnd = p_hwnd;
}

void LiveWallpaperCmdline::reset() {
	mode = MODE_NONE;
	embed_hwnd = 0;
}

static String _app_key() {
	String name = "Blazium";
	if (ProjectSettings::get_singleton()) {
		const String app = ProjectSettings::get_singleton()->get_setting("application/config/name", "Blazium");
		if (!String(app).is_empty()) {
			name = String(app);
		}
	}
	return name.validate_filename();
}

String LiveWallpaperCmdline::mutex_name() {
	return "Local\\BlaziumLiveWallpaper-" + _app_key();
}

String LiveWallpaperCmdline::quit_event_name() {
	return "Local\\BlaziumLiveWallpaper-Quit-" + _app_key();
}

bool LiveWallpaperCmdline::try_consume(const String &p_arg, const String &p_next, bool &r_consumed_next) {
	r_consumed_next = false;
	(void)p_next;
	if (p_arg == "-s" || p_arg == "--script" || p_arg == "-p" || p_arg == "--project-manager") {
		return false;
	}
	if (p_arg == "--livewallpaper-preview" || p_arg == "/p" || p_arg == "/P") {
		mode = MODE_PREVIEW;
		return true;
	}
	if (p_arg == "--livewallpaper-quit") {
		mode = MODE_QUIT;
		return true;
	}
	return false;
}

void LiveWallpaperCmdline::apply_recorded(DisplayServer::WindowMode &r_window_mode, uint32_t &r_window_flags, Vector2i &r_window_position, Size2i &r_window_size, int64_t &r_embed_parent_hwnd, bool &r_use_position) {
	r_use_position = false;
	ProjectSettings *ps = ProjectSettings::get_singleton();
	if (ps == nullptr || !bool(ps->get_setting("blazium/livewallpaper/enabled", false))) {
		return;
	}

	Mode applied = mode;
	if (applied == MODE_NONE) {
		if (LiveWallpaperWorkerW::has_test_override() || _is_non_editor_binary()) {
			applied = MODE_RUN;
			mode = MODE_RUN;
		}
	}
	if (applied == MODE_NONE) {
		return;
	}
	if (applied == MODE_QUIT) {
		r_window_mode = DisplayServer::WINDOW_MODE_WINDOWED;
		r_window_flags &= ~DisplayServer::WINDOW_FLAG_BORDERLESS_BIT;
		r_embed_parent_hwnd = 0;
		embed_hwnd = 0;
		return;
	}
	if (applied == MODE_PREVIEW) {
		r_window_mode = DisplayServer::WINDOW_MODE_WINDOWED;
		r_window_flags |= DisplayServer::WINDOW_FLAG_BORDERLESS_BIT;
		r_embed_parent_hwnd = 0;
		embed_hwnd = 0;
		r_window_size = Size2i(640, 360);
		return;
	}

	embed_hwnd = LiveWallpaperWorkerW::find_workerw();
	r_embed_parent_hwnd = 0;
	r_window_mode = DisplayServer::WINDOW_MODE_WINDOWED;
	r_window_flags |= DisplayServer::WINDOW_FLAG_BORDERLESS_BIT;
	r_window_flags |= DisplayServer::WINDOW_FLAG_NO_FOCUS_BIT;
	r_window_position = Vector2i(-32000, -32000);

	r_window_size = Size2i(256, 256);
	r_use_position = true;
}
