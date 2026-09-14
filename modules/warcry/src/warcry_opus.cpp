/**************************************************************************/
/*  warcry_opus.cpp                                                       */
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

#include "warcry_opus.h"

#include <opus.h>

#include <cstring>

WarcryOpusCodec::WarcryOpusCodec() {}

WarcryOpusCodec::~WarcryOpusCodec() {
	close();
}

bool WarcryOpusCodec::init() {
	close();
	int err = OPUS_OK;
	encoder = opus_encoder_create(SAMPLE_RATE, CHANNELS, OPUS_APPLICATION_VOIP, &err);
	if (!encoder || err != OPUS_OK) {
		close();
		return false;
	}
	opus_encoder_ctl(encoder, OPUS_SET_BITRATE(BITRATE));
	opus_encoder_ctl(encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));

	decoder = opus_decoder_create(SAMPLE_RATE, CHANNELS, &err);
	if (!decoder || err != OPUS_OK) {
		close();
		return false;
	}
	return true;
}

void WarcryOpusCodec::close() {
	if (encoder) {
		opus_encoder_destroy(encoder);
		encoder = nullptr;
	}
	if (decoder) {
		opus_decoder_destroy(decoder);
		decoder = nullptr;
	}
}

bool WarcryOpusCodec::encode_frame(const int16_t *p_pcm, int p_samples, Vector<uint8_t> &r_out) {
	if (!encoder || !p_pcm || p_samples != FRAME_SAMPLES) {
		return false;
	}
	r_out.resize(4000);
	const int bytes = opus_encode(encoder, p_pcm, p_samples, r_out.ptrw(), r_out.size());
	if (bytes < 0) {
		r_out.clear();
		return false;
	}
	r_out.resize(bytes);
	return true;
}

bool WarcryOpusCodec::decode_frame(const uint8_t *p_data, int p_size, Vector<int16_t> &r_out) {
	if (!decoder || !p_data || p_size <= 0) {
		return false;
	}
	r_out.resize(FRAME_SAMPLES);
	const int decoded = opus_decode(decoder, p_data, p_size, r_out.ptrw(), FRAME_SAMPLES, 0);
	if (decoded < 0) {
		r_out.clear();
		return false;
	}
	r_out.resize(decoded);
	return true;
}
