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
#include "core/templates/hash_map.h"
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
	uint16_t light_count = 0;
	struct Light {
		int16_t dx = 0;
		int16_t dy = 0;
		int16_t dz = 0;
		uint8_t r = 128;
		uint8_t g = 128;
		uint8_t b = 128;
	} lights[3];
	uint16_t display_w = 320;
	uint16_t display_h = 240;
	uint8_t region = 0;
	uint8_t display_flags = 0;
	uint16_t gd_count = 0;
	uint16_t luau_count = 0;
	int16_t cam_rx = 0;
	int16_t cam_ry = 0;
	int16_t cam_rz = 0;
	Vector<String> accepted;
	Vector<String> rejected;
	Vector<String> attached;
	Vector<String> warnings;
	HashMap<String, uint16_t> path_owners;
	Vector<String> cooked_gd;
	String map_text;
	String gdbc_path;
	String luau_path;
};

bool source_is_rejected(const String &p_source, String &r_why);
bool method_is_allowlisted(const String &p_name);
Error cook_scripts(const String &p_export_dir, ScriptIR &r_out);
Error write_script_ir(const String &p_export_dir, const ScriptIR &p_ir);

} // namespace ps1_cook
