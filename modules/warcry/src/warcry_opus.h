/**************************************************************************/
/*  warcry_opus.h                                                         */
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
