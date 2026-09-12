/**************************************************************************/
/*  gif_decode.cpp                                                        */
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

#include "gif_decode.h"

#include "core/config/project_settings.h"
#include "core/error/error_macros.h"

#include <string.h>

int gif_get_max_canvas_pixels() {
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/gif/max_canvas_pixels")) {
		return MAX(1, (int)GLOBAL_GET("blazium/gif/max_canvas_pixels"));
	}
	return 16777216;
}

int gif_get_max_frames() {
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/gif/max_frames")) {
		return MAX(1, (int)GLOBAL_GET("blazium/gif/max_frames"));
	}
	return 4096;
}

class GIFReader {
	const uint8_t *data = nullptr;
	int size = 0;
	int pos = 0;

public:
	GIFReader(const uint8_t *p_data, int p_size) :
			data(p_data), size(p_size) {}

	bool eof() const { return pos >= size; }
	int remaining() const { return MAX(0, size - pos); }

	Error read_bytes(uint8_t *p_dst, int p_count) {
		if (p_count < 0 || remaining() < p_count) {
			return ERR_FILE_CORRUPT;
		}
		memcpy(p_dst, data + pos, p_count);
		pos += p_count;
		return OK;
	}

	Error read_u8(uint8_t &r_v) {
		if (remaining() < 1) {
			return ERR_FILE_CORRUPT;
		}
		r_v = data[pos++];
		return OK;
	}

	Error read_u16(uint16_t &r_v) {
		if (remaining() < 2) {
			return ERR_FILE_CORRUPT;
		}
		r_v = uint16_t(data[pos]) | (uint16_t(data[pos + 1]) << 8);
		pos += 2;
		return OK;
	}

	Error skip(int p_count) {
		if (p_count < 0 || remaining() < p_count) {
			return ERR_FILE_CORRUPT;
		}
		pos += p_count;
		return OK;
	}

	Error skip_sub_blocks() {
		uint8_t block_size = 0;
		while (true) {
			if (read_u8(block_size) != OK) {
				return ERR_FILE_CORRUPT;
			}
			if (block_size == 0) {
				return OK;
			}
			if (skip(block_size) != OK) {
				return ERR_FILE_CORRUPT;
			}
		}
	}

	Error read_sub_blocks(Vector<uint8_t> &r_out) {
		r_out.clear();
		uint8_t block_size = 0;
		while (true) {
			if (read_u8(block_size) != OK) {
				return ERR_FILE_CORRUPT;
			}
			if (block_size == 0) {
				return OK;
			}
			const int old = r_out.size();
			r_out.resize(old + block_size);
			if (read_bytes(r_out.ptrw() + old, block_size) != OK) {
				return ERR_FILE_CORRUPT;
			}
		}
	}
};

static Error _gif_lzw_decode(const Vector<uint8_t> &p_compressed, int p_min_code_size, int p_pixel_count, Vector<uint8_t> &r_indices) {
	if (p_min_code_size < 2 || p_min_code_size > 8 || p_pixel_count <= 0) {
		return ERR_FILE_CORRUPT;
	}

	const int clear_code = 1 << p_min_code_size;
	const int eoi_code = clear_code + 1;
	const int src_len = p_compressed.size();
	const uint8_t *src = p_compressed.ptr();

	r_indices.resize(p_pixel_count);
	uint8_t *out = r_indices.ptrw();
	int out_pos = 0;

	int16_t prefix[4096];
	uint8_t suffix[4096];
	uint8_t stack[4096];

	auto reset_dict = [&](int &r_next_code, int &r_code_size) {
		for (int i = 0; i < clear_code; i++) {
			prefix[i] = -1;
			suffix[i] = uint8_t(i);
		}
		r_next_code = eoi_code + 1;
		r_code_size = p_min_code_size + 1;
	};

	int next_code = 0;
	int code_size = 0;
	reset_dict(next_code, code_size);

	uint32_t bit_buf = 0;
	int bit_count = 0;
	int src_pos = 0;

	auto read_code = [&]() -> int {
		while (bit_count < code_size) {
			if (src_pos >= src_len) {
				return -1;
			}
			bit_buf |= uint32_t(src[src_pos++]) << bit_count;
			bit_count += 8;
		}
		const int code = int(bit_buf & ((1u << code_size) - 1u));
		bit_buf >>= code_size;
		bit_count -= code_size;
		return code;
	};

	int prev = -1;
	while (out_pos < p_pixel_count) {
		const int code = read_code();
		if (code < 0) {
			return ERR_FILE_CORRUPT;
		}
		if (code == clear_code) {
			reset_dict(next_code, code_size);
			prev = -1;
			continue;
		}
		if (code == eoi_code) {
			break;
		}

		int c = code;
		int stack_len = 0;
		if (c > next_code) {
			return ERR_FILE_CORRUPT;
		}
		if (c == next_code) {
			if (prev < 0) {
				return ERR_FILE_CORRUPT;
			}
			c = prev;
			int walk = prev;
			while (walk >= clear_code) {
				walk = prefix[walk];
				if (walk < 0 || stack_len >= 4095) {
					return ERR_FILE_CORRUPT;
				}
			}
			stack[stack_len++] = suffix[walk];
		}

		while (c >= clear_code) {
			if (c >= 4096 || stack_len >= 4095) {
				return ERR_FILE_CORRUPT;
			}
			stack[stack_len++] = suffix[c];
			c = prefix[c];
			if (c < 0) {
				return ERR_FILE_CORRUPT;
			}
		}
		stack[stack_len++] = suffix[c];

		for (int i = stack_len - 1; i >= 0; i--) {
			if (out_pos >= p_pixel_count) {
				return ERR_FILE_CORRUPT;
			}
			out[out_pos++] = stack[i];
		}

		if (prev >= 0 && next_code < 4096) {
			prefix[next_code] = int16_t(prev);
			suffix[next_code] = suffix[c];
			next_code++;
			if (next_code == (1 << code_size) && code_size < 12) {
				code_size++;
			}
		}
		prev = code;
	}

	if (out_pos < p_pixel_count) {
		return ERR_FILE_CORRUPT;
	}
	return OK;
}

static void _gif_deinterlace(Vector<uint8_t> &p_indices, int p_width, int p_height) {
	Vector<uint8_t> src = p_indices;
	const uint8_t *s = src.ptr();
	uint8_t *d = p_indices.ptrw();
	int src_y = 0;
	const int starts[] = { 0, 4, 2, 1 };
	const int steps[] = { 8, 8, 4, 2 };
	for (int pass = 0; pass < 4; pass++) {
		for (int y = starts[pass]; y < p_height; y += steps[pass]) {
			memcpy(d + y * p_width, s + src_y * p_width, p_width);
			src_y++;
		}
	}
}

static Error _gif_indices_to_image(const Vector<uint8_t> &p_indices, int p_width, int p_height, const Vector<uint8_t> &p_palette, int p_trans_index, Ref<Image> &r_image) {
	Vector<uint8_t> rgba;
	rgba.resize(p_width * p_height * 4);
	uint8_t *dst = rgba.ptrw();
	const uint8_t *idx = p_indices.ptr();
	const uint8_t *pal = p_palette.ptr();
	const int pal_colors = p_palette.size() / 3;
	for (int i = 0; i < p_width * p_height; i++) {
		const int color = idx[i];
		if (color < 0 || color >= pal_colors) {
			dst[i * 4 + 0] = 0;
			dst[i * 4 + 1] = 0;
			dst[i * 4 + 2] = 0;
			dst[i * 4 + 3] = 0;
			continue;
		}
		dst[i * 4 + 0] = pal[color * 3 + 0];
		dst[i * 4 + 1] = pal[color * 3 + 1];
		dst[i * 4 + 2] = pal[color * 3 + 2];
		dst[i * 4 + 3] = (color == p_trans_index) ? 0 : 255;
	}
	r_image = Image::create_from_data(p_width, p_height, false, Image::FORMAT_RGBA8, rgba);
	return r_image.is_valid() ? OK : ERR_INVALID_DATA;
}

Error gif_decode_buffer(const uint8_t *p_data, int p_size, GIFDecoded &r_out, bool p_first_frame_only) {
	r_out = GIFDecoded();
	ERR_FAIL_COND_V(p_data == nullptr || p_size < 13, ERR_FILE_CORRUPT);

	GIFReader r(p_data, p_size);
	uint8_t sig[6];
	if (r.read_bytes(sig, 6) != OK) {
		return ERR_FILE_CORRUPT;
	}
	const bool gif87 = memcmp(sig, "GIF87a", 6) == 0;
	const bool gif89 = memcmp(sig, "GIF89a", 6) == 0;
	if (!gif87 && !gif89) {
		return ERR_FILE_CORRUPT;
	}

	uint16_t width = 0;
	uint16_t height = 0;
	uint8_t packed = 0;
	uint8_t bg_index = 0;
	uint8_t aspect = 0;
	if (r.read_u16(width) != OK || r.read_u16(height) != OK || r.read_u8(packed) != OK || r.read_u8(bg_index) != OK || r.read_u8(aspect) != OK) {
		return ERR_FILE_CORRUPT;
	}
	if (width == 0 || height == 0) {
		return ERR_FILE_CORRUPT;
	}
	const int64_t canvas_pixels = int64_t(width) * int64_t(height);
	if (canvas_pixels > gif_get_max_canvas_pixels()) {
		ERR_FAIL_V_MSG(ERR_OUT_OF_MEMORY, vformat("GIF canvas %dx%d exceeds blazium/gif/max_canvas_pixels (%d).", width, height, gif_get_max_canvas_pixels()));
	}

	r_out.canvas_size = Vector2i(width, height);

	Vector<uint8_t> global_palette;
	if (packed & 0x80) {
		const int gct_size = 3 * (1 << ((packed & 0x07) + 1));
		global_palette.resize(gct_size);
		if (r.read_bytes(global_palette.ptrw(), gct_size) != OK) {
			return ERR_FILE_CORRUPT;
		}
		if (bg_index < gct_size / 3) {
			r_out.background = Color(global_palette[bg_index * 3] / 255.0f, global_palette[bg_index * 3 + 1] / 255.0f, global_palette[bg_index * 3 + 2] / 255.0f, 1);
		}
	}

	int gce_delay = 10;
	int gce_disposal = 0;
	int gce_trans = -1;
	bool gce_has_trans = false;
	bool have_gce = false;
	const int max_frames = gif_get_max_frames();

	while (!r.eof()) {
		uint8_t introducer = 0;
		if (r.read_u8(introducer) != OK) {
			return ERR_FILE_CORRUPT;
		}
		if (introducer == 0x3B) {
			break;
		}
		if (introducer == 0x21) {
			uint8_t label = 0;
			if (r.read_u8(label) != OK) {
				return ERR_FILE_CORRUPT;
			}
			if (label == 0xF9) {
				uint8_t block_size = 0;
				if (r.read_u8(block_size) != OK || block_size != 4) {
					return ERR_FILE_CORRUPT;
				}
				uint8_t gce_packed = 0;
				uint16_t delay = 0;
				uint8_t trans = 0;
				uint8_t term = 0;
				if (r.read_u8(gce_packed) != OK || r.read_u16(delay) != OK || r.read_u8(trans) != OK || r.read_u8(term) != OK) {
					return ERR_FILE_CORRUPT;
				}
				gce_delay = delay == 0 ? 10 : int(delay);
				gce_disposal = (gce_packed >> 2) & 0x07;
				if (gce_disposal > 3) {
					gce_disposal = 0;
				}
				gce_has_trans = (gce_packed & 0x01) != 0;
				gce_trans = gce_has_trans ? int(trans) : -1;
				have_gce = true;
			} else if (label == 0xFF) {
				uint8_t block_size = 0;
				if (r.read_u8(block_size) != OK) {
					return ERR_FILE_CORRUPT;
				}
				Vector<uint8_t> app_id;
				app_id.resize(block_size);
				if (block_size > 0 && r.read_bytes(app_id.ptrw(), block_size) != OK) {
					return ERR_FILE_CORRUPT;
				}
				Vector<uint8_t> app_data;
				if (r.read_sub_blocks(app_data) != OK) {
					return ERR_FILE_CORRUPT;
				}
				const bool netscape = block_size >= 11 && memcmp(app_id.ptr(), "NETSCAPE2.0", 11) == 0;
				const bool animexts = block_size >= 11 && memcmp(app_id.ptr(), "ANIMEXTS1.0", 11) == 0;
				if ((netscape || animexts) && app_data.size() >= 3 && app_data[0] == 0x01) {
					r_out.loop_count = int(app_data[1]) | (int(app_data[2]) << 8);
				}
			} else {
				if (r.skip_sub_blocks() != OK) {
					return ERR_FILE_CORRUPT;
				}
			}
			continue;
		}
		if (introducer != 0x2C) {
			return ERR_FILE_CORRUPT;
		}

		if (r_out.frames.size() >= max_frames) {
			ERR_FAIL_V_MSG(ERR_OUT_OF_MEMORY, vformat("GIF has more than blazium/gif/max_frames (%d) frames.", max_frames));
		}

		uint16_t left = 0, top = 0, fw = 0, fh = 0;
		uint8_t img_packed = 0;
		if (r.read_u16(left) != OK || r.read_u16(top) != OK || r.read_u16(fw) != OK || r.read_u16(fh) != OK || r.read_u8(img_packed) != OK) {
			return ERR_FILE_CORRUPT;
		}
		if (fw == 0 || fh == 0) {
			return ERR_FILE_CORRUPT;
		}
		const int64_t frame_pixels = int64_t(fw) * int64_t(fh);
		if (frame_pixels > gif_get_max_canvas_pixels()) {
			ERR_FAIL_V_MSG(ERR_OUT_OF_MEMORY, vformat("GIF frame %dx%d exceeds blazium/gif/max_canvas_pixels (%d).", fw, fh, gif_get_max_canvas_pixels()));
		}

		Vector<uint8_t> local_palette;
		if (img_packed & 0x80) {
			const int lct_size = 3 * (1 << ((img_packed & 0x07) + 1));
			local_palette.resize(lct_size);
			if (r.read_bytes(local_palette.ptrw(), lct_size) != OK) {
				return ERR_FILE_CORRUPT;
			}
		}

		uint8_t min_code_size = 0;
		if (r.read_u8(min_code_size) != OK) {
			return ERR_FILE_CORRUPT;
		}
		Vector<uint8_t> compressed;
		if (r.read_sub_blocks(compressed) != OK) {
			return ERR_FILE_CORRUPT;
		}

		Vector<uint8_t> indices;
		if (_gif_lzw_decode(compressed, min_code_size, fw * fh, indices) != OK) {
			return ERR_FILE_CORRUPT;
		}
		if (img_packed & 0x40) {
			_gif_deinterlace(indices, fw, fh);
		}

		const Vector<uint8_t> &palette = local_palette.size() ? local_palette : global_palette;
		if (palette.is_empty()) {
			return ERR_FILE_CORRUPT;
		}

		GIFDecodedFrame frame;
		frame.position = Vector2i(left, top);
		frame.delay_cs = have_gce ? gce_delay : 10;
		frame.disposal = have_gce ? gce_disposal : 0;
		frame.transparent_color = have_gce ? gce_trans : -1;
		frame.has_transparency = have_gce && gce_has_trans;
		if (_gif_indices_to_image(indices, fw, fh, palette, frame.transparent_color, frame.image) != OK) {
			return ERR_FILE_CORRUPT;
		}
		r_out.frames.push_back(frame);

		have_gce = false;
		gce_delay = 10;
		gce_disposal = 0;
		gce_trans = -1;
		gce_has_trans = false;

		if (p_first_frame_only) {
			return OK;
		}
	}

	if (r_out.frames.is_empty()) {
		return ERR_FILE_CORRUPT;
	}
	return OK;
}

Error gif_decode_first_frame(const uint8_t *p_data, int p_size, Ref<Image> &r_image) {
	GIFDecoded decoded;
	const Error err = gif_decode_buffer(p_data, p_size, decoded, true);
	if (err != OK || decoded.frames.is_empty() || decoded.frames[0].image.is_null()) {
		return err == OK ? ERR_FILE_CORRUPT : err;
	}
	r_image = decoded.frames[0].image;
	return OK;
}
