/**************************************************************************/
/*  canvas_blit_ps3.h                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#pragma once

#include "core/io/image.h"
#include "core/math/color.h"
#include "core/math/rect2.h"
#include "core/math/rect2i.h"

class CanvasBlitPS3 {
public:
	struct State {
		uint32_t *pixels = nullptr;
		int width = 0;
		int height = 0;
		Rect2i clip;
		bool has_clip = false;
	};

	static uint32_t color_to_argb(const Color &p_color);
	static void set_clip(State &p_state, const Rect2i &p_clip);
	static void clear_clip(State &p_state);
	static void fill_rect(State &p_state, const Rect2i &p_rect, uint32_t p_color);
	static void blit_image_rect(State &p_state, const Ref<Image> &p_image, const Rect2i &p_dst, const Rect2i &p_src, const Color &p_modulate, bool p_flip_h, bool p_flip_v);
	static void flush_spu_jobs();
};
