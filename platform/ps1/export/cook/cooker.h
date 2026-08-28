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

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"

namespace ps1_cook {

// CookABI is the cooked blob layout. Must match blazium-toolchain guest_abi.
static const int kCookABI = 4;
static const int kMaxTims = 4;
static const int kMaxTris = 2048;
static const int kMaxSprites = 2;
static const int kMaxLights = 3;
static const int kScriptIRSize = 62;

struct CookOptions {
	int width = 320;
	int region = 0; // 0 NTSC, 1 PAL
};

struct CookResult {
	String tim_path;
	String mesh_path;
	String vag_path;
	String sprite_path;
	String script_path;
	String gdbc_path;
	String luau_path;
	String fmv_path;
	String str_path;
	String xa_path;
	String names_path;
	String budget_path;
	String warnings_path;
	String report;
	Vector<String> errors;
	Vector<String> warnings;
	Vector<String> infos;
	int texels = 0;
	int tris = 0;
	int cooked_tris = 0;
	int vram_bytes = 0;
	int display_w = 320;
	int display_h = 240;
	bool over_budget = false;
};

Error cook_project(const String &p_export_dir, CookResult &r_out, const CookOptions &p_opts = CookOptions());

} // namespace ps1_cook
