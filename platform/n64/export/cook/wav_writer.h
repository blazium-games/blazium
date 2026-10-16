/**************************************************************************/
/*  wav_writer.h                                                          */
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

namespace n64_cook {

// Host PCM WAV for toolchain audioconv64. Not VAG, not wav64, not TIM/GTEX.
Error write_wav_pcm(const Vector<int16_t> &p_pcm, int p_rate, Vector<uint8_t> &r_out);
Error write_wav_beep(int p_hz, int p_ms, int p_rate, Vector<uint8_t> &r_out);

} // namespace n64_cook
