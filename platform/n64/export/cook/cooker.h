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
	static const int kMaxPacks = 24;
static const int kMaxCams = 8;
static const int kMaxHits = 64;
static const int kMaxAnims = 16;
static const int kMaxSprites = 32;
static const int kMaxTiles = 256;
static const int kMaxParts = 8;
static const int kMaxPaths = 8;
static const int kMaxPathPoints = 32;
static const int kMaxWaypoints = 32;
static const int kMaxNavmVerts = 64;
static const int kMaxNavmTris = 32;
static const int kMaxVehicles = 4;
static const int kTmemBytes = 4096;
static const int kNtexFmtRgba16 = 1;
static const int kRdramBudget = 6815744; // 6.5 MiB of 8 MiB Expansion Pak
static const int kRdramBudget4 = 2621440; // 2.5 MiB of 4 MiB no-pak (same 1.5 MiB slack)
static const int kRomBudget = 16777216; // 16 MiB cartridge including DFS
static const int kFbZBytes = 460800; // 320x240 16-bit double FB + 16-bit Z
static const int kGuestReserve = 1258291; // ~1.2 MiB .text/heap (breakdown §11)
static const int kRomPad = 393216; // hello .z64 IPL3/ELF pad
static const int kDisplayW = 320;
static const int kDisplayH = 240;

inline int display_height_for_width(int p_w) {
	return p_w == 640 ? 480 : 240;
}

inline int display_width_clamped(int p_w) {
	return p_w == 640 ? 640 : 320;
}

inline int fb_z_bytes_for_width(int p_w) {
	const int w = display_width_clamped(p_w);
	const int h = display_height_for_width(w);
	return w * h * 2 * 3; // 16-bit double color + Z
}

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
	String pack_path;
	String script_path;
	String names_path;
	String budget_path;
	String warnings_path;
	String cam_path;
	String hit_path;
	String anim_path;
	String sprt_path;
	String tile_path;
	String prtn_path;
	String pthn_path;
	String wayn_path;
	String navn_path;
	String navm_path;
	String vehn_path;
	String report;
	Vector<String> errors;
	Vector<String> warnings;
	int nodes = 0;
	int tris = 0;
	int actions = 0;
	int tmem_bytes = 0;
	int rdram_bytes = 0;
	int rom_bytes = 0;
	int pack_count = 1;
	bool over_budget = false;
};

static const int kMaxExtraPacks = 23;
static const int kExtraRdramCap = 2 * 1024 * 1024;
static const int kExtraRdramCap4 = 1 * 1024 * 1024;

Error cook_project(const String &p_export_dir, CookResult &r_out, int p_display_w = 320, int p_rdram_mb = 8);

} // namespace n64_cook
