/**************************************************************************/
/*  gif_texture.h                                                         */
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
#include "scene/resources/texture.h"

class GIFTexture : public Texture2D {
	GDCLASS(GIFTexture, Texture2D);

	Ref<GIFAnimation> animation;
	bool play = true;
	bool loop = true;
	float speed_scale = 1.0;
	int current_frame = 0;
	float time = 0.0;
	uint64_t prev_ticks = 0;
	int loops_done = 0;

	RID proxy_ph;
	RID proxy;
	RWLock rw_lock;

	void _update_proxy();
	void _finish_non_thread_safe_setup();
	void _animation_changed();

protected:
	static void _bind_methods();

public:
	void set_animation(const Ref<GIFAnimation> &p_animation);
	Ref<GIFAnimation> get_animation() const;
	void set_play(bool p_play);
	bool get_play() const;
	void set_loop(bool p_loop);
	bool get_loop() const;
	void set_speed_scale(float p_scale);
	float get_speed_scale() const;
	void set_current_frame(int p_frame);
	int get_current_frame() const;

	virtual int get_width() const override;
	virtual int get_height() const override;
	virtual RID get_rid() const override;
	virtual bool has_alpha() const override;
	virtual Ref<Image> get_image() const override;
	bool is_pixel_opaque(int p_x, int p_y) const override;

	GIFTexture();
	~GIFTexture();
};
