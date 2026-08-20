/**************************************************************************/
/*  gif_texture.cpp                                                       */
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

#include "gif_texture.h"

#include "core/object/message_queue.h"
#include "core/os/os.h"
#include "servers/rendering_server.h"

void GIFTexture::_update_proxy() {
	RWLockRead r(rw_lock);
	if (animation.is_null() || animation->get_frame_count() == 0) {
		return;
	}

	float delta = 0.0f;
	if (prev_ticks == 0) {
		prev_ticks = OS::get_singleton()->get_ticks_usec();
	} else {
		const uint64_t ticks = OS::get_singleton()->get_ticks_usec();
		delta = float(double(ticks - prev_ticks) / 1000000.0);
		prev_ticks = ticks;
	}

	const int count = animation->get_frame_count();
	const bool honor_netscape = animation->get_loop_count() != 0;
	const int max_loops = honor_netscape ? animation->get_loop_count() : 0;
	const bool can_loop = loop && (!honor_netscape || loops_done < max_loops || max_loops == 0);

	if (play && speed_scale != 0.0f) {
		time += delta * Math::abs(speed_scale);
		int guard = count;
		while (guard-- > 0) {
			const float limit = float(animation->get_frame_delay_sec(current_frame));
			if (time < limit) {
				break;
			}
			time -= limit;
			if (speed_scale > 0.0f) {
				current_frame++;
				if (current_frame >= count) {
					if (can_loop) {
						current_frame = 0;
						loops_done++;
					} else {
						current_frame = count - 1;
						time = 0;
						break;
					}
				}
			} else {
				current_frame--;
				if (current_frame < 0) {
					if (can_loop) {
						current_frame = count - 1;
						loops_done++;
					} else {
						current_frame = 0;
						time = 0;
						break;
					}
				}
			}
		}
	}

	Ref<Texture2D> tex = animation->get_active_texture(current_frame);
	if (tex.is_valid()) {
		RenderingServer::get_singleton()->texture_proxy_update(proxy, tex->get_rid());
	}
}

void GIFTexture::_finish_non_thread_safe_setup() {
	RenderingServer::get_singleton()->connect("frame_pre_draw", callable_mp(this, &GIFTexture::_update_proxy));
}

void GIFTexture::_animation_changed() {
	RWLockWrite w(rw_lock);
	current_frame = 0;
	time = 0;
	loops_done = 0;
	prev_ticks = 0;
}

void GIFTexture::set_animation(const Ref<GIFAnimation> &p_animation) {
	if (animation == p_animation) {
		return;
	}
	if (animation.is_valid()) {
		animation->disconnect_changed(callable_mp(this, &GIFTexture::_animation_changed));
	}
	RWLockWrite w(rw_lock);
	animation = p_animation;
	current_frame = 0;
	time = 0;
	loops_done = 0;
	if (animation.is_valid()) {
		animation->connect_changed(callable_mp(this, &GIFTexture::_animation_changed));
	}
	emit_changed();
}

Ref<GIFAnimation> GIFTexture::get_animation() const {
	return animation;
}

void GIFTexture::set_play(bool p_play) {
	RWLockWrite w(rw_lock);
	play = p_play;
}

bool GIFTexture::get_play() const {
	return play;
}

void GIFTexture::set_loop(bool p_loop) {
	RWLockWrite w(rw_lock);
	loop = p_loop;
}

bool GIFTexture::get_loop() const {
	return loop;
}

void GIFTexture::set_speed_scale(float p_scale) {
	ERR_FAIL_COND(p_scale < -1000.0f || p_scale >= 1000.0f);
	RWLockWrite w(rw_lock);
	speed_scale = p_scale;
}

float GIFTexture::get_speed_scale() const {
	return speed_scale;
}

void GIFTexture::set_current_frame(int p_frame) {
	RWLockWrite w(rw_lock);
	if (animation.is_valid()) {
		current_frame = animation->wrap_frame(p_frame);
	} else {
		current_frame = MAX(0, p_frame);
	}
	time = 0;
}

int GIFTexture::get_current_frame() const {
	return current_frame;
}

int GIFTexture::get_width() const {
	if (animation.is_valid() && animation->get_canvas_size().x > 0) {
		return animation->get_canvas_size().x;
	}
	return 1;
}

int GIFTexture::get_height() const {
	if (animation.is_valid() && animation->get_canvas_size().y > 0) {
		return animation->get_canvas_size().y;
	}
	return 1;
}

RID GIFTexture::get_rid() const {
	return proxy;
}

bool GIFTexture::has_alpha() const {
	return true;
}

Ref<Image> GIFTexture::get_image() const {
	if (animation.is_null()) {
		return Ref<Image>();
	}
	Ref<Texture2D> tex = const_cast<GIFAnimation *>(animation.ptr())->get_active_texture(current_frame);
	if (tex.is_valid()) {
		return tex->get_image();
	}
	return animation->get_source_image(current_frame);
}

bool GIFTexture::is_pixel_opaque(int p_x, int p_y) const {
	Ref<Image> img = get_image();
	if (img.is_null() || img->is_empty()) {
		return true;
	}
	if (p_x < 0 || p_y < 0 || p_x >= img->get_width() || p_y >= img->get_height()) {
		return false;
	}
	return img->get_pixel(p_x, p_y).a > 0.001f;
}

void GIFTexture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_animation", "animation"), &GIFTexture::set_animation);
	ClassDB::bind_method(D_METHOD("get_animation"), &GIFTexture::get_animation);
	ClassDB::bind_method(D_METHOD("set_play", "play"), &GIFTexture::set_play);
	ClassDB::bind_method(D_METHOD("get_play"), &GIFTexture::get_play);
	ClassDB::bind_method(D_METHOD("set_loop", "loop"), &GIFTexture::set_loop);
	ClassDB::bind_method(D_METHOD("get_loop"), &GIFTexture::get_loop);
	ClassDB::bind_method(D_METHOD("set_speed_scale", "scale"), &GIFTexture::set_speed_scale);
	ClassDB::bind_method(D_METHOD("get_speed_scale"), &GIFTexture::get_speed_scale);
	ClassDB::bind_method(D_METHOD("set_current_frame", "frame"), &GIFTexture::set_current_frame);
	ClassDB::bind_method(D_METHOD("get_current_frame"), &GIFTexture::get_current_frame);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "animation", PROPERTY_HINT_RESOURCE_TYPE, "GIFAnimation"), "set_animation", "get_animation");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "play"), "set_play", "get_play");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "loop"), "set_loop", "get_loop");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed_scale", PROPERTY_HINT_RANGE, "-60,60,0.1,or_less,or_greater"), "set_speed_scale", "get_speed_scale");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "current_frame"), "set_current_frame", "get_current_frame");
}

GIFTexture::GIFTexture() {
	proxy_ph = RS::get_singleton()->texture_2d_placeholder_create();
	proxy = RS::get_singleton()->texture_proxy_create(proxy_ph);
	RenderingServer::get_singleton()->texture_set_force_redraw_if_visible(proxy, true);
	MessageQueue::get_main_singleton()->push_callable(callable_mp(this, &GIFTexture::_finish_non_thread_safe_setup));
}

GIFTexture::~GIFTexture() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	if (RenderingServer::get_singleton()->is_connected("frame_pre_draw", callable_mp(this, &GIFTexture::_update_proxy))) {
		RenderingServer::get_singleton()->disconnect("frame_pre_draw", callable_mp(this, &GIFTexture::_update_proxy));
	}
	RS::get_singleton()->free(proxy);
	RS::get_singleton()->free(proxy_ph);
}
