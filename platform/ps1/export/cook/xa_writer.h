/**************************************************************************/
/*  xa_writer.h                                                           */
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

#include <cstdint>

namespace ps1_cook {

struct XaBlob {
	Vector<uint8_t> bytes;
	int sample_rate = 37800;
};

// MIT XA-style ADPCM (filter-0), sector-padded. Do not paste GPL encoders.
Error write_xa_pcm(const Vector<int16_t> &p_pcm, int p_sample_rate, XaBlob &r_out);

} // namespace ps1_cook
