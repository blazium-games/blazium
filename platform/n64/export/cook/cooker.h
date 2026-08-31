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

namespace n64_cook {

// Must match blazium-toolchain/guest/n64 CookABI. Do not reuse PS1/PS2 ABI numbers.
static const int kCookABI = 1;
	static const int kMaxNodes = 256;
	static const int kMaxTris = 16384;
	static const int kMaxActions = 32;
static const int kTmemBytes = 4096;
static const int kNtexFmtRgba16 = 1;
static const int kRdramBudget = 6815744; // 6.5 MiB of 8 MiB Expansion Pak
static const int kRomBudget = 16777216; // 16 MiB cartridge including DFS
static const int kFbZBytes = 460800; // 320x240 16-bit double FB + 16-bit Z
static const int kGuestReserve = 1258291; // ~1.2 MiB .text/heap (breakdown §11)
static const int kRomPad = 393216; // hello .z64 IPL3/ELF pad
static const int kDisplayW = 320;
static const int kDisplayH = 240;

enum NodeKind : uint8_t {
	KIND_OTHER = 0,
	KIND_NODE3D = 1,
	KIND_CAMERA3D = 2,
	KIND_MESH_INSTANCE3D = 3,
};

struct CookResult {
	String node_path;
	String mesh_path;
	String ntex_path;
	String inp_path;
	String sfx_path;
	String music_path;
	String names_path;
	String budget_path;
	String report;
	Vector<String> errors;
	int nodes = 0;
	int tris = 0;
	int actions = 0;
	int tmem_bytes = 0;
	int rdram_bytes = 0;
	int rom_bytes = 0;
	bool over_budget = false;
};

Error cook_project(const String &p_export_dir, CookResult &r_out);

} // namespace n64_cook
