/**************************************************************************/
/*  map_parser_vmf_dispinfo.cpp                                           */
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

#include "map_parser_vmf_dispinfo.h"

#include "core/math/math_funcs.h"

static Vector3 _parse_vmf_vector_value(const String &p_value) {
	String cleaned = p_value.strip_edges();
	cleaned = cleaned.replace("[", "").replace("]", "").replace("(", "").replace(")", "");
	const Vector<double> comps = cleaned.split_floats(" ", false);
	if (comps.size() >= 3) {
		return Vector3(comps[0], comps[1], comps[2]);
	}
	return Vector3();
}

void VmfDispinfoParser::reset_all() {
	parsing_dispinfo = false;
	parsing_disp_distances = false;
	parsing_disp_normals = false;
	parsing_disp_offsets = false;
	parsing_disp_offset_normals = false;
	parsing_disp_alphas = false;
	parsing_disp_allowed_verts = false;
	dispinfo_scope = -1;
}

void VmfDispinfoParser::on_dispinfo_line(FaceData *p_face) {
	parsing_dispinfo = true;
	parsing_disp_distances = false;
	parsing_disp_normals = false;
	parsing_disp_offsets = false;
	parsing_disp_offset_normals = false;
	parsing_disp_alphas = false;
	parsing_disp_allowed_verts = false;
	if (p_face != nullptr) {
		p_face->disp.valid = true;
	}
}

void VmfDispinfoParser::on_subblock_line(const String &p_line) {
	if (!parsing_dispinfo) {
		return;
	}
	if (p_line == "distances") {
		parsing_disp_distances = true;
		parsing_disp_normals = false;
		parsing_disp_offsets = false;
		parsing_disp_offset_normals = false;
		parsing_disp_alphas = false;
		parsing_disp_allowed_verts = false;
	} else if (p_line == "normals") {
		parsing_disp_normals = true;
		parsing_disp_distances = false;
		parsing_disp_offsets = false;
		parsing_disp_offset_normals = false;
		parsing_disp_alphas = false;
		parsing_disp_allowed_verts = false;
	} else if (p_line == "offsets") {
		parsing_disp_offsets = true;
		parsing_disp_distances = false;
		parsing_disp_normals = false;
		parsing_disp_offset_normals = false;
		parsing_disp_alphas = false;
		parsing_disp_allowed_verts = false;
	} else if (p_line == "offset_normals") {
		parsing_disp_offset_normals = true;
		parsing_disp_distances = false;
		parsing_disp_normals = false;
		parsing_disp_offsets = false;
		parsing_disp_alphas = false;
		parsing_disp_allowed_verts = false;
	} else if (p_line == "alphas") {
		parsing_disp_alphas = true;
		parsing_disp_distances = false;
		parsing_disp_normals = false;
		parsing_disp_offsets = false;
		parsing_disp_offset_normals = false;
		parsing_disp_allowed_verts = false;
	} else if (p_line == "allowed_verts") {
		parsing_disp_allowed_verts = true;
		parsing_disp_distances = false;
		parsing_disp_normals = false;
		parsing_disp_offsets = false;
		parsing_disp_offset_normals = false;
		parsing_disp_alphas = false;
	}
}

void VmfDispinfoParser::on_brace_open(int p_scope) {
	if (parsing_dispinfo && dispinfo_scope < 0) {
		dispinfo_scope = p_scope;
	}
}

void VmfDispinfoParser::on_brace_close(int &p_scope) {
	if (p_scope > 0) {
		p_scope--;
		if (dispinfo_scope >= 0 && p_scope < dispinfo_scope) {
			reset_all();
		} else if (parsing_disp_distances) {
			parsing_disp_distances = false;
		} else if (parsing_disp_normals) {
			parsing_disp_normals = false;
		} else if (parsing_disp_offsets) {
			parsing_disp_offsets = false;
		} else if (parsing_disp_offset_normals) {
			parsing_disp_offset_normals = false;
		} else if (parsing_disp_alphas) {
			parsing_disp_alphas = false;
		} else if (parsing_disp_allowed_verts) {
			parsing_disp_allowed_verts = false;
		}
		if (p_scope == 0) {
			reset_all();
		}
	}
}

bool VmfDispinfoParser::handle_face_key(const String &p_key, const String &p_value, FaceData &p_face, real_t p_scale_factor) {
	if (parsing_disp_distances && p_key.begins_with("row")) {
		const Vector<double> vals = p_value.split_floats(" ", false);
		for (int i = 0; i < vals.size(); i++) {
			p_face.disp.distances.push_back((real_t)vals[i] * p_scale_factor);
		}
		return true;
	}
	if (parsing_disp_normals && p_key.begins_with("row")) {
		const Vector<double> vals = p_value.split_floats(" ", false);
		for (int i = 0; i + 2 < vals.size(); i += 3) {
			Vector3 normal((real_t)vals[i], (real_t)vals[i + 1], (real_t)vals[i + 2]);
			if (normal.length_squared() > 0.0) {
				p_face.disp.disp_normals.push_back(normal.normalized());
			} else {
				p_face.disp.disp_normals.push_back(Vector3(0, 0, 1));
			}
		}
		return true;
	}
	if (parsing_disp_offsets && p_key.begins_with("row")) {
		const Vector<double> vals = p_value.split_floats(" ", false);
		for (int i = 0; i + 2 < vals.size(); i += 3) {
			p_face.disp.offsets.push_back(Vector3((real_t)vals[i], (real_t)vals[i + 1], (real_t)vals[i + 2]) * p_scale_factor);
		}
		return true;
	}
	if (parsing_disp_offset_normals && p_key.begins_with("row")) {
		const Vector<double> vals = p_value.split_floats(" ", false);
		for (int i = 0; i + 2 < vals.size(); i += 3) {
			Vector3 normal((real_t)vals[i], (real_t)vals[i + 1], (real_t)vals[i + 2]);
			if (normal.length_squared() > 0.0) {
				p_face.disp.offset_normals.push_back(normal.normalized());
			} else {
				p_face.disp.offset_normals.push_back(Vector3(0, 0, 1));
			}
		}
		return true;
	}
	if (parsing_disp_alphas && p_key.begins_with("row")) {
		const Vector<double> vals = p_value.split_floats(" ", false);
		for (int i = 0; i < vals.size(); i++) {
			p_face.disp.alphas.push_back((real_t)vals[i]);
		}
		return true;
	}
	if (parsing_disp_allowed_verts) {
		const Vector<double> vals = p_value.split_floats(" ", false);
		for (int i = 0; i < vals.size(); i++) {
			p_face.disp.allowed_verts.push_back((int)vals[i]);
		}
		return true;
	}
	if (parsing_dispinfo) {
		p_face.disp.valid = true;
		if (p_key == "power") {
			p_face.disp.power = p_value.to_int();
		} else if (p_key == "elevation") {
			p_face.disp.elevation = p_value.to_float() * p_scale_factor;
		} else if (p_key == "startposition") {
			p_face.disp.startposition = _parse_vmf_vector_value(p_value) * p_scale_factor;
		}
		return true;
	}
	return false;
}
