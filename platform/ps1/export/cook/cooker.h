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

namespace ps1_cook {

// CookABI is the cooked blob layout. Must match blazium-toolchain guest_abi.
static const int kCookABI = 2;

struct CookResult {
	String tim_path;
	String mesh_path;
	String vag_path;
	String sprite_path;
	String script_path;
	String fmv_path;
	String names_path;
	String budget_path;
	String report;
	int texels = 0;
	int tris = 0;
	int cooked_tris = 0;
	int vram_bytes = 0;
	bool over_budget = false;
};

Error cook_project(const String &p_export_dir, CookResult &r_out);

} // namespace ps1_cook
