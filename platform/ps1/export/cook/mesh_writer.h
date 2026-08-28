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

namespace ps1_cook {

struct CookTri {
	Vector3 v[3];
	Vector2 uv[3];
};

struct MeshBlob {
	Vector<uint8_t> bytes;
	int tri_count = 0;
	Vector3 center;
	float scale = 1.0f;
};

// Quantize to SVECTOR, cap to p_max_tris (stride sample). Y is flipped for PS1.
Error write_mesh_svector(const Vector<CookTri> &p_tris, int p_max_tris, MeshBlob &r_out);

} // namespace ps1_cook
