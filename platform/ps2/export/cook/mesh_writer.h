/**************************************************************************/
/*  mesh_writer.h                                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* SPDX-License-Identifier: MIT                                           */
/**************************************************************************/

#pragma once

#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/templates/vector.h"

#include <cstdint>

namespace ps2_cook {

// Positions stay Godot Y-up. P4 maps this into the GS guest convention.
struct CookTri {
	Vector3 v[3];
	Vector2 uv[3];
	uint8_t rgb[3][3] = { { 255, 255, 255 }, { 255, 255, 255 }, { 255, 255, 255 } };
	uint16_t tex_slot = 0;
	uint16_t node_id = 0;
};

struct MeshBlob {
	Vector<uint8_t> bytes;
	int tri_count = 0;
	int vert_count = 0;
};

// MESH magic + u16 ABI + float xyz + rgba + uv. Not GTE SVECTOR / TIM.
Error write_mesh_ee(const Vector<CookTri> &p_tris, int p_max_tris, MeshBlob &r_out);

} // namespace ps2_cook
