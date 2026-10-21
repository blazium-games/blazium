/**************************************************************************/
/*  adpcm_writer.h                                                        */
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
#include "core/typedefs.h"

namespace ps2_cook {

// ABI 1 audio blob: magic + u16 ABI + u16 rate + u32 nibble_bytes + IMA ADPCM.
// SFX magic is "SFX ". MUSIC magic is "MUS ". Not VAG. Not TIM.
struct SfxBlob {
	Vector<uint8_t> bytes;
	int sample_rate = 22050;
};

Error write_sfx_beep(SfxBlob &r_out);
Error write_sfx_pcm(const Vector<int16_t> &p_pcm, int p_sample_rate, SfxBlob &r_out);
Error write_music_pcm(const Vector<int16_t> &p_pcm, int p_sample_rate, SfxBlob &r_out);

// A bank of clips: "SFXB" + u16 ABI + u16 count, then per clip u16 rate +
// u32 nibble_bytes + IMA ADPCM. PS2.play_sfx(i) plays clip i.
struct SfxClip {
	Vector<int16_t> pcm;
	int sample_rate = 22050;
};
Error write_sfx_bank(const Vector<SfxClip> &p_clips, SfxBlob &r_out);

} // namespace ps2_cook
