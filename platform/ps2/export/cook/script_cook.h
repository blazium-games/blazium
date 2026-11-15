/**************************************************************************/
/*  script_cook.h                                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* SPDX-License-Identifier: MIT                                           */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/templates/vector.h"

namespace ps2_cook {

struct ScriptCook {
	Vector<String> accepted;
	Vector<String> rejected;
	Vector<String> extra_scenes;
	Vector<String> warnings;
	Vector<uint8_t> scrp;
	String ir_text;
};

bool source_is_rejected(const String &p_source, String &r_why);
void collect_literal_scenes(const String &p_source, Vector<String> &r_paths);
Error cook_scripts(const String &p_export_dir, ScriptCook &r_out);

} // namespace ps2_cook
