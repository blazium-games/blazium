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

#include "core/math/vector3.h"
#include "core/templates/vector.h"
#include "core/error/error_list.h"

namespace n64_cook {

Error write_mesh_blob(const Vector<Vector3> &p_verts, Vector<uint8_t> &r_out, int &r_tris);

} // namespace n64_cook
