/**************************************************************************/
/*  tim_writer.h                                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* SPDX-License-Identifier: MIT                                           */
/**************************************************************************/

#pragma once

#include "core/io/image.h"
#include "core/templates/vector.h"

namespace ps1_cook {

struct TimBlob {
	Vector<uint8_t> bytes;
	int pixel_w = 0;
	int pixel_h = 0;
	int vram_bytes = 0;
};

// 4-bit TIM + 16-color CLUT. Image is resized to at most 64×64 and aligned to 32.
Error write_tim4(const Ref<Image> &p_image, TimBlob &r_out);

} // namespace ps1_cook
