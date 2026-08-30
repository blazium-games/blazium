/**************************************************************************/
/*  adpcm_writer.h                                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* SPDX-License-Identifier: MIT                                           */
/**************************************************************************/

#pragma once

#include "core/templates/vector.h"
#include "core/typedefs.h"

namespace ps2_cook {

// ABI 1 SFX blob: "SFX " + u16 ABI + u16 rate + u32 nibble_bytes + IMA ADPCM.
// Not VAG. Not TIM. Guest decodes with the same step/index tables.
struct SfxBlob {
	Vector<uint8_t> bytes;
	int sample_rate = 22050;
};

Error write_sfx_beep(SfxBlob &r_out);
Error write_sfx_pcm(const Vector<int16_t> &p_pcm, int p_sample_rate, SfxBlob &r_out);

} // namespace ps2_cook
