/**************************************************************************/
/*  pack_writer.h                                                         */
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
#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/typedefs.h"

namespace n64_cook {

Error write_pack_blob(int p_pack_id, const String &p_name, Vector<uint8_t> &r_out, uint32_t p_node_sz = 0, uint32_t p_mesh_sz = 0, uint32_t p_ntex_sz = 0);

} // namespace n64_cook
