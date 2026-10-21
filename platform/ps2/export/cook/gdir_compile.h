/**************************************************************************/
/*  gdir_compile.h                                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "core/math/color.h"
#include "core/math/transform_3d.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include "core/variant/variant.h"

namespace ps2_cook {

// What the cooker knows about one cooked node, for scripts (GDIR).
struct GdirNode {
	int parent = -1;
	String path; // from the scene root: "." for the root, "Showroom/Dais"
	String name;
	uint8_t flags = 1; // 1 visible, 2 Node3D, 4 Control (HUD), 8 camera
	int hud = -1;
	Vector3 lpos;
	Vector3 lrot;
	Vector3 lscale = Vector3(1, 1, 1);
	Transform3D w0; // cooked world transform (Node3D); Controls: origin = global, basis row 0 = local
	String script_path; // GDScript attached to this node (empty: none)
	String script_source;
	HashMap<String, Variant> exported; // @export values as set in the scene
};

struct GdirConn {
	int src = -1; // emitting node
	String signal; // "pressed"
	int target = -1; // node whose script has the method
	String method;
};

struct GdirPack {
	int pack = 0;
	String scene;
	Vector<GdirNode> nodes;
	Vector<GdirConn> conns;
	bool has_clear = false;
	Color clear;
	int camera = -1;
	float fov = 70.0f;
};

struct GdirResult {
	Vector<uint8_t> blob; // empty when no pack has scripts
	Vector<String> errors; // file:line: message
	Vector<String> warnings;
	String listing; // human readable summary for SCRIPT_IR.txt
	int classes = 0;
	int functions = 0;
	int instances = 0;
};

// Compiles every script attached to a cooked node of p_packs into one GDIR
// program. Unsupported constructs are errors (fail closed); the caller decides
// whether that fails the export.
void gdir_compile(const Vector<GdirPack> &p_packs, GdirResult &r_out);

} // namespace ps2_cook
