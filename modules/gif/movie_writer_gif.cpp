/**************************************************************************/
/*  movie_writer_gif.cpp                                                  */
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

#include "movie_writer_gif.h"

#include "gif_decode.h"
#include "gif_encode.h"

uint32_t MovieWriterGIF::get_audio_mix_rate() const {
	return 48000;
}

AudioServer::SpeakerMode MovieWriterGIF::get_audio_speaker_mode() const {
	return AudioServer::SPEAKER_MODE_STEREO;
}

void MovieWriterGIF::get_supported_extensions(List<String> *r_extensions) const {
	r_extensions->push_back("gif");
}

bool MovieWriterGIF::handles_file(const String &p_path) const {
	return p_path.get_extension().to_lower() == "gif";
}

Error MovieWriterGIF::write_begin(const Size2i &p_movie_size, uint32_t p_fps, const String &p_base_path) {
	ERR_FAIL_COND_V(p_movie_size.x <= 0 || p_movie_size.y <= 0, ERR_INVALID_PARAMETER);
	const int64_t pixels = int64_t(p_movie_size.x) * int64_t(p_movie_size.y);
	ERR_FAIL_COND_V_MSG(pixels > gif_get_max_canvas_pixels(), ERR_OUT_OF_MEMORY, "Movie GIF canvas exceeds blazium/gif/max_canvas_pixels.");
	base_path = p_base_path;
	fps = MAX(1u, p_fps);
	frames.clear();
	return OK;
}

Error MovieWriterGIF::write_frame(const Ref<Image> &p_image, const int32_t *p_audio_data) {
	ERR_FAIL_COND_V(p_image.is_null(), ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V_MSG(frames.size() >= gif_get_max_frames(), ERR_OUT_OF_MEMORY, "Movie GIF reached blazium/gif/max_frames.");
	Ref<Image> copy = p_image->duplicate();
	frames.push_back(copy);
	return OK;
}

void MovieWriterGIF::write_end() {
	Vector<GIFEncodeFrame> enc;
	enc.resize(frames.size());
	const int delay_cs = MAX(1, int(Math::round(100.0 / double(fps))));
	for (int i = 0; i < frames.size(); i++) {
		enc.write[i].image = frames[i];
		enc.write[i].delay_cs = delay_cs;
	}
	Vector<uint8_t> buffer;
	if (gif_encode_frames(enc, 0, true, true, buffer) == OK) {
		gif_encode_write_file(base_path, buffer);
	}
	frames.clear();
}

MovieWriterGIF::MovieWriterGIF() {
}
