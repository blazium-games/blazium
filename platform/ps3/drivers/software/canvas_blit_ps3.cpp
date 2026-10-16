/**************************************************************************/
/*  canvas_blit_ps3.cpp                                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#include "canvas_blit_ps3.h"

#include "core/math/math_funcs.h"

#ifdef SPU_JOBS_ENABLED
extern "C" {
#include "platform/ps3/spu/spu_job_queue.h"
}
#endif

uint32_t CanvasBlitPS3::color_to_argb(const Color &p_color) {
	const uint8_t a = (uint8_t)CLAMP(int(p_color.a * 255.0f), 0, 255);
	const uint8_t r = (uint8_t)CLAMP(int(p_color.r * 255.0f), 0, 255);
	const uint8_t g = (uint8_t)CLAMP(int(p_color.g * 255.0f), 0, 255);
	const uint8_t b = (uint8_t)CLAMP(int(p_color.b * 255.0f), 0, 255);
	return (uint32_t(a) << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
}

void CanvasBlitPS3::set_clip(State &p_state, const Rect2i &p_clip) {
	p_state.clip = p_clip;
	p_state.has_clip = true;
}

void CanvasBlitPS3::clear_clip(State &p_state) {
	p_state.has_clip = false;
}

static inline bool _clip_rect(const CanvasBlitPS3::State &p_state, Rect2i &r_rect) {
	if (!p_state.has_clip) {
		return true;
	}
	if (!r_rect.intersects(p_state.clip)) {
		return false;
	}
	r_rect = r_rect.intersection(p_state.clip);
	return true;
}

static inline void _blend_pixel(uint32_t *p_dst, uint32_t p_src) {
	const uint8_t sa = (p_src >> 24) & 0xFF;
	if (sa == 0) {
		return;
	}
	if (sa == 255) {
		*p_dst = p_src;
		return;
	}

	const uint8_t da = (*p_dst >> 24) & 0xFF;
	const uint8_t sr = (p_src >> 16) & 0xFF;
	const uint8_t sg = (p_src >> 8) & 0xFF;
	const uint8_t sb = p_src & 0xFF;
	const uint8_t dr = (*p_dst >> 16) & 0xFF;
	const uint8_t dg = (*p_dst >> 8) & 0xFF;
	const uint8_t db = *p_dst & 0xFF;

	const int inv_sa = 255 - sa;
	const uint8_t a = (uint8_t)MIN(255, sa + (da * inv_sa) / 255);
	const uint8_t r = (uint8_t)((sr * sa + dr * inv_sa) / 255);
	const uint8_t g = (uint8_t)((sg * sa + dg * inv_sa) / 255);
	const uint8_t b = (uint8_t)((sb * sa + db * inv_sa) / 255);
	*p_dst = (uint32_t(a) << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
}

void CanvasBlitPS3::fill_rect(State &p_state, const Rect2i &p_rect, uint32_t p_color) {
	Rect2i rect = p_rect;
	if (!_clip_rect(p_state, rect)) {
		return;
	}

	const uint8_t a = (p_color >> 24) & 0xFF;
	for (int y = rect.position.y; y < rect.position.y + rect.size.y; y++) {
		uint32_t *row = p_state.pixels + y * p_state.width;
		for (int x = rect.position.x; x < rect.position.x + rect.size.x; x++) {
			if (a == 255) {
				row[x] = p_color;
			} else {
				_blend_pixel(&row[x], p_color);
			}
		}
	}
}

static uint32_t _sample_image(const Ref<Image> &p_image, int p_x, int p_y) {
	const Image::Format format = p_image->get_format();
	const int w = p_image->get_width();
	const int h = p_image->get_height();
	p_x = CLAMP(p_x, 0, w - 1);
	p_y = CLAMP(p_y, 0, h - 1);

	if (format == Image::FORMAT_RGBA8) {
		const uint8_t *ptr = p_image->ptr() + (p_y * w + p_x) * 4;
		return (uint32_t(ptr[3]) << 24) | (uint32_t(ptr[0]) << 16) | (uint32_t(ptr[1]) << 8) | uint32_t(ptr[2]);
	}
	if (format == Image::FORMAT_RGB8) {
		const uint8_t *ptr = p_image->ptr() + (p_y * w + p_x) * 3;
		return 0xFF000000 | (uint32_t(ptr[0]) << 16) | (uint32_t(ptr[1]) << 8) | uint32_t(ptr[2]);
	}
	if (format == Image::FORMAT_LA8) {
		const uint8_t *ptr = p_image->ptr() + (p_y * w + p_x) * 2;
		const uint8_t l = ptr[0];
		const uint8_t a = ptr[1];
		return (uint32_t(a) << 24) | (uint32_t(l) << 16) | (uint32_t(l) << 8) | uint32_t(l);
	}
	if (format == Image::FORMAT_L8) {
		const uint8_t l = p_image->ptr()[p_y * w + p_x];
		return 0xFF000000 | (uint32_t(l) << 16) | (uint32_t(l) << 8) | uint32_t(l);
	}

	const Color c = p_image->get_pixel(p_x, p_y);
	return CanvasBlitPS3::color_to_argb(c);
}

void CanvasBlitPS3::blit_image_rect(State &p_state, const Ref<Image> &p_image, const Rect2i &p_dst, const Rect2i &p_src, const Color &p_modulate, bool p_flip_h, bool p_flip_v) {
	if (p_image.is_null() || p_image->is_empty()) {
		return;
	}

	Rect2i dst = p_dst;
	if (!_clip_rect(p_state, dst)) {
		return;
	}

	const int src_w = MAX(p_src.size.x, 1);
	const int src_h = MAX(p_src.size.y, 1);
	const int dst_w = MAX(dst.size.x, 1);
	const int dst_h = MAX(dst.size.y, 1);
	const uint32_t mod_argb = color_to_argb(p_modulate);
	const uint8_t mod_a = (mod_argb >> 24) & 0xFF;

	for (int dy = 0; dy < dst_h; dy++) {
		const int y = dst.position.y + dy;
		int sy = p_src.position.y + (dy * src_h) / dst_h;
		if (p_flip_v) {
			sy = p_src.position.y + p_src.size.y - 1 - ((dy * src_h) / dst_h);
		}

		uint32_t *row = p_state.pixels + y * p_state.width;
		for (int dx = 0; dx < dst_w; dx++) {
			const int x = dst.position.x + dx;
			int sx = p_src.position.x + (dx * src_w) / dst_w;
			if (p_flip_h) {
				sx = p_src.position.x + p_src.size.x - 1 - ((dx * src_w) / dst_w);
			}

			uint32_t pixel = _sample_image(p_image, sx, sy);
			if (mod_a != 255) {
				const uint8_t pa = (pixel >> 24) & 0xFF;
				const uint8_t pr = (pixel >> 16) & 0xFF;
				const uint8_t pg = (pixel >> 8) & 0xFF;
				const uint8_t pb = pixel & 0xFF;
				const uint8_t ma = mod_a;
				const uint8_t mr = (mod_argb >> 16) & 0xFF;
				const uint8_t mg = (mod_argb >> 8) & 0xFF;
				const uint8_t mb = mod_argb & 0xFF;
				const uint8_t a = (uint8_t)((pa * ma) / 255);
				const uint8_t r = (uint8_t)((pr * mr) / 255);
				const uint8_t g = (uint8_t)((pg * mg) / 255);
				const uint8_t b = (uint8_t)((pb * mb) / 255);
				pixel = (uint32_t(a) << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
			}
			_blend_pixel(&row[x], pixel);
		}
	}
}

void CanvasBlitPS3::flush_spu_jobs() {
#ifdef SPU_JOBS_ENABLED
	spu_job_queue_flush_canvas();
#endif
}
