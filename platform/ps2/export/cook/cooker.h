/**************************************************************************/
/*  cooker.h                                                              */
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

// Must match blazium-toolchain/guest/ps2 CookABI and BLAZIUM_PS2_COOK_ABI.
static const int kCookABI = 1;
static const int kMaxScenePacks = 32;
static const int kMaxNodes = 512;
static const int kMaxTris = 32768;
static const int kMaxTextures = 64;
static const int kMaxCameras = 8;
static const int kGsVramLimit = 4 * 1024 * 1024;
static const int kEeRamLimit = 24 * 1024 * 1024;

struct CookOptions {
	int width = 640; // 640, 512, or 320
	int region = 0; // 0 NTSC, 1 PAL
};

struct CookResult {
	String node_path;
	String mesh_path;
	String gtex_path;
	String script_path;
	String script_ir_path;
	String names_path;
	String budget_path;
	String warnings_path;
	String pack1_path;
	String sfx_path;
	String report;
	Vector<String> extra_pack_files;
	int pack_count = 0;
	Vector<String> errors;
	Vector<String> warnings;
	Vector<String> infos;
	int nodes = 0;
	int tris = 0;
	int cooked_tris = 0;
	int textures = 0;
	int vram_bytes = 0;
	int ee_ram_bytes = 0;
	int display_w = 640;
	int display_h = 448;
	bool over_budget = false;
};

Error cook_project(const String &p_export_dir, CookResult &r_out, const CookOptions &p_opts = CookOptions());

} // namespace ps2_cook
