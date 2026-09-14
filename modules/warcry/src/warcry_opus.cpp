/**************************************************************************/
/*  warcry_opus.cpp                                                       */
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
