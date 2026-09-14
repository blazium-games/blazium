/**************************************************************************/
/*  warcry_opus.h                                                         */
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

#include "core/templates/vector.h"

struct OpusEncoder;
struct OpusDecoder;

class WarcryOpusCodec {
public:
	static constexpr int SAMPLE_RATE = 48000;
	static constexpr int CHANNELS = 1;
	static constexpr int FRAME_MS = 20;
	static constexpr int FRAME_SAMPLES = 960;
	static constexpr int BITRATE = 64000;

	WarcryOpusCodec();
	~WarcryOpusCodec();

	bool init();
	void close();
	bool encode_frame(const int16_t *p_pcm, int p_samples, Vector<uint8_t> &r_out);
	bool decode_frame(const uint8_t *p_data, int p_size, Vector<int16_t> &r_out);

private:
	OpusEncoder *encoder = nullptr;
	OpusDecoder *decoder = nullptr;
};
