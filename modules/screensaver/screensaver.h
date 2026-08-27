/**************************************************************************/
/*  screensaver.h                                                         */
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

#include "core/object/object.h"
#include "core/templates/vector.h"
#include "modules/screensaver/screensaver_cmdline.h"

class AcceptDialog;
class CheckBox;
class LineEdit;
class Window;

class Screensaver : public Object {
	GDCLASS(Screensaver, Object);

public:
	enum Mode {
		MODE_DISABLED = ScreensaverCmdline::MODE_NONE,
		MODE_RUN = ScreensaverCmdline::MODE_RUN,
		MODE_PREVIEW = ScreensaverCmdline::MODE_PREVIEW,
		MODE_CONFIGURE = ScreensaverCmdline::MODE_CONFIGURE,
		MODE_CHANGE_PASSWORD = ScreensaverCmdline::MODE_CHANGE_PASSWORD,
	};

	static Screensaver *get_singleton();

	Mode get_mode() const;
	bool is_preview() const;
	bool is_password_enabled() const;
	void set_password_enabled(bool p_enabled);
	bool has_password() const;
	void request_exit();
	bool verify_password(const String &p_plain) const;
	Error set_password(const String &p_old_plain, const String &p_new_plain);
	Error clear_password(const String &p_current_plain);

	void process_frame();
	void setup_runtime();

	static void register_project_settings();

	Screensaver();
	~Screensaver();

protected:
	static void _bind_methods();

private:
	static Screensaver *singleton;

	struct CloneCover {
		Window *window = nullptr;
		int screen = -1;
	};

	bool frame_hooked = false;
	bool unlock_open = false;
	bool mouse_origin_set = false;
	Vector2 mouse_origin;
	bool input_armed = false;
	bool clones_spawned = false;
	bool place_log_written = false;
	Vector<CloneCover> clone_covers;

	void _connect_frame_hook();
	void _disconnect_frame_hook();
	void _apply_run_screens();
	void _place_hwnd(const Rect2i &p_os_rect);
	void _place_window_hwnd(Window *p_window, const Rect2i &p_os_rect);
	void _pin_window_to_screen(int p_screen);
	void _sync_root_window_size(const Size2i &p_size);
	void _write_place_log(const Rect2i &p_target, int64_t p_hwnd);
	bool _virtual_span_clipped(const Rect2i &p_want) const;
	void _spawn_clone_windows(int p_skip_screen);
	void _place_clone_windows();
	void _load_or_show_scene(const String &p_setting, void (Screensaver::*p_builtin)());
	void _show_configure_dialog();
	void _show_unlock_dialog();
	void _show_change_password_dialog();
	void _finish_exit();
	void _on_configure_password_toggled(bool p_pressed);
	void _on_configure_change_password();
	void _on_unlock_accepted();
	void _on_change_password_accepted();
	bool _should_quit_on_input() const;
};

VARIANT_ENUM_CAST(Screensaver::Mode);
