/**************************************************************************/
/*  gif_encode.cpp                                                        */
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

#include "gif_encode.h"

#include "core/io/file_access.h"

#include <cgif.h>

#include <stdlib.h>
#include <string.h>

struct GIFWriteBuf {
	uint8_t *data = nullptr;
	size_t size = 0;
	size_t cap = 0;
};

static int _gif_write_cb(void *p_ctx, const uint8_t *p_data, const size_t p_num_bytes) {
	if (p_ctx == nullptr) {
		return -1;
	}
	if (p_num_bytes == 0) {
		return 0;
	}
	if (p_data == nullptr || p_num_bytes > 64 * 1024 * 1024) {
		return -1;
	}
	GIFWriteBuf *buf = static_cast<GIFWriteBuf *>(p_ctx);
	const size_t needed = buf->size + p_num_bytes;
	if (needed > buf->cap) {
		size_t cap = buf->cap ? buf->cap : 256;
		while (cap < needed) {
			cap *= 2;
		}
		uint8_t *next = static_cast<uint8_t *>(realloc(buf->data, cap));
		if (next == nullptr) {
			return -1;
		}
		buf->data = next;
		buf->cap = cap;
	}
	memcpy(buf->data + buf->size, p_data, p_num_bytes);
	buf->size += p_num_bytes;
	return 0;
}

static Error _image_to_rgba(const Ref<Image> &p_image, Vector<uint8_t> &r_rgba, int &r_w, int &r_h) {
	ERR_FAIL_COND_V(p_image.is_null() || p_image->is_empty(), ERR_INVALID_PARAMETER);
	Ref<Image> img = p_image->duplicate();
	if (img->is_compressed()) {
		const Error err = img->decompress();
		ERR_FAIL_COND_V_MSG(err != OK, err, "Cannot encode compressed GIF frame.");
	}
	if (img->get_format() != Image::FORMAT_RGBA8) {
		img->convert(Image::FORMAT_RGBA8);
	}
	r_w = img->get_width();
	r_h = img->get_height();
	ERR_FAIL_COND_V(r_w <= 0 || r_h <= 0 || r_w > 65535 || r_h > 65535, ERR_INVALID_PARAMETER);
	r_rgba = img->get_data();
	ERR_FAIL_COND_V(r_rgba.size() < r_w * r_h * 4, ERR_INVALID_DATA);
	// cgif's RGB path skips alpha==0 pixels. A fully transparent frame has an
	// empty histogram and writes a GIF our decoder cannot read.
	bool any_visible = false;
	uint8_t *px = r_rgba.ptrw();
	const int pixel_count = r_w * r_h;
	for (int i = 0; i < pixel_count; i++) {
		if (px[i * 4 + 3] != 0) {
			any_visible = true;
			break;
		}
	}
	if (!any_visible) {
		for (int i = 0; i < pixel_count; i++) {
			px[i * 4 + 3] = 255;
		}
	}
	return OK;
}

Error gif_encode_still(const Ref<Image> &p_image, Vector<uint8_t> &r_buffer, bool p_dither, bool p_interlaced) {
	Vector<GIFEncodeFrame> frames;
	GIFEncodeFrame f;
	f.image = p_image;
	f.delay_cs = 10;
	frames.push_back(f);
	return gif_encode_frames(frames, 0, p_dither, false, r_buffer, p_interlaced);
}

Error gif_encode_frames(const Vector<GIFEncodeFrame> &p_frames, int p_loop_count, bool p_dither, bool p_optimize, Vector<uint8_t> &r_buffer, bool p_interlaced) {
	r_buffer.clear();
	ERR_FAIL_COND_V(p_frames.is_empty(), ERR_INVALID_PARAMETER);

	Vector<Vector<uint8_t>> rgba_frames;
	rgba_frames.resize(p_frames.size());
	int width = 0;
	int height = 0;
	for (int i = 0; i < p_frames.size(); i++) {
		int w = 0;
		int h = 0;
		const Error err = _image_to_rgba(p_frames[i].image, rgba_frames.write[i], w, h);
		ERR_FAIL_COND_V(err != OK, err);
		if (i == 0) {
			width = w;
			height = h;
		} else if (w != width || h != height) {
			ERR_FAIL_V_MSG(ERR_INVALID_PARAMETER, "All GIF frames must share the same canvas size.");
		}
	}

	GIFWriteBuf out;
	CGIFrgb_Config cfg = {};
	cfg.pWriteFn = _gif_write_cb;
	cfg.pContext = &out;
	cfg.path = nullptr;
	cfg.width = uint16_t(width);
	cfg.height = uint16_t(height);
	cfg.numLoops = p_loop_count <= 0 ? CGIF_INFINITE_LOOP : uint16_t(MIN(p_loop_count, 65535));
	if (p_frames.size() > 1) {
		cfg.attrFlags |= CGIF_ATTR_IS_ANIMATED;
	}
	if (p_loop_count == 1) {
		cfg.attrFlags |= CGIF_ATTR_NO_LOOP;
	}

	CGIFrgb *gif = cgif_rgb_newgif(&cfg);
	if (gif == nullptr) {
		free(out.data);
		ERR_FAIL_V_MSG(ERR_CANT_CREATE, "Failed to start GIF encoder.");
	}

	Error err = OK;
	for (int i = 0; i < p_frames.size(); i++) {
		if (rgba_frames[i].is_empty() || rgba_frames[i].ptr() == nullptr) {
			err = ERR_INVALID_DATA;
			break;
		}
		uint8_t *pixels = static_cast<uint8_t *>(malloc(rgba_frames[i].size()));
		if (pixels == nullptr) {
			err = ERR_OUT_OF_MEMORY;
			break;
		}
		memcpy(pixels, rgba_frames[i].ptr(), rgba_frames[i].size());
		CGIFrgb_FrameConfig fc = {};
		fc.pImageData = pixels;
		fc.fmtChan = CGIF_CHAN_FMT_RGBA;
		fc.delay = uint16_t(CLAMP(p_frames[i].delay_cs, 1, 65535));
		if (!p_dither) {
			fc.attrFlags |= CGIF_RGB_FRAME_ATTR_NO_DITHERING;
		}
		if (p_interlaced) {
			fc.attrFlags |= CGIF_RGB_FRAME_ATTR_INTERLACED;
		}
		if (p_optimize) {
			fc.genFlags |= CGIF_FRAME_GEN_USE_TRANSPARENCY | CGIF_FRAME_GEN_USE_DIFF_WINDOW;
		}
		if (cgif_rgb_addframe(gif, &fc) != CGIF_OK) {
			free(pixels);
			err = ERR_CANT_CREATE;
			break;
		}
		free(pixels);
	}

	const cgif_result close_res = cgif_rgb_close(gif);
	if (err != OK || close_res != CGIF_OK || out.size == 0 || out.data == nullptr) {
		free(out.data);
		r_buffer.clear();
		return err != OK ? err : ERR_CANT_CREATE;
	}

	r_buffer.resize(int(out.size));
	memcpy(r_buffer.ptrw(), out.data, out.size);
	free(out.data);
	return OK;
}

Error gif_encode_write_file(const String &p_path, const Vector<uint8_t> &p_buffer) {
	ERR_FAIL_COND_V(p_buffer.is_empty(), ERR_INVALID_DATA);
	Error err = OK;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err != OK || f.is_null(), err == OK ? ERR_CANT_CREATE : err, vformat("Can't save GIF at path: '%s'.", p_path));
	f->store_buffer(p_buffer.ptr(), p_buffer.size());
	if (f->get_error() != OK && f->get_error() != ERR_FILE_EOF) {
		return ERR_CANT_CREATE;
	}
	return OK;
}
