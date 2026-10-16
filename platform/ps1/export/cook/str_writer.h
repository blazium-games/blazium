/**************************************************************************/
/*  str_writer.h                                                          */
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
#include "core/io/image.h"
#include "core/templates/vector.h"

namespace ps1_cook {

struct StrBlob {
	Vector<uint8_t> bytes;
	int frames = 0;
	int width = 0;
	int height = 0;
};

// MIT STR sectors (0x0160) + MDEC RLE. Guest DecDCTin. No psxpress encode.
Error write_str_frames(const Vector<Ref<Image>> &p_frames, StrBlob &r_out);

} // namespace ps1_cook
