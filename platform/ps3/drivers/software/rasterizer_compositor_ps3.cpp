/**************************************************************************/
/*  rasterizer_compositor_ps3.cpp                                         */
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

#include "rasterizer_compositor_ps3.h"

#include "display_server_ps3.h"
#include "drivers/norsx/norsx_framebuffer.h"
#include "drivers/software/canvas_blit_ps3.h"

void RasterizerCompositorPS3::_blit_image_to_framebuffer(const Ref<Image> &p_image, const Rect2i &p_dst) {
	DisplayServerPS3 *ds = DisplayServerPS3::get_singleton();
	if (!ds || p_image.is_null() || p_image->is_empty()) {
		return;
	}

	NoRSXFramebuffer *fb = ds->get_framebuffer();
	if (!fb || !fb->is_initialized()) {
		return;
	}

	CanvasBlitPS3::State state;
	state.pixels = reinterpret_cast<uint32_t *>(fb->get_buffer());
	state.width = NoRSXFramebuffer::WIDTH;
	state.height = NoRSXFramebuffer::HEIGHT;

	Rect2i dst = p_dst;
	if (dst.size.x <= 0 || dst.size.y <= 0) {
		dst = Rect2i(0, 0, NoRSXFramebuffer::WIDTH, NoRSXFramebuffer::HEIGHT);
	}

	const Rect2i src(0, 0, p_image->get_width(), p_image->get_height());
	CanvasBlitPS3::blit_image_rect(state, p_image, dst, src, Color(1, 1, 1, 1), false, false);
}

void RasterizerCompositorPS3::blit_render_targets_to_screen(int p_screen, const BlitToScreen *p_render_targets, int p_amount) {
	for (int i = 0; i < p_amount; i++) {
		const BlitToScreen &blit = p_render_targets[i];
		const Ref<Image> image = texture_storage.get_render_target_image(blit.render_target);
		if (image.is_null()) {
			continue;
		}
		_blit_image_to_framebuffer(image, blit.dst_rect);
		blitted_this_frame = true;
	}
}

void RasterizerCompositorPS3::end_frame(bool p_swap_buffers) {
	if (p_swap_buffers) {
		if (!blitted_this_frame) {
			NoRSXFramebuffer *fb = ps3_framebuffer_get_shared();
			if (fb && fb->is_initialized()) {
				fb->clear(NoRSXFramebuffer::SPLASH_COLOR);
			}
		}
		CanvasBlitPS3::flush_spu_jobs();
		DisplayServer::get_singleton()->swap_buffers();
	}
}
