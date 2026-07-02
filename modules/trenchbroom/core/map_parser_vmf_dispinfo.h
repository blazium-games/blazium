/**************************************************************************/
/*  map_parser_vmf_dispinfo.h                                             */
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

#include "data.h"

#include "core/math/vector3.h"
#include "core/string/ustring.h"

struct VmfDispinfoParser {
	bool parsing_dispinfo = false;
	bool parsing_disp_distances = false;
	bool parsing_disp_normals = false;
	bool parsing_disp_offsets = false;
	bool parsing_disp_offset_normals = false;
	bool parsing_disp_alphas = false;
	bool parsing_disp_allowed_verts = false;
	int dispinfo_scope = -1;

	void on_dispinfo_line(FaceData *p_face);
	void on_subblock_line(const String &p_line);
	void on_brace_open(int p_scope);
	void on_brace_close(int &p_scope);
	void reset_all();

	bool handle_face_key(const String &p_key, const String &p_value, FaceData &p_face, real_t p_scale_factor);
};
