/**************************************************************************/
/*  str_writer.cpp                                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/

#include "str_writer.h"

#include "core/math/math_funcs.h"

#include <cmath>

namespace ps1_cook {

static const int kZigzag[64] = {
	0, 1, 8, 16, 9, 2, 3, 10,
	17, 24, 32, 25, 18, 11, 4, 5,
	12, 19, 26, 33, 40, 48, 41, 34,
	27, 20, 13, 6, 7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36,
	29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46,
	53, 60, 61, 54, 47, 55, 62, 63
};

static void append_u16(Vector<uint8_t> &o, uint16_t v) {
	o.push_back(uint8_t(v & 0xff));
	o.push_back(uint8_t(v >> 8));
}

static void dct8(const float *src, int *dst) {
	float tmp[64];
	for (int y = 0; y < 8; y++) {
		for (int u = 0; u < 8; u++) {
			float s = 0.0f;
			for (int x = 0; x < 8; x++) {
				s += src[y * 8 + x] * Math::cos((2.0f * x + 1.0f) * u * 3.14159265f / 16.0f);
			}
			tmp[y * 8 + u] = s * (u == 0 ? 0.35355339f : 0.5f);
		}
	}
	for (int u = 0; u < 8; u++) {
		for (int v = 0; v < 8; v++) {
			float s = 0.0f;
			for (int y = 0; y < 8; y++) {
				s += tmp[y * 8 + u] * Math::cos((2.0f * y + 1.0f) * v * 3.14159265f / 16.0f);
			}
			dst[v * 8 + u] = int(s * (v == 0 ? 0.35355339f : 0.5f) / 8.0f);
		}
	}
}

static uint16_t clip10(int v) {
	if (v > 511) {
		v = 511;
	}
	if (v < -512) {
		v = -512;
	}
	return uint16_t(v & 0x3ff);
}

static void encode_block(const float *pix, Vector<uint16_t> &rle) {
	int coef[64];
	dct8(pix, coef);
	int run = 0;
	for (int i = 0; i < 64; i++) {
		const int z = coef[kZigzag[i]];
		if (z == 0 && i > 0) {
			run++;
			continue;
		}
		if (i == 0) {
			rle.push_back(clip10(z));
		} else {
			rle.push_back(uint16_t((run << 10) | clip10(z)));
			run = 0;
		}
	}
	rle.push_back(0xfe00);
}

static void ycbcr(const Color &c, float &y, float &cb, float &cr) {
	const float r = c.r * 255.0f;
	const float g = c.g * 255.0f;
	const float b = c.b * 255.0f;
	y = 0.299f * r + 0.587f * g + 0.114f * b;
	cb = -0.1687f * r - 0.3313f * g + 0.5f * b;
	cr = 0.5f * r - 0.4187f * g - 0.0813f * b;
}

Error write_str_frames(const Vector<Ref<Image>> &p_frames, StrBlob &r_out) {
	if (p_frames.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	const int fw = 160;
	const int fh = 128;
	Vector<uint8_t> out;
	for (int f = 0; f < p_frames.size(); f++) {
		Ref<Image> img = p_frames[f];
		if (img.is_null() || img->is_empty()) {
			return ERR_INVALID_PARAMETER;
		}
		img = img->duplicate();
		if (img->get_width() != fw || img->get_height() != fh) {
			img->resize(fw, fh, Image::INTERPOLATE_NEAREST);
		}
		if (img->get_format() != Image::FORMAT_RGBA8) {
			img->convert(Image::FORMAT_RGBA8);
		}
		Vector<uint16_t> rle;
		for (int by = 0; by < fh; by += 16) {
			for (int bx = 0; bx < fw; bx += 16) {
				float y0[64], y1[64], y2[64], y3[64], cb[64], cr[64];
				for (int i = 0; i < 64; i++) {
					cb[i] = 0;
					cr[i] = 0;
				}
				auto fill_y = [&](float *dst, int ox, int oy) {
					for (int yy = 0; yy < 8; yy++) {
						for (int xx = 0; xx < 8; xx++) {
							float Y, Cb, Cr;
							ycbcr(img->get_pixel(CLAMP(bx + ox + xx, 0, fw - 1), CLAMP(by + oy + yy, 0, fh - 1)), Y, Cb, Cr);
							dst[yy * 8 + xx] = Y - 128.0f;
							const int cx = (ox / 2 + xx / 2);
							const int cy = (oy / 2 + yy / 2);
							cb[cy * 8 + cx] += Cb * 0.25f;
							cr[cy * 8 + cx] += Cr * 0.25f;
						}
					}
				};
				fill_y(y0, 0, 0);
				fill_y(y1, 8, 0);
				fill_y(y2, 0, 8);
				fill_y(y3, 8, 8);
				encode_block(cr, rle);
				encode_block(cb, rle);
				encode_block(y0, rle);
				encode_block(y1, rle);
				encode_block(y2, rle);
				encode_block(y3, rle);
			}
		}
		if (rle.size() & 1) {
			rle.push_back(0);
		}
		Vector<uint8_t> payload;
		const uint32_t data_words = uint32_t(rle.size() / 2);
		payload.push_back(uint8_t(data_words & 0xff));
		payload.push_back(uint8_t((data_words >> 8) & 0xff));
		payload.push_back(uint8_t((data_words >> 16) & 0xff));
		payload.push_back(uint8_t((data_words >> 24) & 0xff));
		for (int i = 0; i < rle.size(); i++) {
			append_u16(payload, rle[i]);
		}
		const int data_per = 2032;
		const int chunks = MAX(1, (payload.size() + data_per - 1) / data_per);
		int off = 0;
		for (int c = 0; c < chunks; c++) {
			append_u16(out, 0x0160);
			append_u16(out, uint16_t(c));
			append_u16(out, uint16_t(chunks));
			append_u16(out, uint16_t(f));
			append_u16(out, uint16_t(fw));
			append_u16(out, uint16_t(fh));
			append_u16(out, 0);
			append_u16(out, 0);
			const int n = MIN(data_per, payload.size() - off);
			for (int i = 0; i < n; i++) {
				out.push_back(payload[off + i]);
			}
			for (int i = n; i < data_per; i++) {
				out.push_back(0);
			}
			off += n;
		}
	}
	r_out.bytes = out;
	r_out.frames = p_frames.size();
	r_out.width = fw;
	r_out.height = fh;
	return OK;
}

} // namespace ps1_cook
