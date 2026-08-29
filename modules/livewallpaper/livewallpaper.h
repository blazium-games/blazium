/**************************************************************************/
/*  livewallpaper.h                                                       */
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
#include "core/templates/rid.h"
#include "modules/livewallpaper/livewallpaper_cmdline.h"

class LiveWallpaper : public Object {
	GDCLASS(LiveWallpaper, Object);

public:
	enum Mode {
		MODE_DISABLED = LiveWallpaperCmdline::MODE_NONE,
		MODE_RUN = LiveWallpaperCmdline::MODE_RUN,
		MODE_PREVIEW = LiveWallpaperCmdline::MODE_PREVIEW,
		MODE_QUIT = LiveWallpaperCmdline::MODE_QUIT,
	};

	static LiveWallpaper *get_singleton();

	Mode get_mode() const;
	bool is_attached() const;
	int64_t get_workerw() const;
	void request_exit();

	void process_frame();
	void present_frame();
	void setup_runtime();

	static void register_project_settings();

	LiveWallpaper();
	~LiveWallpaper();

protected:
	static void _bind_methods();

private:
	static LiveWallpaper *singleton;

	bool frame_hooked = false;
	bool present_hooked = false;
	bool session_paused = false;
	int attach_settle_frames = 0;
	Size2i last_synced_viewport;
	float overlay_time = 0.0f;
	RID overlay_canvas;
	RID overlay_item;
	void *mutex_handle = nullptr;
	void *quit_event = nullptr;

	void _connect_frame_hook();
	void _disconnect_frame_hook();
	void _release_sync();
	void _signal_quit_event();
	void _ensure_attached();
	void _sync_root_viewport(const Size2i &p_size);
	void _ensure_overlay_canvas();
	void _update_overlay_canvas(const Size2i &p_size);
	void _free_overlay_canvas();
	bool _session_locked() const;
};

VARIANT_ENUM_CAST(LiveWallpaper::Mode);
