/**************************************************************************/
/*  vag_writer.h                                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* SPDX-License-Identifier: MIT                                           */
/**************************************************************************/

#pragma once

#include "core/error/error_list.h"
#include "core/templates/vector.h"

namespace ps1_cook {

struct VagBlob {
	Vector<uint8_t> bytes;
	int sample_rate = 22050;
};

// MIT 4-bit SPU ADPCM beep (VAGp). Do not paste GPL encoders.
Error write_vag_beep(VagBlob &r_out);
// Filter-0 SPU ADPCM from mono PCM16. p_max_bytes includes the 48-byte header.
// Last ADPCM flag is 0x03 when p_loop, else 0x01 (beep stays one-shot).
Error write_vag_pcm(const Vector<int16_t> &p_pcm, int p_sample_rate, int p_max_bytes, VagBlob &r_out, bool p_loop = false);

} // namespace ps1_cook
