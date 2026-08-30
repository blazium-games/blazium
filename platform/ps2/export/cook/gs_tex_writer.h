/**************************************************************************/
/*  gs_tex_writer.h                                                       */
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

#include <cstdint>

namespace ps2_cook {

enum GsPsm : uint8_t {
	GS_PSM_T4 = 0,
	GS_PSM_T8 = 1,
	GS_PSM_CT16 = 2,
};

struct GsTex {
	Vector<uint8_t> data;
	Vector<uint16_t> clut;
	int width = 0;
	int height = 0;
	uint8_t psm = GS_PSM_T4;
	int vram_bytes = 0;
};

struct GtexBlob {
	Vector<uint8_t> bytes;
	int tex_count = 0;
	int vram_bytes = 0;
};

// Image → GS PSM (PSMT4/8 default, PSMCT16 when needed). Not a PS1 TIM.
Error write_gs_tex(const Ref<Image> &p_image, GsTex &r_out);
Error write_gtex_pack(const Vector<GsTex> &p_texs, GtexBlob &r_out);

} // namespace ps2_cook
