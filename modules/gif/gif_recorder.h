/**************************************************************************/
/*  gif_recorder.h                                                        */
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

#include "core/object/ref_counted.h"
#include "gif_texture.h"

class Viewport;
class Window;

class GIFRecorder : public RefCounted {
	GDCLASS(GIFRecorder, RefCounted);

public:
	enum Source {
		SOURCE_VIEWPORT,
		SOURCE_WINDOW,
		SOURCE_SCREEN,
	};

private:
	int fps = 12;
	Vector2i max_size = Vector2i(480, 270);
	int loop_count = 0;
	int max_frames = 0;
	bool dither = true;
	bool recording = false;

	Source source = SOURCE_VIEWPORT;
	ObjectID viewport_id;
	int screen_index = 0;
	double accum = 0.0;
	double frame_interval = 1.0 / 12.0;

	Ref<GIFTexture> texture;
	String pending_path;

	void _process_frame();
	Ref<Image> _capture_image() const;
	Ref<Image> _prepare_image(const Ref<Image> &p_image) const;
	void _connect_process(bool p_connect);
	Error _start_common();

protected:
	static void _bind_methods();

public:
	void set_fps(int p_fps);
	int get_fps() const;
	void set_max_size(const Vector2i &p_size);
	Vector2i get_max_size() const;
	void set_loop_count(int p_count);
	int get_loop_count() const;
	void set_max_frames(int p_frames);
	int get_max_frames() const;
	void set_dither(bool p_dither);
	bool get_dither() const;
	bool is_recording() const;

	Error start_viewport(Viewport *p_viewport);
	Error start_window();
	Error start_screen(int p_screen = 0);
	Error add_frame(const Ref<Image> &p_image);
	Ref<GIFTexture> stop();
	Error save(const String &p_path);

	static Error record_viewport(Viewport *p_viewport, const String &p_path, double p_duration_sec, int p_fps = 12);

	~GIFRecorder();
};

VARIANT_ENUM_CAST(GIFRecorder::Source);
