/**************************************************************************/
/*  gif_sprite_2d.cpp                                                     */
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

#include "gif_sprite_2d.h"

void GIFSprite2D::_advance(double p_delta) {
	if (!playing || animation.is_null() || animation->get_frame_count() == 0 || speed_scale == 0.0f) {
		return;
	}
	const int count = animation->get_frame_count();
	const bool honor_netscape = animation->get_loop_count() != 0;
	const int max_loops = honor_netscape ? animation->get_loop_count() : 0;
	const bool can_loop = loop && (!honor_netscape || loops_done < max_loops || max_loops == 0);

	time += float(p_delta) * Math::abs(speed_scale);
	int guard = count;
	bool changed = false;
	while (guard-- > 0) {
		const float limit = float(animation->get_frame_delay_sec(current_frame));
		if (time < limit) {
			break;
		}
		time -= limit;
		changed = true;
		if (speed_scale > 0.0f) {
			current_frame++;
			if (current_frame >= count) {
				if (can_loop) {
					current_frame = 0;
					loops_done++;
				} else {
					current_frame = count - 1;
					time = 0;
					playing = false;
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
					playing = false;
					break;
				}
			}
		}
	}
	if (changed) {
		queue_redraw();
	}
}

void GIFSprite2D::_animation_changed() {
	current_frame = 0;
	time = 0;
	loops_done = 0;
	queue_redraw();
	item_rect_changed();
}

void GIFSprite2D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_INTERNAL_PROCESS: {
			_advance(get_process_delta_time());
		} break;
		case NOTIFICATION_DRAW: {
			if (animation.is_null()) {
				return;
			}
			Ref<Texture2D> tex = animation->get_active_texture(current_frame);
			if (tex.is_null()) {
				return;
			}
			Size2 size = tex->get_size();
			if (animation->get_display_mode() == GIFAnimation::DISPLAY_SOURCE) {
				const Vector2i pos = animation->get_frame_position(current_frame);
				size = animation->get_canvas_size();
				Point2 draw_pos = offset + Vector2(pos);
				if (centered) {
					draw_pos -= size / 2;
				}
				tex->draw(get_canvas_item(), draw_pos);
			} else {
				Point2 draw_pos = offset;
				if (centered) {
					draw_pos -= size / 2;
				}
				tex->draw(get_canvas_item(), draw_pos);
			}
		} break;
	}
}

void GIFSprite2D::set_animation(const Ref<GIFAnimation> &p_animation) {
	if (animation == p_animation) {
		return;
	}
	if (animation.is_valid()) {
		animation->disconnect_changed(callable_mp(this, &GIFSprite2D::_animation_changed));
	}
	animation = p_animation;
	if (animation.is_valid()) {
		animation->connect_changed(callable_mp(this, &GIFSprite2D::_animation_changed));
	}
	_animation_changed();
}

Ref<GIFAnimation> GIFSprite2D::get_animation() const {
	return animation;
}

void GIFSprite2D::set_playing(bool p_playing) {
	playing = p_playing;
}

bool GIFSprite2D::is_playing() const {
	return playing;
}

void GIFSprite2D::play() {
	playing = true;
}

void GIFSprite2D::pause() {
	playing = false;
}

void GIFSprite2D::stop() {
	playing = false;
	current_frame = 0;
	time = 0;
	loops_done = 0;
	queue_redraw();
}

void GIFSprite2D::set_loop(bool p_loop) {
	loop = p_loop;
}

bool GIFSprite2D::get_loop() const {
	return loop;
}

void GIFSprite2D::set_speed_scale(float p_scale) {
	speed_scale = p_scale;
}

float GIFSprite2D::get_speed_scale() const {
	return speed_scale;
}

void GIFSprite2D::set_frame(int p_frame) {
	if (animation.is_valid()) {
		current_frame = animation->wrap_frame(p_frame);
	} else {
		current_frame = MAX(0, p_frame);
	}
	time = 0;
	queue_redraw();
}

int GIFSprite2D::get_frame() const {
	return current_frame;
}

void GIFSprite2D::set_centered(bool p_centered) {
	if (centered == p_centered) {
		return;
	}
	centered = p_centered;
	queue_redraw();
	item_rect_changed();
}

bool GIFSprite2D::is_centered() const {
	return centered;
}

void GIFSprite2D::set_offset(const Point2 &p_offset) {
	if (offset == p_offset) {
		return;
	}
	offset = p_offset;
	queue_redraw();
	item_rect_changed();
}

Point2 GIFSprite2D::get_offset() const {
	return offset;
}

Rect2 GIFSprite2D::_get_rect() const {
	Size2 size = animation.is_valid() ? Size2(animation->get_canvas_size()) : Size2(1, 1);
	Point2 pos = offset;
	if (centered) {
		pos -= size / 2;
	}
	return Rect2(pos, size);
}

#ifdef DEBUG_ENABLED
Rect2 GIFSprite2D::_edit_get_rect() const {
	return _get_rect();
}

bool GIFSprite2D::_edit_use_rect() const {
	return true;
}
#endif

Rect2 GIFSprite2D::get_anchorable_rect() const {
	return _get_rect();
}

void GIFSprite2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_animation", "animation"), &GIFSprite2D::set_animation);
	ClassDB::bind_method(D_METHOD("get_animation"), &GIFSprite2D::get_animation);
	ClassDB::bind_method(D_METHOD("set_playing", "playing"), &GIFSprite2D::set_playing);
	ClassDB::bind_method(D_METHOD("is_playing"), &GIFSprite2D::is_playing);
	ClassDB::bind_method(D_METHOD("play"), &GIFSprite2D::play);
	ClassDB::bind_method(D_METHOD("pause"), &GIFSprite2D::pause);
	ClassDB::bind_method(D_METHOD("stop"), &GIFSprite2D::stop);
	ClassDB::bind_method(D_METHOD("set_loop", "loop"), &GIFSprite2D::set_loop);
	ClassDB::bind_method(D_METHOD("get_loop"), &GIFSprite2D::get_loop);
	ClassDB::bind_method(D_METHOD("set_speed_scale", "scale"), &GIFSprite2D::set_speed_scale);
	ClassDB::bind_method(D_METHOD("get_speed_scale"), &GIFSprite2D::get_speed_scale);
	ClassDB::bind_method(D_METHOD("set_frame", "frame"), &GIFSprite2D::set_frame);
	ClassDB::bind_method(D_METHOD("get_frame"), &GIFSprite2D::get_frame);
	ClassDB::bind_method(D_METHOD("set_centered", "centered"), &GIFSprite2D::set_centered);
	ClassDB::bind_method(D_METHOD("is_centered"), &GIFSprite2D::is_centered);
	ClassDB::bind_method(D_METHOD("set_offset", "offset"), &GIFSprite2D::set_offset);
	ClassDB::bind_method(D_METHOD("get_offset"), &GIFSprite2D::get_offset);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "animation", PROPERTY_HINT_RESOURCE_TYPE, "GIFAnimation"), "set_animation", "get_animation");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "playing"), "set_playing", "is_playing");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "loop"), "set_loop", "get_loop");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed_scale", PROPERTY_HINT_RANGE, "-60,60,0.1,or_less,or_greater"), "set_speed_scale", "get_speed_scale");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "frame"), "set_frame", "get_frame");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "centered"), "set_centered", "is_centered");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "offset", PROPERTY_HINT_NONE, "suffix:px"), "set_offset", "get_offset");
}

GIFSprite2D::GIFSprite2D() {
	set_process_internal(true);
}
