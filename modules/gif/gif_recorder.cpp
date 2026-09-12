/**************************************************************************/
/*  gif_recorder.cpp                                                      */
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

#include "gif_recorder.h"

#include "gif_decode.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/os/os.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "servers/display_server.h"

void GIFRecorder::_connect_process(bool p_connect) {
	MainLoop *ml = OS::get_singleton() ? OS::get_singleton()->get_main_loop() : nullptr;
	SceneTree *tree = Object::cast_to<SceneTree>(ml);
	if (!tree) {
		return;
	}
	const Callable cb = callable_mp(this, &GIFRecorder::_process_frame);
	if (p_connect) {
		if (!tree->is_connected(SNAME("process_frame"), cb)) {
			tree->connect(SNAME("process_frame"), cb);
		}
	} else if (tree->is_connected(SNAME("process_frame"), cb)) {
		tree->disconnect(SNAME("process_frame"), cb);
	}
}

Error GIFRecorder::_start_common() {
	ERR_FAIL_COND_V_MSG(recording, ERR_ALREADY_IN_USE, "GIFRecorder is already recording.");
	texture.instantiate();
	texture->set_netscape_loop_count(loop_count);
	texture->set_dither(dither);
	texture->set_play(false);
	accum = 0;
	frame_interval = fps > 0 ? (1.0 / double(fps)) : (1.0 / 12.0);
	recording = true;
	_connect_process(true);
	return OK;
}

Ref<Image> GIFRecorder::_capture_image() const {
	if (source == SOURCE_VIEWPORT) {
		Viewport *vp = Object::cast_to<Viewport>(ObjectDB::get_instance(viewport_id));
		if (!vp || vp->get_texture().is_null()) {
			return Ref<Image>();
		}
		return vp->get_texture()->get_image();
	}
	if (source == SOURCE_WINDOW) {
		if (!DisplayServer::get_singleton()) {
			return Ref<Image>();
		}
		const Vector2i pos = DisplayServer::get_singleton()->window_get_position_with_decorations();
		const Vector2i size = DisplayServer::get_singleton()->window_get_size_with_decorations();
		if (size.x <= 0 || size.y <= 0) {
			return Ref<Image>();
		}
		return DisplayServer::get_singleton()->screen_get_image_rect(Rect2i(pos, size));
	}
	if (!DisplayServer::get_singleton()) {
		return Ref<Image>();
	}
	return DisplayServer::get_singleton()->screen_get_image(screen_index);
}

Ref<Image> GIFRecorder::_prepare_image(const Ref<Image> &p_image) const {
	ERR_FAIL_COND_V(p_image.is_null() || p_image->is_empty(), Ref<Image>());
	Ref<Image> img = p_image->duplicate();
	if (img->is_compressed()) {
		img->decompress();
	}
	if (max_size.x > 0 && max_size.y > 0 && (img->get_width() > max_size.x || img->get_height() > max_size.y)) {
		img->resize(max_size.x, max_size.y, Image::INTERPOLATE_BILINEAR);
	}
	const int64_t pixels = int64_t(img->get_width()) * int64_t(img->get_height());
	ERR_FAIL_COND_V_MSG(pixels > gif_get_max_canvas_pixels(), Ref<Image>(), "Captured frame exceeds blazium/gif/max_canvas_pixels.");
	return img;
}

void GIFRecorder::_process_frame() {
	if (!recording) {
		return;
	}
	const double dt = Engine::get_singleton() ? Engine::get_singleton()->get_process_step() : frame_interval;
	accum += dt;
	if (accum < frame_interval) {
		return;
	}
	accum -= frame_interval;
	add_frame(_capture_image());
}

Error GIFRecorder::start_viewport(Viewport *p_viewport) {
	ERR_FAIL_NULL_V_MSG(p_viewport, ERR_INVALID_PARAMETER, "GIFRecorder.start_viewport requires a Viewport.");
	source = SOURCE_VIEWPORT;
	viewport_id = p_viewport->get_instance_id();
	return _start_common();
}

Error GIFRecorder::start_window() {
	source = SOURCE_WINDOW;
	return _start_common();
}

Error GIFRecorder::start_screen(int p_screen) {
	source = SOURCE_SCREEN;
	screen_index = p_screen;
	return _start_common();
}

Error GIFRecorder::add_frame(const Ref<Image> &p_image) {
	Ref<Image> img = _prepare_image(p_image);
	ERR_FAIL_COND_V(img.is_null(), ERR_INVALID_PARAMETER);
	if (texture.is_null()) {
		texture.instantiate();
		texture->set_netscape_loop_count(loop_count);
		texture->set_dither(dither);
		texture->set_play(false);
	}
	const int cap = max_frames > 0 ? max_frames : gif_get_max_frames();
	ERR_FAIL_COND_V_MSG(texture->get_frame_count() >= cap, ERR_OUT_OF_MEMORY, "GIFRecorder reached max_frames.");
	const int delay_cs = MAX(1, int(Math::round((fps > 0 ? (1.0 / double(fps)) : 0.1) * 100.0)));
	texture->add_source_frame(img, delay_cs);
	return OK;
}

Ref<GIFTexture> GIFRecorder::stop() {
	_connect_process(false);
	recording = false;
	Ref<GIFTexture> result = texture;
	if (result.is_valid() && !pending_path.is_empty()) {
		result->save_to_path(pending_path);
		emit_signal(SNAME("recording_finished"), result, pending_path);
		pending_path = String();
	} else if (result.is_valid()) {
		emit_signal(SNAME("recording_finished"), result, String());
	}
	return result;
}

Error GIFRecorder::save(const String &p_path) {
	ERR_FAIL_COND_V(texture.is_null(), ERR_UNCONFIGURED);
	texture->set_dither(dither);
	texture->set_netscape_loop_count(loop_count);
	return texture->save_to_path(p_path);
}

Error GIFRecorder::record_viewport(Viewport *p_viewport, const String &p_path, double p_duration_sec, int p_fps) {
	ERR_FAIL_NULL_V(p_viewport, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_duration_sec <= 0.0, ERR_INVALID_PARAMETER);
	Ref<GIFRecorder> rec;
	rec.instantiate();
	rec->set_fps(p_fps);
	const Error err = rec->start_viewport(p_viewport);
	ERR_FAIL_COND_V(err != OK, err);
	rec->pending_path = p_path;
	const uint64_t start = OS::get_singleton()->get_ticks_msec();
	while (OS::get_singleton()->get_ticks_msec() - start < uint64_t(p_duration_sec * 1000.0)) {
		OS::get_singleton()->delay_usec(1000);
	}
	rec->stop();
	return rec->save(p_path);
}

void GIFRecorder::set_fps(int p_fps) {
	fps = MAX(1, p_fps);
	frame_interval = 1.0 / double(fps);
}

int GIFRecorder::get_fps() const {
	return fps;
}

void GIFRecorder::set_max_size(const Vector2i &p_size) {
	max_size = p_size;
}

Vector2i GIFRecorder::get_max_size() const {
	return max_size;
}

void GIFRecorder::set_loop_count(int p_count) {
	loop_count = MAX(0, p_count);
}

int GIFRecorder::get_loop_count() const {
	return loop_count;
}

void GIFRecorder::set_max_frames(int p_frames) {
	max_frames = MAX(0, p_frames);
}

int GIFRecorder::get_max_frames() const {
	return max_frames;
}

void GIFRecorder::set_dither(bool p_dither) {
	dither = p_dither;
}

bool GIFRecorder::get_dither() const {
	return dither;
}

bool GIFRecorder::is_recording() const {
	return recording;
}

void GIFRecorder::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_fps", "fps"), &GIFRecorder::set_fps);
	ClassDB::bind_method(D_METHOD("get_fps"), &GIFRecorder::get_fps);
	ClassDB::bind_method(D_METHOD("set_max_size", "size"), &GIFRecorder::set_max_size);
	ClassDB::bind_method(D_METHOD("get_max_size"), &GIFRecorder::get_max_size);
	ClassDB::bind_method(D_METHOD("set_loop_count", "count"), &GIFRecorder::set_loop_count);
	ClassDB::bind_method(D_METHOD("get_loop_count"), &GIFRecorder::get_loop_count);
	ClassDB::bind_method(D_METHOD("set_max_frames", "frames"), &GIFRecorder::set_max_frames);
	ClassDB::bind_method(D_METHOD("get_max_frames"), &GIFRecorder::get_max_frames);
	ClassDB::bind_method(D_METHOD("set_dither", "dither"), &GIFRecorder::set_dither);
	ClassDB::bind_method(D_METHOD("get_dither"), &GIFRecorder::get_dither);
	ClassDB::bind_method(D_METHOD("is_recording"), &GIFRecorder::is_recording);
	ClassDB::bind_method(D_METHOD("start_viewport", "viewport"), &GIFRecorder::start_viewport);
	ClassDB::bind_method(D_METHOD("start_window"), &GIFRecorder::start_window);
	ClassDB::bind_method(D_METHOD("start_screen", "screen"), &GIFRecorder::start_screen, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("add_frame", "image"), &GIFRecorder::add_frame);
	ClassDB::bind_method(D_METHOD("stop"), &GIFRecorder::stop);
	ClassDB::bind_method(D_METHOD("save", "path"), &GIFRecorder::save);
	ClassDB::bind_static_method("GIFRecorder", D_METHOD("record_viewport", "viewport", "path", "duration_sec", "fps"), &GIFRecorder::record_viewport, DEFVAL(12));

	ADD_PROPERTY(PropertyInfo(Variant::INT, "fps", PROPERTY_HINT_RANGE, "1,60,1"), "set_fps", "get_fps");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "max_size"), "set_max_size", "get_max_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "loop_count"), "set_loop_count", "get_loop_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_frames"), "set_max_frames", "get_max_frames");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "dither"), "set_dither", "get_dither");

	ADD_SIGNAL(MethodInfo("recording_finished", PropertyInfo(Variant::OBJECT, "animation", PROPERTY_HINT_RESOURCE_TYPE, "GIFTexture"), PropertyInfo(Variant::STRING, "path")));

	BIND_ENUM_CONSTANT(SOURCE_VIEWPORT);
	BIND_ENUM_CONSTANT(SOURCE_WINDOW);
	BIND_ENUM_CONSTANT(SOURCE_SCREEN);
}

GIFRecorder::~GIFRecorder() {
	_connect_process(false);
}
