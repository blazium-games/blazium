/**************************************************************************/
/*  gif_decode.h                                                          */
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

#include "core/io/image.h"
#include "core/math/vector2i.h"
#include "core/templates/vector.h"

struct GIFDecodedFrame {
	Ref<Image> image;
	Vector2i position;
	int delay_cs = 10;
	int disposal = 0;
	int transparent_color = -1;
	bool has_transparency = false;
};

struct GIFDecoded {
	Vector2i canvas_size;
	int loop_count = 0;
	Color background;
	Vector<GIFDecodedFrame> frames;
};

int gif_get_max_canvas_pixels();
int gif_get_max_frames();

Error gif_decode_buffer(const uint8_t *p_data, int p_size, GIFDecoded &r_out, bool p_first_frame_only = false);
Error gif_decode_first_frame(const uint8_t *p_data, int p_size, Ref<Image> &r_image);
