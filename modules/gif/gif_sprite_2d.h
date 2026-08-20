/**************************************************************************/
/*  gif_sprite_2d.h                                                       */
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

#include "gif_animation.h"
#include "scene/2d/node_2d.h"

class GIFSprite2D : public Node2D {
	GDCLASS(GIFSprite2D, Node2D);

	Ref<GIFAnimation> animation;
	bool playing = true;
	bool loop = true;
	float speed_scale = 1.0;
	int current_frame = 0;
	float time = 0.0;
	int loops_done = 0;
	bool centered = true;
	Point2 offset;

	void _advance(double p_delta);
	void _animation_changed();
	Rect2 _get_rect() const;

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_animation(const Ref<GIFAnimation> &p_animation);
	Ref<GIFAnimation> get_animation() const;
	void set_playing(bool p_playing);
	bool is_playing() const;
	void play();
	void pause();
	void stop();
	void set_loop(bool p_loop);
	bool get_loop() const;
	void set_speed_scale(float p_scale);
	float get_speed_scale() const;
	void set_frame(int p_frame);
	int get_frame() const;
	void set_centered(bool p_centered);
	bool is_centered() const;
	void set_offset(const Point2 &p_offset);
	Point2 get_offset() const;

#ifdef DEBUG_ENABLED
	virtual Rect2 _edit_get_rect() const override;
	virtual bool _edit_use_rect() const override;
#endif
	virtual Rect2 get_anchorable_rect() const override;

	GIFSprite2D();
};
