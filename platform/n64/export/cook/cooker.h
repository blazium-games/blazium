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
static const int kTmemBytes = 4096;
static const int kNtexFmtRgba16 = 1;

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
	String names_path;
	String report;
	Vector<String> errors;
	int nodes = 0;
	int tris = 0;
};

Error cook_project(const String &p_export_dir, CookResult &r_out);

} // namespace n64_cook
