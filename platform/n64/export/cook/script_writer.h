/**************************************************************************/
/*  script_writer.h                                                       */
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
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"

namespace n64_cook {

bool source_is_forbidden(const String &p_src);
void collect_scene_refs(const String &p_src, Vector<String> &r_paths);
Error write_script_blob(const Vector<String> &p_sources, const HashMap<String, int> &p_packs, Vector<uint8_t> &r_out, String &r_ir_extra);

} // namespace n64_cook
