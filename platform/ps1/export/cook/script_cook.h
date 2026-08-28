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

#include <cstdint>

namespace ps1_cook {

struct ScriptIR {
	float rotate_y = 0.4f;
	int16_t pos_x = 0;
	int16_t pos_y = 0;
	int16_t pos_z = 512;
	int16_t geom_screen = 160;
	uint16_t flags = 0;
	Vector<String> accepted;
	Vector<String> rejected;
	String map_text;
};

bool source_is_rejected(const String &p_source, String &r_why);
Error cook_scripts(const String &p_export_dir, ScriptIR &r_out);
Error write_script_ir(const String &p_export_dir, const ScriptIR &p_ir);

} // namespace ps1_cook
